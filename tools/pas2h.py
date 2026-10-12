#!/usr/bin/env python3
"""Generate C++ headers from the interface sections of Delphi units.

Usage:
    pas2h.py <delphi-source-dir> <out-core-dir> <out-forms-dir> [unit ...]

For every unit the interface section (constants, types, classes, records,
global variables and routine declarations) is translated mechanically into a
C++17/Qt6 header following the conventions in PORTING.md:

  * identifiers keep their Delphi names
  * every Delphi class instance becomes a pointer (TGame*, TStringList*)
  * property P becomes the accessor pair P() / SetP(value)
  * var/out parameters become references
  * forms derive from TVclForm, frames from QWidget, the main form from QMainWindow
  * DFM components are not declared - they are available as ui.<Name>

The generated header is a starting point: the person (or agent) porting the
implementation may adjust it, but should keep public signatures stable.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# Units that are not ported (replaced by Qt / compat layer)
# ---------------------------------------------------------------------------
SKIP_UNITS = {
    'FastMM4', 'FastMM4Messages', 'FastMM4HTTPFixes', 'GIFImage', 'HashAlgEngine_U', 'HashAlgMD5Engine_U',
    'HashAlgMD5_U', 'HashAlg_U', 'HashValue_U', 'SDUGeneral', 'Resample', 'FileNameConvertor.old1',
    'FileNameConvertor.old2', 'FileNameConvertor.old3', 'ResHookUnit', 'VistaToolsUnit', 'MediaInterface', 'PacakgeManagerRepositoriesEditFormUnit',
}

CPP_KEYWORDS = {
    'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case', 'catch',
    'char', 'class', 'compl', 'const', 'constexpr', 'continue', 'decltype', 'default', 'delete', 'do', 'double',
    'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float', 'for', 'friend', 'goto', 'if', 'inline',
    'int', 'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or',
    'or_eq', 'private', 'protected', 'public', 'register', 'return', 'short', 'signed', 'sizeof', 'static',
    'struct', 'switch', 'template', 'this', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union',
    'unsigned', 'using', 'virtual', 'void', 'volatile', 'while', 'xor', 'xor_eq', 'signals', 'slots', 'emit',
    'foreach', 'forever', 'small', 'near', 'far', 'interface', 'min', 'max',
}

SIMPLE_TYPES = {
    'string': 'QString', 'ansistring': 'QString', 'widestring': 'QString', 'shortstring': 'QString',
    'unicodestring': 'QString', 'utf8string': 'QString',
    'integer': 'int', 'longint': 'int', 'smallint': 'qint16', 'shortint': 'qint8', 'byte': 'quint8',
    'word': 'quint16', 'longword': 'quint32', 'dword': 'quint32', 'cardinal': 'quint32', 'int64': 'qint64',
    'uint64': 'quint64', 'boolean': 'bool', 'bytebool': 'bool', 'longbool': 'bool', 'wordbool': 'bool',
    'char': 'QChar', 'ansichar': 'QChar', 'widechar': 'QChar', 'double': 'double', 'extended': 'double',
    'real': 'double', 'single': 'float', 'currency': 'double', 'comp': 'qint64', 'tdatetime': 'TDateTime',
    'tdate': 'TDateTime', 'ttime': 'TDateTime', 'variant': 'QVariant', 'olevariant': 'QVariant',
    'pointer': 'void*', 'pchar': 'QString', 'thandle': 'THandle', 'hwnd': 'WId', 'tcolor': 'TColor',
    'tnotifyevent': 'TNotifyEvent', 'tshiftstate': 'TShiftState', 'tmodalresult': 'int',
    'tfontcharset': 'int', 'tpoint': 'QPoint', 'trect': 'QRect', 'hicon': 'QIcon', 'tsize': 'QSize',
    'tmsgdlgtype': 'TMsgDlgType', 'tmsgdlgbuttons': 'TMsgDlgButtons',
    'tshortcut': 'QKeySequence', 'tcloseaction': 'TCloseAction', 'tmousebutton': 'Qt::MouseButton',
    'titemchange': 'int', 'hresult': 'long', 'tcompressstrength': 'int', 'tcustomdrawstate': 'int',
    'tcustomdrawstage': 'int', 'tworkmode': 'int', 'tprogressdetails': 'int', 'tpasswordbutton': 'int',
    'tgriddrawstate': 'int', 'tedittyle': 'int', 'tseditstyle': 'int', 'tlistsortcompare': 'std::function<int(void*, void*)>',
    'thelpevent': 'TNotifyEvent', 'teditstyle': 'int',
}

# Delphi class types that map to Qt/compat classes (always used as pointer)
CLASS_MAP = {
    'tobject': 'QObject', 'tcomponent': 'QWidget', 'tcontrol': 'QWidget', 'twincontrol': 'QWidget',
    'tform': 'QWidget', 'tcustomform': 'QWidget', 'tframe': 'QWidget', 'tcustomframe': 'QWidget',
    'tstringlist': 'TStringList', 'tstrings': 'TStringList', 'tlist': 'TList', 'tobjectlist': 'TList',
    'tlistview': 'TListView', 'tlistitem': 'TListItem', 'ttreeview': 'QTreeWidget', 'ttreenode': 'QTreeWidgetItem',
    'timagelist': 'TImageList', 'tchecklistbox': 'QListWidget', 'tlistbox': 'QListWidget',
    'tpopupmenu': 'QMenu', 'tmenuitem': 'QAction', 'tmainmenu': 'QMenuBar', 'tmenu': 'QMenu',
    'tcombobox': 'QComboBox', 'tedit': 'QLineEdit', 'tlabelededit': 'QLineEdit', 'tmemo': 'QPlainTextEdit',
    'trichedit': 'QTextEdit', 'tlabel': 'QLabel', 'tbutton': 'QPushButton', 'tbitbtn': 'QPushButton',
    'tspeedbutton': 'QToolButton', 'tcheckbox': 'QCheckBox', 'tradiobutton': 'QRadioButton',
    'tpanel': 'QFrame', 'tgroupbox': 'QGroupBox', 'ttabsheet': 'QWidget', 'tpagecontrol': 'QTabWidget',
    'ttimer': 'QTimer', 'tstatusbar': 'QStatusBar', 'tprogressbar': 'QProgressBar', 'ttoolbar': 'QToolBar',
    'ttoolbutton': 'QAction', 'tstringgrid': 'QTableWidget', 'tvaluelisteditor': 'TValueListEditorWidget',
    'tradiogroup': 'TRadioGroupWidget', 'tspinedit': 'QSpinBox', 'timage': 'TImageWidget',
    'tscrollbox': 'QScrollArea', 'ttrackbar': 'QSlider', 'tsplitter': 'TSplitterWidget',
    'tstream': 'QIODevice', 'tfilestream': 'QFile', 'tmemorystream': 'QBuffer', 'tstringstream': 'QBuffer',
    'tthread': 'QThread', 'ttrayicon': 'QSystemTrayIcon', 'tcolorbox': 'TColorBoxWidget',
    'tpersistent': 'QObject', 'tcontrolbar': 'QWidget', 'tcoolbar': 'QFrame', 'tnotebook': 'QStackedWidget',
    'tpaintbox': 'QWidget', 'tbevel': 'QFrame', 'tshape': 'QFrame', 'topendialog': 'TOpenDialog',
    'tsavedialog': 'TSaveDialog', 'tmeminifile': 'TMemIniFile', 'tinifile': 'TIniFile',
    'tcustominifile': 'TMemIniFile', 'tregistry': 'QSettings', 'tidhttp': 'QNetworkAccessManager',
    'txmldocument': 'QDomDocument', 'tcanvas': 'QPainter', 'tfont': 'QFont',
    'tmessage': 'QEvent', 'twmdropfiles': 'QEvent', 'tcmhintshow': 'QEvent', 'twmmove': 'QEvent',
    'tcustomlistview': 'TListView', 'tzipmaster': 'void', 'tsevenzip': 'void', 'tinplaceedit': 'QWidget',
    'tidftp': 'QNetworkAccessManager',
}
# value types (not pointers)
VALUE_CLASS_MAP = {
    'ixmlnode': 'QDomElement', 'ixmldocument': 'QDomDocument', 'ixmlnodelist': 'QDomNodeList',
    'tbitmap': 'QImage', 'tpicture': 'QPixmap', 'ticon': 'QIcon', 'tgifimage': 'QImage', 'tjpegimage': 'QImage',
    'tpngobject': 'QImage', 'tpngimage': 'QImage', 'tgraphic': 'QImage',
}

CPP_NAME_FIX = {}
CLASS_HAS_BASE = {}  # class name -> True if it derives from another (virtual) class


def fix_ident(name):
    if name in CPP_KEYWORDS:
        return name + '_'
    return name


# ---------------------------------------------------------------------------
# Lexing
# ---------------------------------------------------------------------------

def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "'":
            j = i + 1
            while j < n:
                if text[j] == "'":
                    if j + 1 < n and text[j + 1] == "'":
                        j += 2
                        continue
                    break
                j += 1
            out.append(text[i:j + 1])
            i = j + 1
        elif c == '{':
            j = text.find('}', i)
            j = n if j < 0 else j
            out.append(' ')
            i = j + 1
        elif text.startswith('(*', i):
            j = text.find('*)', i + 2)
            j = n if j < 0 else j + 1
            out.append(' ')
            i = j + 1
        elif text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<str>(?:'(?:[^']|'')*'|\#\$?[0-9A-Fa-f]+)+)
  | (?P<num>\$[0-9A-Fa-f]+|\d+\.\d+(?:[eE][-+]?\d+)?|\d+)
  | (?P<id>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<op>:=|<=|>=|<>|\.\.|[-+*/=<>()\[\],;:.^@])
""", re.X)


def tokenize(text):
    toks = []
    for m in TOKEN_RE.finditer(text):
        k = m.lastgroup
        if k == 'ws':
            continue
        toks.append((k, m.group(k)))
    return toks


def interface_section(text):
    t = strip_comments(text)
    m = re.search(r'\binterface\b', t, re.I)
    e = re.search(r'\bimplementation\b', t, re.I)
    if not m:
        return ''
    return t[m.end():e.start() if e else len(t)]


# ---------------------------------------------------------------------------
# Pascal expression -> C++ expression (constants / default values)
# ---------------------------------------------------------------------------

def pas_string_to_cpp(tok):
    out = []
    i = 0
    while i < len(tok):
        if tok[i] == "'":
            j = i + 1
            buf = []
            while j < len(tok):
                if tok[j] == "'":
                    if j + 1 < len(tok) and tok[j + 1] == "'":
                        buf.append("'")
                        j += 2
                        continue
                    break
                buf.append(tok[j])
                j += 1
            out.extend(buf)
            i = j + 1
        elif tok[i] == '#':
            j = i + 1
            hexa = False
            if j < len(tok) and tok[j] == '$':
                hexa = True
                j += 1
            k = j
            while k < len(tok) and (tok[k].isdigit() or (hexa and tok[k] in '0123456789abcdefABCDEF')):
                k += 1
            code = int(tok[j:k], 16 if hexa else 10)
            out.append(chr(code))
            i = k
        else:
            i += 1
    s = ''.join(out)
    esc = s.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '\\r').replace('\n', '\\n').replace('\t', '\\t')
    esc = ''.join(c if 32 <= ord(c) < 127 or c in '\\' else (c if ord(c) >= 127 else f'\\x{ord(c):02x}""') for c in esc)
    return f'"{esc}"'


OP_MAP = {'div': '/', 'mod': '%', 'shl': '<<', 'shr': '>>', 'and': '&', 'or': '|', 'xor': '^', 'not': '~',
          '=': '==', '<>': '!=', 'nil': 'nullptr', 'true': 'true', 'false': 'false'}


def expr_to_cpp(toks):
    out = []
    for k, v in toks:
        if k == 'str':
            out.append('QStringLiteral(' + pas_string_to_cpp(v) + ')')
        elif k == 'num':
            out.append('0x' + v[1:] if v.startswith('$') else v)
        elif k == 'id':
            lv = v.lower()
            if lv in OP_MAP:
                out.append(OP_MAP[lv])
            else:
                out.append(fix_ident(v))
        else:
            out.append(OP_MAP.get(v, v))
    s = ' '.join(out)
    s = s.replace('( ', '(').replace(' )', ')').replace(' ,', ',')
    return s


def expr_is_string(toks):
    return any(k == 'str' for k, v in toks)


# ---------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------

class Ctx:
    def __init__(self):
        self.classes = set()      # project class names (lower case) -> pointer semantics
        self.records = set()      # project record names (lower case)
        self.enums = set()
        self.interfaces = set()


def map_type(tname, ctx, as_field=False):
    """Map a (simple) Pascal type name to C++. Returns (cpp, kind) with kind in
    'simple', 'string', 'class', 'record', 'value'."""
    t = tname.strip()
    lt = t.lower()
    if lt in SIMPLE_TYPES:
        cpp = SIMPLE_TYPES[lt]
        return cpp, ('string' if cpp == 'QString' else 'simple')
    if lt in VALUE_CLASS_MAP:
        return VALUE_CLASS_MAP[lt], 'value'
    if lt in CLASS_MAP:
        return CLASS_MAP[lt] + '*', 'class'
    if lt in ctx.classes:
        return t + '*', 'class'
    if lt in ctx.interfaces:
        return t + '*', 'class'
    if lt in ctx.records:
        return t, 'record'
    if t.startswith('^'):
        inner, _ = map_type(t[1:], ctx)
        return inner.rstrip('*') + '*', 'simple'
    return t, 'simple'


def parse_type_tokens(toks, i, stop):
    """Collect tokens of a type expression until a token in stop (at depth 0)."""
    depth = 0
    out = []
    while i < len(toks):
        k, v = toks[i]
        if depth == 0 and (v in stop or (k == 'id' and v.lower() in stop)):
            break
        if v in ('(', '['):
            depth += 1
        elif v in (')', ']'):
            depth -= 1
        out.append((k, v))
        i += 1
    return out, i


def type_tokens_to_cpp(ttoks, ctx, unit_consts=None):
    """Translate a type expression; returns (cpp, kind)."""
    if not ttoks:
        return 'void*', 'simple'
    words = [v for k, v in ttoks]
    low = [w.lower() for w in words]
    if low[0] == 'array':
        # array [a..b] of T  |  array of T
        if len(low) > 1 and low[1] == '[':
            j = words.index(']')
            rng = ttoks[2:j]
            elem, _ = type_tokens_to_cpp(ttoks[j + 2:], ctx)
            # a..b
            parts = []
            cur = []
            for t in rng:
                if t[1] == '..':
                    parts.append(cur)
                    cur = []
                elif t[1] == ',':
                    parts.append(cur)
                    cur = []
                else:
                    cur.append(t)
            parts.append(cur)
            if len(parts) >= 2 and len(parts[0]) == 1 and len(parts[1]) == 1 and parts[0][0][0] == 'str':
                lo_c = pas_string_to_cpp(parts[0][0][1]).strip('"')
                hi_c = pas_string_to_cpp(parts[1][0][1]).strip('"')
                return f"{elem}[{ord(hi_c) - ord(lo_c) + 1}]", 'array'
            if len(parts) >= 2:
                lo = expr_to_cpp(parts[0])
                hi = expr_to_cpp(parts[1])
                size = f'({hi}) - ({lo}) + 1' if lo != '0' else f'({hi}) + 1'
                return f'{elem}[{size}]', 'array'
            return f'QList<{elem}>', 'value'
        elem, _ = type_tokens_to_cpp(ttoks[2:], ctx)
        return f'QList<{elem}>', 'value'
    if low[0] in ('procedure', 'function'):
        j = 1
        params = []
        if j < len(ttoks) and ttoks[j][1] == '(':
            depth = 0
            k = j
            while k < len(ttoks):
                if ttoks[k][1] == '(':
                    depth += 1
                elif ttoks[k][1] == ')':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            params, _, _ = parse_params(ttoks[j:k + 1] + [('op', ';')], 0, ctx)
            j = k + 1
        ret = 'void'
        if j < len(ttoks) and ttoks[j][1] == ':':
            rt = [t for t in ttoks[j + 1:] if t[1].lower() not in DIRECTIVES and t[1].lower() not in ('of', 'object')]
            ret, _ = type_tokens_to_cpp(rt[:1], ctx)
        ptypes = ', '.join(p.split('=')[0].rsplit(' ', 1)[0] for p in params)
        return f'std::function<{ret}({ptypes})>', 'value'
    if low[0] == 'set' and len(low) > 2:
        return 'QSet<int>', 'value'
    if low[0] in ('string', 'shortstring') and len(low) > 1:
        return 'QString', 'string'
    if low[0] == '^':
        inner, _ = type_tokens_to_cpp(ttoks[1:], ctx)
        return inner.rstrip('*') + '*', 'simple'
    if len(ttoks) == 1:
        return map_type(words[0], ctx)
    # qualified name Unit.Type
    if len(ttoks) == 3 and words[1] == '.':
        return map_type(words[2], ctx)
    return ' '.join(words), 'simple'


def param_decl(names, ttoks, mode, default_toks, ctx):
    """Build C++ parameter declarations."""
    res = []
    if not ttoks:
        cpp = 'void*'
        kind = 'simple'
    else:
        cpp, kind = type_tokens_to_cpp(ttoks, ctx)
        if ttoks and ttoks[0][1].lower() == 'array' and len(ttoks) > 1 and ttoks[1][1].lower() == 'of':
            # open array parameter
            elem, _ = type_tokens_to_cpp(ttoks[2:], ctx)
            if elem == 'const' or not ttoks[2:]:
                elem = 'QVariant'
            cpp, kind = f'QList<{elem}>', 'value'
    for n in names:
        n = fix_ident(n)
        if mode in ('var', 'out'):
            d = f'{cpp}& {n}' if not cpp.endswith('*') else f'{cpp}& {n}'
        elif kind in ('string', 'record', 'value'):
            d = f'const {cpp}& {n}'
        else:
            d = f'{cpp} {n}'
        if default_toks:
            dv = expr_to_cpp(default_toks)
            if dv == "QStringLiteral(\"\")":
                dv = 'QString()'
            if cpp == 'QChar' and dv.startswith('QStringLiteral('):
                dv = "QChar(" + dv[len('QStringLiteral('):-1].replace('"', "'") + ")"
                if dv == "QChar('\\'')":
                    dv = "QChar('\\'')"

            d += ' = ' + dv
        res.append(d)
    return res


def parse_params(toks, i, ctx):
    """toks[i] == '('; returns (list of cpp params, list of (name, mode, type), next index)."""
    assert toks[i][1] == '('
    i += 1
    params = []
    raw = []
    while toks[i][1] != ')':
        mode = 'value'
        k, v = toks[i]
        if v.lower() in ('const', 'var', 'out', 'constref'):
            mode = v.lower()
            i += 1
        names = []
        while True:
            names.append(toks[i][1])
            i += 1
            if toks[i][1] == ',':
                i += 1
                continue
            break
        ttoks = []
        default = []
        if toks[i][1] == ':':
            i += 1
            ttoks, i = parse_type_tokens(toks, i, {';', ')', '='})
        if toks[i][1] == '=':
            i += 1
            default, i = parse_type_tokens(toks, i, {';', ')'})
        params.extend(param_decl(names, ttoks, mode, default, ctx))
        for n in names:
            raw.append((n, mode, ' '.join(v for k, v in ttoks)))
        if toks[i][1] == ';':
            i += 1
    return params, raw, i + 1


# ---------------------------------------------------------------------------
# Declaration parsing
# ---------------------------------------------------------------------------

DIRECTIVES = {'virtual', 'override', 'abstract', 'overload', 'reintroduce', 'stdcall', 'cdecl', 'register',
              'inline', 'dynamic', 'static', 'safecall', 'pascal', 'deprecated', 'platform', 'message', 'far',
              'export', 'assembler', 'final', 'external', 'forward', 'varargs'}


def parse_routine(toks, i, ctx, in_class=False, class_name=None):
    """toks[i] is procedure/function/constructor/destructor (optionally preceded by 'class').
    Returns (dict, next index)."""
    is_static = False
    if toks[i][1].lower() == 'class':
        is_static = True
        i += 1
    kind = toks[i][1].lower()
    i += 1
    name = toks[i][1]
    i += 1
    # method resolution "Interface.Method = Impl" not supported
    params, raw = [], []
    if toks[i][1] == '(':
        params, raw, i = parse_params(toks, i, ctx)
    ret = None
    if toks[i][1] == ':':
        i += 1
        rtoks, i = parse_type_tokens(toks, i, {';'})
        ret, _ = type_tokens_to_cpp(rtoks, ctx)
    if toks[i][1] == ';':
        i += 1
    dirs = set()
    while i < len(toks) and toks[i][0] == 'id' and toks[i][1].lower() in DIRECTIVES:
        d = toks[i][1].lower()
        dirs.add(d)
        i += 1
        if d == 'message':
            while toks[i][1] != ';':
                i += 1
        if d == 'external':
            while toks[i][1] != ';':
                i += 1
        if i < len(toks) and toks[i][1] == ';':
            i += 1
    return {'kind': kind, 'name': name, 'params': params, 'raw': raw, 'ret': ret, 'dirs': dirs,
            'static': is_static}, i


def routine_to_cpp(r, class_name=None, iface=False):
    name = fix_ident(r['name'])
    params = ', '.join(r['params'])
    if r['kind'] == 'constructor':
        if class_name and r['name'].lower() == 'create':
            return f'{class_name}({params});'
        return f'static {class_name}* {name}({params});  // Delphi named constructor'
    if r['kind'] == 'destructor':
        if not class_name:
            return f'// destructor {name}'
        return f'~{class_name}() override;' if CLASS_HAS_BASE.get(class_name) else f'virtual ~{class_name}();'
    ret = r['ret'] or 'void'
    prefix = ''
    suffix = ''
    if r['static']:
        prefix = 'static '
    if 'virtual' in r['dirs'] or 'dynamic' in r['dirs']:
        prefix = 'virtual '
    if 'override' in r['dirs']:
        suffix = ' override'
    if 'abstract' in r['dirs'] or iface:
        prefix = 'virtual '
        suffix = ' = 0'
    return f'{prefix}{ret} {name}({params}){suffix};'


def skip_to_semicolon(toks, i):
    depth = 0
    while i < len(toks):
        v = toks[i][1]
        if v in ('(', '['):
            depth += 1
        elif v in (')', ']'):
            depth -= 1
        elif v == ';' and depth == 0:
            return i + 1
        i += 1
    return i


SECTION_WORDS = {'type', 'const', 'var', 'threadvar', 'resourcestring', 'procedure', 'function', 'uses',
                 'constructor', 'destructor'}
VISIBILITY = {'private', 'protected', 'public', 'published', 'strict', 'automated'}


class Unit:
    def __init__(self, name, kind):
        self.name = name
        self.kind = kind   # core | form | frame | mainform
        self.uses = []
        self.out = []      # C++ lines
        self.forward = set()
        self.form_class = None
        self.class_names = []
        self.exported_names = []


def parse_class(toks, i, cname, ctx, unit, is_record=False, is_interface=False):
    """Parse a class/record/interface body starting after 'class(...)'/'record'.
    Returns (lines, next index)."""
    lines = []
    vis = 'published' if not is_record else 'public'
    fields_vis = {}
    members = {'private': [], 'protected': [], 'public': [], 'published': []}
    props = []
    slots = []
    while i < len(toks):
        k, v = toks[i]
        lv = v.lower()
        if lv == 'end':
            i += 1
            if i < len(toks) and toks[i][1] == ';':
                i += 1
            break
        if lv in VISIBILITY:
            if lv == 'strict':
                i += 1
                lv = toks[i][1].lower()
            vis = lv
            i += 1
            continue
        if lv == 'case' and is_record:
            # variant part: case X : T of 0:(...); 1:(...)
            i = skip_to_end_of_variant(toks, i)
            members[vis].append('// TODO: variant record part not translated')
            break
        if lv in ('procedure', 'function', 'constructor', 'destructor') or (lv == 'class' and toks[i + 1][1].lower() in ('procedure', 'function')):
            r, i = parse_routine(toks, i, ctx, True, cname)
            line = routine_to_cpp(r, cname, iface=is_interface)
            is_handler = r['raw'] and r['raw'][0][0].lower() == 'sender' and 'tobject' in r['raw'][0][2].lower()
            if unit.kind in ('form', 'frame', 'mainform') and cname == unit.form_class and vis == 'published':
                slots.append((line, r))
            else:
                members['public' if is_interface else vis].append(line)
            continue
        if lv == 'property':
            p, i = parse_property(toks, i, ctx)
            props.append((vis, p))
            continue
        if lv in ('const', 'type', 'var', 'class') and not is_record:
            # class var / nested consts: skip keyword
            i += 1
            continue
        # field: a, b : T;
        names = []
        while toks[i][1] not in (':',):
            if toks[i][1] != ',':
                names.append(toks[i][1])
            i += 1
        i += 1
        ttoks, i = parse_type_tokens(toks, i, {';', 'end'})
        if i < len(toks) and toks[i][1] == ';':
            i += 1
        if unit.kind in ('form', 'frame', 'mainform') and cname == unit.form_class and vis == 'published':
            continue  # DFM component -> ui.<Name>
        cpp, kind = type_tokens_to_cpp(ttoks, ctx)
        for n in names:
            if '[' in cpp and cpp.endswith(']'):
                base = cpp[:cpp.index('[')]
                dims = cpp[cpp.index('['):]
                members[vis].append(f'{base} {fix_ident(n)}{dims};')
            else:
                init = ''
                if cpp in ('int', 'bool', 'double', 'float', 'quint32', 'qint64', 'quint8', 'quint16', 'qint16', 'qint8', 'TDateTime', 'THandle', 'TColor') or cpp.endswith('*'):
                    init = {'bool': ' = false'}.get(cpp, ' = nullptr' if cpp.endswith('*') else ' = 0')
                members[vis].append(f'{cpp} {fix_ident(n)}{init};')
    return members, props, slots, i


def skip_to_end_of_variant(toks, i):
    depth = 0
    while i < len(toks):
        v = toks[i][1].lower()
        if v == '(':
            depth += 1
        elif v == ')':
            depth -= 1
        elif v == 'end' and depth == 0:
            return i
        i += 1
    return i


def parse_property(toks, i, ctx):
    assert toks[i][1].lower() == 'property'
    i += 1
    name = toks[i][1]
    i += 1
    idx_params = []
    if toks[i][1] == '[':
        # array property parameters
        j = i + 1
        depth = 1
        inner = []
        while depth:
            if toks[j][1] == '[':
                depth += 1
            elif toks[j][1] == ']':
                depth -= 1
                if depth == 0:
                    break
            inner.append(toks[j])
            j += 1
        fake = [('op', '(')] + inner + [('op', ')')]
        idx_params, _, _ = parse_params(fake, 0, ctx)
        i = j + 1
    ptype = None
    if toks[i][1] == ':':
        i += 1
        ttoks, i = parse_type_tokens(toks, i, {';', 'read', 'write', 'index', 'default', 'stored', 'nodefault', 'implements'})
        ptype = type_tokens_to_cpp(ttoks, ctx)
    p = {'name': name, 'type': ptype, 'index': None, 'read': None, 'write': None, 'params': idx_params, 'default': False}
    while i < len(toks) and toks[i][1] != ';':
        lv = toks[i][1].lower()
        if lv == 'index':
            et, i = parse_type_tokens(toks, i + 1, {'read', 'write', ';'})
            p['index'] = expr_to_cpp(et)
            continue
        if lv in ('read', 'write'):
            # name possibly dotted (FRec.Field)
            j = i + 1
            nm = toks[j][1]
            j += 1
            while toks[j][1] == '.':
                nm += '.' + toks[j + 1][1]
                j += 2
            p[lv] = nm
            i = j
            continue
        if lv in ('default', 'stored', 'nodefault', 'implements'):
            et, i = parse_type_tokens(toks, i + 1, {'read', 'write', ';', 'default', 'stored'})
            continue
        i += 1
    i += 1
    if i < len(toks) and toks[i][1].lower() == 'default':
        p['default'] = True
        i += 1
        if toks[i][1] == ';':
            i += 1
    return p, i


def is_field_name(n):
    if '.' in n:
        return True
    return len(n) > 1 and n[0] in 'fF' and (n[1].isupper() or n[1] == '_')


def skip_directives(toks, i):
    """Skip 'stdcall;' 'of object' etc. after a procedural type."""
    while i < len(toks) and toks[i][0] == 'id' and toks[i][1].lower() in DIRECTIVES | {'of', 'object'}:
        i += 1
        if i < len(toks) and toks[i][1] == ';' and i + 1 < len(toks) and toks[i + 1][0] == 'id' and toks[i + 1][1].lower() in DIRECTIVES:
            i += 1
    return i


METHOD_NAMES = {}  # lower case method name -> canonical spelling (all classes)


def canon(n):
    if not n or '.' in n:
        return n
    return METHOD_NAMES.get(n.lower(), n)


def property_to_cpp(p):
    lines = []
    p = dict(p)
    if p['read'] and not is_field_name(p['read']):
        p['read'] = canon(p['read'])
    if p['write'] and not is_field_name(p['write']):
        p['write'] = canon(p['write'])
    name = fix_ident(p['name'])
    cpp, kind = p['type'] if p['type'] else ('QVariant', 'simple')
    arg_cpp = 'const ' + cpp + '&' if kind in ('string', 'record', 'value') else cpp
    params = p['params']
    pnames = [x.split()[-1].split('=')[0] for x in params]
    pdecl = ', '.join(params)
    pcall = ', '.join(pnames)
    rd = p['read']
    wr = p['write']
    if rd:
        if p['index'] is not None:
            call = f'{rd}({p["index"]}{", " + pcall if pcall else ""})'
            lines.append(f'{cpp} {name}({pdecl}) {{ return {call}; }}')
        elif is_field_name(rd):
            if params:
                lines.append(f'{cpp} {name}({pdecl}) {{ return {rd}[{pcall}]; }}')
            else:
                lines.append(f'{cpp} {name}() const {{ return {rd}; }}')
        else:
            lines.append(f'{cpp} {name}({pdecl}) {{ return {rd}({pcall}); }}')
    if wr:
        setter = 'Set' + p['name']
        vdecl = f'{arg_cpp} Value'
        allp = (pdecl + ', ' if pdecl else '') + vdecl
        if p['index'] is not None:
            lines.append(f'void {setter}({allp}) {{ {wr}({p["index"]}{", " + pcall if pcall else ""}, Value); }}')
        elif is_field_name(wr):
            if params:
                lines.append(f'void {setter}({allp}) {{ {wr}[{pcall}] = Value; }}')
            else:
                lines.append(f'void {setter}({allp}) {{ {wr} = Value; }}')
        elif wr == setter:
            pass  # the Delphi write method already has the right name
        else:
            lines.append(f'void {setter}({allp}) {{ {wr}({(pcall + ", ") if pcall else ""}Value); }}')
    return lines


def parse_unit(path, unit, ctx, dfm_events):
    text = open(path, encoding='cp1252', errors='replace').read()
    sec = interface_section(text)
    toks = tokenize(sec)
    out = unit.out
    i = 0
    mode = None
    while i < len(toks):
        k, v = toks[i]
        lv = v.lower()
        if lv == 'uses':
            j = i + 1
            while toks[j][1] != ';':
                if toks[j][0] == 'id' and toks[j][1].lower() != 'in':
                    unit.uses.append(toks[j][1])
                j += 1
            i = j + 1
            continue
        if lv in ('type', 'const', 'var', 'threadvar', 'resourcestring'):
            mode = lv
            i += 1
            continue
        if lv in ('procedure', 'function'):
            r, i = parse_routine(toks, i, ctx)
            out.append(routine_to_cpp(r))
            unit.exported_names.append(r['name'])
            mode = None
            continue
        if mode == 'type':
            i = parse_type_decl(toks, i, ctx, unit, dfm_events)
            continue
        if mode in ('const', 'resourcestring'):
            i = parse_const_decl(toks, i, ctx, unit)
            continue
        if mode in ('var', 'threadvar'):
            i = parse_var_decl(toks, i, ctx, unit)
            continue
        i += 1


def parse_const_decl(toks, i, ctx, unit):
    name = toks[i][1]
    i += 1
    out = unit.out
    unit.exported_names.append(name)
    if toks[i][1] == ':':
        ttoks, i = parse_type_tokens(toks, i + 1, {'='})
        i += 1
        cpp, kind = type_tokens_to_cpp(ttoks, ctx)
        etoks, i = parse_type_tokens(toks, i, {';'})
        i += 1
        if '[' in cpp:
            base = cpp[:cpp.index('[')]
            # array constant
            vals = split_top(etoks[1:-1]) if etoks and etoks[0][1] == '(' else [etoks]
            items = ', '.join(expr_to_cpp(v) for v in vals)
            if base == 'QString':
                out.append(f'inline const QStringList {fix_ident(name)} = {{{items}}};')
            else:
                out.append(f'inline const QList<{base}> {fix_ident(name)} = {{{items}}};')
        elif cpp == 'QString':
            out.append(f'inline const QString {fix_ident(name)} = {expr_to_cpp(etoks)};')
        else:
            out.append(f'inline {cpp} {fix_ident(name)} = {expr_to_cpp(etoks)};  // typed constant')
        return i
    i += 1  # '='
    etoks, i = parse_type_tokens(toks, i, {';'})
    i += 1
    if expr_is_string(etoks):
        out.append(f'inline const QString {fix_ident(name)} = {expr_to_cpp(etoks)};')
    elif etoks and etoks[0][1] == '[':
        out.append(f'inline const QSet<int> {fix_ident(name)} = {{{expr_to_cpp(etoks[1:-1])}}};')
    elif any(t[0] == 'num' and '.' in t[1] for t in etoks):
        out.append(f'constexpr double {fix_ident(name)} = {expr_to_cpp(etoks)};')
    else:
        out.append(f'constexpr int {fix_ident(name)} = {expr_to_cpp(etoks)};')
    return i


def split_top(toks):
    parts = []
    cur = []
    depth = 0
    for t in toks:
        if t[1] in ('(', '['):
            depth += 1
        elif t[1] in (')', ']'):
            depth -= 1
        if t[1] == ',' and depth == 0:
            parts.append(cur)
            cur = []
        else:
            cur.append(t)
    if cur:
        parts.append(cur)
    return parts


def parse_var_decl(toks, i, ctx, unit):
    names = []
    while toks[i][1] != ':':
        if toks[i][1] != ',':
            names.append(toks[i][1])
        i += 1
    i += 1
    ttoks, i = parse_type_tokens(toks, i, {';', '='})
    init = None
    if i < len(toks) and toks[i][1] == '=':
        init, i = parse_type_tokens(toks, i + 1, {';'})
    i += 1
    while i < len(toks) and toks[i][0] == 'id' and toks[i][1].lower() in DIRECTIVES:
        i += 1
        if i < len(toks) and toks[i][1] == ';':
            i += 1
    cpp, kind = type_tokens_to_cpp(ttoks, ctx)
    for n in names:
        unit.exported_names.append(n)
        cmt = f'  // initial value: {expr_to_cpp(init)}' if init else ''
        if '[' in cpp:
            base = cpp[:cpp.index('[')]
            unit.out.append(f'extern {base} {fix_ident(n)}{cpp[cpp.index("["):]};{cmt}')
        else:
            unit.out.append(f'extern {cpp} {fix_ident(n)};{cmt}')
    return i


def parse_type_decl(toks, i, ctx, unit, dfm_events):
    name = toks[i][1]
    i += 1
    if toks[i][1] != '=':
        return skip_to_semicolon(toks, i)
    i += 1
    out = unit.out
    k, v = toks[i]
    lv = v.lower()
    if lv == 'type':
        i += 1
        k, v = toks[i]
        lv = v.lower()
    unit.exported_names.append(name)
    if v == '(':
        # enum
        j = i + 1
        vals = []
        while toks[j][1] != ')':
            if toks[j][1] == ',':
                j += 1
                continue
            nm = toks[j][1]
            j += 1
            if toks[j][1] == '=':
                et, j = parse_type_tokens(toks, j + 1, {',', ')'})
                vals.append(f'{nm} = {expr_to_cpp(et)}')
            else:
                vals.append(nm)
        out.append(f'enum {name} {{ {", ".join(vals)} }};')
        ctx.enums.add(name.lower())
        return skip_to_semicolon(toks, j + 1)
    if lv in ('record', 'packed') and (lv == 'record' or toks[i + 1][1].lower() == 'record'):
        if lv == 'packed':
            i += 1
        i += 1
        members, props, slots, i = parse_class(toks, i, name, ctx, unit, is_record=True)
        out.append(f'struct {name} {{')
        for vis in ('public', 'private', 'protected', 'published'):
            for m in members[vis]:
                out.append('    ' + m)
        out.append('};')
        return i
    if lv == 'class' and toks[i + 1][1].lower() == 'of':
        target = toks[i + 2][1]
        out.append(f'using {name} = std::function<QWidget*(QWidget*)>;  // Delphi metaclass: class of {target}')
        return skip_to_semicolon(toks, i)
    if lv == 'interface':
        i += 1
        if toks[i][1] == '(':
            while toks[i][1] != ')':
                i += 1
            i += 1
        if toks[i][1] == '[':
            while toks[i][1] != ']':
                i += 1
            i += 1
        members, props, slots, i = parse_class(toks, i, name, ctx, unit, is_interface=True)
        out.append(f'class {name}')
        out.append('{')
        out.append('public:')
        out.append(f'    virtual ~{name}() = default;')
        for m in members['public']:
            out.append('    ' + m)
        out.append('};')
        return i
    if lv == 'class':
        i += 1
        if toks[i][1] == ';':
            # forward declaration
            unit.forward.add(name)
            return i + 1
        bases = []
        if toks[i][1] == '(':
            i += 1
            while toks[i][1] != ')':
                if toks[i][1] != ',':
                    bases.append(toks[i][1])
                i += 1
            i += 1
        if toks[i][1] == ';':
            # class(TBase); short form
            bl0 = map_base(bases, unit, name)[0]
            out.append(f'class {name} : public {bl0[0] if bl0 else "QObject"} {{ public: using {bl0[0] if bl0 else "QObject"}::{bl0[0] if bl0 else "QObject"}; }};')
            return i + 1
        is_form = bool(unit.form_class) and name == unit.form_class
        base_list, ui_name = map_base(bases, unit, name)
        CLASS_HAS_BASE[name] = bool(base_list)
        members, props, slots, i = parse_class(toks, i, name, ctx, unit)
        unit.class_names.append(name)
        bl = ', '.join('public ' + b for b in base_list)
        out.append(f'class {name}' + (f' : {bl}' if bl else ''))
        out.append('{')
        if is_form:
            out.append('    Q_OBJECT')
        out.append('public:')
        if is_form:
            out.append(f'    Ui::{ui_name} ui;')
            has_ctor = any(m.startswith(f'{name}(') for m in members['public'])
            if not has_ctor:
                ctor = f'explicit {name}(QWidget *AOwner = nullptr);'
                out.append('    ' + ctor)
            if not any(f'~{name}(' in m for vis in members for m in members[vis]):
                out.append(f'    ~{name}() override;')
        elif not bases or bases[0].lower() in ('tobject',):
            pass
        for vis in ('public', 'published'):
            for m in members[vis]:
                out.append('    ' + m)
        pub_props = [p for v, p in props if v in ('public', 'published')]
        if pub_props:
            out.append('    // properties')
            for p in pub_props:
                for l in property_to_cpp(p):
                    out.append('    ' + l)
        if not is_form and not any(f'~{name}(' in m for vis in members for m in members[vis]):
            if not base_list or base_list[0] in ('QObject',):
                out.append(f'    virtual ~{name}() = default;')
        if slots:
            out.append('    // VCL event handlers (connected in the constructor)')
            if is_form and dfm_events:
                out.append('    // DFM wiring:')
                for ev in dfm_events:
                    out.append(f'    //   {ev}')
            out.append('public slots:')
            for line, r in slots:
                out.append('    ' + line)
        for vis in ('protected',):
            if members[vis] or any(v == vis for v, p in props):
                out.append('protected:')
                for m in members[vis]:
                    out.append('    ' + m)
                for v, p in props:
                    if v == vis:
                        for l in property_to_cpp(p):
                            out.append('    ' + l)
        if members['private'] or any(v == 'private' for v, p in props):
            out.append('private:')
            for m in members['private']:
                out.append('    ' + m)
            for v, p in props:
                if v == 'private':
                    for l in property_to_cpp(p):
                        out.append('    ' + l)
        out.append('};')
        return i
    if lv in ('procedure', 'function'):
        # procedural type
        is_func = lv == 'function'
        i += 1
        params, raw = [], []
        if toks[i][1] == '(':
            params, raw, i = parse_params(toks, i, ctx)
        ret = 'void'
        if toks[i][1] == ':':
            rtoks, i = parse_type_tokens(toks, i + 1, {';', 'of'})
            ret, _ = type_tokens_to_cpp(rtoks, ctx)
        ptypes = ', '.join(p.split('=')[0].rsplit(' ', 1)[0] for p in params)
        out.append(f'using {name} = std::function<{ret}({ptypes})>;')
        i = skip_to_semicolon(toks, i)
        while i < len(toks) and toks[i][0] == 'id' and toks[i][1].lower() in DIRECTIVES:
            i += 1
            if i < len(toks) and toks[i][1] == ';':
                i += 1
        return i
    if lv == 'set':
        out.append(f'using {name} = QSet<int>;  // set of {toks[i + 2][1]}')
        return skip_to_semicolon(toks, i)
    if lv == 'array':
        ttoks, j = parse_type_tokens(toks, i, {';'})
        cpp, kind = type_tokens_to_cpp(ttoks, ctx)
        if '[' in cpp:
            base = cpp[:cpp.index('[')]
            out.append(f'using {name} = {base}[{cpp[cpp.index("[") + 1:-1]}];')
        else:
            out.append(f'using {name} = {cpp};')
        return j + 1
    # alias
    ttoks, j = parse_type_tokens(toks, i, {';'})
    cpp, kind = type_tokens_to_cpp(ttoks, ctx)
    if kind == 'class':
        out.append(f'using {name} = {cpp[:-1]};')
    else:
        out.append(f'using {name} = {cpp};')
    return j + 1


def map_base(bases, unit, name):
    """Returns (list of C++ base classes, ui class name)."""
    ui_name = name[1:] if name.startswith('T') else name
    if not bases:
        return [], ui_name
    first = bases[0].lower()
    rest = bases[1:]
    res = []
    if first == 'tform':
        res.append('QMainWindow' if unit.kind == 'mainform' else 'TVclForm')
    elif first == 'tframe':
        res.append('QWidget')
    elif first in ('tobject', 'tpersistent'):
        pass
    elif first == 'tcomponent':
        res.append('QObject')
    elif first == 'tthread':
        res.append('QThread')
    elif first in CLASS_MAP:
        res.append(CLASS_MAP[first])
    else:
        res.append(bases[0])
    for b in rest:
        if b.lower() in ('idroptarget',):
            continue
        res.append(b)
    return res, ui_name


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def dfm_event_list(dfm_path):
    if not os.path.exists(dfm_path):
        return []
    from dfmparser import parse_dfm
    root = parse_dfm(dfm_path)
    evs = []
    for o in root.walk():
        for k, v in o.props.items():
            if k.startswith('On') and isinstance(v, str):
                evs.append(f'{o.name or o.cls}.{k} = {v}')
    return evs


def unit_kind(src, uname):
    pas = os.path.join(src, uname + '.pas')
    text = open(pas, encoding='cp1252', errors='replace').read()
    sec = strip_comments(text)
    sec = sec[:re.search(r'\bimplementation\b', sec, re.I).start()] if re.search(r'\bimplementation\b', sec, re.I) else sec
    if not os.path.exists(os.path.join(src, uname + '.dfm')):
        return 'core', None
    m = re.search(r'\b(T\w+)\s*=\s*class\s*\(\s*(TForm|TFrame)\b', sec, re.I)
    if not m:
        return 'core', None
    if m.group(1) == 'TDFendReloadedMainForm':
        return 'mainform', m.group(1)
    return ('frame' if m.group(2).lower() == 'tframe' else 'form'), m.group(1)


def collect_ctx(src, units):
    ctx = Ctx()
    for u in units:
        text = strip_comments(open(os.path.join(src, u + '.pas'), encoding='cp1252', errors='replace').read())
        isec = interface_section(text)
        for m in re.finditer(r'\b(?:procedure|function)\s+([A-Za-z_]\w*)\s*[(:;]', isec, re.I):
            METHOD_NAMES.setdefault(m.group(1).lower(), m.group(1))
        for m in re.finditer(r'\b(T\w+|I\w+)\s*=\s*(class|record|packed\s+record|interface)\b(?!\s+of)', text, re.I):
            n, kind = m.group(1), m.group(2).lower()
            if kind == 'class':
                ctx.classes.add(n.lower())
            elif kind == 'interface':
                ctx.interfaces.add(n.lower())
            else:
                ctx.records.add(n.lower())
    return ctx


HEADER_INCLUDES = '''#include "DelphiCompat.h"
#include "Graphics.h"
#include "ComCtrls.h"
#include <QtWidgets>
#include <QtXml>
#include <QtNetwork>
#include <functional>
'''


def main():
    src, out_core, out_forms = sys.argv[1:4]
    only = set(sys.argv[4:])
    all_units = sorted(f[:-4] for f in os.listdir(src) if f.endswith('.pas'))
    units = [u for u in all_units if u not in SKIP_UNITS]
    ctx = collect_ctx(src, units)
    names = {}
    project = {u.lower(): u for u in units}
    for u in units:
        if only and u not in only:
            continue
        kind, form_class = unit_kind(src, u)
        unit = Unit(u, kind)
        unit.form_class = form_class
        events = dfm_event_list(os.path.join(src, u + '.dfm')) if kind != 'core' else []
        try:
            parse_unit(os.path.join(src, u + '.pas'), unit, ctx, events)
        except Exception as e:
            sys.stderr.write(f'[error] {u}: {e!r}\n')
            continue
        outdir = out_core if kind == 'core' else out_forms
        os.makedirs(outdir, exist_ok=True)
        lines = [f'// C++ port of {u}.pas (interface section).',
                 '// Generated by tools/pas2h.py, then maintained by hand.',
                 '#pragma once', '']
        lines.append(HEADER_INCLUDES.rstrip())
        if kind in ('form', 'frame', 'mainform'):
            lines.append('#include "VclForm.h"')
            lines.append('#include "VclLayout.h"')
            lines.append('#include "VclWidgets.h"')
            lines.append(f'#include "ui_{u}.h"')
        for us in unit.uses:
            if us.lower() in project and project[us.lower()] != u:
                lines.append(f'#include "{project[us.lower()]}.h"')
        lines.append('')
        fwd = sorted(set(c for c in unit.forward))
        for f in fwd:
            lines.append(f'class {f};')
        if fwd:
            lines.append('')
        lines.extend(unit.out)
        lines.append('')
        with open(os.path.join(outdir, u + '.h'), 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        for n in unit.exported_names:
            names.setdefault(n.lower(), []).append(u)
    dups = {n: us for n, us in names.items() if len(us) > 1}
    if dups:
        sys.stderr.write('Duplicate global names across units:\n')
        for n, us in sorted(dups.items()):
            sys.stderr.write(f'  {n}: {", ".join(us)}\n')


if __name__ == '__main__':
    main()
