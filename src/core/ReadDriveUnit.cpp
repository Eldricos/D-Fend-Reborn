// C++ port of ReadDriveUnit.pas (implementation section).
#include "ReadDriveUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"

#include <cstdlib>
#include <cstdint>

#ifdef Q_OS_WIN
#include "WinApi.h"
#include <winioctl.h>
#else
#include <QStorageInfo>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef Q_OS_LINUX
#include <linux/fs.h>
#endif
#endif

// ---------------------------------------------------------------------------
// Volume access (Windows: \\.\X: via CreateFileW; elsewhere the block device
// that belongs to the drive letter)
// ---------------------------------------------------------------------------

#ifdef Q_OS_WIN
using TVolumeHandle = HANDLE;
static const TVolumeHandle InvalidVolume = INVALID_HANDLE_VALUE;
#else
using TVolumeHandle = int;
static const TVolumeHandle InvalidVolume = -1;

// Port note: drive letters do not exist outside Windows.  The device is taken
// from the Linux remap setting of the drive letter (a /dev/... path or the
// mount point of the medium); without a setting A:/B: map to /dev/fd0 and
// /dev/fd1 and every other letter to /dev/cdrom.
static QString DevicePath(QChar Drive)
{
    const QChar D = Drive.toUpper();
    QString S;
    if (PrgSetup != nullptr) S = Trim(PrgSetup->LinuxRemap(D));
    if (S != "") {
        if (S.startsWith("/dev/")) return S;
        QStorageInfo Info(S);
        if (Info.isValid() && !Info.device().isEmpty()) return QString::fromLocal8Bit(Info.device());
    }
    if (D == 'A') return "/dev/fd0";
    if (D == 'B') return "/dev/fd1";
    return "/dev/cdrom";
}
#endif

static TVolumeHandle OpenVolume(QChar Drive, bool WriteMode = false)
{
#ifdef Q_OS_WIN
    QString VolumeName = "\\\\.\\A:"; VolumeName[4] = Drive;
    /* VolumeName:='\\.\CdRom0'; */
    if (WriteMode)
        return CreateFileW(reinterpret_cast<LPCWSTR>(VolumeName.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    else
        return CreateFileW(reinterpret_cast<LPCWSTR>(VolumeName.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
#else
    const QByteArray Dev = QFile::encodeName(DevicePath(Drive));
    return ::open(Dev.constData(), WriteMode ? O_RDWR : O_RDONLY);
#endif
}

#ifdef Q_OS_WIN
static const DWORD FILE_ANY_ACCESS_ = 0;
static const DWORD METHOD_NEITHER_ = 3;
static const DWORD FILE_DEVICE_FILE_SYSTEM_ = 9;
static const DWORD FSCTL_ALLOW_EXTENDED_DASD_IO_ = (FILE_DEVICE_FILE_SYSTEM_ << 16) | (FILE_ANY_ACCESS_ << 14) | (32 << 2) | METHOD_NEITHER_;
static const DWORD IOCTL_DISK_GET_DRIVE_GEOMETRY_ = 0x00070000;

#pragma pack(push, 8)
struct TDiskGeometry {
    qint64 Cylinders;
    qint32 MediaType;
    quint32 TracksPerCylinder, SectorsPerTrack, BytesPerSector;
};
#pragma pack(pop)
#endif

static qint64 GetFloppySize(TVolumeHandle Volume)
{
    qint64 result = 0;
#ifdef Q_OS_WIN
    TDiskGeometry DiskGeometry;
    DWORD C;
    if (!DeviceIoControl(Volume, IOCTL_DISK_GET_DRIVE_GEOMETRY_, nullptr, 0, &DiskGeometry, sizeof(DiskGeometry), &C, nullptr)) return result;
    result = DiskGeometry.Cylinders * DiskGeometry.TracksPerCylinder * DiskGeometry.SectorsPerTrack * DiskGeometry.BytesPerSector;
#else
    const off_t Cur = ::lseek(Volume, 0, SEEK_CUR);
    const off_t End = ::lseek(Volume, 0, SEEK_END);
    if (Cur >= 0) ::lseek(Volume, Cur, SEEK_SET);
    if (End > 0) result = End;
#endif
    return result;
}

static bool EnableExtendedAccess(TVolumeHandle Volume)
{
    bool result;
#ifdef Q_OS_WIN
    DWORD I;
    result = DeviceIoControl(Volume, FSCTL_ALLOW_EXTENDED_DASD_IO_, nullptr, 0, nullptr, 0, &I, nullptr);
#else
    Q_UNUSED(Volume);
    result = true; // raw block devices give access to the whole medium
#endif
    if (!result) {
        result = (MessageDlg(LanguageSetup->ReadImageNoExtendedAccessWarning(), mtWarning, mbYes | mbNo, 0) == mrYes);
    }
    return result;
}

static bool CloseVolume(TVolumeHandle& Volume)
{
    bool result = false;
    if (Volume == InvalidVolume) return result;
#ifdef Q_OS_WIN
    result = CloseHandle(Volume);
#else
    result = (::close(Volume) == 0);
#endif
    if (result) Volume = InvalidVolume;
    return result;
}

// ReadFile / WriteFile: returns the number of bytes transferred in I
static bool VolumeRead(TVolumeHandle Volume, void* Buffer, quint32 Count, quint32& I)
{
#ifdef Q_OS_WIN
    DWORD D = 0;
    const bool B = ReadFile(Volume, Buffer, Count, &D, nullptr);
    I = D;
    return B;
#else
    const ssize_t R = ::read(Volume, Buffer, Count);
    I = (R > 0) ? quint32(R) : 0;
    return R >= 0;
#endif
}

static bool VolumeWrite(TVolumeHandle Volume, const void* Buffer, quint32 Count, quint32& I)
{
#ifdef Q_OS_WIN
    DWORD D = 0;
    const bool B = WriteFile(Volume, Buffer, Count, &D, nullptr);
    I = D;
    return B;
#else
    const ssize_t R = ::write(Volume, Buffer, Count);
    I = (R > 0) ? quint32(R) : 0;
    return R >= 0;
#endif
}

static bool GetDriveData(QChar Drive, quint32& BytesToRead, quint32& SectorSize)
{
#ifdef Q_OS_WIN
    DWORD lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters;
    const QString Root = QString(Drive) + ":\\";
    bool result = GetDiskFreeSpaceW(reinterpret_cast<LPCWSTR>(Root.utf16()), &lpSectorsPerCluster, &lpBytesPerSector, &lpNumberOfFreeClusters, &lpTotalNumberOfClusters);
    if (result) { SectorSize = lpBytesPerSector; BytesToRead = lpSectorsPerCluster * SectorSize * lpTotalNumberOfClusters; }
    return result;
#else
    TVolumeHandle V = OpenVolume(Drive);
    if (V == InvalidVolume) return false;
    int SSize = 512;
#ifdef Q_OS_LINUX
    int S = 0;
    if (::ioctl(V, BLKSSZGET, &S) == 0 && S > 0) SSize = S;
#endif
    const qint64 Size = GetFloppySize(V);
    CloseVolume(V);
    if (Size <= 0) return false;
    SectorSize = quint32(SSize);
    BytesToRead = quint32(Size);
    return true;
#endif
}

static bool GetDriveData(QChar Drive, quint32& SectorSize)
{
    quint32 BytesToRead;
    return GetDriveData(Drive, BytesToRead, SectorSize);
}

static void GetAlignedBuffer(quint32 BufferSize, quint32 Align, void*& RealPointer, void*& AlignedPointer)
{
    RealPointer = std::malloc(BufferSize + Align);
    AlignedPointer = RealPointer;
    if (reinterpret_cast<uintptr_t>(AlignedPointer) % Align != 0)
        AlignedPointer = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(AlignedPointer) + Align - (reinterpret_cast<uintptr_t>(AlignedPointer) % Align));
}

TReadDataResult ReadCDData(QChar Drive, const QString& FileName, TReadProgressEvent OnProgress)
{
    TVolumeHandle Volume;
    quint32 BytesToRead = 0, SectorSize = 0, BytesRead, TotalBytesToRead, I;
    void *RealPointer, *AlignedPointer;
    bool ContinueReading;
    if (!GetDriveData(Drive, BytesToRead, SectorSize)) { return RD_CannotGetDriveData; }
    Volume = OpenVolume(Drive);
    if (Volume == InvalidVolume) { return RD_CannotOpen; }
    TReadDataResult result = RD_OK;
    [&]() {
        if (!EnableExtendedAccess(Volume)) { result = RD_CannotSetExtendedAccess; return; }
        QFile OutputFile(FileName);
        if (!OutputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) { result = RD_CannotOpenOutputFile; return; }
        GetAlignedBuffer(SectorSize, SectorSize, RealPointer, AlignedPointer);
        [&]() {
            BytesRead = 0; TotalBytesToRead = BytesToRead;
            while (BytesToRead > 0) {
                if ((!VolumeRead(Volume, AlignedPointer, Min(BytesToRead, SectorSize), I)) || (I != Min(BytesToRead, SectorSize))) {
                    result = RD_ReadError; return;
                }
                OutputFile.write(static_cast<const char*>(AlignedPointer), I);
                BytesToRead = BytesToRead - I;
                BytesRead = BytesRead + I;
                if (OnProgress) {
                    ContinueReading = true;
                    OnProgress(BytesRead, TotalBytesToRead, ContinueReading);
                    if (!ContinueReading) { result = RD_Canceled; return; }
                }
            }
        }();
        std::free(RealPointer);
    }();
    CloseVolume(Volume);
    return result;
}

TReadDataResult ReadFloppyData(QChar Drive, const QString& FileName, TReadProgressEvent OnProgress)
{
    TVolumeHandle Volume;
    quint32 BytesToRead = 0, SectorSize = 0, BytesRead, I;
    void *RealPointer, *AlignedPointer;
    bool ContinueReading;
    quint8 B;
    if (!GetDriveData(Drive, BytesToRead, SectorSize)) { return RD_CannotGetDriveData; }
    Volume = OpenVolume(Drive);
    if (Volume == InvalidVolume) { return RD_CannotOpen; }
    TReadDataResult result = RD_OK;
    [&]() {
        if (!EnableExtendedAccess(Volume)) { result = RD_CannotSetExtendedAccess; return; }
        QFile OutputFile(FileName);
        if (!OutputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) { result = RD_CannotOpenOutputFile; return; }
        GetAlignedBuffer(SectorSize, SectorSize, RealPointer, AlignedPointer);
        [&]() {
            BytesRead = 0;
            do {
                I = 0;
                VolumeRead(Volume, AlignedPointer, SectorSize, I);
                OutputFile.write(static_cast<const char*>(AlignedPointer), I);
                BytesRead = BytesRead + I;
                if (I < SectorSize) {
                    if (BytesRead == 0) { result = RD_ReadError; return; }
                    break;
                }
                if (OnProgress) {
                    ContinueReading = true;
                    OnProgress(BytesRead, 0, ContinueReading);
                    if (!ContinueReading) { result = RD_Canceled; return; }
                }
            } while (true);
            if (BytesRead > 0) {
                qint64 Rest = Max<qint64>(0, GetFloppySize(Volume) - BytesRead);
                B = 0xF6;
                while (Rest > 0) { OutputFile.write(reinterpret_cast<const char*>(&B), 1); Rest--; }
            }
        }();
        std::free(RealPointer);
    }();
    CloseVolume(Volume);
    return result;
}

TReadDataResult WriteFloppyData(QChar Drive, const QString& FileName, TReadProgressEvent OnProgress)
{
    TVolumeHandle Volume;
    quint32 BytesToWrite, SectorSize = 0, BytesWritten, TotalBytesToWrite, I;
    void *RealPointer, *AlignedPointer;
    bool ContinueWriting;
    if (!GetDriveData(Drive, SectorSize)) { return RD_CannotGetDriveData; }
    Volume = OpenVolume(Drive, true);
    if (Volume == InvalidVolume) { return RD_CannotOpen; }
    TReadDataResult result = RD_OK;
    [&]() {
        if (!EnableExtendedAccess(Volume)) { result = RD_CannotSetExtendedAccess; return; }
        QFile InputFile(FileName);
        if (!InputFile.open(QIODevice::ReadOnly)) { result = RD_CannotOpenInputFile; return; }
        GetAlignedBuffer(SectorSize, SectorSize, RealPointer, AlignedPointer);
        [&]() {
            BytesWritten = 0;
            BytesToWrite = quint32(InputFile.size());
            TotalBytesToWrite = BytesToWrite;
            while (BytesToWrite > 0) {
                // Delphi: ReadBuffer raises EReadError if less than SectorSize bytes are left
                if (InputFile.read(static_cast<char*>(AlignedPointer), SectorSize) != qint64(SectorSize)) { result = RD_WriteError; return; }
                I = 0;
                VolumeWrite(Volume, AlignedPointer, SectorSize, I);
                if (I < SectorSize) { result = RD_WriteError; return; }
                BytesWritten = BytesWritten + I;
                BytesToWrite = BytesToWrite - I;
                if (OnProgress) {
                    ContinueWriting = true;
                    OnProgress(BytesWritten, TotalBytesToWrite, ContinueWriting);
                    if (!ContinueWriting) { result = RD_Canceled; return; }
                }
            }
        }();
        std::free(RealPointer);
    }();
    CloseVolume(Volume);
    return result;
}
