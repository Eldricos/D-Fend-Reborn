// C++ port of InstallationRunFormUnit.pas (implementation section).
#include "InstallationRunFormUnit.h"
#include "DosCodePage.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "DosBoxUnit.h"
#include "DOSBoxTempUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "MainUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#include "ZipInfoFormUnit.h"

TInstallationRunForm* InstallationRunForm = nullptr;

// TCheckListBox helpers
static void CheckListAdd(QListWidget* L, const QString& S)
{
    QListWidgetItem* It = new QListWidgetItem(S, L);
    It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
    It->setCheckState(Qt::Unchecked);
}
static void CheckListSetChecked(QListWidget* L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }

TInstallationRunForm::TInstallationRunForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // SourceListBox.OnClickCheck / SourceListBox.OnClick = SourceListBoxClick
    connect(ui.SourceListBox, &QListWidget::itemChanged, this, [this]() { if (!InSourceListBoxClick) SourceListBoxClick(ui.SourceListBox); });
    connect(ui.SourceListBox, &QListWidget::itemClicked, this, [this]() { if (!InSourceListBoxClick) SourceListBoxClick(ui.SourceListBox); });

    FormCreate(this);
}

TInstallationRunForm::~TInstallationRunForm()
{
    FormDestroy(this);
}

void TInstallationRunForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TInstallationRunForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    InstallType = itArchive;
    Sources = new TStringList;
    NewGameDir = "";
    FilesAlreadyInTempDir = "";
    AbortSetup = false;
    SecondTry = false;

    setWindowTitle(LanguageSetup->InstallationSupportRun());
    ui.InfoLabel->setText(LanguageSetup->InstallationSupportRunInfoStarting());
    ui.SourceLabel->setText(LanguageSetup->InstallationSupportRunSource());

    TempFolder = "";
}

void TInstallationRunForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // PostMessage(Handle,WM_USER+1,0,0);
    QTimer::singleShot(0, this, [this]() { QEvent* Msg = nullptr; PostShow(Msg); });
}

void TInstallationRunForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete Sources;
    Sources = nullptr;
}

void TInstallationRunForm::PostShow(QEvent*& Msg)
{
    Q_UNUSED(Msg);
    GamesDir = InitialScanDir(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
    do {
        ui.SourceListBox->clear();
        AbortSetup = false;
        ui.InfoLabel->setText(LanguageSetup->InstallationSupportRunInfoRunning());
        RunInstaller();
        if (AbortSetup) { setModalResult(mrCancel); break; }
        ui.InfoLabel->setText(LanguageSetup->InstallationSupportRunInfoNewGameScan());
        NewGameDir = FindNewDir(GamesDir, MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
        if (NewGameDir != "") {
            setModalResult(mrOk);
        } else {
            if (MessageDlg(LanguageSetup->InstallationSupportRunInfoNewGameScanError(), mtError, mbYes | mbNo, 0) != mrYes) {
                setModalResult(mrCancel); break;
            }
            SecondTry = true;
        }
    } while (ModalResult() != mrOk);
    FreeGamesDirData(GamesDir);
    GamesDir = nullptr;
}

TStringList* TInstallationRunForm::InitialScanDir(const QString& Dir)
{
    TSearchRec Rec;
    int I, J;
    TStringList* St;

    TStringList* result = new TStringList;
    J = 0;
    I = FindFirst(IncludeTrailingPathDelimiter(Dir) + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
            J++; if (J == 10) ProcessMessages();
            St = InitialScanDir(IncludeTrailingPathDelimiter(Dir) + Rec.Name);
            if (St->Count() == 0) { delete St; St = nullptr; }
            result->AddObject(ExtUpperCase(Rec.Name), St);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

QString TInstallationRunForm::FindNewDir(TStringList* OldDir, const QString& NewDir)
{
    TSearchRec Rec;
    int I, J, K;
    QString result = "";

    J = 0;
    I = FindFirst(IncludeTrailingPathDelimiter(NewDir) + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
            J++; if (J == 10) ProcessMessages();
            if (OldDir == nullptr) {
                FindClose(Rec);
                return IncludeTrailingPathDelimiter(NewDir) + Rec.Name + PathDelim;
            }
            K = OldDir->IndexOf(ExtUpperCase(Rec.Name));
            if (K < 0) {
                FindClose(Rec);
                return IncludeTrailingPathDelimiter(NewDir) + Rec.Name + PathDelim;
            } else {
                result = FindNewDir(OldDir->ObjectAs<TStringList>(K), IncludeTrailingPathDelimiter(NewDir) + Rec.Name);
                if (result != "") { FindClose(Rec); return result; }
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TInstallationRunForm::FreeGamesDirData(TStringList* St)
{
    int I;
    if (St == nullptr) return;
    for (I = 0; I < St->Count(); I++) if (St->Objects(I) != nullptr) FreeGamesDirData(St->ObjectAs<TStringList>(I));
    delete St;
}

void TInstallationRunForm::HideSourceSection()
{
    VclLayout::setClientSize(this, width(), 2 * ui.InfoLabel->y() + ui.InfoLabel->height());
    ui.SourceListBox->setVisible(false);
}

QString TInstallationRunForm::FindFileToStart(const QString& Dir)
{
    QString S;
    int I, J;
    TSearchRec Rec;
    TStringList* Names;

    S = IncludeTrailingPathDelimiter(Dir);

    Names = ValueToList(PrgSetup->InstallerNames());
    for (I = 0; I < Names->Count(); I++) {
        for (J = 0; J < ProgramExts.size(); J++) {
            if (FileExists(S + (*Names)[I] + "." + ProgramExts[J])) {
                QString result = S + (*Names)[I] + "." + ProgramExts[J];
                delete Names;
                return result;
            }
        }
    }
    delete Names;

    TStringList St;
    for (I = 0; I < ProgramExts.size(); I++) {
        J = FindFirst(S + "*." + ProgramExts[I], faAnyFile, Rec);
        while (J == 0) {
            St.Add(Rec.Name);
            J = FindNext(Rec);
        }
        FindClose(Rec);
    }
    if (St.Count() == 1) return S + St[0];
    return "";
}

static QChar FloppyDriveLetter()
{
    QChar result = 'A';
    if (PrgSetup->DefaultFloppyDrive() != "") result = PrgSetup->DefaultFloppyDrive()[0];
#ifdef Q_OS_WIN
    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) == DRIVE_REMOVABLE) return QChar(C);
    }
#else
    // TODO(port): no drive letters outside of Windows - the default floppy drive setting is used
#endif
    return result;
}

static QChar GetCDDriveWithMedia()
{
    QChar result = 'D';
#ifdef Q_OS_WIN
    int I;
    TSearchRec Rec;

    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) == DRIVE_CDROM) {
            I = FindFirst(Root + "*.*", faAnyFile, Rec); FindClose(Rec);
            if (I == 0) return QChar(C);
        }
    }

    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) == DRIVE_CDROM) return QChar(C);
    }
#else
    // TODO(port): no drive letters outside of Windows
#endif
    return result;
}

void TInstallationRunForm::BuildMountCommand(TGame* Game, QString& FileToStart, TStringList* StartUpCmds)
{
    QString S;
    int I;

    Game->SetNrOfMounts(2);
    Game->SetMount0(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");

    switch (InstallType) {
        case itFloppy:
            Game->SetMount1(QString(FloppyDriveLetter()) + ":\\;FLOPPY;A;False;;");
            if (SecondTry) FileToStart = ""; else FileToStart = FindFileToStart(QString(FloppyDriveLetter()) + ":\\");
            if (FileToStart == "") StartUpCmds->Add("A:");
            HideSourceSection();
            break;
        case itFolder:
            if (Sources->Count() == 1) {
                Game->SetMount1((*Sources)[0] + ";Drive;A;false;");
                FileToStart = FindFileToStart((*Sources)[0]);
                if (FileToStart == "") StartUpCmds->Add("A:");
                HideSourceSection();
            } else {
                TempFolder = TempDir() + TempSubFolder;
                Game->SetMount1(TempFolder + ";Drive;A;false;");
                InSourceListBoxClick = true;
                for (I = 0; I < Sources->Count(); I++) CheckListAdd(ui.SourceListBox, (*Sources)[I]);
                CheckListSetChecked(ui.SourceListBox, 0, true);
                ui.SourceListBox->setCurrentRow(0);
                InSourceListBoxClick = false;
                LastFolder = -1;
                SourceListBoxClick(this);
                if (SecondTry) FileToStart = ""; else FileToStart = FindFileToStart(TempFolder);
                if (FileToStart == "") StartUpCmds->Add("A:");
            }
            break;
        case itArchive:
            if (FilesAlreadyInTempDir != "") TempFolder = FilesAlreadyInTempDir; else TempFolder = TempDir() + TempSubFolder;
            Game->SetMount1(TempFolder + ";Drive;A;false;");
            if (FilesAlreadyInTempDir == "") {
                if (!ExtractZipFile(this, (*Sources)[0], TempFolder)) AbortSetup = true;
            }
            if (SecondTry) FileToStart = ""; else FileToStart = FindFileToStart(TempFolder);
            if (FileToStart == "") StartUpCmds->Add("A:");
            if (Sources->Count() == 1) {
                HideSourceSection();
            } else {
                InSourceListBoxClick = true;
                for (I = 0; I < Sources->Count(); I++) CheckListAdd(ui.SourceListBox, (*Sources)[I]);
                CheckListSetChecked(ui.SourceListBox, 0, true);
                InSourceListBoxClick = false;
                LastFolder = 0;
            }
            break;
        case itFloppyImage:
            StartUpCmds->Add("A:");
            if (PrgSetup->AllowMultiFloppyImagesMount()) {
                S = (*Sources)[0];
                for (I = 1; I < Sources->Count(); I++) S = S + "$" + (*Sources)[I];
                Game->SetMount1(S + ";FLOPPYIMAGE;A;;;");
                HideSourceSection();
            } else {
                if (Sources->Count() == 1) {
                    Game->SetMount1((*Sources)[0] + ";FLOPPYIMAGE;A;;;");
                    HideSourceSection();
                } else {
                    TempFolder = TempDir() + TempSubFolder;
                    ForceDirectories(TempFolder);
                    Game->SetMount1(TempFolder + PathDelim + "Temp.img;FLOPPYIMAGE;A;false;");
                    InSourceListBoxClick = true;
                    for (I = 0; I < Sources->Count(); I++) CheckListAdd(ui.SourceListBox, (*Sources)[I]);
                    CheckListSetChecked(ui.SourceListBox, 0, true);
                    ui.SourceListBox->setCurrentRow(0);
                    InSourceListBoxClick = false;
                    LastFolder = -1;
                    SourceListBoxClick(this);
                }
            }
            break;
        case itCD:
            S = QString(GetCDDriveWithMedia()) + ":\\";
            Game->SetMount1(S + ";CDROM;D;false;;");
            if (SecondTry) FileToStart = ""; else FileToStart = FindFileToStart(S);
            if (FileToStart == "") StartUpCmds->Add("D:");
            HideSourceSection();
            break;
        case itCDImage:
            S = (*Sources)[0];
            for (I = 1; I < Sources->Count(); I++) S = S + "$" + (*Sources)[I];
            Game->SetMount1(S + ";CDROMIMAGE;D;;;");
            StartUpCmds->Add("D:");
            HideSourceSection();
            break;
    }
}

static void SplitText(TStringList* St, const QString& Lines)
{
    int I;
    QString S;

    // CharToOEM(PChar(Lines),PChar(S));
#ifdef Q_OS_WIN
    S = CharToOemAnsi(Lines);
#else
    // (Linux/macOS: the DOSBox conf writer converts the autoexec lines to the
    // DOS code page, see WriteDOSBoxConfFile)
    S = Lines;
#endif

    while (Length(S) > 75) {
        I = 75;
        while ((I > 1) && (S[I - 1] != ' ')) I--;
        if (S[I - 1] == ' ') {
            /* space in first 75 chars */
            St->Add("echo " + Trim(Copy(S, 1, I - 1)));
            S = Trim(Copy(S, I + 1, MaxInt));
        } else {
            /* no space in 1..75 */
            I = 76;
            while ((I < Length(S)) && (S[I - 1] != ' ')) I++;
            if (I == Length(S)) {
                /* no space at all */
                St->Add("echo " + Trim(S));
                return;
            } else {
                /* space after 75 */
                St->Add("echo " + Trim(Copy(S, 1, I - 1)));
                S = Trim(Copy(S, I + 1, MaxInt));
            }
        }
    }
    St->Add("echo " + Trim(S));
}

void TInstallationRunForm::RunInstaller()
{
    TTempGame* TempGame;
    TStringList *St, *St2, *St3;
    QString FileToStart;
    THandle DOSBoxHandle;
    int I;

    TempGame = new TTempGame;
    std::unique_ptr<TTempGame> TempGameGuard(TempGame);

    TempGame->Game()->SetStartFullscreen(false);
    TempGame->Game()->SetCloseDosBoxAfterGameExit(false);
    if ((ExtUpperCase(TempGame->Game()->Cycles()) == "AUTO") || (TryStrToInt(TempGame->Game()->Cycles(), I) && (I < 20000))) TempGame->Game()->SetCycles("20000");
    FileToStart = "";
    St2 = new TStringList;
    {
        std::unique_ptr<TStringList> St2Guard(St2);
        BuildMountCommand(TempGame->Game(), FileToStart, St2);
        if (AbortSetup) return;
        St = StringToStringList(TempGame->Game()->Autoexec());
        std::unique_ptr<TStringList> StGuard(St);
        St->AddStrings(*St2);
        St->Add("cls");
        if (FileToStart == "") {
            SplitText(St, LanguageSetup->InstallationSupportRunNoGameFile());
        } else {
            SplitText(St, LanguageSetup->InstallationSupportRunStartGameFile1());
            St->Add("echo.");
            SplitText(St, LanguageSetup->InstallationSupportRunStartGameFile2());
            TempGame->Game()->SetGameExe(FileToStart);
            St3 = new TStringList;
            St3->Add("echo.");
            SplitText(St3, LanguageSetup->InstallationSupportRunStartGameFile3());
            St3->Add("echo.");
            SplitText(St3, LanguageSetup->InstallationSupportRunStartGameFile2());
            St3->Add("echo.");
            TempGame->Game()->SetAutoexecFinalization(StringListToString(St3));
            delete St3;
        }
        St->Add("echo.");
        SplitText(St, LanguageSetup->InstallationSupportRunInstallationStart1());
        St->Add("echo.");
        SplitText(St, LanguageSetup->InstallationSupportRunInstallationStart2());
        if ((InstallType == itCDImage) && (Sources->Count() > 1)) {
            St->Add("echo.");
            SplitText(St, LanguageSetup->InstallationSupportRunInstallationStart3());
        }
        if ((InstallType == itFloppyImage) && (Sources->Count() > 1) && PrgSetup->AllowMultiFloppyImagesMount()) {
            St->Add("echo.");
            SplitText(St, LanguageSetup->InstallationSupportRunInstallationStart4());
        }
        if (FileToStart != "") {
            St->Add("echo.");
            St->Add("pause");
        }
        TempGame->Game()->SetAutoexec(StringListToString(St));
    }
    TempGame->Game()->StoreAllValues();
    DOSBoxHandle = RunCommandAndGetHandle(TempGame->Game(), DFendRebornMainForm->DeleteOnExit, "", true);
    // While not (WaitForSingleObject(DOSBoxHandle,0)=WAIT_OBJECT_0) do ...
    while ((DOSBoxHandle != 0) && IsProcessRunning(DOSBoxHandle)) {
        Sleep(100);
        ProcessMessages();
    }
    if ((TempFolder != "") && (FilesAlreadyInTempDir == "")) ExtDeleteFolder(TempFolder, ftTemp);
}

void TInstallationRunForm::SourceListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    bool InitialChange;

    if (ui.SourceListBox->currentRow() < 0) return;
    InSourceListBoxClick = true;
    for (I = 0; I < ui.SourceListBox->count(); I++) CheckListSetChecked(ui.SourceListBox, I, I == ui.SourceListBox->currentRow());
    InSourceListBoxClick = false;
    if (ui.SourceListBox->currentRow() == LastFolder) return;
    InitialChange = (LastFolder == -1);
    LastFolder = ui.SourceListBox->currentRow();

    if (InstallType == itFolder) {
        ExtDeleteFolder(TempFolder, ftTemp);
        CopyFiles(IncludeTrailingPathDelimiter((*Sources)[LastFolder]), TempFolder, false, false);
    }

    if (InstallType == itArchive) {
        ExtDeleteFolder(TempFolder, ftTemp);
        ExtractZipFile(this, (*Sources)[LastFolder], TempFolder);
    }

    if (InstallType == itFloppyImage) {
        ExtDeleteFile(TempFolder + PathDelim + "Temp.img", ftTemp);
        CopyFile((*Sources)[LastFolder], TempFolder + PathDelim + "Temp.img", false);
    }

    if (!InitialChange) MessageDlg(LanguageSetup->InstallationSupportRunUpdateDirectoryInformation(), mtInformation, mbOK, 0);
}
