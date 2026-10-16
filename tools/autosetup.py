#!/usr/bin/env python3
"""Tool for the auto setup templates of D-Fend Reborn
(data/NewUserData/AutoSetup/*.prof, format: docs/AutoSetupTemplates.md).

  autosetup.py check [FILE ...]      check templates (default: all); exit code 1
                                     if a template has errors
  autosetup.py fix FILE ...          normalise templates in place (UTF-8, standard
                                     genre / language / license values, year,
                                     relative mounts); then run "check"
  autosetup.py lookup NAME [--dos]   search Wikidata for a game and show the
                                     values for the [ExtraInfo] section

Run it from the repository root. The standard values are read from
data/Bin/GameInfo.ini (the same file D-Fend Reborn uses).
"""
import configparser
import glob
import json
import os
import re
import sys
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATES = os.path.join(ROOT, "data", "NewUserData", "AutoSetup")
GAMEINFO = os.path.join(ROOT, "data", "Bin", "GameInfo.ini")
USER_AGENT = "D-Fend-Reborn-AutoSetupTool/1.0 (https://github.com/Eldricos/D-Fend-Reborn)"
SPARQL = "https://query.wikidata.org/sparql"
MSDOS = "Q31845"      # Wikidata: MS-DOS
VIDEO_GAME = "Q7889"  # Wikidata: video game


# --- standard values -----------------------------------------------------------

def load_gameinfo():
    cp = configparser.ConfigParser(delimiters=("=",), comment_prefixes=(";",), interpolation=None, strict=False)
    cp.optionxform = str
    with open(GAMEINFO, encoding="utf-8") as f:
        cp.read_file(f)
    sec = lambda s: dict(cp.items(s)) if cp.has_section(s) else {}
    info = {}
    for kind in ("Genre", "Language", "License"):
        values = list(sec(kind + "s").keys())
        aliases = {k.lower(): v for k, v in sec(kind + "Aliases").items()}
        aliases.update({v.lower(): v for v in values})
        info[kind] = (values, aliases)
    info["Wikidata"] = {k.lower(): v for k, v in sec("WikidataGenres").items()}
    return info


INFO = load_gameinfo()


def normalise_list(kind, value):
    """'Plattform;Sport' -> ('Plattform;Sports', [unknown parts])"""
    values, aliases = INFO[kind]
    out, unknown = [], []
    for part in re.split(r"[;,]", value):
        part = part.strip()
        if not part:
            continue
        std = aliases.get(part.lower())
        if std is None:
            unknown.append(part)
            std = part
        for p in std.split(";"):
            if p and p not in out:
                out.append(p)
    return ";".join(out), unknown


# --- reading / writing templates ---------------------------------------------

class Template:
    """A .prof file as list of lines (keeps order, comments and unknown keys)."""

    def __init__(self, path):
        self.path = path
        self.raw = open(path, "rb").read()
        try:
            self.text = self.raw.decode("utf-8")
            self.utf8 = True
        except UnicodeDecodeError:
            # D-Fend Reloaded wrote ANSI (Windows-1252) files
            self.text = self.raw.decode("cp1252", errors="replace")
            self.utf8 = False
        if self.text.startswith("﻿"):
            self.text = self.text[1:]
        self.lines = self.text.replace("\r\n", "\n").replace("\r", "\n").split("\n")

    def sections(self):
        return [l[1:-1] for l in self.lines if l.startswith("[") and l.endswith("]")]

    def _find(self, section, key):
        cur = None
        for i, l in enumerate(self.lines):
            if l.startswith("[") and l.endswith("]"):
                cur = l[1:-1]
            elif cur == section and l.split("=", 1)[0] == key and "=" in l:
                return i
        return -1

    def get(self, section, key, default=""):
        i = self._find(section, key)
        return self.lines[i].split("=", 1)[1] if i >= 0 else default

    def set(self, section, key, value):
        i = self._find(section, key)
        if i >= 0:
            self.lines[i] = key + "=" + value
            return
        for j, l in enumerate(self.lines):
            if l == "[" + section + "]":
                self.lines.insert(j + 1, key + "=" + value)
                return
        self.lines += ["", "[" + section + "]", key + "=" + value]

    def save(self):
        text = "\n".join(self.lines)
        with open(self.path, "wb") as f:
            f.write(text.encode("utf-8"))


def user_info(value):
    """'License=Shareware[13][10]Version=2.0[13][10]' -> [('License','Shareware'), ...]"""
    items = []
    for part in value.replace("[13][10]", "\n").replace("[13]", "\n").replace("[10]", "\n").split("\n"):
        part = part.strip()
        if "=" in part:
            k, v = part.split("=", 1)
            items.append((k.strip(), v.strip()))
    return items


def join_user_info(items):
    return "".join(k + "=" + v + "[13][10]" for k, v in items)


MOUNT_RE = re.compile(r"^(\.\\VirtualHD(\\.*)?|ASK|NUMBER:.*|LABEL:.*|FILE:.*|FOLDER:.*)$", re.I)


# --- check ---------------------------------------------------------------------

def check_template(t, md5_index):
    errors, warnings = [], []
    name = os.path.basename(t.path)
    if not t.utf8:
        errors.append("not UTF-8 (convert with: autosetup.py fix)")
    for sec in ("Extra", "ExtraInfo"):
        if sec not in t.sections():
            errors.append("section [%s] missing" % sec)
    mode = t.get("Extra", "ProfileMode", "DOSBox")
    if mode not in ("DOSBox", "ScummVM"):
        errors.append("ProfileMode must be DOSBox or ScummVM (is %r)" % mode)
    if not t.get("ExtraInfo", "Name").strip():
        errors.append("Name is empty")
    exe = t.get("Extra", "Exe").strip()
    md5 = t.get("Extra", "ExeMD5").strip()
    boot = t.get("Extra", "BootImage").strip()
    if mode == "DOSBox" and boot and not exe:
        warnings.append("boot image template without program file: not recognised automatically (only via \"Add from template\")")
    elif mode == "DOSBox":
        if not exe:
            errors.append("Exe is empty (the program file is needed to recognise the game)")
        elif re.match(r"^([A-Za-z]:|/)", exe):
            errors.append("Exe must not be an absolute path: %s" % exe)
        if not re.fullmatch(r"[0-9A-F]{32}", md5):
            errors.append("ExeMD5 must be the MD5 checksum of the program file (32 hex digits, upper case)")
    for i in range(10):
        m = t.get("Extra", str(i)).strip()
        if m:
            folder = m.split(";")[0].strip()
            if not MOUNT_RE.match(folder):
                errors.append("mount %d must be inside .\\VirtualHD (or ASK, NUMBER:, LABEL:, FILE:, FOLDER:): %s" % (i, folder))
    year = t.get("ExtraInfo", "Year").strip()
    if year and not re.fullmatch(r"(19[7-9]\d|20[0-4]\d)", year):
        errors.append("Year must be a 4 digit year (is %r)" % year)
    for kind, key in (("Genre", "Genre"), ("Language", "Language")):
        v = t.get("ExtraInfo", key)
        norm, unknown = normalise_list(kind, v)
        if unknown:
            errors.append("%s: unknown value(s) %s (allowed: %s)" % (key, ", ".join(unknown), ";".join(INFO[kind][0])))
        elif norm != v.strip():
            errors.append("%s should be %r (standard spelling)" % (key, norm))
    lic = [v for k, v in user_info(t.get("ExtraInfo", "UserInfo")) if k == "License"]
    if lic:
        norm, unknown = normalise_list("License", lic[0])
        if unknown:
            errors.append("License: unknown value %s (allowed: %s)" % (lic[0], ";".join(INFO["License"][0])))
        elif norm != lic[0]:
            errors.append("License should be %r" % norm)
    else:
        warnings.append("License is missing (UserInfo=License=...[13][10])")
    for key in ("Genre", "Developer", "Publisher", "Year", "Language"):
        if not t.get("ExtraInfo", key).strip():
            warnings.append("%s is empty" % key)
    others = [o for o in md5_index.get(md5, []) if o != name] if md5 else []
    if others:
        warnings.append("same ExeMD5 as %s (add File1..File5 checksums in [ExtraGameIdentify] to tell them apart)" % ", ".join(others))
    return errors, warnings


def build_md5_index():
    index = {}
    for p in glob.glob(os.path.join(TEMPLATES, "*.prof")):
        md5 = Template(p).get("Extra", "ExeMD5").strip()
        if md5:
            index.setdefault(md5, []).append(os.path.basename(p))
    return index


def cmd_check(files):
    files = files or sorted(glob.glob(os.path.join(TEMPLATES, "*.prof")))
    md5_index = build_md5_index()
    n_err = n_warn = 0
    gh = os.environ.get("GITHUB_ACTIONS") == "true"
    for p in files:
        t = Template(p)
        errors, warnings = check_template(t, md5_index)
        rel = os.path.relpath(p, ROOT)
        for e in errors:
            print(("::error file=%s::%s" if gh else "ERROR   %s: %s") % (rel, e))
        if len(files) <= 20 or not gh:
            for w in warnings:
                print(("::warning file=%s::%s" if gh else "warning %s: %s") % (rel, w))
        n_err += bool(errors)
        n_warn += bool(warnings)
    print("%d templates checked: %d with errors, %d with warnings" % (len(files), n_err, n_warn))
    return 1 if n_err else 0


# --- fix -----------------------------------------------------------------------

def fix_template(t):
    for kind, key in (("Genre", "Genre"), ("Language", "Language")):
        v = t.get("ExtraInfo", key)
        norm, _ = normalise_list(kind, v)
        if norm != v:
            t.set("ExtraInfo", key, norm)
    year = t.get("ExtraInfo", "Year").strip()
    m = re.search(r"(19[7-9]\d|20[0-4]\d)", year)
    t.set("ExtraInfo", "Year", m.group(1) if m else "")
    items = user_info(t.get("ExtraInfo", "UserInfo"))
    if items:
        items = [(k, normalise_list("License", v)[0] if k == "License" else v) for k, v in items]
        t.set("ExtraInfo", "UserInfo", join_user_info(items))
    for key in ("Name", "Developer", "Publisher"):
        v = t.get("ExtraInfo", key)
        fixed = v.strip()
        try:  # text that was UTF-8 but read as Windows-1252 ("IndÃºstria")
            if re.search("[ÂÃ][\u0080-¿]", fixed):
                fixed = fixed.encode("cp1252").decode("utf-8")
        except (UnicodeEncodeError, UnicodeDecodeError):
            pass
        if fixed != v:
            t.set("ExtraInfo", key, fixed)
    m0 = t.get("Extra", "0")
    if m0 and not MOUNT_RE.match(m0.split(";")[0].strip()):
        parts = m0.split(";")
        parts[0] = ".\\VirtualHD\\"
        t.set("Extra", "0", ";".join(parts))
    t.utf8 = True
    t.save()


def cmd_fix(files):
    if not files:
        print("give the template files to fix")
        return 2
    for p in files:
        fix_template(Template(p))
    return cmd_check(files)


# --- Wikidata --------------------------------------------------------------------

def sparql(query):
    url = SPARQL + "?" + urllib.parse.urlencode({"query": query, "format": "json"})
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/sparql-results+json"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)["results"]["bindings"]


def sparql_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def wikidata_entity_search(name, limit=50):
    """Wikidata items whose label or alias matches name (search order)"""
    url = "https://www.wikidata.org/w/api.php?" + urllib.parse.urlencode({
        "action": "wbsearchentities", "search": name, "language": "en", "uselang": "en",
        "type": "item", "limit": str(limit), "format": "json"})
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=60) as r:
        return [e["id"] for e in json.load(r).get("search", [])]


def wikidata_search(name, dos_only):
    # (the same requests as D-Fend Reborn, src/core/GameInfoUnit.cpp):
    # 1. search the items by name, 2. keep video games and MS-DOS programs
    ids = wikidata_entity_search(name)
    if not ids:
        return []
    q = """SELECT ?item ?itemLabel (MIN(YEAR(?date)) AS ?year) (MAX(?isdos) AS ?dos) WHERE {
  VALUES ?item { %s }
  { ?item wdt:P31/wdt:P279* wd:%s . } UNION { ?item wdt:P400 wd:%s . }
  OPTIONAL { ?item wdt:P577 ?date . }
  BIND(EXISTS { ?item wdt:P400 wd:%s . } AS ?isdos)
  SERVICE wikibase:label { bd:serviceParam wikibase:language "en" . }
} GROUP BY ?item ?itemLabel""" % (" ".join("wd:" + i for i in ids), VIDEO_GAME, MSDOS, MSDOS)
    found = {}
    for b in sparql(q):
        qid = b["item"]["value"].rsplit("/", 1)[1]
        dos = b.get("dos", {}).get("value", "false") in ("true", "1")
        if dos_only and not dos:
            continue
        found[qid] = (qid, b.get("itemLabel", {}).get("value", ""), b.get("year", {}).get("value", ""))
    return [found[i] for i in ids if i in found]


def wikidata_details(qid):
    q = """SELECT ?itemLabel ?itemDescription ?devLabel ?pubLabel ?genreLabel ?date ?website ?article WHERE {
  VALUES ?item { wd:%s }
  OPTIONAL { ?item wdt:P178 ?dev . }
  OPTIONAL { ?item wdt:P123 ?pub . }
  OPTIONAL { ?item wdt:P136 ?genre . }
  OPTIONAL { ?item wdt:P577 ?date . }
  OPTIONAL { ?item wdt:P856 ?website . }
  OPTIONAL { ?article schema:about ?item ; schema:isPartOf <https://en.wikipedia.org/> . }
  SERVICE wikibase:label { bd:serviceParam wikibase:language "en" . }
}""" % qid
    rows = sparql(q)
    def values(key):
        out = []
        for r in rows:
            v = r.get(key, {}).get("value", "")
            if v and v not in out:
                out.append(v)
        return out
    genres, unmapped = [], []
    for g in values("genreLabel"):
        std = INFO["Wikidata"].get(g.lower())
        if std is None:
            unmapped.append(g)
        for p in (std or "").split(";"):
            if p and p not in genres:
                genres.append(p)
    years = sorted(d[:4] for d in values("date"))
    www = (values("website") or values("article") or ["https://www.wikidata.org/wiki/" + qid])[0]
    return {
        "Name": (values("itemLabel") or [""])[0],
        "Genre": ";".join(genres),
        "Developer": ", ".join(values("devLabel")),
        "Publisher": ", ".join(values("pubLabel")),
        "Year": years[0] if years else "",
        "WWW": www,
        "Notes": (values("itemDescription") or [""])[0],
        "_unmapped_genres": unmapped,
    }


def cmd_lookup(args):
    dos = "--dos" in args
    name = " ".join(a for a in args if a != "--dos")
    if not name:
        print("give a game name")
        return 2
    hits = wikidata_search(name, dos)
    if not hits:
        print("no games found")
        return 1
    for qid, label, year in hits[:10]:
        print("%-10s %s (%s)" % (qid, label, year or "?"))
    d = wikidata_details(hits[0][0])
    print("\n[ExtraInfo] values of %s (https://www.wikidata.org/wiki/%s):" % (hits[0][1], hits[0][0]))
    for k in ("Name", "Genre", "Developer", "Publisher", "Year", "WWW", "Notes"):
        print("%s=%s" % (k, d[k]))
    if d["_unmapped_genres"]:
        print("(Wikidata genres without standard genre: %s)" % ", ".join(d["_unmapped_genres"]))
    return 0


def main(argv):
    if len(argv) < 2 or argv[1] not in ("check", "fix", "lookup"):
        print(__doc__)
        return 2
    return {"check": cmd_check, "fix": cmd_fix, "lookup": cmd_lookup}[argv[1]](argv[2:])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
