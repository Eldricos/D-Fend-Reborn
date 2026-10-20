// D-Fend Reborn: "Find missing games" dialog (Extras menu). Lists the games
// whose program files or folders do not exist any more (GameLocationUnit):
// moved games can be searched in a new folder, the profiles of uninstalled
// games deleted. Built in code (no .ui file).
#pragma once

class QWidget;
class TGameDB;

// Shows the dialog (or a message if no game is missing); true if profiles
// were changed or deleted
bool ShowMissingGamesDialog(QWidget* AOwner, TGameDB* GameDB);
