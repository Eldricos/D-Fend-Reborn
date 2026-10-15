#!/usr/bin/env python3
"""Checks the links of the help pages (data/Help/<language>/*.html).

- internal links (other help pages and their anchors) and images must exist
  (errors: exit code 1),
- external links (http/https) are requested; links that cannot be reached are
  reported as warnings (web sites can be down for a while).

Usage: check_help_links.py [--no-external]   (run from the repository root)
"""
import glob
import os
import re
import sys
import urllib.error
import urllib.request

HELP = os.path.join("data", "Help")
HREF = re.compile(r'''(href|src)\s*=\s*"([^"]*)"''', re.I)


def anchors(path, cache={}):
    if path not in cache:
        text = open(path, encoding="latin-1").read()
        cache[path] = set(re.findall(r'''(?:name|id)\s*=\s*"([^"]*)"''', text, re.I))
    return cache[path]


def check_external(url):
    headers = {"User-Agent": "Mozilla/5.0 (D-Fend Reborn help link check)"}
    for method in ("HEAD", "GET"):
        try:
            req = urllib.request.Request(url, headers=headers, method=method)
            with urllib.request.urlopen(req, timeout=30) as r:
                return r.status, r.geturl()
        except urllib.error.HTTPError as e:
            if method == "GET" or e.code not in (403, 405, 501):
                return e.code, url
        except Exception as e:  # noqa: BLE001 - report every problem
            if method == "GET":
                return str(e), url
    return "?", url


def main():
    external = "--no-external" not in sys.argv
    errors = []
    links = {}
    for page in sorted(glob.glob(os.path.join(HELP, "*", "*.html"))):
        folder = os.path.dirname(page)
        for kind, ref in HREF.findall(open(page, encoding="latin-1").read()):
            ref = ref.strip()
            if ref.startswith(("http://", "https://")):
                links.setdefault(ref, []).append(page)
                continue
            if not ref or ref == "#" or ref.startswith(("mailto:", "javascript:")):
                continue
            name, _, anchor = ref.partition("#")
            target = os.path.normpath(os.path.join(folder, name)) if name else page
            if not os.path.exists(target):
                errors.append(f"{page}: {kind} \"{ref}\" - file not found")
            elif anchor and target.endswith(".html") and anchor not in anchors(target):
                errors.append(f"{page}: link \"{ref}\" - anchor not found")

    for e in errors:
        print(f"::error::{e}")
    print(f"{len(errors)} broken internal links / images")

    if external:
        bad = 0
        for url in sorted(links):
            status, final = check_external(url)
            ok = isinstance(status, int) and status < 400
            moved = f" -> {final}" if final.rstrip("/") != url.rstrip("/") else ""
            print(f"{'OK  ' if ok else 'FAIL'} {status} {url}{moved}")
            if not ok:
                bad += 1
                pages = ", ".join(sorted(set(os.path.relpath(p, HELP) for p in links[url])))
                print(f"::warning::{url} ({status}) used in {pages}")
        print(f"{len(links)} external links, {bad} not reachable")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
