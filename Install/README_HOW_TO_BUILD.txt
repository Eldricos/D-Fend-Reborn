How to build the D-Fend Reborn installers (Windows)
====================================================

Requirements: Qt 6 for MSVC (with Qt Multimedia), CMake, NSIS 3 and 7-Zip.
(The scripts have no parentheses in their names on purpose: names like
"Build(x64).bat" break the "if (...)" blocks of batch files.)

1. Build D-Fend Reborn as a release build (Qt Creator: build configuration
   "Release", or: cmake -S .. -B ..\build-release -DCMAKE_BUILD_TYPE=Release
   and cmake --build ..\build-release).

2. Set the version information in VersionSettings.nsi (the same version as in
   ..\CMakeLists.txt, project(... VERSION ...)).
3. Set the version information in
   ..\tools\PortableApps package\D-Fend Reborn Portable\App\AppInfo\appinfo.ini and
   ..\tools\PortableApps package\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstallerConfig.nsh.

4. Run BuildInstallers.bat. Either copy this Install folder into the build
   folder and double-click BuildInstallers.bat there, or give the build folder
   as parameter, e.g.  BuildInstallers.bat "..\build-release"
   The script finds CMake (the one that configured the build folder, e.g. the
   CMake of Qt Creator) and NSIS itself. It creates the program folder (Stage:
   "cmake --install", which also copies the Qt libraries with windeployqt)
   and from it D-Fend-Reborn-<version>-Setup.exe and
   D-Fend-Reborn-<version>-UpdateSetup.exe.

5. Use D-Fend-Reborn-<version>-Setup.exe to create a portable installation in its
   default folder (Desktop\D-Fend Reborn). (Do not run the installation!)
6. Run BuildZips.bat: zip packages (full and mini) and the PortableApps.com
   package.

7. Rename the new files in this folder to have the right version information in
   their names.

The installers are Unicode installers. The language files (Languages\*.nsi) are
UTF-8 files; Languages\ioFile*.ini (the custom installer pages) are UTF-16 files.
The installers also compile with makensis on Linux (use a UTF-8 locale, e.g.
LC_ALL=C.UTF-8, and makensis -DSTAGE=<program folder>).


Releases (GitHub)
=================

The "Release" workflow (.github/workflows/release.yml) builds all downloads
of a version on GitHub and creates a draft release with them:

  D-Fend-Reborn-<version>-Setup.exe             Windows installer
  D-Fend-Reborn-<version>-UpdateSetup.exe       Windows update installer
  D-Fend-Reborn-<version>-Windows-Portable.zip  Windows portable version
  D-Fend-Reborn-<version>-x86_64.AppImage       Linux (Qt included)
  D-Fend-Reborn-<version>-Linux-x86_64.tar.gz   Linux (needs Qt 6.4+ of the distribution)

1. Set the new version in ..\CMakeLists.txt (project(DFendReborn VERSION x.y.z))
   and in VersionSettings.nsi (VER_MAYOR, VER_MINOR1, VER_MINOR2). The program
   takes its version from CMakeLists.txt.
2. Merge the changes into main.
3. Create the tag "vx.y.z" on the merged main branch and push it:
     git checkout main
     git pull
     git tag v1.5.0
     git push origin v1.5.0
   (GitHub Desktop: History > right-click the merge commit > Create Tag...,
   then Push origin.) Do not create a release for the tag by hand - the
   workflow creates it.
4. The workflow checks that the tag and both version numbers are the same
   (it stops with an error otherwise), builds everything and creates the
   draft release "D-Fend Reborn x.y.z" (about 15-20 minutes; see the
   Actions tab).
5. Check the draft on the Releases page (files, release notes) and click
   "Publish release". Only published releases are found by the update check
   of the program.

To redo a release: delete the draft release and the tag on GitHub, fix the
problem and push the tag again. The PortableApps.com package is not built
by the workflow (steps 5 and 6 above).
