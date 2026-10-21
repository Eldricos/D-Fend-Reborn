#!/usr/bin/env python3
"""Language strings of D-Fend Reborn: src/core/LanguageStrings.inc and the
English / German language files (data/Lang/English.ini, German.ini).

  langstrings.py check
      Checks the table (unique names, numbers and keys) and that English.ini
      has the English texts of the table and German.ini a text for every
      key. Exit code 1 if something is wrong (used by the build check).

  langstrings.py sync
      Writes the English texts of the table into English.ini (changed texts
      are updated, missing keys added).

  langstrings.py add <Section> <Key> <Name> "<English text>" "<German text>" [--accessor <Accessor>] [--number <Number>]
      Adds a new string: a line in LanguageStrings.inc (after the last string
      of the section, with the next free number after it unless --number is
      given) and the texts in English.ini and German.ini. The
      accessor (LanguageSetup->Accessor()) is <Name> unless given.

Other language files are not changed: strings missing there are shown in
English until a translator adds them.
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(ROOT, 'src', 'core', 'LanguageStrings.inc')
LANG = os.path.join(ROOT, 'data', 'Lang')
INI_ENCODING = 'cp1252'   # [CharsetSettings] of English.ini and German.ini

ROW = re.compile(r'^(LANG_STRING(?:_NO_ACCESSOR|_EXPR|_INFO)?)\((\w+), (?:(\w+), )?(\d+), "([^"]*)", "([^"]*)"(?:, (.*))?\)$')
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def c_unescape(s):
    return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), s)


def c_escape(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


class Row:
    def __init__(self, index, m):
        self.index = index
        self.kind, self.name, self.accessor, number, self.section, self.key, value = m.groups()
        self.number = int(number)
        self.text = None   # English text (None: expression / info)
        if self.kind in ('LANG_STRING', 'LANG_STRING_NO_ACCESSOR'):
            parts = LITERAL.findall(value or '')
            if not parts or LITERAL.sub('', value).strip():
                raise ValueError(f'line {index + 1}: the text must be a string literal')
            self.text = ''.join(c_unescape(p) for p in parts)


def strip_comment(line):
    """line without a trailing // comment (outside string literals)"""
    quote = False
    i = 0
    while i < len(line):
        c = line[i]
        if quote and c == '\\':
            i += 2
            continue
        if c == '"':
            quote = not quote
        elif not quote and line.startswith('//', i):
            return line[:i].rstrip()
        i += 1
    return line.rstrip()


def read_table():
    lines = open(TABLE, encoding='utf-8').read().split('\n')
    rows = []
    for i, l in enumerate(lines):
        if l.startswith('LANG_STRING'):
            m = ROW.match(strip_comment(l))
            if not m:
                raise ValueError(f'{TABLE}:{i + 1}: line not understood: {l}')
            rows.append(Row(i, m))
    return lines, rows


class IniFile:
    """Language file, edited line by line (keeps order, comments, line endings)."""

    def __init__(self, path):
        self.path = path
        raw = open(path, 'rb').read()
        self.newline = '\r\n' if b'\r\n' in raw else '\n'
        self.lines = raw.decode(INI_ENCODING).split(self.newline)

    def find(self, section, key):
        """(line index of the key or None, index after the last line of the section or None)"""
        sec = None
        end = None
        for i, l in enumerate(self.lines):
            s = l.strip()
            if s.startswith('[') and s.endswith(']'):
                sec = s[1:-1].lower()
                continue
            if sec == section.lower():
                if s:
                    end = i + 1
                if '=' in l and l.split('=', 1)[0].strip().lower() == key.lower():
                    return i, end
        return None, end

    def get(self, section, key):
        i, _ = self.find(section, key)
        return None if i is None else self.lines[i].split('=', 1)[1]

    def set(self, section, key, value):
        i, end = self.find(section, key)
        line = f'{key}={value}'
        if i is not None:
            self.lines[i] = line
        elif end is not None:
            self.lines.insert(end, line)
        else:
            while self.lines and not self.lines[-1].strip():
                self.lines.pop()
            self.lines += ['', f'[{section}]', line, '']

    def save(self):
        open(self.path, 'wb').write(self.newline.join(self.lines).encode(INI_ENCODING))


def check(rows):
    errors = []
    seen = {}
    for r in rows:
        for what, value in (('name', r.name), ('number', r.number), ('key', (r.section.lower(), r.key.lower()))):
            if (what, value) in seen:
                errors.append(f'{TABLE}:{r.index + 1}: {what} {value} already used in line {seen[(what, value)] + 1}')
            seen[(what, value)] = r.index
        if r.text is not None:
            try:
                r.text.encode(INI_ENCODING)
            except UnicodeEncodeError:
                errors.append(f'{TABLE}:{r.index + 1}: text cannot be written to the language files ({INI_ENCODING})')
    english = IniFile(os.path.join(LANG, 'English.ini'))
    german = IniFile(os.path.join(LANG, 'German.ini'))
    for r in rows:
        if r.kind == 'LANG_STRING_INFO':
            continue
        value = english.get(r.section, r.key)
        if value is None:
            errors.append(f'English.ini: [{r.section}] {r.key} missing (langstrings.py sync)')
        elif r.text and value != r.text:
            errors.append(f'English.ini: [{r.section}] {r.key} differs from the table (langstrings.py sync)')
        if german.get(r.section, r.key) is None:
            errors.append(f'German.ini: [{r.section}] {r.key} missing')
    return errors


def sync(rows):
    english = IniFile(os.path.join(LANG, 'English.ini'))
    changed = 0
    for r in rows:
        if r.text is None:
            continue
        value = english.get(r.section, r.key)
        if value is None or (r.text and value != r.text):
            english.set(r.section, r.key, r.text)
            changed += 1
    if changed:
        english.save()
    print(f'English.ini: {changed} texts updated')


def add(lines, rows, a):
    if not re.fullmatch(r'\w+', a.name):
        sys.exit('The name may only contain letters, digits and "_".')
    accessor = a.accessor or a.name
    for r in rows:
        if r.name == a.name or (r.section.lower(), r.key.lower()) == (a.section.lower(), a.key.lower()):
            sys.exit(f'Already in the table (line {r.index + 1}): {lines[r.index]}')
    same = [r for r in rows if r.section.lower() == a.section.lower()]
    used = {r.number for r in rows}
    if a.number is not None:
        number = a.number
        if number in used:
            sys.exit(f'Number {number} is already used.')
    elif same:
        # after the number of the last string of the section
        number = max(same, key=lambda r: r.index).number + 1
        while number in used:
            number += 1
    else:
        sys.exit(f'Section [{a.section}] is new: give the number with --number.')
    row = f'LANG_STRING({a.name}, {accessor}, {number}, "{a.section}", "{a.key}", {c_escape(a.english)})'
    if same:
        lines.insert(max(r.index for r in same) + 1, row)
    else:
        end = next(i for i, l in enumerate(lines) if l.startswith('#undef'))
        lines[end:end] = [f'// ----- [{a.section}]', row, '']
    for text in (a.english, a.german):
        text.encode(INI_ENCODING)
    open(TABLE, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    for file, text in (('English.ini', a.english), ('German.ini', a.german)):
        ini = IniFile(os.path.join(LANG, file))
        ini.set(a.section, a.key, text)
        ini.save()
    print(row)
    print(f'Use: LanguageSetup->{accessor}()')


def main():
    p = argparse.ArgumentParser(description='Language strings of D-Fend Reborn')
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('check')
    sub.add_parser('sync')
    pa = sub.add_parser('add')
    pa.add_argument('section')
    pa.add_argument('key')
    pa.add_argument('name')
    pa.add_argument('english')
    pa.add_argument('german')
    pa.add_argument('--accessor')
    pa.add_argument('--number', type=int)
    a = p.parse_args()
    lines, rows = read_table()
    if a.command == 'check':
        errors = check(rows)
        for e in errors:
            print(e)
        print(f'{len(rows)} strings, {len(errors)} problems')
        sys.exit(1 if errors else 0)
    if a.command == 'sync':
        sync(rows)
    if a.command == 'add':
        add(lines, rows, a)


if __name__ == '__main__':
    main()
