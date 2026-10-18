#!/bin/sh
# Arranges a program folder created by "cmake --install" like an installation
# in program folder / portable mode (as the setup does it, see
# Install/D-Fend-Reborn-Setup.nsi): the contents of NewUserData become the
# data folders next to the program (Templates, AutoSetup, IconLibrary,
# Capture, Settings, VirtualHD\FREEDOS, VirtualHD\DOSZIP) and the
# NewUserData folder is removed. The included DOSBox Staging (Windows or
# Linux) is set to its portable mode.
#
# Usage: portable-layout.sh <program folder>
set -e
D="$1"
N="$D/NewUserData"
[ -d "$N" ] || { echo "no NewUserData folder in $D"; exit 1; }

mkdir -p "$D/Capture/DOSBox DOS" "$D/Templates" "$D/AutoSetup" "$D/IconLibrary" "$D/Settings" "$D/VirtualHD"
cp -R "$N/Capture/DOSBox DOS/." "$D/Capture/DOSBox DOS/"
cp -R "$N/Templates/." "$D/Templates/"
cp -R "$N/AutoSetup/." "$D/AutoSetup/"
cp -R "$N/IconLibrary/." "$D/IconLibrary/"
cp "$N/Icons.ini" "$D/Settings/"
if [ -f "$D/Bin/Cheats.xml" ]; then cp "$D/Bin/Cheats.xml" "$D/Settings/"; fi
cp -R "$N/FREEDOS" "$D/VirtualHD/FREEDOS"
cp -R "$N/DOSZIP" "$D/VirtualHD/DOSZIP"

# Included DOSBox Staging in portable mode (its settings, key mapper file
# and captures in the DOSBox folder instead of the user profile / home
# folder): DOSBox Staging uses the "portable layout" if dosbox-staging.conf
# is next to its program file
if [ -f "$D/DOSBox/dosbox.exe" ] && [ ! -f "$D/DOSBox/dosbox-staging.conf" ]; then
    printf '# DOSBox Staging settings (portable copy of D-Fend Reborn)\r\n' > "$D/DOSBox/dosbox-staging.conf"
fi
if [ -f "$D/DOSBox/dosbox" ] && [ ! -f "$D/DOSBox/dosbox-staging.conf" ]; then
    printf '# DOSBox Staging settings (D-Fend Reborn program folder)\n' > "$D/DOSBox/dosbox-staging.conf"
fi

rm -rf "$N"
