# Contributing to D-Fend Reborn

* **Bugs and ideas:** open an issue on GitHub.
* **Auto setup templates** (game profiles with game information, for new
  games or improved settings / corrected information): see
  [docs/AutoSetupTemplates.md](docs/AutoSetupTemplates.md) - create the
  template with *File > Export > Create prof files...*, check it with
  `tools/autosetup.py` and submit it as a pull request or with the issue form
  "Auto setup template". Templates that need work (missing information,
  duplicates) are listed in
  [docs/AutoSetupTemplatesTodo.txt](docs/AutoSetupTemplatesTodo.txt).
* **Cheats** (for the cheating tool, *Extras > Cheating*): open an issue
  with the game, the save file and the change, or a pull request for
  [online/Cheats/Cheats.xml](online/Cheats/Cheats.xml). D-Fend Reborn
  downloads this file in *Help > Search for updates*; raise its `Version`
  number when adding records so that it is installed.
* **Translations:** the language files are in `data/Lang` (see
  `data/Lang/Readme_OperationMode.txt` and the help page on translations).
* **Code:** C++17 / Qt 6, see README.md for building. Pull requests are
  checked by the "Build" workflow (Linux and Windows).
* **Texts of the program** (language strings): all strings are defined in
  [src/core/LanguageStrings.inc](src/core/LanguageStrings.inc), one line each
  (number, accessor, section and key in the language files, English text).
  Add a new one with
  `python3 tools/langstrings.py add <Section> <Key> <Name> "English" "German"`;
  it writes the line and the texts in `data/Lang/English.ini` and
  `German.ini`, the program uses it as `LanguageSetup-><Name>()`.
  `python3 tools/langstrings.py check` (also run by the "Build" workflow)
  checks that the table and the two language files agree.
* **Commands of the main window** (menus, tool bar, popup menus): each menu
  item has a command number (its `tag`, 1xxx File, 2xxx View, 3xxx Run,
  4xxx Profile, 5xxx Extras, 6xxx Help); the commands are carried out in
  [src/forms/MainUnitMenuWork.cpp](src/forms/MainUnitMenuWork.cpp), one
  function per menu. The `menuCommands` test of `DFRDialogTests` runs every
  command once; with the environment variable `DFR_DEBUG` set it prints
  what each command did, to compare before and after a change.
