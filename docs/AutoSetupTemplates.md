# Auto setup templates

Auto setup templates are D-Fend Reborn profiles for known games
(`data/NewUserData/AutoSetup/*.prof`, installed into the folder `AutoSetup` of
the D-Fend Reborn data folder). When a profile is created for a program file,
D-Fend Reborn compares the program file with the templates. If one matches, the
profile gets the DOSBox settings and the game information (name, genre,
developer, publisher, year, ...) of the template. The templates are also the
offline source of the "Download game information" function in the profile
editor.

Improved templates and templates for games that are not covered yet are
welcome. This page describes the format, how to create a template, how to check
it and how to submit it.

[`AutoSetupTemplatesTodo.txt`](AutoSetupTemplatesTodo.txt) lists the existing
templates that need work: templates with missing game information, templates
sharing the same program file and templates sharing the same game name.

## Creating a template

1. Put the game into a subfolder of the folder `VirtualHD` of D-Fend Reborn
   (drive C: in DOSBox), e.g. `VirtualHD\KEEN4`.
2. Create a profile for it and make the game run well with the DOSBox included
   in D-Fend Reborn (DOSBox Staging). Fill in the game information in the
   profile editor ("Program information" page): name, genre, developer,
   publisher, year, language, license and, if known, the web site.
3. Export it: **File > Export > Create prof files...**, select the profile and
   check **Export profiles as auto setup templates**. The export stores the
   checksums D-Fend Reborn needs to recognise the game and removes personal
   settings (folders on your computer etc.).
4. Check the file (see below) and submit it.

Do not submit game files - only the `.prof` template.

## File format

A template is a D-Fend Reborn profile file (INI format), **UTF-8** encoded,
one game (version) per file. The file name is the game name, optionally with
the version: `Commander Keen 4.prof`, `Bolo Ball 2.0.prof`.

### Section `[Extra]` (program and drives)

| Key | Rules |
|---|---|
| `ProfileMode` | `DOSBox` (or `ScummVM` for ScummVM games) |
| `Exe` | Program file of the game, relative to the game folder (usually only the file name, e.g. `KEEN4E.EXE`) |
| `ExeMD5` | MD5 checksum of the program file, 32 hex digits, upper case (written by the export) |
| `0` ... `9` | Mounts. Folders must be inside `.\VirtualHD` (e.g. `.\VirtualHD\;Drive;C;false;`) - never folders of your own computer |
| `BootImage` | For templates that boot a disk image instead of starting a program (no `Exe` needed; these are only used via "Add from template") |

If several versions of a game use the same program file, add up to five
further files with their checksums in the section `[ExtraGameIdentify]`
(`File1.Filename`, `File1.Checksum`, ...) so that D-Fend Reborn can tell them
apart (the export adds them).

The other sections (`[sdl]`, `[cpu]`, `[sblaster]`, ...) are the DOSBox
settings of the profile as written by D-Fend Reborn.

### Section `[ExtraInfo]` (game information)

| Key | Rules | Example |
|---|---|---|
| `Name` | Required. The name of the game (without version) | `Commander Keen 4: Secret of the Oracle` |
| `Genre` | One or more standard genres (list below), separated by `;` | `Plattform` |
| `Developer` | Developer(s), several separated by `, ` | `id Software` |
| `Publisher` | Publisher(s), several separated by `, ` | `Apogee Software` |
| `Year` | Year of the first release, 4 digits | `1991` |
| `Language` | One or more standard languages (list below), separated by `;` | `English` |
| `WWW` | Web site of the game (official site, Wikipedia or Wikidata page, ...) | `https://www.wikidata.org/wiki/Q1123446` |
| `Notes` | Hints for playing / starting the game. Line breaks are written as `[13][10]` | `Choose 1 for Sound Blaster.[13][10]` |
| `UserInfo` | Further values, each followed by `[13][10]`: `License=` (standard license, list below), optionally `Version=` | `License=Shareware[13][10]Version=1.4[13][10]` |
| `Icon`, `Favorite` | Leave empty / `0` | |

### Standard values

The standard values are defined in [`data/Bin/GameInfo.ini`](../data/Bin/GameInfo.ini)
(D-Fend Reborn translates them in the program language). Other spellings
listed there under `*Aliases` are converted automatically by
`tools/autosetup.py fix`.

* **Genres (games):** Action, Adventure, Arcade, Board game, Cardgame, Demo,
  Economic simulation, Educational, First Person, Flightsimulation, Flipper,
  Lemmings, Plattform, Puzzle, Race, RPG, Simulation, Sports, Strategy
* **Genres (programs):** Program, DOS Graphical user interface, File Manager,
  Music Player, Office program, Operating system, Paint program,
  Programming language
* **Languages:** English, German, French, Italian, Spanish, Dutch, Danish,
  Polish, Russian, Chinese, Multilingual
* **Licenses:** Freeware, Shareware, Commercial, Open Source, Public Domain,
  Creative Commons, Demo

(Some names are kept from D-Fend Reloaded for compatibility with the
translations, e.g. "Plattform" for platform games and "Flightsimulation".)

## Checking a template

```
python3 tools/autosetup.py check "data/NewUserData/AutoSetup/My Game.prof"
python3 tools/autosetup.py fix   "data/NewUserData/AutoSetup/My Game.prof"
```

`check` reports errors (the template can not be accepted) and warnings
(missing information). `fix` converts the file to UTF-8, the standard
spellings, a 4 digit year and relative mounts, then checks it again.

To find or verify the game information, `lookup` searches Wikidata (free,
open data) and prints the values for `[ExtraInfo]`:

```
python3 tools/autosetup.py lookup "Commander Keen" --dos
```

Check the values - Wikidata is not always complete or exact for DOS games.

After changing templates, update the to-do list:

```
python3 tools/autosetup.py report
```

## Submitting

* **Pull request:** add or change the file in `data/NewUserData/AutoSetup/`.
  The "Build" workflow also builds the conf files of all templates for DOSBox
  Staging and DOSBox-X (`ctest`, see README.md).
  The "AutoSetup templates" check runs `tools/autosetup.py check` on all
  templates. Describe what you changed and which DOSBox version you tested
  with.
* **Issue:** open a new issue with the form "Auto setup template" and attach
  the `.prof` file (zipped, GitHub does not accept `.prof` attachments).

Templates are distributed with D-Fend Reborn under the GNU GPL.
