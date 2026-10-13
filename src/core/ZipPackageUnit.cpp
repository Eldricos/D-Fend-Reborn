// C++ port of ZipPackageUnit.pas (implementation section).
//
// The archive work itself (SevenZipVCL in the original) is done by
// ZipInfoFormUnit (ExtractZipFile / CreateZipFile), which drives the external
// 7-Zip program.
#include "ZipPackageUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "UninstallFormUnit.h"
#include "ZipInfoFormUnit.h"
#include "DosBoxUnit.h"
#include "ScummVMUnit.h"
#include "HashCalc.h"
#include "ImportSelectTemplateFormUnit.h"
#include "SelectTemplateForZipImportFormUnit.h"
#include "InstallationSupportFormUnit.h"
#include "PackageDBToolsUnit.h"
#include "ZipPackageDBGLFormUnit.h"
#include "SmallWaitFormUnit.h"

#ifndef CSIDL_WINDOWS
#define CSIDL_WINDOWS 0x0024
#endif
#ifndef CSIDL_DESKTOPDIRECTORY
#define CSIDL_DESKTOPDIRECTORY 0x0010
#endif

// ---------------------------------------------------------------------------
// Port helpers
// ---------------------------------------------------------------------------

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    for (QWidget* W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return nullptr;
}

static WId MainFormHandle()
{
    QWidget* W = ApplicationMainForm();
    return (W != nullptr) ? W->winId() : WId(0);
}

// Application.MainForm.Enabled:=False (returns the old state)
static bool DisableMainForm()
{
    bool B = false;
    QWidget* W = ApplicationMainForm();
    if (W != nullptr) {
        B = W->isEnabled();
        W->setEnabled(false);
    }
    return B;
}

static void RestoreMainForm(bool B)
{
    QWidget* W = ApplicationMainForm();
    if (W != nullptr) {
        // While PeekMessage(Msg,Application.MainForm.Handle,WM_INPUT,WM_INPUT,1) do ;
        // (dropping input queued while disabled) is not needed with Qt.
        W->setEnabled(B);
    }
}

static bool IsDelim(QChar C)
{
    return C == '\\' || C == '/';
}

// Pos('\',S) for file system paths (both delimiters)
static int PosDelim(const QString& S)
{
    for (int I = 0; I < S.length(); I++) if (IsDelim(S[I])) return I + 1;
    return 0;
}

// "Copy(S,2,2)=':\'" - the path could not be made relative to the base dir
static bool IsNotRelativePath(const QString& S)
{
    if (Copy(S, 2, 2) == ":\\" || Copy(S, 2, 2) == ":/") return true;
#ifndef Q_OS_WIN
    if (S.startsWith('/')) return true;
#endif
    return false;
}

// "Copy(S,1,2)='.\'"
static bool StartsWithDotDelim(const QString& S)
{
    return Length(S) >= 2 && S[0] == '.' && IsDelim(S[1]);
}

// T:=ExtractFilePath(S); remove leading '.', leading '\' and trailing '\'
static QString CleanRelDir(const QString& S)
{
    QString T = ExtractFilePath(S);
    if ((T != "") && (T[0] == '.')) Delete(T, 1, 1);
    if ((T != "") && IsDelim(T[0])) Delete(T, 1, 1);
    if ((T != "") && IsDelim(T[Length(T) - 1])) Delete(T, Length(T), 1);
    return T;
}

// While Pos('\',T)>0 do T:=Copy(T,Pos('\',T)+1,MaxInt);
static QString LastPathPart(QString T)
{
    while (PosDelim(T) > 0) T = Copy(T, PosDelim(T) + 1, MaxInt);
    return T;
}

static QList<QDomElement> ChildElements(const QDomElement& N)
{
    QList<QDomElement> result;
    for (QDomNode C = N.firstChild(); !C.isNull(); C = C.nextSibling()) if (C.isElement()) result.append(C.toElement());
    return result;
}

// ---------------------------------------------------------------------------

static bool CopyFileWithMsg(const QString& Source, const QString& Dest, bool ProcessMessages_, bool MoveFileToDestDir)
{
    bool result = ForceDirectories(ExtractFilePath(Dest));
    if (result) { if (FileExists(Dest)) return result; }
    if (result) {
        if (MoveFileToDestDir) {
            result = RenameFile(Source, Dest);
            if (!result) result = CopyFile(Source, Dest, false);
        } else {
            result = CopyFile(Source, Dest, false);
        }
    }
    if (ProcessMessages_) ProcessMessages();
    if (!result) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {Source, Dest}), mtError, mbOK, 0);
    return result;
}

static bool CopyFolderWithMsg(QString Source, QString Dest, bool ProcessMessages_, bool MoveFiles, TGame* NewGame = nullptr, bool IgnoreProfConfInRootDir = false)
{
    int I, Nr;
    TSearchRec Rec;
    QString S, T;
    bool B;

    Source = IncludeTrailingPathDelimiter(Source);
    Dest = IncludeTrailingPathDelimiter(Dest);

    if (!DirectoryExists(Source)) return true;

    if (!ForceDirectories(Dest)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dest}), mtError, mbOK, 0);
        return false;
    }

    TStringList StDirs1, StDirs2, StFiles;

    // Scan for files and folders
    I = FindFirst(Source + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) != 0) {
            if ((Rec.Name != ".") && (Rec.Name != "..")) {
                B = false;
                if ((NewGame != nullptr) && DirectoryExists(Dest + Rec.Name)) {
                    // Change path if destination capture or data folder already exists
                    S = IncludeTrailingPathDelimiter(ExtUpperCase(Dest + Rec.Name));
                    if (IncludeTrailingPathDelimiter(ExtUpperCase(MakeAbsPath(NewGame->CaptureFolder(), PrgSetup->BaseDir()))) == S) {
                        S = IncludeTrailingPathDelimiter(Dest + Rec.Name);
                        Nr = 0;
                        while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(IncludeTrailingPathDelimiter(S))) {
                            Nr++; S = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(Dest + Rec.Name) + IntToStr(Nr));
                        }
                        StDirs1.Add(Source + Rec.Name); StDirs2.Add(S);
                        NewGame->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir(), true)); NewGame->StoreAllValues();
                        B = true;
                    } else {
                        if (IncludeTrailingPathDelimiter(ExtUpperCase(MakeAbsPath(NewGame->DataDir(), PrgSetup->BaseDir()))) == S) {
                            S = IncludeTrailingPathDelimiter(Dest + Rec.Name);
                            Nr = 0;
                            while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(IncludeTrailingPathDelimiter(S))) {
                                Nr++; S = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(Dest + Rec.Name) + IntToStr(Nr));
                            }
                            StDirs1.Add(Source + Rec.Name); StDirs2.Add(S);
                            NewGame->SetDataDir(MakeRelPath(S, PrgSetup->BaseDir(), true)); NewGame->StoreAllValues();
                            B = true;
                        }
                    }
                }
                if (!B) {
                    StDirs1.Add(Source + Rec.Name); StDirs2.Add(Dest + Rec.Name);
                }
            }
        } else {
            if (IgnoreProfConfInRootDir) {
                T = Trim(ExtUpperCase(Rec.Name)); S = ExtractFileExt(T);
                B = (S != ".CONF") && (S != ".PROF") && (T != "INSTALLREADME.TXT") && (T != ExtUpperCase(DBGLPackageInfoFile));
            } else B = true;
            if (B) StFiles.Add(Rec.Name);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    // Copy files
    for (I = 0; I < StFiles.Count(); I++) if (!CopyFileWithMsg(Source + StFiles[I], Dest + StFiles[I], ProcessMessages_, MoveFiles)) return false;

    // Copy subfolders
    for (I = 0; I < StDirs1.Count(); I++) if (!CopyFolderWithMsg(StDirs1[I], StDirs2[I], ProcessMessages_, MoveFiles, NewGame)) return false;

    return true;
}

static bool CreateInfoFile(TGame* Game, const QString& Dir)
{
    TStringList St;
    St.Add("This is a D-Fend Reborn game package for the game");
    St.Add("\"" + Game->CacheName + "\".");
    St.Add("");
    St.Add("If you are using D-Fend Reborn, you can just install this package via");
    St.Add("File|Import|Import zip-file or by just dragging the zip file to the");
    St.Add("D-Fend Reborn program window.");
    St.Add("");
    St.Add("If you do not have a D-Fend Reborn installation on your system, you can");
    St.Add("download it here for free:");
    St.Add(DFRHomepage + "Download.html");
    St.Add("");
    if (ScummVMMode(Game)) {
        St.Add("You can also run the game from this package without having D-Fend Reborn");
        St.Add("installed. All you need is ScummVM (http:/" "/www.scummvm.org/). You can just use");
        St.Add("the ini file from this folder to run the game inside ScummVM.");
    }
    if (DOSBoxMode(Game)) {
        St.Add("You can also run the game from this package without having D-Fend Reborn");
        St.Add("installed. All you need is DOSBox (http:/" "/www.dosbox.com). You can just use");
        St.Add("the conf file from this folder to run the game inside DOSBox.");
    }
    St.Add("");
    St.Add("You can find all files needed to run the game in the subfolders of this folder.");
    if (!St.SaveToFile(IncludeTrailingPathDelimiter(Dir) + "InstallReadme.txt")) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {IncludeTrailingPathDelimiter(Dir) + "InstallReadme.txt"}), mtError, mbOK, 0);
        return false;
    }

    return true;
}

static bool CopyFilesToTempDirWork(TGame* Game, const QString& Dir, bool ProcessMessages_)
{
    QString S, T, U, V;
    TStringList* St;
    TStringList* Files;
    TStringList* Folders;
    int I;

    Game->StoreAllValues();

    // prof file
    if (!CopyFileWithMsg(Game->SetupFile(), Dir + ExtractFileName(Game->SetupFile()), ProcessMessages_, false)) return false;

    // conf or ini file
    S = Dir + ChangeFileExt(ExtractFileName(Game->SetupFile()), ".conf");
    if (DOSBoxMode(Game)) {
        St = BuildConfFile(Game, false, false, -1, nullptr, true);
        if (St != nullptr) {
            bool OK = St->SaveToFile(S);
            delete St;
            if (!OK) { MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {S}), mtError, mbOK, 0); return false; }
        }
    }
    if (ScummVMMode(Game)) {
        St = BuildScummVMIniFile(Game);
        bool OK = (St != nullptr) && St->SaveToFile(S);
        delete St;
        if (!OK) { MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {S}), mtError, mbOK, 0); return false; }
    }

    // *** SCUMMVM ***
    if (ScummVMMode(Game)) {

        // ScummVM: Games folder
        S = Trim(Game->ScummVMPath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir(), true));
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
        }

        // ScummVM: Folder as zip file
        S = Trim(Game->ScummVMZip());
        if (S != "") {
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            T = CleanRelDir(S);
            if (!CopyFileWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), Dir + T + PathDelim + ExtractFileName(S), ProcessMessages_, false)) return false;
        }
        S = Trim(Game->ScummVMSavePath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir(), true));
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
        }

    }

    // *** DOSBOX ***
    if (DOSBoxMode(Game)) {
        // DOSBox: Games folder
        S = Trim(Game->GameExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S == "") S = Trim(Game->SetupExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(ExtractFilePath(S), PrgSetup->BaseDir()));
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
        }
    }

    // *** WINDOWS ***
    if (WindowsExeMode(Game)) {
        S = Trim(Game->GameExe());
        if (S == "") S = Trim(Game->SetupExe());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(ExtractFilePath(S), PrgSetup->BaseDir()));
            if (IsNotRelativePath(S)) {
                if (ExtUpperCase(IncludeTrailingPathDelimiter(GetSpecialFolder(MainFormHandle(), CSIDL_WINDOWS))) == ExtUpperCase(IncludeTrailingPathDelimiter(S))) {
                    MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                    return false;
                }
                S = MakeAbsPath(S, PrgSetup->BaseDir());
                T = LastPathPart(ExcludeTrailingPathDelimiter(S));
                U = ExcludeTrailingPathDelimiter(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
                if (StartsWithDotDelim(U)) U = Copy(U, 3, MaxInt); else {
                    U = LastPathPart(U);
                }
                T = IncludeTrailingPathDelimiter(Dir) + U + PathDelim + T + PathDelim;
                if (!CopyFolderWithMsg(S, T, ProcessMessages_, false)) return false;
            } else {
                if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
            }
        }
    }

    // Screenshots
    S = Trim(Game->CaptureFolder());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir(), true));
        if (IsNotRelativePath(S)) {
            MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
            return false;
        }
        if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
    }

    // Icon
    S = MakeAbsIconName(Game->Icon());
    if ((S != "") && FileExists(S)) {
        if (ExtractFilePath(Game->Icon()) == "") {
            if (!CopyFileWithMsg(S, Dir + IconsSubDir + PathDelim + Trim(Game->Icon()), ProcessMessages_, false)) return false;
        } else {
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            T = CleanRelDir(S);
            if (!CopyFileWithMsg(MakeAbsIconName(Game->Icon()), Dir + T + PathDelim + ExtractFileName(Game->Icon()), ProcessMessages_, false)) return false;
        }
    }

    // Game data files
    S = Trim(Game->DataDir());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(MakeRelPath(S, PrgSetup->BaseDir(), true));
        if (IsNotRelativePath(S)) {
            MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
            return false;
        }
        if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) return false;
    }

    // Extra files
    S = Trim(Game->ExtraFiles());
    if (S != "") {
        St = ValueToList(S);
        for (I = 0; I < St->Count(); I++) if (Trim((*St)[I]) != "") {
            S = MakeRelPath((*St)[I], PrgSetup->BaseDir());
            if (IsNotRelativePath(S)) {
                if ((!WindowsExeMode(Game)) || (ExtUpperCase(IncludeTrailingPathDelimiter(GetSpecialFolder(MainFormHandle(), CSIDL_WINDOWS))) == ExtUpperCase(IncludeTrailingPathDelimiter(S)))) {
                    MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                    delete St;
                    return false;
                }
                S = MakeAbsPath(S, PrgSetup->BaseDir());
                V = ExtractFileName(S); S = ExtractFilePath(S);
                T = LastPathPart(ExcludeTrailingPathDelimiter(S));
                T = IncludeTrailingPathDelimiter(Dir) + T + PathDelim;
                if (!CopyFileWithMsg(IncludeTrailingPathDelimiter(S) + V, IncludeTrailingPathDelimiter(T) + V, ProcessMessages_, false)) { delete St; return false; }
            } else {
                T = CleanRelDir(S);
                if (!CopyFileWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), Dir + T + PathDelim + ExtractFileName(S), ProcessMessages_, false)) { delete St; return false; }
            }
        }
        delete St;
    }

    // Extra folders
    S = Trim(Game->ExtraDirs());
    if (S != "") {
        St = ValueToList(S);
        for (I = 0; I < St->Count(); I++) if (Trim((*St)[I]) != "") {
            S = IncludeTrailingPathDelimiter(MakeRelPath(IncludeTrailingPathDelimiter((*St)[I]), PrgSetup->BaseDir(), true));
            if (IsNotRelativePath(S)) {
                if ((!WindowsExeMode(Game)) || (ExtUpperCase(IncludeTrailingPathDelimiter(GetSpecialFolder(MainFormHandle(), CSIDL_WINDOWS))) == ExtUpperCase(IncludeTrailingPathDelimiter(S)))) {
                    MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                    delete St;
                    return false;
                }
                S = MakeAbsPath(S, PrgSetup->BaseDir());
                T = LastPathPart(ExcludeTrailingPathDelimiter(S));
                T = IncludeTrailingPathDelimiter(Dir) + T + PathDelim;
                if (!CopyFolderWithMsg(S, T, ProcessMessages_, false)) { delete St; return false; }
            } else {
                if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) { delete St; return false; }
            }
        }
        delete St;
    }

    // Drives (images and zip files)
    Files = nullptr; Folders = nullptr;
    FileAndFoldersFromDrives(Game, Files, Folders);
    {
        bool OK = true;
        for (I = 0; OK && I < Files->Count(); I++) {
            S = MakeRelPath((*Files)[I], PrgSetup->BaseDir());
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                OK = false; break;
            }
            T = CleanRelDir(S);
            if (!CopyFileWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), Dir + T + PathDelim + ExtractFileName(S), ProcessMessages_, false)) { OK = false; break; }
        }
        for (I = 0; OK && I < Folders->Count(); I++) {
            S = IncludeTrailingPathDelimiter(MakeRelPath(IncludeTrailingPathDelimiter((*Folders)[I]), PrgSetup->BaseDir(), true));
            if (IsNotRelativePath(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                OK = false; break;
            }
            if (!CopyFolderWithMsg(MakeAbsPath(S, PrgSetup->BaseDir()), MakeAbsPath(S, Dir), ProcessMessages_, false)) { OK = false; break; }
        }
        delete Files;
        delete Folders;
        if (!OK) return false;
    }

    CreateInfoFile(Game, Dir);
    return true;
}

static bool CopyFilesToTempDir(TGame* Game, const QString& Dir, bool ProcessMessages_)
{
    LoadAndShowSmallWaitForm(LanguageSetup->MenuFileExportBuildZipPackagesWait());
    bool result = CopyFilesToTempDirWork(Game, Dir, ProcessMessages_);
    FreeSmallWaitForm();
    return result;
}

static void SplitLargeFile(const QString& AFileName)
{
    const qint64 BufferSize = 2 * 1024 * 1024;
    int Nr;

    if (!FileExists(AFileName)) return;

    LoadAndShowSmallWaitForm(LanguageSetup->MenuFileExportBuildZipPackagesSplit());
    {
        QByteArray P;
        QFile Input(AFileName);
        if (Input.open(QIODevice::ReadOnly)) {
            Nr = 1;
            QFile* Output = new QFile(AFileName + ".part" + IntToStr(Nr));
            // TFileStream.Create raises an exception if the file can not be created
            bool OutputOK = Output->open(QIODevice::WriteOnly | QIODevice::Truncate);
            while (OutputOK && Input.pos() < Input.size()) {
                qint64 Count = qMin<qint64>(PackageMaxFileSize - Output->size(), qMin<qint64>(Input.size() - Input.pos(), BufferSize));
                if (Count > 0) {
                    P = Input.read(Count);
                    if (P.isEmpty()) break;
                    Output->write(P);
                    Output->flush();
                }
                if (Output->size() == PackageMaxFileSize) {
                    delete Output;
                    Nr++; Output = new QFile(AFileName + ".part" + IntToStr(Nr));
                    OutputOK = Output->open(QIODevice::WriteOnly | QIODevice::Truncate);
                }
            }
            if (!OutputOK) {
                QString FailedFile = Output->fileName();
                delete Output;
                Input.close();
                FreeSmallWaitForm();
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {FailedFile}), mtError, mbOK);
                return;   // keep the original file, it was not split completely
            }
            delete Output;
            Input.close();
        }

        DeleteFile(AFileName);
    }
    FreeSmallWaitForm();
}

void BuildZipPackage(QWidget* AOwner, TGame* AGame, const QString& AFileName, bool SplitLargeFiles)
{
    QString Temp;
    bool B;

    if (WindowsExeMode(AGame) && (!PrgSetup->AllowPackingWindowsGames())) return;

    Temp = IncludeTrailingPathDelimiter(TempDir() + TempSubFolder);
    if (!ForceDirectories(Temp)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Temp}), mtError, mbOK, 0);
        return;
    }

    B = DisableMainForm();
    if (CopyFilesToTempDir(AGame, Temp, true)) {
        CreateZipFile(AOwner, AFileName, Temp, dmNo, GetCompressStrengthFromPrgSetup());
        if (SplitLargeFiles && (GetFileSize(AFileName) > PackageMaxFileSize)) SplitLargeFile(AFileName);
    }
    RestoreMainForm(B);

    // ChDir: leave the temp folder so that it can be deleted
    const QString Desktop = GetSpecialFolder(MainFormHandle(), CSIDL_DESKTOPDIRECTORY);
    if (Desktop != "") SetCurrentDir(Desktop); else SetCurrentDir(QDir::homePath());
    ExtDeleteFolder(Temp, ftTemp);
}

static QString GetProfFileName(const QString& Dir)
{
    TSearchRec Rec;
    QString result = "";
    int I = FindFirst(IncludeTrailingPathDelimiter(Dir) + "*.prof", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) result = IncludeTrailingPathDelimiter(Dir) + Rec.Name;
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

static TStringList* GetPrgFilesFromFolder(QString Folder)
{
    TSearchRec Rec;
    int I;

    Folder = IncludeTrailingPathDelimiter(Folder);

    TStringList* result = new TStringList;

    I = FindFirst(Folder + "*.exe", faAnyFile, Rec);
    while (I == 0) {
        if ((ExtUpperCase(Rec.Name) != "DOS4GW.EXE") && (ExtUpperCase(Copy(Rec.Name, 1, 6)) != "README")) result->Add(Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    I = FindFirst(Folder + "*.com", faAnyFile, Rec);
    while (I == 0) {
        if (ExtUpperCase(Copy(Rec.Name, 1, 6)) != "README") result->Add(Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    I = FindFirst(Folder + "*.bat", faAnyFile, Rec);
    while (I == 0) {
        if (ExtUpperCase(Copy(Rec.Name, 1, 6)) != "README") result->Add(Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    return result;
}

static void RatePrgFiles(TStringList* Files)
{
    int J;
    QString S;
    for (int I = 0; I < Files->Count(); I++) {
        S = ExtUpperCase((*Files)[I]);
        J = 0;
        if (Copy(S, 1, 5) == "GAME.") J = 10;
        if (Copy(S, 1, 6) == "SETUP.") J = -10;
        if (S == "RUNME.BAT") J = 20;
        if (S == "RUN.BAT") J = 20;
        if ((J == 0) && (Copy(S, Length(S) - 3, 4) == ".BAT")) J = 1;
        Files->SetObjects(I, reinterpret_cast<void*>(static_cast<intptr_t>(J)));
    }
}

bool AdditionalDataMatching(const QString& Folder, TGame* Template)
{
    QString F, C;
    bool NotMatching;
    bool result = false; NotMatching = false;
    for (int I = 1; I <= 5; I++) {
        switch (I) {
            case 1: F = Template->AddtionalChecksumFile1(); C = Template->AddtionalChecksumFile1Checksum(); break;
            case 2: F = Template->AddtionalChecksumFile2(); C = Template->AddtionalChecksumFile2Checksum(); break;
            case 3: F = Template->AddtionalChecksumFile3(); C = Template->AddtionalChecksumFile3Checksum(); break;
            case 4: F = Template->AddtionalChecksumFile4(); C = Template->AddtionalChecksumFile4Checksum(); break;
            case 5: F = Template->AddtionalChecksumFile5(); C = Template->AddtionalChecksumFile5Checksum(); break;
        }
        if (F == "") continue;
        if (FileExists(Folder + F)) {
            if (GetMD5Sum(Folder + F, false) == C) result = true; else NotMatching = true;
        }
    }
    if (NotMatching) result = false;
    return result;
}

TStringList* GetTemplateFromFolderExt(bool IsFolderScan, const QString& Folder_, TGameDB* AutoSetupDB, TStringList* StartableFiles)
{
    Q_UNUSED(IsFolderScan);
    TStringList* PrgFiles;
    int I, J, LastLevel, NewLevel;
    QString GameCheckSum, S;
    bool B, Delete_;

    TStringList* result = new TStringList;

    QString Folder = IncludeTrailingPathDelimiter(Folder_);

    PrgFiles = GetPrgFilesFromFolder(Folder);
    RatePrgFiles(PrgFiles);
    if (StartableFiles != nullptr) { StartableFiles->Clear(); StartableFiles->AddStrings(*PrgFiles); }

    // Find all matching auto setup templates
    LastLevel = 1000; NewLevel = -1;
    do {
        for (I = 0; I < PrgFiles->Count(); I++) { J = PrgFiles->ObjectAsInt(I); if ((J < LastLevel) && (J > NewLevel)) NewLevel = J; }
        if (NewLevel == -1) break;

        for (I = 0; I < PrgFiles->Count(); I++) if (PrgFiles->ObjectAsInt(I) == NewLevel) {
            GameCheckSum = GetMD5Sum(Folder + (*PrgFiles)[I], false);
            for (J = 0; J < AutoSetupDB->Count(); J++)
                if ((Trim(ExtUpperCase(AutoSetupDB->Game(J)->GameExe())) == Trim(ExtUpperCase((*PrgFiles)[I]))) && (AutoSetupDB->Game(J)->GameExeMD5() != "") && (AutoSetupDB->Game(J)->GameExeMD5() == GameCheckSum)) {
                    result->AddObject((*PrgFiles)[I], static_cast<intptr_t>(J));
                }
        }
        LastLevel = NewLevel; NewLevel = -1;
    } while (true);

    // If there is more than one, use additional checksums
    if (result->Count() > 1) {
        QList<int> MatchByAdditionalData;
        B = false;
        for (I = 0; I < result->Count(); I++) if (AdditionalDataMatching(Folder, AutoSetupDB->Game(result->ObjectAsInt(I)))) {
            MatchByAdditionalData.append(1); B = true;
        } else {
            MatchByAdditionalData.append(0);
        }
        if (B) {
            I = 0;
            while (I < result->Count()) if (MatchByAdditionalData[I] == 0) {
                // Only delete sams PrgFiles records
                Delete_ = false;
                S = (*result)[I];
                for (J = 0; J < result->Count(); J++) if (I != J) if (((*result)[J] == S) && (MatchByAdditionalData[I] != 0)) {
                    Delete_ = true; break;
                }
                if (Delete_) {
                    result->Delete(I);
                    MatchByAdditionalData.removeAt(I);
                } else {
                    I++;
                }
            } else {
                I++;
            }
        }
    }
    delete PrgFiles;
    return result;
}

int GetTemplateFromFolder(bool IsFolderScan, const QString& Folder, TGameDB* AutoSetupDB, QString& FileToStart, TStringList* StartableFiles)
{
    TStringList* MatchingTemplates;
    int I, J, NewLevel;
    int result = -1;

    TStringList PrgFiles;
    MatchingTemplates = GetTemplateFromFolderExt(IsFolderScan, Folder, AutoSetupDB, &PrgFiles);
    if (StartableFiles != nullptr) { StartableFiles->Clear(); StartableFiles->AddStrings(PrgFiles); }

    // If there is still more than one, let the user select the template to use
    if (MatchingTemplates->Count() > 0) {
        if (IsFolderScan) {
            J = ShowSelectTemplateDialog(ApplicationMainForm(), Folder, AutoSetupDB, MatchingTemplates);
        } else {
            J = ShowSelectTemplateDialog(ApplicationMainForm(), "", AutoSetupDB, MatchingTemplates);
        }
        if (J >= 0) {
            result = MatchingTemplates->ObjectAsInt(J);
            FileToStart = (*MatchingTemplates)[J];
        } else {
            result = -2;
        }
        delete MatchingTemplates;
        return result;
    }

    // Use defalt template on highest ranked PrgFile
    NewLevel = -1;
    for (I = 0; I < PrgFiles.Count(); I++) {
        J = PrgFiles.ObjectAsInt(I);
        if (J > NewLevel) { NewLevel = J; FileToStart = PrgFiles[I]; }
    }
    delete MatchingTemplates;
    return result;
}

static QString GetFolderForGame(const QString& GameDir, const QString& NewGameFolderSuggestion)
{
    QString result;
    if (NewGameFolderSuggestion == "") result = "Game"; else result = NewGameFolderSuggestion;

    if ((!DirectoryExists(GameDir + result)) || PrgSetup->IgnoreDirectoryCollisions()) return result;

    int I = 1;
    do {
        if (!DirectoryExists(GameDir + result + IntToStr(I))) { result = result + IntToStr(I); return result; }
        I++;
    } while (true);
}

QString OnlySubFolderCheck(const QString& Dir)
{
    TSearchRec Rec;
    int I, Files, Folders;
    QString SubFolder;
    QString result = Dir;

    Files = 0; Folders = 0; SubFolder = "";
    I = FindFirst(IncludeTrailingPathDelimiter(Dir) + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) Files++; else {
            if ((Rec.Name != ".") && (Rec.Name != "..")) { Folders++; SubFolder = Rec.Name; }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    if ((Files == 0) && (Folders == 1) && (SubFolder != "")) {
        result = IncludeTrailingPathDelimiter(IncludeTrailingPathDelimiter(Dir) + SubFolder);
        result = OnlySubFolderCheck(result);
    }
    return result;
}

static TGame* AddGameFromSimpleFolder(const QString& Dir, TGameDB* GameDB, TGameDB* TemplateDB, TGameDB* AutoSetupDB, const QString& ProfileName, const QString& ProfileFolder, const QString& FileToStart, const QString& SetupFileToStart, int TemplateNr, bool DoNotCopyFolder, bool MoveFiles)
{
    static const char* FileExts[] = {"ico", "png", "jpeg", "jpg", "gif", "bmp"};
    QString GameDir, NewGameFolder, S;
    int I, J;
    TSearchRec Rec;
    TGame* G;
    bool B;
    TGame* result = nullptr;

    // Create folder for new game
    GameDir = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
    if (DoNotCopyFolder) {
        S = ExtUpperCase(GameDir);
        if (ExtUpperCase(Copy(Dir, 1, Length(S))) == S) {
            NewGameFolder = Copy(Dir, Length(S) + 1, MaxInt);
            if ((Length(NewGameFolder) > 0) && IsDelim(NewGameFolder[Length(NewGameFolder) - 1])) NewGameFolder.chop(1);
        } else {
            S = Dir;
            if ((Length(S) > 0) && IsDelim(S[Length(S) - 1])) S.chop(1);
            S = LastPathPart(S);
            NewGameFolder = S;
        }
    } else {
        NewGameFolder = GetFolderForGame(GameDir, ProfileFolder);
        ForceDirectories(GameDir + NewGameFolder);
        B = DisableMainForm();
        bool OK = CopyFolderWithMsg(Dir, GameDir + NewGameFolder, true, MoveFiles);
        RestoreMainForm(B);
        if (!OK) return result;
    }

    // Create game DB record
    I = GameDB->Add(ProfileName); result = GameDB->Game(I);
    if (TemplateNr < 0) {
        if (TemplateNr == -1) {
            G = new TGame(PrgSetup); result->AssignFrom(G); delete G;
        } else {
            result->AssignFrom(TemplateDB->Game(-TemplateNr - 2));
        }
    } else {
        GameDB->Game(I)->AssignFrom(AutoSetupDB->Game(TemplateNr));
    }
    result->SetProfileMode("DOSBox");
    result->SetName(ProfileName);
    result->SetGameExe(MakeRelPath(GameDir + NewGameFolder + PathDelim + FileToStart, PrgSetup->BaseDir()));
    result->SetGameExeMD5(GetMD5Sum(MakeAbsPath(result->GameExe(), PrgSetup->BaseDir()), false));
    if (Trim(SetupFileToStart) != "") {
        result->SetSetupExe(MakeRelPath(GameDir + NewGameFolder + PathDelim + SetupFileToStart, PrgSetup->BaseDir()));
        result->SetSetupExeMD5(GetMD5Sum(MakeAbsPath(result->SetupExe(), PrgSetup->BaseDir()), false));
    } else {
        result->SetSetupExe("");
        result->SetSetupExeMD5("");
    }

    // Remove icon record if icon file does not exist
    if (!FileExists(MakeAbsIconName(result->Icon()))) result->SetIcon("");

    // Look for icons in game folder
    if (result->Icon() == "") for (const char* Ext : FileExts) {
        J = FindFirst(GameDir + NewGameFolder + PathDelim + "*." + QLatin1String(Ext), faAnyFile, Rec);
        if (J == 0) {
            result->SetIcon(MakeRelPath(GameDir + NewGameFolder + PathDelim + Rec.Name, PrgSetup->BaseDir()));
            FindClose(Rec);
            break;
        }
        FindClose(Rec);
    }

    // Set capture directory
    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(result->CacheName) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++; S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(result->CacheName) + IntToStr(I) + PathDelim;
    }
    result->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
    ForceDirectories(MakeAbsPath(result->CaptureFolder(), PrgSetup->BaseDir()));

    result->StoreAllValues();
    result->LoadCache();
    return result;
}

TGame* CreateGameFromSimpleFolder(const QString& Dir_, TGameDB* GameDB, const QString& ArchiveFileName, bool NoDialogIfAutoSetupIsAvailable, bool DoNotCopyFolder)
{
    TGameDB* AutoSetupDB;
    TGameDB* TemplateDB;
    TStringList* StartableFiles;
    TStringList* AutoSetup;
    int Nr = 0;
    QString ProfileName, FileToStart, SetupFileToStart, ProfileFolder;
    bool UseInstallSupport = false;
    QString AlternateProfileNameSource, OrigDir;
    TGame* result = nullptr;

    // If folder has only got some subfolder, use this
    OrigDir = Dir_;
    QString Dir = OnlySubFolderCheck(Dir_);

    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    StartableFiles = new TStringList;
    AutoSetup = GetTemplateFromFolderExt(false, Dir, AutoSetupDB, StartableFiles);
    if (StartableFiles->Count() == 0) {
        if (ArchiveFileName == "")
            MessageDlg(LanguageSetup->MessageFolderImportError(), mtError, mbOK, 0);
        else
            MessageDlg(LanguageSetup->MessageZipImportError(), mtError, mbOK, 0);
        // (the original leaked AutoSetup here)
        delete AutoSetup;
        delete AutoSetupDB;
        delete StartableFiles;
        return result;
    }
    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    if (ArchiveFileName != "") AlternateProfileNameSource = ""; else {
        AlternateProfileNameSource = OrigDir;
        if ((Length(AlternateProfileNameSource) > 0) && IsDelim(AlternateProfileNameSource[Length(AlternateProfileNameSource) - 1])) AlternateProfileNameSource.chop(1);
        AlternateProfileNameSource = LastPathPart(AlternateProfileNameSource);
    }
    if (ShowSelectTemplateForZipImportDialog(ApplicationMainForm(), AutoSetupDB, TemplateDB, StartableFiles, AutoSetup, ArchiveFileName, AlternateProfileNameSource, Dir, ProfileName, FileToStart, SetupFileToStart, ProfileFolder, Nr, NoDialogIfAutoSetupIsAvailable, DoNotCopyFolder, UseInstallSupport)) {
        if (UseInstallSupport) {
            if (ArchiveFileName == "")
                result = RunInstallationFromFolder(ApplicationMainForm(), GameDB, Dir);
            else
                result = RunInstallationFromZipFile(ApplicationMainForm(), GameDB, ArchiveFileName, Dir);
        } else {
            result = AddGameFromSimpleFolder(Dir, GameDB, TemplateDB, AutoSetupDB, ProfileName, ProfileFolder, FileToStart, SetupFileToStart, Nr, DoNotCopyFolder, ArchiveFileName != "");
        }
    }
    delete TemplateDB;
    delete AutoSetup;
    delete AutoSetupDB;
    delete StartableFiles;
    return result;
}

static bool ProfileExistingCheck(TGameDB* GameDB, const QString& TempProfFile)
{
    TGame* TempProf;
    int I;
    bool result = false;

    TempProf = new TGame(TempProfFile);
    TempProf->SetStoreConfigOnExit(false);
    I = GameDB->IndexOf(TempProf->CacheName);
    if (I < 0) { delete TempProf; return true; }
    switch (MessageDlg(Format(LanguageSetup->MessageZipImportDuplicateWarning(), {TempProf->CacheName}), mtWarning, mbYes | mbNo | mbCancel, 0)) {
        case mrYes: result = UninstallGame(ApplicationMainForm(), GameDB, GameDB->Game(I)); break;
        case mrNo: result = true; break;
        case mrCancel: /* nothing, keep result:=False; */ break;
        default: break;
    }
    delete TempProf;
    return result;
}

static QString FixSingleFileRecord(QString S, bool GameDir)
{
    QString T;
    S = Trim(S);
    if ((S != "") && !StartsWithDotDelim(S)) {
        T = ExtractFileName(S); S = ExtractFilePath(S);
        S = LastPathPart(ExcludeTrailingPathDelimiter(S));
        if (GameDir) S = IncludeTrailingPathDelimiter(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir())) + S; else S = "." + PathDelim + S;
        return IncludeTrailingPathDelimiter(S) + T;
    }
    return S;
}

static QString FixSingleFolderRecord(QString S, bool GameDir)
{
    S = Trim(S);
    if ((S != "") && !StartsWithDotDelim(S)) {
        S = LastPathPart(ExcludeTrailingPathDelimiter(S));
        if (GameDir) S = IncludeTrailingPathDelimiter(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir())) + S; else S = "." + PathDelim + S;
        return IncludeTrailingPathDelimiter(S);
    }
    return S;
}

static void FixFileNamesForWindowsProfiles(TGame* Game)
{
    QString S;
    TStringList* St;

    Game->SetGameExe(FixSingleFileRecord(Game->GameExe(), true));
    Game->SetSetupExe(FixSingleFileRecord(Game->SetupExe(), true));

    for (int I = 0; I <= 49; I++) {
        S = Game->ExtraPrgFile(I);
        if (Pos(";", S) == 0) continue;
        Game->SetExtraPrgFile(I, Copy(S, 1, Pos(";", S) - 1) + ";" + FixSingleFileRecord(Copy(S, Pos(";", S) + 1, MaxInt), true));
    }

    S = Trim(Game->ExtraFiles());
    if (S != "") {
        St = ValueToList(S);
        for (int I = 0; I < St->Count(); I++) (*St)[I] = FixSingleFileRecord((*St)[I], false);
        Game->SetExtraFiles(ListToValue(St));
        delete St;
    }

    S = Trim(Game->ExtraDirs());
    if (S != "") {
        St = ValueToList(S);
        for (int I = 0; I < St->Count(); I++) (*St)[I] = FixSingleFolderRecord((*St)[I], false);
        Game->SetExtraDirs(ListToValue(St));
        delete St;
    }

    Game->StoreAllValues();
}

static TGame* CreateGameFromDFRPackageFolder(const QString& Dir, TGameDB* GameDB, const QString& TempProfFile, bool MoveFiles)
{
    QString ProfFile;
    int I;
    bool B;
    TGame* result = nullptr;

    if (!ProfileExistingCheck(GameDB, TempProfFile)) return result;

    // Add profile
    ProfFile = PrgDataDir() + GameListSubDir + PathDelim + ExtractFileName(TempProfFile);
    if (FileExists(ProfFile)) {
        I = 0;
        do { I++; } while (FileExists(ChangeFileExt(ProfFile, IntToStr(I) + ".prof")));
        ProfFile = ChangeFileExt(ProfFile, IntToStr(I) + ".prof");
    }
    if (!CopyFileWithMsg(TempProfFile, ProfFile, false, MoveFiles)) return result;

    result = new TGame(ProfFile);
    GameDB->Add(result);
    if (WindowsExeMode(result)) FixFileNamesForWindowsProfiles(result);

    // Add files
    B = DisableMainForm();
    LoadAndShowSmallWaitForm();
    CopyFolderWithMsg(Dir, PrgSetup->BaseDir(), true, true, result, true);
    FreeSmallWaitForm();
    RestoreMainForm(B);
    return result;
}

// VarType(Node.NodeValue)<>varNull
static bool TextNodeNotEmpty(const QDomElement& Node)
{
    return !Node.text().isEmpty();
}

static QString RealPathFromDOSBoxPath(TGame* Game, const QString& LocalPath)
{
    QString Drive;
    TStringList* St;
    QString result = "";
    if ((Length(LocalPath) < 2) || (LocalPath[1] != ':')) return result;
    Drive = ExtUpperCase(QString(LocalPath[0]));

    for (int I = 0; I < Game->NrOfMounts(); I++) {
        St = ValueToList(Game->Mount(I));
        if (St->Count() < 3) { delete St; continue; }
        if (ExtUpperCase((*St)[2]) == Drive) {
            // the DOS part of the path uses '\' - convert it to the native delimiter
            result = IncludeTrailingPathDelimiter((*St)[0]) + ToNativePath(Copy(LocalPath, 4, MaxInt));
            delete St;
            return result;
        }
        delete St;
    }
    return result;
}

static QString GetGameFileFromAutoexec(TGame* Game, TStringList* Autoexec)
{
    QString S, Path, GlobalPath;
    QList<int> Lines;
    QString result = "";

    Path = "";
    for (int I = 0; I < Autoexec->Count(); I++) {
        S = Trim((*Autoexec)[I]);

        // drive letters
        if ((Length(S) == 2) && (S[1] == ':') && (ExtUpperCase(QString(S[0])) >= "A") && (ExtUpperCase(QString(S[0])) <= "Z")) {
            Path = S; Lines.clear(); Lines.append(I); continue;
        }

        // cd commands
        if ((Length(S) > 3) && (ExtUpperCase(Copy(S, 1, 2)) == "CD") && (S[2] == ' ') && (Path != "")) {
            S = Trim(Copy(S, 4, MaxInt));
            if ((S != "") && (S[0] == '\\')) S = Copy(S, 2, MaxInt);
            if ((S != "") && (S[Length(S) - 1] == '\\')) S = Copy(S, 1, Length(S) - 1);
            Path = Path + "\\" + S;
            Lines.append(I); continue;
        }

        // test filename
        if (I < Autoexec->Count() - 1) { Lines.clear(); continue; }
        if ((Length(S) > 5) && (ExtUpperCase(Copy(S, 1, 4)) == "CALL") && (S[4] == ' ')) S = Trim(Copy(S, 6, MaxInt));
        GlobalPath = RealPathFromDOSBoxPath(Game, Path);
        if (GlobalPath == "") { Lines.clear(); continue; }
        GlobalPath = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(GlobalPath), PrgSetup->BaseDir()));
        if (!FileExists(GlobalPath + S)) { Lines.clear(); continue; }
        result = MakeRelPath(GlobalPath + S, PrgSetup->BaseDir());
        Lines.append(I);
        for (int J = Lines.count() - 1; J >= 0; J--) Autoexec->Delete(Lines[J]);
        return result;
    }
    return result;
}

static void GetRunCommandFromAutoexec(TGame* Game)
{
    int I;
    TStringList* Autoexec;
    QString S;

    Autoexec = StringToStringList(Game->Autoexec());

    // Trim empty lines
    I = 0; while (I < Autoexec->Count()) if (Trim((*Autoexec)[I]) == "") Autoexec->Delete(I); else I++;

    // Exit command
    if ((Autoexec->Count() > 0) && (Trim(ExtUpperCase((*Autoexec)[Autoexec->Count() - 1])) == "EXIT")) {
        Game->SetCloseDosBoxAfterGameExit(true);
        Autoexec->Delete(Autoexec->Count() - 1);
    }

    // Get game filename from autoexec
    {
        TStringList St;
        St.AddStrings(*Autoexec);
        S = GetGameFileFromAutoexec(Game, &St);
        if (S != "") { Autoexec->Clear(); Autoexec->AddStrings(St); Game->SetGameExe(S); }
    }

    Game->SetAutoexec(StringListToString(Autoexec));
    delete Autoexec;
}

static TGame* CreateGameFromDBGLPackage(const QString& Dir, TGameDB* GameDB, const QDomElement& GameNode, TStringList* CustomNames, bool MoveFiles)
{
    int I;
    QString S, Capture, GameDir, ID, Config;
    TStringList* St;
    TGame* result;

    // Load title and create profile
    S = "Game";
    for (const QDomElement& N : ChildElements(GameNode)) if ((N.nodeName() == "title") && TextNodeNotEmpty(N)) S = N.text();
    I = GameDB->Add(S); result = GameDB->Game(I);

    {
        TStringList CustomValues;
        for (I = 1; I <= 10; I++) CustomValues.Add("");

        // Load data from xml node
        Capture = ""; GameDir = ""; ID = ""; Config = "";
        for (const QDomElement& N : ChildElements(GameNode)) {
            if (N.nodeName() == "captures") for (const QDomElement& C : ChildElements(N)) if ((C.nodeName() == "raw") && TextNodeNotEmpty(C)) Capture = C.text();
            if (N.nodeName() == "game-dir") for (const QDomElement& C : ChildElements(N)) if ((C.nodeName() == "raw") && TextNodeNotEmpty(C)) GameDir = C.text();
            if ((N.nodeName() == "id") && TextNodeNotEmpty(N)) ID = N.text();
            if ((N.nodeName() == "setup") && TextNodeNotEmpty(N)) result->SetSetupExe(N.text());
            if ((N.nodeName() == "setup-parameters") && TextNodeNotEmpty(N)) result->SetSetupParameters(N.text());
            if (N.nodeName() == "meta-info") for (const QDomElement& N2 : ChildElements(N)) {
                if ((N2.nodeName() == "developer") && TextNodeNotEmpty(N2)) result->SetDeveloper(N2.text());
                if ((N2.nodeName() == "publisher") && TextNodeNotEmpty(N2)) result->SetPublisher(N2.text());
                if ((N2.nodeName() == "year") && TextNodeNotEmpty(N2)) result->SetYear(N2.text());
                if ((N2.nodeName() == "genre") && TextNodeNotEmpty(N2)) result->SetGenre(N2.text());
                if ((N2.nodeName() == "favorite") && TextNodeNotEmpty(N2)) result->SetFavorite(ExtUpperCase(N2.text()) != "FALSE");
                if ((N2.nodeName() == "notes") && TextNodeNotEmpty(N2)) result->SetNotes(N2.text());
                for (int K = 1; K <= 10; K++) if ((N2.nodeName() == "custom" + IntToStr(K)) && TextNodeNotEmpty(N2)) CustomValues[K - 1] = N2.text();
                for (int L = 1; L <= 9; L++) {
                    if (N2.nodeName() == "link" + IntToStr(L)) for (const QDomElement& C : ChildElements(N2)) if ((C.nodeName() == "raw") && TextNodeNotEmpty(C)) result->SetWWW(L, C.text());
                }
            }
            if ((N.nodeName() == "full-configuration") && TextNodeNotEmpty(N)) Config = N.text();
        }

        // Process custom data
        TStringList St2;
        for (I = 0; I < CustomValues.Count(); I++) if ((Trim(CustomValues[I]) != "") && (Trim(CustomValues[I]) != "0")) St2.Add((*CustomNames)[I] + "=" + CustomValues[I]);
        result->SetUserInfo(StringListToString(&St2));
    }

    // Compact genre data
    St = ValueToList(result->Genre(), ",");
    for (I = 0; I < St->Count(); I++) (*St)[I] = Trim((*St)[I]);
    result->SetGenre(ListToValue(St, ';'));
    delete St;

    // Copy game folders
    CopyFolderWithMsg(IncludeTrailingPathDelimiter(Dir) + "dosroot" + PathDelim + ID + PathDelim, MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()), true, MoveFiles);

    // Load profile settings
    {
        TStringList St2;
        St2.SetText(Config);
        St2.SaveToFile(TempDir() + DosBoxConfFileName);
        ImportConfFileData(GameDB, result, TempDir() + DosBoxConfFileName);
        ExtDeleteFile(TempDir() + DosBoxConfFileName, ftTemp);
    }

    // Setup capture folder and copy capture files
    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(result->Name()) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(result->Name()) + IntToStr(I) + PathDelim;
    }
    result->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
    ForceDirectories(MakeAbsPath(result->CaptureFolder(), PrgSetup->BaseDir()));
    if (Trim(Capture) != "")
        CopyFolderWithMsg(IncludeTrailingPathDelimiter(Dir) + IncludeTrailingPathDelimiter(ToNativePath(Capture)), MakeAbsPath(result->CaptureFolder(), PrgSetup->BaseDir()), true, true);

    // Fix mount commands (DBGL is mounting ".")
    for (I = 0; I < result->NrOfMounts(); I++) {
        St = ValueToList(result->Mount(I));
        if ((St->Count() > 0) && ((Trim((*St)[0]) == ".") || (Trim((*St)[0]) == ".\\") || (Trim((*St)[0]) == "./"))) {
            (*St)[0] = MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
            result->SetMount(I, ListToValue(St));
        }
        delete St;
    }

    // Build game run command
    GetRunCommandFromAutoexec(result);

    // Build setup command
    S = Trim(result->SetupExe());
    if ((S != "") && !StartsWithDotDelim(S) && ((Length(S) < 2) || (S[1] != ':'))) result->SetSetupExe(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir()) + S);

    result->StoreAllValues();
    result->LoadCache();
    return result;
}

static TGame* CreateGamesFromDBGLPackageFolder(QWidget* Owner, const QString& Dir, TGameDB* GameDB, const QString& TempProfFile, bool MoveFiles)
{
    QDomDocument* XMLDoc;
    QList<QDomElement> ProfileNodes;
    QString Title, Author, Notes;
    TGame* G;
    TGame* result = nullptr;

    XMLDoc = LoadXMLDoc(TempProfFile);
    if ((XMLDoc == nullptr) || (XMLDoc->documentElement().nodeName() != "document")) { delete XMLDoc; return result; }

    TStringList ProfileNames;
    TStringList CustomNames;
    for (int I = 1; I <= 10; I++) CustomNames.Add("");

    // Read xml file, find profiles
    Title = ""; Author = ""; Notes = "";
    for (const QDomElement& N : ChildElements(XMLDoc->documentElement())) {
        if (N.nodeName() == "export") for (const QDomElement& N2 : ChildElements(N)) {
            if ((N2.nodeName() == "title") && TextNodeNotEmpty(N2)) Title = N2.text();
            if ((N2.nodeName() == "author") && TextNodeNotEmpty(N2)) Author = N2.text();
            if ((N2.nodeName() == "notes") && TextNodeNotEmpty(N2)) Notes = N2.text();
            for (int K = 1; K <= 10; K++) if ((N2.nodeName() == "custom" + IntToStr(K)) && TextNodeNotEmpty(N2)) CustomNames[K - 1] = N2.text();
        }
        if (N.nodeName() == "profile") for (const QDomElement& C : ChildElements(N)) if ((C.nodeName() == "title") && TextNodeNotEmpty(C)) {
            int K = ProfileNodes.count(); ProfileNodes.append(N);
            ProfileNames.AddObject(C.text(), static_cast<intptr_t>(K));
            break;
        }
    }

    // Select which profiles to install
    if (!ShowZipPackageDBGLDialog(Owner, Title, Author, Notes, &ProfileNames)) { delete XMLDoc; return result; }

    // Install profiles
    LoadAndShowSmallWaitForm(LanguageSetup->ImportDBGLPackageProgress());
    for (int I = 0; I < ProfileNames.Count(); I++) {
        G = CreateGameFromDBGLPackage(Dir, GameDB, ProfileNodes[ProfileNames.ObjectAsInt(I)], &CustomNames, MoveFiles);
        if (G != nullptr) result = G;
    }
    FreeSmallWaitForm();

    delete XMLDoc;
    return result;
}

static TGame* CreateGameFromFolder(QWidget* Owner, const QString& Dir, TGameDB* GameDB, const QString& ArchivFileName, bool NoDialogIfAutoSetupIsAvailable)
{
    TGame* result;
    QString TempProfFile = GetProfFileName(Dir);

    if (TempProfFile == "") {
        if (FileExists(IncludeTrailingPathDelimiter(Dir) + DBGLPackageInfoFile)) {
            // Import DBGL package file
            result = CreateGamesFromDBGLPackageFolder(Owner, Dir, GameDB, IncludeTrailingPathDelimiter(Dir) + DBGLPackageInfoFile, ArchivFileName != "");
        } else {
            // Add simple zip archive
            result = CreateGameFromSimpleFolder(Dir, GameDB, ArchivFileName, NoDialogIfAutoSetupIsAvailable, false);
        }
    } else {
        // Add normal zip package (with prof file)
        result = CreateGameFromDFRPackageFolder(Dir, GameDB, TempProfFile, ArchivFileName != "");
    }
    return result;
}

static void WindowsExeCount(const QString& Folder, int& DOS, int& Windows)
{
    TSearchRec Rec;
    int I;

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.EXE", faAnyFile, Rec);
    while (I == 0) {
        if (IsWindowsExe(IncludeTrailingPathDelimiter(Folder) + Rec.Name)) Windows++; else DOS++;
        I = FindNext(Rec);
    }
    FindClose(Rec);

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.COM", faAnyFile, Rec);
    while (I == 0) { DOS++; I = FindNext(Rec); }
    FindClose(Rec);

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.BAT", faAnyFile, Rec);
    while (I == 0) { DOS++; I = FindNext(Rec); }
    FindClose(Rec);

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) WindowsExeCount(IncludeTrailingPathDelimiter(Folder) + Rec.Name, DOS, Windows);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

static bool FolderWindowsTest(const QString& Folder, bool FromArchive)
{
    int DOS = 0, Windows = 0;
    WindowsExeCount(Folder, DOS, Windows);
    if (Windows > DOS) {
        if (FromArchive)
            return (MessageDlg(LanguageSetup->MessageWindowsExeImportArchiveWarning(), mtWarning, mbYes | mbNo, 0) == mrYes);
        else
            return (MessageDlg(LanguageSetup->MessageWindowsExeImportFolderWarning(), mtWarning, mbYes | mbNo, 0) == mrYes);
    }
    return true;
}

TGame* ImportZipPackage(QWidget* AOwner, const QString& AFileName, TGameDB* AGameDB, bool NoDialogIfAutoSetupIsAvailable)
{
    QString Temp;
    bool InstSupport;
    TGame* result = nullptr;
    InstSupport = false;

    Temp = IncludeTrailingPathDelimiter(TempDir() + TempSubFolder);
    if (!ForceDirectories(Temp)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Temp}), mtError, mbOK, 0);
        return result;
    }

    [&]() {
        if (!ExtractZipFile(AOwner, AFileName, Temp)) return;

        if (!PrgSetup->AllowPackingWindowsGames()) {
            if (!FolderWindowsTest(Temp, true)) return;
        }

        if (TestInstallationSupportNeeded(Temp)) {
            if (MessageDlg(LanguageSetup->InstallationSupportArchiveCheck(), mtConfirmation, mbYes | mbNo, 0) == mrYes) InstSupport = true;
        }

        if (InstSupport) {
            result = RunInstallationFromZipFile(AOwner, AGameDB, AFileName, Temp);
        } else {
            result = CreateGameFromFolder(AOwner, Temp, AGameDB, AFileName, NoDialogIfAutoSetupIsAvailable);
            if (result != nullptr) {
                result->StoreAllValues();
                result->LoadCache();
            }
        }
    }();
    ExtDeleteFolder(Temp, ftTemp);
    return result;
}

TGame* ImportFolder(QWidget* AOwner, const QString& AFolderName, TGameDB* AGameDB, bool NoDialogIfAutoSetupIsAvailable)
{
    bool InstSupport = false;
    TGame* result = nullptr;

    if (!FolderWindowsTest(AFolderName, false)) return result;

    if (TestInstallationSupportNeeded(AFolderName)) {
        if (MessageDlg(LanguageSetup->InstallationSupportFolderCheck(), mtConfirmation, mbYes | mbNo, 0) == mrYes) InstSupport = true;
    }

    if (InstSupport) {
        result = RunInstallationFromFolder(AOwner, AGameDB, AFolderName);
    } else {
        result = CreateGameFromFolder(AOwner, AFolderName, AGameDB, "", NoDialogIfAutoSetupIsAvailable);
        if (result != nullptr) {
            result->StoreAllValues();
            result->LoadCache();
        }
    }
    return result;
}

void TempZipFilesStartCleanUp()
{
    QString Temp = IncludeTrailingPathDelimiter(TempDir() + TempSubFolder);
    ExtDeleteFolder(Temp, ftTemp);
}
