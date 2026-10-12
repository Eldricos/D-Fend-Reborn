// C++ port of ZipInfoFormUnit.pas (implementation section).
//
// The original used the ZipMaster (DelZip DLL) component for zip files and
// the SevenZipVCL component (7za.dll) for 7z files.  Both are replaced by the
// external 7-Zip command line program (7z / 7za, "7z.exe" on Windows), which
// is driven through QProcess.  Progress information is read from the
// "-bsp1" progress output of 7-Zip.
#include "ZipInfoFormUnit.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "DosBoxUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"

TZipInfoForm *ZipInfoForm = nullptr;

// SevenZipVCL error codes used by SevenZipMessage
static constexpr int FFileNotFound = 1, FDataError = 2, FCRCError = 3, FUnsupportedMethod = 4, FIndexOutOfRange = 5, FSFXModuleError = 6;

// Location of the 7-Zip command line program (replaces 7za.dll / DelZip DLL)
static QString Find7ZipProgram()
{
    const QStringList Names = {
#ifdef Q_OS_WIN
        "7z.exe", "7za.exe"
#else
        "7z", "7za", "7zz"
#endif
    };
    for (const QString &N : Names) {
        if (FileExists(PrgDir() + N)) return PrgDir() + N;
        if (FileExists(PrgDir() + BinFolder + PathDelim + N)) return PrgDir() + BinFolder + PathDelim + N;
    }
    for (const QString &N : Names) {
        QString P = QStandardPaths::findExecutable(N);
        if (!P.isEmpty()) return P;
    }
#ifdef Q_OS_WIN
    for (const QString &Dir : {GetEnvironmentVariable("ProgramFiles"), GetEnvironmentVariable("ProgramFiles(x86)")}) {
        if (!Dir.isEmpty() && FileExists(IncludeTrailingPathDelimiter(Dir) + "7-Zip\\7z.exe")) return IncludeTrailingPathDelimiter(Dir) + "7-Zip\\7z.exe";
    }
#endif
    return QString();
}

static qint64 FolderSize(const QString &Folder)
{
    qint64 Size = 0;
    QDirIterator It(Folder, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (It.hasNext()) { It.next(); Size += It.fileInfo().size(); }
    return Size;
}

static bool IsPasswordError(const QString &Output)
{
    return Output.contains("Wrong password", Qt::CaseInsensitive) || Output.contains("Enter password", Qt::CaseInsensitive)
        || Output.contains("Can not open encrypted archive", Qt::CaseInsensitive);
}

TZipInfoForm::TZipInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.CancelButton, &QPushButton::clicked, this, [this]() { CancelButtonClick(ui.CancelButton); });
    FormCreate(this);   // OnCreate
}

TZipInfoForm::~TZipInfoForm() = default;

void TZipInfoForm::FormCreate(QObject * /*Sender*/)
{
    FProcessingCanceled = false;
    FCompressStrength = MAXIMUM;
}

bool TZipInfoForm::closeQuery()
{
    // The modal loop of the VCL only evaluates ModalResult after PostShow has finished.
    return !FWorking;
}

bool TZipInfoForm::Init(TZipMode AMode, const QString &AZipFile, const QString &AFolder)
{
    bool result = false;

    Mode = AMode;
    ZipFile = AZipFile;
    Folder = IncludeTrailingPathDelimiter(AFolder);

    if ((Mode == zmExtract) || (Mode == zmAdd) || (Mode == zmAddAndDelete)) {
        if (!FileExists(ZipFile)) { MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {ZipFile}), mtError, mbOK, 0); return result; }
    }

    if ((Mode == zmCreate) || (Mode == zmCreateAndDelete)) {
        if (FileExists(ZipFile)) {
            if ((FDeleteMode != dmNoNoWarning) && (FDeleteMode != dmFilesNoWarning) && (FDeleteMode != dmFolderNoWarning)) {
                if (MessageDlg(Format(LanguageSetup->ZipFormOverwriteWarning(), {ZipFile}), mtWarning, mbYes | mbNo, 0) != mrYes) return result;
            }
            if (!ExtDeleteFileWithPause(ZipFile, ftZipOperation)) { MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {ZipFile}), mtError, mbOK, 0); return result; }
        }
    }

    ui.InfoLabel->setText("");
    repaint();

    if (Mode == zmExtract) {
        if (!ForceDirectories(Folder)) { MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Folder}), mtError, mbOK, 0); return result; }
    }

    if ((Mode == zmCreate) || (Mode == zmAdd) || (Mode == zmCreateAndDelete) || (Mode == zmAddAndDelete)) {
        if (!DirectoryExists(Folder)) { MessageDlg(Format(LanguageSetup->MessageDirectoryNotFound(), {Folder}), mtError, mbOK, 0); return result; }
    }

    result = true;

    switch (Mode) {
        case zmExtract: setWindowTitle(LanguageSetup->ZipFormCaptionExtract()); break;
        case zmCreate: case zmCreateAndDelete: setWindowTitle(LanguageSetup->ZipFormCaptionCreate()); break;
        case zmAdd: case zmAddAndDelete: setWindowTitle(LanguageSetup->ZipFormCaptionAdd()); break;
        case zmDeleteOnly: setWindowTitle(LanguageSetup->ZipFormCaptionDelete()); break;
    }
    return result;
}

void TZipInfoForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TZipInfoForm::FormShow(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt
    ui.CancelButton->setText(LanguageSetup->Cancel());

    FWorking = true;
    QTimer::singleShot(0, this, [this]() { PostShow(); });   // PostMessage(Handle,WM_USER+1,0,0)
}

void TZipInfoForm::PostShow()
{
    QString Ext, SaveDir;
    bool Ok, Handled;

    Start = GetTickCount();
    FWorking = true;

    SaveDir = GetCurrentDir();
    SetCurrentDir(ExtractFilePath(ExpandFileName(ParamStr(0))));

    if (Mode == zmDeleteOnly) {
        Ok = true;
    } else {
        Ext = Trim(UpperCase(ExtractFileExt(ZipFile)));

        if ((Mode == zmCreate || Mode == zmAdd || Mode == zmCreateAndDelete || Mode == zmAddAndDelete) && (!DirectoryExists(Folder))) {
            MessageDlg(Format(LanguageSetup->MessageDirectoryNotFound(), {Folder}), mtError, mbOK, 0);
            SetCurrentDir(SaveDir);
            FWorking = false;
            setModalResult(mrCancel);   // + PostMessage(Handle,WM_CLOSE,0,0)
            return;
        }

        Handled = false; Ok = false;
        for (int I = 0; I < PrgSetup->PackerSettingsCount(); I++) if (ExtensionInList(Ext, PrgSetup->PackerSettings(I)->FileExtensions())) {
            Handled = true;
            Ok = ExternalWork(I);
        }

        if (!Handled) {
            if (Ext == ".ZIP") { Handled = true; Ok = ZipWork(); }
            if (Ext == ".7Z") { Handled = true; Ok = SevenZipWork(); }
        }

        if (!Handled) {
            MessageDlg(Format(LanguageSetup->ZipFormUnknownExtension(), {Ext}), mtError, mbOK, 0);
        }
    }

    if (Ok && ((Mode == zmCreateAndDelete) || (Mode == zmAddAndDelete) || (Mode == zmDeleteOnly))) Ok = DeleteFiles();
    SetCurrentDir(SaveDir);

    FWorking = false;
    if (Ok) setModalResult(mrOk); else setModalResult(mrCancel);   // + PostMessage(Handle,WM_CLOSE,0,0)
}

int TZipInfoForm::Run7Zip(QProcess *&P, const QStringList &Arguments, const QString &WorkingDir, bool IsZip, qint64 TotalSize, QString &Output)
{
    Output.clear();
    const QString PrgName = Find7ZipProgram();
    if (PrgName.isEmpty()) {
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {
#ifdef Q_OS_WIN
            QString("7z.exe")
#else
            QString("7z")
#endif
        }), mtError, mbOK, 0);
        return -1;
    }

    P = new QProcess(this);
    if (!WorkingDir.isEmpty()) P->setWorkingDirectory(ToNativePath(WorkingDir));
    P->setProcessChannelMode(QProcess::MergedChannels);
    P->start(PrgName, QStringList{"-bsp1", "-bb0"} + Arguments);
    if (!P->waitForStarted(30000)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {PrgName}), mtError, mbOK, 0);
        delete P; P = nullptr;
        return -1;
    }
    P->closeWriteChannel();   // no interactive password prompt

    static const QRegularExpression PercentRE("(\\d+)%");
    int LastPercent = -1;
    bool Killed = false;
    auto ReadProgress = [&]() {
        const QString S = QString::fromLocal8Bit(P->readAll());
        if (S.isEmpty()) return;
        Output += S;
        int Percent = -1;
        auto It = PercentRE.globalMatch(S);
        while (It.hasNext()) Percent = It.next().captured(1).toInt();
        if (Percent >= 0 && Percent != LastPercent) {
            LastPercent = Percent;
            const qint64 Pos = TotalSize * Percent / 100;
            if (IsZip) ZipProgress(P, Percent, Pos); else SevenZipProgress(P, QString(), Pos, 0);
        }
    };

    while (!P->waitForFinished(100)) {
        ReadProgress();
        ProcessMessages();
        if (FProcessingCanceled && !Killed) {   // Zip.Cancel / SevenZip.Cancel
            Killed = true;
            P->kill();
        }
        if (P->state() == QProcess::NotRunning) break;
    }
    ReadProgress();

    int ExitCode = (P->exitStatus() == QProcess::NormalExit) ? P->exitCode() : 255;
    if (Killed) ExitCode = 255;   // "User stopped the process"
    delete P; P = nullptr;
    return ExitCode;
}

bool TZipInfoForm::ZipWork()
{
    bool result = true;
    ZipErrCode = 0;
    ZipPasswordValue.clear();

    ui.CancelButton->setEnabled(true);

    switch (Mode) {
        case zmExtract: ZipExtract(); break;
        case zmCreate: case zmCreateAndDelete: ZipCreate(); break;
        case zmAdd: case zmAddAndDelete: ZipAdd(); break;
        default: break;
    }

    if (FProcessingCanceled) result = false;

    if (ZipErrCode != 0 && !FProcessingCanceled) {
        result = false;
        if (Mode == zmExtract)
            MessageDlg(LanguageSetup->ZipFormErrorExtract(), mtError, mbOK, 0);
        else
            MessageDlg(LanguageSetup->ZipFormErrorCompress(), mtError, mbOK, 0);
    }
    return result;
}

static QString ZipCompLevel(int CompressStrength)
{
    switch (CompressStrength) {
        case SAVE: return "0";
        case FAST: return "3";
        case NORMAL: return "7";
        case MAXIMUM: return "8";
        case ULTRA: return "9";
        default: return "8";
    }
}

void TZipInfoForm::ZipExtract()
{
    // ExtrOptions:=[ExtrDirNames,ExtrOverWrite]; ExtrBaseDir:=Folder
    QString Output;
    QString Password;
    bool UsePassword = false;
    while (true) {
        QStringList Args = {"x", "-y", "-o" + ExcludeTrailingPathDelimiter(Folder)};
        if (UsePassword) Args << "-p" + Password;
        Args << ExpandFileName(ZipFile);
        int Code = Run7Zip(Zip, Args, QString(), true, GetFileSizeOf(ZipFile), Output);
        if (Code == -1) { ZipErrCode = -1; return; }
        if (FProcessingCanceled) return;
        if (Code >= 2 && IsPasswordError(Output)) {
            // OnPasswordError
            if (ZipPassword(this, false, Password, ZipFile)) { UsePassword = true; continue; }
            ZipErrCode = Code;
            return;
        }
        if (Code >= 2) ZipErrCode = Code;
        return;
    }
}

void TZipInfoForm::ZipCreate()
{
    // AddOptions:=[AddHiddenFiles,AddDirNames,AddSeparateDirs]; FSpecArgs:='>*.*'
    QString Output;
    int Code = Run7Zip(Zip, {"a", "-tzip", "-mx=" + ZipCompLevel(FCompressStrength), ExpandFileName(ZipFile), "*"}, Folder, true, FolderSize(Folder), Output);
    if (Code == -1) { ZipErrCode = -1; return; }
    if (Code >= 2 && !FProcessingCanceled) ZipErrCode = Code;
}

void TZipInfoForm::ZipAdd()
{
    TSearchRec Rec;
    int I;
    bool B;

    // AddOptions:=[AddHiddenFiles,AddDirNames,AddSeparateDirs,AddUpdate]; FSpecArgs:='>*.*'

    B = false;
    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Name != ".") && (Rec.Name != "..")) { B = true; break; }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    if (B) {
        QString Output;
        int Code = Run7Zip(Zip, {"u", "-tzip", "-mx=" + ZipCompLevel(FCompressStrength), ExpandFileName(ZipFile), "*"}, Folder, true, FolderSize(Folder), Output);
        if (Code == -1) { ZipErrCode = -1; return; }
        if (Code >= 2 && !FProcessingCanceled) ZipErrCode = Code;
    }
}

bool TZipInfoForm::ZipPassword(QObject * /*Sender*/, bool /*IsZipAction*/, QString &NewPassword, const QString & /*ForFile*/)
{
    return InputQuery(LanguageSetup->ZipFormCaptionExtract(), LanguageSetup->ZipFormPasswordPrompt(), NewPassword, this);
}

void TZipInfoForm::ZipProgress(QObject * /*Sender*/, int TotalPerCent, qint64 TotalPosition)
{
    QString S;
    ui.ProgressBar->setValue(TotalPerCent);

    switch (Mode) {
        case zmExtract: S = LanguageSetup->ZipFormProgressExtract(); break;
        case zmCreate: case zmCreateAndDelete: S = LanguageSetup->ZipFormProgressCreate(); break;
        case zmAdd: case zmAddAndDelete: S = LanguageSetup->ZipFormProgressAdd(); break;
        default: break;
    }

    ui.InfoLabel->setText(
        Format(S, {ExtractFileName(ZipFile)}) + "\n\n" + Folder + "\n\n" +
        Format(LanguageSetup->ZipFormProgress(), {TotalPerCent, TotalPosition / 1024 / 1024, 1000 * TotalPosition / 1024 / 1024 / Max<qint64>(1, qint64(GetTickCount() - Start))}));

    // if FProcessingCanceled then Zip.Cancel:=True -> handled by Run7Zip
}

bool TZipInfoForm::SevenZipWork()
{
    bool result = true;
    SevenZipLastError = 0;
    ZipErrCode = 0;

    ui.CancelButton->setEnabled(true);

    switch (Mode) {
        case zmExtract: SevenZipExtract(); break;
        case zmCreate: case zmCreateAndDelete: SevenZipCreate(); break;
        case zmAdd: case zmAddAndDelete:
            MessageDlg(LanguageSetup->ZipFormErrorNoRepack7ZipSupport(), mtError, mbOK, 0);
            result = false;
            return result;
        default: break;
    }

    if (((ZipErrCode != 0) || (SevenZipLastError != 0)) && !FProcessingCanceled) {
        result = false;
        if (Mode == zmExtract)
            MessageDlg(LanguageSetup->ZipFormErrorExtract(), mtError, mbOK, 0);
        else
            MessageDlg(LanguageSetup->ZipFormErrorCompress(), mtError, mbOK, 0);
    }
    if (FProcessingCanceled) result = false;
    return result;
}

void TZipInfoForm::SevenZipExtract()
{
    // ExtractOptions:=ExtractOptions+[ExtractOverwrite]
    QString Output;
    SevenZipPreProgress(this, GetFileSizeOf(ZipFile));
    int Code = Run7Zip(SevenZip, {"x", "-y", "-o" + ExcludeTrailingPathDelimiter(Folder), ExpandFileName(ZipFile)}, QString(), false, SevenZipMaxProgress, Output);
    if (Code == -1) { ZipErrCode = -1; return; }
    if (FProcessingCanceled) return;
    if (Code >= 2) {
        if (IsPasswordError(Output)) SevenZipMessage(this, FDataError, Output, ZipFile);
        else if (Output.contains("CRC Failed", Qt::CaseInsensitive)) SevenZipMessage(this, FCRCError, Output, ZipFile);
        else if (Output.contains("Unsupported Method", Qt::CaseInsensitive)) SevenZipMessage(this, FUnsupportedMethod, Output, ZipFile);
        else ZipErrCode = Code;
    }
}

void TZipInfoForm::SevenZipCreate()
{
    // LZMACompressStrength:=FCompressStrength; AddOptions:=[AddRecurseDirs]; Files:=Folder+'*.*'
    QString Level;
    switch (FCompressStrength) {
        case SAVE: Level = "0"; break;
        case FAST: Level = "3"; break;
        case NORMAL: Level = "5"; break;
        case MAXIMUM: Level = "7"; break;
        case ULTRA: Level = "9"; break;
        default: Level = "7"; break;
    }
    QString Output;
    SevenZipPreProgress(this, FolderSize(Folder));
    int Code = Run7Zip(SevenZip, {"a", "-t7z", "-mx=" + Level, ExpandFileName(ZipFile), "*"}, Folder, false, SevenZipMaxProgress, Output);
    if (Code == -1) { ZipErrCode = -1; return; }
    if (Code >= 2 && !FProcessingCanceled) {
        if (Output.contains("cannot find", Qt::CaseInsensitive)) SevenZipMessage(this, FFileNotFound, Output, ZipFile);
        else ZipErrCode = Code;
    }
}

void TZipInfoForm::SevenZipPreProgress(QObject * /*Sender*/, qint64 MaxProgress)
{
    SevenZipMaxProgress = MaxProgress;
}

void TZipInfoForm::SevenZipProgress(QObject * /*Sender*/, const QString & /*Filename*/, qint64 FilePosArc, qint64 /*FilePosFile*/)
{
    QString S;
    const qint64 MaxP = Max<qint64>(1, SevenZipMaxProgress);
    ui.ProgressBar->setValue(int(100 * FilePosArc / MaxP));

    switch (Mode) {
        case zmExtract: S = LanguageSetup->ZipFormProgressExtract(); break;
        case zmCreate: case zmCreateAndDelete: S = LanguageSetup->ZipFormProgressCreate(); break;
        case zmAdd: case zmAddAndDelete: S = LanguageSetup->ZipFormProgressAdd(); break;
        default: break;
    }

    ui.InfoLabel->setText(
        Format(S, {ExtractFileName(ZipFile)}) + "\n\n" + Folder + "\n\n" +
        Format(LanguageSetup->ZipFormProgress(), {100 * FilePosArc / MaxP, FilePosArc / 1024 / 1024, 1000 * FilePosArc / 1024 / 1024 / Max<qint64>(1, qint64(GetTickCount() - Start))}));

    ProcessMessages();
    // if FProcessingCanceled then SevenZip.Cancel -> handled by Run7Zip
}

void TZipInfoForm::SevenZipMessage(QObject * /*Sender*/, int ErrCode, const QString & /*Message*/, const QString & /*Filename*/)
{
    if (ErrCode == FDataError) {
        MessageDlg(LanguageSetup->ZipFormErrorNoPassword7ZipSupport(), mtError, mbOK, 0);
        SevenZipLastError = ErrCode;
        return;
    }

    if (ErrCode == FFileNotFound || ErrCode == FDataError || ErrCode == FCRCError || ErrCode == FUnsupportedMethod || ErrCode == FIndexOutOfRange || ErrCode == FSFXModuleError) {
        SevenZipLastError = ErrCode;
        return;
    }
}

bool TZipInfoForm::ExternalWork(int Nr)
{
    QString PrgName, Parameters, ParametersOrig;
    int I;
    bool result = true;

    PrgName = PrgSetup->PackerSettings(Nr)->ZipFileName();
    if (!FileExists(PrgName)) {
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {PrgName}), mtError, mbOK, 0);
        result = false;
        return result;
    }

    switch (Mode) {
        case zmExtract: Parameters = PrgSetup->PackerSettings(Nr)->ExtractFile(); break;
        case zmCreate: case zmCreateAndDelete: Parameters = PrgSetup->PackerSettings(Nr)->CreateFile(); break;
        case zmAdd: case zmAddAndDelete: Parameters = PrgSetup->PackerSettings(Nr)->UpdateFile(); break;
        default: break;
    }
    ParametersOrig = Parameters;

    I = Pos("%1", Parameters);
    if (I == 0) {
        MessageDlg(Format(LanguageSetup->ZipFormInvalidParameters(), {ParametersOrig}), mtError, mbOK, 0);
        result = false;
        return result;
    }
    Parameters = Copy(Parameters, 1, I - 1) + ZipFile + Copy(Parameters, I + 2, MaxInt);

    I = Pos("%2", Parameters);
    if (I == 0) {
        MessageDlg(Format(LanguageSetup->ZipFormInvalidParameters(), {ParametersOrig}), mtError, mbOK, 0);
        result = false;
        return result;
    }
    if (PrgSetup->PackerSettings(Nr)->TrailingBackslash()) {
        Parameters = Copy(Parameters, 1, I - 1) + IncludeTrailingPathDelimiter(Folder) + Copy(Parameters, I + 2, MaxInt);
    } else {
        Parameters = Copy(Parameters, 1, I - 1) + ExcludeTrailingPathDelimiter(Folder) + Copy(Parameters, I + 2, MaxInt);
    }

    // CreateProcess(PrgName, '"'+PrgName+'" '+Parameters, ...)
    QProcess P;
#ifdef Q_OS_WIN
    P.setProgram(PrgName);
    P.setNativeArguments(Parameters);
#else
    P.setProgram(PrgName);
    P.setArguments(SplitCommandLine(Parameters));
#endif
    P.setProcessChannelMode(QProcess::ForwardedChannels);
    P.start();
    if (!P.waitForStarted(30000)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {PrgName}), mtError, mbOK, 0);
        return result;
    }

    ui.InfoLabel->setText(windowTitle());
    repaint();

    while (!P.waitForFinished(100)) {
        ProcessMessages();
        if (P.state() == QProcess::NotRunning) break;
    }
    return result;
}

bool TZipInfoForm::DeleteFolder(QString Folder, bool ThisIsMainFolder)
{
    TSearchRec Rec;
    int I;
    bool result = false;

    Folder = IncludeTrailingPathDelimiter(Folder);
    if (Folder.length() == 3) return result;
#ifndef Q_OS_WIN
    if (Folder == PathDelim) return result;
#endif

    if (!DirectoryExists(Folder)) { result = true; return result; }

    if (!BaseDirSecuriryCheck(Folder)) return result;

    I = FindFirst(Folder + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) != 0) {
            if ((Rec.Name != ".") && (Rec.Name != "..")) {
                if (!DeleteFolder(Folder + Rec.Name, false)) { FindClose(Rec); return result; }
            }
        } else {
            if (!ExtDeleteFile(Folder + Rec.Name, ftZipOperation)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {Folder + Rec.Name}), mtError, mbOK, 0);
                FindClose(Rec);
                return result;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    if ((!ThisIsMainFolder) || (FDeleteMode == dmFolder) || (FDeleteMode == dmFolderNoWarning)) {
        if (!ExtDeleteFolder(Folder, ftZipOperation)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {Folder}), mtError, mbOK, 0);
            return result;
        }
    }

    result = true;
    return result;
}

void TZipInfoForm::CancelButtonClick(QObject * /*Sender*/)
{
    ui.CancelButton->setEnabled(false);
    FProcessingCanceled = true;
}

bool TZipInfoForm::DeleteFiles()
{
    DeleteFolder(Folder, true);
    return true;
}

// global

static bool ZipDialogWork(QWidget *AOwner, const QString &AZipFile, const QString &ADestFolder, TZipMode AZipMode, int ACompressStrength, TDeleteMode ADeleteMode)
{
    bool result = false;
    ZipInfoForm = new TZipInfoForm(AOwner);
    ZipInfoForm->SetCompressStrength(ACompressStrength);
    ZipInfoForm->SetDeleteMode(ADeleteMode);
    if (ZipInfoForm->Init(AZipMode, AZipFile, ADestFolder)) result = (ZipInfoForm->ShowModal() == mrOk);
    delete ZipInfoForm;
    ZipInfoForm = nullptr;
    return result;
}

bool ExtractZipFile(QWidget *AOwner, const QString &AZipFile, const QString &ADestFolder)
{
    return ZipDialogWork(AOwner, AZipFile, ADestFolder, zmExtract, MAXIMUM, dmNo);
}

bool CreateZipFile(QWidget *AOwner, const QString &AZipFile, const QString &ADestFolder, TDeleteMode DeleteMode, int CompressStrength)
{
    if ((DeleteMode != dmNo) && (DeleteMode != dmNoNoWarning))
        return ZipDialogWork(AOwner, AZipFile, ADestFolder, zmCreateAndDelete, CompressStrength, DeleteMode);
    else
        return ZipDialogWork(AOwner, AZipFile, ADestFolder, zmCreate, CompressStrength, DeleteMode);
}

bool AddToZipFile(QWidget *AOwner, const QString &AZipFile, const QString &ADestFolder, TDeleteMode DeleteMode, int CompressStrength)
{
    if ((DeleteMode != dmNo) && (DeleteMode != dmNoNoWarning))
        return ZipDialogWork(AOwner, AZipFile, ADestFolder, zmAddAndDelete, CompressStrength, DeleteMode);
    else
        return ZipDialogWork(AOwner, AZipFile, ADestFolder, zmAdd, CompressStrength, DeleteMode);
}

bool DeleteUnpackedFiles(QWidget *AOwner, const QString &ADestFolder, TDeleteMode DeleteMode)
{
    if ((DeleteMode == dmNo) || (DeleteMode == dmNoNoWarning)) return true;
    return ZipDialogWork(AOwner, "", ADestFolder, zmDeleteOnly, SAVE, DeleteMode);
}

bool ExtractZipDrive(QWidget *AOwner, const QString &AZipFile, const QString &AZipAddFolder, const QString &ADestFolder)
{
    bool result;
    if (AZipAddFolder == ADestFolder) {
        // DestFolder->TempFolder
        result = ExtDeleteFolder(TempDir() + ZipTempDir, ftTemp); if (!result) return result;
        ForceDirectories(TempDir() + ZipTempDir);
        CopyFiles(ADestFolder, TempDir() + ZipTempDir, true, true);
        // ZipFile+TempFolder -> DestFolder
        result = ExtractZipFile(AOwner, AZipFile, ADestFolder);
        if (result) result = CopyFiles(TempDir() + ZipTempDir, ADestFolder, true, true);
        // Delete TempFolder
        ExtDeleteFolder(TempDir() + ZipTempDir, ftTemp);
    } else {
        // ZipFile+ZipAddFolder -> DestFolder
        result = ExtractZipFile(AOwner, AZipFile, ADestFolder);
        if (!result) return result;
        result = CopyFiles(AZipAddFolder, ADestFolder, true, true);
    }
    return result;
}

static QString CheckExtension(QString Extension)
{
    QString result;

    if (Extension.isEmpty()) return result;
    if (Extension[0] == '*') Delete(Extension, 1, 1);

    if (Extension.isEmpty()) return result;
    if (Extension[0] == '.') Delete(Extension, 1, 1);

    result = Extension;
    return result;
}

QString CheckExtensionsList(const QString &AExtensions)
{
    QString result, S;
    int I;
    QString Extensions = Trim(AExtensions);

    do {
        I = Pos(';', Extensions);
        if (I > 0) {
            S = CheckExtension(Trim(Copy(Extensions, 1, I - 1)));
            Extensions = Trim(Copy(Extensions, I + 1, MaxInt));
        } else {
            S = CheckExtension(Extensions);
            Extensions = "";
        }
        if (!S.isEmpty()) {
            if (!result.isEmpty()) result = result + ";";
            result = result + S;
        }
    } while (I != 0);
    return result;
}

bool ExtensionInList(const QString &AExtension, const QString &AList)
{
    int I;
    QString S;
    QString Extension = Trim(ExtUpperCase(CheckExtension(AExtension)));

    QString List = Trim(AList);
    do {
        I = Pos(';', List);
        if (I > 0) {
            S = CheckExtension(Trim(Copy(List, 1, I - 1)));
            List = Trim(Copy(List, I + 1, MaxInt));
        } else {
            S = CheckExtension(List);
            List = "";
        }
        if (Trim(ExtUpperCase(S)) == Extension) return true;
    } while (I != 0);
    return false;
}

QString ProcessFileNameFilter(const QString &Filter, const QString &ArchiveFiles)
{
    int I, J;
    QString S, T, U;
    QString result = Filter;

    I = Pos("%s", Filter); if (I == 0) return result;
    S = Copy(Filter, I + 2, MaxInt); I = Pos("%s", S); if (I == 0) return result;
    S = Copy(S, I + 2, MaxInt); I = Pos("%s", S); if (I == 0) return result;

    TStringList St;
    for (I = 0; I < PrgSetup->PackerSettingsCount(); I++) {
        S = Trim(PrgSetup->PackerSettings(I)->FileExtensions());
        while (!S.isEmpty()) {
            J = Pos(';', S);
            if (J > 0) { T = Copy(S, 1, J - 1); S = Copy(S, J + 1, MaxInt); } else { T = S; S = ""; }
            T = ExtUpperCase(T);
            if (St.IndexOf(T) < 0) St.Add(T);
        }
    }
    S = ""; for (I = 0; I < St.Count(); I++) S = S + ", *." + ExtLowerCase(St[I]);
    U = ""; for (I = 0; I < St.Count(); I++) U = U + ";*." + ExtLowerCase(St[I]);
    T = ""; for (I = 0; I < St.Count(); I++) T = T + Format(ArchiveFiles, {ExtLowerCase(St[I])}) + " (*." + ExtLowerCase(St[I]) + ")|*." + ExtLowerCase(St[I]) + "|";
    result = Format(Filter, {S, U, T});
    return result;
}

int GetCompressStrengthFromPrgSetup()
{
    switch (PrgSetup->CompressionLevel()) {
        case 0: return SAVE;
        case 1: return FAST;
        case 2: return NORMAL;
        case 3: return MAXIMUM;
        case 4: return ULTRA;
        default: return MAXIMUM;
    }
}
