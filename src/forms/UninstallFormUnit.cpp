// C++ port of UninstallFormUnit.pas (implementation section).
#include "UninstallFormUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TUninstallForm* UninstallForm = nullptr;

// TCheckListBox helpers
static void CheckListAdd(QListWidget* L, const QString& S)
{
    QListWidgetItem* It = new QListWidgetItem(S, L);
    It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
    It->setCheckState(Qt::Unchecked);
}
static bool CheckListChecked(QListWidget* L, int I)
{
    return L->item(I)->checkState() == Qt::Checked;
}
static void CheckListSetChecked(QListWidget* L, int I, bool B)
{
    L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked);
}

TUninstallForm::TUninstallForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.DeleteRadioButton, &QRadioButton::clicked, this, [this]() { RadioButtonWork(ui.DeleteRadioButton); });
    connect(ui.UninstallRadioButton, &QRadioButton::clicked, this, [this]() { RadioButtonWork(ui.UninstallRadioButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TUninstallForm::~TUninstallForm()
{
    FormDestroy(this);
}

void TUninstallForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TUninstallForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->UninstallForm());
    ui.DeleteRadioButton->setText(LanguageSetup->UninstallFormDelete());
    ui.UninstallRadioButton->setText(LanguageSetup->UninstallFormUninstall());
    ui.InfoLabel->setText(LanguageSetup->UninstallFormLabel());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    DirList = nullptr;
}

void TUninstallForm::FormShow(QObject* Sender)
{
    QString S, Dir;
    TStringList *St, *Files, *Folders;
    int I;

    DirList = new TStringList;

    CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormProfileRecord() + ": \"" + Game->Name() + "\" (" + Game->SetupFile() + ")");
    DirList->Add("");

    if (ScummVMMode(Game)) {
        S = Trim(Game->ScummVMPath());
        if ((S != "") && (!UsedByOtherGame(GameDB, Game, IncludeTrailingPathDelimiter(S)))) {
            S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
            CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormProgramDir() + ": " + S);
            DirList->Add(S);
        }
        S = Trim(Game->ScummVMZip());
        if ((S != "") && (!ExtraFileUsedByOtherGame(GameDB, Game, S))) {
            S = MakeAbsPath(S, PrgSetup->BaseDir());
            CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraFile() + ": " + S);
            DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
        }
        S = Trim(Game->ScummVMSavePath());
        if ((S != "") && (!UsedByOtherGame(GameDB, Game, IncludeTrailingPathDelimiter(S)))) {
            S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
            CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormSaveDir() + ": " + S);
            DirList->Add(S);
        }
    } else {
        S = Trim(Game->GameExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        if (S == "") S = Trim(Game->SetupExe());
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") S = "";
        S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
        if (S != "") {
            if (!UsedByOtherGame(GameDB, Game, S)) {
                CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormProgramDir() + ": " + S);
                DirList->Add(S);
            } else {
                /* if folder is not removeable offer to remove files at least */
                S = Trim(Game->GameExe());
                if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
                    Dir = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
                    Dir = Trim(ExtUpperCase(ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(Dir), PrgSetup->BaseDir()))));
                    if (Dir == Trim(ExtUpperCase(MakeAbsPath(IncludeTrailingPathDelimiter(PrgSetup->GameDir()), PrgSetup->BaseDir())))) {
                        S = MakeAbsPath(S, PrgSetup->BaseDir());
                        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraFile() + ": " + S);
                        DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
                    }
                }
                S = Trim(Game->SetupExe());
                if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
                    Dir = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
                    Dir = Trim(ExtUpperCase(ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(Dir), PrgSetup->BaseDir()))));
                    if (Dir == Trim(ExtUpperCase(MakeAbsPath(IncludeTrailingPathDelimiter(PrgSetup->GameDir()), PrgSetup->BaseDir())))) {
                        S = MakeAbsPath(S, PrgSetup->BaseDir());
                        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraFile() + ": " + S);
                        DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
                    }
                }
            }
        }
    }

    S = Trim(Game->CaptureFolder());
    if ((S != "") && (!UsedByOtherGame(GameDB, Game, IncludeTrailingPathDelimiter(S)))) {
        S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormCaptureFolder() + ": " + S);
        DirList->Add(S);
    }

    S = MakeAbsIconName(Game->Icon());
    if ((S != "") && FileExists(S) && (!IconUsedByOtherGame(GameDB, Game, S))) {
        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormIcon() + ": " + S);
        DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
    }

    S = Trim(Game->DataDir());
    if ((S != "") && (!UsedByOtherGame(GameDB, Game, IncludeTrailingPathDelimiter(S)))) {
        S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormDataDir() + ": " + S);
        DirList->Add(S);
    }

    S = Trim(Game->ExtraFiles());
    if (S != "") {
        St = ValueToList(S);
        for (I = 0; I < St->Count(); I++) {
            if ((Trim((*St)[I]) != "") && (!ExtraFileUsedByOtherGame(GameDB, Game, (*St)[I]))) {
                S = MakeAbsPath((*St)[I], PrgSetup->BaseDir());
                CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraFile() + ": " + S);
                DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
            }
        }
        delete St;
    }

    S = Trim(Game->ExtraDirs());
    if (S != "") {
        St = ValueToList(S);
        for (I = 0; I < St->Count(); I++) {
            if ((Trim((*St)[I]) != "") && (!UsedByOtherGame(GameDB, Game, IncludeTrailingPathDelimiter((*St)[I])))) {
                S = ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[I]), PrgSetup->BaseDir()));
                CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraDir() + ": " + S);
                DirList->Add(S);
            }
        }
        delete St;
    }

    UnusedFileAndFoldersFromDrives(GameDB, Game, Files, Folders);
    for (I = 0; I < Files->Count(); I++) {
        S = (*Files)[I];
        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraFile() + ": " + S);
        DirList->AddObject(S, (intptr_t)1); /* TObject(1) = is file not folder */
    }
    for (I = 0; I < Folders->Count(); I++) {
        S = (*Folders)[I];
        CheckListAdd(ui.ListBox, LanguageSetup->UninstallFormExtraDir() + ": " + S);
        DirList->Add(S);
    }
    delete Files;
    delete Folders;

    for (I = 0; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, true);

    ui.DeleteRadioButton->setChecked(!PrgSetup->DefaultUninstall());
    ui.UninstallRadioButton->setChecked(PrgSetup->DefaultUninstall());
    RadioButtonWork(Sender);
}

void TUninstallForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    bool ContinueNext = false;

    if (ui.DeleteRadioButton->isChecked()) {
        GameDB->Delete(Game);
    } else {
        SetCurrentDir(PrgDataDir());

        for (I = 1; I < ui.ListBox->count(); I++) {
            if (CheckListChecked(ui.ListBox, I)) {
                if (DirList->ObjectAsInt(I) != 0) {
                    if (BaseDirSecuriryCheck(ExtractFilePath((*DirList)[I]))) {
                        if ((!ExtDeleteFile((*DirList)[I], ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                } else {
                    if (BaseDirSecuriryCheck((*DirList)[I])) {
                        if ((!ExtDeleteFolder((*DirList)[I], ftUninstall, true, ContinueNext)) && (!ContinueNext)) return;
                    }
                }
            }
        }

        if (CheckListChecked(ui.ListBox, 0)) GameDB->Delete(Game);
    }
}

void TUninstallForm::RadioButtonWork(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (ui.ListBox->isEnabled() != ui.UninstallRadioButton->isChecked()) {
        if (ui.UninstallRadioButton->isChecked()) {
            for (I = 0; I < CheckList.size(); I++) CheckListSetChecked(ui.ListBox, I, CheckList[I]);
        } else {
            CheckList.resize(ui.ListBox->count());
            for (I = 0; I < CheckList.size(); I++) CheckList[I] = CheckListChecked(ui.ListBox, I);
            CheckListSetChecked(ui.ListBox, 0, true);
            for (I = 1; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, false);
        }
    }

    ui.ListBox->setEnabled(ui.UninstallRadioButton->isChecked());
    ui.InfoLabel->setEnabled(ui.UninstallRadioButton->isChecked());
    ui.SelectAllButton->setEnabled(ui.UninstallRadioButton->isChecked());
    ui.SelectNoneButton->setEnabled(ui.UninstallRadioButton->isChecked());
}

void TUninstallForm::SelectButtonClick(QObject* Sender)
{
    int I;
    for (I = 0; I < ui.ListBox->count(); I++) CheckListSetChecked(ui.ListBox, I, Sender->property("tag").toInt() == 0);
}

void TUninstallForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete DirList;
    DirList = nullptr;
    PrgSetup->SetDefaultUninstall(ui.UninstallRadioButton->isChecked());
}

void TUninstallForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileUninstall);
}

void TUninstallForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

bool UninstallGame(QWidget* AOwner, TGameDB* AGameDB, TGame* AGame)
{
    bool result;
    if (WindowsExeMode(AGame)) return false;

    UninstallForm = new TUninstallForm(AOwner);
    UninstallForm->GameDB = AGameDB;
    UninstallForm->Game = AGame;
    result = (UninstallForm->ShowModal() == mrOk);
    delete UninstallForm;
    UninstallForm = nullptr;
    return result;
}

bool UsedByOtherGame(TGameDB* GameDB, TGame* Game, const QString& ADir)
{
    QString Dir = ADir;

    auto SameDir = [&Dir](QString S) -> bool {
        if (Trim(S) == "") return false;
        if (ExtUpperCase(Copy(S, 1, 7)) == "DOSBOX:") return false;
        S = Trim(ExtUpperCase(ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()))));
        return (Copy(S, 1, Length(Dir)) == Dir);
    };

    int I, J;
    TStringList *St, *Files, *Folders;

    Dir = Trim(ExtUpperCase(ExtractFilePath(MakeAbsPath(IncludeTrailingPathDelimiter(Dir), PrgSetup->BaseDir()))));

    if (Dir == Trim(ExtUpperCase(MakeAbsPath(IncludeTrailingPathDelimiter(PrgSetup->GameDir()), PrgSetup->BaseDir())))) return true;

    if (Trim(PrgSetup->PathToFREEDOS()) != "") {
        if (Dir == Trim(ExtUpperCase(MakeAbsPath(IncludeTrailingPathDelimiter(PrgSetup->PathToFREEDOS()), PrgSetup->BaseDir())))) return true;
    }

    for (I = 0; I < GameDB->Count(); I++) {
        TGame* G = GameDB->Game(I);
        if (G == Game) continue;

        if (ScummVMMode(G)) {
            if (SameDir(G->ScummVMPath())) return true;
            if (SameDir(G->CaptureFolder())) return true;
        } else {
            if (SameDir(G->GameExe())) return true;
            if (SameDir(G->SetupExe())) return true;
            if (SameDir(G->CaptureFolder())) return true;

            FileAndFoldersFromDrives(G, Files, Folders);
            bool Found = false;
            for (J = 0; J < Folders->Count(); J++) if (SameDir((*Folders)[J])) { Found = true; break; }
            delete Files;
            delete Folders;
            if (Found) return true;
        }
        if (SameDir(G->DataDir())) return true;

        St = ValueToList(G->ExtraDirs());
        bool Found = false;
        for (J = 0; J < St->Count(); J++) if ((Trim((*St)[J]) != "") && SameDir((*St)[J])) { Found = true; break; }
        delete St;
        if (Found) return true;
    }

    return false;
}

bool IconUsedByOtherGame(TGameDB* GameDB, TGame* Game, const QString& Icon)
{
    int I;
    QString S;

    S = Trim(ExtUpperCase(Icon));
    for (I = 0; I < GameDB->Count(); I++) {
        if (GameDB->Game(I) != Game) {
            if (Trim(ExtUpperCase(MakeAbsIconName(GameDB->Game(I)->Icon()))) == S) return true;
        }
    }
    return false;
}

bool ExtraFileUsedByOtherGame(TGameDB* GameDB, TGame* Game, const QString& AExtraFile)
{
    QString ExtraFile = AExtraFile;

    auto SameFile = [&ExtraFile](const QString& S) -> bool {
        return (Trim(S) != "") && (Trim(ExtUpperCase(MakeAbsPath(S, PrgSetup->BaseDir()))) == ExtraFile);
    };

    int I, J;
    TStringList *St, *Files, *Folders;
    bool Found;

    ExtraFile = Trim(ExtUpperCase(MakeAbsPath(ExtraFile, PrgSetup->BaseDir())));
    for (I = 0; I < GameDB->Count(); I++) {
        TGame* G = GameDB->Game(I);
        if (G == Game) continue;

        St = ValueToList(G->ExtraFiles());
        Found = false;
        for (J = 0; J < St->Count(); J++) if (SameFile((*St)[J])) { Found = true; break; }
        delete St;
        if (Found) return true;

        if ((!ScummVMMode(G)) && (!WindowsExeMode(G))) {
            FileAndFoldersFromDrives(G, Files, Folders);
            Found = false;
            for (J = 0; J < Files->Count(); J++) if (SameFile((*Files)[J])) { Found = true; break; }
            delete Files;
            delete Folders;
            if (Found) return true;
        }

        if (ScummVMMode(G) && SameFile(G->ScummVMZip())) return true;
    }

    return false;
}

static bool FolderInNormalList(TGame* Game, const QString& FolderName)
{
    QString S, T;
    TStringList* St;
    int I;

    T = ExtUpperCase(IncludeTrailingPathDelimiter(FolderName));

    S = Trim(Game->GameExe());
    if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
        S = ExtUpperCase(IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()))));
        if (S == Copy(T, 1, Length(S))) return true;
    }

    S = Trim(Game->SetupExe());
    if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
        S = ExtUpperCase(IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()))));
        if (S == Copy(T, 1, Length(S))) return true;
    }

    S = Trim(Game->ExtraDirs());
    if (S == "") return false;
    St = ValueToList(S);
    for (I = 0; I < St->Count(); I++) {
        S = ExtUpperCase(IncludeTrailingPathDelimiter(MakeAbsPath((*St)[I], PrgSetup->BaseDir())));
        if (S == Copy(T, 1, Length(S))) { delete St; return true; }
    }
    delete St;
    return false;
}

static bool FileInNormalList(TGame* Game, const QString& FileName)
{
    QString S;
    TStringList* St;
    int I;

    if (FolderInNormalList(Game, ExtractFilePath(FileName))) return true;

    S = Trim(Game->ExtraFiles());
    if (S == "") return false;
    St = ValueToList(S);
    S = ExtUpperCase(FileName);
    for (I = 0; I < St->Count(); I++) {
        if (ExtUpperCase(MakeAbsPath((*St)[I], PrgSetup->BaseDir())) == S) { delete St; return true; }
    }
    delete St;
    return false;
}

void FileAndFoldersFromDrives(TGame* Game, TStringList*& Files, TStringList*& Folders)
{
    int I, J;
    QString S, T, U;
    TStringList *St, *St2;

    Files = new TStringList;
    Folders = new TStringList;

    for (I = 0; I <= 9; I++) {
        S = Game->Mount(I);
        if (S == "") continue;

        St = ValueToList(S);
        St2 = new TStringList;
        if (St->Count() < 2) { delete St; delete St2; continue; }

        S = Trim((*St)[0]); J = Pos('$', S);
        while (J > 0) { St2->Add(Trim(Copy(S, 1, J - 1))); S = Trim(Copy(S, J + 1, MaxInt)); J = Pos('$', S); }
        St2->Add(S);

        S = Trim(ExtUpperCase((*St)[1]));

        if ((S == "CDROMIMAGE") || (S == "FLOPPYIMAGE") || (S == "IMAGE")) {
            /* Image files */
            for (J = 0; J < St2->Count(); J++) {
                U = (*St2)[J]; if ((U != "") && (U[0] == '\\')) U = Copy(U, 2, MaxInt);
                T = MakeAbsPath(U, PrgSetup->BaseDir());
                if (FileExists(T) && (!FileInNormalList(Game, T))) {
                    Files->Add(T);
                    if (ExtUpperCase(ExtractFileExt(T)) == ".CUE") {
                        U = ChangeFileExt(T, ".bin"); if (FileExists(U) && (!FileInNormalList(Game, U))) Files->Add(U);
                    }
                    if (ExtUpperCase(ExtractFileExt(T)) == ".BIN") {
                        U = ChangeFileExt(T, ".cue"); if (FileExists(U) && (!FileInNormalList(Game, U))) Files->Add(U);
                    }
                }
            }
        }

        if ((S == "PHYSFS") && (St2->Count() == 2)) {
            /* RealFolder$ZipFile in St2 */
            T = MakeAbsPath(IncludeTrailingPathDelimiter((*St2)[0]), PrgSetup->BaseDir());
            if (DirectoryExists(T) && (!FolderInNormalList(Game, T))) Folders->Add(T);
            T = MakeAbsPath((*St2)[1], PrgSetup->BaseDir());
            if (FileExists(T) && (!FileInNormalList(Game, T))) Files->Add(T);
        }

        if ((S == "ZIP") && (St2->Count() == 2)) {
            /* RealFolder$ZipFile in St2, only add ZipFile */
            T = MakeAbsPath((*St2)[1], PrgSetup->BaseDir());
            if (FileExists(T) && (!FileInNormalList(Game, T))) Files->Add(T);
        }

        delete St;
        delete St2;
    }
}

void UnusedFileAndFoldersFromDrives(TGameDB* GameDB, TGame* Game, TStringList*& Files, TStringList*& Folders)
{
    int I;
    FileAndFoldersFromDrives(Game, Files, Folders);

    I = 0;
    while (I < Files->Count()) {
        if (ExtraFileUsedByOtherGame(GameDB, Game, (*Files)[I])) { Files->Delete(I); continue; }
        I++;
    }

    I = 0;
    while (I < Folders->Count()) {
        if (UsedByOtherGame(GameDB, Game, (*Folders)[I])) { Folders->Delete(I); continue; }
        I++;
    }
}
