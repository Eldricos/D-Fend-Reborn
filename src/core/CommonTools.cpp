// C++ port of CommonTools.pas (implementation section).
#include "CommonTools.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "ImageStretch.h"

#include <QFileIconProvider>
#include <QStandardPaths>
#include <QImageReader>
#include <QImageWriter>
#include <QDesktopServices>
#include <QThread>
#include <cmath>

#ifdef Q_OS_WIN
#include "WinApi.h"
#include <objbase.h>
#include <shobjidl.h>
#endif

// ---------------------------------------------------------------------------
// local helpers of the port
// ---------------------------------------------------------------------------

static bool IsDelimC(QChar C) { return C == QLatin1Char('\\') || C == QLatin1Char('/'); }

// "Copy(Path,1,2)='.\'" accepting both path delimiters (profiles written on
// Windows use '\', profiles written on other systems '/')
static bool StartsWithDotDelim(const QString &P) { return P.length() >= 2 && P[0] == QLatin1Char('.') && IsDelimC(P[1]); }
static bool StartsWithDotDotDelim(const QString &P)
{
    return P.length() >= 3 && P[0] == QLatin1Char('.') && P[1] == QLatin1Char('.') && IsDelimC(P[2]);
}

// SysErrorMessage(GetLastError)
static QString LastSysErrorMessage() { return qt_error_string(-1); }

static QString ProgramVersionString()
{
    QString S = QCoreApplication::applicationVersion();
    if (Trim(S).isEmpty()) S = QStringLiteral("1.4.4");
    return S;
}

// Program version as Windows file version: MS = major<<16 | minor, LS = release<<16 | build
static void ProgramVersion(quint32 &MS, quint32 &LS)
{
    QStringList L = ProgramVersionString().split(QLatin1Char('.'));
    int V[4] = {0, 0, 0, 0};
    for (int I = 0; I < 4 && I < L.size(); I++) V[I] = StrToIntDef(L[I], 0);
    MS = (quint32(V[0]) << 16) | quint32(V[1] & 0xFFFF);
    LS = (quint32(V[2]) << 16) | quint32(V[3] & 0xFFFF);
}

static QString ExeFileName()
{
    return ExpandFileName(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

// ---------------------------------------------------------------------------

QString ExtUpperCase(const QString& S)
{
    QString result = Trim(S);
    for (int I = 1; I <= Length(result); I++) {
        const QChar C = result[I - 1];
        if (C >= QLatin1Char('a') && C <= QLatin1Char('z')) {
            result[I - 1] = QChar(C.unicode() - ('a' - 'A'));
            continue;
        }
        int J = Pos(C, LanguageSpecialLowerCase);
        if (J > 0 && J <= Length(LanguageSpecialUpperCase)) result[I - 1] = LanguageSpecialUpperCase[J - 1];
    }
    return result;
}

QString ExtLowerCase(const QString& S)
{
    QString result = Trim(S);
    for (int I = 1; I <= Length(result); I++) {
        const QChar C = result[I - 1];
        if (C >= QLatin1Char('A') && C <= QLatin1Char('Z')) {
            result[I - 1] = QChar(C.unicode() + ('a' - 'A'));
            continue;
        }
        int J = Pos(C, LanguageSpecialUpperCase);
        if (J > 0 && J <= Length(LanguageSpecialLowerCase)) result[I - 1] = LanguageSpecialLowerCase[J - 1];
    }
    return result;
}

double StrToFloatEx(const QString& AS)
{
    QString S = AS;
    // DecimalSeparator of the port is always '.'
    for (int I = 1; I <= Length(S); I++) if (S[I - 1] == QLatin1Char('.') || S[I - 1] == QLatin1Char(',')) S[I - 1] = QLatin1Char('.');
    return StrToFloat(S);
}

TStringList* ValueToList(const QString& AValue, const QString& Divider)
{
    TStringList *result = new TStringList();
    QString Value = Trim(AValue);

    while (!Value.isEmpty()) {
        int I = 0;
        for (int J = 1; J <= Length(Value); J++) if (Pos(Value[J - 1], Divider) != 0) { I = J; break; }

        if (I > 0) {
            result->Add(Trim(Copy(Value, 1, I - 1)));
            Value = Trim(Copy(Value, I + 1, MaxInt));
        } else {
            result->Add(Value);
            Value = QString();
        }
    }

    if (Divider == QLatin1String(";"))
        for (int I = 0; I < result->Count(); I++)
            (*result)[I] = StringReplace((*result)[I], QStringLiteral("<semicolon>"), QStringLiteral(";"), rfReplaceAll | rfIgnoreCase);
    return result;
}

QString ListToValue(TStringList* St, QChar Divider)
{
    QString result;
    for (int I = 0; I < St->Count(); I++) {
        QString S = Trim((*St)[I]);
        if (S.isEmpty()) continue;
        S = StringReplace(S, QStringLiteral(";"), QStringLiteral("<semicolon>"), rfReplaceAll);
        if (!result.isEmpty()) result += Divider;
        result += S;
    }
    return result;
}

TStringList* StringToStringList(const QString& AS)
{
    QString S = AS;
    QString T;
    while (!S.isEmpty()) {
        int I = Pos(QStringLiteral("["), S);
        if (I == 0) { T += S; S = QString(); continue; }
        T += Copy(S, 1, I - 1); S = Copy(S, I + 1, MaxInt);
        I = Pos(QStringLiteral("]"), S);
        if (I == 0) { T += QStringLiteral("[") + S; S = QString(); continue; }
        int J = 0;
        if ((!TryStrToInt(Copy(S, 1, I - 1), J)) || (J < 0) || (J > 255)) { T += QStringLiteral("["); continue; }
        T += QChar(J); S = Copy(S, I + 1, MaxInt);
    }

    TStringList *result = new TStringList();
    result->SetText(T);
    return result;
}

QString StringListToString(TStringList* St)
{
    // TStrings.Text always uses #13#10 in Delphi
    QString S;
    for (int I = 0; I < St->Count(); I++) S += (*St)[I] + QStringLiteral("\r\n");
    return StringListToString(S);
}

QString StringListToString(const QString& S)
{
    QString result;
    for (int I = 1; I <= Length(S); I++)
        if (S[I - 1].unicode() < 32) result += QStringLiteral("[") + IntToStr(S[I - 1].unicode()) + QStringLiteral("]"); else result += S[I - 1];
    return result;
}

QString Replace(const QString& S, const QString& FromSub, const QString& ToSub)
{
    QString Text = S, result;
    int I = Pos(FromSub, Text);
    while (I > 0) {
        if ((!ToSub.isEmpty()) && (FromSub == QString(ToSub[0])) && (Copy(Text, I, Length(ToSub)) == ToSub)) {
            result += Copy(Text, 1, I + Length(ToSub) - 1);
            Text = Copy(Text, I + Length(ToSub), MaxInt);
        } else {
            result += Copy(Text, 1, I - 1) + ToSub;
            Text = Copy(Text, I + Length(FromSub), MaxInt);
        }
        I = Pos(FromSub, Text);
    }
    result += Text;
    return result;
}

bool FindStringInFile(const QString& FileName, const QString& SearchString)
{
    if (!FileExists(FileName)) return false;
    if (SearchString.isEmpty()) return false;
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return false;
    const QByteArray MSt = F.readAll();
    return MSt.contains(EncodeAnsi(SearchString));
}

QString PrgDir()
{
    if (!TempPrgDir.isEmpty())
        return TempPrgDir;
    return IncludeTrailingPathDelimiter(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
}

QString TempDir()
{
    return IncludeTrailingPathDelimiter(QDir::toNativeSeparators(QDir::tempPath()));
}

// Private ShortName of CommonTools (does not check PrgSetup.UseShortFolderNames,
// unlike ImageTools.ShortName / DosBoxUnit.ShortName)
static QString CommonToolsShortName(const QString& LongName)
{
#ifdef Q_OS_WIN
    wchar_t Buffer[MAX_PATH + 10];
    std::wstring L = LongName.toStdWString();
    if (GetShortPathNameW(L.c_str(), Buffer, MAX_PATH) == 0) return LongName;
    return QString::fromWCharArray(Buffer);
#else
    return LongName;
#endif
}

QString MakeRelPath(const QString& APath, const QString& ARel, bool NoEmptyRelPath)
{
    QString Path = APath, Rel = ARel, FileName;

    // No conversion if path DOSBox relative to DOSBox internal file system
    if ((ExtUpperCase(Copy(Trim(Path), 1, 7)) == QLatin1String("DOSBOX:")) || (Trim(Path).isEmpty())) return Path;

    // Check base path
    Rel = Trim(Rel); if (Rel.isEmpty()) return Path;
    Rel = IncludeTrailingPathDelimiter(Rel);

    // Split path into path an file name
    if (DirectoryExists(Path)) {
        FileName = QString();
        Path = IncludeTrailingPathDelimiter(Path);
    } else {
        FileName = ExtractFileName(Path);
        Path = IncludeTrailingPathDelimiter(ExtractFilePath(Path));
    }

    // Convert
    if ((Length(Path) >= Length(Rel)) && (ExtUpperCase(Copy(Path, 1, Length(Rel))) == ExtUpperCase(Rel))) {
        // Path is relative to Rel
        Path = QStringLiteral(".") + PathDelim + Copy(Path, Length(Rel) + 1, MaxInt);
    } else {
        Rel = IncludeTrailingPathDelimiter(CommonToolsShortName(Rel));
        // May be Path is relative to the short version of Rel
        if ((Length(Path) >= Length(Rel)) && (ExtUpperCase(Copy(Path, 1, Length(Rel))) == ExtUpperCase(Rel)))
            Path = QStringLiteral(".") + PathDelim + Copy(Path, Length(Rel) + 1, MaxInt);
    }

    // Remove useless parts
    if ((Path.length() == 2 && StartsWithDotDelim(Path)) || (Path.length() == 1 && IsDelimC(Path[0]))) Path = QString();

    // Add file name again
    QString result = Path + FileName;

    // Set path to ".\" if empty and empty not allowed
    if (NoEmptyRelPath && result.isEmpty()) result = QStringLiteral(".") + PathDelim;
    return result;
}

static bool IsAlreadyAbsolute(const QString &Path)
{
    if ((Length(Path) >= 2) && ((Path[1] == QLatin1Char(':')) || (Copy(Path, 1, 2) == QLatin1String("\\\\")))) return true;
#ifndef Q_OS_WIN
    if (Path.startsWith(QLatin1Char('/'))) return true;
#endif
    return false;
}

QString MakeAbsPath(const QString& APath, const QString& ARel)
{
    QString Path = APath, Rel = ARel;

    // No conversion if path DOSBox relative to DOSBox internal file system
    if ((ExtUpperCase(Copy(Trim(Path), 1, 7)) == QLatin1String("DOSBOX:")) || (Trim(Path).isEmpty())) return Path;

    // Check base path
    Rel = Trim(Rel); if (Rel.isEmpty()) return Path;
    Rel = IncludeTrailingPathDelimiter(Rel);

    // Check if path is already absolute
    if (IsAlreadyAbsolute(Path)) return Path;

    // Remove leading ".\" or "\"
    Path = Trim(Path);
    if (StartsWithDotDelim(Path)) Path = Trim(Copy(Path, 3, MaxInt));
    if (!Path.isEmpty() && IsDelimC(Path[0])) Path = Trim(Copy(Path, 2, MaxInt));

    // Combine base path and relative path
    return Rel + ToNativePath(Path);
}

QString MakeExtRelPath(const QString& APath, const QString& ARel)
{
    QString Path = APath, Rel = ARel, FileName, S, T, ShortPath;
    int C;

    // No conversion if path DOSBox relative to DOSBox internal file system
    if ((ExtUpperCase(Copy(Trim(Path), 1, 7)) == QLatin1String("DOSBOX:")) || (Trim(Path).isEmpty())) return Path;

    // Check base path
    Rel = Trim(Rel); if (Rel.isEmpty()) return Path;
    Rel = IncludeTrailingPathDelimiter(Rel);

    // Split path into path an file name
    if (DirectoryExists(Path)) {
        FileName = QString();
        Path = IncludeTrailingPathDelimiter(Path);
    } else {
        FileName = ExtractFileName(Path);
        Path = IncludeTrailingPathDelimiter(ExtractFilePath(Path));
    }

    // Convert
    if ((Length(Path) >= Length(Rel)) && (ExtUpperCase(Copy(Path, 1, Length(Rel))) == ExtUpperCase(Rel))) {
        // Path is relative to Rel
        Path = QStringLiteral(".") + PathDelim + Copy(Path, Length(Rel) + 1, MaxInt);
    } else {
        // May be Path is relative to the short version of Rel
        Rel = IncludeTrailingPathDelimiter(CommonToolsShortName(Rel));
        if ((Length(Path) >= Length(Rel)) && (ExtUpperCase(Copy(Path, 1, Length(Rel))) == ExtUpperCase(Rel)))
            Path = QStringLiteral(".") + PathDelim + Copy(Path, Length(Rel) + 1, MaxInt);
    }

    // Check if path is ".." path to base path
    if (!StartsWithDotDelim(Path)) {
        C = 0;
        S = CommonToolsShortName(Rel);
        ShortPath = CommonToolsShortName(Path);
        do {
            if (!S.isEmpty()) S.chop(1);
            while ((!S.isEmpty()) && (!IsDelimC(S[S.length() - 1]))) S.chop(1);
            if (S.isEmpty()) break;
            C++;
            if ((Length(ShortPath) >= Length(S)) && (ExtUpperCase(Copy(ShortPath, 1, Length(S))) == ExtUpperCase(S))) {
                T = QString(); while (C > 0) { T += QStringLiteral("..") + PathDelim; C--; }
                Path = T + Copy(ShortPath, Length(S) + 1, MaxInt);
                break;
            }
        } while (true);
    }

    // Remove useless parts
    if (Path.length() == 2 && StartsWithDotDelim(Path)) Path = QString();

    // Combine base path and relative path
    return Path + FileName;
}

QString MakeExtAbsPath(const QString& APath, const QString& ARel)
{
    QString Path = APath, Rel = ARel;

    // No conversion if path DOSBox relative to DOSBox internal file system
    if ((ExtUpperCase(Copy(Trim(Path), 1, 7)) == QLatin1String("DOSBOX:")) || (Trim(Path).isEmpty())) return Path;

    // Check base path
    Rel = Trim(Rel); if (Rel.isEmpty()) return Path;
    Rel = IncludeTrailingPathDelimiter(Rel);

    // Check if path is already absolute
    if (IsAlreadyAbsolute(Path)) return Path;

    Path = Trim(Path);

    // Remove leading ".\" and directly combine base path and relative path (so next rule can't apply)
    if (StartsWithDotDelim(Path)) {
        Path = Trim(Copy(Path, 3, MaxInt));
        return Rel + ToNativePath(Path);
    }

    // If path starts with "..\": remove it and remove last part in base path
    if (StartsWithDotDotDelim(Path)) {
        while (StartsWithDotDotDelim(Path)) {
            if (Rel.isEmpty()) break;
            Rel.chop(1);
            while ((!Rel.isEmpty()) && (!IsDelimC(Rel[Rel.length() - 1]))) Rel.chop(1);
            Path = Copy(Path, 4, MaxInt);
        }
        return Rel + ToNativePath(Path);
    }

    // Combine base path and relative path
    return Rel + ToNativePath(Path);
}

static const QString AllowedCharsDefault = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜabcdefghijklmnopqrstuvwxyzäöüß01234567890-_=.,;!()$#@{}&'`~")
    + QChar(246) + QChar(255) + QChar(0xa0) + QChar(0xe5);

QString MakeFileSysOKFolderName(const QString& AName)
{
    QString result;
    QString AllowedChars = LanguageSetup->CharsetAllowedFileNameChars();
    if (AllowedChars.isEmpty()) AllowedChars = AllowedCharsDefault;
    AllowedChars += QLatin1Char(' ');
    for (int I = 1; I <= Length(AName); I++) if (Pos(AName[I - 1], AllowedChars) > 0) result += AName[I - 1];
    if (result.isEmpty()) result = QStringLiteral("Game");
    return result;
}

QString RemoveIllegalFileNameChars(const QString& Name)
{
    QString result;
    for (int I = 1; I <= Length(Name); I++) if (Pos(Name[I - 1], AllowedCharsDefault) > 0) result += Name[I - 1];
    return result;
}

static QString FindIconInSubfolders(const QString& Folder, const QString& Icon)
{
    TSearchRec Rec;
    TStringList St;
    QString F = IncludeTrailingPathDelimiter(Folder);

    int I = FindFirst(F + QStringLiteral("*.*"), faDirectory, Rec);
    while (I == 0) {
        if ((Rec.Name != QLatin1String(".")) && (Rec.Name != QLatin1String("..")) && ((Rec.Attr & faDirectory) != 0)) St.Add(Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    for (I = 0; I < St.Count(); I++) if (FileExists(F + St[I] + PathDelim + Icon)) return F + St[I] + PathDelim + Icon;
    for (I = 0; I < St.Count(); I++) { QString result = FindIconInSubfolders(F + St[I], Icon); if (!result.isEmpty()) return result; }
    return QString();
}

QString MakeAbsIconName(const QString& Icon)
{
    if (Trim(Icon).isEmpty()) return QString();
    QString S = ExtractFilePath(Icon);

    // Get icon from user defined folder
    if (!S.isEmpty()) return MakeAbsPath(Icon, PrgSetup->BaseDir());

    // Get icon from icon folder (or its subdfolders)
    QString result = PrgDataDir() + IconsSubDir + PathDelim + Icon;

    if (FileExists(result)) return result;
    S = FindIconInSubfolders(PrgDataDir() + IconsSubDir, Icon);
    if (!S.isEmpty()) result = S;
    return result;
}

bool SelectDirectory(THandle AOwner, const QString& Caption, QString& Directory)
{
    Q_UNUSED(AOwner);
    // SHBrowseForFolder with the old directory preselected
    QString S = QFileDialog::getExistingDirectory(QApplication::activeWindow(), Caption, Directory, QFileDialog::ShowDirsOnly);
    if (S.isEmpty()) return false;
    Directory = QDir::toNativeSeparators(S);
    return true;
}

/*
CSIDL_COOKIES              Cookies
CSIDL_DESKTOPDIRECTORY     Desktop
CSIDL_FAVORITES            Favorites
CSIDL_HISTORY              Internet history
CSIDL_INTERNET_CACHE       "Temporary Internet Files"
CSIDL_PERSONAL             User files
CSIDL_PROGRAMS             "Programe files" in start menu
CSIDL_RECENT               "Documents" in start menu
CSIDL_SENDTO               "Send to" in context menu
CSIDL_STARTMENU            Start menu
CSIDL_STARTUP              Auto start
*/
QString GetSpecialFolder(WId hWindow, int Folder)
{
#ifdef Q_OS_WIN
    wchar_t Path[MAX_PATH + 1];
    Path[0] = 0;
    if (FAILED(SHGetFolderPathW(reinterpret_cast<HWND>(hWindow), Folder, nullptr, 0, Path))) return QString();
    QString S = QString::fromWCharArray(Path);
    if (S.isEmpty()) return QString();
    return IncludeTrailingPathDelimiter(S);
#else
    Q_UNUSED(hWindow);
    QString S;
    auto Loc = [](QStandardPaths::StandardLocation L) { return QStandardPaths::writableLocation(L); };
    switch (Folder) {
        case CSIDL_DESKTOP:
        case CSIDL_DESKTOPDIRECTORY: S = Loc(QStandardPaths::DesktopLocation); break;
        case CSIDL_PERSONAL: S = Loc(QStandardPaths::DocumentsLocation); break;
        case CSIDL_MYPICTURES: S = Loc(QStandardPaths::PicturesLocation); break;
        case CSIDL_MYMUSIC: S = Loc(QStandardPaths::MusicLocation); break;
        case CSIDL_MYVIDEO: S = Loc(QStandardPaths::MoviesLocation); break;
        case CSIDL_APPDATA:
        case CSIDL_LOCAL_APPDATA: S = Loc(QStandardPaths::GenericDataLocation); break;
        case CSIDL_COMMON_APPDATA: {
            QStringList L = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
            S = L.isEmpty() ? QStringLiteral("/usr/share") : L.last();
            break;
        }
        case CSIDL_PROFILE:
        case CSIDL_FAVORITES:
        case CSIDL_RECENT:
        case CSIDL_SENDTO: S = Loc(QStandardPaths::HomeLocation); break;
        case CSIDL_PROGRAMS:
        case CSIDL_STARTMENU: S = Loc(QStandardPaths::ApplicationsLocation); break;
        case CSIDL_STARTUP: S = Loc(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart"); break;
        case CSIDL_INTERNET_CACHE:
        case CSIDL_COOKIES:
        case CSIDL_HISTORY: S = Loc(QStandardPaths::GenericCacheLocation); break;
        case CSIDL_PROGRAM_FILES: S = QStringLiteral("/usr"); break;
        case CSIDL_WINDOWS: S = QStringLiteral("/usr"); break;
        case CSIDL_SYSTEM: S = QStringLiteral("/usr/bin"); break;
        default: S = Loc(QStandardPaths::HomeLocation); break;
    }
    if (S.isEmpty()) return QString();
    return IncludeTrailingPathDelimiter(S);
#endif
}

quint32 GetFileVersionEx(const QString& AFileName)
{
    // TODO(port): the original read the VERSIONINFO resource of any file; the port
    // only knows the version of the program itself (the only file it was used for).
    Q_UNUSED(AFileName);
    quint32 MS, LS;
    ProgramVersion(MS, LS);
    return LS;
}

QString GetFileVersionAsString()
{
    quint32 I, J;
    ProgramVersion(I, J);
    // result:=Format('Version %d.%d.%d (Build %d)',[I div 65536,I mod 65536, J div 65536, J mod 65536]);
    return Format(QStringLiteral("Version %d.%d.%d"), {int(I / 65536), int(I % 65536), int(J / 65536)});
}

QString GetNormalFileVersionAsString()
{
    quint32 I, J;
    ProgramVersion(I, J);
    return Format(QStringLiteral("%d.%d.%d"), {int(I / 65536), int(I % 65536), int(J / 65536)});
}

QString GetShortFileVersionAsString()
{
    quint32 I, J;
    ProgramVersion(I, J);
    return Format(QStringLiteral("%d.%d"), {int(I / 65536), int(I % 65536)});
}

int VersionToInt(const QString& AVersion)
{
    QString Version = AVersion;
    int result = 0;
    int I = Pos(QStringLiteral("."), Version);
    while (I > 0) {
        int J = 0; TryStrToInt(Copy(Version, 1, I - 1), J); Version = Copy(Version, I + 1, MaxInt);
        result = result * 100 + J;
        I = Pos(QStringLiteral("."), Version);
    }
    int J = 0; TryStrToInt(Version, J);
    result = result * 100 + J;
    return result;
}

void RunAndWait(const QString& PrgFile, const QString& Path, bool HideWindow)
{
    RunProcessAndWait(PrgFile, QStringList(), Path, HideWindow, false);
}

static void PrepareProcess(QProcess &P, bool HideWindow)
{
#ifdef Q_OS_WIN
    if (HideWindow) {
        P.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *Args) {
            Args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            Args->startupInfo->wShowWindow = SW_HIDE;
        });
    }
#else
    Q_UNUSED(P);
    Q_UNUSED(HideWindow);
#endif
}

TStringList* RunAndGetOutput(const QString& PrgFile, const QString& Parameters, bool HideWindow)
{
    if (!FileExists(PrgFile)) return nullptr;

    QProcess P;
    PrepareProcess(P, HideWindow);
    P.setProcessChannelMode(QProcess::MergedChannels);   // hStdError:=hStdOutWritePipe
    P.setWorkingDirectory(ExtractFilePath(PrgFile));
    P.start(PrgFile, SplitCommandLine(Parameters));
    if (!P.waitForStarted(-1)) return nullptr;
    P.closeWriteChannel();

    // Wait for termination of program (reading the pipe so the program cannot block)
    QByteArray MSt;
    while (!P.waitForFinished(250)) {
        MSt += P.readAll();
        if (P.state() == QProcess::NotRunning) break;
    }
    MSt += P.readAll();

    // Copy data to TStringList
    TStringList *result = new TStringList();
    result->SetText(QString::fromLocal8Bit(MSt));
    return result;
}

bool RunWithInput(const QString& PrgFile, const QString& Parameters, bool HideWindow, TStringList* StdIn)
{
    if (!FileExists(PrgFile)) return false;

    QByteArray MSt;
    for (int I = 0; I < StdIn->Count(); I++) MSt += EncodeAnsi((*StdIn)[I]) + QByteArray(LineBreak.toLatin1());

    QProcess P;
    PrepareProcess(P, HideWindow);
    P.setProcessChannelMode(QProcess::MergedChannels);
    P.setStandardOutputFile(QProcess::nullDevice());   // output is read and dropped
    P.setWorkingDirectory(ExtractFilePath(PrgFile));
    P.start(PrgFile, SplitCommandLine(Parameters));
    if (!P.waitForStarted(-1)) return false;

    qint64 WritePos = 0;
    while (WritePos < MSt.size() && P.state() == QProcess::Running) {
        qint64 I = P.write(MSt.constData() + WritePos, MSt.size() - WritePos);
        if (I < 0) break;
        WritePos += I;
        P.waitForBytesWritten(250);
    }
    while (P.bytesToWrite() > 0 && P.state() == QProcess::Running) P.waitForBytesWritten(250);
    P.closeWriteChannel();

    // Wait for termination of the program
    while (!P.waitForFinished(250)) if (P.state() == QProcess::NotRunning) break;

    return (WritePos == MSt.size());
}

#ifdef Q_OS_WIN
static void CenterWindow(HWND hWindow)
{
    RECT R;
    GetWindowRect(hWindow, &R);
    SetWindowPos(hWindow, HWND_TOP,
                 (GetSystemMetrics(SM_CXSCREEN) / 2) - ((R.right - R.left) / 2),
                 (GetSystemMetrics(SM_CYSCREEN) / 2) - ((R.bottom - R.top) / 2),
                 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
}

static BOOL CALLBACK EnumWindowsFunc(HWND hWindow, LPARAM lParam)
{
    DWORD dwProcessID = 0;
    GetWindowThreadProcessId(hWindow, &dwProcessID);
    if (dwProcessID != DWORD(lParam)) return TRUE;

    LONG C = GetWindowLongW(hWindow, GWL_STYLE);
    if (((C & WS_VISIBLE) != 0) && ((C & WS_CAPTION) != 0) && ((C & WS_SYSMENU) != 0) && ((C & WS_DISABLED) == 0) && ((C & WS_TABSTOP) == 0))
        CenterWindow(hWindow);
    return TRUE;
}
#endif

void CenterWindowFromProcessID(THandle ProcessID)
{
#ifdef Q_OS_WIN
    EnumWindows(EnumWindowsFunc, LPARAM(ProcessID));
#else
    // TODO(port): windows of other processes cannot be moved portably (X11/Wayland)
    Q_UNUSED(ProcessID);
#endif
}

static QString GlobalHideTitle;

#ifdef Q_OS_WIN
static BOOL CALLBACK EnumWindowsFunc2(HWND hWindow, LPARAM lParam)
{
    DWORD dwProcessID = 0;
    GetWindowThreadProcessId(hWindow, &dwProcessID);
    if (dwProcessID != DWORD(lParam)) return TRUE;

    wchar_t Buffer[256];
    if (GetWindowTextW(hWindow, Buffer, 250) == 0) return TRUE;
    QString S = QString::fromWCharArray(Buffer);
    S = Trim(ExtUpperCase(S));
    if (Copy(S, Length(S) + 1 - Length(GlobalHideTitle), Length(GlobalHideTitle)) != GlobalHideTitle) return TRUE;

    ShowWindow(hWindow, SW_HIDE);
    return TRUE;
}
#endif

void HideWindowFromProcessIDAndTitle(THandle ProcessID, const QString& Title)
{
    GlobalHideTitle = ExtUpperCase(Title);
#ifdef Q_OS_WIN
    EnumWindows(EnumWindowsFunc2, LPARAM(ProcessID));
#else
    // TODO(port): windows of other processes cannot be hidden portably (X11/Wayland)
    Q_UNUSED(ProcessID);
#endif
}

QString CheckDOSBoxVersionDirect(int Nr, const QString& Path)
{
    QString DOSBoxPath;
    if (Trim(Path).isEmpty()) DOSBoxPath = PrgSetup->DOSBoxSettings(Nr)->DosBoxDir(); else DOSBoxPath = Path;
    DOSBoxPath = IncludeTrailingPathDelimiter(MakeAbsPath(DOSBoxPath, PrgSetup->BaseDir()));

    if (!DirectoryExists(DOSBoxPath)) return QString();

    QString TempTxt = TempDir() + QStringLiteral("D-Fend-Reloaded-DOSBoxVer.txt");
#ifdef Q_OS_WIN
    QString TempBat = TempDir() + QStringLiteral("D-Fend-Reloaded-DOSBoxVer.bat");
#else
    QString TempBat = TempDir() + QStringLiteral("D-Fend-Reloaded-DOSBoxVer.sh");
#endif

    // Create batch file to run
    {
        TStringList St;
#ifndef Q_OS_WIN
        St.Add(QStringLiteral("#!/bin/sh"));
#endif
        St.Add(QStringLiteral("\"") + DOSBoxPath + DosBoxFileName + QStringLiteral("\" -version >> \"") + TempTxt + QStringLiteral("\""));
        if (!St.SaveToFile(TempBat)) return QString();
#ifndef Q_OS_WIN
        QFile::setPermissions(TempBat, QFile::permissions(TempBat) | QFileDevice::ExeOwner | QFileDevice::ExeUser);
#endif
    }

    // Run DOSBox
    RunAndWait(TempBat, DOSBoxPath, true);
    ExtDeleteFile(TempBat, ftTemp);

    // Read list file
    if (!FileExists(TempTxt)) return QString();

    QString result;
    {
        TStringList St;
        if (!St.LoadFromFile(TempTxt)) return QString();
        if (St.Count() > 0) result = St.Text();
    }

    ExtDeleteFile(TempTxt, ftTemp);
    return result;
}

QString CheckDOSBoxVersionDirect2(int Nr, const QString& Path)
{
    QString DOSBoxPath;
    if (Trim(Path).isEmpty()) DOSBoxPath = PrgSetup->DOSBoxSettings(Nr)->DosBoxDir(); else DOSBoxPath = Path;
    DOSBoxPath = IncludeTrailingPathDelimiter(MakeAbsPath(DOSBoxPath, PrgSetup->BaseDir()));

    if (!DirectoryExists(DOSBoxPath)) return QString();

    TStringList *St = RunAndGetOutput(DOSBoxPath + DosBoxFileName, QStringLiteral("-version"), true);
    if (St == nullptr) return QString();
    QString result;
    if (St->Count() > 0) result = St->Text();
    delete St;
    return result;
}

static TStringList* FindManual(const QString& Path)
{
    TStringList *result = new TStringList();
    TSearchRec Rec;
    int I = FindFirst(Path + QStringLiteral("*.txt"), faAnyFile, Rec);
    while (I == 0) {
        QString S = Trim(ExtUpperCase(Rec.Name));
        if ((S == QLatin1String("README.TXT")) || ((Pos(QStringLiteral("MANUAL"), S) > 0) && (Pos(QStringLiteral("DOSBOX"), S) > 0)))
            result->AddObject(Rec.Name, intptr_t(Rec.Time));
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

static QString GetVersionFromDOSBoxReadmeFile(const QString& FileName)
{
    QString result, S;
    {
        TStringList St;
        if (!St.LoadFromFile(FileName)) return QString();
        for (int I = 0; I < St.Count(); I++) if (!Trim(St[I]).isEmpty()) { S = St[I]; break; }
    }

    bool B = true;
    for (int I = 1; I <= Length(S); I++) {
        const QChar C = S[I - 1];
        if ((C >= QLatin1Char('0') && C <= QLatin1Char('9')) || (C == QLatin1Char('.'))) {
            if (C == QLatin1Char('.')) {
                if (B) B = false; else continue;
            }
            result += C;
        }
    }
    return result;
}

QString CheckDOSBoxVersion(int Nr, const QString& Path)
{
    QString result, DOSBoxPath;

    if (Trim(Path).isEmpty() && (Nr >= 0)) DOSBoxPath = PrgSetup->DOSBoxSettings(Nr)->DosBoxDir(); else DOSBoxPath = Path;
    // No DOSBox folder -> no version. (The Delphi code went on and searched
    // the root of the current drive for readme files here; that is slow if
    // the drive has to wake up and can never find a DOSBox version.)
    if (Trim(DOSBoxPath).isEmpty()) return QString();
    DOSBoxPath = IncludeTrailingPathDelimiter(MakeAbsPath(DOSBoxPath, PrgSetup->BaseDir()));

    TStringList *FileNames = FindManual(DOSBoxPath);
    while (FileNames->Count() > 0) {
        int Date = FileNames->ObjectAsInt(0), J = 0;
        for (int I = 1; I < FileNames->Count(); I++) if (FileNames->ObjectAsInt(I) > Date) {
            Date = FileNames->ObjectAsInt(I); J = I;
        }
        result = GetVersionFromDOSBoxReadmeFile(DOSBoxPath + (*FileNames)[J]);
        if (!result.isEmpty()) break;
        FileNames->Delete(J);
    }
    delete FileNames;
    return result;
}

bool OldDOSBoxVersion(const QString& Version)
{
    QString S = Trim(Version);
    if (Trim(S).isEmpty()) return false;
    for (int I = 1; I <= Length(S); I++) if ((S[I - 1] == QLatin1Char(',')) || (S[I - 1] == QLatin1Char('.'))) S[I - 1] = QLatin1Char('.');
    double D;
    if (!TryStrToFloat(S, D)) return false;
    return (D < MinSupportedDOSBoxVersion - 0.0000001);
}

void DOSBoxOutdatedWarning(const QString& DOSBoxPath)
{
    QString S = FloatToStr(MinSupportedDOSBoxVersion);
    for (int I = 1; I <= Length(S); I++) if (S[I - 1] == QLatin1Char(',')) S[I - 1] = QLatin1Char('.');
    MessageDlg(Format(LanguageSetup->MessageDOSBoxOutdated(), {CheckDOSBoxVersion(-1, DOSBoxPath), S}), mtWarning, mbOK, 0);
}

TDateTime GetFileDate(const QString& FileName)
{
    // CreateFile fails for folders; FileGetDate has DOS (2 seconds) resolution
    if (!FileExists(FileName)) return 0;
    int Age = FileAge(FileName);
    if (Age == -1) return 0;
    return FileDateToDateTime(Age);
}

int GetDosFileDate(const QString& FileName)
{
    return DateTimeToFileDate(GetFileDate(FileName));
}

QString GetFileDateAsString()
{
    TDateTime D = GetFileDate(ExeFileName());
    if (D < 0.001) return QString();
    return DateToStr(D);
}

void CreateLink(const QString& TargetName, const QString& Parameters, const QString& LinkFile, const QString& IconFile, const QString& Description)
{
#ifdef Q_OS_WIN
    HRESULT Init = CoInitialize(nullptr);
    IShellLinkW *ISLink = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&ISLink)))) {
        ISLink->SetPath(reinterpret_cast<const wchar_t *>(TargetName.utf16()));
        ISLink->SetWorkingDirectory(reinterpret_cast<const wchar_t *>(ExtractFilePath(TargetName).utf16()));
        ISLink->SetArguments(reinterpret_cast<const wchar_t *>(Parameters.utf16()));
        ISLink->SetDescription(reinterpret_cast<const wchar_t *>(Description.utf16()));
        if (!IconFile.isEmpty()) ISLink->SetIconLocation(reinterpret_cast<const wchar_t *>(IconFile.utf16()), 0);

        IPersistFile *IPFile = nullptr;
        if (SUCCEEDED(ISLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&IPFile)))) {
            IPFile->Save(reinterpret_cast<const wchar_t *>(LinkFile.utf16()), FALSE);
            IPFile->Release();
        }
        ISLink->Release();
    }
    if (SUCCEEDED(Init)) CoUninitialize();
#else
    // Freedesktop .desktop file instead of a .lnk file
    QString FileName = LinkFile;
    if (ExtUpperCase(ExtractFileExt(FileName)) == QLatin1String(".LNK")) FileName = ChangeFileExt(FileName, QStringLiteral(".desktop"));
    QString Name = ChangeFileExt(ExtractFileName(LinkFile), QString());
    auto Quote = [](const QString &S) {
        QString T = S;
        T.replace(QLatin1String("\\"), QLatin1String("\\\\")).replace(QLatin1String("\""), QLatin1String("\\\""));
        return QStringLiteral("\"") + T + QStringLiteral("\"");
    };
    QString Exec = Quote(TargetName);
    if (!Trim(Parameters).isEmpty()) {
        QStringList Args = SplitCommandLine(Parameters);
        for (const QString &A : Args) Exec += QLatin1Char(' ') + Quote(A);
    }
    QString Text = QStringLiteral("[Desktop Entry]\nType=Application\nVersion=1.0\n");
    Text += QStringLiteral("Name=") + Name + QLatin1Char('\n');
    if (!Description.isEmpty()) Text += QStringLiteral("Comment=") + Description + QLatin1Char('\n');
    Text += QStringLiteral("Exec=") + Exec + QLatin1Char('\n');
    Text += QStringLiteral("Path=") + ExtractFilePath(TargetName) + QLatin1Char('\n');
    if (!IconFile.isEmpty()) Text += QStringLiteral("Icon=") + IconFile + QLatin1Char('\n');
    Text += QStringLiteral("Terminal=false\n");
    if (WriteTextFile(FileName, Text, true, false))
        QFile::setPermissions(FileName, QFile::permissions(FileName) | QFileDevice::ExeOwner | QFileDevice::ExeUser);
#endif
}

void SetStartWithWindows(bool Enabled)
{
#ifdef Q_OS_WIN
    QSettings Reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    if (Enabled) {
        Reg.setValue(QStringLiteral("D-Fend Reloaded"), ExeFileName());
    } else {
        Reg.remove(QStringLiteral("D-Fend Reloaded"));
    }
#else
    // Freedesktop autostart entry
    QString Dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
    QString FileName = Dir + QStringLiteral("/D-Fend Reloaded.desktop");
    if (Enabled) {
        ForceDirectories(Dir);
        QString Text = QStringLiteral("[Desktop Entry]\nType=Application\nName=D-Fend Reloaded\nExec=\"") + ExeFileName() +
                       QStringLiteral("\"\nTerminal=false\n");
        WriteTextFile(FileName, Text, true, false);
    } else {
        if (FileExists(FileName)) DeleteFile(FileName);
    }
#endif
}

// ---------------------------------------------------------------------------
// GIF writer (Qt can read but not write GIF images; TGIFImage could do both)
// ---------------------------------------------------------------------------

static bool WriteGIFFile(const QImage &Source, const QString &FileName)
{
    if (Source.isNull()) return false;
    QImage Img = Source.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither | Qt::PreferDither);
    if (Img.isNull()) return false;
    QList<QRgb> Colors = Img.colorTable();
    while (Colors.size() < 256) Colors.append(qRgb(0, 0, 0));

    QByteArray Out;
    auto Word = [&Out](int V) { Out.append(char(V & 0xFF)); Out.append(char((V >> 8) & 0xFF)); };
    Out.append("GIF89a", 6);
    Word(Img.width()); Word(Img.height());
    Out.append(char(0xF7)); Out.append(char(0)); Out.append(char(0));   // global color table, 256 entries
    for (int I = 0; I < 256; I++) {
        Out.append(char(qRed(Colors[I]))); Out.append(char(qGreen(Colors[I]))); Out.append(char(qBlue(Colors[I])));
    }
    Out.append(char(0x2C)); Word(0); Word(0); Word(Img.width()); Word(Img.height()); Out.append(char(0));
    Out.append(char(8));   // LZW minimum code size

    // LZW compression
    QByteArray Data;
    quint32 BitBuf = 0; int BitCount = 0;
    auto WriteCode = [&](int Code, int Size) {
        BitBuf |= quint32(Code) << BitCount; BitCount += Size;
        while (BitCount >= 8) { Data.append(char(BitBuf & 0xFF)); BitBuf >>= 8; BitCount -= 8; }
    };
    const int ClearCode = 256, EOICode = 257;
    int CodeSize = 9, NextCode = 258, Cur = -1;
    QHash<int, int> Dict;
    WriteCode(ClearCode, CodeSize);
    for (int Y = 0; Y < Img.height(); Y++) {
        const uchar *Line = Img.constScanLine(Y);
        for (int X = 0; X < Img.width(); X++) {
            int P = Line[X];
            if (Cur < 0) { Cur = P; continue; }
            int Key = (Cur << 8) | P;
            auto It = Dict.constFind(Key);
            if (It != Dict.constEnd()) { Cur = It.value(); continue; }
            WriteCode(Cur, CodeSize);
            Dict.insert(Key, NextCode); NextCode++;
            if (NextCode - 1 >= (1 << CodeSize)) CodeSize++;
            if (NextCode - 1 == 4095) { WriteCode(ClearCode, CodeSize); Dict.clear(); CodeSize = 9; NextCode = 258; }
            Cur = P;
        }
    }
    if (Cur >= 0) {
        WriteCode(Cur, CodeSize);
        // the decoder adds one more table entry after the last code
        NextCode++;
        if (NextCode - 1 >= (1 << CodeSize) && CodeSize < 12) CodeSize++;
    }
    WriteCode(ClearCode, CodeSize);
    WriteCode(EOICode, 9);
    if (BitCount > 0) Data.append(char(BitBuf & 0xFF));

    for (int I = 0; I < Data.size(); I += 255) {
        int N = qMin(255, int(Data.size()) - I);
        Out.append(char(N)); Out.append(Data.constData() + I, N);
    }
    Out.append(char(0));
    Out.append(char(0x3B));

    QFile F(FileName);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return F.write(Out) == Out.size();
}

static QImage FlattenOnWhite(const QImage &Img)
{
    if (!Img.hasAlphaChannel()) return Img.convertToFormat(QImage::Format_RGB32);
    QImage B(Img.size(), QImage::Format_RGB32);
    B.fill(Qt::white);
    QPainter P(&B);
    P.drawImage(0, 0, Img);
    P.end();
    return B;
}

static bool SaveImageToFileInt(const QPixmap& Picture, const QString& FileName)
{
    QString Ext = Trim(ExtUpperCase(ExtractFileExt(FileName)));
    if (Picture.isNull()) return false;
    QImage Img = Picture.toImage();

    if ((Ext == QLatin1String(".JPG")) || (Ext == QLatin1String(".JPEG"))) return FlattenOnWhite(Img).save(FileName, "JPG", 95);
    if (Ext == QLatin1String(".BMP")) return FlattenOnWhite(Img).save(FileName, "BMP");
    if (Ext == QLatin1String(".GIF")) return WriteGIFFile(FlattenOnWhite(Img), FileName);
    if (Ext == QLatin1String(".PNG")) return Img.save(FileName, "PNG");

    return Img.save(FileName);
}

void SaveImageToFile(const QPixmap& Picture, const QString& FileName)
{
    SaveImageToFileInt(Picture, FileName);
}

void SaveImageToFile(const QString& OldFileName, const QString& NewFileName)
{
    if (FileExists(NewFileName)) {
        if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {NewFileName}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
    }

    if (Trim(ExtUpperCase(ExtractFileExt(OldFileName))) == Trim(ExtUpperCase(ExtractFileExt(NewFileName)))) {
        if (!CopyFile(OldFileName, NewFileName, false))
            MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OldFileName, NewFileName}), mtError, mbOK, 0);
        return;
    }

    QPixmap P = LoadImageFromFile(OldFileName);
    if (P.isNull()) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OldFileName}), mtError, mbOK, 0);
        return;
    }
    if (!SaveImageToFileInt(P, NewFileName))
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {NewFileName}), mtError, mbOK, 0);
}

QIcon LoadIconFromExeFile(const QString& AFileName)
{
    if (!FileExists(AFileName)) return QIcon();

    if (ExtUpperCase(ExtractFileExt(AFileName)) == QLatin1String(".EXE")) {
        // SHGetFileInfo(..., SHGFI_ICON or SHGFI_LARGEICON)
        QFileIconProvider Provider;
        return Provider.icon(QFileInfo(AFileName));
    }

    // LoadImage(0,..,IMAGE_ICON,I,I,LR_LOADFROMFILE) for I=16..256
    QImageReader Reader(AFileName);
    if (!Reader.canRead()) return QIcon();
    QIcon result;
    do {
        QImage Img = Reader.read();
        if (Img.isNull()) break;
        result.addPixmap(QPixmap::fromImage(Img));
    } while (Reader.jumpToNextImage());
    if (result.availableSizes().isEmpty()) return QIcon();
    return result;
}

static QPixmap LargestPixmapOfIcon(const QIcon &I)
{
    QList<QSize> Sizes = I.availableSizes();
    QSize Best(32, 32);
    for (const QSize &S : Sizes) if (S.width() * S.height() > Best.width() * Best.height()) Best = S;
    return I.pixmap(Best);
}

QPixmap LoadImageFromFile(const QString& FileName)
{
    QString Ext = Trim(ExtUpperCase(ExtractFileExt(FileName)));

    if (!FileExists(FileName)) return QPixmap();

    if (Ext == QLatin1String(".BMP")) {
        QFile F(FileName);
        if (!F.open(QIODevice::ReadOnly)) return QPixmap();
        QByteArray B = F.read(2);
        if ((B.size() != 2) || (uchar(B[0]) != 0x42) || (uchar(B[1]) != 0x4D)) return QPixmap();
    }
    if (Ext == QLatin1String(".ICO")) {
        QPixmap result;
        QImageReader Reader(FileName);
        QImage Img = Reader.read();
        if (!Img.isNull()) {
            // use the largest image of the icon file
            while (Reader.jumpToNextImage()) {
                QImage Next = Reader.read();
                if (!Next.isNull() && Next.width() * Next.height() > Img.width() * Img.height()) Img = Next;
            }
            return QPixmap::fromImage(Img);
        }
        QIcon I = LoadIconFromExeFile(FileName);
        if (I.isNull()) return QPixmap();
        return LargestPixmapOfIcon(I);
    }
    if (Ext == QLatin1String(".EXE")) {
        QIcon I = LoadIconFromExeFile(FileName);
        if (I.isNull()) return QPixmap();
        return LargestPixmapOfIcon(I);
    }

    // GIF: the first frame is used (as TGIFImage did for TPicture)
    QImageReader Reader(FileName);
    Reader.setDecideFormatFromContent(true);
    QImage Img = Reader.read();
    if (Img.isNull()) return QPixmap();
    return QPixmap::fromImage(Img);
}

QIcon LoadImageAsIconFromFile(const QString& FileName)
{
    if (ExtUpperCase(ExtractFileExt(FileName)) == QLatin1String(".EXE")) return LoadIconFromExeFile(FileName);

    QPixmap P = LoadImageFromFile(FileName); if (P.isNull()) return QIcon();

    QImage B(256, 256, QImage::Format_ARGB32);
    B.fill(Qt::black);
    ScaleImage(P, B);
    B = B.convertToFormat(QImage::Format_ARGB32);
    // IL.AddMasked(B,C): pixels of the color of the bottom left pixel are transparent
    QRgb C = B.pixel(0, B.height() - 1);
    for (int Y = 0; Y < B.height(); Y++) {
        QRgb *Line = reinterpret_cast<QRgb *>(B.scanLine(Y));
        for (int X = 0; X < B.width(); X++) if (Line[X] == C) Line[X] = qRgba(0, 0, 0, 0);
    }
    return QIcon(QPixmap::fromImage(B));
}

void SetDesktopWallpaper(const QString& FileName, TWallpaperStyle WPStyle)
{
    QString S = FileName;

    if (ExtUpperCase(ExtractFileExt(S)) == QLatin1String(".PNG")) {
        QPixmap P = LoadImageFromFile(FileName);
        if (P.isNull()) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {FileName}), mtError, mbOK, 0);
            return;
        }
        S = TempDir() + ChangeFileExt(ExtractFileName(S), QStringLiteral(".bmp"));
        SaveImageToFile(P, S);
    }

#ifdef Q_OS_WIN
    // IActiveDesktop replaced by the registry settings + SystemParametersInfo
    QSettings Reg(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Desktop"), QSettings::NativeFormat);
    switch (WPStyle) {
        case WSTile: Reg.setValue(QStringLiteral("WallpaperStyle"), QStringLiteral("0")); Reg.setValue(QStringLiteral("TileWallpaper"), QStringLiteral("1")); break;
        case WSStretch: Reg.setValue(QStringLiteral("WallpaperStyle"), QStringLiteral("2")); Reg.setValue(QStringLiteral("TileWallpaper"), QStringLiteral("0")); break;
        case WSCenter:
        default: Reg.setValue(QStringLiteral("WallpaperStyle"), QStringLiteral("0")); Reg.setValue(QStringLiteral("TileWallpaper"), QStringLiteral("0")); break;
    }
    Reg.sync();
    std::wstring WS = QDir::toNativeSeparators(S).toStdWString();
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(WS.c_str()), SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
#else
    // TODO(port): there is no portable way to set the wallpaper; GNOME based desktops are supported via gsettings
    QString GSettings = QStandardPaths::findExecutable(QStringLiteral("gsettings"));
    if (GSettings.isEmpty()) return;
    QString Uri = QUrl::fromLocalFile(QFileInfo(S).absoluteFilePath()).toString();
    QString Style;
    switch (WPStyle) {
        case WSTile: Style = QStringLiteral("wallpaper"); break;
        case WSStretch: Style = QStringLiteral("stretched"); break;
        case WSCenter:
        default: Style = QStringLiteral("centered"); break;
    }
    RunProcessAndWait(GSettings, {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"), QStringLiteral("picture-uri"), Uri}, QString(), true, false);
    RunProcessAndWait(GSettings, {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"), QStringLiteral("picture-uri-dark"), Uri}, QString(), true, false);
    RunProcessAndWait(GSettings, {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"), QStringLiteral("picture-options"), Style}, QString(), true, false);
#endif
}

struct TCharsetNameRecord {
    const char *Name;
    int Nr;
};

static const TCharsetNameRecord CharsetNamesList[18] = {
    {"ANSI_CHARSET", 0},
    {"DEFAULT_CHARSET", 1},
    {"SYMBOL_CHARSET", 2},
    {"SHIFTJIS_CHARSET", 0x80},
    {"HANGEUL_CHARSET", 129},
    {"GB2312_CHARSET", 134},
    {"CHINESEBIG5_CHARSET", 136},
    {"OEM_CHARSET", 255},
    {"JOHAB_CHARSET", 130},
    {"HEBREW_CHARSET", 177},
    {"ARABIC_CHARSET", 178},
    {"GREEK_CHARSET", 161},
    {"TURKISH_CHARSET", 162},
    {"VIETNAMESE_CHARSET", 163},
    {"BALTIC_CHARSET", 186},
    {"THAI_CHARSET", 222},
    {"EASTEUROPE_CHARSET", 238},
    {"RUSSIAN_CHARSET", 204}
};

int CharsetNameToFontCharSet(const QString& Name)
{
    const int DEFAULT_CHARSET_ = 1;
    QString S = Trim(ExtUpperCase(Name));
    int Nr;
    if (!TryStrToInt(S, Nr)) Nr = -1;
    for (const TCharsetNameRecord &R : CharsetNamesList)
        if ((Trim(ExtUpperCase(QString::fromLatin1(R.Name))) == S) || (R.Nr == Nr)) return R.Nr;
    return DEFAULT_CHARSET_;
}

QString UnmapDrive(const QString& Path, TPathType PathType)
{
    QString result = Path;
    if (!WineSupportEnabled()) return result;
    switch (PathType) {
        case ptMount: if (!PrgSetup->RemapMounts()) return result; break;
        case ptScreenshot: if (!PrgSetup->RemapScreenShotFolder()) return result; break;
        case ptMapper: if (!PrgSetup->RemapMapperFile()) return result; break;
        case ptDOSBox: if (!PrgSetup->RemapDOSBoxFolder()) return result; break;
    }

    if ((Length(Path) < 2) || (Path[1] != QLatin1Char(':'))) return result;
    QChar C = Path[0].toUpper();
    if ((C < QLatin1Char('A')) || (C > QLatin1Char('Z'))) return result;
    QString S = PrgSetup->LinuxRemap(C);
    if (S.isEmpty()) return result;

    for (int I = 1; I <= Length(result); I++) if (result[I - 1] == QLatin1Char('\\')) result[I - 1] = QLatin1Char('/');
    if (S[S.length() - 1] != QLatin1Char('/')) S += QLatin1Char('/');
    result = S + Copy(result, 4, MaxInt);
    if (Copy(result, Length(result) - 1, 2) == QLatin1String("//")) result = Copy(result, 1, Length(result) - 1);
    return result;
}

#ifndef Q_OS_WIN
static bool ScreensaverAllowStatus = true;
#endif

bool GetScreensaverAllowStatus()
{
#ifdef Q_OS_WIN
    BOOL I = FALSE;
    SystemParametersInfoW(SPI_GETSCREENSAVEACTIVE, 0, &I, 0);
    return (I != 0);
#else
    // TODO(port): no portable screensaver API; the status is only remembered
    return ScreensaverAllowStatus;
#endif
}

void SetScreensaverAllowStatus(bool Enable)
{
#ifdef Q_OS_WIN
    SystemParametersInfoW(SPI_SETSCREENSAVEACTIVE, UINT(Enable), nullptr, 0);
#else
    // TODO(port): no portable screensaver API; the status is only remembered
    ScreensaverAllowStatus = Enable;
#endif
}

void SetComboHint(QComboBox* Combo)
// by Alexander Katz (skatz@svitonline.com)
{
    QString str;
    if (Combo->currentIndex() >= 0) str = Combo->itemText(Combo->currentIndex()); else str = Combo->currentText();
    int VisWidth = Combo->width() - 4;
    VisWidth = VisWidth - Combo->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, Combo);
    if (QFontMetrics(Combo->font()).horizontalAdvance(str) + 2 <= VisWidth) str = QString();
    Combo->setToolTip(str);
}

void SetComboDropDownDropDownWidth(QComboBox* Combo)
// by Alexander Katz (skatz@svitonline.com)
{
    int DroppedWidth = 0;
    QFontMetrics FM(Combo->view() != nullptr ? Combo->view()->font() : Combo->font());
    for (int i = 0; i < Combo->count(); i++) {
        int cur_width = FM.horizontalAdvance(Combo->itemText(i)) + 8;
        if (cur_width > DroppedWidth) DroppedWidth = cur_width;
    }

    if (Combo->count() > Combo->maxVisibleItems())
        DroppedWidth = DroppedWidth + Combo->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, Combo);
    QPoint pnt = Combo->mapToGlobal(QPoint(0, 0));
    QScreen *Scr = Combo->screen() != nullptr ? Combo->screen() : QGuiApplication::primaryScreen();
    int ScreenRight = Scr != nullptr ? Scr->virtualGeometry().left() + Scr->virtualGeometry().width() : pnt.x() + DroppedWidth;
    if (pnt.x() + DroppedWidth > ScreenRight) {
        if (ScreenRight - pnt.x() > Combo->width())
            DroppedWidth = ScreenRight - pnt.x();
        else
            DroppedWidth = Combo->width();
    }
    // CB_SETDROPPEDWIDTH
    if (Combo->view() != nullptr) Combo->view()->setMinimumWidth(DroppedWidth);
}

#ifdef Q_OS_WIN
static QString RegDefaultValue(QSettings &Reg)
{
    QString S = Reg.value(QStringLiteral("Default")).toString();
    if (S.isEmpty()) S = Reg.value(QStringLiteral(".")).toString();
    return S;
}

static QString WinDir()
{
    wchar_t Buffer[515];
    UINT L = GetWindowsDirectoryW(Buffer, 512);
    if (L == 0) return QString();
    return QString::fromWCharArray(Buffer, int(L));
}
#endif

static bool TranslateExtendedEnvironmentString(const QString& EnvKey, QString& EnvValue)
{
#ifdef Q_OS_WIN
    bool result = false;
    QString S = ExtUpperCase(EnvKey);

    if ((S == QLatin1String("SYSTEMROOT")) || (S == QLatin1String("%SYSTEMROOT%")) || (S == QLatin1String("WINDIR")) || (S == QLatin1String("%WINDIR%"))) {
        EnvValue = WinDir();
        result = !EnvValue.isEmpty();
        if (result) return true;
    }

    if ((S == QLatin1String("SYSTEMDRIVE")) || (S == QLatin1String("%SYSTEMDRIVE%"))) {
        EnvValue = WinDir();
        result = !EnvValue.isEmpty();
        result = result && (Length(EnvValue) >= 2); EnvValue = Copy(EnvValue, 1, 2);
        if (result) return true;
    }

    if ((S == QLatin1String("SYSTEMDIRECTORY")) || (S == QLatin1String("%SYSTEMDIRECTORY%"))) {
        EnvValue = GetSpecialFolder(0, CSIDL_SYSTEM);
        result = Length(EnvValue) >= 2; EnvValue = ExcludeTrailingPathDelimiter(EnvValue);
        if (result) return true;
    }

    if ((S == QLatin1String("PROGRAMFILES")) || (S == QLatin1String("%PROGRAMFILES%"))) {
        EnvValue = GetSpecialFolder(0, CSIDL_PROGRAM_FILES);
        result = Length(EnvValue) >= 2; EnvValue = ExcludeTrailingPathDelimiter(EnvValue);
        if (result) return true;
    }

    QSettings Reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"), QSettings::NativeFormat);
    const QStringList St = Reg.childKeys();
    for (const QString &Name : St)
        if ((ExtUpperCase(Name) == S) || (QStringLiteral("%") + ExtUpperCase(Name) + QStringLiteral("%") == S)) {
            EnvValue = ExcludeTrailingPathDelimiter(Reg.value(Name).toString());
            return true;
        }
    return false;
#else
    Q_UNUSED(EnvKey);
    Q_UNUSED(EnvValue);
    return false;
#endif
}

static QString GetEditorForIntTry1(const QString& Extension, const QString& RegKey)
{
#ifdef Q_OS_WIN
    // find type name of "*.txt" files
    QSettings Reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + Extension + QStringLiteral("\\UserChoise"), QSettings::NativeFormat);
    QString S = Reg.value(QStringLiteral("Progid")).toString();
    if (S.isEmpty()) return QString();

    // find program to open files of this type
    QSettings Reg2(QStringLiteral("HKEY_CLASSES_ROOT\\") + S + RegKey, QSettings::NativeFormat);
    return RegDefaultValue(Reg2);
#else
    Q_UNUSED(Extension);
    Q_UNUSED(RegKey);
    return QString();
#endif
}

static QString GetEditorForIntTry2(const QString& Extension, const QString& RegKey)
{
#ifdef Q_OS_WIN
    // find type name of "*.txt" files
    QSettings Reg(QStringLiteral("HKEY_CLASSES_ROOT\\") + Extension, QSettings::NativeFormat);
    QString S = RegDefaultValue(Reg);
    if (S.isEmpty()) return QString();

    // find program to open files of this type
    QSettings Reg2(QStringLiteral("HKEY_CLASSES_ROOT\\") + S + RegKey, QSettings::NativeFormat);
    return RegDefaultValue(Reg2);
#else
    Q_UNUSED(Extension);
    Q_UNUSED(RegKey);
    return QString();
#endif
}

static QString GetEditorForIntTry3(const QString& Extension, const QString& RegKey)
{
#ifdef Q_OS_WIN
    // find type name of "*.txt" files
    QSettings Reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + Extension + QStringLiteral("\\OpenWithProgids"), QSettings::NativeFormat);
    QStringList St = Reg.childKeys();
    St.removeAll(QStringLiteral("Default"));
    St.removeAll(QStringLiteral("."));
    QString T = Copy(Extension, 2, MaxInt) + QStringLiteral("file");
    QString S;
    for (int I = 0; I < St.size(); I++) if ((St[I] != T) && (!St[I].isEmpty())) { S = St[I]; break; }
    if (S.isEmpty()) {
        if (S.isEmpty()) return QString();
        S = QString(); for (int I = 0; I < St.size(); I++) if (!St[I].isEmpty()) { S = St[I]; break; }
    }

    // find program to open files of this type
    QSettings Reg2(QStringLiteral("HKEY_CLASSES_ROOT\\") + S + RegKey, QSettings::NativeFormat);
    return RegDefaultValue(Reg2);
#else
    Q_UNUSED(Extension);
    Q_UNUSED(RegKey);
    return QString();
#endif
}

static QString GetEditorFor(const QString& AExtension, bool RemoveParameters)
{
    QString Extension = AExtension, S, T;
    if (Extension.isEmpty()) return QString();
    if (Extension[0] != QLatin1Char('.')) Extension = QStringLiteral(".") + Extension;
    Extension = LowerCase(Extension);

    S = GetEditorForIntTry1(Extension, QStringLiteral("\\shell\\edit\\command"));
    if (S.isEmpty()) S = GetEditorForIntTry2(Extension, QStringLiteral("\\shell\\edit\\command"));
    if (S.isEmpty()) S = GetEditorForIntTry3(Extension, QStringLiteral("\\shell\\edit\\command"));
    if (S.isEmpty() && (Extension == QLatin1String(".txt"))) {
        S = GetEditorForIntTry1(Extension, QStringLiteral("\\shell\\open\\command"));
        if (S.isEmpty()) S = GetEditorForIntTry2(Extension, QStringLiteral("\\shell\\open\\command"));
        if (S.isEmpty()) S = GetEditorForIntTry3(Extension, QStringLiteral("\\shell\\open\\command"));
    }
    if (S.isEmpty()) return QString();

    // remove leading and trailing "
    if ((!S.isEmpty()) && (S[0] == QLatin1Char('"'))) {
        Delete(S, 1, 1);
        for (int I = 1; I <= Length(S); I++) if (S[I - 1] == QLatin1Char('"')) { Delete(S, I, 1); break; }
    }

    if (RemoveParameters) {
        // remove "%1"
        int I = Pos(QStringLiteral("\"%1\""), S); if (I > 0) S = Copy(S, 1, I - 1) + Copy(S, I + 4, MaxInt);
        I = Pos(QStringLiteral("%1"), S); if (I > 0) S = Copy(S, 1, I - 1) + Copy(S, I + 2, MaxInt);
    }

    // Translate %SystemRoot% etc.
    int I = 1;
    while (I < Length(S)) {
        if (S[I - 1] == QLatin1Char('%')) {
            int K = -1;
            for (int J = I + 1; J <= Length(S) - 1; J++) if (S[J - 1] == QLatin1Char('%')) { K = J; break; }
            if (K == -1) break;
            if (TranslateExtendedEnvironmentString(Copy(S, I, K - I + 1), T)) {
                S = Copy(S, 1, I - 1) + T + Copy(S, K + 1, MaxInt);
                continue;
            }
        }
        I++;
    }

    if ((Pos(QStringLiteral("%1"), S) == 0) && (!FileExists(S))) return QString();
    return Trim(S);
}

static void TestAndFixLineWrap(const QString& FileName)
{
    const int BufSize = 512;
    QString S = Trim(ExtUpperCase(ExtractFileExt(FileName)));
    if ((S != QLatin1String(".BAT")) && (S != QLatin1String(".TXT")) && (S != QLatin1String(".NFO")) && (S != QLatin1String(".DIZ")) &&
        (S != QLatin1String(".1ST")) && (S != QLatin1String(".INI")) && (S != QLatin1String(".PROF")) && (S != QLatin1String(".CONF"))) return;

    // #10 -> #13#10
    QFile FSt(FileName);
    if (!FSt.open(QIODevice::ReadOnly)) return;
    const QByteArray Data = FSt.readAll();
    FSt.close();

    bool NeedFix = false;
    const char LastChar = 0;
    qint64 Position = 0;
    const qint64 Size = Data.size();
    while (Position < Size - 1) {
        int I = int(qMin<qint64>(BufSize, Size - Position - 1));
        const char *Buffer = Data.constData() + Position;
        Position += I;
        for (int J = 0; J < I; J++) if (Buffer[J] == '\n') {
            if (J > 0) {
                NeedFix = (Buffer[J - 1] != '\r'); break;
            } else {
                if (LastChar != 0) {
                    NeedFix = (LastChar != '\r'); break;
                }
            }
        }
    }

    if (!NeedFix) return;

    // St.LoadFromFile + St.SaveToFile: all line breaks become #13#10
    QByteArray Out;
    Out.reserve(Data.size() + Data.size() / 20);
    for (int I = 0; I < Data.size(); I++) {
        const char C = Data[I];
        if (C == '\r') {
            Out.append("\r\n", 2);
            if (I + 1 < Data.size() && Data[I + 1] == '\n') I++;
            continue;
        }
        if (C == '\n') { Out.append("\r\n", 2); continue; }
        Out.append(C);
    }
    if (!Out.endsWith("\r\n")) Out.append("\r\n", 2);
    if (FSt.open(QIODevice::WriteOnly | QIODevice::Truncate)) FSt.write(Out);
}

QString GetDefaultEditor(const QString& Extension, bool FileTypeFallback, bool RemoveParameters)
{
    QString result = GetEditorFor(Extension, RemoveParameters);
    if (result.isEmpty() && FileTypeFallback) result = GetEditorFor(QStringLiteral(".txt"), RemoveParameters);
    if (result.isEmpty()) result = QStringLiteral("Notepad.exe");
    return result;
}

void OpenFileInEditor(const QString& FileName)
{
    if (PrgSetup->AutoFixLineWrap()) TestAndFixLineWrap(FileName);

    QString Editor = QStringLiteral("Notepad.exe");
    QString S = Trim(ExtUpperCase(PrgSetup->TextEditor()));
    if (S.isEmpty()) {
        Editor = GetDefaultEditor(ExtractFileExt(FileName), PrgSetup->FileTypeFallbackForEditor(), false);
    } else {
        if ((S != QLatin1String("NOTEPAD")) && (S != QLatin1String("NOTEPAD.EXE"))) {
            if (FileExists(MakeAbsPath(PrgSetup->TextEditor(), PrgSetup->BaseDir()))) Editor = MakeAbsPath(PrgSetup->TextEditor(), PrgSetup->BaseDir());
        }
    }

#ifndef Q_OS_WIN
    // No Notepad outside Windows: use the editor registered in the desktop environment
    if (SameText(Editor, QStringLiteral("Notepad.exe"))) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(FileName));
        return;
    }
#endif

    int I = Pos(QStringLiteral("%1"), Editor);
    if (I == 0) {
        StartProcess(Editor, QStringList() << FileName, ExtractFilePath(FileName), swNormal);
    } else {
        Editor = Copy(Editor, 1, I - 1) + FileName + Copy(Editor, I + 2, MaxInt);
        StartProcessCommandLine(Editor, ExtractFilePath(FileName), swNormal);
    }
}

bool OpenMediaFile(const QString& SetupString, const QString& FileName)
{
    QString S = Trim(ExtUpperCase(SetupString));
    if (S == QLatin1String("DEFAULT")) {
        ShellOpen(FileName, QString(), ExtractFilePath(FileName));
        return true;
    }
    if ((!S.isEmpty()) && (S != QLatin1String("INTERNAL")) && FileExists(MakeAbsPath(SetupString, PrgSetup->BaseDir()))) {
        StartProcess(MakeAbsPath(SetupString, PrgSetup->BaseDir()), QStringList() << FileName, ExtractFilePath(FileName), swNormal);
        return true;
    }

    return false;
}

qint64 GetFileSize(const QString& AFileName)
{
    qint64 result = 0;
    TSearchRec Rec;
    int I = FindFirst(AFileName, faAnyFile, Rec);
    if (I == 0) result = Rec.Size;
    FindClose(Rec);
    return result;
}

QString FileSizeToStr(qint64 Size)
{
    if (Size < 1024) return IntToStr(Size) + QStringLiteral(" Bytes");
    if (Size < 1024 * 1024) return Format(QStringLiteral("%.1fKB"), {double(Size) / 1024});
    if (Size < 1024 * 1024 * 1024) return Format(QStringLiteral("%.1fMB"), {double(Size) / 1024 / 1024});
    return Format(QStringLiteral("%.1fGB"), {double(Size) / 1024 / 1024 / 1024});
}

bool BaseDirSecuriryCheck(const QString& DirToDelete)
{
    bool result = BaseDirSecuriryCheckOnly(DirToDelete);
    if (!result) MessageDlg(Format(LanguageSetup->MessageDeleteErrorProtection(), {IncludeTrailingPathDelimiter(DirToDelete)}), mtError, mbOK, 0);
    return result;
}

bool BaseDirSecuriryCheckOnly(const QString& DirToDelete)
{
    if (!PrgSetup->DeleteOnlyInBaseDir()) return true;
    QString Folder = IncludeTrailingPathDelimiter(DirToDelete);
    if (!DirectoryExists(Folder)) return true;
    QString S = CommonToolsShortName(PrgSetup->BaseDir());
    if (ExtUpperCase(Copy(CommonToolsShortName(Folder), 1, Length(S))) == ExtUpperCase(S)) return true;
    return false;
}

static bool RecordFlag(const QString &Record, TDeleteFileType FileType)
{
    int I = int(FileType);
    return I >= 0 && I < Record.length() && Record[I] == QLatin1Char('1');
}

static bool ExtDeleteFolderOnlyInt(const QString& Dir, const QString& DeletePrgSetupRecord, TDeleteFileType FolderType)
{
    if (!DirectoryExists(Dir)) return true;

    if (RecordFlag(DeletePrgSetupRecord, FolderType)) {
        return DeleteFileToRecycleBinNoWarning(IncludeTrailingPathDelimiter(Dir));
    } else {
        return RemoveDir(IncludeTrailingPathDelimiter(Dir));
    }
}

// SHFileOperation(FO_DELETE) without FOF_ALLOWUNDO
static bool ShellDeleteFolder(const QString &Dir)
{
    QString S = ExcludeTrailingPathDelimiter(Dir);
    if (!DirectoryExists(S)) return false;
    return QDir(S).removeRecursively();
}

static bool DeleteFolderIntNoWarning(const QString& Dir, const QString& DeletePrgSetupRecord, TDeleteFileType FolderType)
{
    if (ShellDeleteFolder(Dir)) return true;

    bool result = true;
    if (!DirectoryExists(Dir)) return result;
    if (Pos(QStringLiteral(".."), Dir) != 0) return false;

    TStringList Dirs, Files;
    TSearchRec Rec;
    int I = FindFirst(Dir + QStringLiteral("*.*"), faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == faDirectory) {
            if (!Rec.Name.isEmpty() && Rec.Name[0] != QLatin1Char('.')) Dirs.Add(Rec.Name);
        } else {
            Files.Add(Rec.Name);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    for (I = 0; I < Files.Count(); I++) {
        if (RecordFlag(DeletePrgSetupRecord, FolderType)) {
            if (!DeleteFileToRecycleBin(Dir + Files[I])) result = false;
        } else {
            if (!DeleteFile(Dir + Files[I])) result = false;
        }
    }

    for (I = 0; I < Dirs.Count(); I++)
        if (!DeleteFolderIntNoWarning(Dir + Dirs[I] + PathDelim, DeletePrgSetupRecord, FolderType)) result = false;

    if (!ExtDeleteFolderOnlyInt(Dir, DeletePrgSetupRecord, FolderType)) result = false;
    return result;
}

static bool DeleteFolderInt(const QString& Dir, const QString& DeletePrgSetupRecord, TDeleteFileType FolderType, bool AllowContinueNext, bool& ContinueNext)
{
    if (ShellDeleteFolder(Dir)) return true;

    ContinueNext = false;
    bool ErrorIgnored = false;

    if (!DirectoryExists(Dir)) return true;

    if (Pos(QStringLiteral(".."), Dir) != 0) {
        MessageDlg(Format(LanguageSetup->MessageDeleteErrorDotDotInPath(), {Dir}), mtError, mbOK, 0);
        return false;
    }

    {
        TStringList Dirs, Files;
        TSearchRec Rec;
        int I = FindFirst(Dir + QStringLiteral("*.*"), faAnyFile, Rec);
        while (I == 0) {
            if ((Rec.Attr & faDirectory) == faDirectory) {
                if (!Rec.Name.isEmpty() && Rec.Name[0] != QLatin1Char('.')) Dirs.Add(Rec.Name);
            } else {
                Files.Add(Rec.Name);
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);

        for (I = 0; I < Files.Count(); I++) if (!ExtDeleteFile(Dir + Files[I], FolderType, AllowContinueNext, ContinueNext, DeletePrgSetupRecord)) {
            if (ContinueNext) ErrorIgnored = true; else return false;
        }

        for (I = 0; I < Dirs.Count(); I++) if (!DeleteFolderInt(Dir + Dirs[I] + PathDelim, DeletePrgSetupRecord, FolderType, AllowContinueNext, ContinueNext)) {
            if (ContinueNext) ErrorIgnored = true; else return false;
        }
    }

    do {
        if (!ExtDeleteFolderOnlyInt(Dir, DeletePrgSetupRecord, FolderType)) {
            if (ErrorIgnored) { ContinueNext = true; return false; }
            if (AllowContinueNext) {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {Dir}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry | mbIgnore, 0)) {
                    case mrIgnore: ContinueNext = true; return false;
                    case mrRetry: break;
                    default: return false;
                }
            } else {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {Dir}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry, 0)) {
                    case mrRetry: break;
                    default: return false;
                }
            }
        } else {
            break;
        }
    } while (true);

    ContinueNext = true;
    return true;
}

static QString DeleteRecord(const QString &DeletePrgSetupRecord)
{
    QString S;
    if (DeletePrgSetupRecord.isEmpty()) {
        if (PrgSetup == nullptr) S = QString(); else S = Copy(PrgSetup->DeleteToRecycleBin(), 1, 7);
    } else {
        S = DeletePrgSetupRecord;
    }
    S = S + Copy(QStringLiteral("1011110"), Length(S) + 1, MaxInt);
    return S;
}

bool ExtDeleteFile(const QString& FileName, TDeleteFileType FileType, bool AllowContinueNext, bool& ContinueNext, const QString& DeletePrgSetupRecord)
{
    if (!FileExists(FileName)) return true;

    QString S = DeleteRecord(DeletePrgSetupRecord);

    if (RecordFlag(S, FileType)) return DeleteFileToRecycleBin(FileName, AllowContinueNext, ContinueNext);

    do {
        if (!DeleteFile(FileName)) {
            if (AllowContinueNext) {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry | mbIgnore, 0)) {
                    case mrIgnore: ContinueNext = true; return false;
                    case mrRetry: break;
                    default: return false;
                }
            } else {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry, 0)) {
                    case mrRetry: break;
                    default: return false;
                }
            }
        } else {
            break;
        }
    } while (true);
    return true;
}

bool ExtDeleteFile(const QString& FileName, TDeleteFileType FileType, const QString& DeletePrgSetupRecord)
{
    bool B = false;
    return ExtDeleteFile(FileName, FileType, true, B, DeletePrgSetupRecord);
}

bool ExtDeleteFileWithPause(const QString& FileName, TDeleteFileType FileType, bool AllowContinueNext, bool& ContinueNext, const QString& DeletePrgSetupRecord)
{
    if (!FileExists(FileName)) return true;

    QString S = DeleteRecord(DeletePrgSetupRecord);

    if (RecordFlag(S, FileType)) return DeleteFileToRecycleBinWithPause(FileName, AllowContinueNext, ContinueNext);

    do {
        for (int I = 0; I <= 10; I++) {
            if (DeleteFile(FileName)) return true;
            Sleep(100);
        }
        if (!DeleteFile(FileName)) {
            if (AllowContinueNext) {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry | mbIgnore, 0)) {
                    case mrIgnore: ContinueNext = true; return false;
                    case mrRetry: break;
                    default: return false;
                }
            } else {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry, 0)) {
                    case mrRetry: break;
                    default: return false;
                }
            }
        } else {
            break;
        }
    } while (true);
    return true;
}

bool ExtDeleteFileWithPause(const QString& FileName, TDeleteFileType FileType, const QString& DeletePrgSetupRecord)
{
    bool B = false;
    return ExtDeleteFileWithPause(FileName, FileType, true, B, DeletePrgSetupRecord);
}

bool ExtDeleteFolder(const QString& Folder, TDeleteFileType FolderType, bool AllowContinueNext, bool& ContinueNext, const QString& DeletePrgSetupRecord)
{
    Q_UNUSED(ContinueNext);
    if (!DirectoryExists(Folder)) return true;

    QString S;
    if (DeletePrgSetupRecord.isEmpty()) {
        if (PrgSetup == nullptr) S = QString(); else S = PrgSetup->DeleteToRecycleBin();
    } else {
        S = DeletePrgSetupRecord;
    }
    S = Copy(S, 1, 7); S = S + Copy(QStringLiteral("1011110"), Length(S) + 1, MaxInt);

    if (RecordFlag(S, FolderType)) return DeleteFileToRecycleBin(Folder);

    bool B = false;
    return DeleteFolderInt(IncludeTrailingPathDelimiter(Folder), S, FolderType, AllowContinueNext, B);
}

bool ExtDeleteFolderNoWarning(const QString& Folder, TDeleteFileType FolderType)
{
    if (!DirectoryExists(Folder)) return true;

    QString S;
    if (PrgSetup == nullptr) S = QString(); else S = PrgSetup->DeleteToRecycleBin();
    S = Copy(S, 1, 7); S = S + Copy(QStringLiteral("1011110"), Length(S) + 1, MaxInt);

    if (RecordFlag(S, FolderType)) return DeleteFileToRecycleBin(Folder);

    return DeleteFolderIntNoWarning(IncludeTrailingPathDelimiter(Folder), S, FolderType);
}

bool ExtDeleteFolder(const QString& Folder, TDeleteFileType FolderType, const QString& DeletePrgSetupRecord)
{
    bool B = false;
    return ExtDeleteFolder(Folder, FolderType, true, B, DeletePrgSetupRecord);
}

// SHFileOperation(FO_DELETE, FOF_ALLOWUNDO)
static bool MoveToRecycleBin(const QString &FileName)
{
    return QFile::moveToTrash(ExcludeTrailingPathDelimiter(FileName));
}

bool DeleteFileToRecycleBin(const QString& FileName, bool AllowContinueNext, bool& ContinueNext)
{
    if ((!FileExists(ExcludeTrailingPathDelimiter(FileName))) && (!DirectoryExists(FileName))) return true;

    do {
        if (!MoveToRecycleBin(FileName)) {
            if (AllowContinueNext) {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry | mbIgnore, 0)) {
                    case mrIgnore: ContinueNext = true; return false;
                    case mrRetry: break;
                    default: return false;
                }
            } else {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry, 0)) {
                    case mrRetry: break;
                    default: return false;
                }
            }
        } else {
            break;
        }
    } while (true);
    return true;
}

bool DeleteFileToRecycleBin(const QString& FileName)
{
    bool B = false;
    return DeleteFileToRecycleBin(FileName, true, B);
}

bool DeleteFileToRecycleBinWithPause(const QString& FileName, bool AllowContinueNext, bool& ContinueNext)
{
    if ((!FileExists(ExcludeTrailingPathDelimiter(FileName))) && (!DirectoryExists(FileName))) return true;

    do {
        for (int I = 0; I <= 10; I++) {
            if (MoveToRecycleBin(FileName)) return true;
            Sleep(100);
        }
        if (!MoveToRecycleBin(FileName)) {
            if (AllowContinueNext) {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry | mbIgnore, 0)) {
                    case mrIgnore: ContinueNext = true; return false;
                    case mrRetry: break;
                    default: return false;
                }
            } else {
                switch (MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}) + QStringLiteral("\n\n") + LastSysErrorMessage(), mtError, mbAbort | mbRetry, 0)) {
                    case mrRetry: break;
                    default: return false;
                }
            }
        } else {
            break;
        }
    } while (true);
    return true;
}

bool DeleteFileToRecycleBinNoWarning(const QString& FileName)
{
    if ((!FileExists(ExcludeTrailingPathDelimiter(FileName))) && (!DirectoryExists(FileName))) return true;
    return MoveToRecycleBin(FileName);
}

bool CompareFiles(const QString& File1, const QString& File2)
{
    const int BSize = 4096;
    QFile FSt1(File1), FSt2(File2);
    if (!FSt1.open(QIODevice::ReadOnly)) return false;
    if (!FSt2.open(QIODevice::ReadOnly)) return false;
    if (FSt1.size() != FSt2.size()) return false;
    qint64 C = FSt1.size();
    while (C > 0) {
        qint64 I = qMin<qint64>(C, BSize);
        QByteArray B1 = FSt1.read(I);
        QByteArray B2 = FSt2.read(I);
        if (B1.size() != I || B2.size() != I) return false;
        if (B1 != B2) return false;
        C -= I;
    }
    return true;
}

static TStringList* GetFileList(const QString& Folder, const QStringList& Exts)
{
    TStringList *result = new TStringList();

    for (const QString &Ext : Exts) {
        TSearchRec Rec;
        int J = FindFirst(IncludeTrailingPathDelimiter(Folder) + QStringLiteral("*.") + Ext, faAnyFile, Rec);
        while (J == 0) {
            if ((Rec.Attr & faDirectory) == 0) result->Add(Rec.Name);
            J = FindNext(Rec);
        }
        FindClose(Rec);
    }

    result->Sort();
    return result;
}

bool RenameAllFiles(const QString& Folder, const QString& Expression, const QString& GameName, TRenameFiles RenameFiles, bool ShowNoFilesInfo)
{
    static const QStringList RequiredPlaceholders = {QStringLiteral("%N"), QStringLiteral("%E")};
    static const QStringList ScreenshotExts = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"), QStringLiteral("bmp")};
    static const QStringList SoundsExts = {QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("mid"), QStringLiteral("midi")};
    static const QStringList VideosExts = {QStringLiteral("avi"), QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("wmv"), QStringLiteral("asf")};

    QString Dir = IncludeTrailingPathDelimiter(Folder);

    // Expression ok?
    for (const QString &P : RequiredPlaceholders) if (Pos(P, Expression) == 0) {
        MessageDlg(Format(LanguageSetup->RenameAllPlaceholderError(), {P}), mtError, mbOK, 0);
        return false;
    }

    // Create list of files to rename
    TStringList *St;
    switch (RenameFiles) {
        case rfScreenshots: St = GetFileList(Dir, ScreenshotExts); break;
        case rfSounds: St = GetFileList(Dir, SoundsExts); break;
        case rfVideos: St = GetFileList(Dir, VideosExts); break;
        default: return false;
    }

    // Process files
    if (St->Count() == 0) {
        if (ShowNoFilesInfo) MessageDlg(Format(LanguageSetup->RenameAllNoFiles(), {Folder}), mtInformation, mbOK, 0);
        delete St;
        return true;
    }
    QString Template = Expression;
    int I = Pos(QStringLiteral("%P"), Template);
    if (I > 0) Template = Copy(Template, 1, I - 1) + MakeFileSysOKFolderName(GameName) + Copy(Template, I + 2, MaxInt);

    const int Digits = int(std::ceil(std::log10(double(St->Count() + 1))));
    for (I = 0; I < St->Count(); I++) {
        QString NewName = Template;
        QString Ext = ExtractFileExt((*St)[I]); if ((!Ext.isEmpty()) && (Ext[0] == QLatin1Char('.'))) Ext = Copy(Ext, 2, MaxInt);
        QString Nr = IntToStr(I + 1); while (Length(Nr) < Digits) Nr = QStringLiteral("0") + Nr;
        int J = Pos(QStringLiteral("%N"), NewName); if (J > 0) NewName = Copy(NewName, 1, J - 1) + Nr + Copy(NewName, J + 2, MaxInt);
        J = Pos(QStringLiteral("%E"), NewName); if (J > 0) NewName = Copy(NewName, 1, J - 1) + Ext + Copy(NewName, J + 2, MaxInt);

        // File has already correct name?
        if ((*St)[I] == NewName) continue;

        // File with new name already existing?
        for (J = I + 1; J < St->Count(); J++) {
            if (ExtUpperCase(NewName) == ExtUpperCase((*St)[J])) {
                QString S = ExtractFileExt((*St)[J]); if (S.isEmpty()) S = QStringLiteral(".");
                S = Copy((*St)[J], 1, Length((*St)[J]) - Length(S)) + QStringLiteral("_") + IntToStr(J + 1) + S;
                RenameFile(Dir + (*St)[J], Dir + S);
                (*St)[J] = S;
            }
        }

        // Rename file
        RenameFile(Dir + (*St)[I], Dir + NewName);
    }
    delete St;
    return true;
}

// This method was discovered by user DRON from www.delphikingdom.com site
bool ForceForegroundWindow(WId Wnd)
{
#ifdef Q_OS_WIN
    INPUT Input;
    ZeroMemory(&Input, sizeof(Input));
    SendInput(1, &Input, sizeof(INPUT));
    return SetForegroundWindow(reinterpret_cast<HWND>(Wnd)) != 0;
#else
    QWidget *W = QWidget::find(Wnd);
    if (W == nullptr) return false;
    W = W->window();
    W->raise();
    W->activateWindow();
    return true;
#endif
}

int CPUCount()
{
    return QThread::idealThreadCount();
}

bool IsRemoteSession()
{
#ifdef Q_OS_WIN
    return (GetSystemMetrics(SM_REMOTESESSION) != 0);
#else
    return false;
#endif
}

QString TempPrgDir = QStringLiteral("");
