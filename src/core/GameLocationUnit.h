// D-Fend Reborn: games whose files are not where the profile expects them
// any more (e.g. a GOG or Steam game that was uninstalled, or a game folder /
// Steam library moved to another drive).
#pragma once

#include <QString>
#include <QStringList>

class TGame;

// Files and folders the game needs (absolute): ScummVM: the game folder;
// otherwise the program file, the setup program and the mounted folders and
// image files (not physical CD drives)
QStringList GamePaths(TGame* Game);

// The paths of GamePaths that do not exist
QStringList MissingGamePaths(TGame* Game);

// Mounted folders and image files that do not exist: "<drive letter>;<path>"
QStringList MissingDrivePaths(TGame* Game);

// Searches the game below Folder (the folder the game or its library was
// moved to: the game folder itself, a folder containing it, a Steam library)
// and changes all paths of the profile from the old to the new location.
// Returns false (profile unchanged) if the files were not found.
bool RelocateGame(TGame* Game, const QString& Folder);
