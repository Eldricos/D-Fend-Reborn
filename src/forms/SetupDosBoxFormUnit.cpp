// C++ port of SetupDosBoxFormUnit.pas (implementation section).
#include "SetupDosBoxFormUnit.h"

#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include <QStandardPaths>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TSetupDosBoxForm* SetupDosBoxForm = nullptr;

static constexpr int CSIDL_PROGRAM_FILES_ = 0x0026;   // ShlObj.CSIDL_PROGRAM_FILES

TSetupDosBoxForm::TSetupDosBoxForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.AbortButton, &QPushButton::clicked, this, [this]() { AbortButtonClick(ui.AbortButton); });

    FormCreate(this);   // OnCreate
}

TSetupDosBoxForm::~TSetupDosBoxForm()
{
}

void TSetupDosBoxForm::showEvent(QShowEvent* Event)
{
    TVclForm::showEvent(Event);
    if (!FirstShowDone) {
        FirstShowDone = true;
        FormShow(this);   // OnShow
    }
}

void TSetupDosBoxForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    SearchType = stDOSBox;
}

void TSetupDosBoxForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered, SetVistaFonts, Font.Charset: not needed in Qt

    switch (SearchType) {
        case stDOSBox: setWindowTitle(LanguageSetup->SetupDOSBoxForm()); break;
        case stOggEnc: setWindowTitle(LanguageSetup->SetupDOSBoxFormOggEnc()); break;
        case stLame: setWindowTitle(LanguageSetup->SetupDOSBoxFormLame()); break;
        case stScummVM: setWindowTitle(LanguageSetup->SetupDOSBoxFormScummVM()); break;
        case stQBasic: setWindowTitle(LanguageSetup->SetupDOSBoxFormQBasic()); break;
    }
    ui.AbortButton->setText(LanguageSetup->Abort());

    UserIconLoader->DialogImage(DI_Abort, ui.AbortButton);

    // PostMessage(Handle,WM_USER+1,0,0);
    QTimer::singleShot(0, this, [this]() { StartSearch(); });
}

void TSetupDosBoxForm::StartSearch()
{
    repaint();
    QCoreApplication::processEvents();

    Aborted = false;
    Count = 0;

    // "finally Close": setModalResult closes the form (an additional close()
    // would replace the modal result by mrCancel)
    if (SearchDir(PrgDir())) { setModalResult(mrOk); return; }
    if (PrgDataDir() != PrgDir()) {
        if (SearchDir(PrgDataDir())) { setModalResult(mrOk); return; }
    }
    if (Aborted) { setModalResult(mrAbort); return; }

#ifdef Q_OS_WIN
    for (char C = 'C'; C <= 'Z'; C++) {
        QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) != DRIVE_NO_ROOT_DIR) {
            if (SearchDir(Root)) { setModalResult(mrOk); return; }
            if (Aborted) { setModalResult(mrAbort); return; }
        }
    }
#else
    // No drive letters: search the home folder first, then the whole file
    // system (SearchDir skips the virtual file systems)
    if (SearchDir(IncludeTrailingPathDelimiter(QDir::homePath()))) { setModalResult(mrOk); return; }
    if (Aborted) { setModalResult(mrAbort); return; }
    if (SearchDir("/")) { setModalResult(mrOk); return; }
    if (Aborted) { setModalResult(mrAbort); return; }
#endif

    setModalResult(mrAbort);
}

bool TSetupDosBoxForm::SearchDir(const QString& Dir)
{
    TSearchRec Rec;
    int I;
    bool result = false;

    switch (SearchType) {
        case stDOSBox: result = DOSBoxExeExists(Dir); if (result) { DOSBoxDir = Dir; return result; } break;
        case stOggEnc: result = FileExists(Dir + OggEncPrgFile); if (result) { PrgSetup->SetWaveEncOgg(Dir + OggEncPrgFile); return result; } break;
        case stLame: result = FileExists(Dir + LamePrgFile); if (result) { PrgSetup->SetWaveEncMp3(Dir + LamePrgFile); return result; } break;
        case stScummVM: result = FileExists(Dir + ScummPrgFile); if (result) { PrgSetup->SetScummVMPath(Dir); return result; } break;
        case stQBasic:
            result = FileExists(Dir + QBasicPrgFile); if (result) { PrgSetup->SetQBasic(Dir + QBasicPrgFile); return result; }
            result = FileExists(Dir + QB45PrgFile); if (result) { PrgSetup->SetQBasic(Dir + QB45PrgFile); return result; }
            result = FileExists(Dir + QB71PrgFile); if (result) { PrgSetup->SetQBasic(Dir + QB71PrgFile); return result; }
            break;
    }

    Count++;
    if (Count % 20 == 0) {
        ui.InfoLabel->setText(Dir);
        QCoreApplication::processEvents();
        if (Aborted) return result;
    }

    I = FindFirst(Dir + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
#ifndef Q_OS_WIN
            // do not follow symbolic links (endless recursion / other file systems)
            if (QFileInfo(Dir + Rec.Name).isSymLink()) { I = FindNext(Rec); continue; }
            // skip virtual file systems and folders already searched
            if ((Dir == "/") && ((Rec.Name == "proc") || (Rec.Name == "sys") || (Rec.Name == "dev") || (Rec.Name == "run"))) { I = FindNext(Rec); continue; }
            if (Dir + Rec.Name == QDir::homePath()) { I = FindNext(Rec); continue; }
#endif
            result = SearchDir(Dir + Rec.Name + PathDelim);
            if (result || Aborted) { FindClose(Rec); return result; }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TSetupDosBoxForm::AbortButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    Aborted = true;
}

/* global */

bool SearchDosBox(QWidget* AOwner, QString& ADOSBoxDir, bool AllowFullSearch)
{
    QString S;
    TStringList St;
    TSearchRec Rec;
    int I;
    bool result;

    if ((!Trim(ADOSBoxDir).isEmpty()) && DOSBoxExeExists(IncludeTrailingPathDelimiter(ADOSBoxDir))) {
        return true;
    }

    S = PrgDir() + "DOSBox" + PathDelim;
    if (DOSBoxExeExists(S)) { ADOSBoxDir = S; return true; }

    S = PrgDir() + BinFolder + PathDelim + "DOSBox" + PathDelim;
    if (DOSBoxExeExists(S)) { ADOSBoxDir = S; return true; }

    S = PrgDataDir() + "DOSBox" + PathDelim;
    if (DOSBoxExeExists(S)) { ADOSBoxDir = S; return true; }

#ifdef Q_OS_WIN
    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_));
    I = FindFirst(S + "*.*", faDirectory, Rec);
    while (I == 0) {
        if ((Rec.Name != ".") && (Rec.Name != "..") && (Copy(ExtUpperCase(Rec.Name), 1, 7) == "DOSBOX-")) St.Add(S + Rec.Name + PathDelim);
        I = FindNext(Rec);
    }
    FindClose(Rec);
    for (I = St.Count() - 1; I >= 0; I--) if (DOSBoxExeExists(St[I])) { ADOSBoxDir = St[I]; return true; }

    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "DOSBox" + PathDelim;
    if (DOSBoxExeExists(S)) { ADOSBoxDir = S; return true; }
    // (installer of DOSBox Staging)
    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "DOSBox Staging" + PathDelim;
    if (DOSBoxExeExists(S)) { ADOSBoxDir = S; return true; }
#else
    // Linux/macOS: DOSBox installed by the package manager (or by the user)
    // is found via the search path; then check the usual install locations.
    Q_UNUSED(Rec);
    // (DOSBox or DOSBox Staging; some packages install DOSBox Staging as
    // "dosbox-staging")
    for (const QString& Name : {DosBoxFileName, DosBoxStagingFileName}) {
        S = QStandardPaths::findExecutable(Name);
        if (!S.isEmpty()) { ADOSBoxDir = IncludeTrailingPathDelimiter(ExtractFilePath(S)); return true; }
    }
    St.Add(IncludeTrailingPathDelimiter(QDir::homePath()) + ".local/bin/");
    St.Add("/usr/local/bin/");
    St.Add("/usr/bin/");
    St.Add("/usr/games/");
    St.Add("/snap/bin/");
    St.Add("/opt/dosbox/");
    St.Add("/opt/dosbox/bin/");
#ifdef Q_OS_MACOS
    St.Add("/opt/homebrew/bin/");
    St.Add("/Applications/DOSBox.app/Contents/MacOS/");
#endif
    for (I = 0; I < St.Count(); I++) {
        QFileInfo FI(DOSBoxExeFile(St[I]));
        if (FI.isFile() && FI.isExecutable()) { ADOSBoxDir = St[I]; return true; }
    }
#endif

    if (!AllowFullSearch) return false;
    PrgSetup->SetDOSBoxFullSearchDone(true);

    SetupDosBoxForm = new TSetupDosBoxForm(AOwner);
    SetupDosBoxForm->SearchType = stDOSBox;
    result = false;
    if (SetupDosBoxForm->SearchDir(PrgDir())) { result = true; ADOSBoxDir = SetupDosBoxForm->DOSBoxDir; }
    else if ((PrgDataDir() != PrgDir()) && SetupDosBoxForm->SearchDir(PrgDataDir())) { result = true; ADOSBoxDir = SetupDosBoxForm->DOSBoxDir; }
    else {
        result = (SetupDosBoxForm->ShowModal() != mrAbort);
        if (result) ADOSBoxDir = SetupDosBoxForm->DOSBoxDir;
    }
    delete SetupDosBoxForm;
    SetupDosBoxForm = nullptr;
    return result;
}

bool SearchOggEnc(QWidget* AOwner)
{
    bool result;

    if (FileExists(PrgSetup->WaveEncOgg())) {
        return true;
    }

    if (FileExists(PrgDir() + OggEncPrgFile)) {
        PrgSetup->SetWaveEncOgg(PrgDir() + OggEncPrgFile);
        return true;
    }

    if (FileExists(PrgDir() + BinFolder + PathDelim + OggEncPrgFile)) {
        PrgSetup->SetWaveEncOgg(PrgDir() + BinFolder + PathDelim + OggEncPrgFile);
        return true;
    }

#ifndef Q_OS_WIN
    // oggenc (vorbis-tools) installed by the package manager
    if (!FindHelperProgram(OggEncPrgFile).isEmpty()) { PrgSetup->SetWaveEncOgg(FindHelperProgram(OggEncPrgFile)); return true; }
#endif

    SetupDosBoxForm = new TSetupDosBoxForm(AOwner);
    SetupDosBoxForm->SearchType = stOggEnc;
    result = (SetupDosBoxForm->ShowModal() != mrAbort);
    delete SetupDosBoxForm;
    SetupDosBoxForm = nullptr;
    return result;
}

bool SearchLame(QWidget* AOwner)
{
    QString S;
    bool result;

    if (FileExists(PrgSetup->WaveEncMp3())) {
        return true;
    }

    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "Lame" + PathDelim + LamePrgFile;
    if (FileExists(S)) { PrgSetup->SetWaveEncMp3(S); return true; }

    S = PrgDir() + BinFolder + PathDelim;
    if (FileExists(S)) { PrgSetup->SetWaveEncMp3(S); return true; }

#ifndef Q_OS_WIN
    S = FindHelperProgram(LamePrgFile);
    if (!S.isEmpty()) { PrgSetup->SetWaveEncMp3(S); return true; }
#endif

    SetupDosBoxForm = new TSetupDosBoxForm(AOwner);
    SetupDosBoxForm->SearchType = stLame;
    result = (SetupDosBoxForm->ShowModal() != mrAbort);
    delete SetupDosBoxForm;
    SetupDosBoxForm = nullptr;
    return result;
}

bool SearchScummVM(QWidget* AOwner)
{
    QString S;
    bool result;

    if (DirectoryExists(PrgSetup->ScummVMPath())) {
        return true;
    }

    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "ScummVM" + PathDelim;
    if (FileExists(S + ScummPrgFile)) { PrgSetup->SetScummVMPath(S); return true; }

#ifndef Q_OS_WIN
    S = FindHelperProgram(ScummPrgFile);
    if (!S.isEmpty()) { PrgSetup->SetScummVMPath(ExtractFilePath(S)); return true; }
#endif

    SetupDosBoxForm = new TSetupDosBoxForm(AOwner);
    SetupDosBoxForm->SearchType = stScummVM;
    result = (SetupDosBoxForm->ShowModal() != mrAbort);
    delete SetupDosBoxForm;
    SetupDosBoxForm = nullptr;
    return result;
}

bool SearchQBasicVM(QWidget* AOwner)
{
    QString S;
    bool result;

    if (FileExists(PrgSetup->QBasic())) {
        return true;
    }

    S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "QBasic" + PathDelim;
    if (FileExists(S + QBasicPrgFile)) { PrgSetup->SetQBasic(S + QBasicPrgFile); return true; }

    S = PrgDir() + BinFolder + PathDelim;
    if (FileExists(S + QBasicPrgFile)) { PrgSetup->SetQBasic(S + QBasicPrgFile); return true; }

    SetupDosBoxForm = new TSetupDosBoxForm(AOwner);
    SetupDosBoxForm->SearchType = stQBasic;
    result = (SetupDosBoxForm->ShowModal() != mrAbort);
    delete SetupDosBoxForm;
    SetupDosBoxForm = nullptr;
    return result;
}

void FastSearchAllTools()
{
    QString S;

#ifndef Q_OS_WIN
    // programs installed by the package manager (oggenc, lame, scummvm)
    if (!FileExists(PrgSetup->WaveEncOgg()) && !FindHelperProgram(OggEncPrgFile).isEmpty()) PrgSetup->SetWaveEncOgg(FindHelperProgram(OggEncPrgFile));
    if (!FileExists(PrgSetup->WaveEncMp3()) && !FindHelperProgram(LamePrgFile).isEmpty()) PrgSetup->SetWaveEncMp3(FindHelperProgram(LamePrgFile));
    if (!DirectoryExists(PrgSetup->ScummVMPath()) && !FindHelperProgram(ScummPrgFile).isEmpty()) PrgSetup->SetScummVMPath(ExtractFilePath(FindHelperProgram(ScummPrgFile)));
#endif

    if (!FileExists(PrgSetup->WaveEncOgg())) {
        if (FileExists(PrgDir() + OggEncPrgFile)) PrgSetup->SetWaveEncOgg(PrgDir() + OggEncPrgFile); else {
            if (FileExists(PrgDir() + BinFolder + PathDelim + OggEncPrgFile)) PrgSetup->SetWaveEncOgg(PrgDir() + BinFolder + PathDelim + OggEncPrgFile);
        }
    }

    if (!FileExists(PrgSetup->WaveEncMp3())) {
        S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "Lame" + PathDelim + LamePrgFile;
        if (FileExists(S)) PrgSetup->SetWaveEncMp3(S); else {
            if (FileExists(PrgDir() + BinFolder + PathDelim + LamePrgFile)) PrgSetup->SetWaveEncMp3(PrgDir() + BinFolder + PathDelim + LamePrgFile);
        }
    }

    if (!DirectoryExists(PrgSetup->ScummVMPath())) {
        S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "ScummVM" + PathDelim;
        if (FileExists(S + ScummPrgFile)) PrgSetup->SetScummVMPath(S);
    }

    if (!FileExists(PrgSetup->QBasic())) {
        S = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROGRAM_FILES_)) + "QBasic" + PathDelim;
        if (FileExists(S + QBasicPrgFile)) PrgSetup->SetQBasic(S + QBasicPrgFile); else {
            if (FileExists(PrgDir() + BinFolder + PathDelim + QBasicPrgFile)) PrgSetup->SetQBasic(PrgDir() + BinFolder + PathDelim + QBasicPrgFile);
        }
    }
}
