"""Parser for Delphi text DFM files.

Produces a tree of DfmObject instances.  Property values are converted to
Python values:
  identifiers / enums  -> Ident(str)
  strings              -> str
  integers / floats    -> int / float
  sets [a, b]          -> Set(list of str)
  lists ( ... )        -> list
  binary { ... }       -> bytes
  collections < ... >  -> Collection(list of dict)
"""
import re


class Ident(str):
    pass


class Set(list):
    pass


class Collection(list):
    pass


class DfmObject:
    def __init__(self, kind, name, cls):
        self.kind = kind          # 'object' | 'inherited' | 'inline'
        self.name = name          # may be '' for unnamed objects (TPage)
        self.cls = cls
        self.props = {}
        self.children = []
        self.parent = None

    def get(self, key, default=None):
        return self.props.get(key, default)

    def walk(self):
        yield self
        for c in self.children:
            yield from c.walk()

    def __repr__(self):
        return f"<{self.name}:{self.cls}>"


_tok_re = re.compile(r"""
    (?P<ws>[ \t\r\n]+)
  | (?P<str>(?:'(?:[^']|'')*'|\#\d+)+)
  | (?P<num>[-+]?\d+\.\d+(?:[eE][-+]?\d+)?|[-+]?\d+(?:[eE][-+]?\d+)?|\$[0-9A-Fa-f]+|-\$[0-9A-Fa-f]+)
  | (?P<ident>[A-Za-z_][A-Za-z0-9_.]*)
  | (?P<bin>\{[0-9A-Fa-f\s]*\})
  | (?P<sym>[=:\[\]\(\)<>,+])
""", re.X)


def _decode_str(tok, codec='cp1252'):
    out = []
    i = 0
    while i < len(tok):
        if tok[i] == "'":
            j = i + 1
            buf = []
            while True:
                if tok[j] == "'":
                    if j + 1 < len(tok) and tok[j + 1] == "'":
                        buf.append("'")
                        j += 2
                        continue
                    break
                buf.append(tok[j])
                j += 1
            out.append(''.join(buf))
            i = j + 1
        elif tok[i] == '#':
            j = i + 1
            while j < len(tok) and tok[j].isdigit():
                j += 1
            code = int(tok[i + 1:j])
            if code < 256:
                out.append(bytes([code]).decode(codec, errors='replace') if code >= 128 else chr(code))
            else:
                out.append(chr(code))
            i = j
        else:
            i += 1
    return ''.join(out)


class Tokenizer:
    def __init__(self, text):
        self.toks = []
        pos = 0
        while pos < len(text):
            m = _tok_re.match(text, pos)
            if not m:
                raise SyntaxError(f"Bad char at {pos}: {text[pos:pos+30]!r}")
            pos = m.end()
            kind = m.lastgroup
            if kind == 'ws':
                continue
            self.toks.append((kind, m.group(kind)))
        self.i = 0

    def peek(self, k=0):
        if self.i + k < len(self.toks):
            return self.toks[self.i + k]
        return (None, None)

    def next(self):
        t = self.toks[self.i]
        self.i += 1
        return t

    def expect(self, val):
        t = self.next()
        if t[1] != val:
            raise SyntaxError(f"expected {val!r} got {t!r}")
        return t


def _parse_value(tz):
    kind, val = tz.next()
    if kind == 'str':
        s = _decode_str(val)
        while tz.peek()[1] == '+':
            tz.next()
            k2, v2 = tz.next()
            s += _decode_str(v2)
        return s
    if kind == 'num':
        if val.startswith('$'):
            return int(val[1:], 16)
        if val.startswith('-$'):
            return -int(val[2:], 16)
        if '.' in val or 'e' in val.lower():
            return float(val)
        return int(val)
    if kind == 'ident':
        return Ident(val)
    if kind == 'bin':
        return bytes.fromhex(re.sub(r'\s', '', val[1:-1]))
    if val == '[':
        items = []
        while tz.peek()[1] != ']':
            k, v = tz.next()
            if v == ',':
                continue
            items.append(v)
        tz.next()
        return Set(items)
    if val == '(':
        items = []
        while tz.peek()[1] != ')':
            items.append(_parse_value(tz))
        tz.next()
        return items
    if val == '<':
        items = Collection()
        while tz.peek()[1] != '>':
            k, v = tz.next()
            if v.lower() != 'item':
                raise SyntaxError(f"expected item got {v}")
            props = {}
            while tz.peek()[1].lower() != 'end':
                pk, pname = tz.next()
                tz.expect('=')
                props[pname] = _parse_value(tz)
            tz.next()
            items.append(props)
        tz.next()
        return items
    if val == '-' or val == '+':
        return _parse_value(tz)
    raise SyntaxError(f"unexpected value token {kind} {val!r}")


def _parse_object(tz):
    kind, kw = tz.next()
    kw = kw.lower()
    if kw not in ('object', 'inherited', 'inline'):
        raise SyntaxError(f"expected object got {kw}")
    # 'object Name: TClass' or 'object TClass'
    k1, v1 = tz.next()
    if tz.peek()[1] == ':':
        tz.next()
        k2, cls = tz.next()
        name = v1
    else:
        name, cls = '', v1
    # optional [index]
    if tz.peek()[1] == '[':
        while tz.next()[1] != ']':
            pass
    obj = DfmObject(kw, name, cls)
    while True:
        k, v = tz.peek()
        if v is None:
            raise SyntaxError("unexpected EOF")
        if v.lower() == 'end':
            tz.next()
            break
        if v.lower() in ('object', 'inherited', 'inline') and tz.peek(1)[0] == 'ident' and tz.peek(1)[1] != '=':
            # child object (careful: a property may be named 'Object'? not in VCL DFMs)
            if tz.peek(1)[1] != '=' and tz.peek(2)[1] != '=':
                child = _parse_object(tz)
                child.parent = obj
                obj.children.append(child)
                continue
        tz.next()
        tz.expect('=')
        obj.props[v] = _parse_value(tz)
    return obj


def parse_dfm(path):
    with open(path, 'rb') as f:
        raw = f.read()
    text = raw.decode('cp1252', errors='replace')
    tz = Tokenizer(text)
    return _parse_object(tz)


if __name__ == '__main__':
    import sys
    root = parse_dfm(sys.argv[1])
    for o in root.walk():
        d = 0
        p = o.parent
        while p:
            d += 1
            p = p.parent
        print('  ' * d + f"{o.name}: {o.cls}", {k: (v if not isinstance(v, bytes) else f'<{len(v)} bytes>') for k, v in o.props.items() if k not in ('Left', 'Top', 'Width', 'Height')})
