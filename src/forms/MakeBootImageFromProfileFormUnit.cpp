// C++ port of MakeBootImageFromProfileFormUnit.pas (implementation section).
#include "MakeBootImageFromProfileFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "PrgSetupUnit.h"
#include "ImageTools.h"
#include "CreateImageToolsUnit.h"
#include "PrgConsts.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "GameDBToolsUnit.h"

TMakeBootImageFromProfileForm* MakeBootImageFromProfileForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, ID)
static void HelpCommandContext(int ID)
{
    // TODO(port): no public help context entry point in HelpTools yet; open the help contents.
    Q_UNUSED(ID);
    if (GLOBAL_HELPROUTER != nullptr) GLOBAL_HELPROUTER->HelpContent();
}

// Work around: TGame::SetExtraPrgFile (property ExtraPrgFile[I] write) is
// declared private in GameDBUnit.h; this reproduces TGame.SetExtraPrgFile.
namespace {
struct TGameAccess : public TGame {
    static void SetExtraPrgFileOf(TGame* G, int I, const QString& S) {
        if ((I < 0) || (I > 49)) return;
        (G->*(&TGameAccess::SetString))(NR_ExtraPrgFile + I, S);
    }
};
}

TMakeBootImageFromProfileForm::TMakeBootImageFromProfileForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TMakeBootImageFromProfileForm::~TMakeBootImageFromProfileForm()
{
}

void TMakeBootImageFromProfileForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TMakeBootImageFromProfileForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    NewGame = nullptr;

    // SetVistaFonts / Font.Charset: not needed in Qt

    setWindowTitle(LanguageSetup->ImageFromProfile());
    ui.InfoLabel->setText(LanguageSetup->ImageFromProfileInfoLabel());
    ui.FreeSizeLabel->setText(LanguageSetup->ImageFromProfileFreeSize());
    ui.FreeSizeMBLabel->setText(LanguageSetup->MBytes());
    ui.CompressedCheckBox->setText(LanguageSetup->CreateImageFormCompression());
    ui.MemoryManagerCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithMemoryManager());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TMakeBootImageFromProfileForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    int Nr;
    TGame* G;

    Nr = -1;
    ui.ProfileComboBox->setUpdatesEnabled(false);
    for (int I = 0; I < GameDB->Count(); I++) {
        G = GameDB->Game(I);
        if (ScummVMMode(G) || WindowsExeMode(G)) continue;
        if ((Trim(G->AutoexecBootImage()) != "") || G->AutoexecOverridegamestart() || G->AutoexecOverrideMount() || (Trim(G->GameExe()) == "")) continue;
        ui.ProfileComboBox->addItem(G->CacheName, QVariant::fromValue<void*>(G));
        if (G == SelectGame) Nr = ui.ProfileComboBox->count() - 1;
    }
    ui.ProfileComboBox->setUpdatesEnabled(true);
    if (Nr >= 0) ui.ProfileComboBox->setCurrentIndex(Nr); else {
        if (ui.ProfileComboBox->count() > 0) ui.ProfileComboBox->setCurrentIndex(0);
    }
}

void TMakeBootImageFromProfileForm::CalcDestDirs(TStringList* SourceDirs, TStringList* DestDirs)
{
    int J;
    QString S, T;

    for (int I = 0; I < SourceDirs->Count(); I++) {
        S = ""; T = (*SourceDirs)[I]; if ((T != "") && (T[Length(T) - 1] == PathDelimChar)) T.chop(1);
        J = Pos(PathDelimChar, T); while (J > 0) { T = Copy(T, J + 1, MaxInt); J = Pos(PathDelimChar, T); }
        for (J = 1; J <= Length(T); J++) {
            if (T[J - 1] != ' ') S = S + T[J - 1];
            if (Length(S) >= 8) break;
        }
        T = ExtUpperCase(S);
        J = 1; S = T;
        while (DestDirs->IndexOf(S) >= 0) {
            S = "~" + IntToStr(J);
            S = Copy(T, 1, 8 - Length(S)) + S;
            J++;
        }
        DestDirs->Add(S);
    }
}

bool TMakeBootImageFromProfileForm::MakeImageFromFolderAndIncludeAutoexec(TStringList* SourceDirs, TStringList* DestDirs, const QString& ImageFile, TGame* Game, const QString& NewGameDir)
{
    const TDiskImageCreatorUpgrades UpgradesWithoutMM = {dicuMakeBootable, dicuKeyboardDriver, dicuMouseDriver, dicuEditor, dicuAutoexecAddLines, dicuCopyFolder};
    const TDiskImageCreatorUpgrades UpgradesWithMM = {dicuMakeBootable, dicuKeyboardDriver, dicuMouseDriver, dicuMemoryManager, dicuEditor, dicuAutoexecAddLines, dicuCopyFolder};
    int ImageSize;
    TStringList *St, *St2;
    QString S, T;
    int I;
    TDiskImageCreatorUpgrades Upgrades;
    bool result = false;

    if (ui.MemoryManagerCheckBox->isChecked()) Upgrades = UpgradesWithMM; else Upgrades = UpgradesWithoutMM;

    ImageSize = DiskImageCreatorUpgradeSize(true, Upgrades, SourceDirs, ui.FreeSizeEdit->value());
    if (!DiskImageCreator(ImageSize / 1024 / 1024 + 1, ImageFile, ui.CompressedCheckBox->isChecked(), true)) return result;

    St = new TStringList();
    St2 = StringToStringList(Game->Autoexec()); St->AddStrings(*St2); delete St2;
    S = Trim(NewGameDir);
    while (S != "") {
        I = Pos('\\', S);
        if (I > 0) { T = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); } else { T = S; S = ""; }
        if (T != "") St->Add("cd " + T);
    }
    St->Add(ExtractFileName(Game->GameExe()));
    St2 = StringToStringList(Game->AutoexecFinalization()); St->AddStrings(*St2); delete St2;

    if (Game->SwapMouseButtons()) Upgrades.insert(dicuSwapMouseButtons);
    if (Game->Force2ButtonMouseMode()) Upgrades.insert(dicuForce2ButtonMouseMode);

    if (!DiskImageCreatorUpgrade(ImageFile, true, Upgrades, SourceDirs, DestDirs, St)) { delete St; return result; }
    delete St;

    result = true;
    return result;
}

static bool IsDirectSubDir(const QString& ShortParent, const QString& LongTestDir)
{
    QString ShortTestDir, S;
    bool result = false;
    ShortTestDir = ShortName(LongTestDir);
    if (ExtUpperCase(Copy(ShortTestDir, 1, Length(ShortParent))) != ExtUpperCase(ShortParent)) return result;
    S = Copy(ShortTestDir, Length(ShortParent) + 1, MaxInt);
    if ((S != "") && (S[0] == PathDelimChar)) S = Copy(S, 2, MaxInt);
    if ((S != "") && (S[Length(S) - 1] == PathDelimChar)) S.chop(1);
    S = Trim(S);
    if (S == "") return result;
    if (Pos(PathDelimChar, S) > 0) return result;
    result = true;
    return result;
}

bool TMakeBootImageFromProfileForm::GetFoldersToCopy(TGame* Game, TStringList* SourceDirs, TStringList* DestDirs)
{
    QString GameDir, ShortGamesDir, S;
    TStringList* ExtraDirs;
    int I;
    bool result = false;

    GameDir = IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir())));
    ShortGamesDir = ShortName(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
    ExtraDirs = ValueToList(Game->ExtraDirs());

    I = 0; while (I < ExtraDirs->Count() - 1) if (Trim((*ExtraDirs)[I]) == "") ExtraDirs->Delete(I); else I++;

    if (!IsDirectSubDir(ShortGamesDir, GameDir)) {
        // GameDir outside games directory or not a direct subdirectory of the games directory
        if (ExtraDirs->Count() > 0) {
            if (MessageDlg(Format(LanguageSetup->ImageFromProfileConfirmationGameDirectoryNotInGamesDirectory(), {GameDir, MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())}), mtWarning, mbYes | mbNo, 0) != mrYes) { delete ExtraDirs; return result; }
        }
        SourceDirs->Add(GameDir);
        CalcDestDirs(SourceDirs, DestDirs);
    } else {
        // GameDir is a direct subdirectory of the games directory
        SourceDirs->Add(GameDir);
        for (I = 0; I < ExtraDirs->Count(); I++) {
            S = MakeAbsPath((*ExtraDirs)[I], PrgSetup->BaseDir());
            if (!IsDirectSubDir(ShortGamesDir, S)) {
                if (MessageDlg(Format(LanguageSetup->ImageFromProfileConfirmationExtraDirectoryNotInGamesDirectory(), {(*ExtraDirs)[I], MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())}), mtWarning, mbYes | mbNo, 0) != mrYes) { delete ExtraDirs; return result; }
            } else {
                SourceDirs->Add(S);
            }
        }
        CalcDestDirs(SourceDirs, DestDirs);
    }
    result = true;
    delete ExtraDirs;
    return result;
}

void TMakeBootImageFromProfileForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TGame* Game;
    QString GameDir, NewGameDir, ImageFileName, NewGameName, S;
    int I;

    if (ui.ProfileComboBox->currentIndex() < 0) return;
    Game = static_cast<TGame*>(ui.ProfileComboBox->itemData(ui.ProfileComboBox->currentIndex()).value<void*>());

    {
        TStringList SourceDirs, DestDirs;

        // Collect folders to copy
        if (!GetFoldersToCopy(Game, &SourceDirs, &DestDirs)) return;

        // Prepare directory for new profile
        GameDir = IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir())));
        NewGameDir = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(GameDir) + "-Boot");
        I = 0; while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(NewGameDir)) {
            I++;
            NewGameDir = IncludeTrailingPathDelimiter(ExcludeTrailingPathDelimiter(GameDir) + "-Boot" + IntToStr(I));
        }
        ForceDirectories(NewGameDir);

        // Create Image
        ImageFileName = NewGameDir + MakeFileSysOKFolderName(Game->CacheName) + "-Harddisk.img";
        if (!MakeImageFromFolderAndIncludeAutoexec(&SourceDirs, &DestDirs, ImageFileName, Game, DestDirs[0])) { setModalResult(mrNone); return; }
    }

    // Setup new profile
    NewGameName = Game->CacheName + " (" + LanguageSetup->ImageFromProfileProfileNameAdd() + ")";
    I = GameDB->Add(NewGameName);
    NewGame = GameDB->Game(I);
    NewGame->AssignFrom(Game);
    NewGame->SetName(NewGameName);
    NewGame->SetGameExe("");
    NewGame->SetGameExeMD5("");
    NewGame->SetSetupExe("");
    NewGame->SetSetupExeMD5("");
    for (I = 0; I <= 9; I++) TGameAccess::SetExtraPrgFileOf(NewGame, I, "");
    NewGame->SetAutoexec("");
    NewGame->SetAutoexecFinalization("");
    NewGame->SetDataDir("");
    NewGame->SetNrOfMounts(1);
    NewGame->SetMount0(MakeRelPath(ImageFileName, PrgSetup->BaseDir()) + ";IMAGE;2;;;" + GetGeometryFromFile(ImageFileName));
    NewGame->SetAutoexecBootImage("2");

    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(NewGame->Name()) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(NewGame->Name()) + IntToStr(I) + PathDelim;
    }
    NewGame->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));

    {
        TStringList St;
        St.Add(MakeRelPath(NewGameDir, PrgSetup->BaseDir()));
        NewGame->SetExtraDirs(ListToValue(&St));
    }

    NewGame->StoreAllValues();
    NewGame->LoadCache();
}

void TMakeBootImageFromProfileForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ExtrasImageFromProfile);
}

void TMakeBootImageFromProfileForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

TGame* ShowMakeBootImageFromProfileDialog(QWidget* AOwner, TGameDB* AGameDB, TGame* ASelectGame)
{
    TGame* result = nullptr;

    if (!DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        MessageDlg(LanguageSetup->ProfileEditorNeedFreeDOS(), mtError, mbOK, 0);
        return result;
    }
    if ((!FileExists(PrgDir() + MakeDOSFilesystemFileName)) && (!FileExists(PrgDir() + BinFolder + PathDelim + MakeDOSFilesystemFileName))) {
        MessageDlg(Format(LanguageSetup->ImageFromProfileMKDOSFSNeeded(), {PrgDir() + BinFolder + PathDelim + MakeDOSFilesystemFileName}), mtError, mbOK, 0);
        return result;
    }

    MakeBootImageFromProfileForm = new TMakeBootImageFromProfileForm(AOwner);
    MakeBootImageFromProfileForm->GameDB = AGameDB;
    MakeBootImageFromProfileForm->SelectGame = ASelectGame;
    if (MakeBootImageFromProfileForm->ShowModal() == mrOk) result = MakeBootImageFromProfileForm->NewGame;
    delete MakeBootImageFromProfileForm;
    MakeBootImageFromProfileForm = nullptr;
    return result;
}
