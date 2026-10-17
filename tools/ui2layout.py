#!/usr/bin/env python3
"""Convert the absolute positions of a .ui file created by dfm2ui.py into Qt
layouts.

  ui2layout.py FILE.ui [--only NAME,...] [--skip NAME,...] [--free NAME,...] [--grow-big] [--dry-run]

--free: these controls stay outside of the layouts (overlays that the form
code positions itself).

--grow-big: lists, tables, memos and tab controls fill the remaining space
(as if anchored to all sides), e.g. for dialogs that had a fixed size.

Every container (the form itself, group boxes, frames, tab pages) whose
children are positioned by pixels (no vclAlign) gets a QGridLayout. The rows
and columns are taken from the design positions: controls starting at the
same x are in one column, controls starting at the same y in one row; larger
vertical gaps become spacers. Controls anchored to the right side (akRight)
span to the right edge and grow with the window, controls anchored to the
bottom get the remaining height. Input controls keep their design width as
minimum width, so a page looks like before at 100 % but grows with larger
fonts and scaling instead of cutting off text.

Containers that cannot be converted automatically (aligned children,
controls overlapping each other) are left unchanged and reported; they are
converted by hand. The widget names do not change, so the form code works
without changes (except code that moves or resizes controls itself).
"""
import re
import sys
import xml.etree.ElementTree as ET

TOL = 6           # px: edges closer than this are the same row / column
WIDE = 300        # px: wider edits / combo boxes stretch with the page
GAP = 14          # px: vertical gaps larger than this get a spacer
CONTAINERS = {"QWidget", "QGroupBox", "QFrame"}
NO_LAYOUT_CHILD = {"QMenu", "QAction", "QToolBar"}
INPUTS = {"QComboBox", "QLineEdit", "QSpinBox", "QDoubleSpinBox", "QPushButton", "QToolButton",
          "TColorBoxWidget", "QSlider", "QDateEdit"}
GROW_BIG = False
FREE = set()
TEXTS = {"QLabel", "QCheckBox", "QRadioButton"}
BIG = {"QListWidget", "QTextEdit", "QPlainTextEdit", "QTableWidget", "QTreeWidget", "TListView",
       "TValueListEditorWidget", "QTabWidget", "QScrollArea", "TImageWidget", "QListView"}


def prop(w, name):
    for p in w.findall("property"):
        if p.get("name") == name:
            return p
    return None


def geom(w):
    p = prop(w, "geometry")
    if p is None:
        return None
    r = p.find("rect")
    return tuple(int(r.find(k).text) for k in ("x", "y", "width", "height"))


def anchors(w):
    p = prop(w, "vclAnchors")
    if p is None:
        return {"akLeft", "akTop"}
    return {s.strip() for s in p.find("string").text.split(",") if s.strip()}


def remove_prop(w, name):
    p = prop(w, name)
    if p is not None:
        w.remove(p)


def set_prop(w, name, xml):
    remove_prop(w, name)
    e = ET.fromstring('<property name="%s">%s</property>' % (name, xml))
    # properties first (before child widgets / layouts)
    idx = 0
    for i, c in enumerate(list(w)):
        if c.tag == "property":
            idx = i + 1
    w.insert(idx, e)


def cluster(values):
    """values: sorted unique ints -> list of cluster starts"""
    starts = []
    for v in sorted(set(values)):
        if not starts or v - starts[-1][-1] > TOL:
            starts.append([v])
        else:
            starts[-1].append(v)
    return [s[0] for s in starts]


def index_of(starts, v):
    best = 0
    for i, s in enumerate(starts):
        if v >= s - TOL:
            best = i
    return best


class Fail(Exception):
    pass


def design_size(w):
    g = geom(w)
    dw, dh = prop(w, "vclDesignWidth"), prop(w, "vclDesignHeight")
    if dw is not None and dh is not None:
        return int(dw.find("number").text), int(dh.find("number").text)
    return (g[2], g[3]) if g else (0, 0)


def spacer_xml(name, vertical, height=0, fixed=False):
    return ET.fromstring(
        '<spacer name="%s"><property name="orientation"><enum>Qt::%s</enum></property>%s'
        '<property name="sizeHint" stdset="0"><size><width>0</width><height>%d</height></size></property></spacer>'
        % (name, "Vertical" if vertical else "Horizontal",
           '<property name="sizeType"><enum>QSizePolicy::Fixed</enum></property>' if fixed else "", height))


def margin_props(lay, m):
    for n, v in zip(("leftMargin", "topMargin", "rightMargin", "bottomMargin"), m):
        lay.append(ET.fromstring('<property name="%s"><number>%d</number></property>' % (n, v)))


def row_spans(items):
    rows = cluster([i["y"] for i in items])
    for i in items:
        i["row"] = index_of(rows, i["y"])
        lastr = i["row"]
        for ri in range(i["row"], len(rows)):
            if rows[ri] < i["b"] - TOL:
                lastr = ri
        i["rs"] = lastr - i["row"] + 1
    return rows


def sections(items):
    """Splits the controls at larger vertical gaps into sections that get a
    grid each, so the columns of one section do not depend on the others.
    Returns [(items, gap before the section)]."""
    rows = row_spans(items)
    result, cur, gap_before = [], [], 0
    for ri, top in enumerate(rows):
        if ri > 0 and cur:
            ending = [i for i in cur if i["row"] + i["rs"] - 1 < ri]
            spanning = [i for i in cur if i["row"] + i["rs"] - 1 >= ri]
            gap = top - max((i["b"] for i in ending), default=top)
            if not spanning and gap > GAP:
                result.append((cur, gap_before))
                cur, gap_before = [], gap
        cur += [i for i in items if i["row"] == ri]
    if cur:
        result.append((cur, gap_before))
    return result


def build_grid(items, name, margins, check_only=False):
    cols = cluster([i["x"] for i in items])
    rows = row_spans(items)
    for i in items:
        i["c"] = index_of(cols, i["x"])
        last = i["c"]
        for ci in range(i["c"], len(cols)):
            if cols[ci] < i["r"] - TOL:
                last = ci
        i["last"] = last
    # a control ends before the next control in the same rows (VCL check
    # boxes, radio buttons and labels are often wider than their text)
    for i in items:
        for o in items:
            if o is i or o["c"] <= i["c"]:
                continue
            if o["row"] < i["row"] + i["rs"] and i["row"] < o["row"] + o["rs"]:
                i["last"] = min(i["last"], o["c"] - 1)
    # columns of controls anchored to the right only (e.g. "..." buttons
    # right of an edit) come after the filler column
    def right_only(i):
        return "akRight" in i["an"] and "akLeft" not in i["an"]
    rcols = [ci for ci in range(len(cols)) if all(right_only(i) for i in items if i["c"] == ci)
             and any(i["c"] == ci for i in items)]
    if rcols and rcols != list(range(rcols[0], len(cols))):
        rcols = []
    nleft = len(cols) - len(rcols)
    colmap = {ci: (ci if ci < nleft else ci + 1) for ci in range(len(cols))}
    ncols = nleft  # filler column index
    for i in items:
        i["oc"] = i["c"]
    for i in items:
        first, last = colmap[i["c"]], colmap[i["last"]]
        free_right = not any(o is not i and o["row"] < i["row"] + i["rs"] and i["row"] < o["row"] + o["rs"]
                             and o["oc"] > i["last"] for o in items)
        if i["cls"] in TEXTS and free_right and last < ncols:
            # longer texts (translations, larger fonts) may use the free space
            last = ncols
        elif "akRight" in i["an"] and "akLeft" in i["an"] and last < ncols:
            # grows with the window: up to the filler column if nothing is
            # right of it in the left part
            free = not any(o is not i and o["row"] < i["row"] + i["rs"] and i["row"] < o["row"] + o["rs"]
                           and i["last"] < o["oc"] < nleft for o in items)
            if free:
                last = ncols
        i["c"], i["cs"] = first, last - first + 1
    ntotal = len(cols) + 1
    occ = {}
    for i in items:
        for r in range(i["row"], i["row"] + i["rs"]):
            for cc in range(i["c"], i["c"] + i["cs"]):
                if (r, cc) in occ:
                    o = occ[(r, cc)]
                    # alternatives (one of them hidden in the design) starting
                    # in the same cell share it
                    if (is_hidden(i["w"]) or is_hidden(o["w"])) and (i["row"], i["c"]) == (o["row"], o["c"]):
                        continue
                    raise Fail("%s overlaps %s" % (i["w"].get("name"), o["w"].get("name")))
                occ[(r, cc)] = i
    if check_only:
        return None
    lay = ET.Element("layout", {"class": "QGridLayout", "name": name})
    margin_props(lay, margins)
    lay.append(ET.fromstring('<property name="horizontalSpacing"><number>8</number></property>'))
    lay.append(ET.fromstring('<property name="verticalSpacing"><number>4</number></property>'))
    for i in sorted(items, key=lambda i: (i["row"], i["c"])):
        k = i["w"]
        x, y, w, h = geom(k)
        attrs = {"row": str(i["row"]), "column": str(i["c"])}
        if i["rs"] > 1:
            attrs["rowspan"] = str(i["rs"])
        if i["cs"] > 1:
            attrs["colspan"] = str(i["cs"])
        stretch_h = "akRight" in i["an"] and "akLeft" in i["an"]
        cls = i["cls"]
        align = None
        if not stretch_h:
            if "akRight" in i["an"]:
                align = "Qt::AlignRight"
            elif cls in INPUTS or cls in BIG or cls in CONTAINERS or cls in ("TRadioGroupWidget",):
                align = "Qt::AlignLeft"
        if align and i["rs"] > 1:
            align += "|Qt::AlignTop"
        if cls == "QComboBox" and stretch_h:
            # long entries (e.g. paths) must not widen the page
            set_prop(k, "sizeAdjustPolicy", "<enum>QComboBox::AdjustToMinimumContentsLengthWithIcon</enum>")
            set_prop(k, "minimumContentsLength", "<number>10</number>")
        if cls in INPUTS and not stretch_h:
            set_prop(k, "minimumSize", "<size><width>%d</width><height>0</height></size>" % w)
        elif cls in BIG or cls in CONTAINERS or cls == "TRadioGroupWidget":
            mw = 0 if stretch_h else w
            mh = 0 if ("akBottom" in i["an"]) else h
            if (cls in CONTAINERS and k.find("layout") is not None) or cls == "TRadioGroupWidget":
                mh = 0  # sized by their own layout
            set_prop(k, "minimumSize", "<size><width>%d</width><height>%d</height></size>" % (mw, mh))
        elif cls == "QLabel" and prop(k, "wordWrap") is not None and not stretch_h:
            set_prop(k, "minimumSize", "<size><width>%d</width><height>0</height></size>" % w)
        if align:
            attrs["alignment"] = align
        remove_prop(k, "geometry")
        remove_prop(k, "vclAnchors")
        remove_prop(k, "vclAutoSize")
        item = ET.SubElement(lay, "item", attrs)
        item.append(k)
    # filler column: keeps the controls at the left
    it = ET.SubElement(lay, "item", {"row": "0", "column": str(ncols)})
    it.append(spacer_xml(name + "_HFill", False))
    cs = ["0"] * ntotal
    cs[ncols] = "1"
    lay.set("columnstretch", ",".join(cs))
    grow = [i for i in items if "akBottom" in i["an"] and "akTop" in i["an"]]
    if grow:
        stretch = ["0"] * len(rows)
        for i in grow:
            stretch[i["row"] + i["rs"] - 1] = "1"
        lay.set("rowstretch", ",".join(stretch))
    return lay, bool(grow), len(rows), ntotal


def is_hidden(k):
    v = prop(k, "visible")
    return v is not None and v.find("bool").text == "false"


def shift_hidden_alternatives(items):
    """Controls hidden in the design that share their place with visible
    ones (the form code shows either the one or the other group): the hidden
    group is placed below the visible controls of that place."""
    hid = [i for i in items if is_hidden(i["w"])]
    vis = [i for i in items if not is_hidden(i["w"])]
    if not hid:
        return
    top = min(i["y"] for i in hid)
    over = [v for v in vis if v["b"] > top and v["y"] < max(h["b"] for h in hid)
            and any(v["x"] < h["r"] and h["x"] < v["r"] and v["y"] < h["b"] and h["y"] < v["b"] for h in hid)]
    if not over:
        return
    dy = max(v["b"] for v in over) - top + 4
    for h in hid:
        h["y"] += dy
        h["b"] += dy


def convert_container(c, cls_of, report, top_offset):
    kids = [k for k in c.findall("widget") if k.get("class") not in NO_LAYOUT_CHILD and k.get("name") not in FREE]
    if c.find("layout") is not None or not kids:
        return False
    for k in kids:
        if prop(k, "vclAlign") is not None:
            raise Fail("aligned child %s" % k.get("name"))
        if geom(k) is None:
            raise Fail("no geometry: %s" % k.get("name"))
    W, H = design_size(c)
    items = []
    # --grow-big only where the design has no anchors of its own
    grow_big = GROW_BIG and all(anchors(k) == {"akLeft", "akTop"} for k in kids)
    for k in kids:
        x, y, w, h = geom(k)
        an = anchors(k)
        if grow_big and k.get("class") in BIG:
            an = {"akLeft", "akTop", "akRight", "akBottom"}
        if k.get("class") in ("QLineEdit", "QComboBox") and w > WIDE:
            # wide inputs grow with the page instead of a large minimum width
            an = an | {"akLeft", "akRight"}
        items.append(dict(w=k, x=x, y=y, r=x + w, b=y + h, cls=k.get("class"), an=an))
    # VCL info texts often reach into the bevel line below them
    for ln in items:
        fs = prop(ln["w"], "frameShape")
        if ln["cls"] == "QFrame" and fs is not None and "HLine" in "".join(fs.itertext()):
            for i in items:
                if i is not ln and i["y"] < ln["y"] < i["b"]:
                    i["b"] = ln["y"]
            ln["y"] = max(ln["y"], max((i["b"] for i in items if i is not ln and i["b"] <= ln["y"] + TOL), default=ln["y"]))
    right_any = any("akRight" in i["an"] for i in items)
    ml = min(i["x"] for i in items)
    mt = max(0, min(i["y"] for i in items) - top_offset)
    mr = max(4, W - max(i["r"] for i in items)) if right_any else 4
    mr = min(mr, max(ml, 8))
    base = c.get("name")
    try:
        build_grid([dict(i) for i in items], "probe", (0, 0, 0, 0), check_only=True)
    except Fail:
        shift_hidden_alternatives(items)
    secs = sections(items)
    if len(secs) == 1:
        lay, grows, nrows, _ = build_grid(items, base + "_Layout", (ml, mt, mr, 8))
        if not grows:
            it = ET.SubElement(lay, "item", {"row": str(nrows), "column": "0"})
            it.append(spacer_xml(base + "_VFill", True))
        desc = "1 section"
    else:
        lay = ET.Element("layout", {"class": "QVBoxLayout", "name": base + "_Layout"})
        margin_props(lay, (ml, mt, mr, 8))
        lay.append(ET.fromstring('<property name="spacing"><number>0</number></property>'))
        stretch, any_grow, prev_grows = [], False, False
        for n, (sec, gap) in enumerate(secs):
            if prev_grows:
                gap = min(gap, 12)  # the growing control above takes the space
            if n > 0:
                it = ET.SubElement(lay, "item")
                it.append(spacer_xml("%s_Gap%d" % (base, n), True, gap, True))
                stretch.append("0")
            # the section's own left edge (controls indented in the design)
            sl = min(i["x"] for i in sec) - ml
            g, grows, _, _ = build_grid(sec, "%s_Section%d" % (base, n + 1), (sl, 0, 0, 0))
            it = ET.SubElement(lay, "item")
            it.append(g)
            stretch.append("1" if grows else "0")
            any_grow = any_grow or grows
            prev_grows = grows
        if not any_grow:
            it = ET.SubElement(lay, "item")
            it.append(spacer_xml(base + "_VFill", True))
            stretch.append("1")
        lay.set("stretch", ",".join(stretch))
        desc = "%d sections" % len(secs)
    for i in items:
        c.remove(i["w"])
    # layout after properties, before remaining non-layout children (menus)
    idx = 0
    for n, ch in enumerate(list(c)):
        if ch.tag == "property":
            idx = n + 1
    c.insert(idx, lay)
    report.append("converted %s (%d controls, %s)" % (base, len(items), desc))
    return True


def walk(c, only, skip, report, is_top):
    # children first: a group box gets its layout before its parent
    for k in list(c.findall("widget")):
        cls = k.get("class")
        if cls in ("QTabWidget", "QStackedWidget", "QScrollArea"):
            for page in k.findall("widget"):
                walk(page, only, skip, report, True)
                if cls == "QScrollArea" and page.find("layout") is not None:
                    # the contents get the size of the viewport, but at least
                    # the size their layout needs (then the scroll bars appear)
                    set_prop(k, "widgetResizable", "<bool>true</bool>")
        elif cls in CONTAINERS:
            walk(k, only, skip, report, False)
    name = c.get("name")
    if skip and name in skip:
        report.append("skipped %s" % name)
        return
    if only and name not in only:
        return
    off = 0
    if c.get("class") == "QGroupBox":
        off = 18
    try:
        convert_container(c, None, report, off)
    except Fail as e:
        report.append("NOT converted %s: %s" % (name, e))


INLINE = {"property", "attribute", "tabstop", "include", "addaction", "zorder"}


def compact(e):
    """element as one line without the whitespace between its tags"""
    e = ET.fromstring(ET.tostring(e, encoding="unicode"))
    for x in e.iter():
        if len(x):
            x.text = (x.text or "").strip() or None
        x.tail = (x.tail or "").strip() or None
    return ET.tostring(e, encoding="unicode").strip()


def serialize(e, depth):
    """same style as dfm2ui.py: properties on one line, two spaces indent"""
    ind = "  " * depth
    if e.tag in INLINE or not len(e):
        return ind + compact(e) + "\n"
    attrs = "".join(' %s="%s"' % (k, v.replace("&", "&amp;").replace('"', "&quot;")) for k, v in e.attrib.items())
    out = ind + "<%s%s>\n" % (e.tag, attrs)
    for c in e:
        out += serialize(c, depth + 1)
    return out + ind + "</%s>\n" % e.tag


def main(argv):
    args = argv[1:]
    dry = "--dry-run" in args
    global GROW_BIG
    GROW_BIG = "--grow-big" in args
    global FREE
    if "--free" in args:
        FREE = set(args[args.index("--free") + 1].split(","))
    only = skip = None
    if "--only" in args:
        only = set(args[args.index("--only") + 1].split(","))
    if "--skip" in args:
        skip = set(args[args.index("--skip") + 1].split(","))
    path = args[0]
    with open(path, encoding="utf-8") as f:
        text = f.read()
    root = ET.fromstring(text)
    top = root.find("widget")
    report = []
    walk(top, only, skip, report, True)
    for r in report:
        print("%s: %s" % (path, r))
    if not dry:
        out = '<?xml version="1.0" encoding="UTF-8"?>\n' + serialize(root, 0)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
