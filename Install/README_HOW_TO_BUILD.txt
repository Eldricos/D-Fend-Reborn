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
   (Only for BuildZips.bat: the Release workflow sets them itself, see
   ..\packaging\portableapps.sh.)

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
of a version on GitHub and attaches them to its release:

  D-Fend-Reborn-<version>-Setup.exe             Windows installer
  D-Fend-Reborn-<version>-UpdateSetup.exe       Windows update installer
  D-Fend-Reborn-<version>-Windows-Portable.zip  Windows portable version
  D-Fend-Reborn-<version>-Portable.paf.exe      PortableApps.com package
  D-Fend-Reborn-<version>-x86_64.AppImage       Linux (Qt included; X11 and Wayland)
  D-Fend-Reborn-<version>-Linux-x86_64.tar.gz   Linux (needs Qt 6.4+ of the distribution)

1. Set the new version in ..\CMakeLists.txt (project(DFendReborn VERSION x.y.z))
   and in VersionSettings.nsi (VER_MAYOR, VER_MINOR1, VER_MINOR2), and merge
   the changes into main. The program takes its version from CMakeLists.txt.
   (DFR_COMPAT_VERSION in CMakeLists.txt is the D-Fend Reloaded version whose
   data D-Fend Reborn uses - package lists and language files are checked
   against it. Do not change it for a release.)
2. Start the workflow - either way:
   a) On GitHub: Releases > Draft a new release > Choose a tag > type
      "vx.y.z" (e.g. v0.5.0; the tag must be the version number) > "Create
      new tag: vx.y.z on publish", target "main", enter title and text and
      click "Publish release". The workflow adds the files and a download
      section to this release when they are built (about 20 minutes, see the
      Actions tab). Until then the release has no files.
   b) With git:  git tag vx.y.z  and  git push origin vx.y.z
      The workflow creates a draft release "D-Fend Reborn x.y.z" with the
      files and generated release notes; check it and click "Publish release".
3. The workflow first checks that the tag and both version numbers are the
   same and stops with an error otherwise (nothing is built then).
   The tag is fixed to the commit it was created for: set the version numbers
   on main before creating the release. If they were changed afterwards,
   delete the release and the tag and create both again.

Only published releases that are not marked as "pre-release" are found by the
update check of the program.

To redo a release: delete the release and the tag on GitHub, fix the problem
and create the release (tag) again.

Test build without a release: Actions > Release > Run workflow (choose the
branch). It builds and tests all downloads of the version in CMakeLists.txt
and keeps them as workflow artifacts (7 days) instead of creating a release.
