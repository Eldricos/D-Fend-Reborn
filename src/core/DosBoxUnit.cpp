// C++ port of DosBoxUnit.pas (implementation section).
//
// Builds the DOSBox configuration file (BuildConfFile / BuildAutoexec) and
// starts DOSBox.  The generated configuration must be identical to the one
// created by the original program, so every line is translated literally.
//
// Notes on the port:
// * THandle is the process id of the started program (see Process.h);
//   0 replaces INVALID_HANDLE_VALUE, CloseHandle is not needed.
// * Paths inside DOSBox ("C:\GAME") always use '\'.  Host path fragments
//   that become part of DOSBox paths are converted from PathDelim to '\'
//   (no-op on Windows).
#include "DosBoxUnit.h"

#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <functional>

#include "Process.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "ZipManagerUnit.h"
#include "ScreensaverControlUnit.h"
#include "FullscreenInfoFormUnit.h"
#include "DOSBoxCountUnit.h"
#include "DOSBoxShortNameUnit.h"
#include "DosCodePage.h"
#include "PhysicalDrivesUnit.h"
#include <QRegularExpression>
#include "RunPrgManagerUnit.h"
#include "SelectCDDriveToMountFormUnit.h"
#include "SelectCDDriveToMountByDataFormUnit.h"
#include "FileNameConvertor.h"
#include "DOSBoxTempUnit.h"
#include "WindowsFileWarningFormUnit.h"
#include "MIDITools.h"
#include "HistoryUnit.h"
#include "MainUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#ifndef CSIDL_DESKTOPDIRECTORY
#define CSIDL_DESKTOPDIRECTORY 0x0010
#endif

// {DEFINE ShowSelectDialogEvenIfOnlyOneCDDrive}

bool MinimizedAtDOSBoxStart = false;

// INVALID_HANDLE_VALUE (THandle = process id in the port)
static const THandle InvalidHandle = 0;

// --- Application (Forms.TApplication) helpers -------------------------------

static QWidget* ApplicationMainForm()
{
    return DFendRebornMainForm;
}

static void ApplicationRestore()
{
    if (DFendRebornMainForm == nullptr) return;
    if (DFendRebornMainForm->isMinimized()) DFendRebornMainForm->showNormal();
    DFendRebornMainForm->activateWindow();
}

static void ApplicationMinimize()
{
    if (DFendRebornMainForm != nullptr) DFendRebornMainForm->showMinimized();
}

static bool MainFormIsMinimized()
{
    return (DFendRebornMainForm != nullptr) && DFendRebornMainForm->isMinimized();
}

// try/finally replacement
struct TFinally {
    std::function<void()> F;
    ~TFinally() { F(); }
};

// Host path fragment -> path fragment inside DOSBox (no-op on Windows)
static QString ToDOSDelims(QString S)
{
    if (PathDelimChar != QChar('\\')) S.replace(PathDelimChar, QChar('\\'));
    return S;
}

// IncludeTrailingPathDelimiter for paths inside DOSBox
static QString IncludeTrailingDOSDelimiter(const QString& S)
{
    if (S.isEmpty() || S[S.length() - 1] != QChar('\\')) return S + "\\";
    return S;
}

// --- speed test --------------------------------------------------------------

static TStringList* SpeedTestSt = nullptr;
static QString LastSpeedTestStep = "";
static QString LastSpeedTestStep2 = "";
static quint32 LastSpeedTestTickCount = 0;
static int SpeedTest = -1;

static QString SpeedTestLogFile()
{
    return IncludeTrailingPathDelimiter(GetSpecialFolder(DFendRebornMainForm ? DFendRebornMainForm->winId() : WId(0), CSIDL_DESKTOPDIRECTORY)) + "D-Fend-Reborn-DOSBoxStartLog.txt";
}

static bool IsSpeedTest()
{
    bool result;
    if (SpeedTest == -1) {
        result = PrgSetup->DOSBoxStartLogging() || FileExists(SpeedTestLogFile());
        if (result) SpeedTest = 1; else SpeedTest = 0;
    } else {
        result = (SpeedTest == 1);
    }
    return result;
}

static void SpeedTestInfo(const QString& Info, bool Init = false)
{
    if (!IsSpeedTest()) return;

    if (SpeedTestSt != nullptr) {
        SpeedTestSt->Add(LastSpeedTestStep + ": " + IntToStr(GetTickCount() - LastSpeedTestTickCount) + "ms");
        if (!LastSpeedTestStep2.isEmpty()) SpeedTestSt->Add("  " + LastSpeedTestStep2);
        LastSpeedTestStep = Info;
        LastSpeedTestStep2 = "";
        LastSpeedTestTickCount = GetTickCount();
        return;
    }

    LastSpeedTestStep = "";
    LastSpeedTestStep2 = "";

    if (!Init) return;

    SpeedTestSt = new TStringList();
    SpeedTestSt->Add(TimeToStr(Now()) + " ### Benchmarking DOSBox start ###" + "\r");
    LastSpeedTestStep = Info;
    LastSpeedTestTickCount = GetTickCount();
}

static void SpeedTestInfoOnly(const QString& Info)
{
    if (!IsSpeedTest()) return;

    if (SpeedTestSt != nullptr) {
        LastSpeedTestStep2 = Info;
    }
}

static void SpeedTestDone()
{
    TStringList* St;
    if (!IsSpeedTest()) return;

    if (SpeedTestSt == nullptr) return;
    SpeedTestInfo("");
    St = new TStringList();
    if (FileExists(SpeedTestLogFile())) {
        St->LoadFromFile(SpeedTestLogFile());
    }
    St->Add("");
    St->AddStrings(*SpeedTestSt);
    St->SaveToFile(SpeedTestLogFile());
    delete St;
    delete SpeedTestSt;
    SpeedTestSt = nullptr;
}

// Delphi: Function BoolToStr(const B : Boolean) : String (renamed, the compat
// layer has a SysUtils.BoolToStr with a different result)
static QString ConfBoolToStr(bool B)
{
    if (B) return "true"; else return "false";
}

QString ShortName(const QString& LongName)
{
    if (PrgSetup->UseShortFolderNames()) {
#ifdef Q_OS_WIN
        wchar_t Buffer[MAX_PATH + 10];
        QString L = ToNativePath(LongName);
        DWORD Len = GetShortPathNameW(reinterpret_cast<LPCWSTR>(L.utf16()), Buffer, MAX_PATH);
        if (Len == 0 || Len >= MAX_PATH)
            return LongName;
        else
            return QString::fromWCharArray(Buffer, static_cast<int>(Len));
#else
        // Port note: no short (8.3) path names outside Windows
        return LongName;
#endif
    } else {
        return LongName;
    }
}

QString LongPath(const QString& ShortPath)
{
#ifdef Q_OS_WIN
    QString S, T;
    int I;
    QString result = Copy(ShortPath, 1, 3);
    S = ExcludeTrailingPathDelimiter(Copy(ShortPath, 4, MaxInt));
    while (!S.isEmpty()) {
        I = Pos("\\", S);
        if (I == 0) { T = S; S = ""; } else { T = Copy(S, 1, I - 1); S = Copy(S, I + 1, MaxInt); }
        result = result + WinExpandLongPathName(result, T) + "\\";
    }
    return result;
#else
    // Port note: there are no short path names outside Windows (ShortName and
    // MakePathShort return the path unchanged), so the long path is the path itself.
    if (ShortPath.isEmpty()) return ShortPath;
    return IncludeTrailingPathDelimiter(ShortPath);
#endif
}

/* Function LongPath(const Root, ShortPath : String) : String; -> WinExpandLongPathName
   (commented out in the original code) */

static bool AllSameDir(TStringList* St)
{
    int I;
    QString S;
    if (St->Count() < 2) return false; // {No benefit if less than 2 files to mount}
    S = Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ExtractFilePath((*St)[0]))));
    for (I = 1; I < St->Count(); I++) if (S != Trim(ExtUpperCase(IncludeTrailingPathDelimiter(ExtractFilePath((*St)[I]))))) return false;
    return true;
}

static QString MakeDOSBoxPath(QString Part, QString Root)
{
    QString S;
    int I;
    Root = IncludeTrailingPathDelimiter(Root);
    QString result = "";
    Part = ToDOSDelims(ExcludeTrailingPathDelimiter(Part));
    if ((!Part.isEmpty()) && (Part[0] == '\\')) Part = Copy(Part, 2, MaxInt);

    while (!Part.isEmpty()) {
        I = Pos("\\", Part);
        if (I == 0) { S = Part; Part = ""; } else { S = Copy(Part, 1, I - 1); Part = Copy(Part, I + 1, MaxInt); }
        S = WinExpandLongPathName(Root, S);
        result = result + "\\" + DOSBoxShortName(MakeAbsPath(Root, PrgSetup->BaseDir()), S);
        Root = Root + S + PathDelim;
    }

    if (result.isEmpty()) result = "\\";
    return result;
}

static QString SpecialMultiImgMount(const QString& DriveLetter, TStringList* Imgs, const QString& FreeDriveLetters, const QString& MountCommandAdd)
{
    int I;
    QChar TempDrive;
    QString Path, S;

    TempDrive = 'Y';
    for (I = Length(FreeDriveLetters); I >= 1; I--) if (FreeDriveLetters[I - 1] != '-') { TempDrive = FreeDriveLetters[I - 1]; break; }
    // {There must be free drive letters, because there are only 10 mounts}

    Path = IncludeTrailingPathDelimiter(ExtractFilePath(ShortName((*Imgs)[0])));

    S = QString("\"") + TempDrive + ":\\" + DOSBoxShortName(ExtractFilePath((*Imgs)[0]), ExtractFileName((*Imgs)[0])) + "\"";
    for (I = 1; I < Imgs->Count(); I++) S = S + " \"" + TempDrive + ":\\" + DOSBoxShortName(ExtractFilePath((*Imgs)[I]), ExtractFileName((*Imgs)[I])) + "\"";

    return
        QString("mount ") + TempDrive + " \"" + UnmapDrive(Path, ptMount) + "\"" + "\r" +
        "imgmount " + DriveLetter + " " + S + " " + MountCommandAdd + "\r" +
        "mount -u " + TempDrive;
}

// CD drives are identified by their root folders ("D:\" on Windows, the
// mount point of the medium on Linux/BSD - see PhysicalDrivesUnit)
static bool SpecialCDDriveMounting(QString& Folder, const QString& DriveLetter, const QString& ProfileName, bool SelectCD, QStringList& SpecialMountedCDDrives)
{
    QString S, T, U;
    QStringList Drives;
    int I;
    bool result = true;
    S = Trim(ExtUpperCase(Folder));

    // {ASK /  NUMBER:x /  LABEL:x / FILE:x / FOLDER:x}

    if (S == "ASK") {
        Drives = CDDriveRoots();
        if (Drives.isEmpty()) {
            if (SelectCD) {
                MessageDlg(Format(LanguageSetup->MessageNoCDDriveMount(), {DriveLetter}), mtError, mbOK, 0);
                result = false;
            }
            Folder = ""; return result;
        }
        if ((Drives.size() == 1) || (!SelectCD)) { Folder = Drives[0]; SpecialMountedCDDrives << NormalizedDriveRoot(Drives[0]); return result; }
        result = ShowSelectCDDriveToMountDialog(ApplicationMainForm(), Drives, DriveLetter, ProfileName, Folder);
        if (result && !Folder.isEmpty()) SpecialMountedCDDrives << NormalizedDriveRoot(Folder);
        return result;
    }

    if (Pos(":", S) == 0) return result;
    // (original case - file and folder names are case sensitive on Linux; the
    // label is compared case insensitive)
    T = Trim(Copy(Folder, Pos(":", S) + 1, MaxInt));
    S = Copy(S, 1, Pos(":", S) - 1);

    Drives = CDDriveRoots();
    if (Drives.isEmpty()) {
        if ((SelectCD && (S == "LABEL")) || (S == "FILE") || (S == "FOLDER")) {
            MessageDlg(Format(LanguageSetup->MessageNoCDDriveMount(), {DriveLetter}), mtError, mbOK, 0);
            result = false;
            Folder = ""; return result;
        }
    }

    if (S == "NUMBER") {
        if (!TryStrToInt(T, I)) I = 1;
        // Port note: the Delphi code accesses Drives[1] even if there is no CD drive at all
        if (Drives.isEmpty()) { Folder = ""; return result; }
        I = Max(1, Min(int(Drives.size()), I));
        SpecialMountedCDDrives << NormalizedDriveRoot(Drives[I - 1]);
        Folder = Drives[I - 1]; return result;
    }
    if (SelectCD) {
        TMountByData MountBy;
        if (S == "LABEL") MountBy = mbLabel; else if (S == "FILE") MountBy = mbFile; else if (S == "FOLDER") MountBy = mbFolder; else return result;
        result = ShowSelectCDDriveToMountByDataDialog(ApplicationMainForm(), MountBy, T, DriveLetter, ProfileName, U);
        if (result) { Folder = U; SpecialMountedCDDrives << NormalizedDriveRoot(U); } else Folder = "";
        return result;
    }
    return result;
}

static QString BuildMountData(const QString& ProfString, double DOSBoxVersion, QString& FreeDriveLetters, const QString& ProfileName, bool SelectCD, bool& OK, QStringList& SpecialMountedCDDrives, bool BuildForArchivePackage)
{
    auto MarkDriveUsed = [&FreeDriveLetters](QString S) {
        S = Trim(ExtUpperCase(S)); if (Length(S) != 1) return;
        if ((S[0] < 'A') || (S[0] > 'Z')) return;
        FreeDriveLetters[S[0].unicode() - 'A'] = '-';
    };
    TStringList *St, *St2;
    QString S, T, U, V;
    int I;
    QString result = ""; OK = true;

    St = ValueToList(ProfString);
    St2 = new TStringList();
    TFinally Cleanup{[&]() { delete St; delete St2; }};

    if (St->Count() < 3) return result;

    S = Trim((*St)[0]);
    T = Trim(ExtUpperCase((*St)[1]));
    I = Pos("$", S);
    while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos("$", S); }
    St2->Add(S);
    for (I = 0; I < St2->Count(); I++) {
        S = Trim(ExtUpperCase((*St2)[I]));
        if ((T == "CDROM") && ((S == "ASK") || (Copy(S, 1, 7) == "NUMBER:") || (Copy(S, 1, 6) == "LABEL:") || (Copy(S, 1, 5) == "FILE:") || (Copy(S, 1, 7) == "FOLDER:"))) continue;
        (*St2)[I] = MakeAbsPath((*St2)[I], PrgSetup->BaseDir());
    }
    S = (*St2)[0];

    // {general: RealFolder;Type;Letter;IO;Label;FreeSpace}
    if (St->Count() == 3) St->Add("false");
    if (St->Count() == 4) St->Add("");
    if (St->Count() == 5) St->Add("");

    if (T == "DRIVE") {
        // {RealFolder;DRIVE;Letter;False;;FreeSpace}
        MarkDriveUsed((*St)[2]);
        U = UnmapDrive(ShortName(S), ptMount);
        if (BuildForArchivePackage) U = MakeRelPath(U, PrgDataDir());
        result = "mount " + (*St)[2] + " \"" + U + "\"";
        if ((St->Count() >= 6) && (!(*St)[5].isEmpty())) result = result + " -freesize " + (*St)[5];
    }

    if (T == "FLOPPY") {
        // {RealFolder;FLOPPY;Letter;False;;}
        MarkDriveUsed((*St)[2]);
        U = UnmapDrive(ShortName(S), ptMount);
        if (BuildForArchivePackage) U = MakeRelPath(U, PrgDataDir());
        result = "mount " + (*St)[2] + " \"" + U + "\" -t floppy";
    }

    if (T == "CDROM") {
        // {RealFolder;CDROM;Letter;IO;Label; (ASK /  NUMBER:x /  LABEL:x / FILE:x / FOLDER:x)}
        if (!SpecialCDDriveMounting(S, (*St)[2], ProfileName, SelectCD, SpecialMountedCDDrives)) { OK = false; return result; }
        MarkDriveUsed((*St)[2]);
        U = UnmapDrive(ShortName(S), ptMount);
        if (BuildForArchivePackage) U = MakeRelPath(U, PrgDataDir());
        result = "mount " + (*St)[2] + " \"" + U + "\" -t cdrom";
        if (Trim(UpperCase((*St)[3])) == "TRUE") result = result + " -IOCTL";
        if (Trim(UpperCase((*St)[3])) == "NOIOCTL") result = result + " -NOIOCTL";
        if (Trim(UpperCase((*St)[3])) == "DX") result = result + " -IOCTL_DX";
        if (Trim(UpperCase((*St)[3])) == "DIO") result = result + " -IOCTL_DIO";
        if (Trim(UpperCase((*St)[3])) == "MCI") result = result + " -IOCTL_MCI";
        if (!(*St)[4].isEmpty()) result = result + " -label " + (*St)[4];
    }

    if (T == "FLOPPYIMAGE") {
        // {ImageFile;FLOPPYIMAGE;Letter;;;}
        if ((Trim((*St)[2]) == "0") || (Trim((*St)[2]) == "1")) U = "none"; else U = "fat";
        MarkDriveUsed((*St)[2]);
        if (AllSameDir(St2)) {
            result = SpecialMultiImgMount((*St)[2], St2, FreeDriveLetters, "-t floppy -fs " + U);
        } else {
            S = "\"" + ShortName((*St2)[0]) + "\""; for (I = 1; I < St2->Count(); I++) S = S + " \"" + ShortName((*St2)[I]) + "\"";
            result = "imgmount " + (*St)[2] + " " + S + " -t floppy -fs " + U;
        }
    }

    if (T == "CDROMIMAGE") {
        // {ImageFile;CDROMIMAGE;Letter;;;}
        MarkDriveUsed((*St)[2]);
        if (AllSameDir(St2)) {
            if (DOSBoxVersion > 0.73) {
                result = SpecialMultiImgMount((*St)[2], St2, FreeDriveLetters, "-t cdrom");
            } else {
                result = SpecialMultiImgMount((*St)[2], St2, FreeDriveLetters, "-t iso -fs iso");
            }
        } else {
            S = "\"" + ShortName((*St2)[0]) + "\""; for (I = 1; I < St2->Count(); I++) S = S + " \"" + ShortName((*St2)[I]) + "\"";
            if (DOSBoxVersion > 0.73) {
                result = "imgmount " + (*St)[2] + " " + S + " -t cdrom";
            } else {
                result = "imgmount " + (*St)[2] + " " + S + " -t iso -fs iso";
            }
        }
    }

    if (T == "IMAGE") {
        // {ImageFile;IMAGE;LetterOR23;;;geometry}
        if (St->Count() >= 6) {
            if ((Trim((*St)[2]) == "2") || (Trim((*St)[2]) == "3")) U = "none"; else U = "fat";
            MarkDriveUsed((*St)[2]);
            result = "imgmount " + (*St)[2] + " \"" + ShortName(S) + "\" -t hdd -fs " + U + " -size " + (*St)[5];
        }
    }

    if (T == "PHYSFS") {
        // {RealFolder$ZipFile;PHYSFS;Letter;False;;FreeSpace}
        if (St2->Count() == 2) {
            MarkDriveUsed((*St)[2]);
            CreateDir(MakeAbsPath((*St2)[0], PrgSetup->BaseDir()));
            U = UnmapDrive(ShortName((*St2)[0]), ptMount);
            V = UnmapDrive(ShortName((*St2)[1]), ptMount);
            if (BuildForArchivePackage) U = MakeRelPath(U, PrgDataDir());
            if (BuildForArchivePackage) V = MakeRelPath(V, PrgDataDir());
            result = "mount " + (*St)[2] + " \"" + U + ":" + V + ":/\"";
            if ((St->Count() >= 6) && (!(*St)[5].isEmpty())) result = result + " -freesize " + (*St)[5];
        }
    }

    if (T == "ZIP") {
        // {RealFolder$ZipFile;ZIP;Letter;False;;FreeSpace;DeleteMode(no;files;folder)}
        if (St2->Count() == 2) {
            MarkDriveUsed((*St)[2]);
            U = UnmapDrive(ShortName((*St2)[0]), ptMount);
            if (BuildForArchivePackage) U = MakeRelPath(U, PrgDataDir());
            result = "mount " + (*St)[2] + " \"" + U + "\"";
            if ((St->Count() >= 6) && (!(*St)[5].isEmpty())) result = result + " -freesize " + (*St)[5];
        }
    }

    return result;
}

static void TempMountFreeDOSDir(TGame* Game, const QString& FreeDriveLetters, QString& UseDir, QString& Mount, QString& UnMount)
{
    QString FreeDOSPath;
    int I;
    TStringList* St;
    QString S;
    QChar TempDrive;

    UseDir = ""; Mount = ""; UnMount = "";

    FreeDOSPath = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir()));
    if (!DirectoryExists(FreeDOSPath)) return;

    UseDir = "";
    for (I = 0; I < Game->NrOfMounts(); I++) {
        St = ValueToList(Game->Mount(I));
        // (the Delphi code reads St[2] without checking the number of entries)
        S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(St->Count() > 0 ? (*St)[0] : QString()), PrgSetup->BaseDir()));
        if (ExtUpperCase(Copy(FreeDOSPath, 1, Length(S))) == ExtUpperCase(S) && St->Count() > 2) {
            UseDir = (*St)[2] + ":\\" + ToDOSDelims(Copy(FreeDOSPath, Length(S) + 1, MaxInt));
            delete St;
            break;
        }
        delete St;
    }
    if (UseDir.isEmpty()) {
        TempDrive = 'Y';
        for (I = Length(FreeDriveLetters); I >= 1; I--) if (FreeDriveLetters[I - 1] != '-') { TempDrive = FreeDriveLetters[I - 1]; break; }
        // {There must be free drive letters, because there are only 10 mounts}

        Mount = QString("mount ") + TempDrive + ": \"" + UnmapDrive(ShortName(FreeDOSPath), ptMount) + "\"";
        UseDir = QString("") + TempDrive + ":\\";
        UnMount = QString("mount -u ") + TempDrive;
    }
}

static QString MakePathShort(const QString& S)
{
#ifdef Q_OS_WIN
    wchar_t Temp[MAX_PATH + 10];
    QString L = ToNativePath(S);
    DWORD Len = GetShortPathNameW(reinterpret_cast<LPCWSTR>(L.utf16()), Temp, MAX_PATH);
    if (Len != 0 && Len < MAX_PATH) {
        return Trim(ExtUpperCase(QString::fromWCharArray(Temp, static_cast<int>(Len))));
    } else {
        return S;
    }
#else
    // Port note: no short (8.3) path names outside Windows (GetShortPathName fails -> S)
    return S;
#endif
}

static void BuildLocalRunCommands(TStringList* St, const QString& ProgramFile, const QString& ProgramParameters, bool UseLoadFix, int LoadFixMemory, bool Use4DOS, bool UseDOS32A, TGame* Game, const QString& FreeDriveLetters, bool WarnIfNotReachable)
{
    Q_UNUSED(ProgramParameters); Q_UNUSED(UseLoadFix); Q_UNUSED(LoadFixMemory); Q_UNUSED(Use4DOS);
    Q_UNUSED(UseDOS32A); Q_UNUSED(Game); Q_UNUSED(FreeDriveLetters); Q_UNUSED(WarnIfNotReachable);
    QString T;
    T = ToDOSDelims(MakePathShort(Trim(ExtractFilePath(ProgramFile))));

    if ((Length(T) >= 2) && (T[1] == ':')) {
        St->Add(Copy(T, 1, 2));
        St->Add("cd\\");
        T = Trim(Copy(T, 3, MaxInt));
    }

    if (!T.isEmpty()) {
        if (T[0] != '\\') T = "\\" + T;
        if (T[Length(T) - 1] == '\\') T.chop(1);
        St->Add("cd " + T);
    }
}

static bool MountQBasic(TStringList* Autoexec, TGame* Game, const QString& FreeDriveLetters, QString& QBasicPathAndFile)
{
    QString QBFile, S, T, Path, Drive;
    int I;
    TStringList* St;

    QBasicPathAndFile = "QBasic.exe";

    QBFile = Trim(PrgSetup->QBasic());
    if ((QBFile.isEmpty()) || (!FileExists(QBFile))) return false;

    T = MakePathShort(Trim(ExtractFilePath(QBFile)));
    Path = ""; Drive = "";
    for (I = 0; I < Game->NrOfMounts(); I++) {
        St = ValueToList(Game->Mount(I));
        TFinally Cleanup{[St]() { delete St; }};
        if (St->Count() == 3) St->Add("false");
        if (St->Count() == 4) St->Add("");
        if (St->Count() < 5) continue;
        if ((Trim(ExtUpperCase((*St)[1])) != "DRIVE") && (Trim(ExtUpperCase((*St)[1])) != "CDROM") && (Trim(ExtUpperCase((*St)[1])) != "FLOPPY")) continue;
        S = MakePathShort(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[0]), PrgSetup->BaseDir()));

        if (Copy(T, 1, Length(S)) == S) {
            S = Copy(T, Length(S) + 1, MaxInt);
            if ((Drive.isEmpty()) || (Length(S) < Length(Path))) { Path = S; Drive = (*St)[2] + ":"; }
        }
    }
    if (Drive.isEmpty()) {
        for (I = Length(FreeDriveLetters); I >= 1; I--) if (FreeDriveLetters[I - 1] != '-') {
            Autoexec->Add(QString("mount ") + FreeDriveLetters[I - 1] + " " + UnmapDrive(ExtractFilePath(T), ptMount));
            QBasicPathAndFile = QString(FreeDriveLetters[I - 1]) + ":\\" + ExtractFileName(QBFile);
            break;
        }
    } else {
        Path = ToDOSDelims(Path);
        if ((!Path.isEmpty()) && (Path[0] == '\\')) Path = Copy(Path, 2, MaxInt);
        QBasicPathAndFile = IncludeTrailingDOSDelimiter(Drive + "\\" + Path) + ExtractFileName(QBFile);
    }

    return true;
}

static bool MountUserInterpreter(TStringList* Autoexec, TGame* Game, const QString& FreeDriveLetters, QString& InterpreterPathAndFile, int InterpreterNr)
{
    QString InterpreterFile, S, T, Path, Drive;
    int I;
    TStringList* St;

    if (InterpreterNr >= PrgSetup->DOSBoxBasedUserInterpretersPrograms()->Count()) return false;

    InterpreterPathAndFile = ExtractFileName((*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[InterpreterNr]);

    InterpreterFile = Trim(MakeAbsPath((*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[InterpreterNr], PrgSetup->BaseDir()));
    if ((InterpreterFile.isEmpty()) || (!FileExists(InterpreterFile))) return false;

    T = MakePathShort(Trim(ExtractFilePath(InterpreterFile)));
    Path = ""; Drive = "";
    for (I = 0; I < Game->NrOfMounts(); I++) {
        St = ValueToList(Game->Mount(I));
        TFinally Cleanup{[St]() { delete St; }};
        if (St->Count() == 3) St->Add("false");
        if (St->Count() == 4) St->Add("");
        if (St->Count() < 5) continue;
        if ((Trim(ExtUpperCase((*St)[1])) != "DRIVE") && (Trim(ExtUpperCase((*St)[1])) != "CDROM") && (Trim(ExtUpperCase((*St)[1])) != "FLOPPY")) continue;
        S = MakePathShort(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[0]), PrgSetup->BaseDir()));

        if (Copy(T, 1, Length(S)) == S) {
            S = Copy(T, Length(S) + 1, MaxInt);
            if ((Drive.isEmpty()) || (Length(S) < Length(Path))) { Path = S; Drive = (*St)[2] + ":"; }
        }
    }
    if (Drive.isEmpty()) {
        for (I = Length(FreeDriveLetters); I >= 1; I--) if (FreeDriveLetters[I - 1] != '-') {
            Autoexec->Add(QString("mount ") + FreeDriveLetters[I - 1] + " " + UnmapDrive(ExtractFilePath(T), ptMount));
            InterpreterPathAndFile = QString(FreeDriveLetters[I - 1]) + ":\\" + ExtractFileName(InterpreterFile);
            break;
        }
    } else {
        Path = ToDOSDelims(Path);
        if ((!Path.isEmpty()) && (Path[0] == '\\')) Path = Copy(Path, 2, MaxInt);
        InterpreterPathAndFile = IncludeTrailingDOSDelimiter(Drive + "\\" + Path) + ExtractFileName(InterpreterFile);
    }

    return true;
}

static int RunViaUserInterpreter(const QString& ProgramFile)
{
    int I, J;
    QString Ext;
    TStringList* St;

    Ext = Trim(ExtUpperCase(ExtractFileExt(ProgramFile)));
    if ((!Ext.isEmpty()) && (Ext[0] == '.')) Ext = Copy(Ext, 2, MaxInt);

    for (I = 0; I < PrgSetup->DOSBoxBasedUserInterpretersExtensions()->Count(); I++) {
        St = ValueToList((*PrgSetup->DOSBoxBasedUserInterpretersExtensions())[I]);
        for (J = 0; J < St->Count(); J++) if (Trim(ExtUpperCase((*St)[J])) == Ext) { delete St; return I; }
        delete St;
    }
    return -1;
}

static bool BuildRunCommands(TStringList* St, const QString& ProgramFile, const QString& ProgramParameters, bool UseLoadFix, int LoadFixMemory, bool Use4DOS, bool UseDOS32A, TGame* Game, const QString& FreeDriveLetters, bool WarnIfNotReachable, bool WarnIfWindowsExe)
{
    QString Prefix, S, T, UsePath, Mount, Unmount;
    int I;
    TStringList* St2;
    bool NoFreeDOS;
    QString Path, Drive, RootPath, RootPathTemp;
    bool B;
    bool result = true;

    // {Find path to FreeDOS and mount it if needed}
    SpeedTestInfo("Find path to FreeDOS and mount it if needed");
    TempMountFreeDOSDir(Game, FreeDriveLetters, UsePath, Mount, Unmount);
    NoFreeDOS = (UsePath.isEmpty());
    SpeedTestInfoOnly("FreeDOS path inside DOSBox: " + UsePath + " mount command (only if needed): " + Mount);
    if (Use4DOS || UseDOS32A) {
        if (!Mount.isEmpty()) St->Add(Mount);
    }

    S = Trim(ExtUpperCase(ProgramFile));
    if ((Copy(S, 1, 7) == "DOSBOX:") && (Length(S) > 7)) {
        // {No path to file needed, because file is in image etc. and given path is local to DOSBox directory structure}
        SpeedTestInfo("Build run command for DOSBox local file");
        BuildLocalRunCommands(St, Trim(Copy(Trim(ProgramFile), 8, MaxInt)), ProgramParameters, UseLoadFix, LoadFixMemory, Use4DOS, UseDOS32A, Game, FreeDriveLetters, WarnIfNotReachable);
    } else {
        // {Find path to file}
        SpeedTestInfo("Find file to run");
        if ((!Trim(ProgramFile).isEmpty()) && (!FileExists(MakeAbsPath(ProgramFile, PrgSetup->BaseDir()))) && WarnIfNotReachable) {
            ApplicationRestore();
            MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {MakeAbsPath(ProgramFile, PrgSetup->BaseDir())}), mtError, mbOK, 0);
            result = false;
            return result;
        }

        SpeedTestInfo("Windows file check");
        if (IsWindowsExe(MakeAbsPath(ProgramFile, PrgSetup->BaseDir())) && WarnIfNotReachable && WarnIfWindowsExe) {
            if (!Game->IgnoreWindowsFileWarnings()) {
                ApplicationRestore();
                B = false;
                ShowWindowsFileWarningDialog(ApplicationMainForm(), MakeAbsPath(ProgramFile, PrgSetup->BaseDir()), B);
                if (B) { Game->SetIgnoreWindowsFileWarnings(true); Game->StoreAllValues(); }
            }
        }

        T = MakePathShort(Trim(ExtractFilePath(MakeAbsPath(ProgramFile, PrgSetup->BaseDir()))));

        SpeedTestInfo("Build directory change commands to select program direcotry");
        if ((!Trim(ProgramFile).isEmpty()) && (!T.isEmpty())) {
            Path = ""; Drive = ""; RootPath = "";
            for (I = 0; I < Game->NrOfMounts(); I++) {
                St2 = ValueToList(Game->Mount(I));
                TFinally Cleanup{[St2]() { delete St2; }};
                if (St2->Count() == 3) St2->Add("false");
                if (St2->Count() == 4) St2->Add("");
                if (St2->Count() < 5) continue;
                if ((Trim(ExtUpperCase((*St2)[1])) != "DRIVE") && (Trim(ExtUpperCase((*St2)[1])) != "CDROM") && (Trim(ExtUpperCase((*St2)[1])) != "FLOPPY")) continue;
                S = MakePathShort(MakeAbsPath(IncludeTrailingPathDelimiter((*St2)[0]), PrgSetup->BaseDir()));

                if (Copy(T, 1, Length(S)) == S) {
                    RootPathTemp = LongPath(S);
                    S = Copy(T, Length(S) + 1, MaxInt);
                    if ((Drive.isEmpty()) || (Length(S) < Length(Path))) { Path = S; Drive = (*St2)[2] + ":"; RootPath = RootPathTemp; }
                }
            }
            if (!Drive.isEmpty()) {
                St->Add(Drive);
                St->Add("cd\\");
                S = ToDOSDelims(Path);
                if (!S.isEmpty()) {
                    if (S[0] != '\\') S = "\\" + S;
                    S = MakeDOSBoxPath(S, RootPath);
                    St->Add("cd " + S);
                }
            } else {
                St->Add("Rem !!! The program file is not reachable via any mounted drive. !!!");
                St->Add("Rem !!! Trying to interpret the path to the file as a DOSBox relative path. !!!");
                BuildLocalRunCommands(St, Trim(ProgramFile), ProgramParameters, UseLoadFix, LoadFixMemory, Use4DOS, UseDOS32A, Game, FreeDriveLetters, WarnIfNotReachable);
                if (WarnIfNotReachable) {
                    ApplicationRestore();
                    MessageDlg(Format(LanguageSetup->MessageNoMountToReachFolder(), {ExtractFilePath(MakeAbsPath(ProgramFile, PrgSetup->BaseDir()))}), mtError, mbOK, 0);
                    result = false;
                }
            }
        }
    }

    if (UseLoadFix) Prefix = "loadfix -" + IntToStr(LoadFixMemory) + " "; else Prefix = "";

    SpeedTestInfo("Run via user interpreter check");
    I = RunViaUserInterpreter(ProgramFile);
    if (I >= 0) {
        SpeedTestInfoOnly("  User interpreter " + IntToStr(I) + " (zero based)");
        // {Run via user defined interpreter}
        SpeedTestInfo("Try to mount user interpreter directory");
        SpeedTestInfoOnly("Interpreter: " + S);
        if (!MountUserInterpreter(St, Game, FreeDriveLetters, S, I)) {
            if (WarnIfNotReachable) {
                ApplicationRestore();
                QString IP = (I < PrgSetup->DOSBoxBasedUserInterpretersPrograms()->Count()) ? (*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[I] : QString();
                MessageDlg(Format(LanguageSetup->MessageUserInterpreterNeededToExecuteFile(), {IP, ExtractFileName(ProgramFile)}), mtError, mbOK, 0);
            }
        }
        SpeedTestInfo("Build run command");
        if (PrgSetup->DOSBoxBasedUserInterpretersParameters()->Count() > I) T = Trim((*PrgSetup->DOSBoxBasedUserInterpretersParameters())[I]); else T = "";
        if (T.isEmpty()) T = "%s";
        try { T = Format(T, {ExtractFileName(ProgramFile)}); } catch (...) { T = Format("%s", {ExtractFileName(ProgramFile)}); }
        T = S + " " + T;
    } else {
        if (ExtUpperCase(ExtractFileExt(ProgramFile)) == ".BAS") {
            // {Run via QBasic}
            SpeedTestInfo("Try to mount QBasic");
            if (!MountQBasic(St, Game, FreeDriveLetters, S)) {
                if (WarnIfNotReachable) {
                    ApplicationRestore();
                    MessageDlg(Format(LanguageSetup->MessageQBasicNeededToExecuteFile(), {ExtractFileName(ProgramFile)}), mtError, mbOK, 0);
                }
            }
            SpeedTestInfo("Build run command");
            T = Trim(PrgSetup->QBasicParam()); if (T.isEmpty()) T = "/run %s";
            try { T = Format(T, {ExtractFileName(ProgramFile)}); } catch (...) { T = Format("/run %s", {ExtractFileName(ProgramFile)}); }
            T = S + " " + T;
        } else {
            // {Just run in DOSBox}
            SpeedTestInfo("Build run command");
            if ((!UseLoadFix) && (!Use4DOS) && (!UseDOS32A) && (Trim(ExtUpperCase(ExtractFileExt(ProgramFile))) == ".BAT")) Prefix = "call ";

            if ((!T.isEmpty()) && (!Trim(ProgramParameters).isEmpty())) T = " " + ProgramParameters; else T = "";
            if (Copy(Trim(ExtUpperCase(ProgramFile)), 1, 7) == "DOSBOX:") {
                S = ExtractFileName(ProgramFile);
            } else {
                SpeedTestInfo("Make DOSBox short path name");
                S = MakeDOSBoxPath(ExtractFileName(ProgramFile), ExtractFilePath(ProgramFile));
                if ((!S.isEmpty()) && (S[0] == '\\')) S = Copy(S, 2, MaxInt);
            }
            T = S + T;

            SpeedTestInfo("Optionally add DOS32A or 4DOS commands");

            if (UseDOS32A && (!NoFreeDOS)) {
                St->Add(UsePath + "DOS32A.EXE " + T);
                SpeedTestInfoOnly("Command: " + UsePath + "DOS32A.EXE " + T);
                return result;
            }

            if (Use4DOS && (!NoFreeDOS)) {
                if (!T.isEmpty()) T = " /C " + T;
                St->Add(UsePath + "4DOS.COM" + T);
                SpeedTestInfoOnly("Command: " + UsePath + "4DOS.COM" + T);
                return result;
            }
        }
    }

    if (Game->SecureMode()) St->Add("Z:\\config.com -securemode > nul");

    St->Add(Prefix + T);
    return result;
}

static void MultiLineAdd(TStringList* St, QString NewLines)
{
    int I;
    I = Pos("\r", NewLines);
    while (I != 0) {
        St->Add(Copy(NewLines, 1, I - 1)); NewLines = Copy(NewLines, I + 1, MaxInt);
        I = Pos("\r", NewLines);
    }
    St->Add(NewLines);
}

static void BuildFloppyBoot(TStringList* Autoexec, QString Images, const QString& FreeDriveLetters, bool BuildForArchivePackage)
{
    TStringList* Imgs;
    int I;
    QChar TempDrive;
    QString Path, S, T;

    Imgs = new TStringList();
    I = Pos("$", Images);
    while (I != 0) { Imgs->Add(MakeAbsPath(Copy(Images, 1, I - 1), PrgSetup->BaseDir())); Images = Copy(Images, I + 1, MaxInt); I = Pos("$", Images); }
    Imgs->Add(MakeAbsPath(Images, PrgSetup->BaseDir()));

    if (AllSameDir(Imgs)) {
        TempDrive = 'Y';
        for (I = Length(FreeDriveLetters); I >= 1; I--) if (FreeDriveLetters[I - 1] != '-') { TempDrive = FreeDriveLetters[I - 1]; break; }
        // {There must be free drive letters, because there are only 10 mounts}

        Path = IncludeTrailingPathDelimiter(ExtractFilePath(ShortName((*Imgs)[0])));

        S = QString("\"") + TempDrive + ":\\" + ExtractFileName(ShortName((*Imgs)[0])) + "\"";
        for (I = 1; I < Imgs->Count(); I++) S = S + " \"" + TempDrive + ":\\" + ExtractFileName(ShortName((*Imgs)[I])) + "\"";

        T = UnmapDrive(Path, ptMount);
        if (BuildForArchivePackage) T = MakeRelPath(T, PrgDataDir());
        Autoexec->Add(QString("mount ") + TempDrive + " \"" + T + "\"");
        Autoexec->Add("boot " + S);
    } else {

        S = "\"" + IncludeTrailingPathDelimiter(ExtractFilePath(ShortName((*Imgs)[0]))) + ExtractFileName(ShortName((*Imgs)[0])) + "\"";
        for (I = 1; I < Imgs->Count(); I++) S = S + " \"" + IncludeTrailingPathDelimiter(ExtractFilePath(ShortName((*Imgs)[I]))) + ExtractFileName(ShortName((*Imgs)[I])) + "\"";
        Autoexec->Add("boot " + S);
    }
    delete Imgs;
}

static void AutoMountCDs(TStringList* St, TGame* Game, double DOSBoxVersion, QString& FreeDriveLetters, QStringList& SpecialMountedCDDrives, bool BuildForArchivePackage)
{
    QString S, T;
    int I;
    TStringList* St2;
    bool OK;
    QStringList TDummy;

    // {SpecialMountedCDDrives <- mounted by ASK, NUMBER, LABEL, FOLDER or FILE (not detectable my scanning the mount list)}
    QStringList AlreadyMounted = SpecialMountedCDDrives;

    // {Check which CD drives are mounted regularly}
    for (I = 0; I <= Game->NrOfMounts(); I++) {
        S = Game->Mount(I);
        St2 = ValueToList(S);
        TFinally Cleanup{[St2]() { delete St2; }};
        if (St2->Count() < 2) continue;
        if (Trim(ExtUpperCase((*St2)[1])) != "CDROM") continue;
        S = Trim((*St2)[0]);
        if (S.isEmpty()) continue;
#ifdef Q_OS_WIN
        S = ExtUpperCase(S);
        if ((S[0] < 'A') || (S[0] > 'Z')) continue;
        if (Length(S) > 3) continue;
        if ((Length(S) >= 2) && (S[1] != ':')) continue;
        if ((Length(S) >= 3) && (S[1] != '\\')) continue;
        S = QString(S[0]);
#endif
        AlreadyMounted << NormalizedDriveRoot(S);
    }

    // {Find CDs and autmount them}
    for (const TPhysicalDrive& Drive : PhysicalDrives(pdtOptical)) {
        if (Drive.Root.isEmpty()) continue;   // no medium (Linux: not mounted)
        if (AlreadyMounted.contains(NormalizedDriveRoot(Drive.Root))) continue;
#ifdef Q_OS_WIN
        const QChar C = Drive.Letter;
        DWORD lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters;
        if (!GetDiskFreeSpaceW(reinterpret_cast<LPCWSTR>(Drive.Root.utf16()), &lpSectorsPerCluster, &lpBytesPerSector, &lpNumberOfFreeClusters, &lpTotalNumberOfClusters)) continue;
        if (lpTotalNumberOfClusters == 0) continue;
        if (Pos(C, FreeDriveLetters) > 0) S = C; else {
            S = "";
            for (char D = char(C.unicode()) + 1; D <= 'Y'; D++) if (Pos(QChar(D), FreeDriveLetters) > 0) { S = QChar(D); break; }
            if (S.isEmpty()) for (char D = 'A'; D <= char(C.unicode()) - 1; D++) if (Pos(QChar(D), FreeDriveLetters) > 0) { S = QChar(D); break; }
        }
#else
        // (the DOS drive letter: the first free letter from D on)
        S = "";
        for (char D = 'D'; D <= 'Y'; D++) if (Pos(QChar(D), FreeDriveLetters) > 0) { S = QChar(D); break; }
#endif
        if (S.isEmpty()) continue;
        S = Drive.Root + ";CDROM;" + S + ";TRUE;;";
        St->Add(BuildMountData(S, DOSBoxVersion, FreeDriveLetters, "", true, OK, TDummy, BuildForArchivePackage)); // {"''", "OK" and "T" can be ignored; only for user interactive mounting}
    }
}

static double GetDOSBoxVersionOfGame(TGame* Game)
{
    QString S;
    int DOSBoxNr;
    double DOSBoxVersion;
    DOSBoxNr = Max(0, GetDOSBoxNr(Game));
    if (DOSBoxNr >= 0) S = CheckDOSBoxVersion(DOSBoxNr); else S = CheckDOSBoxVersion(-1, Game->CustomDOSBoxDir());
    if (S.isEmpty())
        DOSBoxVersion = MinSupportedDOSBoxVersion;
    else {
        try { DOSBoxVersion = StrToFloatEx(S); } catch (...) { DOSBoxVersion = MinSupportedDOSBoxVersion; }
    }
    return DOSBoxVersion;
}

bool BuildAutoexec(TGame* Game, bool RunSetup, TStringList* St, bool WarnIfNotReachable, int RunExtraFile, bool WarnIfWindowsExe, bool SelectCD, bool BuildForArchivePackage)
{
    auto SetVolume = [St](const QString& Channel, int Left, int Right) {
        if ((Left != 100) || (Right != 100)) St->Add("mixer " + Channel + " " + IntToStr(Left) + ":" + IntToStr(Right) + " /NOSHOW");
    };
    QString S, T, U, NumCommands, MouseCommands, UsePath, Mount, UnMount;
    int I;
    bool B;
    QString FreeDriveLetters;
    TStringList* St2;
    int DOSBoxNr;
    bool OK;
    QStringList SpecialMountedCDDrives;
    double DOSBoxVersion;

    bool result = true;

    // (DOSBoxNr:=Max(0,GetDOSBoxNr(Game)); If DOSBoxNr>=0 then S:=CheckDOSBoxVersion(DOSBoxNr) else ...)
    DOSBoxVersion = GetDOSBoxVersionOfGame(Game);

    St->Add("");
    St->Add("[autoexec]");
    St->Add("@echo off");

    // { Environment variables }

    SpeedTestInfo("Adding environment variables to [autoexec] section of DOSBox conf file");

    St2 = StringToStringList(Game->Environment());
    for (I = 0; I < St2->Count(); I++) if (!Trim((*St2)[I]).isEmpty()) St->Add("SET " + (*St2)[I]);
    delete St2;

    // { Keyboard layout }

    SpeedTestInfo("Adding keyboard settings to [autoexec] section of DOSBox conf file");

    T = Trim(ExtUpperCase(Game->KeyboardLayout()));
    if ((T.isEmpty()) || (T == "DEFAULT")) {
        DOSBoxNr = Max(0, GetDOSBoxNr(Game));
        T = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->KeyboardLayout()));
        if ((T.isEmpty()) || (T == "DEFAULT")) T = LanguageSetup->GameKeyboardLayoutDefault();
    }
    if (Pos("(", T) > 0) {
        S = Copy(T, Pos("(", T) + 1, MaxInt);
        if (Pos(")", S) > 0) { S = Trim(Copy(S, 1, Pos(")", S) - 1)); if (!S.isEmpty()) T = S; }
    }

    S = Trim(ExtUpperCase(Game->Codepage()));
    if ((S.isEmpty()) || (S == "DEFAULT")) {
        DOSBoxNr = Max(0, GetDOSBoxNr(Game));
        S = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->Codepage()));
        if ((S.isEmpty()) || (S == "DEFAULT")) S = LanguageSetup->GameKeyboardCodepageDefault();
    }
    if (Pos("(", S) > 0) {
        S = Copy(S, 1, Pos("(", S) - 1);
    }

    if (ExtUpperCase(T) == "NONE") T = "none"; // {DOSBox keyb accepts GR and gr but not NONE}

    if (ExtUpperCase(T) == "AUTO") {
        DOSBoxNr = Max(0, GetDOSBoxNr(Game));
        T = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->KeyboardLayout()));
        if ((T.isEmpty()) || (T == "DEFAULT")) T = LanguageSetup->GameKeyboardLayoutDefault();
        if (Pos("(", T) > 0) {
            U = Copy(T, Pos("(", T) + 1, MaxInt);
            if (Pos(")", U) > 0) { U = Trim(Copy(U, 1, Pos(")", U) - 1)); if (!U.isEmpty()) T = U; }
        }
    }

    St->Add("keyb " + T + " " + S /* +' > nul' */); // {no "> nul" to display possible error message if layout and codepage do not match}

    // { Reported DOS version }

    SpeedTestInfo("Adding reported DOS version settings to [autoexec] section of DOSBox conf file");

    S = Trim(ExtUpperCase(Game->ReportedDOSVersion()));
    if ((!S.isEmpty()) && (S != "DEFAULT") && (S != "AUTO")) {
        if (Pos(".", S) != 0) { T = Trim(Copy(S, Pos(".", S) + 1, MaxInt)); S = Trim(Copy(S, 1, Pos(".", S) - 1)); } else { T = ""; }
        St->Add("ver set " + S + " " + T);
    }

    // { Text mode lines }

    SpeedTestInfo("Adding text mode settings to [autoexec] section of DOSBox conf file");

    if (PrgSetup->AllowTextModeLineChange()) {
        if (Game->TextModeLines() == 28) St->Add("Z:\\28.COM");
        if (Game->TextModeLines() == 50) St->Add("Z:\\50.COM");
    }

    // { IPX connect }

    SpeedTestInfo("Adding IPX init to [autoexec] section of DOSBox conf file");

    S = Trim(ExtUpperCase(Game->IPXType()));
    if (S == "CLIENT") St->Add("IPXNET CONNECT " + Game->IPXAddress() + " " + Game->IPXPort());
    if (S == "SERVER") St->Add("IPXNET STARTSERVER " + Game->IPXPort());

    // { Mounting }

    SpeedTestInfo("Adding mount commands to [autoexec] section of DOSBox conf file");

    FreeDriveLetters = "---DEFGHIJKLMNOPQRSTUVWXY-";
    SpecialMountedCDDrives.clear(); // {CD drives mounted by ASK, NUMBER, LABEL, FOLDER or FILE -> for AutoMountCDs to avoid double mounting}
    if (!Game->AutoexecOverrideMount()) {
        for (I = 0; I <= 9; I++) {
            if (Game->NrOfMounts() >= I + 1) {
                MultiLineAdd(St, BuildMountData(Game->Mount(I), DOSBoxVersion, FreeDriveLetters, Game->CacheName, SelectCD, OK, SpecialMountedCDDrives, BuildForArchivePackage));
                if (!OK) { result = false; return result; }
            } else {
                break;
            }
        }
        if (Game->AutoMountCDs()) AutoMountCDs(St, Game, DOSBoxVersion, FreeDriveLetters, SpecialMountedCDDrives, BuildForArchivePackage);
    }

    // { Mixer }

    SpeedTestInfo("Adding mixer settings to [autoexec] section of DOSBox conf file");

    SetVolume("MASTER", Game->MixerVolumeMasterLeft(), Game->MixerVolumeMasterRight());
    SetVolume("DISNEY", Game->MixerVolumeDisneyLeft(), Game->MixerVolumeDisneyRight());
    SetVolume("SPKR", Game->MixerVolumeSpeakerLeft(), Game->MixerVolumeSpeakerRight());
    SetVolume("GUS", Game->MixerVolumeGUSLeft(), Game->MixerVolumeGUSRight());
    SetVolume("SB", Game->MixerVolumeSBLeft(), Game->MixerVolumeSBRight());
    SetVolume("FM", Game->MixerVolumeFMLeft(), Game->MixerVolumeFMRight());
    SetVolume("CDAUDIO", Game->MixerVolumeCDLeft(), Game->MixerVolumeCDRight());

    // { Setting num, caps and scroll lock and CuteMouse }

    SpeedTestInfo("Adding num, caps, scroll and mouse settings to [autoexec] section of DOSBox conf file");

    NumCommands = "";
    S = Trim(ExtUpperCase(Game->NumLockStatus()));
    if ((S == "ON") || (S == "1") || (S == "TRUE")) NumCommands = "/N1";
    if ((S == "OFF") || (S == "0") || (S == "FALSE")) NumCommands = "/N0";
    S = Trim(ExtUpperCase(Game->CapsLockStatus()));
    if ((S == "ON") || (S == "1") || (S == "TRUE")) { if (!NumCommands.isEmpty()) NumCommands = NumCommands + " "; NumCommands = NumCommands + "/C1"; }
    if ((S == "OFF") || (S == "0") || (S == "FALSE")) { if (!NumCommands.isEmpty()) NumCommands = NumCommands + " "; NumCommands = NumCommands + "/C0"; }
    S = Trim(ExtUpperCase(Game->ScrollLockStatus()));
    if ((S == "ON") || (S == "1") || (S == "TRUE")) { if (!NumCommands.isEmpty()) NumCommands = NumCommands + " "; NumCommands = NumCommands + "/S1"; }
    if ((S == "OFF") || (S == "0") || (S == "FALSE")) { if (!NumCommands.isEmpty()) NumCommands = NumCommands + " "; NumCommands = NumCommands + "/S0"; }

    MouseCommands = "";
    if (Game->Force2ButtonMouseMode()) MouseCommands = "/Y";
    if (Game->SwapMouseButtons()) { if (!MouseCommands.isEmpty()) MouseCommands = MouseCommands + " "; MouseCommands = MouseCommands + "/L"; }

    SpeedTestInfo("Determining how to temporary mount the FreeDOS directory");

    TempMountFreeDOSDir(Game, FreeDriveLetters, UsePath, Mount, UnMount);
    SpeedTestInfoOnly("FreeDOS directory inside DOSBox: " + UsePath + " mount command (only if needed): " + Mount);

    if (((!NumCommands.isEmpty()) || (!MouseCommands.isEmpty())) && (!UsePath.isEmpty())) {
        if (!Mount.isEmpty()) St->Add(Mount);
        if (!NumCommands.isEmpty()) St->Add(UsePath + "4dos.com /C Keybd " + NumCommands);
        if (!MouseCommands.isEmpty()) St->Add(UsePath + "ctmouse " + MouseCommands);
        if (!UnMount.isEmpty()) St->Add(UnMount);
    }

    // { User defined Autoexec }

    SpeedTestInfo("Adding user defined autoexec lines to [autoexec] section of DOSBox conf file");

    St2 = StringToStringList(Game->Autoexec());
    B = false;
    if (St2->Count() > 0) for (I = 0; I <= Min(St2->Count() - 1, 2); I++) if (ExtUpperCase((*St2)[I]) == "ECHO.") { B = true; break; }
    if (!B) St->Add("echo.");
    St->AddStrings(*St2);
    delete St2;

    // { Run command }

    S = Trim(Game->AutoexecBootImage());
    if (!S.isEmpty()) {
        SpeedTestInfo("Building image boot command");
        if ((S == "2") || (S == "3")) {
            if (S == "2") St->Add("boot -l C"); else St->Add("boot -l D");
        } else {
            BuildFloppyBoot(St, S, FreeDriveLetters, BuildForArchivePackage);
        }
        SpeedTestInfoOnly("Command: " + (*St)[St->Count() - 1]);
    } else {
        if (Game->AutoexecOverridegamestart()) {
            if (Game->SecureMode()) St->Add("Z:\\config.com -securemode > nul");
        } else {
            SpeedTestInfo("Selecting file to run");
            if (RunExtraFile >= 0) {
                T = Trim(Game->ExtraPrgFile(RunExtraFile));
                I = Pos(";", T); // {ExtraPrgFile=Description;PathAndFile}
                if ((T.isEmpty()) || (I == 0)) {
                    result = BuildRunCommands(St, Game->GameExe(), Game->GameParameters(), Game->LoadFix(), Game->LoadFixMemory(), Game->Use4DOS(), Game->UseDOS32A(), Game, FreeDriveLetters, WarnIfNotReachable, WarnIfWindowsExe);
                } else {
                    T = Copy(T, I + 1, MaxInt);
                    result = BuildRunCommands(St, T, Game->ExtraPrgFileParameter(RunExtraFile), Game->LoadFix(), Game->LoadFixMemory(), Game->Use4DOS(), false, Game, FreeDriveLetters, WarnIfNotReachable, WarnIfWindowsExe);
                }
            } else {
                if (RunSetup) {
                    result = BuildRunCommands(St, Game->SetupExe(), Game->SetupParameters(), Game->LoadFix(), Game->LoadFixMemory(), Game->Use4DOS(), false, Game, FreeDriveLetters, WarnIfNotReachable, WarnIfWindowsExe);
                } else {
                    result = BuildRunCommands(St, Game->GameExe(), Game->GameParameters(), Game->LoadFix(), Game->LoadFixMemory(), Game->Use4DOS(), Game->UseDOS32A(), Game, FreeDriveLetters, WarnIfNotReachable, WarnIfWindowsExe);
                }
            }
        }
    }

    // { User defined Finalization }

    St2 = StringToStringList(Game->AutoexecFinalization());
    St->AddStrings(*St2);
    delete St2;

    // { Conditionally close DOSBox }

    if ((S.isEmpty()) && (!Game->AutoexecOverridegamestart()) && Game->CloseDosBoxAfterGameExit()) {
        if (RunExtraFile >= 0) {
            St->Add("exit");
        } else {
            if (RunSetup) {
                if (!Trim(Game->SetupExe()).isEmpty()) St->Add("exit");
            } else {
                if (!Trim(Game->GameExe()).isEmpty()) St->Add("exit");
            }
        }
    }
    return result;
}

static QString GetPixelShader(TGame* Game)
{
    QString S, Dir;
    int I;
    TSearchRec Rec;
    QString result = "none";

    Dir = "";
    S = Trim(ExtUpperCase(Game->CustomDOSBoxDir()));
    if ((S.isEmpty()) || (S == "DEFAULT")) {
        Dir = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
    } else {
        for (I = 1; I < PrgSetup->DOSBoxSettingsCount(); I++) if (Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(I)->Name())) == S) { Dir = PrgSetup->DOSBoxSettings(I)->DosBoxDir(); break; }
    }
    if (Dir.isEmpty()) Dir = Game->CustomDOSBoxDir();
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(Dir, PrgSetup->BaseDir()));

    S = Trim(ExtUpperCase(Game->PixelShader()));
    I = FindFirst(Dir + "Shaders" + PathDelim + "*.fx", faAnyFile, Rec);
    while (I == 0) {
        if (S == Trim(ExtUpperCase(ChangeFileExt(Rec.Name, "")))) { result = Game->PixelShader(); break; }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

TStringList* BuildConfFile(TGame* Game, bool RunSetup, bool WarnIfNotReachable, int RunExtraFile, TStringList* DeleteOnExit, bool BuildForArchivePackage)
{
    TStringList* St;
    QString S, T;
    int DOSBoxNr;
    double DOSBoxVersion;
    int I;
    TStringList* result;

    SpeedTestInfo("Check DOSBox version");
    DOSBoxNr = Max(0, GetDOSBoxNr(Game));
    DOSBoxVersion = GetDOSBoxVersionOfGame(Game);
    SpeedTestInfoOnly("DOSBox version: " + FloatToStr(DOSBoxVersion));

    SpeedTestInfo("Build [sdl] section of DOSBox conf file");
    result = new TStringList();

    result->Add("[sdl]");
    result->Add("fullscreen=" + ConfBoolToStr(Game->StartFullscreen()));
    result->Add("fulldouble=" + ConfBoolToStr(Game->UseDoublebuffering()));
    result->Add("fullresolution=" + Game->FullscreenResolution());
    result->Add("windowresolution=" + Game->WindowResolution());
    result->Add("output=" + Game->Render());
    result->Add("autolock=" + ConfBoolToStr(Game->AutoLockMouse()));
    result->Add("sensitivity=" + IntToStr(Game->MouseSensitivity()));
    result->Add("waitonerror=" + ConfBoolToStr(PrgSetup->DOSBoxSettings(DOSBoxNr)->WaitOnError()));
    result->Add("usescancodes=" + ConfBoolToStr(Game->UseScanCodes()));
    result->Add("priority=" + Game->Priority());
    S = Trim(Game->CustomKeyMappingFile());
    if ((S.isEmpty()) || (ExtUpperCase(S) == "DEFAULT")) S = PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxMapperFile();
    T = UnmapDrive(MakeAbsPath(S, PrgDataDir()), ptMapper);
    if (BuildForArchivePackage) T = MakeRelPath(T, PrgDataDir());
    result->Add("mapperfile=" + T);
    if (PrgSetup->AllowPixelShader()) {
        SpeedTestInfo("Get pixel shader");
        S = GetPixelShader(Game);
        if ((!S.isEmpty()) && (ExtUpperCase(Copy(S, Length(S) - 2, 3)) != ".FX")) S = S + ".fx";
        result->Add("pixelshader=" + S);
    }

    SpeedTestInfo("Build [dosbox] section of DOSBox conf file");
    result->Add("");
    result->Add("[dosbox]");
    S = Trim(ExtUpperCase(Game->CustomDOSBoxLanguage()));
    if (S != "ENGLISH") {
        if ((S.isEmpty()) || (S == "DEFAULT")) S = ""; else {
            T = Trim(ExtUpperCase(Game->CustomDOSBoxDir()));
            if ((T.isEmpty()) || (T == "DEFAULT")) T = PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxDir();
            if (FileExists(IncludeTrailingPathDelimiter(T) + S)) S = IncludeTrailingPathDelimiter(T) + S; else {
                if (!FileExists(S)) S = "";
            }
        }
        if (S.isEmpty()) S = PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxLanguage();

        T = ExtractFileName(S);
        if (FileExists(PrgDataDir() + LanguageSubDir + PathDelim + T)) S = PrgDataDir() + LanguageSubDir + PathDelim + T;
        // {Check if language file is on network share (which DOSBox can't handle)}
        if (Copy(S, 1, 2) == "\\\\") {
            if (DeleteOnExit != nullptr) {
                CopyFile(S, TempDir() + ExtractFileName(S), false);
                DeleteOnExit->Add(TempDir() + ExtractFileName(S));
            }
            S = TempDir() + ExtractFileName(S);
            T = UnmapDrive(S, ptDOSBox);
            if (BuildForArchivePackage) {
                T = MakeRelPath(T, PrgDataDir());
                if (BuildForArchivePackage && (Copy(T, 2, 1) == ":")) T = ExtractFileName(T); // {Completly remove path if its not possible to make the path realtive.}
            }
            result->Add("language=" + T);
        } else {
            T = UnmapDrive(S, ptDOSBox);
            if (BuildForArchivePackage) {
                T = MakeRelPath(T, PrgDataDir());
                if (BuildForArchivePackage && (Copy(T, 2, 1) == ":")) T = ExtractFileName(T); // {Completly remove path if its not possible to make the path realtive.}
            }
            if (FileExists(S)) result->Add("language=" + T);
        }
    }
    S = Game->VideoCard(); T = Trim(ExtUpperCase(S));
    if (DOSBoxVersion > 0.72) {
        if (T == "VGA") S = "svga_s3";
    } else {
        if ((T == "VGAONLY") || (T == "SVGA_S3") || (T == "SVGA_ET3000") || (T == "SVGA_ET4000") || (T == "SVGA_PARADISE") || (T == "VESA_NOLFB") || (T == "VESA_OLDVBE")) S = "vga";
    }
    result->Add("machine=" + S);
    T = UnmapDrive(MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir()), ptScreenshot);
    if (BuildForArchivePackage) T = MakeRelPath(T, PrgDataDir());
    result->Add("captures=" + T);
    result->Add("memsize=" + IntToStr(Game->Memory()));
    if (PrgSetup->AllowVGAChipsetSettings()) {
        I = Game->VideoRam();
        if ((I % 1024) != 0) I = (I / 1024) + 1; else I = I / 1024;
        result->Add("vmemsize=" + IntToStr(I));
    }

    result->Add("");
    result->Add("[render]");
    result->Add("frameskip=" + IntToStr(Game->FrameSkip()));
    result->Add("aspect=" + ConfBoolToStr(Game->AspectCorrection()));
    result->Add("scaler=" + Game->Scale());

    SpeedTestInfo("Build [vga, glide, cpi, midi, sblaster, gus and speaker] sections of DOSBox conf file");

    if (PrgSetup->AllowVGAChipsetSettings()) {
        result->Add("");
        result->Add("[vga]");
        result->Add("svgachipset=" + Game->VGAChipset());
        result->Add("videoram=" + IntToStr(Game->VideoRam()));
    }

    if (PrgSetup->AllowGlideSettings()) {
        result->Add("");
        result->Add("[glide]");
        S = Trim(ExtUpperCase(Game->GlideEmulation()));
        if ((S == "TRUE") || (S == "1")) S = "true"; else {
            if ((S == "FALSE") || (S == "0")) S = "false"; else S = Trim(ExtLowerCase(Game->GlideEmulation()));
        }
        result->Add("glide=" + S);
        if (S != "false") {
            result->Add("grport=" + Game->GlidePort());
            result->Add("lfb=" + Game->GlideLFB());
        }
    }

    result->Add("");
    result->Add("[cpu]");
    result->Add("core=" + Game->Core());
    if (!Trim(Game->CPUType()).isEmpty()) result->Add("cputype=" + Game->CPUType());

    if (DOSBoxVersion > 0.72) {
        if (TryStrToInt(Game->Cycles(), I)) result->Add("cycles=fixed " + Game->Cycles()); else result->Add("cycles=" + Game->Cycles());
    } else {
        result->Add("cycles=" + Game->Cycles());
    }
    result->Add("cycleup=" + IntToStr(Game->CyclesUp()));
    result->Add("cycledown=" + IntToStr(Game->CyclesDown()));

    result->Add("");
    result->Add("[mixer]");
    result->Add("nosound=" + ConfBoolToStr(Game->MixerNosound()));
    result->Add("rate=" + IntToStr(Game->MixerRate()));
    result->Add("blocksize=" + IntToStr(Game->MixerBlocksize()));
    result->Add("prebuffer=" + IntToStr(Game->MixerPrebuffer()));

    result->Add("");
    result->Add("[midi]");
    result->Add("mpu401=" + Game->MIDIType());
    if (DOSBoxVersion > 0.72) {
        result->Add("mididevice=" + Game->MIDIDevice());
        result->Add("midiconfig=" + MakeDOSBoxMIDIString(Game->MIDIConfig()));
    } else {
        result->Add("device=" + Game->MIDIDevice());
        result->Add("config=" + MakeDOSBoxMIDIString(Game->MIDIConfig()));
        if (ExtUpperCase(Game->MIDIDevice()) == "MT32") {
            result->Add("mt32reverb.mode=" + Game->MIDIMT32Mode());
            result->Add("mt32reverb.time=" + Game->MIDIMT32Time());
            result->Add("mt32reverb.level=" + Game->MIDIMT32Level());
        }
    }

    result->Add("");
    result->Add("[sblaster]");
    result->Add("sbtype=" + Game->SBType());
    result->Add("sbbase=" + Game->SBBase());
    result->Add("irq=" + IntToStr(Game->SBIRQ()));
    result->Add("dma=" + IntToStr(Game->SBDMA()));
    result->Add("hdma=" + IntToStr(Game->SBHDMA()));
    if (DOSBoxVersion > 0.72)
        result->Add("sbmixer=" + ConfBoolToStr(Game->SBMixer()));
    else
        result->Add("mixer=" + ConfBoolToStr(Game->SBMixer()));
    result->Add("oplmode=" + Game->SBOplMode());
    result->Add("oplrate=" + IntToStr(Game->SBOplRate()));
    if (DOSBoxVersion > 0.72) result->Add("oplemu=" + Game->SBOplEmu());

    result->Add("");
    result->Add("[gus]");
    result->Add("gus=" + ConfBoolToStr(Game->GUS()));
    result->Add("gusrate=" + IntToStr(Game->GUSRate()));
    result->Add("gusbase=" + Game->GUSBase());
    if (DOSBoxVersion > 0.72) {
        result->Add("gusirq=" + IntToStr(Game->GUSIRQ()));
        result->Add("gusdma=" + IntToStr(Game->GUSDMA()));
    } else {
        result->Add("irq1=" + IntToStr(Game->GUSIRQ()));
        result->Add("irq2=" + IntToStr(Game->GUSIRQ()));
        result->Add("dma1=" + IntToStr(Game->GUSDMA()));
        result->Add("dma2=" + IntToStr(Game->GUSDMA()));
    }
    result->Add("ultradir=" + Game->GUSUltraDir());

    result->Add("");
    result->Add("[speaker]");
    result->Add("pcspeaker=" + ConfBoolToStr(Game->SpeakerPC()));
    result->Add("pcrate=" + IntToStr(Game->SpeakerRate()));
    result->Add("tandy=" + Game->SpeakerTandy());
    result->Add("tandyrate=" + IntToStr(Game->SpeakerTandyRate()));
    result->Add("disney=" + ConfBoolToStr(Game->SpeakerDisney()));

    if (PrgSetup->AllowInnova()) {
        result->Add("");
        result->Add("[innova]");
        result->Add("innova=" + ConfBoolToStr(Game->Innova()));
        result->Add("samplerate=" + IntToStr(Game->InnovaRate()));
        result->Add("sidbase=" + Game->InnovaBase());
        result->Add("quality=" + IntToStr(Game->InnovaQuality()));
    }

    SpeedTestInfo("Build [dos] section of DOSBox conf file");
    result->Add("");
    result->Add("[dos]");
    result->Add("xms=" + ConfBoolToStr(Game->XMS()));
    result->Add("ems=" + ConfBoolToStr(Game->EMS()));
    result->Add("umb=" + ConfBoolToStr(Game->UMB()));

    T = Trim(ExtUpperCase(Game->KeyboardLayout()));
    if ((T.isEmpty()) || (T == "DEFAULT")) {
        DOSBoxNr = Max(0, GetDOSBoxNr(Game));
        T = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->KeyboardLayout()));
        if ((T.isEmpty()) || (T == "DEFAULT")) T = LanguageSetup->GameKeyboardLayoutDefault();
    }
    if (Pos("(", T) > 0) {
        S = Copy(T, Pos("(", T) + 1, MaxInt);
        if (Pos(")", S) > 0) { S = Trim(Copy(S, 1, Pos(")", S) - 1)); if (!S.isEmpty()) T = S; }
    }
    if (ExtUpperCase(T) == "NONE") T = "none"; // {DOSBox keyb accepts GR and gr but not NONE}
    result->Add("keyboardlayout=" + T); // {setting keyboard layout here and via keyb command in autoexec; if keyb in autoexec fails due to wrong codepade, keyboard layout will be right anyway}

    /* keyboardlayout can't handle layout+codepage -> moved to autoexec as keyb command
    S:=Trim(ExtUpperCase(Game.Codepage));
    If (S='') or (S='DEFAULT') then S:=LanguageSetup.GameKeyboardCodepageDefault;
    result.Add('keyboardlayout='+T+' '+S); */

    SpeedTestInfo("Build [joystick, serial, ipx, printer] section of DOSBox conf file");

    result->Add("");
    result->Add("[joystick]");
    result->Add("joysticktype=" + Game->JoystickType());
    result->Add("timed=" + ConfBoolToStr(Game->JoystickTimed()));
    result->Add("autofire=" + ConfBoolToStr(Game->JoystickAutoFire()));
    result->Add("swap34=" + ConfBoolToStr(Game->JoystickSwap34()));
    result->Add("buttonwrap=" + ConfBoolToStr(Game->JoystickButtonwrap()));

    if (PrgSetup->AllowNe2000()) {
        result->Add("");
        result->Add("[ne2000]");
        result->Add("ne2000=" + ConfBoolToStr(Game->NE2000()));
        result->Add("nicbase=" + Game->NE2000Base());
        result->Add("nicirq=" + IntToStr(Game->NE2000IRQ()));
        result->Add("macaddr=" + Game->NE2000MACAddress());
        result->Add("realnic=" + Game->NE2000RealInterface());
    }

    result->Add("");
    result->Add("[serial]");
    S = ExtLowerCase(Game->Serial1());
    result->Add("serial1=" + S);
    S = ExtLowerCase(Game->Serial2());
    result->Add("serial2=" + S);
    S = ExtLowerCase(Game->Serial3());
    result->Add("serial3=" + S);
    S = ExtLowerCase(Game->Serial4());
    result->Add("serial4=" + S);

    if (Game->IPX()) {
        result->Add("");
        result->Add("[ipx]");
        result->Add("ipx=true");
    }

    if (PrgSetup->AllowPrinterSettings()) {
        result->Add("");
        result->Add("[printer]");
        result->Add("printer=" + ConfBoolToStr(Game->EnablePrinterEmulation()));
        T = UnmapDrive(MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir()), ptScreenshot);
        if (BuildForArchivePackage) T = MakeRelPath(T, PrgDataDir());
        result->Add("docpath=" + T);
        result->Add("dpi=" + IntToStr(Game->PrinterResolution()));
        result->Add("width=" + IntToStr(Game->PaperWidth()));
        result->Add("height=" + IntToStr(Game->PaperHeight()));
        result->Add("printoutput=" + Game->PrinterOutputFormat());
        result->Add("multipage=" + ConfBoolToStr(Game->PrinterMultiPage()));
    }

    if (!BuildAutoexec(Game, RunSetup, result, WarnIfNotReachable, RunExtraFile, true, true, BuildForArchivePackage)) { delete result; result = nullptr; return result; }

    SpeedTestInfo("Add custom settings to DOSBox conf file");

    St = StringToStringList(Game->CustomSettings());
    if (result != nullptr) result->AddStrings(*St);
    delete St;

    St = StringToStringList(PrgSetup->DOSBoxSettings(DOSBoxNr)->CustomSettings());
    if (result != nullptr) result->AddStrings(*St);
    delete St;

    return result;
}

#ifdef Q_OS_WIN
static void FindAlternativeDOSBoxFile(QString& PrgFile)
{
    TSearchRec Rec;
    int I;
    I = FindFirst(ChangeFileExt(PrgFile, "*.exe"), faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            PrgFile = IncludeTrailingPathDelimiter(ExtractFilePath(PrgFile)) + Rec.Name;
            break;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}
#endif

#ifndef Q_OS_WIN
// Linux/macOS: writes a DOSBox conf file. Host file names are UTF-8, so the
// settings and the mount / imgmount / boot commands (host paths) are written
// in UTF-8; the other autoexec lines (echo texts etc.) are shown by DOS and
// are written in the DOS code page of the "keyb" command (437 or 850).
static bool WriteDOSBoxConfFile(TStringList* St, const QString& FileName)
{
    static const QRegularExpression KeybRx(QStringLiteral("^\\s*@?keyb\\s+\\S+\\s+(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression HostPathRx(QStringLiteral("^\\s*@?(mount|imgmount|boot)\\b"), QRegularExpression::CaseInsensitiveOption);
    int CodePage = 437;
    bool InAutoexec = false;
    for (int I = 0; I < St->Count(); I++) {
        const QString L = Trim((*St)[I]);
        if (L.startsWith('[')) InAutoexec = (L.compare(QStringLiteral("[autoexec]"), Qt::CaseInsensitive) == 0);
        else if (InAutoexec) {
            const QRegularExpressionMatch M = KeybRx.match(L);
            if (M.hasMatch() && DosCodePageSupported(M.captured(1).toInt())) CodePage = M.captured(1).toInt();
        }
    }
    QByteArray Data;
    InAutoexec = false;
    for (int I = 0; I < St->Count(); I++) {
        const QString& L = (*St)[I];
        const QString T = Trim(L);
        if (T.startsWith('[')) InAutoexec = (T.compare(QStringLiteral("[autoexec]"), Qt::CaseInsensitive) == 0);
        if (InAutoexec && !T.startsWith('[') && !HostPathRx.match(L).hasMatch()) Data += EncodeDosCodePage(L, CodePage);
        else Data += L.toUtf8();
        Data += '\n';
    }
    QFile F(FileName);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return F.write(Data) == Data.size();
}
#endif

QString GetDOSBoxCommandLine(int DOSBoxNr, const QString& ConfFile, int ShowConsole, const QString& DosBoxCommandLine)
{
    QString Add;
    Add = "";
#ifdef Q_OS_WIN
    // (-NOCONSOLE: DOSBox only has a console window on Windows)
    switch (ShowConsole) {
        case 0: Add = " -NOCONSOLE"; break;
        case 2: break; // {always show console}
        default: /* {1 :} */ if (PrgSetup->DOSBoxSettings(Max(0, DOSBoxNr))->HideDosBoxConsole()) Add = " -NOCONSOLE"; break;
    }
#else
    Q_UNUSED(ShowConsole);
#endif
    // (the Delphi code uses DOSBoxSettings[DOSBoxNr] for the value - Max(0,...) avoids an invalid index)
    if (!Trim(PrgSetup->DOSBoxSettings(Max(0, DOSBoxNr))->CommandLineParameters()).isEmpty()) Add = Add + " " + Trim(PrgSetup->DOSBoxSettings(Max(0, DOSBoxNr))->CommandLineParameters());
    if (!DosBoxCommandLine.isEmpty()) Add = Add + " " + DosBoxCommandLine;
    return "-CONF \"" + ConfFile + "\"" + Add;
}

static THandle RunDosBox(const QString& DOSBoxPath, int DOSBoxNr, const QString& ConfFile, bool FullScreen, int ShowConsole, bool asAdmin, const QString& DosBoxCommandLine = QString())
{
    QString PrgFile, Params, Env, S;
    bool Waited;
    THandle result;

    SpeedTestInfo("Building DOSBox command line");
    Params = GetDOSBoxCommandLine(DOSBoxNr, ConfFile, ShowConsole, DosBoxCommandLine);

    SpeedTestInfo("Searching DOSBox program file");
    PrgFile = Trim(DOSBoxPath);
    if ((PrgFile.isEmpty()) || (ExtUpperCase(PrgFile) == "DEFAULT")) PrgFile = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxDir()) + DosBoxFileName; else {
#ifdef Q_OS_WIN
        if (ExtUpperCase(Copy(PrgFile, Length(PrgFile) - 3, 4)) != ".EXE") PrgFile = IncludeTrailingPathDelimiter(MakeAbsPath(PrgFile, PrgSetup->BaseDir())) + DosBoxFileName;
#else
        // no ".exe" to tell program files from folders
        if (DirectoryExists(MakeAbsPath(PrgFile, PrgSetup->BaseDir()))) PrgFile = IncludeTrailingPathDelimiter(MakeAbsPath(PrgFile, PrgSetup->BaseDir())) + DosBoxFileName;
        else PrgFile = MakeAbsPath(PrgFile, PrgSetup->BaseDir());
#endif
    }
#ifdef Q_OS_WIN
    if (!FileExists(PrgFile)) FindAlternativeDOSBoxFile(PrgFile);
#else
    // fall back to a DOSBox in the search path
    if (!FileExists(PrgFile)) {
        QString T = QStandardPaths::findExecutable(DosBoxFileName);
        if (!T.isEmpty()) PrgFile = T;
    }
#endif
    if (!FileExists(PrgFile)) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindDOSBox(), {PrgFile}), mtError, mbOK, 0);
        result = InvalidHandle;
        return result;
    }
    PrgFile = UnmapDrive(PrgFile, ptDOSBox);

    SpeedTestInfoOnly("DOSBox program file: " + PrgFile);

    if (FullScreen) ShowFullscreenInfoDialog(ApplicationMainForm());

    if (asAdmin) {
        S = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->SDLVideodriver()));
        Env = "";
        if (S == "WINDIB") Env = "windib";
        if (S == "DIRECTX") Env = "directx";
        result = InvalidHandle;
        S = IncludeTrailingPathDelimiter(ExtractFilePath(PrgFile));
#ifdef Q_OS_WIN
        ShellOpen(PrgDir() + BinFolder + PathDelim + "AdminLauncher.exe", "/driver=" + Env + " /dir=" + S + " /run=\"" + PrgFile + "\" " + Params, S);
        return result;
#else
        // Port note: AdminLauncher.exe only exists on Windows - start DOSBox normally
#endif
    }

    S = Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(DOSBoxNr)->SDLVideodriver()));
    Env = "";
#ifdef Q_OS_WIN
    // (the SDL video drivers "windib" / "directx" only exist on Windows)
    if (S == "WINDIB") Env = "windib";
    if (S == "DIRECTX") Env = "directx";
#endif
    if (!Env.isEmpty()) {
        SpeedTestInfo("Setting up environment variables for DOSBox");
        Env = "SDL_VIDEODRIVER=" + Env;
        SpeedTestInfoOnly("Added " + Env);
    }

    SpeedTestInfo("Starting DOSBox");
    SpeedTestInfoOnly("Command: \"" + PrgFile + "\" " + Params);

    QStringList EnvList;
    if (!Env.isEmpty()) EnvList << Env;
#ifndef Q_OS_WIN
    // Linux/macOS: windows of other programs can not be moved (Wayland), so
    // DOSBox is asked to center its window itself (SDL 1.2 based builds)
    if (PrgSetup->DOSBoxSettings(DOSBoxNr)->CenterDOSBoxWindow() && (!FullScreen)) EnvList << "SDL_VIDEO_CENTERED=1" << "SDL_VIDEO_WINDOW_POS=center";
#endif
    result = StartProcessNative(PrgFile, Params, ExtractFilePath(PrgFile), EnvList);
    if (result == 0) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {PrgFile}), mtError, mbOK, 0);
        result = InvalidHandle;
        return result;
    }
    SpeedTestInfo("DOSBox started");
    if (result != InvalidHandle) SpeedTestInfoOnly("DOSBox successfully started");

    // {ShellExecute(Application.MainForm.Handle,'open',PChar(IncludeTrailingPathDelimiter(PrgSetup.DosBoxDir)+DosBoxFileName),PChar('-CONF "'+ConfFile+'"'+Add),PChar(IncludeTrailingPathDelimiter((ExtractFilePath(ConfFile)))),SW_SHOW);}

#ifdef Q_OS_WIN
    if (PrgSetup->DOSBoxSettings(DOSBoxNr)->CenterDOSBoxWindow() && (!FullScreen)) {
#else
    if (false) {   // (centered via SDL environment variables, see above)
#endif
        SpeedTestInfo("Center DOSBox window");
        Sleep(1000); Waited = true;
        CenterWindowFromProcessID(result);
    } else {
        Waited = false;
    }

    if (PrgSetup->MinimizeOnDosBoxStart()) {
        if (!Waited) Sleep(1000);
        // {MinimizedAtDOSBoxStart:=True; -> RunGameInt}
    }

    DOSBoxCounter->Add(result);
    return result;
}

static THandle RunGameInt(TGame* Game, bool RunSetup, const QString& DosBoxCommandLine, TStringList* DeleteOnExit, int RunExtraFile = -1)
{
    TStringList *St, *St2;
    QString T;
    int ZipRecNr;
    bool Error;
    int DOSBoxNr;
    bool AlreadyMinimized;
    bool RunAsAdmin;
    THandle result = InvalidHandle;

    AlreadyMinimized = false;

    SpeedTestInfo("Starting profile " + Game->SetupFile(), true);
    SpeedTestInfo("File checksum test");
    if (!RunCheck(Game, RunSetup, RunExtraFile)) return result;

    SpeedTestInfo("DOSBox installation selection");
    DOSBoxNr = GetDOSBoxNr(Game);
    SpeedTestInfoOnly("Selected DOSBox installation number " + IntToStr(DOSBoxNr));

    TFinally OuterFinally{[&]() {
        if (PrgSetup->MinimizeOnDosBoxStart() && (!AlreadyMinimized)) MinimizedAtDOSBoxStart = true;
    }};

    SpeedTestInfo("Capture folder check");
    if (Trim(Game->CaptureFolder()).isEmpty()) {
        Game->SetCaptureFolder(MakeRelPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir()));
        Game->StoreAllValues();
    }
    T = MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir());
    ForceDirectories(T);
    SpeedTestInfoOnly("Capture folder: " + T);

    St = BuildConfFile(Game, RunSetup, true, RunExtraFile, DeleteOnExit, false);
    if (St == nullptr) return result;

    TFinally InnerFinally{[St]() { delete St; }};

    SpeedTestInfo("Storing conf file");
    SpeedTestInfoOnly("Conf filename:" + TempDir() + DosBoxConfFileName);
#ifdef Q_OS_WIN
    const bool Saved = St->SaveToFile(TempDir() + DosBoxConfFileName);
#else
    const bool Saved = WriteDOSBoxConfFile(St, TempDir() + DosBoxConfFileName);
#endif
    if (!Saved) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {TempDir() + DosBoxConfFileName}), mtError, mbOK, 0);
        return result;
    }
    if (Game->CacheName != TempDOSBoxName) {
        SpeedTestInfo("Adding historiy");
        SpeedTestInfoOnly("Game name: " + Game->CacheName);
        History->Add(Game->CacheName);
    }

    if (DOSBoxNr >= 0) T = MakeAbsPath(PrgSetup->DOSBoxSettings(DOSBoxNr)->DosBoxDir(), PrgSetup->BaseDir()); else T = Trim(Game->CustomDOSBoxDir());

    SpeedTestInfo("Process zipped drives");
    ZipRecNr = ZipManager->AddGame(Game, Error);
    if (Error) return result;

    SpeedTestInfo("Process run before main file");
    RunPrgManager->RunBeforeExecutionCommand(Game);

    if (PrgSetup->MinimizeOnDosBoxStart()) {
        SpeedTestInfo("Minimize main window");
        AlreadyMinimized = MainFormIsMinimized();
        ApplicationMinimize();
    }

    RunAsAdmin = false;
    if (Game->RunAsAdmin() && PrgSetup->OfferRunAsAdmin()) {
        if (ZipRecNr < 0) RunAsAdmin = true; else MessageDlg(LanguageSetup->ProfileMountingZipAdminError(), mtError, mbOK, 0);
    }

    result = RunDosBox(T, Max(0, DOSBoxNr), TempDir() + DosBoxConfFileName, Game->StartFullscreen(), Game->ShowConsoleWindow(), RunAsAdmin, DosBoxCommandLine);

    St2 = new TStringList();
    St2->LoadFromFile(Game->SetupFile());
    SpeedTestInfoOnly(QString("\r") + "\r" + "### Content of prof file:" + "\r" + "\r" + St2->Text() + "\r" + "\r" + "### Content of conf file:" + "\r" + "\r" + St->Text());
    delete St2;
    SpeedTestDone();

    if (ZipRecNr >= 0) ZipManager->ActivateRepackCheck(ZipRecNr, result);
    RunPrgManager->AddCommand(Game, result);
    ScreensaverControl->DOSBoxStarted(result, PrgSetup->DOSBoxSettings(Max(0, DOSBoxNr))->DisableScreensaver());

    return result;
}

void RunGame(TGame* Game, TStringList* DeleteOnExit, bool RunSetup, const QString& DosBoxCommandLine, bool Wait)
{
    THandle DOSBoxHandle;
    bool B;

    DOSBoxHandle = RunGameInt(Game, RunSetup, DosBoxCommandLine, DeleteOnExit);
    if (Wait) {
        WaitForProcess(DOSBoxHandle);
    } else {
        B = (DosBoxCommandLine.isEmpty());
        if (B && (!Game->IgnoreWindowsFileWarnings())) {
            if (RunSetup) {
                if ((!Trim(Game->SetupExe()).isEmpty()) && (ExtUpperCase(Copy(Trim(Game->SetupExe()), 1, 7)) != "DOSBOX:")) B = !IsWindowsExe(MakeAbsPath(Game->SetupExe(), PrgSetup->BaseDir()));
            } else {
                if ((!Trim(Game->GameExe()).isEmpty()) && (ExtUpperCase(Copy(Trim(Game->GameExe()), 1, 7)) != "DOSBOX:")) B = !IsWindowsExe(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir()));
            }
        }
        if (B) {
            LastDOSBoxStartTime = GetTickCount();
            LastDOSBoxProfile = Game;
        }
    }
    // CloseHandle(DOSBoxHandle) -> nothing to do for process ids
}

THandle RunGameAndGetHandle(TGame* Game, TStringList* DeleteOnExit, bool RunSetup, const QString& DosBoxCommandLine)
{
    return RunGameInt(Game, RunSetup, DosBoxCommandLine, DeleteOnExit);
}

void RunExtraFile(TGame* Game, TStringList* DeleteOnExit, int ExtraFile)
{
    RunGameInt(Game, false, "", DeleteOnExit, ExtraFile);
}

static THandle RunCommandInt(TGame* Game, TStringList* DeleteOnExit, const QString& Command, bool DisableFullscreen)
{
    TStringList* St;
    QString AutoexecSave, FinalizationSave;
    bool FullscreenSave;

    FullscreenSave = Game->StartFullscreen();
    AutoexecSave = Game->Autoexec();
    FinalizationSave = Game->AutoexecFinalization();
    TFinally Restore{[&]() {
        Game->SetAutoexec(AutoexecSave);
        Game->SetAutoexecFinalization(FinalizationSave);
        Game->SetStartFullscreen(FullscreenSave);
    }};

    St = StringToStringList(AutoexecSave);
    if (!Command.isEmpty()) St->Add(Command);
    Game->SetAutoexec(StringListToString(St));
    delete St;
    if (DisableFullscreen) Game->SetStartFullscreen(false);
    return RunGameAndGetHandle(Game, DeleteOnExit, false, "");
}

void RunCommand(TGame* Game, TStringList* DeleteOnExit, const QString& Command, bool DisableFullscreen)
{
    RunCommandInt(Game, DeleteOnExit, Command, DisableFullscreen);
}

void RunCommandAndWait(TGame* Game, TStringList* DeleteOnExit, const QString& Command, bool DisableFullscreen)
{
    THandle DOSBoxHandle;
    DOSBoxHandle = RunCommandInt(Game, DeleteOnExit, Command, DisableFullscreen);
    WaitForProcess(DOSBoxHandle);
}

THandle RunCommandAndGetHandle(TGame* Game, TStringList* DeleteOnExit, const QString& Command, bool DisableFullscreen)
{
    return RunCommandInt(Game, DeleteOnExit, Command, DisableFullscreen);
}

void RunWithCommandline(TGame* Game, TStringList* DeleteOnExit, const QString& CommandLine, bool DisableFullscreen)
{
    bool FullscreenSave;
    FullscreenSave = Game->StartFullscreen();
    TFinally Restore{[&]() { Game->SetStartFullscreen(FullscreenSave); }};
    if (DisableFullscreen) Game->SetStartFullscreen(false);
    RunGame(Game, DeleteOnExit, false, CommandLine);
}

void RunWithCommandlineAndWait(TGame* Game, TStringList* DeleteOnExit, const QString& CommandLine, bool DisableFullscreen)
{
    bool FullscreenSave;
    FullscreenSave = Game->StartFullscreen();
    TFinally Restore{[&]() { Game->SetStartFullscreen(FullscreenSave); }};
    if (DisableFullscreen) Game->SetStartFullscreen(false);
    RunGame(Game, DeleteOnExit, false, CommandLine, true);
}

// --- exe file type detection -------------------------------------------------

// TFileStream.Read replacement: reads up to Count bytes into Buf (rest unchanged)
static void StreamRead(QFile& FSt, void* Buf, int Count)
{
    FSt.read(static_cast<char*>(Buf), Count);
}

static bool DeepWindowsCheck(QFile& FSt)
{
    static const char SearchString[] = "Microsoft Windows";
    const int SearchLen = static_cast<int>(sizeof(SearchString)) - 1;
    int I, J;
    char A[0x400 + 1];

    FSt.seek(0);
    // ReadBuffer raises an exception if the file is too short
    if (FSt.read(A, sizeof(A)) != static_cast<qint64>(sizeof(A))) throw Exception("Stream read error");
    J = 1;
    for (I = 0; I <= 0x400; I++) if (A[I] != SearchString[J - 1]) J = 1; else {
        J++;
        if (J == SearchLen) { return true; }
    }
    return false;
}

static bool IsDosZipHybridExe(const QString& FileName)
{
    if (ExtUpperCase(ExtractFileName(FileName)) != "DZ.EXE") return false;
    if (!FileExists(ChangeFileExt(FileName, ".DOS"))) return false;
    return true;
}

bool IsWindowsExe(const QString& FileName)
{
    qint32 I = 0;
    char C[4] = {0, 0, 0, 0};
    quint16 W = 0;

    if (!FileExists(FileName)) return false;
    if (Trim(ExtUpperCase(ExtractFileExt(FileName))) != ".EXE") return false;
    QFile FSt(ToNativePath(FileName));
    if (!FSt.open(QIODevice::ReadOnly)) return false;
    try {
        const qint64 Size = FSt.size();
        if (Size < 4) return false;
        StreamRead(FSt, C, 4);
        if ((C[0] != 'M') || (C[1] != 'Z')) return false;
        if (0x3C > Size - 4) return false;
        StreamRead(FSt, &W, 2);

        // {Detect win32 PE header}
        FSt.seek(0x3C);
        StreamRead(FSt, &I, 4);
        I = qFromLittleEndian(I);
        if ((I < 0) || (I > Size - 4)) return false;
        FSt.seek(I);
        StreamRead(FSt, C, 4);
        if ((C[0] == 'P') && (C[1] == 'E') && (C[2] == 0) && (C[3] == 0)) {
            if (IsDosZipHybridExe(FileName)) { return false; }
            return true;
        }

        // {Detect win16 NE header}
        FSt.seek(0x3C);
        StreamRead(FSt, &W, 2);
        W = qFromLittleEndian(W);
        if (W > Size - 2) return false;
        FSt.seek(W);
        StreamRead(FSt, C, 2);
        if ((C[0] == 'N') && (C[1] == 'E')) { return DeepWindowsCheck(FSt); }
    } catch (...) { return false; }
    return false;
}

bool IsDOSExe(const QString& FileName)
{
    qint32 I = 0;
    char C[4] = {0, 0, 0, 0};

    if (!FileExists(FileName)) return false;
    if (Trim(ExtUpperCase(ExtractFileExt(FileName))) != ".EXE") return false;
    QFile FSt(ToNativePath(FileName));
    if (!FSt.open(QIODevice::ReadOnly)) return false;
    try {
        const qint64 Size = FSt.size();
        if (Size < 4) return false;
        StreamRead(FSt, C, 4);
        if ((C[0] != 'M') || (C[1] != 'Z')) return false;
        if (0x3C > Size - 4) return false;
        FSt.seek(0x3C);
        StreamRead(FSt, &I, 4);
        I = qFromLittleEndian(I);
        if ((I < 0) || (I > Size - 4)) { return true; }
        FSt.seek(I);
        StreamRead(FSt, C, 4);
        return !((C[0] == 'P') && (C[1] == 'E') && (C[2] == 0) && (C[3] == 0));
    } catch (...) { return false; }
}
