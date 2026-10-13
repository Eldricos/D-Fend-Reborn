// Physical drives of the host system (CD/DVD, floppy and removable drives).
//
// D-Fend Reborn addition. The original program worked with Windows drive
// letters only. On Windows the drives are still identified by their letters;
// on Linux/BSD the drives are found via the kernel's block device list
// (/sys/block) and the mounted file systems:
//
//  - the files of a medium are reached via its mount point ("Root"),
//  - raw access (reading an ISO / floppy image) uses the device file,
//  - the dialogs that work with drive letters get a letter for each floppy
//    (A, B) and CD/DVD drive (D, E, ...) - PhysicalDriveByLetter maps it back.
#pragma once

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

enum TPhysicalDriveType { pdtOptical, pdtFloppy, pdtRemovable };

struct TPhysicalDrive {
    TPhysicalDriveType Type = pdtOptical;
    QChar Letter;       // Windows drive letter / letter assigned on other systems (none for removable media)
    QString Root;       // folder with the files of the medium ("D:\", "/media/user/LABEL/"), empty if no medium is mounted
    QString Device;     // device file on Linux/BSD ("/dev/sr0"), empty on Windows
    QString Label;      // volume label of the medium (if known)
};

// Floppy and CD/DVD drives (with or without medium) and mounted removable media.
QList<TPhysicalDrive> PhysicalDrives();
QList<TPhysicalDrive> PhysicalDrives(TPhysicalDriveType Type);

// Root folders of the CD/DVD drives that can be mounted in DOSBox (on
// Linux/BSD: drives with a mounted medium; on Windows: all CD drives, like
// the original program).
QStringList CDDriveRoots();

// Drive with the given (assigned) letter; false if there is none.
bool PhysicalDriveByLetter(QChar Letter, TPhysicalDrive& Drive);

// Normalized root folder for comparisons ("d:\" -> "D:\", "/media/x" -> "/media/x/")
QString NormalizedDriveRoot(const QString& Root);

#ifndef Q_OS_WIN
// Linux: classification of a block device from /sys/block.
// BlockName: "sr0", "fd0", "sdb", ...; returns false for non removable devices.
bool LinuxBlockDeviceType(const QString& SysBlockDir, const QString& BlockName, TPhysicalDriveType& Type);

// Linux: the drive list built from a /sys/block style folder and a list of
// mounted file systems (PhysicalDrives() uses /sys/block and the real mounts;
// separate for tests).
struct TMountedVolume { QString Device; QString Root; QString Label; };
QList<TPhysicalDrive> PhysicalDrivesFrom(const QString& SysBlockDir, const QList<TMountedVolume>& Mounts);
#endif
