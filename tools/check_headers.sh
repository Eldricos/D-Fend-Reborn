#!/bin/bash
# Syntax checks every header of the port in isolation (developer tool).
# Usage: tools/check_headers.sh [header ...]
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEN=${GEN:-/tmp/dfend-headercheck}
mkdir -p "$GEN"
UIC=${UIC:-/usr/lib/qt6/libexec/uic}
for f in "$ROOT"/src/forms/*.ui; do
  b=$(basename "$f" .ui)
  [ "$GEN/ui_$b.h" -nt "$f" ] || { "$UIC" "$f" -o "$GEN/ui_$b.h.tmp.$$" && mv -f "$GEN/ui_$b.h.tmp.$$" "$GEN/ui_$b.h"; }
done
BASEFLAGS="-std=c++17 -fPIC $(pkg-config --cflags Qt6Widgets Qt6Xml Qt6Network) -I$ROOT/src/compat -I$ROOT/src/core -I$ROOT/src/forms -I$GEN"
# precompiled header with the Qt includes
PCHTXT=$'#include <QtWidgets>\n#include <QtXml>\n#include <QtNetwork>\n#include <functional>'
# (re)build the PCH under an exclusive lock so parallel runs never see a half written file
(
  flock -x 9
  if [ "$(cat "$GEN/pch.h" 2>/dev/null)" != "$PCHTXT" ]; then printf '%s\n' "$PCHTXT" > "$GEN/pch.h"; fi
  if [ ! -s "$GEN/pch.h.gch" ] || [ ! "$GEN/pch.h.gch" -nt "$GEN/pch.h" ]; then
    g++ $BASEFLAGS -x c++-header "$GEN/pch.h" -o "$GEN/pch.h.gch.tmp.$$" && mv -f "$GEN/pch.h.gch.tmp.$$" "$GEN/pch.h.gch"
  fi
) 9>"$GEN/pch.lock"
FLAGS="$BASEFLAGS -fsyntax-only -include $GEN/pch.h"
if [ $# -gt 0 ]; then HDRS="$@"; else HDRS=$(ls "$ROOT"/src/core/*.h "$ROOT"/src/forms/*.h); fi
check() { g++ $FLAGS -x c++ "$1" -o /dev/null 2>"$GEN/$(basename "$1").log" && rm -f "$GEN/$(basename "$1").log" || echo "FAIL $(basename "$1") ($(grep -c 'error:' "$GEN/$(basename "$1").log") errors)"; }
export -f check; export FLAGS GEN
printf '%s\n' $HDRS | xargs -P 8 -I{} bash -c 'check {}'
