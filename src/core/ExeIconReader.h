// Reads the program icon stored in a Windows program file (.exe / .dll).
//
// D-Fend Reborn addition. On Windows the icons of program files are read via
// the shell (SHGetFileInfo / QFileIconProvider); on other systems the shell
// only knows a generic icon for .exe files, so the icon resources are read
// from the file itself: the first icon group (RT_GROUP_ICON) of the resource
// section of a PE file (32 and 64 bit programs; 16 bit NE programs are not
// supported), which is the icon Windows shows for the program.
#pragma once

#include <QIcon>
#include <QString>

// The program icon with all its sizes; a null icon if the file has no icon
// resources (or is no PE file).
QIcon ReadExeFileIcon(const QString& FileName);
