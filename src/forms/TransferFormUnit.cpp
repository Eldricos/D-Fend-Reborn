// C++ port of TransferFormUnit.pas (implementation section).
#include "TransferFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "WaitFormUnit.h"
#include "GameDBToolsUnit.h"
#include "SmallWaitFormUnit.h"
#include "UninstallFormUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TTransferForm* TransferForm = nullptr;

// TCheckListBox helpers
static bool CheckListChecked(QListWidget* L, int I) { return L->item(I)->checkState() == Qt::Checked; }
static void CheckListSetChecked(QListWidget* L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
// ListBox.Items.Objects[I] (BuildCheckList stores the TGame pointer as Qt::UserRole data)
static TGame* CheckListGame(QListWidget* L, int I) { return static_cast<TGame*>(L->item(I)->data(Qt::UserRole).value<void*>()); }

// "Copy(S,1,2)='.\'" - relative path as returned by MakeRelPath
static bool IsDotRelPath(const QString& S) { return Copy(S, 1, 2) == ".\\" || Copy(S, 1, 2) == "./"; }
// "Copy(S,2,2)=':\'" - path could not be made relative to the base dir
static bool PathNotRelative(const QString& S)
{
    if (Copy(S, 2, 2) == ":\\" || Copy(S, 2, 2) == ":/") return true;
#ifndef Q_OS_WIN
    if (Copy(S, 1, 1) == "/") return true;
#endif
    return false;
}

TTransferForm::TTransferForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.DestPrgDirButton, &QToolButton::clicked, this, [this]() { DestPrgDirButtonClick(ui.DestPrgDirButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TTransferForm::~TTransferForm() = default;

void TTransferForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TTransferForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->TransferForm());
    ui.InfoLabel->setText(LanguageSetup->TransferFormInfo());
    ui.DestPrgDirEdit_EditLabel->setText(LanguageSetup->TransferFormDestPrgDir());
    ui.DestPrgDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    ui.CopyDFRCheckBox->setText(LanguageSetup->TransferFormCopyDFendRebornPrgFiles());

    UserIconLoader->DialogImage(DI_SelectFolder, ui.DestPrgDirButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TTransferForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, false, false, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, false, true);
}

void TTransferForm::SelectButtonClick(QObject* Sender)
{
    int I;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, Sender->property("tag").toInt() == 0);
                break;
            case 2: {
                QPoint P = ui.SelectGenreButton->mapToGlobal(QPoint(0, 0));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TTransferForm::DestPrgDirButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = ui.DestPrgDirEdit->text();
    if (SelectDirectory((THandle)winId(), LanguageSetup->ChooseFolder(), S)) ui.DestPrgDirEdit->setText(S);
}

bool TTransferForm::CopyInstallation(const QString& DestPrgDir)
{
    TStringList* St;
    int I;
    QString S;
    TPrgSetup* NewSetup;
    TGameDB* NewGameDB;
    TOperationMode OperationModeSave;
    TSearchRec Rec;

    LoadAndShowSmallWaitForm(LanguageSetup->TransferFormCopyDFendRebornPrgFilesWait());
    struct WaitGuard { ~WaitGuard() { FreeSmallWaitForm(); } } Guard;  // try/finally

    /* Create destination folder */

    if (!ForceDirectories(DestPrgDir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestPrgDir}), mtError, mbOK, 0);
        return false;
    }

    if (!ForceDirectories(DestPrgDir + BinFolder + PathDelim)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestPrgDir + BinFolder + PathDelim}), mtError, mbOK, 0);
        return false;
    }

    if (!ForceDirectories(DestPrgDir + LanguageSubDir + PathDelim)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestPrgDir + BinFolder + PathDelim}), mtError, mbOK, 0);
        return false;
    }

    if (!ForceDirectories(DestPrgDir + SettingsFolder + PathDelim)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestPrgDir + SettingsFolder + PathDelim}), mtError, mbOK, 0);
        return false;
    }

    /* Transfer program files */

    /* PrgDir -> PrgDir */
    St = new TStringList;
    St->Add(ExtractFileName(ParamStr(0)));  // Application.ExeName
    St->Add("Readme_OperationMode.txt");
    for (I = 0; I < St->Count(); I++) {
        CopyFile(PrgDir() + (*St)[I], DestPrgDir + (*St)[I], false);
        ProcessMessages();
    }
    delete St;

    /* PrgDir+BinFolder (or fall back PrgDir) -> PrgDir+BinFolder */
    St = new TStringList;
    St->Add(MakeDOSFilesystemFileName);
    St->Add(OggEncPrgFile);
    St->Add("License.txt");
    St->Add("LicenseComponents.txt");
    St->Add("ChangeLog.txt");
    St->Add(NSIInstallerHelpFile);
    St->Add("SetInstallerLanguage.exe");
    St->Add("7za.dll");
    St->Add("DelZip179.dll");
    St->Add("mediaplr.dll");
    St->Add("UpdateCheck.exe");
    St->Add("InstallVideoCodec.exe");
    for (I = 0; I < St->Count(); I++) {
        S = "";
        if (FileExists(PrgDir() + BinFolder + PathDelim + (*St)[I])) S = PrgDir() + BinFolder + PathDelim + (*St)[I];
        if ((S == "") && FileExists(PrgDir() + (*St)[I])) S = PrgDir() + (*St)[I];
        if (S != "") {
            CopyFile(S, DestPrgDir + BinFolder + PathDelim + (*St)[I], false);
            ProcessMessages();
        }
    }
    delete St;

    /* PrgDataDir+SettingsFolder (or fall back PrgDataDir or fall back PrgDir+BinFolder or even more fall back PrgDir) -> PrgDir+SettingsFolder */
    St = new TStringList;
    St->Add("Links.txt");
    St->Add("SearchLinks.txt");
    for (I = 0; I < St->Count(); I++) {
        S = "";
        if (FileExists(PrgDataDir() + SettingsFolder + PathDelim + (*St)[I])) S = PrgDataDir() + SettingsFolder + PathDelim + (*St)[I];
        if ((S == "") && FileExists(PrgDataDir() + (*St)[I])) S = PrgDataDir() + (*St)[I];
        if ((S == "") && FileExists(PrgDir() + BinFolder + PathDelim + (*St)[I])) S = PrgDir() + BinFolder + PathDelim + (*St)[I];
        if ((S == "") && FileExists(PrgDir() + (*St)[I])) S = PrgDir() + (*St)[I];
        if (S != "") {
            CopyFile(S, DestPrgDir + SettingsFolder + PathDelim + (*St)[I], false);
            ProcessMessages();
        }
    }
    delete St;

    if (!CopyDir(IncludeTrailingPathDelimiter(PrgDir() + LanguageSubDir), IncludeTrailingPathDelimiter(DestPrgDir + LanguageSubDir))) return false;

    /* Transfer settings */

    St = new TStringList;
    St->Add(ConfOptFile);
    St->Add(ScummVMConfOptFile);
    St->Add(IconsConfFile);
    St->Add(MainSetupFile);
    St->Add(NSIInstallerHelpFile);
    for (I = 0; I < St->Count(); I++) {
        S = "";
        if (FileExists(PrgDataDir() + SettingsFolder + PathDelim + (*St)[I])) S = PrgDataDir() + SettingsFolder + PathDelim + (*St)[I];
        if ((S == "") && FileExists(PrgDataDir() + (*St)[I])) S = PrgDataDir() + (*St)[I];
        if (S != "") {
            CopyFile(S, DestPrgDir + SettingsFolder + PathDelim + (*St)[I], false);
            ProcessMessages();
        }
    }
    delete St;

    {
        TStringList St2;
        St2.Add("PORTABLEMODE");
        if (!St2.SaveToFile(DestPrgDir + OperationModeConfig)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {DestPrgDir + OperationModeConfig}), mtError, mbOK, 0);
            return false;
        }
    }

    {
        TStringList St2;
        St2.Add(IconsSubDir);
        St2.Add(GameListSubDir);
        St2.Add("VirtualHD");
        for (I = 0; I < St2.Count(); I++) {
            if (!ForceDirectories(DestPrgDir + St2[I])) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestPrgDir + St2[I]}), mtError, mbOK, 0);
                return false;
            }
        }
    }

    {
        TStringList St2;
        St2.Add(AutoSetupSubDir);
        St2.Add(LanguageSubDir);
        St2.Add(TemplateSubDir);
        for (I = 0; I < St2.Count(); I++) {
            if (!CopyDir(IncludeTrailingPathDelimiter(PrgDataDir() + St2[I]), IncludeTrailingPathDelimiter(DestPrgDir + St2[I]))) return false;
            ProcessMessages();
        }
    }

    /* Transfer icon sets */

    CopyDir(IncludeTrailingPathDelimiter(PrgDataDir() + IconSetsFolder), IncludeTrailingPathDelimiter(DestPrgDir + IconSetsFolder));
    if (PrgDir() != PrgDataDir()) {
        CopyFile(PrgDir() + IconSetsFolder + PathDelim + "IconSets_Readme.txt", DestPrgDir + IconSetsFolder + PathDelim + "IconSets_Readme.txt", true);
        I = FindFirst(PrgDir() + IconSetsFolder + PathDelim + "*.*", faDirectory, Rec);
        while (I == 0) {
            if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
                if (!DirectoryExists(DestPrgDir + IconSetsFolder + PathDelim + Rec.Name))
                    CopyDir(IncludeTrailingPathDelimiter(PrgDir() + IconSetsFolder + PathDelim + Rec.Name), IncludeTrailingPathDelimiter(DestPrgDir + IconSetsFolder + PathDelim + Rec.Name));
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }

    /* Transfer tools */

    {
        TStringList St2;
        St2.Add("FREEDOS");
        St2.Add("DOSZIP");
        for (I = 0; I < St2.Count(); I++) {
            S = "";
            if (DirectoryExists(PrgDir() + NewUserDataSubDir + PathDelim + St2[I])) S = PrgDir() + NewUserDataSubDir + PathDelim + St2[I]; else {
                if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())) + St2[I])) S = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())) + St2[I];
            }
            if (S != "") {
                if (!CopyDir(IncludeTrailingPathDelimiter(S), DestPrgDir + "VirtualHD" + PathDelim + St2[I] + PathDelim)) return false;
            }
        }
    }

    /* Transfer DOSBox */

    if (!CopyDir(IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()), IncludeTrailingPathDelimiter(DestPrgDir + "DOSBox"))) return false;
    if (Trim(PrgSetup->DOSBoxSettings(0)->DosBoxMapperFile()) != "")
        CopyFile(MakeAbsPath(PrgSetup->DOSBoxSettings(0)->DosBoxMapperFile(), PrgSetup->BaseDir()), IncludeTrailingPathDelimiter(DestPrgDir + "DOSBox") + ExtractFileName(PrgSetup->DOSBoxSettings(0)->DosBoxMapperFile()), false);

    /* Change DFend.ini */

    OperationModeSave = OperationMode;
    NewSetup = new TPrgSetup(DestPrgDir + SettingsFolder + PathDelim + MainSetupFile);
    if (ExtUpperCase(PrgSetup->WaveEncOgg()) == ExtUpperCase(PrgDir() + BinFolder + PathDelim + OggEncPrgFile)) {
        NewSetup->SetWaveEncOgg(DestPrgDir + BinFolder + PathDelim + OggEncPrgFile);
    }
    NewSetup->SetBaseDir(DestPrgDir);
    NewSetup->SetGameDir(DestPrgDir + "VirtualHD");
    NewSetup->SetDataDir(DestPrgDir + "GameData");
    NewSetup->SetCaptureDir(DestPrgDir + CaptureSubDir);
    NewSetup->DOSBoxSettings(0)->SetDosBoxDir(DestPrgDir + "DOSBox");
    if (Trim(PrgSetup->DOSBoxSettings(0)->DosBoxLanguage()) == "")
        NewSetup->DOSBoxSettings(0)->SetDosBoxLanguage("");
    else
        NewSetup->DOSBoxSettings(0)->SetDosBoxLanguage(DestPrgDir + "DOSBox" + PathDelim + ExtractFileName(PrgSetup->DOSBoxSettings(0)->DosBoxLanguage()));
    NewSetup->DOSBoxSettings(0)->SetDosBoxMapperFile("." + PathDelim + ExtractFileName(PrgSetup->DOSBoxSettings(0)->DosBoxMapperFile()));
    NewSetup->SetPathToFREEDOS("." + PathDelim + "VirtualHD" + PathDelim + "FREEDOS" + PathDelim);
    if (FileExists(DestPrgDir + OggEncPrgFile)) NewSetup->SetWaveEncOgg("." + PathDelim + OggEncPrgFile);

    S = MakeRelPath(PrgSetup->ScummVMPath(), PrgSetup->BaseDir());
    if (IsDotRelPath(S)) NewSetup->SetScummVMPath(MakeAbsPath(S, DestPrgDir));
    S = MakeRelPath(PrgSetup->QBasic(), PrgSetup->BaseDir());
    if (IsDotRelPath(S)) NewSetup->SetQBasic(MakeAbsPath(S, DestPrgDir));
    S = MakeRelPath(PrgSetup->WaveEncOgg(), PrgSetup->BaseDir());
    if (IsDotRelPath(S)) NewSetup->SetWaveEncOgg(MakeAbsPath(S, DestPrgDir));
    S = MakeRelPath(PrgSetup->WaveEncMp3(), PrgSetup->BaseDir());
    if (IsDotRelPath(S)) NewSetup->SetWaveEncMp3(MakeAbsPath(S, DestPrgDir));
    NewSetup->DOSBoxBasedUserInterpretersPrograms()->Clear();
    for (I = 0; I < PrgSetup->DOSBoxBasedUserInterpretersPrograms()->Count(); I++) {
        S = MakeRelPath((*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[I], PrgSetup->BaseDir());
        if (!IsDotRelPath(S)) S = (*PrgSetup->DOSBoxBasedUserInterpretersPrograms())[I];
        NewSetup->DOSBoxBasedUserInterpretersPrograms()->Add(S);
    }

    OperationMode = omPortable;
    TempPrgDir = DestPrgDir;
    delete NewSetup;  // saves the settings (in portable mode relative to DestPrgDir)
    OperationMode = OperationModeSave;
    TempPrgDir = "";

    /* Build "DOSBox DOS" profile */

    NewGameDB = new TGameDB(DestPrgDir + GameListSubDir);
    BuildDefaultDosProfile(NewGameDB, false);
    delete NewGameDB;

    {
        TStringList St2;
        St2.Add("IconLibrary");
        St2.Add("Capture");
        for (I = 0; I < St2.Count(); I++) {
            S = "";
            if (DirectoryExists(PrgDir() + NewUserDataSubDir + PathDelim + St2[I])) S = PrgDir() + NewUserDataSubDir + PathDelim + St2[I];
            if (S != "") {
                if (!CopyDir(IncludeTrailingPathDelimiter(S), DestPrgDir + St2[I] + PathDelim)) return false;
            }
        }
    }

    return true;
}

bool TTransferForm::GetOperationMode(TOperationMode& OpMode)
{
    QString S;
    int I;

    S = Trim(ui.DestPrgDirEdit->text());
    if (S == "") {
        MessageDlg(LanguageSetup->MessageNoInstallationSelected(), mtError, mbOK, 0);
        return false;
    }

    OpMode = omPrgDir;

    S = IncludeTrailingPathDelimiter(S) + ExtractFileName(OperationModeConfig);
    if (!FileExists(S)) return true;

    TStringList St;
    if (!St.LoadFromFile(S)) return true;
    for (I = 0; I < St.Count(); I++) {
        S = Trim(ExtUpperCase(St[I]));
        if (S == "PRGDIRMODE") { OpMode = omPrgDir; return true; }
        if (S == "USERDIRMODE") { OpMode = omUserDir; return true; }
        if (S == "PORTABLEMODE") { OpMode = omPortable; return true; }
    }
    return true;
}

bool TTransferForm::GetDestPrgDataDir(TOperationMode OpMode, QString& DestPrgDataDir)
{
    QString S, T;
    TPrgSetup* LocalPrgSetup;
    TOperationMode SaveOpMode;

    if (OpMode == omUserDir) {
        S = UserDataDir();
    } else {
        S = IncludeTrailingPathDelimiter(Trim(ui.DestPrgDirEdit->text()));
    }

    T = S;
    S = S + SettingsFolder + PathDelim + ExtractFileName(MainSetupFile);
    if (!FileExists(S)) {
        MessageDlg(Format(LanguageSetup->MessageNoSetupFileFound(), {ExtractFilePath(S)}), mtError, mbOK, 0);
        return false;
    }

    SaveOpMode = OperationMode;
    OperationMode = OpMode;
    TempPrgDir = IncludeTrailingPathDelimiter(ExtractFilePath(T));
    LocalPrgSetup = new TPrgSetup(S);
    DestPrgDataDir = LocalPrgSetup->BaseDir();
    delete LocalPrgSetup;
    OperationMode = SaveOpMode;
    TempPrgDir = "";

    return true;
}

bool TTransferForm::CopyDir(const QString& SourceDir, const QString& DestDir)
{
    TSearchRec Rec;
    int I;
    bool result = false;

    if (!ForceDirectories(DestDir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {DestDir}), mtError, mbOK, 0);
        return false;
    }

    I = FindFirst(SourceDir + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) != 0) {
            if (Rec.Name[0] != '.') {
                if (!CopyDir(SourceDir + Rec.Name + PathDelim, DestDir + Rec.Name + PathDelim)) { FindClose(Rec); return result; }
            }
        } else {
            ProcessMessages();
            if (!CopyFile(SourceDir + Rec.Name, DestDir + Rec.Name, false)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {SourceDir + Rec.Name, DestDir + Rec.Name}), mtError, mbOK, 0);
                FindClose(Rec);
                return result;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    return true;
}

bool TTransferForm::CopyFiles(const QString& RelDir, const QString& SourceBaseDir, const QString& DestBaseDir, const QString& GameName)
{
    QString S, T;
    S = IncludeTrailingPathDelimiter(MakeRelPath(RelDir, SourceBaseDir));
    if (PathNotRelative(S)) {
        MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {RelDir, GameName, S}), mtError, mbOK, 0);
        return false;
    }
    S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(RelDir), SourceBaseDir));
    T = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(RelDir), DestBaseDir));
    return CopyDir(S, T);
}

bool TTransferForm::CopyFiles(const QString& RelDir1, const QString& RelDir2, const QString& SourceBaseDir, const QString& DestBaseDir, const QString& GameName)
{
    QString S, T;
    S = IncludeTrailingPathDelimiter(MakeRelPath(RelDir1, SourceBaseDir));
    if (PathNotRelative(S)) {
        MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {RelDir1, GameName, S}), mtError, mbOK, 0);
        return false;
    }
    S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(RelDir1), SourceBaseDir));
    T = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(RelDir2), DestBaseDir));
    return CopyDir(S, T);
}

bool TTransferForm::CopySingleFile(const QString& RelFile, const QString& SourceBaseDir, const QString& DestBaseDir, const QString& GameName)
{
    QString S, T;

    S = IncludeTrailingPathDelimiter(MakeRelPath(RelFile, SourceBaseDir));
    if (PathNotRelative(S)) {
        MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {RelFile, GameName, S}), mtError, mbOK, 0);
        return false;
    }

    S = MakeAbsPath(RelFile, SourceBaseDir);
    T = MakeAbsPath(RelFile, DestBaseDir);

    if (!ForceDirectories(ExtractFilePath(T))) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {T}), mtError, mbOK, 0);
        return false;
    }

    if (!CopyFile(S, T, false)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {S, T}), mtError, mbOK, 0);
        return false;
    }

    return true;
}

bool TTransferForm::TransferGame(TGame* Game, const QString& DestDataDir)
{
    QString S, T, DestProfFile;
    int I;
    TStringList *St, *Files, *Folders;
    TGame* TempGame;

    Game->StoreAllValues();

    DestProfFile = DestDataDir + GameListSubDir + PathDelim + ExtractFileName(Game->SetupFile());
    if (FileExists(DestProfFile)) {
        I = 0;
        do { I++; } while (FileExists(ChangeFileExt(DestProfFile, IntToStr(I) + ".prof")));
        DestProfFile = ChangeFileExt(DestProfFile, IntToStr(I) + ".prof");
    }
    if (!CopyFile(Game->SetupFile(), DestProfFile, true)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {Game->SetupFile(), DestProfFile}), mtError, mbOK, 0);
        return false;
    }

    if (ScummVMMode(Game)) {
        S = Trim(Game->ScummVMPath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(S);
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
        S = Trim(Game->ScummVMZip());
        if (S != "") {
            if (!CopySingleFile(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
        S = Trim(Game->ScummVMSavePath());
        if (S != "") {
            S = IncludeTrailingPathDelimiter(S);
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
    } else {
        S = Trim(Game->GameExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S == "") S = Trim(Game->SetupExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S != "") {
            S = IncludeTrailingPathDelimiter(ExtractFilePath(S));
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
    }

    S = Trim(Game->CaptureFolder());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(S);
        if ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(DestDataDir + S)) {
            I = 0;
            do { I++; } while (DirectoryExists(DestDataDir + IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(S) + IntToStr(I))));
            T = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(S) + IntToStr(I));
            TempGame = new TGame(DestProfFile); TempGame->SetCaptureFolder(T); TempGame->StoreAllValues(); delete TempGame;
            if (!CopyFiles(S, T, PrgDataDir(), DestDataDir, Game->Name())) return false;
        } else {
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
    }

    S = MakeAbsIconName(Game->Icon());
    if ((S != "") && FileExists(S)) {
        if (ExtractFilePath(Game->Icon()) == "") {
            S = Trim(Game->Icon());
            if (!CopyFile(PrgDataDir() + IconsSubDir + PathDelim + S, DestDataDir + IconsSubDir + PathDelim + S, false)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {PrgDataDir() + IconsSubDir + PathDelim + S, DestDataDir + IconsSubDir + PathDelim + S}), mtError, mbOK, 0);
                return false;
            }
        } else {
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (PathNotRelative(S)) {
                MessageDlg(Format(LanguageSetup->MessagePathNotRelative(), {S, Game->Name(), PrgSetup->BaseDir()}), mtError, mbOK, 0);
                return false;
            }
            T = ExtractFilePath(S);
            T = IncludeTrailingPathDelimiter(MakeAbsPath(T, DestDataDir)) + ExtractFileName(S);
            S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (!CopyFile(S, T, false)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {S, T}), mtError, mbOK, 0);
                return false;
            }
        }
    }

    S = Trim(Game->DataDir());
    if (S != "") {
        S = IncludeTrailingPathDelimiter(S);
        if ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(DestDataDir + S)) {
            I = 0;
            do { I++; } while (DirectoryExists(DestDataDir + IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(S) + IntToStr(I))));
            T = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(S) + IntToStr(I));
            TempGame = new TGame(DestProfFile); TempGame->SetDataDir(T); TempGame->StoreAllValues(); delete TempGame;
            if (!CopyFiles(S, T, PrgDataDir(), DestDataDir, Game->Name())) return false;
        } else {
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
    }

    S = Trim(Game->ExtraFiles());
    if (S != "") {
        St = ValueToList(S);
        std::unique_ptr<TStringList> StGuard(St);
        for (I = 0; I < St->Count(); I++) {
            if (Trim((*St)[I]) == "") continue;
            S = (*St)[I];
            if (!ForceDirectories(ExtractFilePath(MakeAbsPath(S, DestDataDir)))) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {ExtractFilePath(MakeAbsPath(S, DestDataDir))}), mtError, mbOK, 0);
                return false;
            }
            if (!CopyFile(MakeAbsPath(S, PrgDataDir()), MakeAbsPath(S, DestDataDir), false)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {MakeAbsPath(S, PrgDataDir()), MakeAbsPath(S, DestDataDir)}), mtError, mbOK, 0);
                return false;
            }
        }
    }

    S = Trim(Game->ExtraDirs());
    if (S != "") {
        St = ValueToList(S);
        std::unique_ptr<TStringList> StGuard(St);
        for (I = 0; I < St->Count(); I++) {
            if (Trim((*St)[I]) == "") continue;
            S = IncludeTrailingPathDelimiter((*St)[I]);
            if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
        }
    }

    FileAndFoldersFromDrives(Game, Files, Folders);
    std::unique_ptr<TStringList> FilesGuard(Files), FoldersGuard(Folders);
    for (I = 0; I < Files->Count(); I++) {
        S = MakeRelPath((*Files)[I], PrgSetup->BaseDir());
        if (!ForceDirectories(ExtractFilePath(MakeAbsPath(S, DestDataDir)))) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {ExtractFilePath(MakeAbsPath(S, DestDataDir))}), mtError, mbOK, 0);
            return false;
        }
        if (!CopyFile(MakeAbsPath(S, PrgDataDir()), MakeAbsPath(S, DestDataDir), false)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {MakeAbsPath(S, PrgDataDir()), MakeAbsPath(S, DestDataDir)}), mtError, mbOK, 0);
            return false;
        }
    }
    for (I = 0; I < Folders->Count(); I++) {
        S = (*Folders)[I];
        if (!CopyFiles(S, PrgDataDir(), DestDataDir, Game->Name())) return false;
    }

    return true;
}

void TTransferForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TOperationMode OpMode;
    QString DestPrgDataDir;
    int I, J;

    if (ui.CopyDFRCheckBox->isChecked()) {
        if (!CopyInstallation(IncludeTrailingPathDelimiter(ui.DestPrgDirEdit->text()))) { setModalResult(mrNone); return; }
    }

    if (!GetOperationMode(OpMode)) { setModalResult(mrNone); return; }
    if (!GetDestPrgDataDir(OpMode, DestPrgDataDir)) { setModalResult(mrNone); return; }

    J = 0; for (I = 0; I < ui.ListBox->count(); I++) if (CheckListChecked(ui.ListBox, I)) J++;
    InitProgressWindow(this, J);
    struct ProgressGuard { ~ProgressGuard() { DoneProgressWindow(); } } Guard;  // try/finally
    for (I = 0; I < ui.ListBox->count(); I++) {
        if (!CheckListChecked(ui.ListBox, I)) continue;
        StepProgressWindow();
        if (!TransferGame(CheckListGame(ui.ListBox, I), DestPrgDataDir)) { setModalResult(mrNone); return; }
    }
}

void TTransferForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasTransferProfiles);
}

void TTransferForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

bool TransferGames(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    TransferForm = new TTransferForm(AOwner);
    TransferForm->GameDB = AGameDB;
    result = (TransferForm->ShowModal() == mrOk);
    delete TransferForm;
    TransferForm = nullptr;
    return result;
}
