// D-Fend Reborn addition - see PhysicalDrivesUnit.h
#include "PhysicalDrivesUnit.h"
#include "SysUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#ifndef Q_OS_WIN
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#ifdef DFR_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#endif

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

// udisks2 object path of a block device ("/dev/sr0" -> .../block_devices/sr0;
// characters other than A-Z, a-z, 0-9 are escaped as _XX like udisks does)
#ifdef DFR_HAVE_DBUS
static QString UDisksBlockObjectPath(const QString& Device)
{
    const QString Name = QFileInfo(Device).fileName();
    QString Escaped;
    for (QChar C : Name) {
        if ((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9')) Escaped += C;
        else Escaped += QString::asprintf("_%02x", C.unicode() & 0xFF);
    }
    return QStringLiteral("/org/freedesktop/UDisks2/block_devices/") + Escaped;
}

static QDBusMessage UDisksCall(const QString& Device, const QString& Interface, const QString& Method, const QVariantList& Args)
{
    QDBusMessage M = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.UDisks2"), UDisksBlockObjectPath(Device), Interface, Method);
    M.setArguments(Args);
    M.setInteractiveAuthorizationAllowed(true);   // polkit may ask for a password
    return QDBusConnection::systemBus().call(M, QDBus::Block, 120000);
}
#endif

int OpenDriveDeviceViaUDisks(const QString& Device, bool Write)
{
#ifdef DFR_HAVE_DBUS
    if (!QDBusConnection::systemBus().isConnected()) return -1;
    const QVariantMap NoOptions;
    // udisks >= 2.7.3: Block.OpenDevice; older versions: OpenForBackup / OpenForRestore
    QDBusMessage Reply = UDisksCall(Device, QStringLiteral("org.freedesktop.UDisks2.Block"), QStringLiteral("OpenDevice"),
                                    {Write ? QStringLiteral("rw") : QStringLiteral("r"), NoOptions});
    if (Reply.type() == QDBusMessage::ErrorMessage && Reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"))
        Reply = UDisksCall(Device, QStringLiteral("org.freedesktop.UDisks2.Block"), Write ? QStringLiteral("OpenForRestore") : QStringLiteral("OpenForBackup"), {NoOptions});
    if (Reply.type() != QDBusMessage::ReplyMessage || Reply.arguments().isEmpty()) return -1;
    const QDBusUnixFileDescriptor FD = qvariant_cast<QDBusUnixFileDescriptor>(Reply.arguments().constFirst());
    if (!FD.isValid()) return -1;
    return ::dup(FD.fileDescriptor());   // (the QDBusUnixFileDescriptor closes its own copy)
#else
    Q_UNUSED(Device); Q_UNUSED(Write);
    return -1;
#endif
}

int OpenDriveDevice(const QString& Device, bool Write)
{
    const QByteArray Dev = QFile::encodeName(Device);
    const int FD = ::open(Dev.constData(), Write ? O_RDWR : O_RDONLY);
    if (FD >= 0) return FD;
    if (errno != EACCES && errno != EPERM) return -1;
    return OpenDriveDeviceViaUDisks(Device, Write);
}

QString MountDriveMedium(const QString& Device)
{
#ifdef DFR_HAVE_DBUS
    if (!QDBusConnection::systemBus().isConnected()) return QString();
    const QDBusMessage Reply = UDisksCall(Device, QStringLiteral("org.freedesktop.UDisks2.Filesystem"), QStringLiteral("Mount"), {QVariantMap()});
    if (Reply.type() != QDBusMessage::ReplyMessage || Reply.arguments().isEmpty()) return QString();
    const QString Path = Reply.arguments().constFirst().toString();
    return Path.isEmpty() ? QString() : IncludeTrailingPathDelimiter(Path);
#else
    Q_UNUSED(Device);
    return QString();
#endif
}

// Is there a medium in the drive? (/sys/block/<name>/size is 0 without one)
static bool MediumInserted(const QString& Device)
{
    bool OK;
    const qint64 Size = ReadSysFile("/sys/block/" + QFileInfo(Device).fileName() + "/size").toLongLong(&OK);
    return OK && Size > 0;
}

QStringList CDDriveRoots()
{
    // DOSBox mounts folders: only drives with a mounted medium can be used.
    // Inserted discs that are not mounted yet are mounted via udisks2.
    QStringList Result;
    for (const TPhysicalDrive& D : PhysicalDrives(pdtOptical)) {
        QString Root = D.Root;
        if (Root.isEmpty() && MediumInserted(D.Device)) Root = MountDriveMedium(D.Device);
        if (!Root.isEmpty()) Result << Root;
    }
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
