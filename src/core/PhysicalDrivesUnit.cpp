// D-Fend Reborn addition - see PhysicalDrivesUnit.h
#include "PhysicalDrivesUnit.h"
#include "SysUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

#include <algorithm>

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

QString NormalizedDriveRoot(const QString& Root)
{
    QString S = Trim(Root);
    if (S.isEmpty()) return S;
#ifdef Q_OS_WIN
    if (S.length() == 1) S += ":";
    return IncludeTrailingPathDelimiter(S).toUpper();
#else
    return IncludeTrailingPathDelimiter(S);
#endif
}

#ifdef Q_OS_WIN

QList<TPhysicalDrive> PhysicalDrives()
{
    QList<TPhysicalDrive> Result;
    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        const UINT T = GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16()));
        TPhysicalDrive D;
        if (T == DRIVE_CDROM) D.Type = pdtOptical;
        else if (T == DRIVE_REMOVABLE) D.Type = (C == 'A' || C == 'B') ? pdtFloppy : pdtRemovable;
        else continue;
        D.Letter = QChar(C);
        D.Root = Root;
        wchar_t Name[512];
        DWORD C1 = 0, C2 = 0, C3 = 0;
        Name[0] = 0;
        if (GetVolumeInformationW(reinterpret_cast<LPCWSTR>(Root.utf16()), Name, 510, &C1, &C2, &C3, nullptr, 0)) D.Label = QString::fromWCharArray(Name);
        Result << D;
    }
    return Result;
}

QStringList CDDriveRoots()
{
    // all CD drives (with or without medium), like GetCDDrives of the original
    QStringList Result;
    for (const TPhysicalDrive& D : PhysicalDrives(pdtOptical)) Result << D.Root;
    return Result;
}

#else

static QString ReadSysFile(const QString& FileName)
{
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return QString();
    return QString::fromLatin1(F.readAll()).trimmed();
}

bool LinuxBlockDeviceType(const QString& SysBlockDir, const QString& BlockName, TPhysicalDriveType& Type)
{
    const QString Dir = IncludeTrailingPathDelimiter(SysBlockDir) + BlockName + "/";
    if (BlockName.startsWith("loop") || BlockName.startsWith("ram") || BlockName.startsWith("zram")) return false;
    // SCSI device type 5 = CD/DVD drive (USB, SATA, ...)
    if (BlockName.startsWith("sr") || ReadSysFile(Dir + "device/type") == "5") { Type = pdtOptical; return true; }
    if (BlockName.startsWith("fd")) { Type = pdtFloppy; return true; }
    if (ReadSysFile(Dir + "removable") != "1") return false;
    // USB floppy drives are removable block devices with up to 2.88 MB
    // ("size" is given in 512 byte sectors)
    bool OK;
    const qint64 Sectors = ReadSysFile(Dir + "size").toLongLong(&OK);
    Type = (OK && Sectors > 0 && Sectors <= 5760) ? pdtFloppy : pdtRemovable;
    return true;
}

QList<TPhysicalDrive> PhysicalDrivesFrom(const QString& SysBlockDir, const QList<TMountedVolume>& Volumes)
{
    QList<TPhysicalDrive> Result;

    // mounted file systems of a block device (and of its partitions)
    auto MountsOf = [&Volumes](const QString& BlockName, bool WithPartitions) {
        QList<TMountedVolume> R;
        const QString Dev = "/dev/" + BlockName;
        for (const TMountedVolume& V : Volumes) {
            if (V.Device == Dev || (WithPartitions && V.Device.startsWith(Dev) && V.Device.length() > Dev.length())) R << V;
        }
        return R;
    };

    // built-in floppy (fd*) and CD/DVD drives (sr*) first, so they get the
    // first letters (A / D)
    QStringList Names = QDir(SysBlockDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
    auto Rank = [](const QString& N) { return N.startsWith("fd") ? 0 : (N.startsWith("sr") ? 1 : 2); };
    std::sort(Names.begin(), Names.end(), [&Rank](const QString& A, const QString& B) {
        return (Rank(A) != Rank(B)) ? (Rank(A) < Rank(B)) : (A < B);
    });
    char NextFloppy = 'A', NextOptical = 'D';
    for (const QString& Name : Names) {
        TPhysicalDriveType Type;
        if (!LinuxBlockDeviceType(SysBlockDir, Name, Type)) continue;
        const QList<TMountedVolume> Mounts = MountsOf(Name, Type != pdtOptical);
        if (Type == pdtRemovable) {
            // one entry per mounted partition (USB sticks, memory cards, ...)
            for (const TMountedVolume& V : Mounts) {
                TPhysicalDrive D;
                D.Type = pdtRemovable;
                D.Device = V.Device;
                D.Root = IncludeTrailingPathDelimiter(V.Root);
                D.Label = V.Label;
                Result << D;
            }
            continue;
        }
        TPhysicalDrive D;
        D.Type = Type;
        D.Device = "/dev/" + Name;
        if (Type == pdtFloppy) { if (NextFloppy <= 'B') D.Letter = QChar(NextFloppy++); }
        else if (NextOptical <= 'Z') D.Letter = QChar(NextOptical++);
        if (!Mounts.isEmpty()) {
            D.Root = IncludeTrailingPathDelimiter(Mounts[0].Root);
            D.Label = Mounts[0].Label;
        }
        Result << D;
    }
    return Result;
}

QList<TPhysicalDrive> PhysicalDrives()
{
    QList<TMountedVolume> Mounts;
    for (const QStorageInfo& V : QStorageInfo::mountedVolumes()) {
        QString D = QString::fromLocal8Bit(V.device());
        if (!D.startsWith("/dev/") || !V.isValid() || !V.isReady()) continue;
        const QString Canonical = QFileInfo(D).canonicalFilePath();   // /dev/cdrom -> /dev/sr0
        if (!Canonical.isEmpty()) D = Canonical;
        Mounts << TMountedVolume{D, V.rootPath(), V.name()};
    }
    return PhysicalDrivesFrom(QStringLiteral("/sys/block"), Mounts);
}

QStringList CDDriveRoots()
{
    // DOSBox mounts folders: only drives with a mounted medium can be used
    QStringList Result;
    for (const TPhysicalDrive& D : PhysicalDrives(pdtOptical)) if (!D.Root.isEmpty()) Result << D.Root;
    return Result;
}

#endif

QList<TPhysicalDrive> PhysicalDrives(TPhysicalDriveType Type)
{
    QList<TPhysicalDrive> Result;
    for (const TPhysicalDrive& D : PhysicalDrives()) if (D.Type == Type) Result << D;
    return Result;
}

bool PhysicalDriveByLetter(QChar Letter, TPhysicalDrive& Drive)
{
    const QChar L = Letter.toUpper();
    for (const TPhysicalDrive& D : PhysicalDrives()) if (D.Letter == L) { Drive = D; return true; }
    return false;
}
