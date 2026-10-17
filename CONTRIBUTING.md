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
