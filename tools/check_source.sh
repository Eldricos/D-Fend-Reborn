#!/bin/bash
# Syntax checks C++ source files of the port (developer tool).
# Usage: tools/check_source.sh src/core/Foo.cpp [more.cpp ...]
# Run tools/check_headers.sh once before (creates the ui_*.h files and the
# precompiled header in $GEN).
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEN=${GEN:-/tmp/dfend-headercheck}
FLAGS="-std=c++17 -fPIC -fsyntax-only $(pkg-config --cflags Qt6Widgets Qt6Xml Qt6Network Qt6Multimedia Qt6MultimediaWidgets) -I$ROOT/src -I$ROOT/src/compat -I$ROOT/src/core -I$ROOT/src/forms -I$GEN -include $GEN/pch.h"
rc=0
for f in "$@"; do
  if g++ $FLAGS -x c++ "$f" -o /dev/null 2>"$GEN/$(basename "$f").log"; then
    echo "OK   $f"
  else
    echo "FAIL $f ($(grep -c 'error:' "$GEN/$(basename "$f").log") errors, see $GEN/$(basename "$f").log)"
    grep -m 15 'error:' "$GEN/$(basename "$f").log"
    rc=1
  fi
done
exit $rc
