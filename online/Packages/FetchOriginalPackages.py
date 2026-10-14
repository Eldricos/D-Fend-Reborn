#!/usr/bin/env python3
"""Downloads the data packages (ExePackage entries) of a package list.

The original D-Fend Reloaded package list offers several games in one package
as NSIS installers. They are the source of the D-Fend Reborn game packages:
DFRPackageTool splits them into one zip package per game (see
tools/PackageTool/README.md).

Usage: FetchOriginalPackages.py LIST.xml DIR
Downloads every installer of the list into DIR (if not there yet), checks it
against the PackageChecksum (MD5) of the list and prints the file names.
"""
import hashlib
import os
import re
import shutil
import sys
import urllib.request

ENTRY_RE = re.compile(r'<ExePackage\b([^>]*)>([^<]*)</ExePackage>', re.S)


def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest().upper()


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    lst, folder = sys.argv[1], sys.argv[2]
    os.makedirs(folder, exist_ok=True)
    with open(lst, 'rb') as f:
        xml = f.read().decode('latin-1')   # the lists are ISO-8859-1
    for attrs, url in ENTRY_RE.findall(xml):
        url = url.strip()
        checksum = (re.search(r'\bPackageChecksum="([^"]*)"', attrs) or [None, ''])[1].upper()
        dest = os.path.join(folder, url.rsplit('/', 1)[-1])
        if not os.path.exists(dest):
            print('downloading', url, file=sys.stderr)
            req = urllib.request.Request(url, headers={'User-Agent': 'D-Fend Reborn'})
            with urllib.request.urlopen(req, timeout=600) as r, open(dest + '.part', 'wb') as f:
                shutil.copyfileobj(r, f, 1 << 20)
            os.replace(dest + '.part', dest)
        if checksum and md5(dest) != checksum:
            print('%s: checksum mismatch' % dest, file=sys.stderr)
            return 1
        print(dest)
    return 0


if __name__ == '__main__':
    sys.exit(main())
