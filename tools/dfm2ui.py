#!/usr/bin/env python3
"""Convert Delphi VCL text DFM files into Qt Designer .ui files.

Usage:
    dfm2ui.py <delphi-source-dir> <output-forms-dir> <output-resources-dir>

Every visual VCL control is mapped onto the closest Qt widget and placed at
exactly the same pixel position as in the original form.  VCL layout
information that Qt Designer can not express directly (Align, Anchors,
ModalResult, BitBtn Kind, ...) is stored as dynamic "vcl*" properties which
are evaluated at runtime by VclLayout (src/compat/VclLayout.cpp).

Embedded images (button glyphs, image lists, pictures, icons) are extracted
into the resources directory and a Qt resource file (dfend.qrc) is written.
"""
import hashlib
import io
import os
import re
import sys
import xml.sax.saxutils as sx

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dfmparser import parse_dfm, Ident, Set, Collection  # noqa: E402

# --------------------------------------------------------------------------
# Image helpers
# --------------------------------------------------------------------------


def _read_bmp(data):
    """Read a Windows BMP (starting with 'BM'). Returns (RGBA image, bytes used)."""
    if data[:2] != b'BM':
        raise ValueError('not a BMP')
    file_size = int.from_bytes(data[2:6], 'little')
    off_bits = int.from_bytes(data[10:14], 'little')
    hdr_size = int.from_bytes(data[14:18], 'little')
    width = int.from_bytes(data[18:22], 'little', signed=True)
    height = int.from_bytes(data[22:26], 'little', signed=True)
    bpp = int.from_bytes(data[28:30], 'little')
    compression = int.from_bytes(data[30:34], 'little')
    if bpp == 32 and compression in (0, 3):
        top_down = height < 0
        h = abs(height)
        stride = width * 4
        px = data[off_bits:off_bits + stride * h]
        img = Image.new('RGBA', (width, h))
        pix = []
        has_alpha = False
        for y in range(h):
            row = px[y * stride:(y + 1) * stride]
            for x in range(width):
                b, g, r, a = row[x * 4:x * 4 + 4]
                if a:
                    has_alpha = True
                pix.append((r, g, b, a))
        img.putdata(pix)
        if not top_down:
            img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img.info['has_alpha'] = has_alpha
        used = max(file_size, off_bits + stride * h)
        return img, used
    row = ((abs(width) * bpp + 31) // 32) * 4
    used = max(file_size, off_bits + row * abs(height))
    img = Image.open(io.BytesIO(data[:used]))
    img.load()
    img = img.convert('RGBA')
    img.info['has_alpha'] = False
    return img, used


def _make_transparent(img, color):
    img = img.copy()
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if (r, g, b) == color[:3]:
                px[x, y] = (r, g, b, 0)
    return img


class ResourceStore:
    def __init__(self, outdir):
        self.outdir = outdir
        self.files = []  # resource paths relative to outdir

    def _save(self, relpath, writer):
        full = os.path.join(self.outdir, relpath)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        writer(full)
        if relpath not in self.files:
            self.files.append(relpath)
        return ':/' + relpath

    def glyph(self, data, num_glyphs=1):
        """TBitBtn / TSpeedButton glyph. Returns resource path."""
        size = int.from_bytes(data[:4], 'little')
        bmp = data[4:4 + size]
        img, _ = _read_bmp(bmp)
        n = max(1, int(num_glyphs or 1))
        if n > 1 and img.width % n == 0:
            w = img.width // n
            img = img.crop((0, 0, w, img.height))
        if not img.info.get('has_alpha'):
            transparent = img.getpixel((0, img.height - 1))
            img = _make_transparent(img, transparent)
        buf = io.BytesIO()
        img.save(buf, 'PNG')
        digest = hashlib.md5(buf.getvalue()).hexdigest()[:16]
        return self._save(f'glyphs/{digest}.png', lambda p: open(p, 'wb').write(buf.getvalue()))

    def imagelist(self, unit, listname, data):
        """TImageList 'IL' stream. Returns list of resource paths."""
        if data[:2] != b'IL':
            return []
        count = int.from_bytes(data[4:6], 'little', signed=True)
        cx = int.from_bytes(data[10:12], 'little', signed=True)
        cy = int.from_bytes(data[12:14], 'little', signed=True)
        pos = data.find(b'BM', 14)
        if pos < 0:
            return []
        color, used = _read_bmp(data[pos:])
        mask = None
        mpos = data.find(b'BM', pos + used)
        if mpos >= 0 and mpos == pos + used:
            try:
                mask, _ = _read_bmp(data[mpos:])
            except Exception:
                mask = None
        cols = max(1, color.width // cx)
        result = []
        for i in range(count):
            x = (i % cols) * cx
            y = (i // cols) * cy
            if y + cy > color.height:
                break
            tile = color.crop((x, y, x + cx, y + cy))
            if not color.info.get('has_alpha') and mask is not None:
                m = mask.crop((x, y, x + cx, y + cy))
                tp = tile.load()
                mp = m.load()
                for yy in range(cy):
                    for xx in range(cx):
                        r, g, b, a = mp[xx, yy]
                        pr, pg, pb, pa = tp[xx, yy]
                        # white in mask = transparent
                        tp[xx, yy] = (pr, pg, pb, 0 if r > 127 else 255)
            elif not color.info.get('has_alpha'):
                tile = tile.copy()
                tp = tile.load()
                for yy in range(cy):
                    for xx in range(cx):
                        pr, pg, pb, pa = tp[xx, yy]
                        tp[xx, yy] = (pr, pg, pb, 255)
            buf = io.BytesIO()
            tile.save(buf, 'PNG')
            rel = f'imagelists/{unit}/{listname}/{i}.png'
            result.append(self._save(rel, lambda p, b=buf: open(p, 'wb').write(b.getvalue())))
        return result

    def picture(self, unit, name, data):
        """TImage.Picture.Data. Returns resource path or None."""
        n = data[0]
        cls = data[1:1 + n].decode('latin-1')
        rest = data[1 + n:]
        if cls == 'TBitmap':
            size = int.from_bytes(rest[:4], 'little')
            img, _ = _read_bmp(rest[4:4 + size])
            buf = io.BytesIO()
            img.save(buf, 'PNG')
            return self._save(f'pictures/{unit}_{name}.png', lambda p: open(p, 'wb').write(buf.getvalue()))
        if cls in ('TJPEGImage',):
            size = int.from_bytes(rest[:4], 'little')
            return self._save(f'pictures/{unit}_{name}.jpg', lambda p: open(p, 'wb').write(rest[4:4 + size]))
        if cls in ('TGIFImage',):
            return self._save(f'pictures/{unit}_{name}.gif', lambda p: open(p, 'wb').write(rest))
        if cls in ('TPngImage', 'TPNGObject'):
            return self._save(f'pictures/{unit}_{name}.png', lambda p: open(p, 'wb').write(rest))
        if cls == 'TIcon':
            return self._save(f'pictures/{unit}_{name}.ico', lambda p: open(p, 'wb').write(rest))
        return None

    def icon(self, unit, name, data):
        return self._save(reborn_name(f'icons/{unit}_{name}.ico'), lambda p: open(p, 'wb').write(data))

    def write_qrc(self, path):
        # all files below the resource directory (generated ones and hand made ones)
        files = []
        for dirpath, dirnames, filenames in os.walk(self.outdir):
            for fn in filenames:
                if fn.endswith('.qrc') or fn.endswith('.rc'):
                    continue
                files.append(os.path.relpath(os.path.join(dirpath, fn), self.outdir).replace(os.sep, '/'))
        with open(path, 'w', encoding='utf-8') as f:
            f.write('<!DOCTYPE RCC>\n<RCC version="1.0">\n<qresource prefix="/">\n')
            for rel in sorted(files):
                f.write(f'    <file>{sx.escape(rel)}</file>\n')
            f.write('</qresource>\n</RCC>\n')


# --------------------------------------------------------------------------
# Value helpers
# --------------------------------------------------------------------------

VK_NAMES = {
    0x08: 'Backspace', 0x09: 'Tab', 0x0D: 'Return', 0x1B: 'Esc', 0x20: 'Space',
    0x21: 'PgUp', 0x22: 'PgDown', 0x23: 'End', 0x24: 'Home', 0x25: 'Left',
    0x26: 'Up', 0x27: 'Right', 0x28: 'Down', 0x2D: 'Ins', 0x2E: 'Del',
    0x6B: '+', 0x6D: '-', 0xBB: '=', 0xBD: '-',
}
for _i in range(12):
    VK_NAMES[0x70 + _i] = f'F{_i + 1}'


def shortcut_to_qt(value):
    if not isinstance(value, int) or value == 0:
        return None
    key = value & 0xFF
    mods = []
    if value & 0x4000:
        mods.append('Ctrl')
    if value & 0x8000:
        mods.append('Alt')
    if value & 0x2000:
        mods.append('Shift')
    if key in VK_NAMES:
        name = VK_NAMES[key]
    elif 0x30 <= key <= 0x39 or 0x41 <= key <= 0x5A:
        name = chr(key)
    elif 0x60 <= key <= 0x69:
        name = str(key - 0x60)
    else:
        return None
    return '+'.join(mods + [name])


SYS_COLORS = {
    'clBtnFace': 'palette(button)', 'clWindow': 'palette(base)', 'clWindowText': 'palette(text)',
    'clInfoBk': '#FFFFE1', 'clInfoText': '#000000', 'clHighlight': 'palette(highlight)',
    'clHighlightText': 'palette(highlighted-text)', 'clBtnText': 'palette(button-text)',
    'clGrayText': '#808080', 'clBtnShadow': 'palette(mid)', 'clBtnHighlight': 'palette(light)',
    'clCaptionText': 'palette(window-text)', 'clHotLight': '#0066CC', 'clMenu': 'palette(window)',
    'clCream': '#F0FBFF', 'clMoneyGreen': '#C0DCC0', 'clSkyBlue': '#A6CAF0', 'clMedGray': '#A4A0A0',
    'clBlack': '#000000', 'clMaroon': '#800000', 'clGreen': '#008000', 'clOlive': '#808000',
    'clNavy': '#000080', 'clPurple': '#800080', 'clTeal': '#008080', 'clGray': '#808080',
    'clSilver': '#C0C0C0', 'clRed': '#FF0000', 'clLime': '#00FF00', 'clYellow': '#FFFF00',
    'clBlue': '#0000FF', 'clFuchsia': '#FF00FF', 'clAqua': '#00FFFF', 'clWhite': '#FFFFFF',
    'clLtGray': '#C0C0C0', 'clDkGray': '#808080',
}


def color_to_css(value):
    if isinstance(value, Ident):
        return SYS_COLORS.get(str(value))
    if isinstance(value, int):
        v = value & 0xFFFFFF
        r, g, b = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF
        return f'#{r:02X}{g:02X}{b:02X}'
    return None


def strip_accel(s):
    """Remove VCL accelerator markers from a label text ('&&' -> '&')."""
    out = []
    i = 0
    while i < len(s):
        if s[i] == '&':
            if i + 1 < len(s) and s[i + 1] == '&':
                out.append('&')
                i += 2
                continue
            i += 1
            continue
        out.append(s[i])
        i += 1
    return ''.join(out)


def esc(s):
    return sx.escape(str(s))


# --------------------------------------------------------------------------
# UI writer
# --------------------------------------------------------------------------

class Ui:
    def __init__(self):
        self.lines = []
        self.ind = 0

    def w(self, s):
        self.lines.append('  ' * self.ind + s)

    def open(self, s):
        self.w(s)
        self.ind += 1

    def close(self, s):
        self.ind -= 1
        self.w(s)

    def prop(self, name, kind, value, stdset=True):
        attr = '' if stdset else ' stdset="0"'
        if kind == 'string':
            self.w(f'<property name="{name}"{attr}><string>{esc(value)}</string></property>')
        elif kind == 'notr':
            self.w(f'<property name="{name}"{attr}><string notr="true">{esc(value)}</string></property>')
        elif kind == 'bool':
            self.w(f'<property name="{name}"{attr}><bool>{"true" if value else "false"}</bool></property>')
        elif kind == 'number':
            self.w(f'<property name="{name}"{attr}><number>{int(value)}</number></property>')
        elif kind == 'enum':
            self.w(f'<property name="{name}"{attr}><enum>{value}</enum></property>')
        elif kind == 'set':
            self.w(f'<property name="{name}"{attr}><set>{value}</set></property>')
        elif kind == 'stringlist':
            self.open(f'<property name="{name}"{attr}>')
            self.open('<stringlist notr="true">')
            for s in value:
                self.w(f'<string>{esc(s)}</string>')
            self.close('</stringlist>')
            self.close('</property>')
        elif kind == 'geometry':
            x, y, wd, ht = value
            self.w(f'<property name="geometry"><rect><x>{x}</x><y>{y}</y><width>{wd}</width>'
                   f'<height>{ht}</height></rect></property>')
        elif kind == 'size':
            wd, ht = value
            self.w(f'<property name="{name}"><size><width>{wd}</width><height>{ht}</height></size></property>')
        elif kind == 'icon':
            self.w(f'<property name="{name}"><iconset resource="../../resources/dfend.qrc">'
                   f'<normaloff>{esc(value)}</normaloff></iconset></property>')
        elif kind == 'pixmap':
            self.w(f'<property name="{name}"><pixmap resource="../../resources/dfend.qrc">{esc(value)}</pixmap></property>')
        elif kind == 'font':
            self.open(f'<property name="{name}">')
            self.open('<font>')
            for k, v in value.items():
                if isinstance(v, bool):
                    self.w(f'<{k}>{"true" if v else "false"}</{k}>')
                else:
                    self.w(f'<{k}>{esc(v)}</{k}>')
            self.close('</font>')
            self.close('</property>')
        elif kind == 'shortcut':
            self.w(f'<property name="{name}"><string notr="true">{esc(value)}</string></property>')
        else:
            raise ValueError(kind)


CUSTOM_WIDGETS = {
    # class: (extends, header)
    'TRadioGroupWidget': ('QGroupBox', 'VclWidgets.h'),
    'TSplitterWidget': ('QFrame', 'VclWidgets.h'),
    'TColorBoxWidget': ('QComboBox', 'VclWidgets.h'),
    'TValueListEditorWidget': ('QTableWidget', 'VclWidgets.h'),
    'TImageWidget': ('QLabel', 'VclWidgets.h'),
    'TListView': ('QWidget', 'ComCtrls.h'),
}

FOCUSABLE = {'QLineEdit', 'QPlainTextEdit', 'QTextEdit', 'QComboBox', 'QCheckBox', 'QRadioButton',
             'QPushButton', 'QSpinBox', 'QTreeWidget', 'QListWidget', 'QTableWidget', 'QSlider',
             'QTabWidget', 'TColorBoxWidget', 'TValueListEditorWidget', 'TListView'}


class Converter:
    def __init__(self, unit, root, base_kind, res, form_ui_name):
        self.unit = unit
        self.root = root
        self.base_kind = base_kind  # 'form' | 'frame' | 'mainform'
        self.res = res
        self.ui = Ui()
        self.actions = []  # (name, props dict)
        self.used_custom = set()
        self.imagelists = {}  # name -> list of resource paths
        self.tabstops = []
        self.unnamed = 0
        self.names = set(o.name for o in root.walk() if o.name)
        self.menus_for_form = []
        self.form_ui_name = form_ui_name
        self.popup_menus = []
        self.radio_groups = {}

    # -------------------- helpers
    def uniq(self, base):
        n = base
        i = 1
        while n in self.names:
            n = f'{base}_{i}'
            i += 1
        self.names.add(n)
        return n

    def img_for(self, images_name, index):
        if images_name is None or index is None or not isinstance(index, int) or index < 0:
            return None
        lst = self.imagelists.get(str(images_name))
        if lst and index < len(lst):
            return lst[index]
        return None

    def common_props(self, o, qcls, geometry=True, offset=(0, 0)):
        ui = self.ui
        p = o.props
        if geometry:
            x = int(p.get('Left', 0)) + offset[0]
            y = int(p.get('Top', 0)) + offset[1]
            wd = int(p.get('Width', 0) or 0)
            ht = int(p.get('Height', 0) or 0)
            ui.prop('geometry', 'geometry', (x, y, wd, ht))
        if p.get('Visible') == Ident('False') or p.get('TabVisible') == Ident('False') and False:
            ui.prop('visible', 'bool', False)
        if p.get('Enabled') == Ident('False'):
            ui.prop('enabled', 'bool', False)
        if isinstance(p.get('Hint'), str) and p.get('Hint'):
            ui.prop('toolTip', 'string', p['Hint'])
        font = {}
        if 'Font.Name' in p and p.get('ParentFont') != Ident('True'):
            fname = str(p['Font.Name'])
            if fname not in ('Tahoma', 'MS Sans Serif'):
                font['family'] = fname
            h = p.get('Font.Height')
            if isinstance(h, int) and h != -11:
                font['pointsize'] = round(abs(h) * 72 / 96) if h < 0 else h
            st = p.get('Font.Style') or []
            if 'fsBold' in st:
                font['bold'] = True
            if 'fsItalic' in st:
                font['italic'] = True
            if 'fsUnderline' in st:
                font['underline'] = True
            if 'fsStrikeOut' in st:
                font['strikeout'] = True
        if font:
            ui.prop('font', 'font', font)
        css = []
        if 'Font.Color' in p and p.get('ParentFont') != Ident('True'):
            c = color_to_css(p['Font.Color'])
            if c and p['Font.Color'] not in (Ident('clWindowText'), Ident('clBtnText'), Ident('clBlack')):
                css.append(f'color: {c};')
        if 'Color' in p and p.get('ParentColor') != Ident('True') and qcls in ('QFrame', 'QLabel', 'QWidget', 'QScrollArea'):
            c = color_to_css(p['Color'])
            if c and p['Color'] != Ident('clBtnFace'):
                css.append(f'background-color: {c};')
        if css:
            ui.prop('styleSheet', 'notr', f'#{o.name} {{ {" ".join(css)} }}')
        # dynamic VCL props (including class specific default Align values that
        # Delphi does not store in the DFM)
        align = p.get('Align')
        if align is None:
            if o.cls in ('TCoolBar', 'TToolBar'):
                align = 'alClient' if (o.parent is not None and o.parent.cls == 'TCoolBar') else 'alTop'
        if align is not None and align != 'alNone':
            ui.prop('vclAlign', 'notr', str(align), stdset=False)
        if 'Anchors' in p:
            anchors = ','.join(p['Anchors'])
            if anchors != 'akLeft,akTop':
                ui.prop('vclAnchors', 'notr', anchors, stdset=False)
        if isinstance(p.get('Tag'), int) and p['Tag'] != 0:
            ui.prop('tag', 'number', p['Tag'], stdset=False)
        for c in ('Constraints.MinWidth', 'Constraints.MinHeight', 'Constraints.MaxWidth', 'Constraints.MaxHeight'):
            if isinstance(p.get(c), int) and p[c] > 0:
                ui.prop('vcl' + c.replace('Constraints.', ''), 'number', p[c], stdset=False)
        if p.get('AutoSize') == Ident('True') and o.cls in ('TPanel', 'TToolBar', 'TEdit', 'TLabeledEdit'):
            pass
        if 'PopupMenu' in p:
            ui.prop('vclPopupMenu', 'notr', str(p['PopupMenu']), stdset=False)
        extra = getattr(self, 'pending_extra', None)
        if extra:
            for k, v in extra.items():
                ui.prop(k, 'number', v, stdset=False)
            self.pending_extra = None
        if 'TabOrder' in p and qcls in FOCUSABLE:
            pass

    # -------------------- tree emission
    def emit_children(self, o, offset=(0, 0)):
        kids = list(o.children)
        # VCL paints later children on top; Qt does the same with sibling order.
        for c in kids:
            self.emit(c, offset)

    def emit(self, o, offset=(0, 0)):
        cls = o.cls
        fn = getattr(self, 'w_' + cls, None)
        if fn is None:
            fn = self.w_generic
        fn(o, offset)

    def collect_tab_order(self, o):
        """Depth first tab order respecting TabOrder per container."""
        kids = [c for c in o.children]
        kids_sorted = sorted(kids, key=lambda c: (int(c.props.get('TabOrder', 9999)) if isinstance(c.props.get('TabOrder'), int) else 9999))
        for c in kids_sorted:
            q = self.qclass_of.get(c.name)
            if q in FOCUSABLE and c.props.get('TabStop') != Ident('False'):
                self.tabstops.append(c.name)
            if c.cls in ('TLabeledEdit',):
                pass
            self.collect_tab_order(c)

    # non visual
    def w_nonvisual(self, o, offset):
        pass

    w_TImageList = w_nonvisual
    w_TOpenDialog = w_nonvisual
    w_TSaveDialog = w_nonvisual
    w_TTimer = w_nonvisual
    w_TTrayIcon = w_nonvisual
    w_TApplicationEvents = w_nonvisual
    w_TXPManifest = w_nonvisual
    w_TMainMenu = w_nonvisual  # handled at form level

    def w_generic(self, o, offset):
        sys.stderr.write(f'  [warn] {self.unit}: unmapped class {o.cls} ({o.name}) -> QWidget\n')
        self.ui.open(f'<widget class="QWidget" name="{o.name}">')
        self.common_props(o, 'QWidget', offset=offset)
        self.ui.prop('vclClass', 'notr', o.cls, stdset=False)
        self.emit_children(o)
        self.ui.close('</widget>')

    def w_TLabel(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QLabel" name="{o.name}">')
        self.common_props(o, 'QLabel', offset=offset)
        cap = p.get('Caption', '')
        if p.get('ShowAccelChar') == Ident('False'):
            ui.prop('text', 'string', cap)
        else:
            ui.prop('text', 'string', strip_accel(cap) if not p.get('FocusControl') else cap)
        ui.prop('textFormat', 'enum', 'Qt::PlainText')
        if p.get('WordWrap') == Ident('True'):
            ui.prop('wordWrap', 'bool', True)
        align = {'taRightJustify': 'Qt::AlignRight', 'taCenter': 'Qt::AlignHCenter'}.get(str(p.get('Alignment', '')), 'Qt::AlignLeft')
        valign = {'tlCenter': 'Qt::AlignVCenter', 'tlBottom': 'Qt::AlignBottom'}.get(str(p.get('Layout', '')), 'Qt::AlignTop')
        ui.prop('alignment', 'set', f'{align}|{valign}')
        if p.get('AutoSize') != Ident('False'):
            ui.prop('vclAutoSize', 'bool', True, stdset=False)
        if p.get('FocusControl'):
            ui.prop('vclFocusControl', 'notr', str(p['FocusControl']), stdset=False)
        if p.get('Transparent') == Ident('False') and 'Color' in p:
            pass
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QLabel'

    w_TStaticText = w_TLabel

    def w_TEdit(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QLineEdit" name="{o.name}">')
        self.common_props(o, 'QLineEdit', offset=offset)
        if isinstance(p.get('Text'), str):
            ui.prop('text', 'string', p['Text'])
        if p.get('ReadOnly') == Ident('True'):
            ui.prop('readOnly', 'bool', True)
        if isinstance(p.get('PasswordChar'), str) and p['PasswordChar'] and p['PasswordChar'] != '\0':
            ui.prop('echoMode', 'enum', 'QLineEdit::Password')
        if isinstance(p.get('MaxLength'), int) and p['MaxLength'] > 0:
            ui.prop('maxLength', 'number', p['MaxLength'])
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QLineEdit'

    def w_TLabeledEdit(self, o, offset):
        p = o.props
        self.w_TEdit(o, offset)
        # separate label above the edit (LabelPosition=lpAbove, LabelSpacing=3)
        lname = self.uniq(o.name + '_EditLabel')
        lh = int(p.get('EditLabel.Height', 13))
        lw = int(p.get('EditLabel.Width', 100))
        x = int(p.get('Left', 0)) + offset[0]
        y = int(p.get('Top', 0)) + offset[1]
        pos = str(p.get('LabelPosition', 'lpAbove'))
        spacing = int(p.get('LabelSpacing', 3))
        if pos == 'lpLeft':
            lx, ly = x - lw - spacing, y + (int(p.get('Height', 21)) - lh) // 2
        elif pos == 'lpRight':
            lx, ly = x + int(p.get('Width', 0)) + spacing, y + (int(p.get('Height', 21)) - lh) // 2
        elif pos == 'lpBelow':
            lx, ly = x, y + int(p.get('Height', 21)) + spacing
        else:
            lx, ly = x, y - lh - spacing
        ui = self.ui
        ui.open(f'<widget class="QLabel" name="{lname}">')
        ui.prop('geometry', 'geometry', (lx, ly, lw, lh))
        ui.prop('text', 'string', p.get('EditLabel.Caption', ''))
        ui.prop('textFormat', 'enum', 'Qt::PlainText')
        ui.prop('vclAutoSize', 'bool', True, stdset=False)
        ui.prop('vclFocusControl', 'notr', o.name, stdset=False)
        ui.prop('vclLabeledEditOf', 'notr', o.name, stdset=False)
        if 'Anchors' in p and 'akLeft' not in p['Anchors']:
            ui.prop('vclAnchors', 'notr', ','.join(p['Anchors']), stdset=False)
        if p.get('Visible') == Ident('False'):
            ui.prop('visible', 'bool', False)
        if p.get('Enabled') == Ident('False'):
            ui.prop('enabled', 'bool', False)
        ui.close('</widget>')
        self.qclass_of[lname] = 'QLabel'

    def w_TMemo(self, o, offset, cls='QPlainTextEdit'):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="{cls}" name="{o.name}">')
        self.common_props(o, cls, offset=offset)
        lines = p.get('Lines.Strings')
        if isinstance(lines, list):
            ui.prop('plainText', 'string', '\n'.join(str(x) for x in lines))
        if p.get('ReadOnly') == Ident('True'):
            ui.prop('readOnly', 'bool', True)
        if p.get('WordWrap') == Ident('False'):
            ui.prop('lineWrapMode', 'enum', f'{cls}::NoWrap')
        sb = str(p.get('ScrollBars', 'ssNone'))
        if sb in ('ssVertical', 'ssBoth'):
            ui.prop('verticalScrollBarPolicy', 'enum', 'Qt::ScrollBarAlwaysOn')
        if sb in ('ssHorizontal', 'ssBoth'):
            ui.prop('horizontalScrollBarPolicy', 'enum', 'Qt::ScrollBarAlwaysOn')
        if sb == 'ssNone' and o.cls == 'TMemo':
            ui.prop('verticalScrollBarPolicy', 'enum', 'Qt::ScrollBarAsNeeded')
        if p.get('PlainText') == Ident('True') and cls == 'QTextEdit':
            ui.prop('acceptRichText', 'bool', False)
        ui.close('</widget>')
        self.qclass_of[o.name] = cls

    def w_TRichEdit(self, o, offset):
        self.w_TMemo(o, offset, 'QTextEdit')

    def w_TComboBox(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QComboBox" name="{o.name}">')
        self.common_props(o, 'QComboBox', offset=offset)
        style = str(p.get('Style', 'csDropDown'))
        editable = style in ('csDropDown', 'csSimple')
        ui.prop('editable', 'bool', editable)
        if isinstance(p.get('DropDownCount'), int):
            ui.prop('maxVisibleItems', 'number', p['DropDownCount'])
        else:
            ui.prop('maxVisibleItems', 'number', 8)
        if p.get('Sorted') == Ident('True'):
            ui.prop('vclSorted', 'bool', True, stdset=False)
        items = p.get('Items.Strings')
        if isinstance(items, list):
            for it in items:
                ui.w(f'<item><property name="text"><string>{esc(it)}</string></property></item>')
            idx = p.get('ItemIndex', -1)
            if isinstance(idx, int) and idx >= 0:
                ui.prop('currentIndex', 'number', idx)
        if editable and isinstance(p.get('Text'), str) and p['Text']:
            ui.prop('currentText', 'string', p['Text'])
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QComboBox'

    def w_TColorBox(self, o, offset):
        ui = self.ui
        self.used_custom.add('TColorBoxWidget')
        ui.open(f'<widget class="TColorBoxWidget" name="{o.name}">')
        self.common_props(o, 'TColorBoxWidget', offset=offset)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'TColorBoxWidget'

    def _check_like(self, o, offset, cls):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="{cls}" name="{o.name}">')
        self.common_props(o, cls, offset=offset)
        ui.prop('text', 'string', p.get('Caption', ''))
        if p.get('Checked') == Ident('True') or p.get('State') == Ident('cbChecked'):
            ui.prop('checked', 'bool', True)
        if cls == 'QCheckBox' and p.get('AllowGrayed') == Ident('True'):
            ui.prop('tristate', 'bool', True)
        if cls == 'QCheckBox' and p.get('State') == Ident('cbGrayed'):
            ui.prop('tristate', 'bool', True)
            ui.prop('vclGrayed', 'bool', True, stdset=False)
        if p.get('WordWrap') == Ident('True'):
            ui.prop('vclWordWrap', 'bool', True, stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = cls

    def w_TCheckBox(self, o, offset):
        self._check_like(o, offset, 'QCheckBox')

    def w_TRadioButton(self, o, offset):
        self._check_like(o, offset, 'QRadioButton')

    def _button_icon(self, o):
        p = o.props
        if isinstance(p.get('Glyph.Data'), bytes):
            try:
                return self.res.glyph(p['Glyph.Data'], p.get('NumGlyphs', 1))
            except Exception as e:  # pragma: no cover
                sys.stderr.write(f'  [warn] glyph {self.unit}.{o.name}: {e}\n')
        if 'ImageIndex' in p and 'Images' in p:
            return self.img_for(p['Images'], p['ImageIndex'])
        return None

    def w_TButton(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QPushButton" name="{o.name}">')
        self.common_props(o, 'QPushButton', offset=offset)
        ui.prop('text', 'string', p.get('Caption', ''))
        icon = self._button_icon(o)
        if icon:
            ui.prop('icon', 'icon', icon)
        if p.get('Default') == Ident('True'):
            ui.prop('default', 'bool', True)
        else:
            ui.prop('autoDefault', 'bool', False)
        if p.get('Cancel') == Ident('True'):
            ui.prop('vclCancel', 'bool', True, stdset=False)
        kind = str(p.get('Kind', 'bkCustom'))
        if kind != 'bkCustom':
            ui.prop('vclKind', 'notr', kind, stdset=False)
            if kind in ('bkOK', 'bkYes') and p.get('Default') is None:
                ui.prop('default', 'bool', True)
            if kind in ('bkCancel', 'bkNo'):
                ui.prop('vclCancel', 'bool', True, stdset=False)
        mr = p.get('ModalResult')
        kind_mr = {'bkOK': 'mrOk', 'bkCancel': 'mrCancel', 'bkYes': 'mrYes', 'bkNo': 'mrNo',
                   'bkAbort': 'mrAbort', 'bkRetry': 'mrRetry', 'bkIgnore': 'mrIgnore', 'bkAll': 'mrAll'}
        if mr is None and kind in kind_mr:
            mr = kind_mr[kind]
        if kind == 'bkClose':
            mr = mr or 'mrClose'
        if isinstance(mr, int):
            mr = {0: None, 1: 'mrOk', 2: 'mrCancel', 3: 'mrAbort', 4: 'mrRetry', 5: 'mrIgnore', 6: 'mrYes', 7: 'mrNo', 8: 'mrAll'}.get(mr, str(mr))
        if mr and mr != 'mrNone':
            ui.prop('vclModalResult', 'notr', str(mr), stdset=False)
        if 'OnClick' in p:
            ui.prop('vclOnClick', 'notr', str(p['OnClick']), stdset=False)
        if p.get('WordWrap') == Ident('True'):
            ui.prop('vclWordWrap', 'bool', True, stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QPushButton'

    w_TBitBtn = w_TButton

    def w_TSpeedButton(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QToolButton" name="{o.name}">')
        self.common_props(o, 'QToolButton', offset=offset)
        cap = p.get('Caption', '')
        if cap:
            ui.prop('text', 'string', cap)
        icon = self._button_icon(o)
        if icon:
            ui.prop('icon', 'icon', icon)
            ui.prop('iconSize', 'size', (16, 16))
        if icon and cap:
            ui.prop('toolButtonStyle', 'enum', 'Qt::ToolButtonTextBesideIcon')
        elif cap:
            ui.prop('toolButtonStyle', 'enum', 'Qt::ToolButtonTextOnly')
        if p.get('Flat') == Ident('True'):
            ui.prop('autoRaise', 'bool', True)
        gi = p.get('GroupIndex', 0)
        if isinstance(gi, int) and gi > 0:
            ui.prop('checkable', 'bool', True)
            if p.get('Down') == Ident('True'):
                ui.prop('checked', 'bool', True)
            ui.prop('vclGroupIndex', 'number', gi, stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QToolButton'

    def w_TSpinEdit(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QSpinBox" name="{o.name}">')
        self.common_props(o, 'QSpinBox', offset=offset)
        mn = p.get('MinValue', 0)
        mx = p.get('MaxValue', 0)
        if mn == 0 and mx == 0:
            mn, mx = -2147483647, 2147483647
        ui.prop('minimum', 'number', mn)
        ui.prop('maximum', 'number', mx)
        if isinstance(p.get('Value'), int):
            ui.prop('value', 'number', p['Value'])
        if isinstance(p.get('Increment'), int):
            ui.prop('singleStep', 'number', p['Increment'])
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QSpinBox'

    def _frame_shape(self, p, default_outer='bvRaised'):
        outer = str(p.get('BevelOuter', default_outer))
        inner = str(p.get('BevelInner', 'bvNone'))
        border = str(p.get('BorderStyle', 'bsNone'))
        if border == 'bsSingle':
            return 'QFrame::Box', 'QFrame::Plain'
        if outer == 'bvNone' and inner == 'bvNone':
            return 'QFrame::NoFrame', 'QFrame::Plain'
        if outer == 'bvLowered' or (outer == 'bvNone' and inner == 'bvLowered'):
            return 'QFrame::Panel', 'QFrame::Sunken'
        return 'QFrame::Panel', 'QFrame::Raised'

    def w_TPanel(self, o, offset, default_outer='bvRaised'):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QFrame" name="{o.name}">')
        self.common_props(o, 'QFrame', offset=offset)
        shape, shadow = self._frame_shape(p, default_outer)
        ui.prop('frameShape', 'enum', shape)
        ui.prop('frameShadow', 'enum', shadow)
        ui.prop('vclClass', 'notr', o.cls, stdset=False)
        cap = p.get('Caption', '')
        if o.cls == 'TPanel' and cap and p.get('ShowCaption') != Ident('False'):
            # VCL paints the caption centered on the panel
            lname = self.uniq(o.name + '_Caption')
            ui.open(f'<widget class="QLabel" name="{lname}">')
            ui.prop('geometry', 'geometry', (0, 0, int(p.get('Width', 0)), int(p.get('Height', 0))))
            ui.prop('text', 'string', strip_accel(cap))
            al = {'taLeftJustify': 'Qt::AlignLeft', 'taRightJustify': 'Qt::AlignRight'}.get(str(p.get('Alignment', '')), 'Qt::AlignHCenter')
            ui.prop('alignment', 'set', f'{al}|Qt::AlignVCenter')
            ui.prop('vclAlign', 'notr', 'alClient', stdset=False)
            ui.prop('vclPanelCaptionOf', 'notr', o.name, stdset=False)
            ui.close('</widget>')
        self.emit_children(o)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QFrame'

    def w_TCoolBar(self, o, offset):
        self.w_TPanel(o, offset, default_outer='bvNone')

    w_TCategoryPanel = w_TPanel

    def w_TBevel(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QFrame" name="{o.name}">')
        self.common_props(o, 'QFrame', offset=offset)
        shape = str(p.get('Shape', 'bsBox'))
        style = str(p.get('Style', 'bsLowered'))
        qshape = {'bsBox': 'QFrame::Box', 'bsFrame': 'QFrame::Box', 'bsTopLine': 'QFrame::HLine',
                  'bsBottomLine': 'QFrame::HLine', 'bsLeftLine': 'QFrame::VLine', 'bsRightLine': 'QFrame::VLine',
                  'bsSpacer': 'QFrame::NoFrame'}.get(shape, 'QFrame::Box')
        ui.prop('frameShape', 'enum', qshape)
        ui.prop('frameShadow', 'enum', 'QFrame::Sunken' if style == 'bsLowered' else 'QFrame::Raised')
        ui.prop('vclClass', 'notr', 'TBevel', stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QFrame'

    def w_TShape(self, o, offset):
        self.w_TBevel(o, offset)

    def w_TGroupBox(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QGroupBox" name="{o.name}">')
        self.common_props(o, 'QGroupBox', offset=offset)
        ui.prop('title', 'string', p.get('Caption', ''))
        self.emit_children(o)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QGroupBox'

    def w_TRadioGroup(self, o, offset):
        p = o.props
        ui = self.ui
        self.used_custom.add('TRadioGroupWidget')
        ui.open(f'<widget class="TRadioGroupWidget" name="{o.name}">')
        self.common_props(o, 'TRadioGroupWidget', offset=offset)
        ui.prop('title', 'string', p.get('Caption', ''))
        items = p.get('Items.Strings')
        if isinstance(items, list):
            ui.prop('items', 'stringlist', [str(x) for x in items], stdset=False)
        if isinstance(p.get('Columns'), int):
            ui.prop('columns', 'number', p['Columns'], stdset=False)
        if isinstance(p.get('ItemIndex'), int):
            ui.prop('itemIndex', 'number', p['ItemIndex'], stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'TRadioGroupWidget'

    def w_TScrollBox(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QScrollArea" name="{o.name}">')
        self.common_props(o, 'QScrollArea', offset=offset)
        if str(p.get('BorderStyle', 'bsSingle')) == 'bsNone':
            ui.prop('frameShape', 'enum', 'QFrame::NoFrame')
        ui.prop('widgetResizable', 'bool', False)
        if p.get('HorzScrollBar.Visible') == Ident('False'):
            ui.prop('horizontalScrollBarPolicy', 'enum', 'Qt::ScrollBarAlwaysOff')
        if p.get('VertScrollBar.Visible') == Ident('False'):
            ui.prop('verticalScrollBarPolicy', 'enum', 'Qt::ScrollBarAlwaysOff')
        # contents size: bounding box of the children
        wmax = int(p.get('Width', 0)) - 4
        hmax = int(p.get('Height', 0)) - 4
        for c in o.children:
            cp = c.props
            if isinstance(cp.get('Left'), int) and isinstance(cp.get('Width'), int):
                wmax = max(wmax, cp['Left'] + cp['Width'])
            if isinstance(cp.get('Top'), int) and isinstance(cp.get('Height'), int):
                hmax = max(hmax, cp['Top'] + cp['Height'])
        inner = self.uniq(o.name + '_Contents')
        ui.open(f'<widget class="QWidget" name="{inner}">')
        ui.prop('geometry', 'geometry', (0, 0, wmax, hmax))
        ui.prop('vclScrollContents', 'bool', True, stdset=False)
        ui.prop('vclDesignWidth', 'number', wmax, stdset=False)
        ui.prop('vclDesignHeight', 'number', hmax, stdset=False)
        self.emit_children(o)
        ui.close('</widget>')
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QScrollArea'

    def w_TPageControl(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QTabWidget" name="{o.name}">')
        self.common_props(o, 'QTabWidget', offset=offset)
        pages = [c for c in o.children if c.cls == 'TTabSheet']
        order = {id(c): i for i, c in enumerate(pages)}
        pages.sort(key=lambda c: c.props['PageIndex'] if isinstance(c.props.get('PageIndex'), int) else order[id(c)])
        active = str(p.get('ActivePage', ''))
        idx = 0
        for i, pg in enumerate(pages):
            if pg.name == active:
                idx = i
        ui.prop('currentIndex', 'number', idx)
        if p.get('MultiLine') == Ident('True'):
            ui.prop('usesScrollButtons', 'bool', False)
        if str(p.get('TabPosition', '')) == 'tpBottom':
            ui.prop('tabPosition', 'enum', 'QTabWidget::South')
        if str(p.get('Style', '')) in ('tsButtons', 'tsFlatButtons'):
            ui.prop('documentMode', 'bool', True)
        for pg in pages:
            ui.open(f'<widget class="QWidget" name="{pg.name}">')
            ui.w(f'<attribute name="title"><string>{esc(pg.props.get("Caption", ""))}</string></attribute>')
            icon = self.img_for(p.get('Images'), pg.props.get('ImageIndex'))
            if icon:
                ui.w(f'<attribute name="icon"><iconset resource="../../resources/dfend.qrc"><normaloff>{esc(icon)}</normaloff></iconset></attribute>')
            # VCL tab sheet client area: page control minus border and one tab row
            ui.prop('vclDesignWidth', 'number', max(0, int(p.get('Width', 0)) - 8), stdset=False)
            ui.prop('vclDesignHeight', 'number', max(0, int(p.get('Height', 0)) - 28), stdset=False)
            if pg.props.get('TabVisible') == Ident('False'):
                ui.prop('vclTabVisible', 'bool', False, stdset=False)
            if isinstance(pg.props.get('Tag'), int) and pg.props['Tag']:
                ui.prop('tag', 'number', pg.props['Tag'], stdset=False)
            self.emit_children(pg)
            ui.close('</widget>')
            self.qclass_of[pg.name] = 'QWidget'
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QTabWidget'

    def w_TNotebook(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QStackedWidget" name="{o.name}">')
        self.common_props(o, 'QStackedWidget', offset=offset)
        pages = [c for c in o.children]
        names = []
        for i, pg in enumerate(pages):
            pname = pg.name or self.uniq(f'{o.name}_Page{i}')
            pg.name = pname
            names.append(str(pg.props.get('Caption', '')))
        ui.prop('currentIndex', 'number', int(p.get('PageIndex', 0)))
        ui.prop('vclPageNames', 'stringlist', names, stdset=False)
        for pg in pages:
            ui.open(f'<widget class="QWidget" name="{pg.name}">')
            ui.prop('vclPageCaption', 'notr', str(pg.props.get('Caption', '')), stdset=False)
            ui.prop('vclDesignWidth', 'number', int(p.get('Width', 0)), stdset=False)
            ui.prop('vclDesignHeight', 'number', int(p.get('Height', 0)), stdset=False)
            self.emit_children(pg)
            ui.close('</widget>')
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QStackedWidget'

    def w_TListView(self, o, offset):
        p = o.props
        ui = self.ui
        self.used_custom.add('TListView')
        ui.open(f'<widget class="TListView" name="{o.name}">')
        self.common_props(o, 'TListView', offset=offset)
        vs = str(p.get('ViewStyle', 'vsIcon'))
        ui.prop('viewStyle', 'notr', vs, stdset=False)
        cols = p.get('Columns') or []
        if cols:
            ui.prop('columnCaptions', 'stringlist', [str(c.get('Caption', '')) for c in cols], stdset=False)
            ui.prop('columnWidths', 'notr', ','.join(str(c.get('Width', 50)) for c in cols), stdset=False)
        if p.get('Checkboxes') == Ident('True'):
            ui.prop('checkboxes', 'bool', True, stdset=False)
        if p.get('MultiSelect') == Ident('True'):
            ui.prop('multiSelect', 'bool', True, stdset=False)
        ui.prop('readOnly', 'bool', p.get('ReadOnly') == Ident('True'), stdset=False)
        if p.get('ShowColumnHeaders') == Ident('False'):
            ui.prop('showColumnHeaders', 'bool', False, stdset=False)
        if p.get('HideSelection') == Ident('False'):
            ui.prop('hideSelection', 'bool', False, stdset=False)
        if p.get('GridLines') == Ident('True'):
            ui.prop('gridLines', 'bool', True, stdset=False)
        if p.get('RowSelect') == Ident('True'):
            ui.prop('rowSelect', 'bool', True, stdset=False)
        if p.get('SortType') and str(p.get('SortType')) != 'stNone':
            ui.prop('sortType', 'notr', str(p['SortType']), stdset=False)
        for k in ('SmallImages', 'LargeImages', 'StateImages'):
            if k in p:
                ui.prop('vcl' + k, 'notr', str(p[k]), stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'TListView'

    def w_TTreeView(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QTreeWidget" name="{o.name}">')
        self.common_props(o, 'QTreeWidget', offset=offset)
        ui.prop('headerHidden', 'bool', True)
        ui.prop('columnCount', 'number', 1)
        ui.w('<column><property name="text"><string notr="true">1</string></property></column>')
        if p.get('ShowRoot') == Ident('False') or p.get('ShowButtons') == Ident('False'):
            ui.prop('rootIsDecorated', 'bool', False)
        if p.get('ReadOnly') == Ident('True'):
            ui.prop('editTriggers', 'set', 'QAbstractItemView::NoEditTriggers')
        else:
            ui.prop('editTriggers', 'set', 'QAbstractItemView::EditKeyPressed|QAbstractItemView::SelectedClicked')
        if isinstance(p.get('Indent'), int):
            ui.prop('indentation', 'number', p['Indent'])
        if p.get('MultiSelect') == Ident('True'):
            ui.prop('selectionMode', 'enum', 'QAbstractItemView::ExtendedSelection')
        if 'Images' in p:
            ui.prop('vclImages', 'notr', str(p['Images']), stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QTreeWidget'

    def w_TListBox(self, o, offset, checklist=False):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QListWidget" name="{o.name}">')
        self.common_props(o, 'QListWidget', offset=offset)
        if p.get('MultiSelect') == Ident('True'):
            ui.prop('selectionMode', 'enum',
                    'QAbstractItemView::ExtendedSelection' if p.get('ExtendedSelect') != Ident('False') else 'QAbstractItemView::MultiSelection')
        if p.get('Sorted') == Ident('True'):
            ui.prop('sortingEnabled', 'bool', True)
        if checklist:
            ui.prop('vclCheckList', 'bool', True, stdset=False)
        if isinstance(p.get('Columns'), int) and p['Columns'] > 0:
            ui.prop('flow', 'enum', 'QListView::TopToBottom')
            ui.prop('isWrapping', 'bool', True)
        items = p.get('Items.Strings')
        if isinstance(items, list):
            for it in items:
                ui.open('<item>')
                ui.w(f'<property name="text"><string>{esc(it)}</string></property>')
                if checklist:
                    ui.w('<property name="checkState"><enum>Unchecked</enum></property>')
                ui.close('</item>')
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QListWidget'

    def w_TCheckListBox(self, o, offset):
        self.w_TListBox(o, offset, checklist=True)

    def w_TStringGrid(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QTableWidget" name="{o.name}">')
        self.common_props(o, 'QTableWidget', offset=offset)
        cols = int(p.get('ColCount', 5))
        rows = int(p.get('RowCount', 5))
        fixed_cols = int(p.get('FixedCols', 1))
        fixed_rows = int(p.get('FixedRows', 1))
        ui.prop('columnCount', 'number', cols - fixed_cols)
        ui.prop('rowCount', 'number', rows - fixed_rows)
        ui.prop('vclFixedCols', 'number', fixed_cols, stdset=False)
        ui.prop('vclFixedRows', 'number', fixed_rows, stdset=False)
        opts = p.get('Options') or []
        if fixed_rows == 0:
            ui.w('<attribute name="horizontalHeaderVisible"><bool>false</bool></attribute>')
        if fixed_cols == 0:
            ui.w('<attribute name="verticalHeaderVisible"><bool>false</bool></attribute>')
        if 'goEditing' not in opts:
            ui.prop('editTriggers', 'set', 'QAbstractItemView::NoEditTriggers')
        if 'goRowSelect' in opts:
            ui.prop('selectionBehavior', 'enum', 'QAbstractItemView::SelectRows')
        if isinstance(p.get('DefaultRowHeight'), int):
            ui.w(f'<attribute name="verticalHeaderDefaultSectionSize"><number>{p["DefaultRowHeight"]}</number></attribute>')
        else:
            ui.w('<attribute name="verticalHeaderDefaultSectionSize"><number>24</number></attribute>')
        cw = p.get('ColWidths')
        if isinstance(cw, list):
            ui.prop('vclColumnWidths', 'notr', ','.join(str(x) for x in cw[fixed_cols:]), stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QTableWidget'

    def w_TValueListEditor(self, o, offset):
        p = o.props
        ui = self.ui
        self.used_custom.add('TValueListEditorWidget')
        ui.open(f'<widget class="TValueListEditorWidget" name="{o.name}">')
        self.common_props(o, 'TValueListEditorWidget', offset=offset)
        cw = p.get('ColWidths')
        if isinstance(cw, list):
            ui.prop('vclColumnWidths', 'notr', ','.join(str(x) for x in cw), stdset=False)
        tc = p.get('TitleCaptions.Strings')
        if isinstance(tc, list):
            ui.prop('titleCaptions', 'stringlist', [str(x) for x in tc], stdset=False)
        st = p.get('Strings.Strings')
        if isinstance(st, list):
            st = [str(x) for x in st if str(x) != '=']
            if st:
                ui.prop('strings', 'stringlist', st, stdset=False)
        ko = p.get('KeyOptions') or []
        if 'keyEdit' in ko:
            ui.prop('keyEdit', 'bool', True, stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'TValueListEditorWidget'

    def w_TTrackBar(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QSlider" name="{o.name}">')
        self.common_props(o, 'QSlider', offset=offset)
        ori = 'Qt::Vertical' if str(p.get('Orientation', 'trHorizontal')) == 'trVertical' else 'Qt::Horizontal'
        ui.prop('orientation', 'enum', ori)
        ui.prop('minimum', 'number', int(p.get('Min', 0)))
        ui.prop('maximum', 'number', int(p.get('Max', 10)))
        if isinstance(p.get('Position'), int):
            ui.prop('value', 'number', p['Position'])
        if isinstance(p.get('PageSize'), int):
            ui.prop('pageStep', 'number', p['PageSize'])
        ui.prop('tickInterval', 'number', int(p.get('Frequency', 1)))
        tm = str(p.get('TickMarks', 'tmBottomRight'))
        if str(p.get('TickStyle', 'tsAuto')) == 'tsNone':
            ui.prop('tickPosition', 'enum', 'QSlider::NoTicks')
        else:
            ui.prop('tickPosition', 'enum', {'tmBoth': 'QSlider::TicksBothSides', 'tmTopLeft': 'QSlider::TicksAbove'}.get(tm, 'QSlider::TicksBelow'))
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QSlider'

    def w_TProgressBar(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QProgressBar" name="{o.name}">')
        self.common_props(o, 'QProgressBar', offset=offset)
        ui.prop('minimum', 'number', int(p.get('Min', 0)))
        ui.prop('maximum', 'number', int(p.get('Max', 100)))
        ui.prop('value', 'number', int(p.get('Position', 0)))
        ui.prop('textVisible', 'bool', False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QProgressBar'

    def w_TStatusBar(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QStatusBar" name="{o.name}">')
        self.common_props(o, 'QStatusBar', offset=offset)
        if 'Align' not in p:
            ui.prop('vclAlign', 'notr', 'alBottom', stdset=False)
        ui.prop('sizeGripEnabled', 'bool', False)
        panels = p.get('Panels') or []
        if panels:
            ui.prop('vclPanelWidths', 'notr', ','.join(str(x.get('Width', 50)) for x in panels), stdset=False)
        if p.get('SimplePanel') == Ident('True'):
            ui.prop('vclSimplePanel', 'bool', True, stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QStatusBar'

    def w_TImage(self, o, offset):
        p = o.props
        ui = self.ui
        self.used_custom.add('TImageWidget')
        ui.open(f'<widget class="TImageWidget" name="{o.name}">')
        self.common_props(o, 'QLabel', offset=offset)
        if isinstance(p.get('Picture.Data'), bytes):
            try:
                path = self.res.picture(self.unit, o.name, p['Picture.Data'])
                if path:
                    ui.prop('pixmap', 'pixmap', path)
            except Exception as e:  # pragma: no cover
                sys.stderr.write(f'  [warn] picture {self.unit}.{o.name}: {e}\n')
        if p.get('Stretch') == Ident('True'):
            ui.prop('scaledContents', 'bool', True)
        if p.get('Proportional') == Ident('True'):
            ui.prop('proportional', 'bool', True, stdset=False)
        if p.get('Center') == Ident('True'):
            ui.prop('alignment', 'set', 'Qt::AlignCenter')
        else:
            ui.prop('alignment', 'set', 'Qt::AlignLeft|Qt::AlignTop')
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QLabel'

    def w_TPaintBox(self, o, offset):
        self.w_generic(o, offset)

    def w_TAnimate(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QLabel" name="{o.name}">')
        self.common_props(o, 'QLabel', offset=offset)
        ui.prop('vclClass', 'notr', 'TAnimate', stdset=False)
        if 'CommonAVI' in p:
            ui.prop('vclCommonAVI', 'notr', str(p['CommonAVI']), stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QLabel'

    def w_TSplitter(self, o, offset):
        p = o.props
        ui = self.ui
        self.used_custom.add('TSplitterWidget')
        # TSplitter defaults (not stored in the DFM): Width = 3, Align = alLeft;
        # alTop/alBottom splitters store Height but default Width is unused
        if 'Width' not in p and p.get('Align', Ident('alLeft')) in (Ident('alLeft'), Ident('alRight')):
            p['Width'] = 3
        if 'Height' not in p and p.get('Align') in (Ident('alTop'), Ident('alBottom')):
            p['Height'] = 3
        ui.open(f'<widget class="TSplitterWidget" name="{o.name}">')
        self.common_props(o, 'TSplitterWidget', offset=offset)
        if 'Align' not in p:
            ui.prop('vclAlign', 'notr', 'alLeft', stdset=False)
        if isinstance(p.get('MinSize'), int):
            ui.prop('minSize', 'number', p['MinSize'], stdset=False)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'TSplitterWidget'

    # -------------------- toolbar
    def w_TToolBar(self, o, offset):
        p = o.props
        ui = self.ui
        ui.open(f'<widget class="QToolBar" name="{o.name}">')
        self.common_props(o, 'QToolBar', offset=offset)
        ui.prop('movable', 'bool', False)
        ui.prop('floatable', 'bool', False)
        bw = int(p.get('ButtonWidth', 23))
        bh = int(p.get('ButtonHeight', 22))
        img = p.get('Images')
        isize = 16
        ui.prop('iconSize', 'size', (isize, isize))
        if p.get('ShowCaptions') == Ident('True'):
            if p.get('List') == Ident('True'):
                ui.prop('toolButtonStyle', 'enum', 'Qt::ToolButtonTextBesideIcon')
            else:
                ui.prop('toolButtonStyle', 'enum', 'Qt::ToolButtonTextUnderIcon')
        else:
            ui.prop('toolButtonStyle', 'enum', 'Qt::ToolButtonIconOnly')
        ui.prop('vclButtonWidth', 'number', bw, stdset=False)
        ui.prop('vclButtonHeight', 'number', bh, stdset=False)
        if img:
            ui.prop('vclImages', 'notr', str(img), stdset=False)
        if p.get('Wrapable') == Ident('False'):
            ui.prop('vclWrapable', 'bool', False, stdset=False)
        # children: ordered by Left/Top (VCL arranges toolbuttons by position)
        kids = sorted(o.children, key=lambda c: (int(c.props.get('Top', 0)), int(c.props.get('Left', 0))))
        pos = 0
        embedded = []
        for c in kids:
            if c.cls == 'TToolButton':
                style = str(c.props.get('Style', 'tbsButton'))
                if style in ('tbsSeparator', 'tbsDivider'):
                    if c.name:
                        # named separator action (VclLayout turns it into a separator) so that
                        # code like ToolButton5.Visible := False keeps working
                        ui.w(f'<addaction name="{c.name}"/>')
                        self.actions.append((c.name, {'vclSeparator': True, 'visible': c.props.get('Visible') != Ident('False')}))
                    else:
                        ui.w('<addaction name="separator"/>')
                else:
                    ui.w(f'<addaction name="{c.name}"/>')
                    props = {'text': c.props.get('Caption', ''), 'toolTip': c.props.get('Hint', '')}
                    icon = self.img_for(img, c.props.get('ImageIndex'))
                    if icon:
                        props['icon'] = icon
                    if style == 'tbsCheck':
                        props['checkable'] = True
                        if c.props.get('Down') == Ident('True'):
                            props['checked'] = True
                    if style == 'tbsDropDown':
                        props['vclDropDown'] = True
                    if c.props.get('DropdownMenu'):
                        props['vclDropdownMenu'] = str(c.props['DropdownMenu'])
                    if c.props.get('Visible') == Ident('False'):
                        props['visible'] = False
                    if c.props.get('Enabled') == Ident('False'):
                        props['enabled'] = False
                    if isinstance(c.props.get('Tag'), int) and c.props['Tag']:
                        props['tag'] = c.props['Tag']
                    self.actions.append((c.name, props))
                pos += 1
            else:
                embedded.append((pos, c))
                pos += 1
        for pos_i, c in embedded:
            # emitted as child widget; VclLayout inserts it into the toolbar at runtime
            self.pending_extra = {'vclToolbarPos': pos_i}
            self.emit(c)
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QToolBar'

    def w_TToolButton(self, o, offset):
        # Only reached if a TToolButton lives outside a TToolBar
        self.w_TSpeedButton(o, offset)

    # -------------------- menus
    def menu_items(self, items, images, parent_is_menubar=False):
        """Emit QMenu widgets for sub menus; returns list of addaction names."""
        ui = self.ui
        names = []
        for it in items:
            cap = it.props.get('Caption', '')
            if cap == '-':
                names.append(('separator', it))
                continue
            if it.children:
                ui.open(f'<widget class="QMenu" name="{it.name}">')
                ui.prop('title', 'string', cap)
                icon = self.img_for(images, it.props.get('ImageIndex'))
                if icon:
                    ui.prop('icon', 'icon', icon)
                if it.props.get('Visible') == Ident('False'):
                    ui.prop('vclMenuVisible', 'bool', False, stdset=False)
                if it.props.get('Enabled') == Ident('False'):
                    ui.prop('vclMenuEnabled', 'bool', False, stdset=False)
                if isinstance(it.props.get('Tag'), int) and it.props['Tag']:
                    ui.prop('tag', 'number', it.props['Tag'], stdset=False)
                sub = self.menu_items(it.children, images)
                for n, sit in sub:
                    if n == 'separator':
                        if sit.name:
                            self.actions.append((sit.name, {'vclSeparator': True, 'visible': sit.props.get('Visible') != Ident('False')}))
                            ui.w(f'<addaction name="{sit.name}"/>')
                        else:
                            ui.w('<addaction name="separator"/>')
                    else:
                        ui.w(f'<addaction name="{n}"/>')
                ui.close('</widget>')
                names.append((it.name, it))
                self.qclass_of[it.name] = 'QMenu'
            else:
                props = {'text': cap}
                icon = self.img_for(images, it.props.get('ImageIndex'))
                if icon:
                    props['icon'] = icon
                sc = shortcut_to_qt(it.props.get('ShortCut'))
                if sc:
                    props['shortcut'] = sc
                if it.props.get('Checked') == Ident('True') or it.props.get('AutoCheck') == Ident('True') or it.props.get('RadioItem') == Ident('True'):
                    props['checkable'] = True
                if it.props.get('Checked') == Ident('True'):
                    props['checked'] = True
                if it.props.get('RadioItem') == Ident('True'):
                    props['vclRadioItem'] = True
                    props['vclGroupIndex'] = int(it.props.get('GroupIndex', 0))
                if it.props.get('Visible') == Ident('False'):
                    props['visible'] = False
                if it.props.get('Enabled') == Ident('False'):
                    props['enabled'] = False
                if it.props.get('Default') == Ident('True'):
                    props['vclDefault'] = True
                if isinstance(it.props.get('Hint'), str) and it.props['Hint']:
                    props['statusTip'] = it.props['Hint']
                if isinstance(it.props.get('Tag'), int) and it.props['Tag']:
                    props['tag'] = it.props['Tag']
                self.actions.append((it.name, props))
                names.append((it.name, it))
        return names

    def w_TPopupMenu(self, o, offset):
        ui = self.ui
        ui.open(f'<widget class="QMenu" name="{o.name}">')
        ui.prop('vclPopupMenu', 'bool', True, stdset=False)
        if o.props.get('AutoPopup') == Ident('False'):
            ui.prop('vclAutoPopup', 'bool', False, stdset=False)
        names = self.menu_items(o.children, o.props.get('Images'))
        for n, it in names:
            if n == 'separator':
                if it.name:
                    self.actions.append((it.name, {'vclSeparator': True, 'visible': it.props.get('Visible') != Ident('False')}))
                    ui.w(f'<addaction name="{it.name}"/>')
                else:
                    ui.w('<addaction name="separator"/>')
            else:
                ui.w(f'<addaction name="{n}"/>')
        ui.close('</widget>')
        self.qclass_of[o.name] = 'QMenu'

    def emit_menubar(self, menu, name='menubar', geometry=None):
        ui = self.ui
        ui.open(f'<widget class="QMenuBar" name="{menu.name}">')
        if geometry:
            ui.prop('geometry', 'geometry', geometry)
        names = self.menu_items(menu.children, menu.props.get('Images'), parent_is_menubar=True)
        for n, it in names:
            if n != 'separator':
                ui.w(f'<addaction name="{n}"/>')
        ui.close('</widget>')
        self.qclass_of[menu.name] = 'QMenuBar'

    def emit_actions(self):
        ui = self.ui
        for name, props in self.actions:
            ui.open(f'<action name="{name}">')
            if props.get('vclSeparator'):
                ui.prop('vclSeparator', 'bool', True, stdset=False)
                if props.get('visible') is False:
                    ui.prop('visible', 'bool', False)
                ui.close('</action>')
                continue
            for k, v in props.items():
                if k == 'text':
                    ui.prop('text', 'string', v)
                elif k == 'toolTip':
                    if v:
                        ui.prop('toolTip', 'string', v)
                elif k == 'statusTip':
                    ui.prop('statusTip', 'string', v)
                elif k == 'icon':
                    ui.prop('icon', 'icon', v)
                elif k == 'shortcut':
                    ui.prop('shortcut', 'shortcut', v)
                elif k in ('checkable', 'checked', 'visible', 'enabled'):
                    ui.prop(k, 'bool', v)
                elif k == 'tag':
                    ui.prop('tag', 'number', v, stdset=False)
                elif k in ('vclDropDown', 'vclRadioItem', 'vclDefault'):
                    ui.prop(k, 'bool', v, stdset=False)
                elif k == 'vclGroupIndex':
                    ui.prop(k, 'number', v, stdset=False)
                elif k == 'vclDropdownMenu':
                    ui.prop(k, 'notr', v, stdset=False)
            ui.close('</action>')

    # -------------------- top level
    def convert(self):
        root = self.root
        p = root.props
        ui = self.ui
        self.qclass_of = {}
        # pre-extract image lists
        for o in root.walk():
            if o.cls == 'TImageList' and isinstance(o.props.get('Bitmap'), bytes):
                try:
                    self.imagelists[o.name] = self.res.imagelist(self.unit, o.name, o.props['Bitmap'])
                except Exception as e:  # pragma: no cover
                    sys.stderr.write(f'  [warn] imagelist {self.unit}.{o.name}: {e}\n')
        name = self.form_ui_name
        menu = None
        if 'Menu' in p:
            for c in root.children:
                if c.name == str(p['Menu']):
                    menu = c
        ui.w('<?xml version="1.0" encoding="UTF-8"?>')
        ui.open('<ui version="4.0">')
        ui.w(f'<class>{name}</class>')
        if self.base_kind == 'mainform':
            top_cls = 'QMainWindow'
        elif self.base_kind == 'frame':
            top_cls = 'QWidget'
        else:
            top_cls = 'QDialog'
        ui.open(f'<widget class="{top_cls}" name="{name}">')
        if self.base_kind == 'frame':
            w, h = int(p.get('Width', 400)), int(p.get('Height', 300))
        else:
            w, h = int(p.get('ClientWidth', p.get('Width', 400))), int(p.get('ClientHeight', p.get('Height', 300)))
        menu_h = 21 if menu is not None else 0
        ui.prop('geometry', 'geometry', (0, 0, w, h + (menu_h if top_cls == 'QMainWindow' else 0)))
        if self.base_kind != 'frame':
            ui.prop('windowTitle', 'string', p.get('Caption', ''))
            for k in ('BorderStyle', 'Position', 'WindowState', 'FormStyle'):
                if k in p:
                    ui.prop('vcl' + k, 'notr', str(p[k]), stdset=False)
            if 'BorderIcons' in p:
                ui.prop('vclBorderIcons', 'notr', ','.join(p['BorderIcons']), stdset=False)
            if p.get('KeyPreview') == Ident('True'):
                ui.prop('vclKeyPreview', 'bool', True, stdset=False)
            if isinstance(p.get('Constraints.MinWidth'), int):
                ui.prop('vclMinWidth', 'number', p['Constraints.MinWidth'], stdset=False)
            if isinstance(p.get('Constraints.MinHeight'), int):
                ui.prop('vclMinHeight', 'number', p['Constraints.MinHeight'], stdset=False)
            if isinstance(p.get('Icon.Data'), bytes):
                path = self.res.icon(self.unit, name, p['Icon.Data'])
                ui.prop('windowIcon', 'icon', path)
            if p.get('Color') and p.get('Color') != Ident('clBtnFace'):
                css = color_to_css(p['Color'])
                if css:
                    ui.prop('styleSheet', 'notr', f'#{name} {{ background-color: {css}; }}')
        ui.prop('vclDesignWidth', 'number', w, stdset=False)
        ui.prop('vclDesignHeight', 'number', h, stdset=False)
        if top_cls == 'QMainWindow':
            ui.open('<widget class="QWidget" name="centralwidget">')
            ui.prop('vclDesignWidth', 'number', w, stdset=False)
            ui.prop('vclDesignHeight', 'number', h, stdset=False)
            for c in root.children:
                if c is menu:
                    continue
                self.emit(c)
            ui.close('</widget>')
            if menu is not None:
                self.emit_menubar(menu, geometry=(0, 0, w, menu_h))
        elif menu is not None:
            # dialog with menu: vertical layout [menubar, client widget]
            ui.open('<layout class="QVBoxLayout" name="vclMenuLayout" stretch="0,1">')
            ui.w('<property name="spacing"><number>0</number></property>')
            for side in ('leftMargin', 'topMargin', 'rightMargin', 'bottomMargin'):
                ui.w(f'<property name="{side}"><number>0</number></property>')
            ui.open('<item>')
            self.emit_menubar(menu)
            ui.close('</item>')
            ui.open('<item>')
            ui.open('<widget class="QWidget" name="vclClientArea">')
            ui.prop('minimumSize', 'size', (0, 0))
            ui.prop('vclDesignWidth', 'number', w, stdset=False)
            ui.prop('vclDesignHeight', 'number', h, stdset=False)
            for c in root.children:
                if c is menu:
                    continue
                self.emit(c)
            ui.close('</widget>')
            ui.close('</item>')
            ui.close('</layout>')
        else:
            for c in root.children:
                self.emit(c)
        ui.close('</widget>')
        # actions must be declared inside the top level widget in Designer's format;
        # uic accepts <action> elements as children of the top level widget.
        # Re-open: insert actions before the closing tag of the top widget.
        closing = ui.lines.pop()
        ui.ind += 1
        self.emit_actions()
        ui.ind -= 1
        ui.lines.append(closing)
        if self.used_custom:
            ui.open('<customwidgets>')
            for cw in sorted(self.used_custom):
                ext, hdr = CUSTOM_WIDGETS[cw]
                ui.open('<customwidget>')
                ui.w(f'<class>{cw}</class>')
                ui.w(f'<extends>{ext}</extends>')
                ui.w(f'<header>{hdr}</header>')
                ui.close('</customwidget>')
            ui.close('</customwidgets>')
        self.collect_tab_order(root)
        stops = [s for s in self.tabstops if self.qclass_of.get(s) in FOCUSABLE]
        if len(stops) > 1:
            ui.open('<tabstops>')
            for s in stops:
                ui.w(f'<tabstop>{s}</tabstop>')
            ui.close('</tabstops>')
        ui.w('<resources><include location="../../resources/dfend.qrc"/></resources>')
        ui.w('<connections/>')
        ui.close('</ui>')
        return '\n'.join(ui.lines) + '\n'


def detect_base_kind(pas_path, root):
    try:
        text = open(pas_path, encoding='cp1252', errors='replace').read()
    except OSError:
        return 'form'
    m = re.search(r'\b' + re.escape(root.cls) + r'\s*=\s*class\s*\(\s*(\w+)', text, re.I)
    if m and m.group(1).lower() == 'tframe':
        return 'frame'
    if root.cls == 'TDFendReloadedMainForm':
        return 'mainform'
    return 'form'


# D-Fend Reborn: names derived from the Delphi sources that contain the old
# program name (main form class, widget names, icon file names)
def reborn_name(s):
    return s.replace('DFendReloaded', 'DFendReborn')


def main():
    src, out_forms, out_res = sys.argv[1:4]
    os.makedirs(out_forms, exist_ok=True)
    import shutil
    for sub in ('glyphs', 'imagelists', 'pictures', 'icons'):
        shutil.rmtree(os.path.join(out_res, sub), ignore_errors=True)
    res = ResourceStore(out_res)
    count = 0
    index = []
    for fn in sorted(os.listdir(src)):
        if not fn.lower().endswith('.dfm'):
            continue
        unit = fn[:-4]
        root = parse_dfm(os.path.join(src, fn))
        kind = detect_base_kind(os.path.join(src, unit + '.pas'), root)
        ui_name = reborn_name(root.cls[1:] if root.cls.startswith('T') else root.cls)
        conv = Converter(unit, root, kind, res, ui_name)
        xml = reborn_name(conv.convert())
        with open(os.path.join(out_forms, unit + '.ui'), 'w', encoding='utf-8') as f:
            f.write(xml)
        index.append(f'{unit}\t{root.cls}\t{kind}\tUi::{ui_name}')
        count += 1
    res.write_qrc(os.path.join(out_res, 'dfend.qrc'))
    with open(os.path.join(out_forms, 'FORMS_INDEX.txt'), 'w') as f:
        f.write('# unit\tdelphi class\tkind\tgenerated Ui class\n' + '\n'.join(index) + '\n')
    print(f'converted {count} forms, {len(res.files)} resource files')


if __name__ == '__main__':
    main()
