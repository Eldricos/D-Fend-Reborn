# Game packages for the package manager

The package manager (File > Import > Download packages) lists the games of
its package lists on the "Games" page. Each game is a zip package of its own:
the user ticks the games they want and only those are downloaded and
installed. More games can be added to a list at any time.

D-Fend Reborn's game packages are the assets of the `game-packages` release
of this repository, and the release's `GamePackages.xml` is the package list
for them. To show them in the package manager, add the address of the list in
the package manager's repositories list:

    https://github.com/Eldricos/D-Fend-Reborn/releases/download/game-packages/GamePackages.xml

## Adding a game

1. Set up the game in D-Fend Reborn and check that it runs.
2. File > Export > Build zip packages: build the zip package of the game.
3. Upload the zip file to the `game-packages` release (edit the release on
   GitHub and drop the file on it).
4. Actions > Game packages > Run workflow: the package list is written again
   for all zip packages of the release.

To replace a game, upload the new zip file with the same name; to remove one,
delete it from the release; then run the workflow again.

## The games of the original program

The download list of D-Fend Reloaded offered its freeware and shareware games
in seven packages (NSIS installers with several games each). Running the
"Game packages" workflow with "Import the games of the original D-Fend
Reloaded packages" checked downloads these installers
(`tools/PackageServer/FetchOriginalPackages.py`), splits them into one zip
package per game and uploads them to the release.

## DFRPackageTool

The workflow uses `DFRPackageTool` (built with the program, from
`tools/PackageTool/main.cpp`). It runs the program code without windows in a
temporary data folder, so the packages have exactly the format the program's
own export creates:

    DFRPackageTool split --out DIR PACKAGE...
        Installs the data packages (NSIS installers or .7z/.zip archives with
        Confs/, VirtualHD/, ...) and builds one zip package per game in DIR.

    DFRPackageTool list --out LIST.xml --url-base URL [--name NAME] ZIP...
        Writes a package list with one "Game" entry per zip package (name,
        genre, developer, ... from the profile, checksums and size).

On Linux it needs the `7z` program (package `7zip` or `p7zip-full`).
