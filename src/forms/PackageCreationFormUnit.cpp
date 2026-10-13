// C++ port of PackageCreationFormUnit.pas (implementation section).
#include "PackageCreationFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "PackageBuilderUnit.h"
#include "IconLoaderUnit.h"
#include "ZipPackageUnit.h"
#include "ZipInfoFormUnit.h"
#include "HashCalc.h"
#include "MainUnit.h"

TPackageCreationForm *PackageCreationForm = nullptr;

// Delphi virtual key codes
#ifndef VK_RETURN_Code
static constexpr quint16 VK_RETURN_Code = 0x0D, VK_INSERT_Code = 0x2D, VK_DELETE_Code = 0x2E, VK_F1_Code = 0x70;
#endif

static quint16 QtKeyToVK(int Key)
{
    switch (Key) {
        case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN_Code;
        case Qt::Key_Insert: return VK_INSERT_Code;
        case Qt::Key_Delete: return VK_DELETE_Code;
        case Qt::Key_F1: return VK_F1_Code;
        default: return quint16(Key & 0xFFFF);
    }
}

// TCheckListBox helpers
static void CheckListAddStrings(QListWidget *L, const TStringList &St)
{
    for (int I = 0; I < St.Count(); I++) {
        auto *Item = new QListWidgetItem(St[I], L);
        Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
        Item->setCheckState(Qt::Unchecked);
    }
}
static bool CheckListChecked(QListWidget *L, int I) { return L->item(I)->checkState() == Qt::Checked; }
static void CheckListSetChecked(QListWidget *L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }

// Is the action part of the popup menu (TMenuItem.Owner = PopupMenu)?
static bool MenuContainsAction(QMenu *Menu, QAction *A)
{
    for (QAction *X : Menu->actions()) {
        if (X == A) return true;
        if (X->menu() != nullptr && MenuContainsAction(X->menu(), A)) return true;
    }
    return false;
}

// Column.Width:=-1 / -2
static void AutoSizeColumn(TListView *LV, int I)
{
    LV->reportView()->resizeColumnToContents(I);
}

TPackageCreationForm::TPackageCreationForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // Non visual DFM components
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "exe";
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "xml";
    IconsImageList = new TImageList(16, 16);
    DummyImageList1 = new TImageList(16, 16);
    DummyImageList2 = new TImageList(16, 16);
    ImageList = TImageList::FromResource("PackageCreationFormUnit", "ImageList");
    ui.IconsListView->SetSmallImages(IconsImageList);

    connect(ui.OutputFileButton, &QToolButton::clicked, this, [this]() { OutputFileButtonClick(ui.OutputFileButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    for (QPushButton *B : {ui.GamesSelectAllButton, ui.GamesSelectNoneButton, ui.GamesSelectGenreButton,
                           ui.AutoSetupSelectAllButton, ui.AutoSetupSelectNoneButton, ui.AutoSetupSelectGenreButton,
                           ui.IconsSelectAllButton, ui.IconsSelectNoneButton, ui.IconSetsSelectAllButton, ui.IconSetsSelectNoneButton,
                           ui.LanguageFilesSelectAllButton, ui.LanguageFilesSelectNoneButton,
                           ui.PackagesAddButton, ui.PackagesEditButton, ui.PackagesDelButton})
        connect(B, &QPushButton::clicked, this, [this, B]() { ButtonWork(B); });
    connect(ui.GamesListView, &TListView::columnClick, this, [this](int C) { GamesListViewColumnClick(ui.GamesListView, ui.GamesListView->Column(C)); });
    connect(ui.AutoSetupListView, &TListView::columnClick, this, [this](int C) { AutoSetupListViewColumnClick(ui.AutoSetupListView, ui.AutoSetupListView->Column(C)); });
    connect(ui.PackagesListBox, &QListWidget::currentRowChanged, this, [this]() { PackagesListBoxClick(ui.PackagesListBox); });
    connect(ui.PackagesListBox, &QListWidget::itemClicked, this, [this]() { PackagesListBoxClick(ui.PackagesListBox); });
    connect(ui.PackagesListBox, &QListWidget::itemDoubleClicked, this, [this]() { PackagesListBoxDblClick(ui.PackagesListBox); });
    ui.PackagesListBox->installEventFilter(this);   // OnKeyDown
    connect(ui.ToolsCheckBox, &QCheckBox::clicked, this, [this]() { ToolsCheckBoxClick(ui.ToolsCheckBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        FormKeyDown(this, K, Mods);
        return (Key == Qt::Key_F1) && (Mods == Qt::NoModifier);
    });
    FormCreate(this);   // OnCreate
}

TPackageCreationForm::~TPackageCreationForm()
{
    FormDestroy(this);   // OnDestroy
    ui.IconsListView->SetSmallImages(nullptr);
    delete OpenDialog;
    delete SaveDialog;
    delete IconsImageList;
    delete DummyImageList1;
    delete DummyImageList2;
    delete ImageList;
}

void TPackageCreationForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FShown) { FShown = true; FormShow(this); }
}

bool TPackageCreationForm::eventFilter(QObject *O, QEvent *E)
{
    if (O == ui.PackagesListBox && E->type() == QEvent::KeyPress) {
        auto *KE = static_cast<QKeyEvent *>(E);
        quint16 K = QtKeyToVK(KE->key());
        PackagesListBoxKeyDown(ui.PackagesListBox, K, KE->modifiers());
        if (K == 0) return true;
    }
    return TVclForm::eventFilter(O, E);
}

void TPackageCreationForm::FormCreate(QObject * /*Sender*/)
{
    IconSetFolderNames = new TStringList();
}

void TPackageCreationForm::FormShow(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(...) - not needed with Qt

    setWindowTitle(LanguageSetup->PackageCreator());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    QTabWidget *PC = ui.PageControl;
    PC->setTabText(PC->indexOf(ui.TabSheet1), LanguageSetup->PackageManagerPageGames());
    PC->setTabText(PC->indexOf(ui.TabSheet2), LanguageSetup->PackageManagerPageAutoSetups());
    PC->setTabText(PC->indexOf(ui.TabSheet3), LanguageSetup->PackageManagerPageIcons());
    PC->setTabText(PC->indexOf(ui.TabSheet6), LanguageSetup->PackageManagerPageIconSets());
    PC->setTabText(PC->indexOf(ui.TabSheet4), LanguageSetup->PackageManagerPageLanguages());
    PC->setTabText(PC->indexOf(ui.TabSheet5), LanguageSetup->PackageManagerPageExePackages());
    PC->setTabText(PC->indexOf(ui.TabSheet7), LanguageSetup->PackageManagerPageTools());

    ui.DescriptionEdit_EditLabel->setText(LanguageSetup->PackageCreatorDescription());
    ui.OutputFileEdit_EditLabel->setText(LanguageSetup->PackageCreatorOutputFile());
    ui.OutputFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.InfoLabel->setText(LanguageSetup->PackageCreatorOutputFileInfo());

    ui.GamesSelectAllButton->setText(LanguageSetup->All());
    ui.GamesSelectNoneButton->setText(LanguageSetup->None());
    ui.GamesSelectGenreButton->setText(LanguageSetup->GameBy());
    ui.AutoSetupSelectAllButton->setText(LanguageSetup->All());
    ui.AutoSetupSelectNoneButton->setText(LanguageSetup->None());
    ui.AutoSetupSelectGenreButton->setText(LanguageSetup->GameBy());
    ui.IconsSelectAllButton->setText(LanguageSetup->All());
    ui.IconsSelectNoneButton->setText(LanguageSetup->None());
    ui.IconSetsSelectAllButton->setText(LanguageSetup->All());
    ui.IconSetsSelectNoneButton->setText(LanguageSetup->None());
    ui.LanguageFilesSelectAllButton->setText(LanguageSetup->All());
    ui.LanguageFilesSelectNoneButton->setText(LanguageSetup->None());
    ui.PackagesAddButton->setText(LanguageSetup->Add());
    ui.PackagesEditButton->setText(LanguageSetup->Edit());
    ui.PackagesDelButton->setText(LanguageSetup->Del());
    ui.ToolsCheckBox->setText(LanguageSetup->PackageManagerPageToolsSpecial());
    ui.ToolsRadioButton1->setText(LanguageSetup->PackageManagerPageToolsSpecial1());
    ui.ToolsRadioButton2->setText(LanguageSetup->PackageManagerPageToolsSpecial2());
    ui.ToolsRadioButton3->setText(LanguageSetup->PackageManagerPageToolsSpecial3());

    UserIconLoader->DirectLoad(ImageList, "PackageManager");
    // PageControl.Images = ImageList (TabSheet ImageIndex values from the DFM)
    PC->setTabIcon(PC->indexOf(ui.TabSheet1), ImageList->Icon(4));
    PC->setTabIcon(PC->indexOf(ui.TabSheet2), ImageList->Icon(5));
    PC->setTabIcon(PC->indexOf(ui.TabSheet3), ImageList->Icon(6));
    PC->setTabIcon(PC->indexOf(ui.TabSheet6), ImageList->Icon(7));
    PC->setTabIcon(PC->indexOf(ui.TabSheet4), ImageList->Icon(8));
    PC->setTabIcon(PC->indexOf(ui.TabSheet5), ImageList->Icon(9));
    PC->setTabIcon(PC->indexOf(ui.TabSheet7), ImageList->Icon(10));
    UserIconLoader->DialogImage(DI_Add, ui.PackagesAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.PackagesEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.PackagesDelButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.OutputFileButton);

    OpenDialog->Title = LanguageSetup->PackageManagerPageExePackagesSelect();
    OpenDialog->Filter = LanguageSetup->BuildInstallerDestFileFilter();

    ListSortGames = slbName;
    ListSortGamesReverse = false;
    ListSortAutoSetups = slbName;
    ListSortAutoSetupsReverse = false;

    InitListViewForGamesList(ui.GamesListView, true);
    InitListViewForGamesList(ui.AutoSetupListView, true);
    LoadLists();
}

void TPackageCreationForm::FormDestroy(QObject * /*Sender*/)
{
    delete IconSetFolderNames;
    IconSetFolderNames = nullptr;
}

void TPackageCreationForm::LoadLists()
{
    static const char *FileExts[] = {"ico", "png", "jpeg", "jpg", "gif", "bmp"};
    TSearchRec Rec;
    TListItem *L;
    int I;

    // Games
    LoadGamesList();
    BuildSelectPopupMenu(ui.GamesPopupMenu, GameDB, [this](QObject *S) { ButtonWork(S); }, true, true);

    // AutoSetup
    LoadAutoSetupsList();
    BuildSelectPopupMenu(ui.AutoSetupPopupMenu, AutoSetupDB, [this](QObject *S) { ButtonWork(S); }, false, true);

    // Icons
    ui.IconsListView->Items()->BeginUpdate();
    for (int J = 0; J < 6; J++) {
        I = FindFirst(PrgDataDir() + IconsSubDir + PathDelim + "*." + FileExts[J], faAnyFile, Rec);
        while (I == 0) {
            if ((Rec.Attr & faDirectory) == 0) {
                L = ui.IconsListView->Items()->Add(); L->SetCaption(Rec.Name);
                QIcon Icon;
                if (J == 0) {
                    Icon = QIcon(PrgDataDir() + IconsSubDir + PathDelim + Rec.Name);
                } else {
                    Icon = LoadImageAsIconFromFile(PrgDataDir() + IconsSubDir + PathDelim + Rec.Name);
                }
                if (!Icon.isNull()) { IconsImageList->AddIcon(Icon); L->SetImageIndex(IconsImageList->Count() - 1); }
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }
    ui.IconsListView->Items()->EndUpdate();
    AutoSizeColumn(ui.IconsListView, 0);

    // IconSets
    {
        TStringList St, St2;
        IconSetFolderNames->Clear();
        ListOfIconSets(&St, &St2, nullptr, IconSetFolderNames);
        CheckListAddStrings(ui.IconSetsListBox, St2);
    }

    // Language files
    {
        TStringList *St = GetLanguageList();
        CheckListAddStrings(ui.LanguageFilesListBox, *St);
        delete St;
    }

    // Games packages
    PackagesListBoxClick(this);
}

void TPackageCreationForm::LoadGamesList()
{
    TList CheckedGames;
    for (int I = 0; I < ui.GamesListView->Items()->Count(); I++) if (ui.GamesListView->Items()->Item(I)->Checked()) CheckedGames.append(ui.GamesListView->Items()->Item(I)->Data());
    AddGamesToList(ui.GamesListView, DummyImageList1, DummyImageList2, DFendReloadedMainForm->ImageList, GameDB, "", "", "", true, ListSortGames, ListSortGamesReverse, false, true, true, false);
    for (int I = 0; I < ui.GamesListView->Items()->Count(); I++) ui.GamesListView->Items()->Item(I)->SetChecked(CheckedGames.indexOf(ui.GamesListView->Items()->Item(I)->Data()) >= 0);
}

void TPackageCreationForm::LoadAutoSetupsList()
{
    TList CheckedGames;
    for (int I = 0; I < ui.AutoSetupListView->Items()->Count(); I++) if (ui.AutoSetupListView->Items()->Item(I)->Checked()) CheckedGames.append(ui.AutoSetupListView->Items()->Item(I)->Data());
    AddGamesToList(ui.AutoSetupListView, DummyImageList1, DummyImageList2, DFendReloadedMainForm->ImageList, AutoSetupDB, "", "", "", true, ListSortAutoSetups, ListSortAutoSetupsReverse, false, true, false, false);
    for (int I = 0; I < ui.AutoSetupListView->Items()->Count(); I++) ui.AutoSetupListView->Items()->Item(I)->SetChecked(CheckedGames.indexOf(ui.AutoSetupListView->Items()->Item(I)->Data()) >= 0);
}

void TPackageCreationForm::GamesListViewColumnClick(QObject * /*Sender*/, TListColumn *Column)
{
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSortGames, ListSortGamesReverse);
    LoadGamesList();
}

void TPackageCreationForm::AutoSetupListViewColumnClick(QObject * /*Sender*/, TListColumn *Column)
{
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSortAutoSetups, ListSortAutoSetupsReverse);
    LoadAutoSetupsList();
}

void TPackageCreationForm::ButtonWork(QObject *Sender)
{
    int I;
    QPoint P;

    if (auto *A = qobject_cast<QAction *>(Sender)) {
        if (MenuContainsAction(ui.GamesPopupMenu, A)) SelectGamesByPopupMenu(Sender, ui.GamesListView);
        if (MenuContainsAction(ui.AutoSetupPopupMenu, A)) SelectGamesByPopupMenu(Sender, ui.AutoSetupListView);
        return;
    }

    const int Tag = Sender->property("tag").toInt();
    switch (Tag) {
        case 0: case 1:
            for (I = 0; I < ui.GamesListView->Items()->Count(); I++) ui.GamesListView->Items()->Item(I)->SetChecked(Tag == 0);
            break;
        case 2:
            P = ui.GamesPanel->mapToGlobal(QPoint(ui.GamesSelectGenreButton->x(), ui.GamesSelectGenreButton->y()));
            ui.GamesPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 3: case 4:
            for (I = 0; I < ui.AutoSetupListView->Items()->Count(); I++) ui.AutoSetupListView->Items()->Item(I)->SetChecked(Tag == 3);
            break;
        case 5:
            P = ui.AutoSetupPanel->mapToGlobal(QPoint(ui.AutoSetupSelectGenreButton->x(), ui.AutoSetupSelectGenreButton->y()));
            ui.AutoSetupPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 6: case 7:
            for (I = 0; I < ui.IconsListView->Items()->Count(); I++) ui.IconsListView->Items()->Item(I)->SetChecked(Tag == 6);
            break;
        case 8: case 9:
            for (I = 0; I < ui.IconSetsListBox->count(); I++) CheckListSetChecked(ui.IconSetsListBox, I, Tag == 8);
            break;
        case 10: case 11:
            for (I = 0; I < ui.LanguageFilesListBox->count(); I++) CheckListSetChecked(ui.LanguageFilesListBox, I, Tag == 10);
            break;
        case 12:
            OpenDialog->InitialDir = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!OpenDialog->Execute()) return;
            ui.PackagesListBox->addItem(OpenDialog->FileName);
            ui.PackagesListBox->setCurrentRow(ui.PackagesListBox->count() - 1);
            PackagesListBoxClick(this);
            break;
        case 13:
            if (ui.PackagesListBox->currentRow() >= 0) {
                OpenDialog->InitialDir = ExtractFilePath(ui.PackagesListBox->item(ui.PackagesListBox->currentRow())->text());
                if (!OpenDialog->Execute()) return;
                ui.PackagesListBox->item(ui.PackagesListBox->currentRow())->setText(OpenDialog->FileName);
            }
            break;
        case 14:
            if (ui.PackagesListBox->currentRow() >= 0) {
                I = ui.PackagesListBox->currentRow();
                delete ui.PackagesListBox->takeItem(I);
                if (ui.PackagesListBox->count() > 0) ui.PackagesListBox->setCurrentRow(Max(0, I - 1));
                PackagesListBoxClick(this);
            }
            break;
    }
}

void TPackageCreationForm::PackagesListBoxClick(QObject * /*Sender*/)
{
    ui.PackagesEditButton->setEnabled(ui.PackagesListBox->currentRow() >= 0);
    ui.PackagesDelButton->setEnabled(ui.PackagesListBox->currentRow() >= 0);
}

void TPackageCreationForm::PackagesListBoxDblClick(QObject * /*Sender*/)
{
    ButtonWork(ui.PackagesEditButton);
}

void TPackageCreationForm::PackagesListBoxKeyDown(QObject * /*Sender*/, quint16 &Key, TShiftState /*Shift*/)
{
    switch (Key) {
        case VK_INSERT_Code: ButtonWork(ui.PackagesAddButton); break;
        case VK_RETURN_Code: ButtonWork(ui.PackagesEditButton); break;
        case VK_DELETE_Code: ButtonWork(ui.PackagesDelButton); break;
        default: return;
    }
    Key = 0;
}

void TPackageCreationForm::OutputFileButtonClick(QObject * /*Sender*/)
{
    QString S = Trim(ExtractFilePath(ui.OutputFileEdit->text()));
    if (S.isEmpty()) S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
    // Note: the original code computes S but does not assign it to SaveDialog.InitialDir

    SaveDialog->Title = LanguageSetup->PackageCreatorOutputTitle();
    SaveDialog->Filter = LanguageSetup->PackageCreatorOutputFilter();
    if (!SaveDialog->Execute()) return;

    ui.OutputFileEdit->setText(SaveDialog->FileName);
}

static bool CopyWithWithMsg(const QString &Source, const QString &Dest)
{
    bool result = CopyFile(Source, Dest, false);
    if (!result) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {Source, Dest}), mtError, mbOK, 0);
    return result;
}

TStringList *TPackageCreationForm::CreatePackageFile(const QString &OutputDir)
{
    TStringList *result = nullptr;
    QString S, T;
    int J;

    QDomDocument *Doc = AddPackageDataHeader(this, ui.DescriptionEdit->text()); if (Doc == nullptr) return result;
    auto Done = [&]() { delete Doc; return result; };

    // Games
    for (int I = 0; I < ui.GamesListView->Items()->Count(); I++) if (ui.GamesListView->Items()->Item(I)->Checked()) {
        auto *G = static_cast<TGame *>(ui.GamesListView->Items()->Item(I)->Data());
        S = OutputDir + ChangeFileExt(ExtractFileName(G->SetupFile()), ".zip");
        BuildZipPackage(this, G, S, true);
        if (!AddPackageDataZip(Doc, G, S)) return Done();
    }

    // AutoSetup
    for (int I = 0; I < ui.AutoSetupListView->Items()->Count(); I++) if (ui.AutoSetupListView->Items()->Item(I)->Checked()) {
        S = static_cast<TGame *>(ui.AutoSetupListView->Items()->Item(I)->Data())->SetupFile();
        T = OutputDir + ExtractFileName(S);
        if (!CopyWithWithMsg(S, T)) { delete result; result = nullptr; return Done(); }
        if (!AddPackageDataAutoSetup(Doc, T, "")) return Done();
    }

    // Icons
    for (int I = 0; I < ui.IconsListView->Items()->Count(); I++) if (ui.IconsListView->Items()->Item(I)->Checked()) {
        S = PrgDataDir() + IconsSubDir + PathDelim + ui.IconsListView->Items()->Item(I)->Caption();
        T = OutputDir + ui.IconsListView->Items()->Item(I)->Caption();
        if (!CopyWithWithMsg(S, T)) { delete result; result = nullptr; return Done(); }
        if (!AddPackageDataIcon(Doc, T)) return Done();
    }

    // IconSets
    for (int I = 0; I < ui.IconSetsListBox->count(); I++) if (CheckListChecked(ui.IconSetsListBox, I)) {
        S = (*IconSetFolderNames)[I];
        T = S; if (!T.isEmpty() && (T[T.length() - 1] == '\\' || T[T.length() - 1] == '/')) T.chop(1);
        J = Pos(PathDelim, T); while (J > 0) { T = Copy(T, J + 1, MaxInt); J = Pos(PathDelim, T); }
        T = OutputDir + T + ".zip";
        if (!CreateZipFile(this, T, S, dmNo, GetCompressStrengthFromPrgSetup())) return Done();
        if (!AddPackageDataIconSet(Doc, IncludeTrailingPathDelimiter(S) + IconsConfFile, T)) return Done();
    }

    // Language files
    for (int I = 0; I < ui.LanguageFilesListBox->count(); I++) if (CheckListChecked(ui.LanguageFilesListBox, I)) {
        S = LanguageFileNameFromName(ui.LanguageFilesListBox->item(I)->text()); if (S.isEmpty()) continue;
        T = OutputDir + ExtractFileName(S);
        if (!CopyWithWithMsg(S, T)) { delete result; result = nullptr; return Done(); }
        if (!AddPackageDataLanguage(Doc, T)) { delete result; result = nullptr; return Done(); }
    }

    // Games packages
    for (int I = 0; I < ui.PackagesListBox->count(); I++) {
        if (!AddPackageDataExe(Doc, ui.PackagesListBox->item(I)->text())) { delete result; result = nullptr; return Done(); }
    }

    result = AddPackageDataFooter(Doc);
    return Done();
}

TStringList *TPackageCreationForm::CreatePackageFileForPlainZips(const QString &OutputDir, bool AddAutoSetups, const QString &AutoSetupsMaxVersion)
{
    TStringList *result = nullptr;
    TSearchRec Rec;
    int I;

    if (!DirectoryExists(OutputDir)) return result;
    QDomDocument *Doc = AddPackageDataHeader(this, ui.DescriptionEdit->text()); if (Doc == nullptr) return result;

    I = FindFirst(IncludeTrailingPathDelimiter(OutputDir) + "*.zip", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            if (!AddPackageDataPlainZip(Doc, IncludeTrailingPathDelimiter(OutputDir) + Rec.Name, AutoSetupDB, this, AddAutoSetups, AutoSetupsMaxVersion)) {
                FindClose(Rec);
                delete Doc;
                return result;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    result = AddPackageDataFooter(Doc);
    delete Doc;
    return result;
}

TStringList *TPackageCreationForm::CreatePackageFileForAutoSetupTemplates(const QString &OutputDir, const QString &MaxVersion)
{
    TStringList *result = nullptr;
    TSearchRec Rec;
    int I;

    if (!DirectoryExists(OutputDir)) return result;
    QDomDocument *Doc = AddPackageDataHeader(this, ui.DescriptionEdit->text()); if (Doc == nullptr) return result;

    I = FindFirst(IncludeTrailingPathDelimiter(OutputDir) + "*.prof", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            if (!AddPackageDataAutoSetup(Doc, IncludeTrailingPathDelimiter(OutputDir) + Rec.Name, MaxVersion)) {
                FindClose(Rec);
                delete Doc;
                return result;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    result = AddPackageDataFooter(Doc);
    delete Doc;
    return result;
}

void TPackageCreationForm::OKButtonClick(QObject * /*Sender*/)
{
    TStringList *St = nullptr;
    QString OutputFile, S;

    OutputFile = ui.OutputFileEdit->text();

    if (Trim(OutputFile).isEmpty()) {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    OutputFile = MakeAbsPath(OutputFile, GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY));
    if (ExtractFileExt(OutputFile).isEmpty()) OutputFile = OutputFile + ".xml";

    if (ui.ToolsCheckBox->isChecked()) {
        if (ui.ToolsRadioButton1->isChecked() || ui.ToolsRadioButton2->isChecked()) {
            if (ui.ToolsRadioButton2->isChecked()) {
                S = GetNormalFileVersionAsString();
                if (!InputQuery(LanguageSetup->PackageManagerPageToolsSpecial2(), LanguageSetup->PackageManagerPageToolsMaxVersionInfoGame(), S, this)) return;
            } else {
                S = "";
            }
            St = CreatePackageFileForPlainZips(IncludeTrailingPathDelimiter(ExtractFilePath(OutputFile)), ui.ToolsRadioButton2->isChecked(), S);
        }
        if (ui.ToolsRadioButton3->isChecked()) {
            S = GetNormalFileVersionAsString();
            if (!InputQuery(LanguageSetup->PackageManagerPageToolsSpecial3(), LanguageSetup->PackageManagerPageToolsMaxVersionInfoAutoSetup(), S, this)) { delete St; return; }
            delete St;
            St = CreatePackageFileForAutoSetupTemplates(IncludeTrailingPathDelimiter(ExtractFilePath(OutputFile)), Trim(S));
        }
    } else {
        St = CreatePackageFile(IncludeTrailingPathDelimiter(ExtractFilePath(OutputFile)));
    }
    if (St == nullptr) { setModalResult(mrNone); return; }
    if (!St->SaveToFile(OutputFile)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {OutputFile}), mtError, mbOK, 0);
        setModalResult(mrNone);
    }
    delete St;
}

void TPackageCreationForm::HelpButtonClick(QObject * /*Sender*/)
{
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileExportPackageListCreator);
}

void TPackageCreationForm::ToolsCheckBoxClick(QObject * /*Sender*/)
{
    ui.ToolsRadioButton1->setEnabled(ui.ToolsCheckBox->isChecked());
    ui.ToolsRadioButton2->setEnabled(ui.ToolsCheckBox->isChecked());
    ui.ToolsRadioButton3->setEnabled(ui.ToolsCheckBox->isChecked());
}

void TPackageCreationForm::FormKeyDown(QObject *Sender, quint16 &Key, TShiftState Shift)
{
    if ((Key == VK_F1_Code) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowPackageCreationDialog(QWidget *AOwner, TGameDB *AGameDB)
{
    bool result;
    PackageCreationForm = new TPackageCreationForm(AOwner);
    auto *AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
    PackageCreationForm->GameDB = AGameDB;
    PackageCreationForm->AutoSetupDB = AutoSetupDB;
    result = (PackageCreationForm->ShowModal() == mrOk);
    delete AutoSetupDB;
    delete PackageCreationForm;
    PackageCreationForm = nullptr;
    return result;
}
