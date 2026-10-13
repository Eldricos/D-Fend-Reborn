// C++ port of PackageManagerFormUnit.pas (implementation section).
#include "PackageManagerFormUnit.h"
#include "VirtualKeys.h"

#include <QGuiApplication>

#include "IniFiles.h"
#include "PackageDBToolsUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "DownloadWaitFormUnit.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "ZipPackageUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "ZipInfoFormUnit.h"
#include "PackageManagerRepositoriesEditFormUnit.h"
#include "ProviderFormUnit.h"
#include "HashCalc.h"
#include "Process.h"

TPackageManagerForm *PackageManagerForm = nullptr;

// GetKeyState(VK_LSHIFT) / GetKeyState(VK_RSHIFT)
static bool ShiftPressed()
{
    return QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
}

// ExePackagesListBox.Items.Objects[I] (stored as Qt::UserRole data)
static void *ListBoxObject(QListWidget *L, int I)
{
    if (I < 0 || I >= L->count()) return nullptr;
    return L->item(I)->data(Qt::UserRole).value<void *>();
}

// "Object is TCachedPackage": cached packages are owned by PackageDBCache
static bool IsCachedPackage(TPackageDBCache *Cache, void *O)
{
    for (int I = 0; I < Cache->Count(); I++) if (Cache->Package(I) == O) return true;
    return false;
}

TPackageManagerForm::TPackageManagerForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageList = TImageList::FromResource("PackageManagerFormUnit", "ImageList");

    for (QAction *A : {ui.CloseButton, ui.UpdateButton, ui.ResponsitoriesButton, ui.HelpButton})
        connect(A, &QAction::triggered, this, [this, A]() { ButtonWork(A); });
    connect(ui.GamesButton, &QPushButton::clicked, this, [this]() { GamesButtonClick(ui.GamesButton); });
    connect(ui.GamesFilterButton, &QPushButton::clicked, this, [this]() { FilterClick(ui.GamesFilterButton); });
    connect(ui.AutoSetupButton, &QPushButton::clicked, this, [this]() { AutoSetupButtonClick(ui.AutoSetupButton); });
    connect(ui.AutoSetupFilterButton, &QPushButton::clicked, this, [this]() { FilterClick(ui.AutoSetupFilterButton); });
    connect(ui.IconsButton, &QPushButton::clicked, this, [this]() { IconsButtonClick(ui.IconsButton); });
    connect(ui.IconSetsButton, &QPushButton::clicked, this, [this]() { IconSetsButtonClick(ui.IconSetsButton); });
    connect(ui.LanguageButton, &QPushButton::clicked, this, [this]() { LanguageButtonClick(ui.LanguageButton); });
    for (TListView *LV : {ui.GamesListView, ui.AutoSetupListView, ui.IconsListView, ui.IconSetsListView, ui.LanguagesListView}) {
        // OnChanging (check box / selection changes) and OnClick
        connect(LV, &TListView::itemChecked, this, [this, LV](TListItem *Item) { bool Allow = true; ListViewChanging(LV, Item, 2, Allow); });
        connect(LV, &TListView::change, this, [this, LV](TListItem *Item) { bool Allow = true; ListViewChanging(LV, Item, 2, Allow); });
        connect(LV, &TListView::click, this, [this, LV]() { ListViewClick(LV); });
    }
    connect(ui.GamesListView, &TListView::columnClick, this, [this](int C) { GamesListViewColumnClick(ui.GamesListView, ui.GamesListView->Column(C)); });
    for (TListView *LV : {ui.GamesListView, ui.AutoSetupListView, ui.LanguagesListView})
        LV->OnInfoTip = [this, LV](TListItem *Item) { QString S; ListViewInfoTip(LV, Item, S); return S; };
    connect(ui.ExePackagesListBox, &QListWidget::currentRowChanged, this, [this]() { ExePackagesListBoxClick(ui.ExePackagesListBox); });
    connect(ui.ExePackagesButton, &QPushButton::clicked, this, [this]() { ExePackagesButtonClick(ui.ExePackagesButton); });
    connect(ui.ExePackagesDelButton, &QPushButton::clicked, this, [this]() { ExePackagesDelButtonClick(ui.ExePackagesDelButton); });
    connect(ui.ListViewPopupMenuDelete, &QAction::triggered, this, [this]() { ListViewPopupMenuDeleteClick(ui.ListViewPopupMenuDelete); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        FormKeyDown(this, K, Mods);
        return (Key == Qt::Key_F1) && (Mods == Qt::NoModifier);
    });
    FormCreate(this);   // OnCreate
}

TPackageManagerForm::~TPackageManagerForm()
{
    FormDestroy(this);   // OnDestroy
    delete ImageList;
}

void TPackageManagerForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FShown) { FShown = true; FormShow(this); }
}

void TPackageManagerForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(...) - not needed with Qt

    HideIfAlreadyInstalled = true;
    LanguageToActivate = "";

    setWindowTitle(LanguageSetup->PackageManager());
    ui.CloseButton->setText(LanguageSetup->Close());
    ui.CloseButton->setToolTip(LanguageSetup->CloseHintWindow());
    ui.UpdateButton->setText(LanguageSetup->PackageManagerMenuUpdateLists());
    ui.UpdateButton->setToolTip(LanguageSetup->PackageManagerMenuUpdateListsHint());
    ui.ResponsitoriesButton->setText(LanguageSetup->PackageManagerMenuRepositoriesList());
    ui.ResponsitoriesButton->setToolTip(LanguageSetup->PackageManagerMenuRepositoriesListHint());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.HelpButton->setToolTip(LanguageSetup->HelpHint());

    QTabWidget *PC = ui.PageControl;
    PC->setTabText(PC->indexOf(ui.TabSheet1), LanguageSetup->PackageManagerPageGames());
    PC->setTabText(PC->indexOf(ui.TabSheet2), LanguageSetup->PackageManagerPageAutoSetups());
    PC->setTabText(PC->indexOf(ui.TabSheet5), LanguageSetup->PackageManagerPageIcons());
    PC->setTabText(PC->indexOf(ui.TabSheet6), LanguageSetup->PackageManagerPageIconSets());
    PC->setTabText(PC->indexOf(ui.TabSheet3), LanguageSetup->PackageManagerPageLanguages());
    PC->setTabText(PC->indexOf(ui.TabSheet4), LanguageSetup->PackageManagerPageExePackages());

    UserIconLoader->DirectLoad(ImageList, "PackageManager");
    // ToolBar.Images / PageControl.Images / ListViewPopupMenu.Images (ImageIndex values from the DFM)
    ui.CloseButton->setIcon(ImageList->Icon(0));
    ui.UpdateButton->setIcon(ImageList->Icon(2));
    ui.ResponsitoriesButton->setIcon(ImageList->Icon(1));
    ui.HelpButton->setIcon(ImageList->Icon(3));
    PC->setTabIcon(PC->indexOf(ui.TabSheet1), ImageList->Icon(4));
    PC->setTabIcon(PC->indexOf(ui.TabSheet2), ImageList->Icon(5));
    PC->setTabIcon(PC->indexOf(ui.TabSheet5), ImageList->Icon(6));
    PC->setTabIcon(PC->indexOf(ui.TabSheet6), ImageList->Icon(7));
    PC->setTabIcon(PC->indexOf(ui.TabSheet3), ImageList->Icon(8));
    PC->setTabIcon(PC->indexOf(ui.TabSheet4), ImageList->Icon(9));
    ui.ListViewPopupMenuDelete->setIcon(ImageList->Icon(10));

    ui.GamesFilterButton->setText(LanguageSetup->PackageManagerFilterList());
    ui.GamesButton->setText(LanguageSetup->PackageManagerInstallGames());
    ui.AutoSetupFilterButton->setText(LanguageSetup->PackageManagerFilterList());
    ui.AutoSetupButton->setText(LanguageSetup->PackageManagerInstallAutoSetups());
    ui.IconsButton->setText(LanguageSetup->PackageManagerInstallIcons());
    ui.IconSetsButton->setText(LanguageSetup->PackageManagerInstallIconSets());
    ui.LanguageButton->setText(LanguageSetup->PackageManagerInstallLanguages());
    ui.ExePackagesButton->setText(LanguageSetup->PackageManagerInstallPackage());
    ui.ExePackagesDelButton->setText(LanguageSetup->PackageManagerDeletePackage());

    ui.ListViewPopupMenuDelete->setText(LanguageSetup->Del());

    UserIconLoader->DialogImage(DI_Import, ui.GamesButton);
    UserIconLoader->DialogImage(DI_Import, ui.AutoSetupButton);
    UserIconLoader->DialogImage(DI_Import, ui.LanguageButton);
    UserIconLoader->DialogImage(DI_Import, ui.IconsButton);
    UserIconLoader->DialogImage(DI_Import, ui.IconSetsButton);
    UserIconLoader->DialogImage(DI_Import, ui.ExePackagesButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ExePackagesDelButton);

    GamesFilter.Key = fkNoFilter; GamesFilter.UpperValue = "";
    AutoSetupFilter.Key = fkNoFilter; AutoSetupFilter.UpperValue = "";

    for (int I = 0; I <= 4; I++) SortCol[I] = 1;
    InitPackageManagerListView(ui.GamesListView, false);
    InitPackageManagerListView(ui.AutoSetupListView, true);
    InitPackageManagerIconsListView(ui.IconsListView);
    InitPackageManagerIconSetsListView(ui.IconSetsListView);
    InitPackageManagerLanguagessListView(ui.LanguagesListView);
}

void TPackageManagerForm::FormShow(QObject * /*Sender*/)
{
    bool DoUpdateCheck;

    PackageDB = new TPackageDB();
    PackageDB->LoadDB(false, false, true);
    PackageDB->SetOnDownload([this](QObject *S, qint64 Progress, qint64 Size, TDownloadStatus Status, bool &ContinueDownload) { PackageDBDownload(S, Progress, Size, Status, ContinueDownload); });
    PackageDBCache = new TPackageDBCache();

    // NewWinControlType(..., TListViewSpecialHint, ...): the info tips are shown by TListView::OnInfoTip

    LanguageChecksumScanner1 = new TLanguageFileChecksumScanner(PrgDir() + LanguageSubDir, false, nullptr);
    if (PrgDir() == PrgDataDir()) {
        LanguageChecksumScanner2 = nullptr;
    } else {
        LanguageChecksumScanner2 = new TLanguageFileChecksumScanner(PrgDataDir() + LanguageSubDir, false, nullptr);
    }
    AutoSetupChecksumScanner = new TChecksumScanner(PrgDataDir() + AutoSetupSubDir, "*.prof", true, true, parentWidget());
    IconChecksumScanner = new TChecksumScanner(PrgDataDir() + IconsSubDir, "*.*", true, false, parentWidget());

    DoUpdateCheck = false;
    const int Today = Round(double(qint64(Date())));
    switch (PrgSetup->PackageListsCheckForUpdates()) {
        case 0: DoUpdateCheck = false; break;
        case 1: DoUpdateCheck = (Today >= PrgSetup->LastDataReaderUpdateCheck() + 7); break;
        case 2: DoUpdateCheck = (Today >= PrgSetup->LastDataReaderUpdateCheck() + 1); break;
        case 3: DoUpdateCheck = true; break;
    }
    if (DoUpdateCheck) {
        PackageDB->UpdateRemoteFiles(true);
        PrgSetup->SetLastPackageListeUpdateCheck(Today);
    }

    LoadLists();
}

void TPackageManagerForm::FormDestroy(QObject * /*Sender*/)
{
    delete PackageDB; PackageDB = nullptr;
    delete PackageDBCache; PackageDBCache = nullptr;
    delete LanguageChecksumScanner1; LanguageChecksumScanner1 = nullptr;
    if (LanguageChecksumScanner2 != nullptr) { delete LanguageChecksumScanner2; LanguageChecksumScanner2 = nullptr; }
    delete AutoSetupChecksumScanner; AutoSetupChecksumScanner = nullptr;
    delete IconChecksumScanner; IconChecksumScanner = nullptr;
}

void TPackageManagerForm::LoadLists()
{
    int I, J;
    bool B;

    // Games list
    LoadListView(ui.GamesListView, PackageDB, GamesFilter, AutoSetupChecksumScanner, false, GameDB, AutoSetupDB, HideIfAlreadyInstalled);
    ui.GamesFilterButton->setEnabled(ui.GamesListView->Items()->Count() > 0);
    SortListView(ui.GamesListView, abs(SortCol[0]) - 1, SortCol[0] < 0, abs(SortCol[0]) == 4);

    // Auto setup templates list
    LoadListView(ui.AutoSetupListView, PackageDB, AutoSetupFilter, AutoSetupChecksumScanner, true, GameDB, AutoSetupDB, HideIfAlreadyInstalled);
    ui.AutoSetupFilterButton->setEnabled(ui.AutoSetupListView->Items()->Count() > 0);
    SortListView(ui.AutoSetupListView, abs(SortCol[1]) - 1, SortCol[1] < 0, abs(SortCol[0]) == 3);

    // Icon list
    LoadIconsListView(ui.IconsListView, PackageDB, IconChecksumScanner, HideIfAlreadyInstalled);
    SortListView(ui.IconsListView, abs(SortCol[2]) - 1, SortCol[2] < 0, abs(SortCol[0]) == 2);

    // Icon sets
    LoadIconSetsListView(ui.IconSetsListView, PackageDB, HideIfAlreadyInstalled);
    SortListView(ui.IconSetsListView, abs(SortCol[3]) - 1, SortCol[3] < 0, abs(SortCol[0]) == 2);

    // Language files list
    LoadLanguagesListView(ui.LanguagesListView, PackageDB, HideIfAlreadyInstalled, LanguageChecksumScanner1, LanguageChecksumScanner2);
    SortListView(ui.LanguagesListView, abs(SortCol[4]) - 1, SortCol[4] < 0, false);

    // Multiple games pacakges list
    {
        const QSignalBlocker Blocker(ui.ExePackagesListBox);
        ui.ExePackagesListBox->clear();
        TStringList St;
        for (I = 0; I < PackageDB->Count(); I++) for (J = 0; J < PackageDB->List(I)->ExePackageCount(); J++)
            St.AddObject(PackageDB->List(I)->ExePackage(J)->Name(), static_cast<TDownloadData *>(PackageDB->List(I)->ExePackage(J)));
        for (I = 0; I < PackageDBCache->Count(); I++) if (St.IndexOf(PackageDBCache->Package(I)->Name()) < 0)
            St.AddObject(PackageDBCache->Package(I)->Name(), PackageDBCache->Package(I));
        St.Sort();
        for (I = 0; I < St.Count(); I++) {
            auto *Item = new QListWidgetItem(St[I], ui.ExePackagesListBox);
            Item->setData(Qt::UserRole, QVariant::fromValue<void *>(St.Objects(I)));
        }
        if (ui.ExePackagesListBox->count() > 0) ui.ExePackagesListBox->setCurrentRow(0);
    }
    ExePackagesListBoxClick(this);

    // Info label
    ui.InfoLabel->setText(LanguageSetup->PackageManagerAllListsEmpty());
    B = true;
    for (I = 0; I < PackageDB->Count(); I++) {
        TPackageList *L = PackageDB->List(I);
        if (L->GamesCount() + L->AutoSetupCount() + L->LanguageCount() + L->ExePackageCount() + L->IconCount() + L->IconSetCount() > 0) { B = false; break; }
    }
    ui.InfoPanel->setVisible(B);
    ListViewClick(nullptr);
}

void TPackageManagerForm::ListViewInfoTip(QObject * /*Sender*/, TListItem *Item, QString &InfoTip)
{
    InfoTip = "";
    if ((Item == nullptr) || (Item->Data() == nullptr)) return;
    InfoTip = GetPackageManagerToolTip(static_cast<TDownloadData *>(Item->Data()));
}

void TPackageManagerForm::ListViewPopupMenuDeleteClick(QObject * /*Sender*/)
{
    auto DeleteRecord = [this](void *Data) -> bool {
        bool result = false;
        switch (ui.PageControl->currentIndex()) {
            case 0: result = PackageDB->FindAndRemoveGame(static_cast<TDownloadZipData *>(static_cast<TDownloadData *>(Data)), this); break;
            case 1: result = PackageDB->FindAndRemoveAutoSetup(static_cast<TDownloadAutoSetupData *>(static_cast<TDownloadData *>(Data)), this); break;
            case 2: result = PackageDB->FindAndRemoveIcon(static_cast<TDownloadIconData *>(static_cast<TDownloadData *>(Data)), this); break;
            case 3: result = PackageDB->FindAndRemoveIconSet(static_cast<TDownloadIconSetData *>(static_cast<TDownloadData *>(Data)), this); break;
            case 4: result = PackageDB->FindAndRemoveLanguage(static_cast<TDownloadLanguageData *>(static_cast<TDownloadData *>(Data)), this); break;
        }
        return result;
    };

    TListView *ListView = nullptr;
    int I;
    bool Checked, NeedRebuild;

    switch (ui.PageControl->currentIndex()) {
        case 0: ListView = ui.GamesListView; break;
        case 1: ListView = ui.AutoSetupListView; break;
        case 2: ListView = ui.IconsListView; break;
        case 3: ListView = ui.IconSetsListView; break;
        case 4: ListView = ui.LanguagesListView; break;
    }
    if (ListView == nullptr) return;
    Checked = false;
    for (I = 0; I < ListView->Items()->Count(); I++) if (ListView->Items()->Item(I)->Checked()) { Checked = true; break; }
    if (!Checked && (ListView->ItemIndex() == -1)) return;

    ListView->setVisible(false);
    bool DoLoad = true;
    if (Checked) {
        NeedRebuild = false;
        for (I = 0; I < ListView->Items()->Count(); I++) if (ListView->Items()->Item(I)->Checked()) {
            if (DeleteRecord(ListView->Items()->Item(I)->Data())) NeedRebuild = true;
        }
        if (!NeedRebuild) DoLoad = false;
    } else {
        if (!DeleteRecord(ListView->Items()->Item(ListView->ItemIndex())->Data())) DoLoad = false;
    }
    if (DoLoad) LoadLists();
    ListView->setVisible(true);
}

void TPackageManagerForm::PackageDBDownload(QObject * /*Sender*/, qint64 Progress, qint64 Size, TDownloadStatus Status, bool &ContinueDownload)
{
    switch (Status) {
        case dsStart: setEnabled(false); InitDownloadWaitForm(this, int(Size / 1024)); break;
        case dsProgress:
            if (DownloadWaitForm != nullptr && DownloadWaitForm->ui.ProgressBar->maximum() == 1) DownloadWaitForm->ui.ProgressBar->setMaximum(int(Max<qint64>(1, Size / 1024)));
            ContinueDownload = StepDownloadWaitForm(int(Progress / 1024));
            break;
        case dsDone: setEnabled(true); DoneDownloadWaitForm(); break;
    }
}

namespace {
struct TPackageInfo {
    TPackageList *PackageList = nullptr;
    QStringList DownloadData;
};
}

static QString FileNameOnly(const QString &S)
{
    QString result = S;
    int I = Pos('/', result); while (I > 0) { result = Copy(result, I + 1, MaxInt); I = Pos('/', result); }
    I = Pos('\\', result); while (I > 0) { result = Copy(result, I + 1, MaxInt); I = Pos('\\', result); }
    return result;
}

void TPackageManagerForm::ProviderAction(const QList<TDownloadData *> &Data)
{
    int J, K;
    QList<TPackageInfo> InfoURL, InfoDialog;

    for (TDownloadData *D : Data) {
        if (D->PackageList()->ProviderActions().contains(paInfoDialog)) {
            K = -1; for (J = 0; J < InfoDialog.size(); J++) if (InfoDialog[J].PackageList == D->PackageList()) { K = J; break; }
            if (K < 0) { K = InfoDialog.size(); InfoDialog.append(TPackageInfo()); InfoDialog[K].PackageList = D->PackageList(); }
            InfoDialog[K].DownloadData.append(FileNameOnly(D->Name()));
        }
        if (D->PackageList()->ProviderActions().contains(paOpenURL)) {
            K = -1; for (J = 0; J < InfoURL.size(); J++) if (InfoURL[J].PackageList == D->PackageList()) { K = J; break; }
            if (K < 0) { K = InfoURL.size(); InfoURL.append(TPackageInfo()); InfoURL[K].PackageList = D->PackageList(); }
            InfoURL[K].DownloadData.append(FileNameOnly(D->Name()));
        }
    }

    for (int I = 0; I < InfoURL.size(); I++) ShellOpen(InfoURL[I].PackageList->ProviderURL());

    if (InfoDialog.isEmpty()) return;

    for (int I = 0; I < InfoDialog.size(); I++) {
        TStringList St;
        for (J = 0; J < InfoDialog[I].DownloadData.size(); J++) St.Add(InfoDialog[I].DownloadData[J]);
        ShowProviderDialog(this, &St, InfoDialog[I].PackageList->ProviderName(), InfoDialog[I].PackageList->ProviderText(), InfoDialog[I].PackageList->ProviderURL());
    }
}

void TPackageManagerForm::ButtonWork(QObject *Sender)
{
    switch (Sender->property("tag").toInt()) {
        case 0: Close(); break;
        case 1:
            PackageDB->LoadDB(true, ShiftPressed(), false);
            PrgSetup->SetLastPackageListeUpdateCheck(Round(double(qint64(Date()))));
            LoadLists();
            break;
        case 2:
            if (ShowPackageManagerRepositoriesEditDialog(this)) {
                PackageDB->LoadDB(true, false, true);
                LoadLists();
            }
            break;
        case 3: ApplicationHelpCommand(HELP_CONTEXT, ID_FileImportDownload); break;
    }
}

void TPackageManagerForm::CheckList(TListView *ListView, QPushButton *Button, QLabel *InfoLabel)
{
    bool B = false;
    qint64 Size = 0;
    for (int I = 0; I < ListView->Items()->Count(); I++) if (ListView->Items()->Item(I)->Checked() && (ListView->Items()->Item(I)->Data() != nullptr)) {
        Size = Size + static_cast<TDownloadData *>(ListView->Items()->Item(I)->Data())->Size();
        B = true;
    }
    Button->setEnabled(B);
    InfoLabel->setText(LanguageSetup->PackageManagerDownloadSize() + ": " + GetNiceFileSize(Size));
}

void TPackageManagerForm::ListViewChanging(QObject *Sender, TListItem * /*Item*/, int /*Change*/, bool & /*AllowChange*/)
{
    ListViewClick(Sender);
}

void TPackageManagerForm::ListViewClick(QObject *Sender)
{
    if ((Sender == ui.GamesListView) || (Sender == nullptr)) CheckList(ui.GamesListView, ui.GamesButton, ui.GamesSizeLabel);
    if ((Sender == ui.AutoSetupListView) || (Sender == nullptr)) CheckList(ui.AutoSetupListView, ui.AutoSetupButton, ui.AutoSetupSizeLabel);
    if ((Sender == ui.IconsListView) || (Sender == nullptr)) CheckList(ui.IconsListView, ui.IconsButton, ui.IconsSizeLabel);
    if ((Sender == ui.IconSetsListView) || (Sender == nullptr)) CheckList(ui.IconSetsListView, ui.IconSetsButton, ui.IconSetsSizeLabel);
    if ((Sender == ui.LanguagesListView) || (Sender == nullptr)) CheckList(ui.LanguagesListView, ui.LanguageButton, ui.LanguageFilesSizeLabel);
}

void TPackageManagerForm::FilterClick(QObject *Sender)
{
    int Nr;
    bool AutoSetups;
    TFilterKey Key;
    QString S;
    TFilter *Filter;
    TGameDB *DB;

    if (auto *A = qobject_cast<QAction *>(Sender)) {
        Nr = A->property("tag").toInt();
        AutoSetups = (ui.FilterPopupMenu->property("tag").toInt() == 1);
        if (Nr < 0) { Key = fkNoFilter; S = ""; } else switch (A->property("parentTag").toInt()) {
            case 1: Key = fkLicense; S = (*FilterSelectLicense)[Nr]; break;
            case 2: Key = fkGenre; S = (*FilterSelectGenre)[Nr]; break;
            case 3: Key = fkDeveloper; S = (*FilterSelectDeveloper)[Nr]; break;
            case 4: Key = fkPublisher; S = (*FilterSelectPublisher)[Nr]; break;
            case 5: Key = fkYear; S = (*FilterSelectYear)[Nr]; break;
            case 6: Key = fkLanguage; S = (*FilterSelectLanguage)[Nr]; break;
            default: return;
        }
        if (AutoSetups) Filter = &AutoSetupFilter; else Filter = &GamesFilter;
        Filter->Key = Key; Filter->UpperValue = ExtUpperCase(S);
        if (AutoSetups)
            LoadListView(ui.AutoSetupListView, PackageDB, AutoSetupFilter, AutoSetupChecksumScanner, true, GameDB, AutoSetupDB, HideIfAlreadyInstalled);
        else
            LoadListView(ui.GamesListView, PackageDB, GamesFilter, AutoSetupChecksumScanner, false, GameDB, AutoSetupDB, HideIfAlreadyInstalled);
        ListViewClick(nullptr);
    } else if (auto *C = qobject_cast<QWidget *>(Sender)) {
        QPoint P = C->parentWidget()->mapToGlobal(QPoint(C->x() + 5, C->y() + 5));
        const bool IsAutoSetups = (C->property("tag").toInt() == 1);
        if (IsAutoSetups) DB = AutoSetupDB; else DB = GameDB;
        OpenFilterPopup(P, IsAutoSetups, ui.FilterPopupMenu, PackageDB, [this](QObject *S) { FilterClick(S); }, HideIfAlreadyInstalled, AutoSetupChecksumScanner, DB);
    }
}

void TPackageManagerForm::GamesButtonClick(QObject * /*Sender*/)
{
    QList<TDownloadData *> DownloadZipData;
    QString FileName;
    TGame *G;
    bool B;
    TDownloadResult DR;
    int J;

    for (int I = 0; I < ui.GamesListView->Items()->Count(); I++) if (ui.GamesListView->Items()->Item(I)->Checked())
        DownloadZipData.append(static_cast<TDownloadData *>(ui.GamesListView->Items()->Item(I)->Data()));
    if (DownloadZipData.isEmpty()) return;

    setEnabled(false);
    ProviderAction(DownloadZipData);
    for (int I = 0; I < DownloadZipData.size(); I++) {
        auto *ZipData = static_cast<TDownloadZipData *>(DownloadZipData[I]);
        FileName = ExtractFileNameFromURL(DownloadZipData[I]->URL(), ".zip", true);

        if (ZipData->MetaLink()) {
            DR = MetaLinkDownload(this, int(DownloadZipData[I]->Size()), DownloadZipData[I]->PackageFileURL(), DownloadZipData[I]->URL(), DownloadZipData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        } else {
            DR = DownloadFileWithDialog(this, DownloadZipData[I]->Size(), DownloadZipData[I]->PackageFileURL(), DownloadZipData[I]->URL(), DownloadZipData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        }
        if (DR == drCancel) break;
        if (DR != drSuccess) {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadZipData[I]->URL()}), mtError, mbOK, 0);
            continue;
        }

        if (!Trim(ZipData->AutoSetupForZipURL()).isEmpty()) {
            if (!Trim(ZipData->AutoSetupForZipURLMaxVersion()).isEmpty()) {
                try {
                    B = VersionToInt(GetNormalFileVersionAsString()) <= VersionToInt(ZipData->AutoSetupForZipURLMaxVersion());
                } catch (...) {
                    B = true;
                }
            } else {
                B = true;
            }
            if (B) {
                DR = DownloadFileWithDialog(this, 0, DownloadZipData[I]->PackageFileURL(), ZipData->AutoSetupForZipURL(), DownloadZipData[I]->PackageList()->Referer(), PackageDB->DBDir() + "DFRTemp.prof");
                if (DR == drCancel) break;
                if (DR != drSuccess) {
                    MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {ZipData->AutoSetupForZipURL()}), mtError, mbOK, 0);
                    continue;
                }
            }
            if (!AutoSetupChecksumScanner->ChecksumInList(GetMD5Sum(PackageDB->DBDir() + "DFRTemp.prof", true))) {
                J = AutoSetupDB->Add(DownloadZipData[I]->Name());
                G = new TGame(PackageDB->DBDir() + "DFRTemp.prof");
                AutoSetupDB->Game(J)->AssignFrom(G);
                delete G;
                AutoSetupChecksumScanner->ReScan();
            }
            ExtDeleteFile(PackageDB->DBDir() + "DFRTemp.prof", ftTemp);
        }

        G = ImportZipPackage(this, PackageDB->DBDir() + FileName, GameDB, true);

        if (G != nullptr) {
            WriteMetaInformationToProfile(G, ZipData);
            if (DownloadZipData[I]->PackageList()->ProviderActions().contains(paWriteInfoToProfile)) {
                WriteProverInfoToProfile(G, DownloadZipData[I]->PackageList()->ProviderName(), DownloadZipData[I]->PackageList()->ProviderURL());
            }
        }
        ExtDeleteFile(PackageDB->DBDir() + FileName, ftTemp);
    }
    setEnabled(true);
    LoadLists();
}

void TPackageManagerForm::GamesListViewColumnClick(QObject *Sender, TListColumn *Column)
{
    int I = -1, J = -1, ColNr;
    if (Sender == ui.GamesListView) { I = 0; J = 3; }
    if (Sender == ui.AutoSetupListView) { I = 1; J = 2; }
    if (Sender == ui.IconsListView) { I = 2; J = 1; }
    if (Sender == ui.IconSetsListView) { I = 3; J = 1; }
    if (Sender == ui.LanguagesListView) { I = 4; J = 1; }
    if (I < 0 || Column == nullptr) return;

    ColNr = Column->Index() + 1;

    if (abs(SortCol[I]) == ColNr) SortCol[I] = -SortCol[I]; else SortCol[I] = ColNr;

    SortListView(static_cast<TListView *>(Sender), abs(SortCol[I]) - 1, SortCol[I] < 0, abs(SortCol[I]) == J + 1);
}

void TPackageManagerForm::AutoSetupButtonClick(QObject * /*Sender*/)
{
    QList<TDownloadData *> DownloadAutoSetupData;
    QString FileName;
    TGame *G, *G2;
    TDownloadResult DR;
    int J;

    for (int I = 0; I < ui.AutoSetupListView->Items()->Count(); I++) if (ui.AutoSetupListView->Items()->Item(I)->Checked())
        DownloadAutoSetupData.append(static_cast<TDownloadData *>(ui.AutoSetupListView->Items()->Item(I)->Data()));
    if (DownloadAutoSetupData.isEmpty()) return;

    setEnabled(false);
    ProviderAction(DownloadAutoSetupData);
    for (int I = 0; I < DownloadAutoSetupData.size(); I++) {
        FileName = ExtractFileNameFromURL(DownloadAutoSetupData[I]->URL(), ".prof", true);
        DR = DownloadFileWithDialog(this, DownloadAutoSetupData[I]->Size(), DownloadAutoSetupData[I]->PackageFileURL(), DownloadAutoSetupData[I]->URL(), DownloadAutoSetupData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        if (DR == drCancel) break;
        if (DR != drSuccess) {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadAutoSetupData[I]->URL()}), mtError, mbOK, 0);
            continue;
        }
        ForceDirectories(PrgDataDir() + AutoSetupSubDir + PathDelim);
        if (FileExists(PrgDataDir() + AutoSetupSubDir + PathDelim + FileName)) {
            G = new TGame(PackageDB->DBDir() + FileName);
            J = AutoSetupDB->Add(DownloadAutoSetupData[I]->Name()); G2 = AutoSetupDB->Game(J);
            G2->AssignFrom(G);
            WriteMetaInformationToProfile(G2, static_cast<TDownloadAutoSetupData *>(DownloadAutoSetupData[I]));
            if (DownloadAutoSetupData[I]->PackageList()->ProviderActions().contains(paWriteInfoToProfile)) {
                WriteProverInfoToProfile(G2, DownloadAutoSetupData[I]->PackageList()->ProviderName(), DownloadAutoSetupData[I]->PackageList()->ProviderURL());
            }
            delete G;
            ExtDeleteFile(PackageDB->DBDir() + FileName, ftTemp);
        } else {
            RenameFile(PackageDB->DBDir() + FileName, PrgDataDir() + AutoSetupSubDir + PathDelim + FileName);
            G = new TGame(PrgDataDir() + AutoSetupSubDir + PathDelim + FileName);
            AutoSetupDB->Add(G);
        }
    }
    setEnabled(true);
    AutoSetupChecksumScanner->ReScan();
    LoadLists();
}

void TPackageManagerForm::IconsButtonClick(QObject * /*Sender*/)
{
    QList<TDownloadData *> DownloadIconData;
    QString FileName;
    TDownloadResult DR;

    for (int I = 0; I < ui.IconsListView->Items()->Count(); I++) if (ui.IconsListView->Items()->Item(I)->Checked())
        DownloadIconData.append(static_cast<TDownloadData *>(ui.IconsListView->Items()->Item(I)->Data()));
    if (DownloadIconData.isEmpty()) return;

    setEnabled(false);
    ProviderAction(DownloadIconData);
    for (int I = 0; I < DownloadIconData.size(); I++) {
        FileName = ExtractFileNameFromURL(DownloadIconData[I]->URL(), ".ico", true);
        DR = DownloadFileWithDialog(this, DownloadIconData[I]->Size(), DownloadIconData[I]->PackageFileURL(), DownloadIconData[I]->URL(), DownloadIconData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        if (DR == drCancel) break;
        if (DR == drSuccess) {
            ForceDirectories(PrgDataDir() + IconsSubDir + PathDelim);
            if (FileExists(PrgDataDir() + IconsSubDir + PathDelim + FileName)) ExtDeleteFile(PrgDataDir() + IconsSubDir + PathDelim + FileName, ftTemp);
            RenameFile(PackageDB->DBDir() + FileName, PrgDataDir() + IconsSubDir + PathDelim + FileName);
        } else {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadIconData[I]->URL()}), mtError, mbOK, 0);
        }
    }
    setEnabled(true);
    IconChecksumScanner->ReScan();
    LoadLists();
}

void TPackageManagerForm::IconSetsButtonClick(QObject * /*Sender*/)
{
    QList<TDownloadData *> DownloadIconSetData;
    QString FileName, DestDir;
    bool B;
    TDownloadResult DR;
    int J;

    for (int I = 0; I < ui.IconSetsListView->Items()->Count(); I++) if (ui.IconSetsListView->Items()->Item(I)->Checked())
        DownloadIconSetData.append(static_cast<TDownloadData *>(ui.IconSetsListView->Items()->Item(I)->Data()));
    if (DownloadIconSetData.isEmpty()) return;

    setEnabled(false);
    B = false;
    bool Canceled = false;
    ProviderAction(DownloadIconSetData);
    for (int I = 0; I < DownloadIconSetData.size(); I++) {
        FileName = ExtractFileNameFromURL(DownloadIconSetData[I]->URL(), ".zip", true);
        DR = DownloadFileWithDialog(this, DownloadIconSetData[I]->Size(), DownloadIconSetData[I]->PackageFileURL(), DownloadIconSetData[I]->URL(), DownloadIconSetData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        if (DR == drCancel) { Canceled = true; break; }
        if (DR == drSuccess) {
            B = true;
            ForceDirectories(PrgDataDir() + IconSetsFolder + PathDelim);
            DestDir = PrgDataDir() + IconSetsFolder + PathDelim + ChangeFileExt(FileName, "");
            J = 0; while (DirectoryExists(DestDir)) {
                J++; DestDir = PrgDataDir() + IconSetsFolder + PathDelim + ChangeFileExt(FileName, "") + IntToStr(J);
            }
            ForceDirectories(DestDir);
            ExtractZipFile(this, PackageDB->DBDir() + FileName, DestDir);
            DeleteFile(PackageDB->DBDir() + FileName);

            TIniFile Ini(IncludeTrailingPathDelimiter(DestDir) + IconsConfFile);
            auto *SetData = static_cast<TDownloadIconSetData *>(DownloadIconSetData[I]);
            Ini.WriteString("Information", "Name", DownloadIconSetData[I]->Name());
            Ini.WriteString("Information", "MinVersion", SetData->MinVersion());
            Ini.WriteString("Information", "MaxVersion", SetData->MaxVersion());
        } else {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadIconSetData[I]->URL()}), mtError, mbOK, 0);
        }
    }
    if (!Canceled && B) MessageDlg(LanguageSetup->PackageManagerIconSetDownloaded(), mtInformation, mbOK, 0);
    setEnabled(true);
    LoadLists();
}

void TPackageManagerForm::LanguageButtonClick(QObject * /*Sender*/)
{
    QList<TDownloadData *> DownloadLanguageData;
    QString FileName, DestLangFile;
    int C;
    TDownloadResult DR;

    for (int I = 0; I < ui.LanguagesListView->Items()->Count(); I++) if (ui.LanguagesListView->Items()->Item(I)->Checked())
        DownloadLanguageData.append(static_cast<TDownloadData *>(ui.LanguagesListView->Items()->Item(I)->Data()));
    if (DownloadLanguageData.isEmpty()) return;

    auto Finally = [this]() {
        setEnabled(true);
        LanguageChecksumScanner1->ReScan();
        if (LanguageChecksumScanner2 != nullptr) LanguageChecksumScanner2->ReScan();
        LoadLists();
    };

    setEnabled(false);
    C = 0; DestLangFile = "";
    ProviderAction(DownloadLanguageData);
    for (int I = 0; I < DownloadLanguageData.size(); I++) {
        FileName = ExtractFileNameFromURL(DownloadLanguageData[I]->URL(), ".ini", true);
        DR = DownloadFileWithDialog(this, DownloadLanguageData[I]->Size(), DownloadLanguageData[I]->PackageFileURL(), DownloadLanguageData[I]->URL(), DownloadLanguageData[I]->PackageList()->Referer(), PackageDB->DBDir() + FileName);
        if (DR == drCancel) { Finally(); return; }
        if (DR == drSuccess) {
            C++;
            ForceDirectories(PrgDataDir() + LanguageSubDir + PathDelim);
            DestLangFile = PrgDataDir() + LanguageSubDir + PathDelim + FileName;
            if (FileExists(DestLangFile)) ExtDeleteFile(DestLangFile, ftTemp);
            RenameFile(PackageDB->DBDir() + FileName, DestLangFile);
            TIniFile Ini(DestLangFile);
            Ini.WriteString("LanguageFileInfo", "MaxVersion", static_cast<TDownloadLanguageData *>(DownloadLanguageData[I])->MaxVersion());
            Ini.WriteString("LanguageFileInfo", "DownloadChecksum", DownloadLanguageData[I]->PackageChecksum());
        } else {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadLanguageData[I]->URL()}), mtError, mbOK, 0);
        }
    }
    if (C == 1) {
        if (MessageDlg(LanguageSetup->PackageManagerLanguageDownloadedSingle(), mtConfirmation, mbYes | mbNo, 0) == mrYes) {
            LanguageToActivate = DestLangFile;
            Close();
            Finally();
            return;
        }
    }
    if (C > 0) MessageDlg(LanguageSetup->PackageManagerLanguageDownloaded(), mtInformation, mbOK, 0);
    Finally();
}

void TPackageManagerForm::ExePackagesListBoxClick(QObject * /*Sender*/)
{
    const int ItemIndex = ui.ExePackagesListBox->currentRow();
    ui.ExePackagesMemo->clear();
    if (ItemIndex >= 0) {
        TStringList *St = ExePackageInfo(ListBoxObject(ui.ExePackagesListBox, ItemIndex), PackageDBCache);
        for (int I = 0; I < St->Count(); I++) ui.ExePackagesMemo->append((*St)[I]);
        delete St;
    }
    ui.ExePackagesButton->setEnabled(ItemIndex >= 0);

    if (ItemIndex >= 0) {
        void *O = ListBoxObject(ui.ExePackagesListBox, ItemIndex);
        if (IsCachedPackage(PackageDBCache, O)) {
            ui.ExePackagesDelButton->setEnabled(true);
        } else {
            auto *DownloadExeData = static_cast<TDownloadExeData *>(static_cast<TDownloadData *>(O));
            ui.ExePackagesDelButton->setEnabled(!PackageDBCache->FindChecksum(DownloadExeData->Name()).isEmpty());
        }
    } else {
        ui.ExePackagesDelButton->setEnabled(false);
    }
}

void TPackageManagerForm::ExePackagesButtonClick(QObject * /*Sender*/)
{
    TDownloadExeData *DownloadExeData;
    QString FileName;
    TCachedPackage *CachedPackage;
    QString OldChecksum;
    bool OK;

    const int ItemIndex = ui.ExePackagesListBox->currentRow();
    if (ItemIndex < 0) return;

    void *O = ListBoxObject(ui.ExePackagesListBox, ItemIndex);
    if (IsCachedPackage(PackageDBCache, O)) {
        CachedPackage = static_cast<TCachedPackage *>(O);
        ShellOpen(CachedPackage->Filename(), "/D=" + PrgDir());
        Close();
        return;
    }

    DownloadExeData = static_cast<TDownloadExeData *>(static_cast<TDownloadData *>(O));

    ProviderAction(QList<TDownloadData *>{DownloadExeData});

    OldChecksum = PackageDBCache->FindChecksum(DownloadExeData->Name());
    FileName = ExtractFileNameFromURL(DownloadExeData->URL(), ".exe", true);
    if (DownloadExeData->PackageChecksum() != OldChecksum) {
        setEnabled(false);
        OK = (DownloadFileWithDialog(this, DownloadExeData->Size(), DownloadExeData->PackageFileURL(), DownloadExeData->URL(), DownloadExeData->PackageList()->Referer(), PackageDB->DBDir() + FileName) == drSuccess);
        setEnabled(true);
        if (OK) {
            PackageDBCache->DelCache(DownloadExeData->Name());
            RenameFile(PackageDB->DBDir() + FileName, PackageDBCache->DBDir() + FileName);
            PackageDBCache->AddCache(DownloadExeData->Name(), DownloadExeData->Description(), PackageDBCache->DBDir() + FileName);
        } else {
            if (OldChecksum.isEmpty()) {
                MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {DownloadExeData->URL()}), mtError, mbOK, 0);
                return;
            } else {
                if (MessageDlg(LanguageSetup->PackageManagerDownloadFailedUseOldVersion(), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                CachedPackage = PackageDBCache->FindPackage(DownloadExeData->Name()); if (CachedPackage == nullptr) return;
                ShellOpen(CachedPackage->Filename(), "/D=" + PrgDir());
                Close();
                return;
            }
        }
    }

    // TODO(port): the packages are Windows installers (NSIS, "/D=" = destination folder)
    ShellOpen(PackageDBCache->DBDir() + FileName, "/D=" + PrgDir());
    Close();
}

void TPackageManagerForm::ExePackagesDelButtonClick(QObject * /*Sender*/)
{
    const int ItemIndex = ui.ExePackagesListBox->currentRow();
    if (ItemIndex < 0) return;
    void *O = ListBoxObject(ui.ExePackagesListBox, ItemIndex);
    if (IsCachedPackage(PackageDBCache, O)) {
        PackageDBCache->DelCache(static_cast<TCachedPackage *>(O));
    } else {
        // O is TDownloadExeData
        TCachedPackage *P = PackageDBCache->FindPackage(static_cast<TDownloadData *>(O)->Name());
        PackageDBCache->DelCache(P);
    }
    LoadLists();
}

void TPackageManagerForm::FormKeyDown(QObject * /*Sender*/, quint16 &Key, TShiftState Shift)
{
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) ButtonWork(ui.HelpButton);
}

// global

QString ShowPackageManagerDialog(QWidget *AOwner, TGameDB *AGameDB, TGameDB *AAutoSetupDB)
{
    QString result;
    PackageManagerForm = new TPackageManagerForm(AOwner);
    PackageManagerForm->GameDB = AGameDB;
    PackageManagerForm->AutoSetupDB = AAutoSetupDB;
    PackageManagerForm->HideIfAlreadyInstalled = !ShiftPressed();
    PackageManagerForm->ShowModal();
    result = PackageManagerForm->LanguageToActivate;
    delete PackageManagerForm;
    PackageManagerForm = nullptr;
    return result;
}

TGame *DonwloadAndInstall(QWidget *Owner, const QString &URL, TGameDB *GameDB)
{
    TGame *result = nullptr;
    TDownloadResult B;
    QString TempFile = TempDir() + ChangeFileExt(MakeFileSysOKFolderName(URL), ".zip");
    if (Pos("METALINK.PHP", ExtUpperCase(URL)) > 0) {
        B = MetaLinkDownload(Owner, 0, "", URL, "automatic", TempFile);
    } else {
        B = DownloadFileWithDialog(Owner, 0, "", URL, "automatic", TempFile);
    }
    if (B == drCancel) return result;
    if (B != drSuccess) {
        MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {URL}), mtError, mbOK, 0);
        return result;
    }
    result = ImportZipPackage(Owner, TempFile, GameDB, false);
    ExtDeleteFile(TempFile, ftTemp);
    return result;
}
