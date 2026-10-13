// C++ port of CreateImageToolsUnit.pas (implementation section).
#include "CreateImageToolsUnit.h"

#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "DosBoxUnit.h"
#include "ImageTools.h"
#include "DOSBoxTempUnit.h"
#include "LoggingUnit.h"
#include "MainUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#include <winioctl.h>
#include <io.h>
#endif

/* Creating plain and fat disk images */

// Program used to create FAT file systems: the bundled mkdosfs.exe on
// Windows, mkfs.fat (dosfstools) on Linux/macOS.
QString MakeDOSFilesystemProgram()
{
    QString S = FindHelperProgram(MakeDOSFilesystemFileName);
#ifndef Q_OS_WIN
    if (S.isEmpty()) S = FindHelperProgram(QStringLiteral("mkdosfs"));
#endif
    return S;
}

bool DiskImageCreatorFATAvailable()
{
    return !MakeDOSFilesystemProgram().isEmpty();
}

#ifdef Q_OS_WIN
static const DWORD COMPRESSION_FORMAT_NONE_ = 0;
static const DWORD COMPRESSION_FORMAT_DEFAULT_ = 1;
static const DWORD FILE_DEVICE_FILE_SYSTEM_ = 9;
static const DWORD METHOD_BUFFERED_ = 0;
static const DWORD FILE_READ_DATA_ = 1;
static const DWORD FILE_WRITE_DATA_ = 2;
static const DWORD FSCTL_SET_COMPRESSION_ = (FILE_DEVICE_FILE_SYSTEM_ << 16) | ((FILE_READ_DATA_ | FILE_WRITE_DATA_) << 14) | (16 << 2) | METHOD_BUFFERED_;
#endif

static void CompressFile(const QString& FileName)
{
#ifdef Q_OS_WIN
    HANDLE hFile;
    SHORT Mode;
    DWORD C;
    hFile = CreateFileW(reinterpret_cast<LPCWSTR>(FileName.utf16()), GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    Mode = COMPRESSION_FORMAT_DEFAULT_;
    DeviceIoControl(hFile, FSCTL_SET_COMPRESSION_, &Mode, sizeof(Mode), nullptr, 0, &C, nullptr);
    CloseHandle(hFile);
#else
    // TODO(port): NTFS file compression has no equivalent on other systems.
    Q_UNUSED(FileName);
#endif
}

static bool DiskImageCreatorPlainInt(int SecCount, const QString& ImageFileName, bool CompressImage)
{
    char Buffer[512];
    bool result;
    memset(Buffer, 0, sizeof(Buffer));

    QFile hFile(ImageFileName);
    result = hFile.open(QIODevice::ReadWrite | QIODevice::Truncate);
    if (!result) return result;
    [&]() {
        result = (hFile.write(Buffer, sizeof(Buffer)) == qint64(sizeof(Buffer))); if (!result) return;

        if (CompressImage) {
#ifdef Q_OS_WIN
            hFile.flush();
            HANDLE H = reinterpret_cast<HANDLE>(_get_osfhandle(hFile.handle()));
            SHORT Mode = COMPRESSION_FORMAT_DEFAULT_;
            DWORD C;
            DeviceIoControl(H, FSCTL_SET_COMPRESSION_, &Mode, sizeof(Mode), nullptr, 0, &C, nullptr);
#endif
        }

        const qint64 Pos = qint64(SecCount - 1) << 9;
        result = hFile.seek(Pos);

        result = (hFile.write(Buffer, sizeof(Buffer)) == qint64(sizeof(Buffer)));
    }();
    hFile.close();
    if (!result) MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ImageFileName}), mtError, mbOK, 0);
    return result;
}

static bool DiskImageCreatorPlain(int Cylinders, int Heads, int SPT, const QString& ImageFileName, bool CompressImage)
{
    return DiskImageCreatorPlainInt(Cylinders * Heads * SPT, ImageFileName, CompressImage);
}

static bool DiskImageCreatorPlain(int HDSize, const QString& ImageFileName, bool CompressImage)
{
    int I;
    bool result = false;
    I = HDSize * 128 / 63; /* 512*63*16*X=Bytes => X=MBytes*128/63 */
    if (I <= 0) {
        MessageDlg(LanguageSetup->MessageWrongDiskSize(), mtError, mbOK, 0);
        return result;
    }
    I = I * 16 * 63;
    result = DiskImageCreatorPlainInt(I, ImageFileName, CompressImage);
    return result;
}

#ifdef Q_OS_WIN
static void DiskImageCreatorFATInt(TStringList* Data, const QString& ImageFileName, bool CompressImage)
{
    // mkdosfs.exe (Bochs tool) reads its answers from stdin
    if (!RunWithInput(MakeDOSFilesystemProgram(), "", false, Data)) return;
    if (CompressImage) CompressFile(ImageFileName);
}
#else
// mkfs.fat (dosfstools) takes command line parameters; the images are made
// like mkdosfs.exe does (same geometry, hard disk images with a MBR and one
// partition starting at sector 63).
static bool RunMkfsFat(const QStringList& Args)
{
    QProcess P;
    P.setProcessChannelMode(QProcess::MergedChannels);
    P.start(MakeDOSFilesystemProgram(), Args);
    if (!P.waitForStarted() || !P.waitForFinished(120000)) return false;
    if (P.exitStatus() != QProcess::NormalExit || P.exitCode() != 0) {
        LogInfo("mkfs.fat " + Args.join(' ') + " failed: " + QString::fromLocal8Bit(P.readAll()));
        return false;
    }
    return true;
}

static bool DiskImageCreatorFATFloppy(int Cylinders, int Heads, int SPT, const QString& ImageFileName)
{
    const int Sectors = Cylinders * Heads * SPT;
    if (FileExists(ImageFileName)) DeleteFile(ImageFileName);   // -C needs a new file
    const QString SPC = ((SPT == 36) && (Heads == 2) && (Cylinders == 80)) ? "2" : "1";
    return RunMkfsFat({"-C", "-F", "12", "-f", "2", "-s", SPC, "-g", IntToStr(Heads) + "/" + IntToStr(SPT), ImageFileName, IntToStr(Sectors / 2)});
}

static bool WriteMBR(const QString& ImageFileName, quint32 Sectors)
{
    // like mkdosfs.exe: one bootable partition at sector 63, 16 heads, 63 spt
    const quint32 Heads = 16, SPT = 63;
    unsigned char MBR[512];
    memset(MBR, 0, sizeof(MBR));
    unsigned char *E = MBR + 446;
    E[0] = 0x80;                                   // bootable
    E[1] = 1; E[2] = 1; E[3] = 0;                   // start: head 1, sector 1, cylinder 0
    E[4] = (Sectors < 65536) ? 0x04 : 0x06;          // FAT16 <32MB / FAT16
    const quint32 Last = Sectors - 1 + 63;
    if (Last <= 16450560) {
        const quint32 Sect = ((Last % SPT) + 1) & 0x3F;
        const quint32 Cyl = (Last / SPT) / Heads;
        E[5] = (unsigned char)((Last / SPT) % Heads);
        E[6] = (unsigned char)(((Cyl & 0x300) >> 2) | Sect);
        E[7] = (unsigned char)(Cyl & 0xFF);
    } else {
        E[5] = E[6] = E[7] = 0xFF;
    }
    const quint32 Start = 63;
    for (int I = 0; I < 4; I++) { E[8 + I] = (unsigned char)(Start >> (8 * I)); E[12 + I] = (unsigned char)(Sectors >> (8 * I)); }
    MBR[510] = 0x55; MBR[511] = 0xAA;

    QFile F(ImageFileName);
    if (!F.open(QIODevice::ReadWrite)) return false;
    const bool Ok = (F.write(reinterpret_cast<const char *>(MBR), sizeof(MBR)) == qint64(sizeof(MBR)));
    F.close();
    return Ok;
}

static bool DiskImageCreatorFATHD(int MB, int SPC, const QString& ImageFileName)
{
    const quint32 Sectors = quint32(MB) * 2 * 16 * 63;   // cylinders = 2 * MB, 16 heads, 63 spt
    if (!DiskImageCreatorPlainInt(int(Sectors + 63), ImageFileName, false)) return false;
    if (!WriteMBR(ImageFileName, Sectors)) return false;
    QStringList Args = {"--offset=63", "-h", "63", "-g", "16/63", "-M", "0xF8", "-f", "2", "-s", IntToStr(SPC)};
    // FAT16 needs at least 4085 clusters; mkfs.fat refuses smaller FAT16
    // file systems, so let it choose the FAT type for very small images
    if (RunMkfsFat(QStringList(Args) << "-F" << "16" << ImageFileName << IntToStr(Sectors / 2))) return true;
    return RunMkfsFat(QStringList(Args) << ImageFileName << IntToStr(Sectors / 2));
}
#endif

static bool DiskImageCreatorFAT(int Cylinders, int Heads, int SPT, const QString& ImageFileName, bool CompressImage)
{
    TStringList St;
    /* type */ St.Add("fd");
    /* filename */ St.Add(ImageFileName);
    /* sectors/tracks */ St.Add(IntToStr(SPT));
    /* heads */ St.Add(IntToStr(Heads));
    /* cylinders */ St.Add(IntToStr(Cylinders));
    /* sectors/cluster */ if ((SPT == 36) && (Heads == 2) && (Cylinders == 80)) St.Add("2"); else St.Add("1");
    /* number of fats */ St.Add("2");
    /* fat size */ St.Add("12");
#ifdef Q_OS_WIN
    DiskImageCreatorFATInt(&St, ImageFileName, CompressImage);
    return true;
#else
    Q_UNUSED(CompressImage);
    if (!DiskImageCreatorFATFloppy(Cylinders, Heads, SPT, ImageFileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ImageFileName}), mtError, mbOK, 0);
        return false;
    }
    return true;
#endif
}

static bool DiskImageCreatorFAT(int HDSize, const QString& ImageFileName, bool CompressImage)
{
    TStringList St;
    int I;
    bool result = false;
    I = HDSize;
    if ((I <= 0) || (I > 1000)) {
        MessageDlg(LanguageSetup->MessageWrongDiskSize(), mtError, mbOK, 0);
        return result;
    }
    I = (I * 1024) / 1000;
    /* type */ St.Add("hd");
    /* create MBR */ St.Add("y");
    /* filename */ St.Add(ImageFileName);
    /* size (in MB) */ St.Add(IntToStr(I));
    /* sectors/track */
    if (I >= 1 && I <= 32) St.Add("1");
    else if (I >= 33 && I <= 64) St.Add("2");
    else if (I >= 65 && I <= 128) St.Add("4");
    else if (I >= 129 && I <= 256) St.Add("8");
    else if (I >= 257 && I <= 512) St.Add("16");
    else St.Add("32");
    /* number of fats */ St.Add("2");
    /* fat size */ St.Add("16");
#ifdef Q_OS_WIN
    DiskImageCreatorFATInt(&St, ImageFileName, CompressImage);
    result = true;
#else
    Q_UNUSED(CompressImage);
    result = DiskImageCreatorFATHD(I, St[4].toInt(), ImageFileName);
    if (!result) MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ImageFileName}), mtError, mbOK, 0);
#endif
    return result;
}

bool DiskImageCreator(int Cylinders, int Heads, int SPT, const QString& ImageFileName, bool CompressImage, bool FAT)
{
    if (FAT && DiskImageCreatorFATAvailable())
        return DiskImageCreatorFAT(Cylinders, Heads, SPT, ImageFileName, CompressImage);
    else
        return DiskImageCreatorPlain(Cylinders, Heads, SPT, ImageFileName, CompressImage);
}

bool DiskImageCreator(const QString& ImageFileName)
{
    return DiskImageCreatorFATAvailable() && DiskImageCreator(80, 2, 18, ImageFileName, false, true);
}

bool DiskImageCreator(int HDSize, const QString& ImageFileName, bool CompressImage, bool FAT)
{
    if (FAT && DiskImageCreatorFATAvailable())
        return DiskImageCreatorFAT(HDSize, ImageFileName, CompressImage);
    else
        return DiskImageCreatorPlain(HDSize, ImageFileName, CompressImage);
}

/* Making images bootable and copy files */

static const quint8 BootSec[512] = {
    0xEB, 0x3C, 0x90, 0x46, 0x52, 0x44, 0x4F, 0x53, 0x34, 0x2E, 0x31, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x40, 0x0B, 0xF0, 0x09, 0x00,
    0x12, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x0B, 0x60, 0xF5, 0x37, 0x46, 0x52, 0x45, 0x45, 0x44,
    0x4F, 0x53, 0x32, 0x30, 0x30, 0x30, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0xFA, 0xFC, 0x31, 0xC0, 0x8E, 0xD8, 0xBD, 0x00, 0x7C, 0xB8,
    0xE0, 0x1F, 0x8E, 0xC0, 0x89, 0xEE, 0x89, 0xEF, 0xB9, 0x00, 0x01, 0xF3, 0xA5, 0xEA, 0x5E, 0x7C, 0xE0, 0x1F, 0x00, 0x00, 0x60, 0x00, 0x8E, 0xD8,
    0x8E, 0xD0, 0x8D, 0x66, 0xA0, 0xFB, 0x80, 0x7E, 0x24, 0xFF, 0x75, 0x03, 0x88, 0x56, 0x24, 0xC7, 0x46, 0xC0, 0x10, 0x00, 0xC7, 0x46, 0xC2, 0x01,
    0x00, 0x8C, 0x5E, 0xC6, 0xC7, 0x46, 0xC4, 0xA0, 0x63, 0x8B, 0x76, 0x1C, 0x8B, 0x7E, 0x1E, 0x03, 0x76, 0x0E, 0x83, 0xD7, 0x00, 0x89, 0x76, 0xD2,
    0x89, 0x7E, 0xD4, 0x8A, 0x46, 0x10, 0x98, 0xF7, 0x66, 0x16, 0x01, 0xC6, 0x11, 0xD7, 0x89, 0x76, 0xD6, 0x89, 0x7E, 0xD8, 0x8B, 0x5E, 0x0B, 0xB1,
    0x05, 0xD3, 0xEB, 0x8B, 0x46, 0x11, 0x31, 0xD2, 0xF7, 0xF3, 0x50, 0x01, 0xC6, 0x83, 0xD7, 0x00, 0x89, 0x76, 0xDA, 0x89, 0x7E, 0xDC, 0x8B, 0x46,
    0xD6, 0x8B, 0x56, 0xD8, 0x5F, 0xC4, 0x5E, 0x5A, 0xE8, 0x98, 0x00, 0xC4, 0x7E, 0x5A, 0xB9, 0x0B, 0x00, 0xBE, 0xF1, 0x7D, 0x57, 0xF3, 0xA6, 0x5F,
    0x26, 0x8B, 0x45, 0x1A, 0x74, 0x0B, 0x83, 0xC7, 0x20, 0x26, 0x80, 0x3D, 0x00, 0x75, 0xE7, 0x72, 0x68, 0x50, 0xC4, 0x5E, 0x5A, 0x8B, 0x7E, 0x16,
    0x8B, 0x46, 0xD2, 0x8B, 0x56, 0xD4, 0xE8, 0x6A, 0x00, 0x58, 0x1E, 0x07, 0x8E, 0x5E, 0x5C, 0xBF, 0x00, 0x20, 0xAB, 0x89, 0xC6, 0x01, 0xF6, 0x01,
    0xC6, 0xD1, 0xEE, 0xAD, 0x73, 0x04, 0xB1, 0x04, 0xD3, 0xE8, 0x80, 0xE4, 0x0F, 0x3D, 0xF8, 0x0F, 0x72, 0xE8, 0x31, 0xC0, 0xAB, 0x0E, 0x1F, 0xC4,
    0x5E, 0x5A, 0xBE, 0x00, 0x20, 0xAD, 0x09, 0xC0, 0x75, 0x05, 0x88, 0xD3, 0xFF, 0x6E, 0x5A, 0x48, 0x48, 0x8B, 0x7E, 0x0D, 0x81, 0xE7, 0xFF, 0x00,
    0xF7, 0xE7, 0x03, 0x46, 0xDA, 0x13, 0x56, 0xDC, 0xE8, 0x20, 0x00, 0xEB, 0xE0, 0x5E, 0xAC, 0x56, 0xB4, 0x0E, 0xCD, 0x10, 0x3C, 0x2E, 0x75, 0xF5,
    0xC3, 0xE8, 0xF1, 0xFF, 0x45, 0x72, 0x72, 0x6F, 0x72, 0x21, 0x2E, 0x30, 0xE4, 0xCD, 0x13, 0xCD, 0x16, 0xCD, 0x19, 0x56, 0x89, 0x46, 0xC8, 0x89,
    0x56, 0xCA, 0x8C, 0x86, 0x9E, 0xE7, 0x89, 0x9E, 0x9C, 0xE7, 0xE8, 0xD0, 0xFF, 0x2E, 0xB4, 0x41, 0xBB, 0xAA, 0x55, 0x8A, 0x56, 0x24, 0x84, 0xD2,
    0x74, 0x19, 0xCD, 0x13, 0x72, 0x15, 0xD1, 0xE9, 0x81, 0xDB, 0x54, 0xAA, 0x75, 0x0D, 0x8D, 0x76, 0xC0, 0x89, 0x5E, 0xCC, 0x89, 0x5E, 0xCE, 0xB4,
    0x42, 0xEB, 0x26, 0x8B, 0x4E, 0xC8, 0x8B, 0x56, 0xCA, 0x8A, 0x46, 0x18, 0xF6, 0x66, 0x1A, 0x91, 0xF7, 0xF1, 0x92, 0xF6, 0x76, 0x18, 0x89, 0xD1,
    0x88, 0xC6, 0x86, 0xE9, 0xD0, 0xC9, 0xD0, 0xC9, 0x08, 0xE1, 0x41, 0xC4, 0x5E, 0xC4, 0xB8, 0x01, 0x02, 0x8A, 0x56, 0x24, 0xCD, 0x13, 0x72, 0x89,
    0x8B, 0x46, 0x0B, 0x57, 0xBE, 0xA0, 0x63, 0xC4, 0xBE, 0x9C, 0xE7, 0x89, 0xC1, 0xF3, 0xA4, 0x5F, 0xB1, 0x04, 0xD3, 0xE8, 0x01, 0x86, 0x9E, 0xE7,
    0x83, 0x46, 0xC8, 0x01, 0x83, 0x56, 0xCA, 0x00, 0x4F, 0x75, 0x8B, 0xC4, 0x9E, 0x9C, 0xE7, 0x5E, 0xC3, 0x4B, 0x45, 0x52, 0x4E, 0x45, 0x4C, 0x20,
    0x20, 0x53, 0x59, 0x53, 0x00, 0x00, 0x55, 0xAA
};

// TMemoryStream.WriteBuffer at a position (the stream grows if needed)
static void MemWrite(QByteArray& MSt, int Position, const void* Data, int Count)
{
    if (MSt.size() < Position + Count) MSt.resize(Position + Count, '\0');
    memcpy(MSt.data() + Position, Data, Count);
}

static void WriteFloppyBootSector(int Cylinders, int Heads, int SPT, const QString& ImageFile)
{
    QByteArray MSt;
    quint8 B;
    {
        QFile F(ImageFile);
        if (!F.open(QIODevice::ReadOnly)) throw Exception(Format(LanguageSetup->MessageCouldNotOpenFile(), {ImageFile}));
        MSt = F.readAll();
    }
    MemWrite(MSt, 62, &BootSec[62], 512 - 62);

    /* mediaType */
    B = 0xF0;
    if ((Heads == 2) && ((SPT == 9) || (SPT == 15)) && (Cylinders == 80)) B = 0xF9;
    if ((Heads == 2) && (SPT == 9) && (Cylinders == 40)) B = 0xFD;
    if ((Heads == 2) && (SPT == 8) && (Cylinders == 40)) B = 0xFF;
    if ((Heads == 1) && (SPT == 9) && (Cylinders == 40)) B = 0xFC;
    if ((Heads == 1) && (SPT == 8) && (Cylinders == 40)) B = 0xFE;
    MemWrite(MSt, 0x15, &B, 1);
    MemWrite(MSt, 1 * 512, &B, 1); /* beginning of sector 1 = beginning of fat 1 */
    MemWrite(MSt, 10 * 512, &B, 1); /* beginning of sector 10 = beginning of fat 1 */

    /* fix fats */
    B = 0xFF; MemWrite(MSt, 1 * 512 + 2, &B, 1);
    B = 0x00; MemWrite(MSt, 1 * 512 + 4, &B, 1);
    B = 0x00; MemWrite(MSt, 1 * 512 + 5, &B, 1);
    B = 0xFF; MemWrite(MSt, 10 * 512 + 2, &B, 1);
    B = 0x00; MemWrite(MSt, 10 * 512 + 4, &B, 1);
    B = 0x00; MemWrite(MSt, 10 * 512 + 5, &B, 1);

    QFile F(ImageFile);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate) || F.write(MSt) != MSt.size())
        throw Exception(Format(LanguageSetup->MessageCouldNotSaveFile(), {ImageFile}));
}

static QString GetCPIFileName()
{
    static const QList<int> EGA = {437, 850, 858, 852, 853, 857};
    static const QList<int> EGA2 = {859, 775, 1116, 1117, 1118, 1119};
    static const QList<int> EGA3 = {771, 772, 855, 872, 866, 808};
    static const QList<int> EGA4 = {61282, 30010, 1125, 848, 1131, 849};
    static const QList<int> EGA5 = {737, 851, 869, 858, 113, 852};
    static const QList<int> EGA6 = {59829, 60853, 30008, 899, 60258, 58210};
    static const QList<int> EGA7 = {30011, 30013, 30014, 30017, 30018, 30019};
    static const QList<int> EGA8 = {770, 773, 774, 775, 777, 778};
    static const QList<int> EGA9 = {858, 860, 861, 863, 865, 867};
    static const QList<int> EGA10 = {667, 668, 790, 991, 852, 57781};
    static const QList<int> EGA11 = {858, 30000, 30001, 30004, 30007, 30009};
    static const QList<int> EGA12 = {858, 852, 58335, 30003, 30029, 30030};
    static const QList<int> EGA13 = {852, 895, 58152, 59234, 62306, 30002};
    static const QList<int> EGA14 = {30006, 30012, 30015, 30016, 30020, 30021};
    static const QList<int> EGA15 = {30023, 30024, 30025, 30026, 30027, 30028};
    static const QList<int> EGA16 = {858, 30005, 30022};
    auto Check = [](int CP, const QList<int>& A) -> bool { return A.contains(CP); };
    int CP;
    QString result = "EGA";

    if (!TryStrToInt(LanguageSetup->GameKeyboardCodepageDefault(), CP)) return result;
    if (Check(CP, EGA)) { result = "EGA"; return result; }
    if (Check(CP, EGA2)) { result = "EGA2"; return result; }
    if (Check(CP, EGA3)) { result = "EGA3"; return result; }
    if (Check(CP, EGA4)) { result = "EGA4"; return result; }
    if (Check(CP, EGA5)) { result = "EGA5"; return result; }
    if (Check(CP, EGA6)) { result = "EGA6"; return result; }
    if (Check(CP, EGA7)) { result = "EGA7"; return result; }
    if (Check(CP, EGA8)) { result = "EGA8"; return result; }
    if (Check(CP, EGA9)) { result = "EGA9"; return result; }
    if (Check(CP, EGA10)) { result = "EGA10"; return result; }
    if (Check(CP, EGA11)) { result = "EGA11"; return result; }
    if (Check(CP, EGA12)) { result = "EGA12"; return result; }
    if (Check(CP, EGA13)) { result = "EGA13"; return result; }
    if (Check(CP, EGA14)) { result = "EGA14"; return result; }
    if (Check(CP, EGA15)) { result = "EGA15"; return result; }
    if (Check(CP, EGA16)) { result = "EGA16"; return result; }
    return result;
}

static QString GetKeyaboardLayout()
{
    QString S;
    QString result = LanguageSetup->GameKeyboardLayoutDefault();
    if (Pos("(", result) > 0) {
        S = Copy(result, Pos("(", result) + 1, MaxInt);
        if (Pos(")", S) > 0) { S = Trim(Copy(S, 1, Pos(")", S) - 1)); if (S != "") result = S; }
    }

    if (ExtUpperCase(result) == "NONE") result = "us";
    return result;
}

static QString GetFreeDOSDir()
{
    QString result = Trim(PrgSetup->PathToFREEDOS());
    if (result != "") result = IncludeTrailingPathDelimiter(MakeAbsPath(result, PrgSetup->BaseDir()));
    if ((result == "") || (!DirectoryExists(result))) {
        MessageDlg(LanguageSetup->ProfileEditorNeedFreeDOS(), mtError, mbOK, 0);
        result = "";
        return result;
    }
    return result;
}

static QString GetDoszipDir()
{
    QString result = Trim(".\\VirtualHD\\Doszip\\");
    result = IncludeTrailingPathDelimiter(MakeAbsPath(result, PrgSetup->BaseDir()));
    if ((result == "") || (!DirectoryExists(result) || (!FileExists(result + "dz.exe")))) {
        result = "";
        return result;
    }
    return result;
}

static QString GetCodepage()
{
    return LanguageSetup->GameKeyboardCodepageDefault();
}

static bool DiskImageCreatorRunDOSBoxCommandsOnFloppyImage(const QString& ImageFileName, TStringList* Autoexec, TStringList* AdditionalFolders)
{
    QString FreeDOSDir, DoszipDir;
    TTempGame* TempGame;
    int Add;
    bool result = false;
    FreeDOSDir = GetFreeDOSDir(); if (FreeDOSDir == "") return result;
    DoszipDir = GetDoszipDir();

    TempGame = new TTempGame;
    TempGame->Game()->SetMount0(FreeDOSDir + ";DRIVE;C;False;;" + IntToStr(DefaultFreeHDSize));
    TempGame->Game()->SetMount1(ImageFileName + ";FLOPPYIMAGE;A;;;");
    if (DoszipDir != "") TempGame->Game()->SetMount2(DoszipDir + ";DRIVE;D;False;;" + IntToStr(DefaultFreeHDSize));
    if (DoszipDir != "") Add = 1; else Add = 0;
    if (AdditionalFolders != nullptr) {
        TempGame->Game()->SetNrOfMounts(2 + Add + AdditionalFolders->Count());
        for (int I = 0; I < AdditionalFolders->Count(); I++)
            TempGame->Game()->SetMount(2 + Add + I, (*AdditionalFolders)[I] + ";DRIVE;" + QChar('D' + I + Add) + ";False;;1000");
    } else {
        TempGame->Game()->SetNrOfMounts(2 + Add);
    }
    TempGame->Game()->SetAutoexec(StringListToString(Autoexec));

    TempGame->Game()->SetCycles("max");
    TempGame->Game()->StoreAllValues();
    RunCommandAndWait(TempGame->Game(), DFendReloadedMainForm->DeleteOnExit, "", true);
    delete TempGame;
    result = true;
    return result;
}

static bool DiskImageCreatorRunDOSBoxCommandsOnHDImage(const QString& ImageFileName, TStringList* Autoexec, TStringList* AdditionalFolders)
{
    QString FreeDOSDir, DoszipDir;
    TTempGame* TempGame;
    int Add;
    bool result = false;
    FreeDOSDir = GetFreeDOSDir(); if (FreeDOSDir == "") return result;
    DoszipDir = GetDoszipDir();

    TempGame = new TTempGame;
    TempGame->Game()->SetMount0(FreeDOSDir + ";DRIVE;C;False;;" + IntToStr(DefaultFreeHDSize));
    TempGame->Game()->SetMount1(ImageFileName + ";IMAGE;D;;;" + GetGeometryFromFile(ImageFileName));
    if (DoszipDir != "") TempGame->Game()->SetMount2(DoszipDir + ";DRIVE;E;False;;" + IntToStr(DefaultFreeHDSize));
    if (DoszipDir != "") Add = 1; else Add = 0;
    if (AdditionalFolders != nullptr) {
        TempGame->Game()->SetNrOfMounts(2 + Add + AdditionalFolders->Count());
        for (int I = 0; I < AdditionalFolders->Count(); I++)
            TempGame->Game()->SetMount(2 + I + Add, (*AdditionalFolders)[I] + ";DRIVE;" + QChar('E' + I + Add) + ";False;;1000");
    } else {
        TempGame->Game()->SetNrOfMounts(2 + Add);
    }
    TempGame->Game()->SetAutoexec(StringListToString(Autoexec));

    TempGame->Game()->SetCycles("max");
    TempGame->Game()->StoreAllValues();
    RunCommandAndWait(TempGame->Game(), DFendReloadedMainForm->DeleteOnExit, "", true);
    delete TempGame;
    result = true;
    return result;
}

static void AddSoundConfig(TStringList* St, const QString& Pre, const QString& Add, TGame* Game)
{
    TGame* G;
    if (Game == nullptr) {
        G = new TGame(PrgSetup); AddSoundConfig(St, Pre, Add, G); delete G;
        return;
    }
    if (Game->MixerNosound()) return;
    St->Add(Pre + "SET BLASTER=A" + Game->SBBase() + " I" + IntToStr(Game->SBIRQ()) + " D" + IntToStr(Game->SBDMA()) + " H" + IntToStr(Game->SBHDMA()) + " P330 T6" + Add);
    if (Game->GUS()) St->Add(Pre + "SET ULTRASND=" + Game->GUSBase() + "," + IntToStr(Game->GUSDMA()) + "," + IntToStr(Game->GUSDMA()) + "," + IntToStr(Game->GUSIRQ()) + "," + IntToStr(Game->GUSIRQ()) + Add);
}

static bool DiskImageCreatorUpgradeFloppy(int Cylinders, int Heads, int SPT, const QString& ImageFileName, const TDiskImageCreatorUpgrades& Upgrades, TStringList* CopyFolders, TStringList* CopyFoldersDest, TStringList* AutoexecAdd, TGame* Game)
{
    QString lh, CtMouseParameters;
    QChar C;
    bool result = false;

    if (Upgrades.contains(dicuMakeBootable)) {
        try { WriteFloppyBootSector(Cylinders, Heads, SPT, ImageFileName); } catch (...) { return result; }
    }

    TStringList St;
    if (Upgrades.contains(dicuCopyFolder) && (CopyFolders != nullptr)) {
        for (int I = 0; I < CopyFolders->Count(); I++) {
            C = QChar('D' + I);
            if ((CopyFoldersDest != nullptr) && (CopyFoldersDest->Count() > I)) {
                St.Add("mkdir A:\\" + (*CopyFoldersDest)[I]);
                St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* A:\\" + (*CopyFoldersDest)[I] + "\\ /S");
            } else {
                St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* A:\\ /S");
            }
        }
    }
    if (Upgrades.contains(dicuMakeBootable)) {
        St.Add("copy C:\\kernel.sys A:\\");
        St.Add("copy C:\\command.com A:\\");
        St.Add("echo LASTDRIVE=Z > A:\\config.sys");
        AddSoundConfig(&St, "echo ", " >> A:\\config.sys", Game);
        St.Add("echo BUFFERS=20 >> A:\\config.sys");
        St.Add("echo FILES=40 >> A:\\config.sys");
        St.Add("echo country=" + LanguageSetup->GameKeyboardCountryDefault() + " >> A:\\config.sys");
        St.Add("echo set COMSPEC=C:\\COMMAND.COM > A:\\autoexec.bat");
        St.Add("echo set temp=A:\\ >> A:\\autoexec.bat");
        St.Add("echo set tmp=A:\\ >> A:\\autoexec.bat");
        St.Add("echo set dircmd=/ogn >> A:\\autoexec.bat");
        if (Upgrades.contains(dicuMemoryManager)) {
            if (Upgrades.contains(dicuUseFD11Tools)) {
                St.Add("copy C:\\jemmex.exe A:\\");
            } else {
                St.Add("copy C:\\himem.exe A:\\");
                St.Add("copy C:\\emm386.exe A:\\");
            }
            St.Add("copy C:\\mem.exe A:\\");
            St.Add("echo dos=high,umb >> A:\\config.sys");
            St.Add("echo DOSDATA=UMB >> A:\\config.sys");
            if (Upgrades.contains(dicuUseFD11Tools)) {
                St.Add("echo devicehigh=jemmex.exe I=c800-dfff >> A:\\config.sys");
            } else {
                St.Add("echo device=himem.exe >> A:\\config.sys");
                St.Add("echo devicehigh=emm386.exe >> A:\\config.sys");
            }
            St.Add("echo SHELLHIGH=A:\\COMMAND.COM /E:256 /P >> A:\\config.sys");
            lh = "lh ";
        } else {
            lh = "";
            St.Add("echo. > A:\\config.sys");
        }
        if (Upgrades.contains(dicuKeyboardDriver)) {
            St.Add("copy C:\\display.exe A:\\");
            St.Add("copy C:\\mode.com A:\\");
            St.Add("copy C:\\CPI\\" + GetCPIFileName() + ".CPX A:\\");
            St.Add("echo " + lh + "display CON=(EGA," + GetCodepage() + ",1) >> A:\\Autoexec.bat");
            St.Add("echo mode CON CP PREP=((" + GetCodepage() + ") " + GetCPIFileName() + ".CPX) >> A:\\Autoexec.bat");
            St.Add("echo " + lh + "mode CON CP SEL=" + GetCodepage() + " >> A:\\Autoexec.bat");
            St.Add("echo " + lh + "keyb " + GetKeyaboardLayout() + "," + GetCodepage() + " >> A:\\Autoexec.bat");
            St.Add("copy C:\\keyb.exe A:\\");
            St.Add("copy C:\\keyboard.sys A:\\");
            St.Add("copy C:\\keybrd2.sys A:\\");
            St.Add("copy C:\\keybrd3.sys A:\\");
        }
        if (Upgrades.contains(dicuMouseDriver)) {
            CtMouseParameters = "";
            if (Upgrades.contains(dicuSwapMouseButtons)) CtMouseParameters = CtMouseParameters + " /L";
            if (Upgrades.contains(dicuForce2ButtonMouseMode)) CtMouseParameters = CtMouseParameters + " /Y";
            St.Add("echo " + lh + "ctmouse.exe" + CtMouseParameters + " >> A:\\Autoexec.bat");
            St.Add("copy C:\\ctmouse.exe A:\\");
        }
    }
    if (Upgrades.contains(dicuDiskTools)) {
        St.Add("copy C:\\chkdsk.exe A:\\");
        St.Add("copy C:\\fdisk.exe A:\\");
        St.Add("copy C:\\format.exe A:\\");
        St.Add("copy C:\\sys.com A:\\");
        St.Add("copy C:\\bootlace.com A:\\");
        St.Add("copy C:\\grldr A:\\");
        St.Add("copy C:\\bootfix.com A:\\");
    }
    if (Upgrades.contains(dicuAutoexecAddLines) && (AutoexecAdd != nullptr)) {
        for (int I = 0; I < AutoexecAdd->Count(); I++) St.Add("echo " + (*AutoexecAdd)[I] + " >> A:\\Autoexec.bat");
    }
    if (Upgrades.contains(dicuEditor)) {
        St.Add("copy C:\\edit.* A:\\");
    }
    if (Upgrades.contains(dicuDoszip)) {
        St.Add("copy D:\\dz.exe A:\\");
        St.Add("copy D:\\dz.dll A:\\");
        St.Add("copy D:\\doszip.txt A:\\");
    }
    St.Add("exit");
    result = DiskImageCreatorRunDOSBoxCommandsOnFloppyImage(ImageFileName, &St, CopyFolders);
    return result;
}

static bool DiskImageCreatorFloppyForHDSetup(const QString& ImageFileName)
{
    bool result = false;

    if (!DiskImageCreator(ImageFileName)) return result;
    try { WriteFloppyBootSector(80, 2, 18, ImageFileName); } catch (...) { return result; }

    TStringList St;
    St.Add("copy C:\\kernel.sys A:\\");
    St.Add("copy C:\\command.com A:\\");
    St.Add("copy C:\\fdisk.exe A:\\");
    St.Add("copy C:\\sys.com A:\\");
    St.Add("copy C:\\bootlace.com A:\\");
    St.Add("copy C:\\grldr A:\\");

    St.Add("echo default 0 > A:\\menu.lst");
    St.Add("echo timeout 0 >> A:\\menu.lst");
    St.Add("echo title FreeDOS >> A:\\menu.lst");
    St.Add("echo root (hd0,0) >> A:\\menu.lst");
    St.Add("echo chainloader (hd0,0)/kernel.sys >> A:\\menu.lst");
    St.Add("echo boot >> A:\\menu.lst");

    St.Add("echo. > A:\\config.sys");

    St.Add("echo bootlace 0x80 > A:\\Autoexec.bat");
    St.Add("echo sys C: >> A:\\Autoexec.bat");
    St.Add("echo copy grldr C: >> A:\\Autoexec.bat");
    St.Add("echo copy menu.lst C: >> A:\\Autoexec.bat");
    St.Add("echo fdisk /reboot >> A:\\Autoexec.bat");

    St.Add("exit");

    result = DiskImageCreatorRunDOSBoxCommandsOnFloppyImage(ImageFileName, &St, nullptr);
    return result;
}

static bool DiskImageCreatorRunBootdiskOnHD(const QString& ImageFileName, const QString& FloppyFileName)
{
    TTempGame* TempGame = new TTempGame;
    TempGame->Game()->SetNrOfMounts(1);
    TempGame->Game()->SetMount0(ImageFileName + ";IMAGE;2;;;" + GetGeometryFromFile(ImageFileName));
    TempGame->Game()->SetAutoexecBootImage(FloppyFileName);

    TempGame->Game()->SetCycles("max");
    TempGame->Game()->StoreAllValues();
    RunCommandAndWait(TempGame->Game(), DFendReloadedMainForm->DeleteOnExit, "", true);
    delete TempGame;
    return true;
}

static bool DiskImageCreatorUpgradeHD(const QString& ImageFileName, const TDiskImageCreatorUpgrades& Upgrades, TStringList* CopyFolders, TStringList* CopyFoldersDest, TStringList* AutoexecAdd, TGame* Game)
{
    QString TempFloppyImage;
    QString lh, CtMouseParameters;
    int J, Add;
    QChar C;
    bool result = false;

    /* Make bootable */
    if (Upgrades.contains(dicuMakeBootable)) {
        TempFloppyImage = TempDir() + "DFR-Floppy-Bootimage.img";
        if (!DiskImageCreatorFloppyForHDSetup(TempFloppyImage)) return result;
        DiskImageCreatorRunBootdiskOnHD(ImageFileName, TempFloppyImage);
        ExtDeleteFile(TempFloppyImage, ftTemp);
    }

    /* Copy files to image */
    TStringList St;
    if (Upgrades.contains(dicuMakeBootable)) {
        St.Add("echo LASTDRIVE=Z > D:\\config.sys");
        AddSoundConfig(&St, "echo ", " >> D:\\config.sys", Game);
        St.Add("echo BUFFERS=20 >> D:\\config.sys");
        St.Add("echo FILES=40 >> D:\\config.sys");
        St.Add("echo country=" + LanguageSetup->GameKeyboardCountryDefault() + " >> D:\\config.sys");
        St.Add("echo set COMSPEC=C:\\COMMAND.COM > D:\\autoexec.bat");
        St.Add("echo set temp=C:\\ >> D:\\autoexec.bat");
        St.Add("echo set tmp=C:\\ >> D:\\autoexec.bat");
        St.Add("echo set dircmd=/ogn >> D:\\autoexec.bat");
        if (Upgrades.contains(dicuMemoryManager)) {
            if (Upgrades.contains(dicuUseFD11Tools)) {
                St.Add("copy C:\\jemmex.exe D:\\");
            } else {
                St.Add("copy C:\\himem.exe D:\\");
                St.Add("copy C:\\emm386.exe D:\\");
            }
            St.Add("copy C:\\mem.exe D:\\");
            St.Add("echo dos=high,umb >> D:\\config.sys");
            St.Add("echo DOSDATA=UMB >> D:\\config.sys");
            if (Upgrades.contains(dicuUseFD11Tools)) {
                St.Add("echo devicehigh=jemmex.exe I=c800-dfff >> D:\\config.sys");
            } else {
                St.Add("echo device=himem.exe >> D:\\config.sys");
                St.Add("echo devicehigh=emm386.exe >> D:\\config.sys");
            }
            St.Add("echo SHELLHIGH=C:\\COMMAND.COM /E:256 /P >> D:\\config.sys");
            lh = "lh ";
        } else {
            lh = "";
        }
        if (Upgrades.contains(dicuKeyboardDriver)) {
            St.Add("copy C:\\display.exe D:\\");
            St.Add("copy C:\\mode.com D:\\");
            St.Add("copy C:\\CPI\\" + GetCPIFileName() + ".CPX D:\\");
            St.Add("echo " + lh + "display CON=(EGA," + GetCodepage() + ",1) >> D:\\Autoexec.bat");
            St.Add("echo mode CON CP PREP=((" + GetCodepage() + ") " + GetCPIFileName() + ".CPX) >> D:\\Autoexec.bat");
            St.Add("echo " + lh + "mode CON CP SEL=" + GetCodepage() + " >> D:\\Autoexec.bat");
            St.Add("echo " + lh + "keyb " + GetKeyaboardLayout() + "," + GetCodepage() + " >> D:\\Autoexec.bat");
            St.Add("copy C:\\keyb.exe D:\\");
            St.Add("copy C:\\keyboard.sys D:\\");
            St.Add("copy C:\\keybrd2.sys D:\\");
            St.Add("copy C:\\keybrd3.sys D:\\");
        }
        if (Upgrades.contains(dicuMouseDriver)) {
            CtMouseParameters = "";
            if (Upgrades.contains(dicuSwapMouseButtons)) CtMouseParameters = CtMouseParameters + " /L";
            if (Upgrades.contains(dicuForce2ButtonMouseMode)) CtMouseParameters = CtMouseParameters + " /Y";
            St.Add("echo " + lh + "ctmouse.exe" + CtMouseParameters + " >> D:\\Autoexec.bat");
            St.Add("copy C:\\ctmouse.exe D:\\");
        }
    }
    if (Upgrades.contains(dicuAutoexecAddLines) && (AutoexecAdd != nullptr)) {
        for (int I = 0; I < AutoexecAdd->Count(); I++) St.Add("echo " + (*AutoexecAdd)[I] + " >> D:\\Autoexec.bat");
    }
    if (Upgrades.contains(dicuDiskTools)) {
        St.Add("copy C:\\chkdsk.exe D:\\");
        St.Add("copy C:\\fdisk.exe D:\\");
        St.Add("copy C:\\format.exe D:\\");
        St.Add("copy C:\\sys.com D:\\");
        St.Add("copy C:\\bootlace.com D:\\");
        St.Add("copy C:\\grldr D:\\");
        St.Add("copy C:\\bootfix.com D:\\");
    }
    if (Upgrades.contains(dicuEditor)) {
        St.Add("copy C:\\edit.* D:\\");
    }
    if (Upgrades.contains(dicuDoszip)) {
        St.Add("copy E:\\dz.exe D:\\");
        St.Add("copy E:\\dz.dll D:\\");
        St.Add("copy E:\\doszip.txt D:\\");
    }
    {
        TStringList St2;
        if (GetDoszipDir() != "") Add = 1; else Add = 0;
        if (Upgrades.contains(dicuCopyFolder) && (CopyFolders != nullptr) && (CopyFoldersDest != nullptr)) {
            for (int I = 0; I <= Min(7, CopyFolders->Count() - 1); I++) {
                C = QChar('E' + I + Add);
                St2.Add((*CopyFolders)[I]);
                if ((CopyFoldersDest != nullptr) && (CopyFoldersDest->Count() > I)) {
                    St.Add("mkdir D:\\" + (*CopyFoldersDest)[I]);
                    St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* D:\\" + (*CopyFoldersDest)[I] + "\\ /S");
                } else {
                    St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* D:\\ /S");
                }
            }
        }
        if (St.Count() > 0) {
            St.Add("exit");
            DiskImageCreatorRunDOSBoxCommandsOnHDImage(ImageFileName, &St, &St2);
        }
        if ((CopyFolders != nullptr) && (CopyFoldersDest != nullptr)) {
            J = 8;
            while (CopyFolders->Count() > J) {
                St.Clear(); St2.Clear();
                for (int I = J; I <= Min(7 + J, CopyFolders->Count() - 1); I++) {
                    C = QChar('E' + I - J + Add);
                    St2.Add((*CopyFolders)[I]);
                    if ((CopyFoldersDest != nullptr) && (CopyFoldersDest->Count() > I)) {
                        St.Add("mkdir D:\\" + (*CopyFoldersDest)[I]);
                        St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* D:\\" + (*CopyFoldersDest)[I] + "\\ /S");
                    } else {
                        St.Add(QString("C:\\4DOS.COM /C COPY ") + C + ":\\*.* D:\\ /S");
                    }
                }
                St.Add("exit");
                DiskImageCreatorRunDOSBoxCommandsOnHDImage(ImageFileName, &St, &St2);
                J += 8;
            }
        }
    }

    result = true;
    return result;
}

bool DiskImageCreatorUpgrade(const QString& ImageFileName, bool IsHDImage, TDiskImageCreatorUpgrades Upgrades, TStringList* CopyFolders, TStringList* CopyFoldersDest, TStringList* AutoexecAdd, TGame* Game, int FloppyCylinders, int FloppyHeads, int FloppySPT)
{
    if (IsHDImage)
        return DiskImageCreatorUpgradeHD(ImageFileName, Upgrades, CopyFolders, CopyFoldersDest, AutoexecAdd, Game);
    else
        return DiskImageCreatorUpgradeFloppy(FloppyCylinders, FloppyHeads, FloppySPT, ImageFileName, Upgrades, CopyFolders, CopyFoldersDest, AutoexecAdd, Game);
}

static int CalcSektorByteSize(const QString& Dir, const QStringList& Files, int ClusterSize)
{
    int J;
    TSearchRec Rec;
    int result = 0;
    for (int I = 0; I < Files.size(); I++) {
        J = FindFirst(IncludeTrailingPathDelimiter(Dir) + Files[I], faAnyFile, Rec);
        if (J == 0) {
            result += int((Rec.Size / ClusterSize) * ClusterSize);
            if (Rec.Size % ClusterSize != 0) result += ClusterSize;
        }
        FindClose(Rec);
    }
    return result;
}

static int FolderSize(const QString& Folder, int ClusterSize)
{
    int I;
    TSearchRec Rec;
    int result = 0;

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) != 0) {
            if ((Rec.Name != "") && (Rec.Name != ".") && (Rec.Name != "..")) result += FolderSize(IncludeTrailingPathDelimiter(Folder) + Rec.Name, ClusterSize);
        } else {
            result += int((Rec.Size / ClusterSize) * ClusterSize);
            if (Rec.Size % ClusterSize != 0) result += ClusterSize;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

static int GetClusterSize(int Size)
{
    int result;
    const int MB = Size / 1024 / 1024;
    if (MB >= 1 && MB <= 32) result = 1;
    else if (MB >= 33 && MB <= 64) result = 2;
    else if (MB >= 65 && MB <= 128) result = 4;
    else if (MB >= 129 && MB <= 256) result = 8;
    else if (MB >= 257 && MB <= 512) result = 16;
    else result = 32;
    result = result * 512;
    return result;
}

static int DiskImageCreatorUpgradeSizeInt(bool IsHDImage, const TDiskImageCreatorUpgrades& Upgrades, TStringList* CopyFolders, int ClusterSize)
{
    QString FreeDOS, Doszip;
    int result = 0;

    FreeDOS = Trim(PrgSetup->PathToFREEDOS());
    if (FreeDOS != "") FreeDOS = IncludeTrailingPathDelimiter(MakeAbsPath(FreeDOS, PrgSetup->BaseDir()));
    if ((FreeDOS != "") && DirectoryExists(FreeDOS)) {
        if (Upgrades.contains(dicuMakeBootable)) {
            result += CalcSektorByteSize(FreeDOS, {"kernel.sys", "command.com"}, ClusterSize);
            if (IsHDImage) result += CalcSektorByteSize(FreeDOS, {"grldr"}, ClusterSize) + ClusterSize; /* menu.lst */
            result += ClusterSize; /* config.sys */
            if (Upgrades.contains(dicuUseFD11Tools)) {
                if (Upgrades.contains(dicuMemoryManager)) result += CalcSektorByteSize(FreeDOS, {"jemmex.exe", "mem.exe"}, ClusterSize);
            } else {
                if (Upgrades.contains(dicuMemoryManager)) result += CalcSektorByteSize(FreeDOS, {"himem.exe", "emm386.exe", "mem.exe"}, ClusterSize);
            }
            if (Upgrades.contains(dicuKeyboardDriver)) result += CalcSektorByteSize(FreeDOS, {"keyb.exe", "keyboard.sys", "keybrd2.sys", "keybrd3.sys", "mode.com", "display.exe", "CPI/" + GetCPIFileName() + ".CPX"}, ClusterSize);
            if (Upgrades.contains(dicuMouseDriver)) result += CalcSektorByteSize(FreeDOS, {"ctmouse.exe"}, ClusterSize);
            if (Upgrades.contains(dicuKeyboardDriver) || Upgrades.contains(dicuMouseDriver)) result += ClusterSize; /* autoexec.bat */
        }
        if (Upgrades.contains(dicuDiskTools)) {
            result += CalcSektorByteSize(FreeDOS, {"chkdsk.exe", "fdisk.exe", "format.exe", "sys.com", "bootlace.com", "bootfix.com"}, ClusterSize);
            if ((!IsHDImage) || (!Upgrades.contains(dicuMakeBootable))) result += CalcSektorByteSize(FreeDOS, {"grldr"}, ClusterSize);
        }
        if (Upgrades.contains(dicuEditor)) result += CalcSektorByteSize(FreeDOS, {"edit.exe", "edit.hlp", "edit.cfg"}, ClusterSize);
    }

    Doszip = Trim(".\\VirtualHD\\Doszip\\");
    if (Doszip != "") Doszip = IncludeTrailingPathDelimiter(MakeAbsPath(Doszip, PrgSetup->BaseDir()));
    if ((Doszip != "") && DirectoryExists(Doszip)) {
        if (Upgrades.contains(dicuDoszip)) result += CalcSektorByteSize(Doszip, {"doszip.txt", "dz.dll", "dz.exe"}, ClusterSize);
    }

    if (Upgrades.contains(dicuCopyFolder) && (CopyFolders != nullptr)) for (int I = 0; I < CopyFolders->Count(); I++)
        result += FolderSize(MakeAbsPath((*CopyFolders)[I], PrgSetup->BaseDir()), ClusterSize);
    return result;
}

int DiskImageCreatorUpgradeSize(bool IsHDImage, TDiskImageCreatorUpgrades Upgrades, TStringList* CopyFolders, int AddMB)
{
    int ClusterSize;
    int result;
    ClusterSize = 512;
    result = DiskImageCreatorUpgradeSizeInt(IsHDImage, Upgrades, CopyFolders, ClusterSize) + AddMB * 1024 * 1024;
    if (!IsHDImage) return result;

    while (ClusterSize < GetClusterSize(result)) {
        ClusterSize = GetClusterSize(result);
        result = DiskImageCreatorUpgradeSizeInt(IsHDImage, Upgrades, CopyFolders, ClusterSize) + AddMB * 1024 * 1024;
    }
    return result;
}

bool FD11ToolsAvailable()
{
    QString Dir;
    bool result = false;
    Dir = Trim(PrgSetup->PathToFREEDOS()); if (Dir == "") return result;
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(Dir, PrgSetup->BaseDir()));
    if (!DirectoryExists(Dir)) return result;
    result = FileExists(Dir + "jemmex.exe");
    return result;
}

bool DosZipAvailable()
{
    QString Dir;
    bool result = false;
    Dir = Trim(".\\VirtualHD\\Doszip\\"); if (Dir == "") return result;
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(Dir, PrgSetup->BaseDir()));
    if (!DirectoryExists(Dir)) return result;
    result = FileExists(Dir + "dz.exe");
    return result;
}
