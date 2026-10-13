// Installation of data packages (several games in one archive file).
//
// D-Fend Reborn addition. The original program offered these packages
// ("ExePackage" entries of the package lists) as NSIS installers, which
// copied the profiles (Confs), game folders (VirtualHD), screenshots
// (Capture), icons (IconLibrary), ... into the data folder.
//
// Data packages are now archive files (.7z or .zip) with the same layout:
//
//   Confs/<Game>.prof
//   VirtualHD/<GameFolder>/...
//   Capture/<Game>/...
//   IconLibrary/...
//
// (the folders may also be inside one top folder of the archive). The files
// are extracted by 7-Zip and copied into the data folder; existing files are
// kept (profiles and saved games of games that are already installed are not
// overwritten). The original NSIS installers (.exe) can be installed the same
// way when the 7-Zip program can open NSIS installers (7z / 7zz on Linux).
#pragma once

#include <QString>

class QWidget;
class TGameDB;

// Is the file a data package that InstallDataPackage can install?
// (.7z / .zip on all systems; .exe on systems other than Windows, where the
// original installers cannot be run)
bool IsInstallableDataPackage(const QString& FileName);

// Installs the data package into the data folder and adds the new profiles
// to the games list. Returns the number of added profiles, -1 on errors
// (an error message has been shown then).
int InstallDataPackage(QWidget* Owner, const QString& FileName, TGameDB* GameDB);

// The folder of an extracted data package that holds the data folder layout
// (the folder itself or a sub folder containing "Confs"); empty if there is
// none. (separate for tests)
QString FindDataPackageRoot(const QString& Folder);

// Copies the files from SourceFolder into DestFolder, keeping existing files.
// NewProfiles receives the copied profile files (Confs/*.prof).
// Returns false if a file could not be copied.
bool CopyDataPackageFiles(const QString& SourceFolder, const QString& DestFolder, QStringList& NewProfiles, QString& FailedFile);
