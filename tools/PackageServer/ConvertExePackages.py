#!/usr/bin/env python3
"""Converts the data packages of a package list from NSIS installers to .7z.

The original package lists offer several games in one package as NSIS
installers ("ExePackage" entries with a .exe URL), which only work on
Windows. D-Fend Reborn installs data packages from .7z archives on all
systems (src/core/DataPackageUnit.cpp). This script

  1. downloads the installers of all ExePackage entries (or takes them from
     the --cache folder),
  2. checks them against the PackageChecksum (MD5) of the list,
  3. extracts them with 7-Zip (it reads NSIS installers; Linux: "7z" from the
     7zip / p7zip-full package or "7zz"),
  4. packs the data folder part (Confs, VirtualHD, Capture, IconLibrary, ...)
     into D-Fend-Reborn-<name>.7z and
  5. writes a copy of the package list with the new URLs, checksums and sizes.

Usage:
  ConvertExePackages.py LIST.xml OUTDIR --url-base https://.../download/data-packages/
                        [--cache DIR] [--sevenzip 7z]
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request

ENTRY_RE = re.compile(r'(<ExePackage\b(?P<attrs>[^>]*)>)(?P<url>[^<]*)(</ExePackage>)', re.S)


def attr(attrs, name):
    m = re.search(r'\b%s="([^"]*)"' % name, attrs)
    return m.group(1) if m else ''


def set_attr(attrs, name, value):
    return re.sub(r'(\b%s=")[^"]*(")' % name, lambda m: m.group(1) + value + m.group(2), attrs)


def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest().upper()


def download(url, dest):
    print('  downloading', url)
    req = urllib.request.Request(url, headers={'User-Agent': 'D-Fend Reborn package converter'})
    with urllib.request.urlopen(req, timeout=300) as r, open(dest + '.part', 'wb') as f:
        shutil.copyfileobj(r, f, 1 << 20)
    os.replace(dest + '.part', dest)


def find_root(folder):
    """The folder with the data folder layout (contains "Confs")."""
    if os.path.isdir(os.path.join(folder, 'Confs')):
        return folder
    for name in sorted(os.listdir(folder)):
        sub = os.path.join(folder, name)
        if os.path.isdir(os.path.join(sub, 'Confs')):
            return sub
    return None


def convert(exe, out7z, sevenzip):
    with tempfile.TemporaryDirectory(prefix='dfr-pkg-') as tmp:
        subprocess.run([sevenzip, 'x', '-y', '-bso0', '-bsp0', '-o' + tmp, exe], check=True)
        root = find_root(tmp)
        if root is None:
            raise RuntimeError('%s does not contain a data folder (no "Confs" folder)' % exe)
        if os.path.exists(out7z):
            os.remove(out7z)
        entries = sorted(os.listdir(root))
        subprocess.run([sevenzip, 'a', '-t7z', '-mx=9', '-ms=on', '-bso0', '-bsp0', os.path.abspath(out7z)] + entries,
                       cwd=root, check=True)
        profiles = len([n for n in os.listdir(os.path.join(root, 'Confs')) if n.lower().endswith('.prof')])
        return profiles


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('list', help='package list (DFRPackagesFile XML)')
    p.add_argument('outdir', help='folder for the .7z packages and the new package list')
    p.add_argument('--url-base', required=True, help='download URL of the folder / release that will hold the .7z files')
    p.add_argument('--cache', help='folder with already downloaded installers (file name as in the URL)')
    p.add_argument('--sevenzip', default=shutil.which('7z') or shutil.which('7zz') or '7z', help='7-Zip program')
    args = p.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    cache = args.cache or os.path.join(args.outdir, 'cache')
    os.makedirs(cache, exist_ok=True)
    url_base = args.url_base if args.url_base.endswith('/') else args.url_base + '/'

    with open(args.list, 'rb') as f:
        xml = f.read().decode('latin-1')   # the lists are ISO-8859-1

    def replace(m):
        attrs, url = m.group('attrs'), m.group('url').strip()
        name = attr(attrs, 'Name')
        if not url.lower().endswith('.exe'):
            print('skipping', name, '(no installer)')
            return m.group(0)
        print(name)
        exe = os.path.join(cache, url.rsplit('/', 1)[-1])
        if not os.path.exists(exe):
            download(url, exe)
        checksum = attr(attrs, 'PackageChecksum').upper()
        if checksum and md5(exe) != checksum:
            raise RuntimeError('%s: checksum mismatch (expected %s, got %s)' % (exe, checksum, md5(exe)))
        base = os.path.splitext(os.path.basename(exe))[0].replace('D-Fend-Reloaded', 'D-Fend-Reborn')
        out7z = os.path.join(args.outdir, base + '.7z')
        profiles = convert(exe, out7z, args.sevenzip)
        size = os.path.getsize(out7z)
        print('  -> %s (%d profiles, %d bytes)' % (out7z, profiles, size))
        attrs = set_attr(attrs, 'PackageChecksum', md5(out7z))
        attrs = set_attr(attrs, 'Size', str(size))
        return m.group(1).replace(m.group('attrs'), attrs, 1) + url_base + os.path.basename(out7z) + m.group(4)

    new_xml = ENTRY_RE.sub(replace, xml)
    out_list = os.path.join(args.outdir, os.path.basename(args.list))
    with open(out_list, 'wb') as f:
        f.write(new_xml.encode('latin-1'))
    print('package list:', out_list)


if __name__ == '__main__':
    sys.exit(main())
