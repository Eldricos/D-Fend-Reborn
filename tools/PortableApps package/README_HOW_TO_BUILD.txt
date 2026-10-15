1.) Update version information in .\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstallerConfig.nsh
    and in .\D-Fend Reborn Portable\App\AppInfo\appinfo.ini
2.) Copy a portable installation of DFR to the .\D-Fend Reborn Portable\App\D-Fend Reborn folder
3.) Run .\D-Fend Reborn Portable\Other\Launcher Source\PortableAppsLauncher.nsi
4.) Run .\D-Fend Reborn Portable\Other\Installer Source\PortableApps.comInstaller.nsi

(Or use the automated build scripts in ..\..\Install for steps 2 to 4 !)

The Release workflow builds the package with ..\..\packaging\portableapps.sh
(steps 1 to 4, version from CMakeLists.txt; also works on Linux with makensis).
The installer source files are UTF-8 files: the installer is an ANSI installer
(because of its plugins), NSIS converts the texts to the code page of each
language.