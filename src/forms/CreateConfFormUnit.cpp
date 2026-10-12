// C++ port of CreateConfFormUnit.pas (implementation section).
#include "CreateConfFormUnit.h"

#include "LanguageSetupUnit.h"
#include "DosBoxUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "ScummVMUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "TemplateFormUnit.h"
#include "IconLoaderUnit.h"
#include "PrgConsts.h"
#include "HashCalc.h"
#include "Process.h"

TCreateConfForm* CreateConfForm = nullptr;

static TGame* CheckListGame(QListWidget* ListBox, int I)
{
    return static_cast<TGame*>(ListBox->item(I)->data(Qt::UserRole).value<void*>());
}

TCreateConfForm::TCreateConfForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.SelectFolderButton, &QToolButton::clicked, this, [this]() { SelectFolderButtonClick(ui.SelectFolderButton); });
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
    FormCreate(this);  // OnCreate
}

TCreateConfForm::~TCreateConfForm() = default;

void TCreateConfForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TCreateConfForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->CreateConfForm());
    ui.InfoLabel->setText(LanguageSetup->CreateConfFormInfo());
    ui.FolderEdit_EditLabel->setText(LanguageSetup->CreateConfFormSelectFolder());
    ui.SelectFolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    ui.ExportAutoSetupTemplatesCheckBox->setText(LanguageSetup->CreateConfFormAutoSetup());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.SelectFolderButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    ProfFileMode = false;
}

void TCreateConfForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    QAction* M;

    if (ProfFileMode) {
        setWindowTitle(LanguageSetup->CreateConfFormProfMode());
        ui.InfoLabel->setText(LanguageSetup->CreateConfFormInfoProfMode());
        ui.FolderEdit_EditLabel->setText(LanguageSetup->CreateConfFormSelectFolderProfMode());
        ui.ExportAutoSetupTemplatesCheckBox->setVisible(true);
    }

    BuildCheckList(ui.ListBox, GameDB, true, false, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, true, true);

    // PopupMenu.Items[0].Add(...)
    QMenu* Items0 = ui.PopupMenu;
    if (!ui.PopupMenu->actions().isEmpty() && ui.PopupMenu->actions()[0]->menu() != nullptr) Items0 = ui.PopupMenu->actions()[0]->menu();
    Items0->addSeparator();
    M = new QAction(LanguageSetup->CreateConfFormSelectGamesWithoutAutoSetup(), Items0); M->setProperty("tag", -2);
    connect(M, &QAction::triggered, this, [this, M]() { SelectButtonClick(M); });
    Items0->addAction(M);

    ui.FolderEdit->setText(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY));
}

void TCreateConfForm::SelectButtonClick(QObject* Sender)
{
    int I;
    QPoint P;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++) ui.ListBox->item(I)->setCheckState((Sender->property("tag").toInt() == 0) ? Qt::Checked : Qt::Unchecked);
                break;
            case 2:
                P = mapToGlobal(QPoint(ui.SelectGenreButton->x(), ui.SelectGenreButton->y()));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
        }
        return;
    }

    if (qobject_cast<QAction*>(Sender) != nullptr) {
        if (Sender->property("tag").toInt() == -2) {
            SelectGamesWithoutTemplate();
            ui.ExportAutoSetupTemplatesCheckBox->setChecked(true);
        } else {
            SelectGamesByPopupMenu(Sender, ui.ListBox);
        }
    }
}

void TCreateConfForm::SelectFolderButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = ui.FolderEdit->text(); if (S == "") S = PrgDataDir();
    if (SelectDirectory(THandle(winId()), LanguageSetup->SetupFormBaseDir(), S)) ui.FolderEdit->setText(S);
}

bool TCreateConfForm::FindAutoSetupForGame(TGame* Game, TGameDB* AutoSetupDB)
{
    int I, J;
    QString File1, File2, Hash1, Hash2, S;
    TGame* A;
    TSearchRec Rec;

    if (Trim(Game->GameExe()) == "") return false;

    /* Level 1: Check if there is an auto setup template matching with exactly the same files to start */

    /* Calc Hash values */
    File1 = Trim(ExtUpperCase(ExtractFileName(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir()))));
    Hash1 = GetMD5Sum(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir()), false);
    if (Trim(Game->SetupExe()) != "") {
        File2 = Trim(ExtUpperCase(ExtractFileName(MakeAbsPath(Game->SetupExe(), PrgSetup->BaseDir()))));
        Hash2 = GetMD5Sum(MakeAbsPath(Game->SetupExe(), PrgSetup->BaseDir()), false);
    } else {
        File2 = "";
        Hash2 = "";
    }

    /* Check auto setup templates */
    for (I = 0; I < AutoSetupDB->Count(); I++) {
        A = AutoSetupDB->Game(I); if ((A->GameExe() == "") || (A->GameExeMD5() == "")) continue;
        if ((Trim(ExtUpperCase(A->GameExe())) != File1) || (A->GameExeMD5() != Hash1)) continue;
        if ((Trim(A->SetupExe()) != "") && (File2 != "")) {
            if ((Trim(ExtUpperCase(A->SetupExe())) != File2) || (A->SetupExeMD5() != Hash2)) continue;
        }
        return true;
    }

    /* Level 2: Check if there is an auto setup template matching the game but using some other files to start (game.exe -> start.bat for example) */

    /* Calc Hash values */
    TStringList Files, Hashs;
    S = IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir())));
    for (const char* Mask : {"*.exe", "*.com", "*.bat"}) {
        I = FindFirst(S + Mask, faAnyFile, Rec);
        while (I == 0) { Files.Add(ExtUpperCase(Rec.Name)); Hashs.Add(GetMD5Sum(S + Rec.Name, false)); I = FindNext(Rec); }
        FindClose(Rec);
    }

    /* Check auto setup templates */
    for (I = 0; I < AutoSetupDB->Count(); I++) {
        A = AutoSetupDB->Game(I); if ((A->GameExe() == "") || (A->GameExeMD5() == "")) continue;

        J = Files.IndexOf(Trim(ExtUpperCase(A->GameExe())));
        if ((J < 0) || (Hashs[J] != A->GameExeMD5())) continue;
        if (Trim(A->SetupExe()) != "") {
            J = Files.IndexOf(Trim(ExtUpperCase(A->SetupExe())));
            if ((J < 0) || (Hashs[J] != A->SetupExeMD5())) continue;
        }
        return true;
    }

    /* Nothing found */
    return false;
}

void TCreateConfForm::SelectGamesWithoutTemplate()
{
    int I;
    TGame* G;
    TGameDB* AutoSetupDB;

    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    for (I = 0; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() != Qt::Checked) {
        G = CheckListGame(ui.ListBox, I);
        if (ScummVMMode(G) || WindowsExeMode(G)) continue;
        if (G->Name() == DosBoxDOSProfile) continue;
        if (!FindAutoSetupForGame(G, AutoSetupDB)) ui.ListBox->item(I)->setCheckState(Qt::Checked);
    }
    delete AutoSetupDB;
}

void TCreateConfForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    TGame *G, *G2;
    QString Dir, S;
    TStringList* St;

    Dir = IncludeTrailingPathDelimiter(ui.FolderEdit->text());

    if (!ForceDirectories(Dir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dir}), mtError, mbOK, 0, this);
        setModalResult(mrNone);
        return;
    }

    for (I = 0; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked) {
        G = CheckListGame(ui.ListBox, I);
        if (ProfFileMode) {
            G->StoreAllValues();
            if (ui.ExportAutoSetupTemplatesCheckBox->isChecked()) {
                /* export as auto setup template */
                if (ScummVMMode(G)) continue;
                CreateGameCheckSum(G, false);
                CreateSetupCheckSum(G, false);
                AddAdditionalChecksumDataToAutoSetupTemplate(G);
                G2 = new TGame(Dir + ExtractFileName(G->SetupFile()));
                G2->AssignFrom(G);
                RemoveNonAutoSetupValues(G2);
                G2->LoadCache();
                G2->StoreAllValues();
                delete G2;
            } else {
                /* just copy profile file */
                if (!CopyFile(G->SetupFile(), Dir + ExtractFileName(G->SetupFile()), true)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {G->SetupFile(), Dir + ExtractFileName(G->SetupFile())}), mtError, mbOK, 0, this);
                    setModalResult(mrNone);
                    return;
                }
            }
        } else {
            if (ScummVMMode(G)) {
                S = Dir + ChangeFileExt(ExtractFileName(G->SetupFile()), ".ini");
                St = BuildScummVMIniFile(G);
            } else {
                S = Dir + ChangeFileExt(ExtractFileName(G->SetupFile()), ".conf");
                St = BuildConfFile(G, false, false, -1, nullptr, false);
                if (St == nullptr) continue;
            }
            if (!St->SaveToFile(S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {S}), mtError, mbOK, 0, this);
                setModalResult(mrNone);
                delete St;
                return;
            }
            delete St;
        }
    }
}

void TCreateConfForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ProfFileMode)
        ApplicationHelpCommand(HELP_CONTEXT, ID_FileExportProf);
    else
        ApplicationHelpCommand(HELP_CONTEXT, ID_FileExportConf);
}

void TCreateConfForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ExportConfFiles(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    CreateConfForm = new TCreateConfForm(AOwner);
    CreateConfForm->GameDB = AGameDB;
    result = (CreateConfForm->ShowModal() == mrOk);
    delete CreateConfForm;
    CreateConfForm = nullptr;
    return result;
}

bool ExportProfFiles(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    CreateConfForm = new TCreateConfForm(AOwner);
    CreateConfForm->ProfFileMode = true;
    CreateConfForm->GameDB = AGameDB;
    result = (CreateConfForm->ShowModal() == mrOk);
    delete CreateConfForm;
    CreateConfForm = nullptr;
    return result;
}
