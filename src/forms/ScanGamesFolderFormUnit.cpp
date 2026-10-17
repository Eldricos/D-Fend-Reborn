// C++ port of ScanGamesFolderFormUnit.pas (implementation section).
#include "ScanGamesFolderFormUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgConsts.h"
#include "WaitFormUnit.h"
#include "ZipPackageUnit.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "HashCalc.h"
#include "Process.h"
#include <QTreeView>

TScanGamesFolderForm* ScanGamesFolderForm = nullptr;

static inline TNewGame* NG(TList* L, int I) { return static_cast<TNewGame*>((*L)[I]); }

// TNewGame

TNewGame::TNewGame(const QString& AName, const QString& AExeFile)
{
    FName = AName;
    FExeFile = AExeFile;
    FSelectedTemplate = -1;
    FMatchingAutoSetup = new TStringList();
    FAllExeFiles = new TStringList();
}

TNewGame::~TNewGame()
{
    delete FMatchingAutoSetup;
    delete FAllExeFiles;
}

bool TNewGame::MatchingAutoSetupTemplate(int AutoSetupTemplateNr)
{
    for (int I = 0; I < FMatchingAutoSetup->Count(); I++) if (FMatchingAutoSetup->ObjectAsInt(I) == AutoSetupTemplateNr) return true;
    return false;
}

void TNewGame::SetExeNameFromMatchingAutoSetupTemplate(int AutoSetupTemplateNr)
{
    for (int I = 0; I < FMatchingAutoSetup->Count(); I++) if (FMatchingAutoSetup->ObjectAsInt(I) == AutoSetupTemplateNr) {
        FExeFile = IncludeTrailingPathDelimiter(ExtractFilePath(FExeFile)) + (*FMatchingAutoSetup)[I];
        return;
    }
}

// TScanGamesFolderResultsForm

TScanGamesFolderForm::TScanGamesFolderForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("ScanGamesFolderFormUnit", "ImageList");

    // VCL OnChange of csDropDownList combo boxes only fires on user changes -> activated
    connect(ui.ProfileNameEdit, &QLineEdit::textChanged, this, [this]() { UpdateListRec(ui.ProfileNameEdit); });
    connect(ui.Template1ComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { Template1ComboBoxChange(ui.Template1ComboBox); });
    connect(ui.Template2ComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { UpdateListRec(ui.Template2ComboBox); });
    connect(ui.ProgramFileComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { UpdateListRec(ui.ProgramFileComboBox); });
    // OnDropDown
    for (QComboBox* C : {ui.Template1ComboBox, ui.Template2ComboBox, ui.ProgramFileComboBox}) C->installEventFilter(this);
    connect(ui.List, &TListView::change, this, [this](TListItem* Item) { ListChange(ui.List, Item, 0); });
    QAction* Buttons[] = {ui.CloseButton, ui.OpenFolderButton, ui.NameTypeButton, ui.ScanButton, ui.AddButton, ui.HelpButton,
                          ui.PopupNameFilename, ui.PopupNameFoldername, ui.PopupSelectAll, ui.PopupSelectAutoSetup, ui.PopupSelectNone};
    for (QAction* A : Buttons) connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TScanGamesFolderForm::~TScanGamesFolderForm()
{
    FormDestroy(this);
    delete ImageList;
}

void TScanGamesFolderForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

bool TScanGamesFolderForm::eventFilter(QObject *O, QEvent *E)
{
    if ((O == ui.Template1ComboBox || O == ui.Template2ComboBox || O == ui.ProgramFileComboBox) &&
        (E->type() == QEvent::MouseButtonPress || (E->type() == QEvent::KeyPress && (static_cast<QKeyEvent*>(E)->key() == Qt::Key_F4 || ((static_cast<QKeyEvent*>(E)->modifiers() & Qt::AltModifier) && static_cast<QKeyEvent*>(E)->key() == Qt::Key_Down)))))
        ComboBoxDropDown(O);
    return TVclForm::eventFilter(O, E);
}

void TScanGamesFolderForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    TListColumn* C;

    // SetVistaFonts / Font.Charset: not needed in Qt

    setWindowTitle(LanguageSetup->AutoDetectGames());
    ui.CloseButton->setText(LanguageSetup->Close());
    ui.CloseButton->setToolTip(LanguageSetup->CloseHintWindow());
    ui.OpenFolderButton->setText(LanguageSetup->AutoDetectGamesOpenFolderButton());
    ui.OpenFolderButton->setToolTip(LanguageSetup->AutoDetectGamesOpenFolderButtonHint());
    ui.NameTypeButton->setText(LanguageSetup->AutoDetectProfileNameType());
    ui.NameTypeButton->setToolTip(LanguageSetup->AutoDetectProfileNameTypeHint());
    ui.ScanButton->setText(LanguageSetup->AutoDetectGamesScanButton());
    ui.ScanButton->setToolTip(LanguageSetup->AutoDetectGamesScanButtonHint());
    ui.AddButton->setText(LanguageSetup->AutoDetectGamesAddButton());
    ui.AddButton->setToolTip(LanguageSetup->AutoDetectGamesAddButtonHint());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.HelpButton->setToolTip(LanguageSetup->HelpHint());
    ui.PopupNameFilename->setText(LanguageSetup->AutoDetectProfileNameTypeGameFilename());
    ui.PopupNameFoldername->setText(LanguageSetup->AutoDetectProfileNameTypeFoldername());
    ui.PopupSelectAll->setText(LanguageSetup->AutoDetectProfilePopupSelectAll());
    ui.PopupSelectAutoSetup->setText(LanguageSetup->AutoDetectProfilePopupSelectAutoSetup());
    ui.PopupSelectNone->setText(LanguageSetup->AutoDetectProfilePopupSelectNone());
    ui.InfoLabel->setText(LanguageSetup->AutoDetectGamesResultStart());
    ui.List->Columns()->Clear();
    C = ui.List->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->AutoDetectProfileListName());
    C = ui.List->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->AutoDetectProfileListFolder());
    C = ui.List->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->AutoDetectProfileListFilename());
    C = ui.List->Columns()->Add(); C->SetWidth(-1); C->SetCaption(LanguageSetup->AutoDetectProfileListTemplate());

    ui.ProfileNameEdit_EditLabel->setText(LanguageSetup->AutoDetectProfileEditProfileName());
    ui.Template1Label->setText(LanguageSetup->AutoDetectProfileEditTemplateType());
    ui.Template2Label->setText(LanguageSetup->AutoDetectProfileEditTemplate());
    ui.ProgramFileLabel->setText(LanguageSetup->AutoDetectProfileEditProgramFile());
    ui.Template1ComboBox->addItem(LanguageSetup->AutoDetectProfileEditTemplateTypeUser());
    ui.Template1ComboBox->addItem(LanguageSetup->AutoDetectProfileEditTemplateTypeAutoSetup1());
    ui.Template1ComboBox->addItem(LanguageSetup->AutoDetectProfileEditTemplateTypeAutoSetup2());

    // ImageIndex values of the tool buttons from the DFM
    UserIconLoader->DialogImage(DI_CloseWindow, ImageList, 0);
    UserIconLoader->DialogImage(DI_SelectFolder, ImageList, 1);
    UserIconLoader->DialogImage(DI_NameType, ImageList, 2);
    UserIconLoader->DialogImage(DI_Wizard, ImageList, 3);
    UserIconLoader->DialogImage(DI_Add, ImageList, 4);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ImageList, 5);
    ui.CloseButton->setIcon(ImageList->Icon(0));
    ui.OpenFolderButton->setIcon(ImageList->Icon(1));
    ui.NameTypeButton->setIcon(ImageList->Icon(2));
    ui.ScanButton->setIcon(ImageList->Icon(3));
    ui.AddButton->setIcon(ImageList->Icon(4));
    ui.HelpButton->setIcon(ImageList->Icon(5));

    if (PrgSetup->ScanFolderUseFileName()) ui.PopupNameFilename->setChecked(true); else ui.PopupNameFoldername->setChecked(true);

    AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);

    NewGames = new TList();

    GamesAdded = false;
    LastSelected = -1; JustUpdating = false;
}

void TScanGamesFolderForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetColWidths();
}

void TScanGamesFolderForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    LastSelected = -1;
    PrgSetup->SetScanFolderUseFileName(ui.PopupNameFilename->isChecked());
    delete AutoSetupDB; AutoSetupDB = nullptr;
    delete TemplateDB; TemplateDB = nullptr;

    if (NewGames != nullptr) {
        for (int I = 0; I < NewGames->size(); I++) delete NG(NewGames, I);
        delete NewGames; NewGames = nullptr;
    }
}

void TScanGamesFolderForm::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
        case 0: Close(); break;
        case 1:
            if (PrgSetup->GameDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
            OpenFolder(S);
            break;
        case 2: {
            // NameTypeButton.CheckMenuDropdown
            auto* B = qobject_cast<QToolButton*>(ui.ToolBar->widgetForAction(ui.NameTypeButton));
            if (B != nullptr) B->showMenu(); else ui.PopupMenuName->popup(QCursor::pos());
            break;
        }
        case 3:
            setEnabled(false);
            Clear();
            ScanFolders();
            SetColWidths();
            setEnabled(true);
            break;
        case 4:
            ui.List->SetItemIndex(-1); ListChange(ui.List, ui.List->Selected(), 0);
            I = 0;
            while (I < ui.List->Items()->Count()) {
                if (!ui.List->Items()->Item(I)->Checked()) { I++; continue; }
                GamesAdded = true;
                AddGame(NG(NewGames, I));
                delete NG(NewGames, I); NewGames->removeAt(I); ui.List->Items()->Delete(I);
            }
            break;
        case 5: ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasScanGamesFolder); break;
        case 6: ui.PopupNameFilename->setChecked(true); break;
        case 7: ui.PopupNameFoldername->setChecked(true); break;
        case 8:
            ListChange(ui.List, ui.List->Selected(), 0);
            for (I = 0; I < ui.List->Items()->Count(); I++) ui.List->Items()->Item(I)->SetChecked(true);
            break;
        case 9:
            ListChange(ui.List, ui.List->Selected(), 0);
            for (I = 0; I < ui.List->Items()->Count(); I++) ui.List->Items()->Item(I)->SetChecked(NG(NewGames, I)->SelectedTemplate() >= 0);
            break;
        case 10:
            ListChange(ui.List, ui.List->Selected(), 0);
            for (I = 0; I < ui.List->Items()->Count(); I++) ui.List->Items()->Item(I)->SetChecked(false);
            break;
    }
}

void TScanGamesFolderForm::SetColWidths()
{
    // Width:=-1 (LVSCW_AUTOSIZE) / -2 (LVSCW_AUTOSIZE_USEHEADER)
    for (int I = 0; I < ui.List->Columns()->Count(); I++) {
        if (ui.List->Items()->Count() > 0) {
            ui.List->Column(I)->SetWidth(-2);
            ui.List->Column(I)->SetWidth(-1);
        } else {
            ui.List->Column(I)->SetWidth(-2);
        }
        ui.List->reportView()->resizeColumnToContents(I);
    }

    ui.AddButton->setEnabled(ui.List->Items()->Count() > 0);
    ui.PopupSelectAll->setEnabled(ui.List->Items()->Count() > 0);
    ui.PopupSelectAutoSetup->setEnabled(ui.List->Items()->Count() > 0);
    ui.PopupSelectNone->setEnabled(ui.List->Items()->Count() > 0);
}

void TScanGamesFolderForm::Clear()
{
    LastSelected = -1;

    for (int I = 0; I < NewGames->size(); I++) delete NG(NewGames, I);
    NewGames->clear();
    ui.List->Items()->Clear();

    ListChange(ui.List, ui.List->Selected(), 0);
}

void TScanGamesFolderForm::ComboBoxDropDown(QObject* Sender)
{
    if (auto* C = qobject_cast<QComboBox*>(Sender)) SetComboDropDownDropDownWidth(C);
}

void TScanGamesFolderForm::AddResultsToList()
{
    int T;
    TListItem* L;

    for (int I = 0; I < NewGames->size(); I++) {
        L = ui.List->Items()->Add();
        L->SetCaption(NG(NewGames, I)->Name());
        L->SubItems().Add(MakeRelPath(ExtractFilePath(NG(NewGames, I)->ExeFile()), PrgSetup->BaseDir()));
        L->SubItems().Add(ExtractFileName(NG(NewGames, I)->ExeFile()));
        T = NG(NewGames, I)->SelectedTemplate();
        if (T >= 0) L->SubItems().Add(AutoSetupDB->Game(T)->CacheName); else {
            if (T == -1) L->SubItems().Add(LanguageSetup->TemplateFormDefault()); else L->SubItems().Add(TemplateDB->Game(-T - 2)->CacheName);
        }
        L->SetChecked(T >= 0);
    }

    if (ui.List->Items()->Count() > 0) ui.List->SetItemIndex(0);
    ListChange(ui.List, ui.List->Selected(), 0);
}

void TScanGamesFolderForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) ButtonWork(ui.HelpButton);
}

void TScanGamesFolderForm::UpdateListRec(QObject* Sender)
{
    Q_UNUSED(Sender);
    TNewGame* NewGame;
    if ((LastSelected < 0) || JustUpdating) return;
    if (LastSelected >= NewGames->size() || LastSelected >= ui.List->Items()->Count()) return;

    NewGame = NG(NewGames, LastSelected);
    NewGame->SetName(ui.ProfileNameEdit->text());
    ui.List->Items()->Item(LastSelected)->SetCaption(ui.ProfileNameEdit->text());
    if (ui.Template1ComboBox->currentIndex() == 2) {
        // (Matching) auto setup
        NewGame->SetSelectedTemplate(ui.Template2ComboBox->itemData(ui.Template2ComboBox->currentIndex()).toInt());
        NewGame->SetExeNameFromMatchingAutoSetupTemplate(NewGame->SelectedTemplate());
    } else {
        if (ui.Template1ComboBox->currentIndex() == 1) {
            // (Not matching) auto setup template
            if (ui.Template2ComboBox->currentIndex() < 0) NewGame->SetSelectedTemplate(-1); else NewGame->SetSelectedTemplate(ui.Template2ComboBox->itemData(ui.Template2ComboBox->currentIndex()).toInt());
            NewGame->SetExeFile(IncludeTrailingPathDelimiter(ExtractFilePath(NewGame->ExeFile())) + ui.ProgramFileComboBox->currentText());
        } else {
            // User template
            NewGame->SetSelectedTemplate(-ui.Template2ComboBox->currentIndex() - 1);
            NewGame->SetExeFile(IncludeTrailingPathDelimiter(ExtractFilePath(NewGame->ExeFile())) + ui.ProgramFileComboBox->currentText());
        }
    }
    ui.List->Items()->Item(LastSelected)->SubItems().SetStrings(1, ExtractFileName(NewGame->ExeFile()));
    if (NewGame->SelectedTemplate() >= 0) ui.List->Items()->Item(LastSelected)->SubItems().SetStrings(2, AutoSetupDB->Game(NewGame->SelectedTemplate())->CacheName); else {
        if (NewGame->SelectedTemplate() == -1) ui.List->Items()->Item(LastSelected)->SubItems().SetStrings(2, LanguageSetup->TemplateFormDefault()); else ui.List->Items()->Item(LastSelected)->SubItems().SetStrings(2, TemplateDB->Game(-NewGame->SelectedTemplate() - 2)->CacheName);
    }
}

void TScanGamesFolderForm::ListChange(QObject* Sender, TListItem* Item, int Change)
{
    Q_UNUSED(Item); Q_UNUSED(Change);
    TNewGame* NewGame;

    if (LastSelected >= 0) UpdateListRec(Sender);

    LastSelected = ui.List->ItemIndex();
    ui.EditPanel->setVisible(ui.List->ItemIndex() >= 0);
    if (ui.List->ItemIndex() < 0) return;
    NewGame = NG(NewGames, LastSelected);

    JustUpdating = true;
    ui.ProfileNameEdit->setText(NewGame->Name());
    if (NewGame->MatchingAutoSetup()->Count() == 0) {
        if (ui.Template1ComboBox->count() == 3) ui.Template1ComboBox->removeItem(2);
    } else {
        if (ui.Template1ComboBox->count() == 2) ui.Template1ComboBox->addItem(LanguageSetup->AutoDetectProfileEditTemplateTypeAutoSetup2());
    }
    if (NewGame->SelectedTemplate() >= 0) {
        if (NewGame->MatchingAutoSetupTemplate(NewGame->SelectedTemplate())) ui.Template1ComboBox->setCurrentIndex(2); else ui.Template1ComboBox->setCurrentIndex(1);
    } else {
        ui.Template1ComboBox->setCurrentIndex(0);
    }

    ui.ProgramFileComboBox->clear();
    ui.ProgramFileComboBox->addItems(NewGame->AllExeFiles()->ToQStringList());
    ui.ProgramFileComboBox->setCurrentIndex(NewGame->AllExeFiles()->IndexOf(ExtractFileName(NewGame->ExeFile())));

    LastTemplateType = -1;
    Template1ComboBoxChange(Sender);
    JustUpdating = false;
}

void TScanGamesFolderForm::Template1ComboBoxChange(QObject* Sender)
{
    int I, J;
    TNewGame* NewGame;
    bool SaveState;

    if (ui.List->ItemIndex() < 0) return;
    NewGame = NG(NewGames, ui.List->ItemIndex());

    ui.ProgramFileLabel->setVisible((ui.Template1ComboBox->currentIndex() < 2) && (ui.ProgramFileComboBox->count() > 1));
    ui.ProgramFileComboBox->setVisible((ui.Template1ComboBox->currentIndex() < 2) && (ui.ProgramFileComboBox->count() > 1));
    ui.Template2Label->setVisible((ui.Template1ComboBox->currentIndex() < 2) || (NewGame->MatchingAutoSetup()->Count() > 1));
    ui.Template2ComboBox->setVisible((ui.Template1ComboBox->currentIndex() < 2) || (NewGame->MatchingAutoSetup()->Count() > 1));
    VclLayout::fitHeight(ui.EditPanel);   // (height of the visible settings)

    SaveState = JustUpdating; JustUpdating = true;

    if (ui.Template1ComboBox->currentIndex() == 0) {
        // User template
        ui.Template2ComboBox->clear();
        ui.Template2ComboBox->addItem(LanguageSetup->TemplateFormDefault());
        for (I = 0; I < TemplateDB->Count(); I++) ui.Template2ComboBox->addItem(TemplateDB->Game(I)->CacheName);
        ui.Template2ComboBox->setCurrentIndex(Max(0, Min(ui.Template2ComboBox->count() - 1, -NewGame->SelectedTemplate() - 1)));
    } else if (ui.Template1ComboBox->currentIndex() == 1) {
        // (Not matching) auto setup template
        ui.Template2ComboBox->clear();
        for (I = 0; I < AutoSetupDB->Count(); I++) if (ui.Template2ComboBox->findText(AutoSetupDB->Game(I)->CacheName, Qt::MatchFixedString) < 0) ui.Template2ComboBox->addItem(AutoSetupDB->Game(I)->CacheName, I);
        I = -1;
        for (J = 0; J < ui.Template2ComboBox->count(); J++) if (ui.Template2ComboBox->itemData(J).toInt() == NewGame->SelectedTemplate()) { I = J; break; }
        ui.Template2ComboBox->setCurrentIndex(Max(0, Min(ui.Template2ComboBox->count() - 1, I)));
    } else if (ui.Template1ComboBox->currentIndex() == 2) {
        // (Matching) auto setup template
        ui.Template2ComboBox->clear();
        J = 0;
        for (I = 0; I < NewGame->MatchingAutoSetup()->Count(); I++) {
            ui.Template2ComboBox->addItem(AutoSetupDB->Game(NewGame->MatchingAutoSetup()->ObjectAsInt(I))->CacheName, NewGame->MatchingAutoSetup()->ObjectAsInt(I));
            if (NewGame->MatchingAutoSetup()->ObjectAsInt(I) == NewGame->SelectedTemplate()) J = I;
        }
        ui.Template2ComboBox->setCurrentIndex(J);
    }

    JustUpdating = SaveState;
    UpdateListRec(Sender);
}

TStringList* TScanGamesFolderForm::GetSubFolderList()
{
    TSearchRec Rec;
    int I;
    QString S;
    TStringList* result = new TStringList();

    if (PrgSetup->GameDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
    S = IncludeTrailingPathDelimiter(S);

    I = FindFirst(S + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) == faDirectory) && (Rec.Name != ".") && (Rec.Name != "..")) result->Add(S + Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TScanGamesFolderForm::ScanFolder(const QString& Folder, bool AddAllPrgs)
{
    QString S, FileToStart;
    TNewGame* NewGame;
    TStringList *Templates, *PrgFiles;
    int J, NewLevel;

    if ((!AddAllPrgs) && FolderAlreadyknown(Folder)) return;

    PrgFiles = new TStringList();
    Templates = GetTemplateFromFolderExt(true, Folder, AutoSetupDB, PrgFiles);
    if (PrgFiles->Count() == 0) { delete Templates; delete PrgFiles; return; }

    if (Templates->Count() == 0) {
        NewLevel = -1;
        for (int I = 0; I < PrgFiles->Count(); I++) {
            J = PrgFiles->ObjectAsInt(I);
            if (J > NewLevel) { NewLevel = J; FileToStart = (*PrgFiles)[I]; }
        }
        if (ui.PopupNameFilename->isChecked()) {
            NewGame = new TNewGame(ChangeFileExt(FileToStart, ""), IncludeTrailingPathDelimiter(Folder) + FileToStart);
        } else {
            S = Folder; if ((S != "") && (S[Length(S) - 1] == PathDelimChar)) S.chop(1);
            while ((S != "") && (Pos(PathDelimChar, S) != 0)) { S = Copy(S, Pos(PathDelimChar, S) + 1, MaxInt); }
            if (S == "") S = ChangeFileExt(FileToStart, "");
            NewGame = new TNewGame(S, IncludeTrailingPathDelimiter(Folder) + FileToStart);
        }
        NewGame->SetSelectedTemplate(-1);
        NewGame->AllExeFiles()->AddStrings(*PrgFiles);
        NewGames->append(NewGame);
    } else {
        if ((Templates->Count() == 1) || (!AddAllPrgs)) {
            FileToStart = (*Templates)[0];
            NewGame = new TNewGame(AutoSetupDB->Game(Templates->ObjectAsInt(0))->CacheName, IncludeTrailingPathDelimiter(Folder) + FileToStart);
            NewGame->MatchingAutoSetup()->AddStrings(*Templates);
            NewGame->SetSelectedTemplate(Templates->ObjectAsInt(0));
            NewGame->AllExeFiles()->AddStrings(*PrgFiles);
            NewGames->append(NewGame);
        } else {
            for (int I = 0; I < Templates->Count(); I++) {
                FileToStart = (*Templates)[I];
                NewGame = new TNewGame(AutoSetupDB->Game(Templates->ObjectAsInt(I))->CacheName, IncludeTrailingPathDelimiter(Folder) + FileToStart);
                NewGame->MatchingAutoSetup()->AddStrings(*Templates);
                NewGame->SetSelectedTemplate(Templates->ObjectAsInt(I));
                NewGame->AllExeFiles()->Add(FileToStart);
                NewGames->append(NewGame);
            }
        }
    }
    delete Templates;
    delete PrgFiles;
}

bool TScanGamesFolderForm::FolderAlreadyknown(const QString& Folder)
{
    QString S, T;
    TStringList* St;

    T = ExtUpperCase(Folder);
    for (int I = 0; I < GameDB->Count(); I++) {
        TGame* G = GameDB->Game(I);
        S = G->GameExe();
        if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
            S = ExtUpperCase(MakeAbsPath(S, PrgSetup->BaseDir()));
            if (Copy(S, 1, Length(T)) == T) return true;
        }
        S = G->SetupExe();
        if ((S != "") && (ExtUpperCase(Copy(S, 1, 7)) != "DOSBOX:")) {
            S = ExtUpperCase(MakeAbsPath(S, PrgSetup->BaseDir()));
            if (Copy(S, 1, Length(T)) == T) return true;
        }
        if (Trim(G->ExtraDirs()) != "") {
            St = ValueToList(G->ExtraDirs());
            for (int J = 0; J < St->Count(); J++) if (Trim((*St)[J]) != "") {
                S = ExtUpperCase(MakeAbsPath((*St)[J], PrgSetup->BaseDir()));
                if (Copy(S, 1, Length(T)) == T) { delete St; return true; }
            }
            delete St;
        }
    }

    return false;
}

void TScanGamesFolderForm::ScanFolders()
{
    TStringList* Dirs;
    QString FreeDOS, Dir;

    ui.InfoPanel->setVisible(false);
    Dirs = GetSubFolderList();

    FreeDOS = Trim(PrgSetup->PathToFREEDOS());
    if (FreeDOS != "") FreeDOS = IncludeTrailingPathDelimiter(MakeAbsPath(FreeDOS, PrgSetup->BaseDir()));
    if (!DirectoryExists(FreeDOS)) FreeDOS = "";
    FreeDOS = ExtUpperCase(FreeDOS);

    WaitForm = CreateWaitForm(this, LanguageSetup->AutoDetectProfileScanning(), Dirs->Count());
    for (int I = 0; I < Dirs->Count(); I++) {
        WaitForm->Step(I);
        Dir = IncludeTrailingPathDelimiter((*Dirs)[I]);
        ScanFolder(Dir, ExtUpperCase(Dir) == FreeDOS);
    }
    delete WaitForm; WaitForm = nullptr;
    ForceForegroundWindow(winId());
    delete Dirs;

    AddResultsToList();
    if (ui.List->Items()->Count() == 0) {
        ui.InfoLabel->setText(LanguageSetup->AutoDetectGamesResultNoGames());
        ui.InfoPanel->setVisible(true);
    }
}

void TScanGamesFolderForm::AddGame(TNewGame* NewGame)
{
    TGame *G, *G2;
    int I;
    QString Folder;
    TSearchRec Rec;
    QString S;
    TStringList* St;

    I = GameDB->Add(NewGame->Name());
    G = GameDB->Game(I);

    if (NewGame->SelectedTemplate() >= 0) {
        if (NewGame->MatchingAutoSetupTemplate(NewGame->SelectedTemplate())) {
            // Matching auto setup template
            G->AssignFrom(AutoSetupDB->Game(NewGame->SelectedTemplate()));
        } else {
            // Not matching auto setup template
            G->AssignFrom(AutoSetupDB->Game(NewGame->SelectedTemplate()));
        }
        G->SetGameExe(MakeRelPath(NewGame->ExeFile(), PrgSetup->BaseDir()));
        G->SetGameExeMD5(GetMD5Sum(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()), false));
        if ((Trim(G->SetupExe()) != "") && FileExists(IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()))) + ExtractFileName(G->SetupExe()))) {
            G->SetSetupExe(MakeRelPath(IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()))) + ExtractFileName(G->SetupExe()), PrgSetup->BaseDir()));
            G->SetSetupExeMD5(GetMD5Sum(MakeAbsPath(G->SetupExe(), PrgSetup->BaseDir()), false));
        } else {
            G->SetSetupExe("");
            G->SetSetupExeMD5("");
        }
    } else {
        if (NewGame->SelectedTemplate() == -1) {
            // Default template
            G2 = new TGame(PrgSetup); G->AssignFrom(G2); delete G2;
        } else {
            // User template
            G->AssignFrom(TemplateDB->Game(-NewGame->SelectedTemplate() - 2));
        }
        G->SetGameExe(MakeRelPath(NewGame->ExeFile(), PrgSetup->BaseDir()));
    }
    G->SetName(NewGame->Name());

    if (Trim(NewGame->ExeFile()) != "") S = ExtractFilePath(NewGame->ExeFile()); else S = "";
    if (ExtUpperCase(Copy(Trim(S), 1, 7)) == "DOSBOX:") S = "";
    St = BuildGameDirMountData(G, S, "");
    for (I = 0; I <= 9; I++) G->SetMount(I, "");
    G->SetNrOfMounts(St->Count());
    for (I = 0; I < St->Count(); I++) G->SetMount(I, (*St)[I]);
    delete St;

    Folder = IncludeTrailingPathDelimiter(ExtractFilePath(NewGame->ExeFile()));

    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + IntToStr(I) + PathDelim;
    }
    G->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
    ForceDirectories(MakeAbsPath(G->CaptureFolder(), PrgSetup->BaseDir()));

    // Look for Icons in game folder
    I = FindFirst(Folder + "*.ico", faAnyFile, Rec);
    if (I == 0) G->SetIcon(MakeRelPath(Folder + Rec.Name, PrgSetup->BaseDir()));
    FindClose(Rec);

    G->SetProfileMode("DOSBox");
    G->StoreAllValues();
    G->LoadCache();
}

// global

bool ShowScanGamesFolderResultsDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    ScanGamesFolderForm = new TScanGamesFolderForm(AOwner);
    ScanGamesFolderForm->GameDB = AGameDB;
    ScanGamesFolderForm->ShowModal();
    result = ScanGamesFolderForm->GamesAdded;
    delete ScanGamesFolderForm;
    ScanGamesFolderForm = nullptr;
    return result;
}
