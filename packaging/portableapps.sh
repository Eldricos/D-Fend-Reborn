#!/bin/sh
# Builds the PortableApps.com package D-Fend-Reborn-<version>-Portable.paf.exe
# from a portable program folder (like the one of the portable zip: data
# folders next to DFend.exe, DFend.dat with PORTABLEMODE), using the package
# template in "tools/PortableApps package" (launcher and installer source).
#
# Usage (from the repository root):
#   portableapps.sh <portable program folder> <version x.y.z> <output folder> [makensis]
set -e
SRC="$1"
VERSION="$2"
OUT="$3"
MAKENSIS="${4:-makensis}"
[ -f "$SRC/DFend.exe" ] || { echo "no DFend.exe in $SRC"; exit 1; }
echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo "version must be x.y.z"; exit 1; }

# work folder (relative: the paths are passed to makensis.exe on Windows)
W=build-portableapps
rm -rf "$W"
mkdir -p "$W" "$OUT"
cp -R "tools/PortableApps package/D-Fend Reborn Portable" "$W/"
P="$W/D-Fend Reborn Portable"

sed -i -e "s/^PackageVersion=[0-9.]*/PackageVersion=$VERSION.0/" \
       -e "s/^DisplayVersion=[0-9.]*/DisplayVersion=$VERSION/" "$P/App/AppInfo/appinfo.ini"
mkdir -p "$P/App"
cp -R "$SRC" "$P/App/D-Fend Reborn"

# launcher "D-Fend Reborn Portable.exe", then the installer (packs App, Other
# and the launcher; an ANSI installer because of its plugins, the language
# files are UTF-8 and converted to the code page of each language)
"$MAKENSIS" -V2 "$P/Other/Launcher Source/PortableAppsLauncher.nsi"
"$MAKENSIS" -V2 -DVERSION="$VERSION.0" -DFILENAME="D-Fend-Reborn-$VERSION-Portable" \
    "$P/Other/Installer Source/PortableApps.comInstaller.nsi"

mv "$W/D-Fend-Reborn-$VERSION-Portable.paf.exe" "$OUT/"
rm -rf "$W"
