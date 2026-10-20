// D-Fend Reborn: import of DOS games bought at GOG.com.
//
// GOG ships its DOS games with its own DOSBox and DOSBox conf files
// (dosbox<Game>.conf with the settings, dosbox<Game>_single.conf with the
// mount commands and the start of the game, dosbox<Game>_settings.conf for
// the setup program). The Windows versions describe how the game is started
// in goggame-<ID>.info (JSON, "playTasks"); the conf files are passed to
// DOSBox with "-conf" and their paths are relative to the working folder
// (usually the DOSBOX subfolder). The Linux versions have a "dosbox"
// subfolder and the conf files next to it (and a "gameinfo" file).
//
// The import turns this into a normal profile: the settings of the conf
// files, the mounted folders and images as drives and the program file
// (and setup program) of the game. If the autoexec section does more than
// starting one program (e.g. a menu), it is kept and replaces the start of
// the game.
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

class TGame;
class TGameDB;

struct TGOGGame {
    QString Folder;           // game folder
    QString Name;             // name of the game
    QString ID;               // GOG game ID (goggame-<ID>.info)
    QString WorkingDir;       // folder the paths in the conf files are relative to
    QStringList GameConfs;    // conf files for starting the game (in order)
    QStringList SetupConfs;   // conf files for the setup program
    QString Icon;             // goggame-<ID>.ico
};

// Is Folder the folder of a GOG DOS game? (Game: the data of the game)
bool ReadGOGGame(const QString& Folder, TGOGGame& Game);

// GOG DOS games in Folder: Folder itself or its subfolders (e.g. "GOG Games")
QList<TGOGGame> FindGOGGames(const QString& Folder);

// Folders GOG installs its games to on this computer (existing ones)
QStringList DefaultGOGFolders();

// Creates a profile for the game (nullptr: no conf file could be read)
TGame* ImportGOGGame(TGameDB* GameDB, const TGOGGame& Game);

// Is the game already in the games list (same program file or same name)?
bool GOGGameInList(TGameDB* GameDB, const TGOGGame& Game);

// Mount commands of an autoexec section as D-Fend Reborn drives (Mounts:
// "RealFolder;Type;Letter;..." as in the profiles, folders absolute; the
// paths in the commands are relative to WorkingDir or Folder); returns the
// remaining lines. (Used by ImportGOGGame, public for the tests.)
QStringList GOGMountsFromAutoexec(const QStringList& Autoexec, const QString& WorkingDir, const QString& Folder, QStringList& Mounts);

// The program started by an autoexec section (after the mounts were
// removed): Exe (absolute) and Parameters; Rest: the lines that still have
// to run before the program. Returns false if the autoexec does more than
// starting one program (e.g. a menu with "choice" and "goto").
bool GOGProgramFromAutoexec(const QStringList& Autoexec, const QStringList& Mounts, QString& Exe, QString& Parameters, QStringList& Rest);
