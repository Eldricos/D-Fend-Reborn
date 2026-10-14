How to build the D-Fend Reborn installers (Windows)
====================================================

Requirements: Qt 6 for MSVC (with Qt Multimedia), CMake, NSIS 3 and 7-Zip.

1. Build D-Fend Reborn as a release build (Qt Creator: build configuration
   "Release", or: cmake -S .. -B ..\build-release -DCMAKE_BUILD_TYPE=Release
   and cmake --build ..\build-release).

2. Set the version information in VersionSettings.nsi (the same version as in
   ..\CMakeLists.txt, project(... VERSION ...)).
3. Set the version information in
   ..\tools\PortableApps package\D-Fend Reborn Portable\App\AppInfo\appinfo.ini and
   ..\tools\PortableApps package\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstallerConfig.nsh.

4. Run BuildInstallers(x64).bat or BuildInstallers(x86).bat (depending on the
   Windows version, for the NSIS folder) with the build folder as parameter,
   e.g.  BuildInstallers(x64).bat "..\build-release"
   This creates the program folder (Stage: "cmake --install", which also copies
   the Qt libraries with windeployqt) and from it
   D-Fend-Reborn-<version>-Setup.exe and D-Fend-Reborn-<version>-UpdateSetup.exe.

5. Use D-Fend-Reborn-<version>-Setup.exe to create a portable installation in its
   default folder (Desktop\D-Fend Reborn). (Do not run the installation!)
6. Run BuildZips(x86).bat or BuildZips(x64).bat: zip packages (full and mini)
   and the PortableApps.com package.

7. Rename the new files in this folder to have the right version information in
   their names.

The installers are Unicode installers. The language files (Languages\*.nsi) are
UTF-8 files; Languages\ioFile*.ini (the custom installer pages) are UTF-16 files.
The installers also compile with makensis on Linux (use a UTF-8 locale, e.g.
LC_ALL=C.UTF-8, and makensis -DSTAGE=<program folder>).
