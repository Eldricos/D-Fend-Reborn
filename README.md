# D-Fend Reborn

C++17 / Qt 6 port of **D-Fend Reloaded**, the DOSBox frontend, converted
directly from the original Delphi sources (`D-Fend-Reloaded-Base`).

* All 172 forms were converted from the Delphi `.dfm` files into Qt Designer
  `.ui` files (`src/forms`) with the original control positions, captions,
  icons and glyphs; a small VCL layout engine reproduces `Align`/`Anchors`
  behaviour so the dialogs look and resize like the originals.
* Every Delphi unit has a C++ counterpart with the same name
  (`src/core` for non visual units, `src/forms` for forms and frames).
* `src/compat` contains a thin Delphi RTL/VCL compatibility layer.

See [PORTING.md](PORTING.md) for the conventions used by the port.

## Building

Requirements: CMake ≥ 3.19, a C++17 compiler and Qt ≥ 6.4 with the Widgets,
Network, Xml and Multimedia modules.

```
cmake -S . -B build
cmake --build build
```

or open `CMakeLists.txt` in Qt Creator.

## Data files

`data/` holds the run-time files of the original program in the layout of an
installed copy: `Bin` (helper programs, links, cheats database, 7-Zip),
`DOSBox` (bundled Windows DOSBox 0.74), `DOSBoxLang` (DOSBox language
files), `Lang` (program translations), `Help` (the help pages), `IconSets` and
`NewUserData` (templates, AutoSetup profiles, FreeDOS, Doszip, ...).

Every build copies them next to the executable (`cmake/DeployData.cmake`),
so a build can be run directly:

* `Bin`, `Lang`, `IconSets`, `NewUserData` (and on Windows `DOSBox` plus the
  DOSBox language and FreeDOS code page files) are refreshed on each build.
  On Linux/macOS the Windows programs in `Bin` (`*.exe`, `*.dll`) are left
  out; the installed system programs are used instead (see below).
* The user data the original installer created (`Templates`, `AutoSetup`,
  `IconLibrary`, `Capture`, `VirtualHD`, `Settings/Icons.ini`,
  `Settings/DataReader.xml`, `Settings/Cheats.xml`) is copied only once per
  build folder. Delete `Settings/.DataDeployed` to copy it again.

On Linux the program uses these system programs (install them with the
package manager): `dosbox` (or `dosbox-staging`'s `dosbox`), `7z`/`7za`/`7zz`
(p7zip or 7zip, for zip/7z archives), `mkfs.fat` (dosfstools, for creating
disk images), `oggenc` (vorbis-tools), `curl` (FTP downloads in the package
manager) and optionally `lame` and `scummvm`.

Physical drives on Linux: CD/DVD, floppy and USB drives are found
automatically. Discs that the desktop did not mount are mounted via udisks2,
and reading / writing disk images from / to real media uses udisks2 when the
user has no direct permission for the device (not in the `floppy` / `disk`
group) - the desktop may ask for a password then. (udisks2 is installed on
all common desktops; it needs the Qt D-Bus module at build time.)

Without a `DFend.dat` file in the program folder D-Fend Reborn keeps all
data next to the program. With `USERDIRMODE` in `DFend.dat` the user data
is stored in `%USERPROFILE%\D-Fend Reborn\` on Windows and in
`~/.local/share/D-Fend Reborn/` on Linux.

All downloads use the proxy settings of the system (Windows: Internet
options; Linux: `http_proxy` / `https_proxy` / `no_proxy`). Certificates named
by `SSL_CERT_FILE` (e.g. of a company proxy) are trusted in addition to the
system certificates.

The program update check asks GitHub for the latest release of this
repository (Help > Search for updates) and offers to open its download page.

The help (`data/Help/<Language>/`) is a set of plain HTML pages, used on all
systems: the help buttons and menu items open the matching page in the
default web browser. (The original program compiled these pages into `.chm`
files.) The language files name their help folder (`HelpFile=English`).

The package manager installs single games from zip packages ("Games" page)
and data packages with several games (.7z / .zip archives with the data
folder layout, extracted by D-Fend Reborn itself; on Linux also the original
NSIS installers). D-Fend Reborn's own game packages are published as the
`game-packages` release, built by the "Game packages" workflow and the
`DFRPackageTool` command line tool - see `tools/PackageTool/README.md` for
how to add games. The online services of the program are the files in `online/` (same
layout as the old D-Fend Reloaded homepage), which the program downloads
from this repository: the package lists (`Packages`: main list, add-ons,
auto setup templates), the data reader configuration (`DataReader`), the
cheats database (`Cheats`) and the document type definitions of the XML
files.

Every push and pull request is built on Linux and Windows by GitHub Actions
(`.github/workflows/build.yml`). Pushing a version tag (`v1.5.0`) starts the
"Release" workflow (`.github/workflows/release.yml`): it builds the Windows
installers, a portable Windows zip, a Linux AppImage and a Linux tar.gz and
creates a draft GitHub release with them (see `Install/README_HOW_TO_BUILD.txt`).

`Install` holds the NSIS scripts of the Windows installers (full and update
installer); they package the program folder that `cmake --install` creates
(`Install/README_HOW_TO_BUILD.txt`). The Windows build check also builds the
installers and offers them as a download of the workflow run.

Other folders taken over from the original sources: `tools` (sources of the
NSIS helper programs, PortableApps.com package, source of `mkdosfs.exe`).
