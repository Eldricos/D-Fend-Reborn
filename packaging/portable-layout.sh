#!/bin/sh
# Arranges a program folder created by "cmake --install" like an installation
# in program folder / portable mode (as the setup does it, see
# Install/D-Fend-Reborn-Setup.nsi): the contents of NewUserData become the
# data folders next to the program (Templates, AutoSetup, IconLibrary,
# Capture, Settings, VirtualHD\FREEDOS, VirtualHD\DOSZIP) and the
# NewUserData folder is removed. On Windows the DOSBox language files and the
# FreeDOS code page files are copied into the DOSBox folder.
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
cp "$N/Icons.ini" "$N/DataReader.xml" "$D/Settings/"
if [ -f "$D/Bin/Cheats.xml" ]; then cp "$D/Bin/Cheats.xml" "$D/Settings/"; fi
cp -R "$N/FREEDOS" "$D/VirtualHD/FREEDOS"
cp -R "$N/DOSZIP" "$D/VirtualHD/DOSZIP"

# Windows: included DOSBox
if [ -d "$D/DOSBox" ]; then
    cp "$N/FREEDOS/CPI/"* "$D/DOSBox/"
    if [ -d "$D/DOSBoxLang" ]; then
        cp -R "$D/DOSBoxLang/." "$D/DOSBox/"
        rm -rf "$D/DOSBoxLang"
    fi
fi

rm -rf "$N"
