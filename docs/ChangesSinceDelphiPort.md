# D-Fend Reborn – all changes since the Delphi conversion

D-Fend Reborn started as a conversion of **D-Fend Reloaded 1.4.4** (Delphi, Windows only, by Alexander Herzog) to **C++ and Qt 6**. This list covers everything changed since that conversion, up to **version 0.5.10** (10/24/2026). It is based on the change log of each release and on the commit history of the repository. Many fixes and internal changes in the commits never made it into the change log, so they are listed here too.

The version in brackets is the release that first contained the change.

Legend: **Added** = new feature · **Changed** = different behaviour or look · **Fixed** = bug fix · **Removed** = taken out

---

## 1. The port itself (0.5.0)

- **Changed** – All units, forms and frames of D-Fend Reloaded 1.4.4 translated from Delphi to C++/Qt 6. The program runs on **Windows 10/11 (64 bit)** and **Linux**.
- **Added** – Compatibility layer for the Delphi runtime and VCL (`src/compat`): SysUtils, TStringList, TMemIniFile, MessageDlg, TListView, VCL Align/Anchors layout engine, forms with ModalResult, process helpers.
- **Added** – Converter `tools/dfm2ui.py`: all 173 Delphi forms converted to Qt Designer `.ui` files (positions, captions, glyphs, image lists, menus, toolbars).
- **Added** – Generated C++ headers from the Pascal interfaces (`tools/pas2h.py`) and generated default English language strings.
- **Changed** – Uses the profiles, settings, templates and language files of D-Fend Reloaded 1.4.4 unchanged. A second "compatibility version" (1.4.4) keeps package lists, language files and settings upgrades working.
- **Changed** – CMake project (Qt Creator). The program code is built once as the `DFendCore` library and shared by the program, the package tool and the tests.
- **Fixed** – Windows build problems: `GetTickCount` and Win32 A/W macro clashes, `VK_*` constant clashes, unchecked `QFile::open` results.
- **Changed** – Language file and settings lookups use hash indexes (about 7× faster), so OK in the Program Options is no longer slow.
- **Added** – Diagnostic output: the environment variable `DFEND_TRACE=1` prints timed trace messages and reports a blocked event loop.
- **Fixed** – OK in the Program Options took about 12 s because an empty DOSBox path made it scan the root of the current drive.
- **Fixed** – The list views showed Qt model reset warnings.
- **Fixed** – Toolbar separators were shown as blank buttons, and the main window's search box was placed one slot too early.
- **Fixed** – Splitters (main window tree, options, profile editor, package manager, quick start, file viewer) had width 0 and could not be dragged.
- **Changed** – The search of all drives for DOSBox runs only once, not at every start; the quick checks still run at every start.
- **Fixed** – The modern profile editor's tree, tools menu, drive list and game info lists had no icons.
- **Added** – The value list editor supports read-only cells and edit masks; the classic profile editor uses them.
- **Fixed** – Games tree: the icons of the chosen icon set were only shown on the first start; now they are shown on every start.
- **Fixed** – Colour boxes (games list, tree, screenshot list appearance) turned system colours such as "window text" into fixed RGB values; colour names now match the original.
- **Changed** – Image scaling (Program Options > Image scaling) uses the original Box, Triangle, Hermite, Bell, B-Spline, Lanczos3 and Mitchell filters instead of Qt's bilinear scaling; transparent images are scaled correctly.
- **Changed** – Search animation (wait dialog for internet data): a magnifying glass searching a folder, as in the original.

## 2. Name, branding and project home (0.5.0 – 0.5.1)

- **Changed** – Renamed to **D-Fend Reborn** everywhere: texts, all 16 language files, window titles, generated files, registry keys, autostart entry, installer, help, code identifiers.
  - Language file keys and the file names `DFend.exe`, `DFend.ini` and `DFend.dat` were kept, so existing translations and data still work.
- **Changed** – The homepage, update check and "Forum" menu point to the GitHub project. The forum entry is now "GitHub issues".
- **Changed** – About dialog: "based on D-Fend Reloaded by Alexander Herzog", the history D-Fend → D-Fend Reloaded → D-Fend Reborn, and the D-Fend Reborn change log followed by the original one.
- **Changed** – First run wizard and About dialog pictures show "REBORN"; the logo letters were built from the original pixel font.
- **Fixed** – The change log was not shown on Linux (wrong file name case). (0.5.1)
- **Changed** – Two version numbers: the D-Fend Reborn version (0.5.x) for releases and updates, and the compatibility version 1.4.4 for data.

## 3. Linux support (0.5.0, extended later)

- **Added** – Native Linux version.
  - User data is in `~/.local/share/D-Fend Reborn`.
  - Special folders follow XDG (documents, start menu, autostart); the Windows folders map to the Wine prefix.
- **Added** – Uses the system programs: DOSBox (or DOSBox Staging), ScummVM, 7-Zip, `mkfs.fat` (disk images with the same layout as `mkdosfs.exe`), `oggenc`, `lame` and `curl`. A clear message is shown if `curl` is missing for FTP downloads.
- **Added** – Finding DOSBox on Linux: the search path and the usual install folders; the full search starts in the home folder and skips system folders.
- **Added** – Windows programs of Windows profiles are started with **Wine**. The command is configurable, e.g. `flatpak run org.winehq.Wine`.
- **Added** – Program icons are read from Windows `.exe` files.
- **Added** – Physical drives on Linux:
  - CD/DVD, floppy and USB drives are found automatically from `/sys/block` and the mount points;
  - raw device access and mounting of inserted media go through **udisks2**;
  - DOSBox CD mounting (ask / number / label / file / folder / automatic), installation support and disk image reading/writing all work.
- **Added** – Wallpaper on GNOME/Unity/Budgie, KDE Plasma, Xfce, Cinnamon, MATE and LXQt.
- **Added** – The screensaver is inhibited over D-Bus while DOSBox or ScummVM runs.
- **Added** – DOSBox/ScummVM window centering and position via SDL environment variables.
- **Fixed** – DOSBox did not start on Linux: the Windows-only `SDL_VIDEODRIVER=directx` and `-noconsole` options were passed.
- **Fixed** – Folders with non-ASCII names were not found. Conf files now use UTF-8 for host paths, and the DOS code page (437/850) for the other autoexec lines.
- **Added** – Shortcuts are created as `.desktop` files.
- **Changed** – Texts say "file manager" instead of "Explorer" on Linux.
- **Changed** – "Minimize to tray" only hides the window when the desktop has a tray.
- **Changed** – "Start with Windows" is called "Start when logging in" on Linux. (0.5.5)
- **Added** – Quick start window: the whole folder tree is watched for changes (at most 2000 folders).
- **Fixed** – Package manager opened in about 20 s on Linux (case-sensitive file lookups); now a few seconds.
- **Fixed** – File names from NSIS packages were not valid UTF-8 on Linux and could not be copied or deleted; they are converted now.
- **Added** – AppImage runs natively on Wayland desktops. (0.5.4)
- **Added** – AppImage and tar.gz include **DOSBox Staging**; a DOSBox installed on the system is no longer needed. (0.5.7)
- **Fixed** – ScummVM games did not start when "Hide ScummVM console" was set: `--no-console` only exists in the Windows version of ScummVM. (0.5.9)

## 4. DOSBox: DOSBox Staging and DOSBox-X (0.5.4 – 0.5.8)

- **Changed** – **DOSBox Staging 0.83.0** replaces DOSBox 0.74 as the included DOSBox (Windows from 0.5.4, Linux from 0.5.7).
  - The update installer replaces the 0.74 files; portable copies use Staging's portable mode.
  - Its source code is attached to each release.
- **Added** – DOSBox Staging and DOSBox-X installations are recognised and started with the right parameters:
  - DOSBox Staging: no local `dosbox.conf`, its own translations, no SDL 1.2 video driver setting;
  - DOSBox-X: no `-noconsole`.
- **Added** – New setting "DOSBox program file" for installations, for folders that contain several DOSBox versions.
- **Fixed** – Games did not start with DOSBox Staging or DOSBox-X: the mount commands ended with `\"`, which these versions read as an escaped quote. (0.5.4)
- **Changed** – Conf files for DOSBox Staging use Staging's setting names (e.g. `cpu_cycles`, `mouse_capture`, `keyboard_layout`). Settings it no longer has are left out, so it shows no warnings, and the key mapper file is kept separate (`mapper-sdl2.map`). (0.5.5)
- **Changed** – Conf files for DOSBox-X write `vmemsize` and `realnic` in the sections it expects. The DOSBox-X notice was removed. (0.5.5)
- **Added** – Profile editors (modern and classic) grey out settings the profile's DOSBox does not use, with a tooltip; the values stay in the profile. (0.5.5)
- **Added** – New profile page/tab **"DOSBox Staging"** with 16 Staging-only settings (0.5.5):
  - graphics: shader, integer scaling, viewport size, CGA composite, 3dfx Voodoo;
  - CPU: protected mode cycles, CPU throttling;
  - sound: reverb, chorus, crossfeed, parallel port sound device, IBM PS/1 Audio;
  - MIDI: FluidSynth SoundFont, MT-32 model and ROM folder;
  - mouse capture.
- **Added** – Default value lists (Program Options) include the values of DOSBox Staging and DOSBox-X. Existing lists get the new values; values the user added or removed stay as they are. (0.5.5)
- **Added** – Edit multiple profiles: new section "DOSBox Staging". (0.5.5)
- **Added** – Choosers for the shader, SoundFont and MT-32 ROM folder settings. They list what the profile's DOSBox Staging can find: shaders with their presets, SoundFonts in Staging's folders and e.g. `/usr/share/sounds/sf2`, and folders with ROMs. (0.5.8)
- **Added** – The classic profile editor's value list can show a pick list and a "..." button together. (0.5.8)
- **Changed** – Help menu (DOSBox): getting started, keyboard shortcuts, manual and homepage of DOSBox Staging; the Program Options download link points to Staging. (0.5.6)
- **Removed** – The DOSBox 0.74 video codec installer (`InstallVideoCodec.exe`); videos are played with Qt Multimedia. (0.5.4)
- **Fixed** – Run | Open DOSBox configuration file with DOSBox-X opens the newest `dosbox-x*.conf` of the DOSBox-X user folder (`%LOCALAPPDATA%\DOSBox-X`, `~/.config/dosbox-x`) when there is none in the program folder, e.g. for a DOSBox-X installed by the Linux package manager. (0.5.10)
- **Removed** – Help | DOSBox | Demo compatibility list: the website is no longer maintained. (0.5.10)
- **Fixed** – ScummVM: Help menu links (manual, FAQ, compatibility list) and the download link in the program options lead to the current pages of the ScummVM website; a `README.md` in the ScummVM folder is shown as the manual. Without a ScummVM folder the program says so instead of "ScummVM does not exist at: /scummvm". (0.5.10)

## 5. Importing games (0.5.8 – 0.5.9)

- **Added** – **Import GOG games** (File | Import). (0.5.9)
  - Creates profiles for DOS games bought at GOG.com from GOG's own DOSBox conf files.
  - Reads `goggame-*.info` on Windows, and `dosbox*.conf` / `dosbox*_single.conf` next to a `dosbox` folder for Linux and older versions.
  - Imports settings, mounted folders, CD folders, CD/floppy images, the program file with its parameters, the setup program and the icon.
  - Paths are resolved from GOG's working folder, ignoring upper/lower case.
  - Start commands with a menu are kept and start the game.
  - Works on one game folder or a folder with many games; games already in the list are skipped; the game files are not copied.
- **Added** – Games GOG sells with **ScummVM** (e.g. Beneath a Steel Sky) get ScummVM profiles: game ID, data folder, language, platform and extra path come from GOG's ScummVM ini file. (0.5.9)
- **Added** – **Import Steam games** (File | Import). (0.5.9)
  - The same import for DOS games Steam sells with DOSBox.
  - Conf files can have any name, in the game folder or a subfolder.
  - Setup variants give the setup program; network variants are ignored.
  - Game names and IDs come from `appmanifest_*.acf`.
  - Steam libraries are found through `libraryfolders.vdf` and, on Windows, the registry.
- **Added** – ScummVM game detection with `scummvm --detect`: choosing a game folder in the wizard or the profile editor selects the game, and the wizard shows the game and version found. (0.5.8)
- **Added** – The GOG/Steam import understands CPU cycles written as `fixed 20000`. (0.5.9)

## 6. Managing the games list (0.5.9)

- **Added** – **Extras | Find missing games**: lists games whose program files, setup program, drive folders or disc images (ScummVM: game folder) no longer exist, e.g. uninstalled or moved GOG/Steam games.
  - "Search new location" finds moved games and changes every path of the profile.
  - "Delete profiles" deletes the profiles only, never game files.
  - Physical CD drives are not checked.
- **Changed** – "Check profiles for not existing files" also checks mounted folders and image files.

## 7. Look and layout (0.5.6 – 0.5.8)

- **Changed** – All dialogs use **Qt layouts** instead of fixed pixel positions (0.5.6–0.5.7): profile editors, Program Options, wizards, the multiple profiles editor, package manager, mount editor, update check, cheats, image tools and the rest.
  - With display scaling, larger fonts or long translations, texts and controls are no longer cut off, and windows grow when needed.
- **Fixed** – Buttons and labels in some dialogs cut off their text with large fonts (Info, link list editor, package creation). On Windows, Cancel was cut off. (0.5.7)
- **Fixed** – Search for updates: the status texts were cut off. (0.5.6)
- **Fixed** – The help buttons showed a wrong icon (a dark "S"). (0.5.7)
- **Added** – **Dark mode**: follows the light/dark setting of Windows or the Linux desktop, also when it changes while the program runs. Program Options > User interface > "Colors" can also force light or dark. (0.5.8)
  - Links, hints and warnings stay readable in dark mode.
  - On Windows the dark colours use the Fusion style, and the title bars change at once.
- **Fixed** – Some windows were much taller than needed, e.g. the wizard (871 → 601 px) and the Program Options. (0.5.8)

## 8. Game information (0.5.4 – 0.5.6)

- **Changed** – "Download game information" became **"Look up game information"** and uses the auto setup templates (offline) or **Wikidata** (online). The MobyGames and TheGamesDB pages it used to read had changed. (0.5.4)
- **Removed** – The MobyGames/TheGamesDB readers, their configuration (`DataReader.xml`) and its update option. (0.5.6)
- **Fixed** – D-Fend Reborn could crash when closed after "Look up game information" had been used. (0.5.7)
- **Changed** – Web search links ("Look up game at") and help menu links use current addresses and https. Dead sites (Squakenet, TheLegacy.de, Liberated games, 3D Realms downloads) were removed, also from lists the user changed. (0.5.6)

## 9. Auto setup templates (0.5.4)

- **Added** – Documented template format (`docs/AutoSetupTemplates.md`), `CONTRIBUTING.md` and an issue form for new or improved templates.
- **Added** – `tools/autosetup.py`: format check, automatic fixes, Wikidata lookup and a to-do report (`docs/AutoSetupTemplatesTodo.txt`). A GitHub workflow checks all templates.
- **Changed** – All 1651 included templates normalised: UTF-8, standard genre/language/license names, 4-digit years; a personal mount and wrong developer names fixed. Exported templates are written in this format.
- **Added** – `Bin/GameInfo.ini`: standard genres, languages and licenses with aliases, and the Wikidata genre mapping.

## 10. Package manager, online data and downloads (0.5.0 – 0.5.6)

- **Changed** – Package lists, cheats database and other online files come from the GitHub repository (`online/`) instead of the old homepage; XML DTD links point there too.
- **Added** – **Game packages**: one zip package per game (203 games from the original 7 packages). You tick the games you want and only those are downloaded; they are published as the "game-packages" release.
- **Added** – Data packages as `.7z`/`.zip` archives, installed by the program on all systems. On Linux the original NSIS packages are extracted with 7-Zip. Existing files are kept.
- **Added** – Package lists are downloaded automatically the first time the package manager is opened.
- **Fixed** – The automatic list update compared against the wrong date.
- **Fixed** – The repositories list failed on the unreachable old address.
- **Fixed** – The package list files had lost their CRLF line endings, so none matched their checksums; all 277 entries match now.
- **Fixed** – A newer cheats database was never installed: a Delphi `not` was evaluated bitwise.
- **Removed** – The old D-Fend Reloaded language add-on and template lists. (0.5.6)
- **Added** – All downloads use the **system proxy settings**; certificates named in `SSL_CERT_FILE` are trusted too.
- **Changed** – Update check uses the GitHub releases. (0.5.0)
- **Added** – Windows: built-in program update. It downloads the update installer of a new release, installs it and restarts D-Fend Reborn, for installed and portable copies. (0.5.2)
- **Removed** – The external `UpdateCheck.exe`, which virus scanners blocked. (0.5.2)

## 11. Help (0.5.0 and later)

- **Changed** – The help is a set of **HTML pages** opened in the web browser on all systems, instead of `.chm` files. Help buttons still open the page of their dialog.
- **Fixed** – Broken and wrongly cased links in the help pages, CHM-only boxes, typos and untranslated paragraphs.
- **Added** – Linux notes throughout the help (file manager, MIDI, Wine, drives, data folders, DOSBox/ScummVM options, shortcuts).
- **Removed** – An outdated keyboard FAQ entry.
- **Changed** – The number of auto setup templates in the help is current.
- **Changed** – External links use current addresses and https (e.g. the NSIS homepage moved).
- **Added** – Help pages for every new feature: DOSBox Staging page, game information lookup, GOG and Steam import, Find missing games, dark mode, ScummVM detection, Staging choosers.

## 12. Translations and language files (0.5.0)

- **Added** – The new texts are in all language files (Linux file manager, Wine option). Later features have English and German texts; the other languages fall back to English until translated.
- **Fixed** – The Czech, Polish, Turkish and Traditional Chinese language files declared the wrong character set and only displayed correctly on a matching Windows. The Chinese files failed to load completely.
- **Fixed** – On Windows, language files in other code pages are decoded even when Qt has no codec for them.
- **Fixed** – Dutch, Spanish, Portuguese-BR: the "Parameters" text of the extra programs dialog had a wrong key and was never used.
- **Fixed** – The installer language setting had no effect: it still used the D-Fend Reloaded registry key.
- **Fixed** – "Restore defaults" in the ScummVM options now sets "Hide ScummVM console" to its real default (on).
- **Fixed** – File | Import | Import XML file: the menu item and the file dialog texts can be translated; the file filter required the dot of `.xml`. (0.5.10)
- **Fixed** – The "could not delete folder" message used the key of "could not delete file" (since D-Fend Reloaded), so it said "file"; it has its own key now. (0.5.10)

## 13. Saving, robustness and Windows fixes

- **Changed** – Settings, profiles and lists are saved through a temporary file. If the program is stopped or the disk is full while saving, the old file stays intact. (0.5.7)
- **Fixed** – Split large file / downloads: a file that could not be created is reported instead of failing silently.
- **Fixed** – Windows: NSIS was never found, so the "create installer" menu items were always hidden; it is now found in the registry, Program Files and the search path.

## 14. Windows installers and downloads (0.5.0 – 0.5.4)

- **Changed** – Installers renamed to D-Fend Reborn in all 14 languages.
  - They install the Qt program instead of the Delphi files and add a start menu entry for the HTML help.
  - They are Unicode installers, so non-ASCII file names work on every Windows language.
  - The UAC plugin was replaced, and D-Fend Reborn is started unelevated after installing.
- **Removed** – Games Explorer registration (Vista/7 only), UPX packing, DelZip/7za/media player DLLs, `.chm` help.
- **Changed** – The default install folder is `C:\Program Files` (64 bit); existing installations keep their folder. (0.5.2)
- **Added** – The update installer updates portable copies in place. (0.5.2)
- **Changed** – The Qt plugins are in the `Qt` subfolder; unneeded DLLs are no longer shipped. (0.5.3)
- **Changed** – The portable zip and the Linux tar.gz have the data folders laid out like an installation. (0.5.3)
- **Added** – **PortableApps.com** package with each release. (0.5.4)
- **Added** – 7-Zip 24.09 (`7za.exe`) included for archives on Windows.
- **Fixed** – The installer build scripts failed because of parentheses in their names; they now find CMake, NSIS and 7-Zip themselves.
- **Changed** – Batch files are always stored with CRLF line endings.
- **Changed** – The list of used components lists Qt, DOSBox Staging, mkdosfs and NSIS; components that are no longer used were removed. (0.5.4)

## 15. Development, tests and releases

- **Added** – GitHub Actions build check for every push and pull request: Linux (GCC, Qt 6.4) and Windows (MSVC, Qt 6.8).
  - It checks the deployed data files and builds the Windows installers and the PortableApps package.
  - Workflows run on Node.js 24.
- **Added** – Release workflow: publishing a release builds the Windows installer, update installer, portable zip, PortableApps package, Linux AppImage (with a test start on X11 and Wayland) and tar.gz, and checks that the tag matches the version.
- **Added** – Automated tests (Qt Test + ctest, run in CI). (0.5.7)
  - Core tests: conf conversion, DOSBox detection, safe saving, ScummVM detection, profiles, all auto setup templates, Staging resources, GOG/Steam import, missing games.
  - Dialog tests: every dialog opened with normal and large fonts and in dark mode.
- **Added** – Weekly "DOSBox Staging update" workflow that opens an issue when a new Staging release is out (`tools/update-dosbox-staging.py`).
- **Added** – Other workflows: "Help links" (`tools/check_help_links.py`), "AutoSetup templates" and "Game packages".
- **Added** – `DFRPackageTool`: builds game packages and package lists without windows.
- **Changed** – All language strings are defined in one table, `src/core/LanguageStrings.inc` (number, accessor, section, key, English text); `tools/langstrings.py` adds new strings and checks the table against `English.ini` and `German.ini` (run by the build check). Before, each text needed edits in five places. (0.5.10)
- **Changed** – The commands of the main window are carried out in one function per menu (`src/forms/MainUnitMenuWork.cpp`) instead of one 730-line switch. (0.5.10)
- **Added** – Test of all main window commands: each menu, tool bar and popup menu command is run with a DOSBox, a ScummVM and a Windows test profile; a test hook stops programs, web pages and folders from being opened. (0.5.10)
- **Changed** – The original data files were sorted into `data/` and are copied next to the executable by every build. Delphi leftovers, unused DLLs and duplicated files were removed.

---

*Releases: 0.5.0 (10/14/2026) · 0.5.1 – 0.5.4 (10/15/2026) · 0.5.5 (10/16/2026) · 0.5.6 (10/18/2026) · 0.5.7 (10/19/2026) · 0.5.8 (10/20/2026) · 0.5.9 (10/20/2026) · 0.5.10 (10/24/2026)*
