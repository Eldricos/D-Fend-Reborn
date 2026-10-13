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
files), `Lang` (program translations and help files), `IconSets` and
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

Without a `DFend.dat` file in the program folder D-Fend Reborn keeps all
data next to the program. With `USERDIRMODE` in `DFend.dat` the user data
is stored in `%USERPROFILE%\D-Fend Reborn\` on Windows and in
`~/.local/share/D-Fend Reborn/` on Linux.

The program update check asks GitHub for the latest release of this
repository (Help > Search for updates) and offers to open its download page.

Other folders taken over from the original sources: `Help` (HTML sources of
the `.chm` help files), `Install` (original NSIS installer scripts; their
paths still point to the old layout) and `tools` (sources of the NSIS helper
programs, server side data of the package/data reader/cheat services,
PortableApps.com package, source of `mkdosfs.exe`).
