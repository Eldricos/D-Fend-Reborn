#!/usr/bin/env python3
"""Update the DOSBox Staging version included in D-Fend Reborn
(cmake/DOSBoxStaging.cmake: version and SHA-256 checksum of the Windows zip).

  update-dosbox-staging.py check            show the included and the latest
                                            DOSBox Staging version
  update-dosbox-staging.py apply [VERSION]  download the Windows zip of VERSION
                                            (default: the latest release),
                                            check it and write the version and
                                            the checksum to DOSBoxStaging.cmake

Run it from the repository root. After "apply": build, test some games, add a
line to the change logs and commit cmake/DOSBoxStaging.cmake. The release
workflow takes the version from the cmake file (it also attaches the matching
source archive to the release).

In GitHub Actions "check" writes current=, latest= and update=true/false to
$GITHUB_OUTPUT.
"""
import hashlib
import io
import os
import re
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CMAKE = os.path.join(ROOT, "cmake", "DOSBoxStaging.cmake")
REPO = "dosbox-staging/dosbox-staging"
ZIP_NAME = "dosbox-staging-windows-x64-v{0}.zip"
ZIP_URL = "https://github.com/%s/releases/download/v{0}/%s" % (REPO, ZIP_NAME)
VERSION_RE = re.compile(r"^set\(DFR_DOSBOX_STAGING_VERSION ([0-9.]+)\)", re.M)
SHA_RE = re.compile(r"^set\(DFR_DOSBOX_STAGING_SHA256 ([0-9a-f]{64})\)", re.M)


def request(url):
    req = urllib.request.Request(url, headers={"User-Agent": "D-Fend-Reborn-update-dosbox-staging"})
    return urllib.request.urlopen(req, timeout=300)


def version_key(v):
    return tuple(int(x) for x in v.split("."))


def current_version():
    with open(CMAKE, encoding="utf-8") as f:
        m = VERSION_RE.search(f.read())
    if not m:
        sys.exit("DFR_DOSBOX_STAGING_VERSION not found in " + CMAKE)
    return m.group(1)


def latest_version():
    """Latest release of DOSBox Staging (github.com/.../releases/latest
    redirects to the tag of the latest release; pre-releases are not used)."""
    with request("https://github.com/%s/releases/latest" % REPO) as r:
        m = re.search(r"/releases/tag/v?(\d+\.\d+(?:\.\d+)?)$", r.geturl())
    if not m:
        sys.exit("could not find the latest DOSBox Staging release")
    return m.group(1)


def cmd_check(args):
    cur, new = current_version(), latest_version()
    update = version_key(new) > version_key(cur)
    print("included: %s, latest: %s%s" % (cur, new, " (update available)" if update else ""))
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as f:
            f.write("current=%s\nlatest=%s\nupdate=%s\n" % (cur, new, "true" if update else "false"))
    return 0


def cmd_apply(args):
    version = args[0].lstrip("v") if args else latest_version()
    url = ZIP_URL.format(version)
    print("downloading " + url)
    with request(url) as r:
        data = r.read()
    sha = hashlib.sha256(data).hexdigest()
    # same checks as cmake/DOSBoxStaging.cmake: one folder with dosbox.exe
    names = zipfile.ZipFile(io.BytesIO(data)).namelist()
    tops = {n.split("/")[0] for n in names}
    if len(tops) != 1 or (tops.pop() + "/dosbox.exe") not in names:
        sys.exit("unexpected content of %s (expected one folder with dosbox.exe); "
                 "cmake/DOSBoxStaging.cmake has to be adapted" % ZIP_NAME.format(version))
    with open(CMAKE, encoding="utf-8") as f:
        text = f.read()
    text = VERSION_RE.sub("set(DFR_DOSBOX_STAGING_VERSION %s)" % version, text, count=1)
    text, n = SHA_RE.subn("set(DFR_DOSBOX_STAGING_SHA256 %s)" % sha, text, count=1)
    if n != 1:
        sys.exit("DFR_DOSBOX_STAGING_SHA256 not found in " + CMAKE)
    with open(CMAKE, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("cmake/DOSBoxStaging.cmake: DOSBox Staging %s, SHA-256 %s" % (version, sha))
    return 0


def main(argv):
    if len(argv) < 2 or argv[1] not in ("check", "apply"):
        print(__doc__)
        return 2
    return {"check": cmd_check, "apply": cmd_apply}[argv[1]](argv[2:])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
