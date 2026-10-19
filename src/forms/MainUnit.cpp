// C++ port of MainUnit.pas (implementation section).
#include "MainUnit.h"
#include "ColorScheme.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>
#include <QMimeData>
#include <QSystemTrayIcon>
#include <QFileSystemWatcher>

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "SetupDosBoxFormUnit.h"
#include "ProfileEditorFormUnit.h"
#include "SetupFormUnit.h"
#include "IconManagerFormUnit.h"
#include "DosBoxUnit.h"
#include "HistoryFormUnit.h"
#include "TemplateFormUnit.h"
#include "UninstallFormUnit.h"
#include "ViewImageFormUnit.h"
#include "UninstallSelectFormUnit.h"
#include "CreateConfFormUnit.h"
#include "WizardFormUnit.h"
#include "CreateShortcutFormUnit.h"
#include "CreateImageUnit.h"
#include "InfoFormUnit.h"
#include "TransferFormUnit.h"
#include "BuildInstallerFormUnit.h"
#include "BuildInstallerForSingleGameFormUnit.h"
#include "ModernProfileEditorFormUnit.h"
#include "ListViewImageUnit.h"
#include "StatisticsFormUnit.h"
#include "CacheChooseFormUnit.h"
#include "IconLoaderUnit.h"
#include "LanguageEditorStartFormUnit.h"
#include "LanguageEditorFormUnit.h"
#include "PlaySoundFormUnit.h"
#include "WallpaperStyleFormUnit.h"
#include "CreateISOImageFormUnit.h"
#include "CreateXMLFormUnit.h"
#include "ExpandImageFormUnit.h"
#include "FirstRunWizardFormUnit.h"
#include "CommonComponents.h"
#include "BuildImageFromFolderFormUnit.h"
#include "MiniRunFormUnit.h"
#include "ScummVMUnit.h"
#include "ListScummVMGamesFormUnit.h"
#include "DragNDropErrorFormUnit.h"
#include "SimpleXMLUnit.h"
#include "OperationModeInfoFormUnit.h"
#include "ZipManagerUnit.h"
#include "ZipWaitInfoFormUnit.h"
#include "CopyProfileFormUnit.h"
#include "PlayVideoFormUnit.h"
#include "ImageCacheUnit.h"
#include "ZipPackageUnit.h"
#include "WindowsProfileUnit.h"
#include "HelpConsts.h"
#include "BuildZipPackagesFormUnit.h"
#include "DOSBoxCountUnit.h"
#include "QuickStartFormUnit.h"
#include "ClassExtensions.h"
#include "SetupFrameWaveEncoderUnit.h"
#include "ZipInfoFormUnit.h"
#include "ScanGamesFolderFormUnit.h"
#include "ScummVMToolsUnit.h"
#include "CreateShortcutsFormUnit.h"
#include "PackageManagerFormUnit.h"
#include "PackageBuilderUnit.h"
#include "RunPrgManagerUnit.h"
#include "PackageCreationFormUnit.h"
#include "TextEditPopupUnit.h"
#include "MultipleProfilesEditorFormUnit.h"
#include "InstallationSupportFormUnit.h"
#include "RenameAllScreenshotsFormUnit.h"
#include "MakeBootImageFromProfileFormUnit.h"
#include "CheatApplyFormUnit.h"
#include "CheatDBEditFormUnit.h"
#include "CheatSearchFormUnit.h"
#include "UpdateCheckFormUnit.h"
#include "ProgramUpdateCheckUnit.h"
#include "GameDBFilterUnit.h"
#include "LoggingUnit.h"
#include "DOSBoxFailedFormUnit.h"
#include "DOSBoxLangEditFormUnit.h"
#include "DOSBoxLangStartFormUnit.h"
#include "HistoryUnit.h"
#include "ExportGamesListFormUnit.h"
#include "DOSBoxOutputTestFormUnit.h"
#include "SingleInstanceUnit.h"
#include "SetupFrameZipPrgsUnit.h"
#include "DOSBoxTempUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TDFendRebornMainForm* DFendRebornMainForm = nullptr;
bool MainWindowShowComplete = false;

// ---------------------------------------------------------------------------
// Port helpers
// ---------------------------------------------------------------------------

// Windows constants used by the original code
constexpr long cS_OK = 0, cS_FALSE = 1;
constexpr int cDROPEFFECT_NONE = 0, cDROPEFFECT_COPY = 1, cDROPEFFECT_LINK = 4;
constexpr THandle cINVALID_HANDLE_VALUE = -1;

// (Sender as TComponent).Tag
static int Tag(QObject *O)
{
    return O ? O->property("tag").toInt() : 0;
}

static void SetTag(QObject *O, int T)
{
    O->setProperty("tag", T);
}

// TMenuItem.Parent for menu items created at runtime
static QAction *ParentItem(QAction *A)
{
    if (!A) return nullptr;
    return qobject_cast<QAction *>(A->property("vclParentItem").value<QObject *>());
}

// TMenuItem with sub items: the QAction gets a QMenu when the first item is added
static QMenu *ItemMenu(QAction *A)
{
    if (!A->menu()) {
        auto *M = new QMenu();
        M->setToolTipsVisible(true);
        A->setMenu(M);
        QObject::connect(A, &QObject::destroyed, M, &QObject::deleteLater);
    }
    return A->menu();
}

// TMenuItem.Count
static int MenuItemCount(QAction *A)
{
    return A->menu() ? A->menu()->actions().count() : 0;
}

// TMenuItem.Clear / while Count>0 do Items[0].Free
static void ClearMenuItem(QAction *A)
{
    QMenu *M = A->menu();
    if (!M) return;
    A->setMenu(static_cast<QMenu *>(nullptr));
    const QList<QAction *> L = M->actions();
    for (QAction *X : L) { M->removeAction(X); delete X; }
    M->deleteLater();
}

// TMenuItem.Create + Caption/Tag/ImageIndex + Parent.Add
static QAction *AddMenuItem(QAction *Parent, const QString &Caption, int ATag, int ImageIndex, TImageList *Images)
{
    QMenu *M = ItemMenu(Parent);
    auto *A = new QAction(Caption, M);
    SetTag(A, ATag);
    A->setProperty("vclParentItem", QVariant::fromValue<QObject *>(Parent));
    A->setProperty("vclImageIndex", ImageIndex);
    if (ImageIndex >= 0 && Images && ImageIndex < Images->Count()) A->setIcon(Images->Icon(ImageIndex));
    M->addAction(A);
    return A;
}

// (Sender as TMenuItem).Hint
static QString ActionHint(QObject *Sender)
{
    if (!Sender) return QString();
    QVariant V = Sender->property("hint");
    if (V.isValid()) return V.toString();
    auto *A = qobject_cast<QAction *>(Sender);
    if (!A) return QString();
    // QAction::toolTip() returns the text when no tool tip was set
    QString T = A->text();
    T.remove('&');
    if (A->toolTip() == T) return QString();
    return A->toolTip();
}

// SetProcessWorkingSetSize(GetCurrentProcess,$ffffffff,$ffffffff)
static void TrimWorkingSet()
{
#ifdef Q_OS_WIN
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
}

// TTreeView.Items[I] (all nodes in tree order)
static void CollectTreeItems(QTreeWidgetItem *Item, QList<QTreeWidgetItem *> &L)
{
    L << Item;
    for (int I = 0; I < Item->childCount(); I++) CollectTreeItems(Item->child(I), L);
}

static QList<QTreeWidgetItem *> AllTreeItems(QTreeWidget *T)
{
    QList<QTreeWidgetItem *> L;
    for (int I = 0; I < T->topLevelItemCount(); I++) CollectTreeItems(T->topLevelItem(I), L);
    return L;
}

static QColor ColorFromSetting(const QString &S, TColor Default, bool &IsColor)
{
    TColor C;
    IsColor = true;
    if (Trim(S).isEmpty()) return TColorToQColor(Default);
    if (TryStringToColor(Trim(S), C)) return TColorToQColor(C);
    IsColor = false;
    return TColorToQColor(Default);
}

static void SetFontSize(QWidget *W, int FontSize)
{
    if (FontSize <= 0) return;
    QFont F = W->font();
    F.setPointSize(FontSize);
    W->setFont(F);
}

static TGame *GameOf(TListItem *Item)
{
    return Item ? static_cast<TGame *>(Item->Data()) : nullptr;
}

// Watches a folder and all sub folders (FindFirstChangeNotification(...,True,...))
static void AddDirTreeToWatcher(QFileSystemWatcher *W, const QString &Dir)
{
    if (!QDir(Dir).exists()) return;
    QStringList L;
    L << Dir;
    QDirIterator It(Dir, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (It.hasNext()) L << It.next();
    QStringList Known = W->directories();
    QStringList New;
    for (const QString &S : L) if (!Known.contains(S)) New << S;
    if (!New.isEmpty()) W->addPaths(New);
}

// ---------------------------------------------------------------------------

static int LastDOSBoxCount = 0;
static int LastLeft = -1;
static int LastTop = -1;

TDFendRebornMainForm::TDFendRebornMainForm(QWidget *AOwner) : QMainWindow(AOwner)
{
    DFendRebornMainForm = this;
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // the operating system switched between light and dark colours: the colours of the lists
    // (copied from the palette when they are not set in the program options) are loaded again
    AddColorSchemeChangedHandler(this, [this]() { LoadGUISetup(false); });

    // The application ends when the main form is closed (Delphi), not when the
    // last visible window is closed (the main form may be hidden in the tray).
    qApp->setQuitOnLastWindowClosed(false);

    // --- non visual components of the DFM
    ImageList = TImageList::FromResource("MainUnit", "ImageList");
    if (!ImageList) ImageList = new TImageList(16, 16);
    GameNotesImageList = TImageList::FromResource("MainUnit", "GameNotesImageList");
    if (!GameNotesImageList) GameNotesImageList = new TImageList(16, 16);
    ListviewImageList = new TImageList(16, 16);
    ListviewIconImageList = new TImageList(32, 32);
    ScreenshotImageList = new TImageList(160, 100);
    ListviewScreenshotImageList = new TImageList(100, 100);

    ui.ListView->SetSmallImages(ListviewImageList);
    ui.ListView->SetLargeImages(ListviewIconImageList);
    ui.ScreenshotListView->SetLargeImages(ScreenshotImageList);
    ui.SoundListView->SetSmallImages(ImageList);
    ui.VideoListView->SetSmallImages(ImageList);

    // ImageIndex of the menu items / tool buttons (for icon set changes)
    const QList<QPair<QAction*,int>> ActionImages = {
        {ui.ButtonClose, 7}, {ui.ButtonRun, 1}, {ui.ButtonRunSetup, 2}, {ui.ButtonAdd, 3},
        {ui.ButtonEdit, 4}, {ui.ButtonDelete, 5}, {ui.ButtonHelp, 17},
        {ui.PopupRunGame, 1}, {ui.PopupRunSetup, 2}, {ui.PopupEdit, 4}, {ui.PopupCopy, 15},
        {ui.PopupDelete, 5}, {ui.PopupMarkAsFavorite, 9}, {ui.PopupMakeInstaller, 25},
        {ui.PopupMakeZipArchive, 39}, {ui.PopupViewConfFile, 27}, {ui.PopupViewINIFile, 27},
        {ui.PopupCreateShortcut, 19}, {ui.PopupOpenFolder, 11}, {ui.PopupOpenFolderDOSBox, 16},
        {ui.PopupOpenCaptureFolder, 35}, {ui.PopupOpenDataFolder, 13}, {ui.PopupOpenFileInProgramFolder, 23},
        {ui.PopupOpenFileInDataFolder, 23}, {ui.PopupWWW, 20},
        {ui.ScreenshotPopupOpen, 28}, {ui.ScreenshotPopupOpenCaptureFolder, 11}, {ui.ScreenshotPopupImport, 21},
        {ui.ScreenshotPopupCopy, 29}, {ui.ScreenshotPopupSave, 30}, {ui.ScreenshotPopupDelete, 5},
        {ui.ScreenshotPopupUseAsBackground, 35},
        {ui.AddButtonMenuAdd, 16}, {ui.AddButtonMenuAddScummVM, 37}, {ui.AddButtonMenuAddWithWizard, 41},
        {ui.AddButtonMenuImportZip, 39}, {ui.AddButtonMenuAddInstallationSupport, 0},
        {ui.AddButtonMenuDownloadPackages, 31}, {ui.AddButtonMenuAddGetGames, 17},
        {ui.SoundPopupOpen, 33}, {ui.SoundPopupOpenCaptureFolder, 11}, {ui.SoundPopupImport, 21},
        {ui.SoundPopupSave, 30}, {ui.SoundPopupDelete, 5},
        {ui.TrayIconPopupRun, 1}, {ui.TrayIconPopupAddProfile, 16}, {ui.TrayIconPopupAddScummVMProfile, 37},
        {ui.TrayIconPopupClose, 7},
        {ui.VideoPopupOpen, 38}, {ui.VideoPopupOpenCaptureFolder, 11}, {ui.VideoPopupImport, 21},
        {ui.VideoPopupSave, 30}, {ui.VideoPopupDelete, 5},
        {ui.MenuFileImportConfFile, 16}, {ui.MenuFileImportProfFile, 34}, {ui.MenuFileImportZIPFile, 39},
        {ui.MenuFileImportFolder, 11}, {ui.MenuFileImportDownload, 31}, {ui.MenuFileExportGamesList, 32},
        {ui.MenuFileCreateConf, 16}, {ui.MenuFileCreateProf, 34}, {ui.MenuFileCreateZIP, 39},
        {ui.MenuFileCreateInstaller, 25}, {ui.MenuFileExportBuildZipPackages, 39},
        {ui.MenuFileExportBuildInstaller, 25}, {ui.MenuFileExportTransferProfiles, 15},
        {ui.MenuFileSetup, 6}, {ui.MenuFileQuit, 7},
        {ui.MenuRunGame, 1}, {ui.MenuRunSetup, 2}, {ui.MenuRunRunDosBox, 16}, {ui.MenuRunRunDosBoxKeyMapper, 18},
        {ui.MenuRunOpenDosBoxConfig, 27}, {ui.MenuRunRunScummVM, 37}, {ui.MenuRunOpenScummVMConfig, 27},
        {ui.MenuProfileAdd, 16}, {ui.MenuProfileAddScummVM, 37}, {ui.MenuProfileAddWithWizard, 41},
        {ui.MenuProfileImportZip, 39}, {ui.MenuProfileAddInstallationSupport, 0},
        {ui.MenuProfileDownloadPackages, 31}, {ui.MenuProfileEdit, 4}, {ui.MenuProfileCopy, 15},
        {ui.MenuProfileDelete, 5}, {ui.MenuProfileMarkAsFavorite, 9}, {ui.MenuProfileMakeInstaller, 25},
        {ui.MenuProfileMakeZipArchive, 39}, {ui.MenuProfileViewConfFile, 27}, {ui.MenuProfileViewIniFile, 27},
        {ui.MenuProfileCreateShortcut, 19}, {ui.MenuProfileOpenFolder, 11}, {ui.MenuProfileOpenFolderDOSBox, 16},
        {ui.MenuProfileOpenCaptureFolder, 35}, {ui.MenuProfileOpenDataFolder, 13},
        {ui.MenuProfileOpenFileInProgramFolder, 23}, {ui.MenuProfileOpenFileInDataFolder, 23},
        {ui.MenuProfileWWW, 20},
        {ui.MenuExtrasIconManager, 42}, {ui.MenuExtrasViewLogs, 26}, {ui.MenuExtrasOpenGamesFolder, 11},
        {ui.MenuExtrasCreateImage, 14}, {ui.MenuExtrasCreateIMGImage, 0}, {ui.MenuExtrasImageFromFolder, 11},
        {ui.MenuExtrasCreateISOImage, 36}, {ui.MenuExtrasChangeProfiles, 4}, {ui.MenuExtrasTransferProfiles, 15},
        {ui.MenuExtrasCreateShortcuts, 19},
        {ui.MenuHelpHelp, 17}, {ui.MenuHelpHelpContent, 13}, {ui.MenuHelpHomepage, 20}, {ui.MenuHelpForum, 45},
        {ui.MenuHelpDosBoxReadme, 13}, {ui.MenuHelpDosBoxHomepage, 20}, {ui.MenuHelpScummVMReadme, 13},
        {ui.MenuHelpScummVMHomepage, 20}, {ui.MenuHelpStatistics, 32}, {ui.MenuHelpUpdates, 31}
    };
    for (const auto &P : ActionImages) P.first->setProperty("vclImageIndex", P.second);
    ui.MenuProfileDeinstall->setProperty("vclImageIndex", -1);
    ui.PopupDeinstall->setProperty("vclImageIndex", -1);
    const QList<QPair<QMenu*,int>> MenuImages = {
        {ui.MenuFileImport, 21}, {ui.MenuFileExport, 22}, {ui.MenuExtrasTranslation, 43},
        {ui.MenuHelpDosBox, 16}, {ui.MenuHelpScummVM, 37}
    };
    for (const auto &P : MenuImages) P.first->setProperty("vclImageIndex", P.second);
    const QList<QPair<QWidget*,int>> TabImages = {
        {ui.CaptureScreenshotsTab, 35}, {ui.CaptureSoundTab, 33}, {ui.CaptureVideoTab, 38},
        {ui.GameNotesPanel, 44}, {ui.DataFilesTab, 13}
    };
    for (const auto &P : TabImages) P.first->setProperty("vclImageIndex", P.second);
    ui.GameNotesCutButton->setProperty("vclGameNotesImageIndex", 0);
    ui.GameNotesCopyButton->setProperty("vclGameNotesImageIndex", 1);
    ui.GameNotesPasteButton->setProperty("vclGameNotesImageIndex", 2);
    ui.GameNotesUndoButton->setProperty("vclGameNotesImageIndex", 3);

    TrayIcon = new QSystemTrayIcon(this);
    TrayIcon->setToolTip("D-Fend Reborn");
    TrayIcon->setIcon(windowIcon().isNull() ? qApp->windowIcon() : windowIcon());
    TrayIcon->setContextMenu(ui.TrayIconPopupMenu);
    connect(TrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason R) {
        if (R == QSystemTrayIcon::DoubleClick) TrayIconDblClick(TrayIcon);
#ifndef Q_OS_WIN
        // many Linux/macOS tray hosts only report single clicks
        else if (R == QSystemTrayIcon::Trigger) TrayIconDblClick(TrayIcon);
#endif
    });

    IdleAddonTimer = new QTimer(this);
    IdleAddonTimer->setInterval(500);
    connect(IdleAddonTimer, &QTimer::timeout, this, [this]() { TTraceScope T("IdleAddonTimer", 50); IdleAddonTimerTimer(IdleAddonTimer); });
    IdleAddonTimer->start();

    SearchEditTimer = new QTimer(this);
    SearchEditTimer->setInterval(1500);
    connect(SearchEditTimer, &QTimer::timeout, this, [this]() { TTraceScope T("SearchEditTimer", 50); SearchEditTimerTimer(SearchEditTimer); });

    // TApplicationEvents.OnIdle: called regularly while the application is idle
    ApplicationEventsIdleTimer = new QTimer(this);
    ApplicationEventsIdleTimer->setInterval(100);
    connect(ApplicationEventsIdleTimer, &QTimer::timeout, this, [this]() {
        if (!FApplicationEventsOnIdle) return;
        if (QApplication::activePopupWidget()) return;
        bool Done = true;
        TTraceScope T("ApplicationEventsIdle", 50);
        ApplicationEventsIdle(ApplicationEventsIdleTimer, Done);
    });
    ApplicationEventsIdleTimer->start();

    // ApplicationEvents.OnHelp (must be assigned before InitHTMLHelp is called)
    ApplicationOnHelp = [this](quint16 C, qintptr D, bool &CH) { return ApplicationEventsHelp(C, int(D), CH); };

    // node images of the tree view (read by InitTreeViewForGamesList)
    ui.TreeView->setProperty("vclImageList", QVariant::fromValue<void *>(ImageList));

    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "conf";
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "txt";
    ScreenshotSaveDialog = new TSaveDialog(this);
    ScreenshotSaveDialog->DefaultExt = "png";

    hScreenshotsChangeNotification = cINVALID_HANDLE_VALUE;
    hConfsChangeNotification = cINVALID_HANDLE_VALUE;

    // --- DFM event wiring
    const QList<QAction *> MenuWorkActions = {
        ui.ButtonClose, ui.ButtonRun, ui.ButtonRunSetup, ui.ButtonAdd, ui.ButtonEdit, ui.ButtonDelete, ui.ButtonHelp,
        ui.MenuFileImportConfFile, ui.MenuFileImportProfFile, ui.MenuFileImportXMLFile, ui.MenuFileImportZIPFile,
        ui.MenuFileImportZIPFileWithInstallationSupport, ui.MenuFileImportFolder, ui.MenuFileImportFolderWithInstallationSupport,
        ui.MenuFileImportDownload, ui.MenuFileExportGamesList, ui.MenuFileCreateConf, ui.MenuFileCreateProf,
        ui.MenuFileCreateXML, ui.MenuFileCreateZIP, ui.MenuFileCreateInstaller, ui.MenuFileExportBuildZipPackages,
        ui.MenuFileExportBuildInstaller, ui.MenuFileExportTransferProfiles, ui.MenuFileCreateXMlPackageList,
        ui.MenuFileSetup, ui.MenuFileQuit, ui.MenuViewShowMenubar, ui.MenuViewShowToolbar, ui.MenuViewsShowTree,
        ui.MenuViewsShowScreenshots, ui.MenuViewsShowSearchBox, ui.MenuViewsShowExtraInfo, ui.MenuViewsShowTooltips,
        ui.MenuViewsSimpleListNoIcons, ui.MenuViewsSimpleList, ui.MenuViewsListNoIcons, ui.MenuViewsList,
        ui.MenuViewsSmallIcons, ui.MenuViewsIcons, ui.MenuViewsScreenshots, ui.MenuViewsQuickStarter,
        ui.MenuRunGame, ui.MenuRunSetup, ui.MenuRunRunDosBox, ui.MenuRunRunDosBoxKeyMapper, ui.MenuRunOpenDosBoxConfig,
        ui.MenuRunDOSBoxOutputTest, ui.MenuRunRunScummVM, ui.MenuRunOpenScummVMConfig,
        ui.MenuProfileAdd, ui.MenuProfileAddScummVM, ui.MenuProfileAddWindows, ui.MenuProfileAddWithWizard,
        ui.MenuProfileImportZip, ui.MenuProfileAddInstallationSupport, ui.MenuProfileDownloadPackages,
        ui.MenuProfileEdit, ui.MenuProfileCopy, ui.MenuProfileDelete, ui.MenuProfileDeinstall,
        ui.MenuProfileMarkAsFavorite, ui.MenuProfileMakeInstaller, ui.MenuProfileMakeZipArchive,
        ui.MenuProfileViewConfFile, ui.MenuProfileViewIniFile, ui.MenuProfileCreateShortcut, ui.MenuProfileOpenFolder,
        ui.MenuProfileOpenFolderDOSBox, ui.MenuProfileOpenCaptureFolder, ui.MenuProfileOpenDataFolder,
        ui.MenuProfileCheating,
        ui.MenuExtrasIconManager, ui.MenuExtrasViewLogs, ui.MenuExtrasOpenGamesFolder, ui.MenuExtrasTemplates,
        ui.MenuExtrasCheatingApply, ui.MenuExtrasCheatingEdit, ui.MenuExtrasCheatingSearch, ui.MenuExtrasCreateImage,
        ui.MenuExtrasCreateIMGImage, ui.MenuExtrasWriteIMGImage, ui.MenuExtrasImageFromFolder, ui.MenuExtrasExtractImage,
        ui.MenuExtrasCreateISOImage, ui.MenuExtrasImageFromProfile, ui.MenuExtrasScanGamesFolder,
        ui.MenuExtrasCheckProfiles, ui.MenuExtrasChangeProfiles, ui.MenuExtrasDeinstallMultipleGames,
        ui.MenuExtrasTransferProfiles, ui.MenuExtrasCreateShortcuts,
        ui.MenuExtrasTranslationDFendReborn, ui.MenuExtrasTranslationDOSBox,
        ui.MenuHelpHelp, ui.MenuHelpHelpContent, ui.MenuHelpIntroduction, ui.MenuHelpGetGames, ui.MenuHelpAddingGames,
        ui.MenuHelpHomepage, ui.MenuHelpForum, ui.MenuHelpDosBoxFAQ, ui.MenuHelpDosBoxIntro, ui.MenuHelpDosBoxIntroDosBox,
        ui.MenuHelpDosBoxHotkeys, ui.MenuHelpDosBoxHotkeysDosbox, ui.MenuHelpDosBoxHomepage,
        ui.MenuHelpDosBoxCompatibility, ui.MenuHelpDemoCompatibility, ui.MenuHelpScummVMReadme, ui.MenuHelpScummVMFAQ,
        ui.MenuHelpScummVMHomepage, ui.MenuHelpScummVMCompatibility, ui.MenuHelpScummVMCompatibilityIntern,
        ui.MenuHelpStatistics, ui.MenuHelpOperationMode, ui.MenuHelpUpdates, ui.MenuHelpAbout,
        ui.PopupRunGame, ui.PopupRunSetup, ui.PopupEdit, ui.PopupCopy, ui.PopupDelete, ui.PopupDeinstall,
        ui.PopupMarkAsFavorite, ui.PopupMakeInstaller, ui.PopupMakeZipArchive, ui.PopupViewConfFile,
        ui.PopupViewINIFile, ui.PopupCreateShortcut, ui.PopupOpenFolder, ui.PopupOpenFolderDOSBox,
        ui.PopupOpenCaptureFolder, ui.PopupOpenDataFolder, ui.PopupCheating, ui.PopupSimpleListNoIcons,
        ui.PopupSimpleList, ui.PopupListNoIcons, ui.PopupList, ui.PopupSmallIcons, ui.PopupIcons, ui.PopupScreenshots,
        ui.ScreenshotPopupOpenCaptureFolder,
        ui.AddButtonMenuAdd, ui.AddButtonMenuAddScummVM, ui.AddButtonMenuAddWindows, ui.AddButtonMenuAddFromTemplate,
        ui.AddButtonMenuAddWithWizard, ui.AddButtonMenuImportZip, ui.AddButtonMenuAddInstallationSupport,
        ui.AddButtonMenuDownloadPackages, ui.AddButtonMenuAddGetGames,
        ui.SoundPopupOpenCaptureFolder, ui.VideoPopupOpenCaptureFolder
    };
    for (QAction *A : MenuWorkActions) connect(A, &QAction::triggered, this, [this, A]() { MenuWork(A); });

    const QList<QAction *> ScreenshotActions = {
        ui.ScreenshotPopupOpen, ui.ScreenshotPopupOpenExternal, ui.ScreenshotPopupRefresh, ui.ScreenshotPopupImport,
        ui.ScreenshotPopupCopy, ui.ScreenshotPopupSave, ui.ScreenshotPopupRename, ui.ScreenshotPopupRenameAll,
        ui.ScreenshotPopupDelete, ui.ScreenshotPopupDeleteAll, ui.ScreenshotPopupUseAsBackground,
        ui.ScreenshotPopupUseInScreenshotList
    };
    for (QAction *A : ScreenshotActions) connect(A, &QAction::triggered, this, [this, A]() { ScreenshotMenuWork(A); });

    const QList<QAction *> SoundActions = {
        ui.SoundPopupOpen, ui.SoundPopupOpenExternal, ui.SoundPopupRefresh, ui.SoundPopupImport, ui.SoundPopupSave,
        ui.SoundPopupSaveMp3, ui.SoundPopupSaveOgg, ui.SoundPopupRename, ui.SoundPopupRenameAll, ui.SoundPopupDelete,
        ui.SoundPopupDeleteAll, ui.SoundPopupSaveMp3All, ui.SoundPopupSaveOggAll
    };
    for (QAction *A : SoundActions) connect(A, &QAction::triggered, this, [this, A]() { SoundMenuWork(A); });

    const QList<QAction *> VideoActions = {
        ui.VideoPopupOpen, ui.VideoPopupOpenExternal, ui.VideoPopupRefresh, ui.VideoPopupImport, ui.VideoPopupSave,
        ui.VideoPopupRename, ui.VideoPopupRenameAll, ui.VideoPopupDelete, ui.VideoPopupDeleteAll
    };
    for (QAction *A : VideoActions) connect(A, &QAction::triggered, this, [this, A]() { VideoMenuWork(A); });

    const QList<QAction *> TrayActions = {
        ui.TrayIconPopupRestore, ui.TrayIconPopupRun, ui.TrayIconPopupAddProfile, ui.TrayIconPopupAddScummVMProfile,
        ui.TrayIconPopupAddWindowsProfile, ui.TrayIconPopupAddWithWizard, ui.TrayIconPopupClose
    };
    for (QAction *A : TrayActions) connect(A, &QAction::triggered, this, [this, A]() { TrayIconPopupClick(A); });

    for (QAction *A : {ui.ToolbarPopupSetupButtons, ui.ToolbarPopupSetupIconSet, ui.ToolbarPopupShowMenubar})
        connect(A, &QAction::triggered, this, [this, A]() { ToolbarPopupClick(A); });

    for (QAction *A : {ui.GameNotesCutButton, ui.GameNotesCopyButton, ui.GameNotesPasteButton, ui.GameNotesUndoButton})
        connect(A, &QAction::triggered, this, [this, A]() { GameNotesButtonWork(A); });

    connect(ui.TreeViewPopupEditUserFilters, &QAction::triggered, this, [this]() { TreeViewPopupEditUserFiltersClick(ui.TreeViewPopupEditUserFilters); });
    connect(ui.TrayIconPopupMenu, &QMenu::aboutToShow, this, [this]() { TrayIconPopupMenuPopup(ui.TrayIconPopupMenu); });
    connect(ui.FirstRunInfoButton, &QToolButton::clicked, this, [this]() { FirstRunInfoButtonClick(ui.FirstRunInfoButton); });

    connect(ui.Splitter, &TSplitterWidget::moved, this, [this]() { FormResize(ui.Splitter); });
    connect(ui.TreeView, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *Current, QTreeWidgetItem *) {
        if (FTreeViewOnChange) TreeViewChange(ui.TreeView, Current);
    });
    connect(ui.SearchEdit, &QLineEdit::textChanged, this, [this]() { if (FSearchEditOnChange) SearchEditChange(ui.SearchEdit); });
    ui.SearchEdit->installEventFilter(this);   // SearchEdit.OnClick

    connect(ui.ListView, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) {
        if (FListViewOnSelectItem) ListViewSelectItem(ui.ListView, Item, Selected);
    });
    connect(ui.ListView, &TListView::dblClick, this, [this]() { ListViewDblClick(ui.ListView); });
    connect(ui.ListView, &TListView::columnClick, this, [this](int Column) { ListViewColumnClick(ui.ListView, ui.ListView->Column(Column)); });
    SetListViewShowHint(true);
    SetListViewCustomDraw(true);

    connect(ui.CapturePageControl, &QTabWidget::currentChanged, this, [this]() { CapturePageControlChange(ui.CapturePageControl); });

    auto KeyOf = [](int Key) { return Key == Qt::Key_Enter ? int(Qt::Key_Return) : Key; };
    auto ShiftOf = [](Qt::KeyboardModifiers M) { return TShiftState(M & ~Qt::KeypadModifier); };
    connect(ui.ScreenshotListView, &TListView::dblClick, this, [this]() { ScreenshotListViewDblClick(ui.ScreenshotListView); });
    connect(ui.ScreenshotListView, &TListView::keyDown, this, [this, KeyOf, ShiftOf](int Key, Qt::KeyboardModifiers M) {
        int K = KeyOf(Key); ScreenshotListViewKeyDown(ui.ScreenshotListView, K, ShiftOf(M));
    });
    connect(ui.ScreenshotListView, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { ScreenshotListViewSelectItem(ui.ScreenshotListView, Item, Selected); });
    connect(ui.SoundListView, &TListView::dblClick, this, [this]() { SoundListViewDblClick(ui.SoundListView); });
    connect(ui.SoundListView, &TListView::keyDown, this, [this, KeyOf, ShiftOf](int Key, Qt::KeyboardModifiers M) {
        int K = KeyOf(Key); SoundListViewKeyDown(ui.SoundListView, K, ShiftOf(M));
    });
    connect(ui.SoundListView, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { SoundListViewSelectItem(ui.SoundListView, Item, Selected); });
    connect(ui.VideoListView, &TListView::dblClick, this, [this]() { VideoListViewDblClick(ui.VideoListView); });
    connect(ui.VideoListView, &TListView::keyDown, this, [this, KeyOf, ShiftOf](int Key, Qt::KeyboardModifiers M) {
        int K = KeyOf(Key); VideoListViewKeyDown(ui.VideoListView, K, ShiftOf(M));
    });
    connect(ui.VideoListView, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { VideoListViewSelectItem(ui.VideoListView, Item, Selected); });

    ui.GameNotesEdit->installEventFilter(this);       // OnEnter / OnExit
    for (QWidget *W : {static_cast<QWidget *>(ui.ZipInfoPanel), static_cast<QWidget *>(ui.ZipInfoLabel),
                       static_cast<QWidget *>(ui.FirstRunInfoPanel), static_cast<QWidget *>(ui.FirstRunInfoLabel)})
        W->installEventFilter(this);                  // OnDblClick

    // KeyPreview=True
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    // WM_DISPLAYCHANGE
    if (QScreen *S = QGuiApplication::primaryScreen())
        connect(S, &QScreen::geometryChanged, this, [this, S]() { DisplayResolutionChange(S->depth()); });

    FormCreate(this);
}

TDFendRebornMainForm::~TDFendRebornMainForm()
{
    if (!FFormCreateAborted) FormDestroy(this);
    if (QuickStartForm) { delete QuickStartForm; QuickStartForm = nullptr; }
    StopCaptureChangeNotify();
    ui.ListView->SetSmallImages(nullptr);
    ui.ListView->SetLargeImages(nullptr);
    ui.ScreenshotListView->SetLargeImages(nullptr);
    ui.SoundListView->SetSmallImages(nullptr);
    ui.VideoListView->SetSmallImages(nullptr);
    if (UserIconLoader) UserIconLoader->UnRegisterImageList(ImageList);
    delete ImageList;
    delete GameNotesImageList;
    delete ListviewImageList;
    delete ListviewIconImageList;
    delete ScreenshotImageList;
    delete ListviewScreenshotImageList;
    delete OpenDialog;
    delete SaveDialog;
    delete ScreenshotSaveDialog;
    if (DFendRebornMainForm == this) DFendRebornMainForm = nullptr;
}

// ---------------------------------------------------------------------------
// Qt event mapping
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::showEvent(QShowEvent *E)
{
    QMainWindow::showEvent(E);
    if (FDesignedPosition) {
        // Position:=poDesigned (set in InitGUI) - undo the poScreenCenter centering
        FDesignedPosition = false;
        move(FDesignedPos);
    }
    FormShow(this);
}

void TDFendRebornMainForm::closeEvent(QCloseEvent *E)
{
    bool CanClose = true;
    FormCloseQuery(this, CanClose);
    if (!CanClose) { E->ignore(); return; }
    TCloseAction Action = caHide;
    FormClose(this, Action);
    if (Action == caNone) { E->ignore(); return; }
    E->accept();
    QMainWindow::closeEvent(E);
    // Closing the main form terminates the application
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void TDFendRebornMainForm::resizeEvent(QResizeEvent *E)
{
    QMainWindow::resizeEvent(E);
    FormResize(this);
}

void TDFendRebornMainForm::moveEvent(QMoveEvent *E)
{
    QMainWindow::moveEvent(E);
    WMMove();
}

void TDFendRebornMainForm::changeEvent(QEvent *E)
{
    QMainWindow::changeEvent(E);
    if (E->type() == QEvent::WindowStateChange) {
        bool Min = isMinimized();
        if (Min && !FWasMinimized) { FWasMinimized = true; ApplicationEventsMinimize(this); }
        else if (!Min && FWasMinimized) { FWasMinimized = false; ApplicationEventsRestore(this); }
    }
}

bool TDFendRebornMainForm::eventFilter(QObject *O, QEvent *E)
{
    if (O == ui.SearchEdit && E->type() == QEvent::MouseButtonPress) {
        SearchEditChange(ui.SearchEdit);   // SearchEdit.OnClick
    }
    if (O == ui.GameNotesEdit) {
        if (E->type() == QEvent::FocusIn) GameNotesEditEnter(ui.GameNotesEdit);
        if (E->type() == QEvent::FocusOut) GameNotesEditExit(ui.GameNotesEdit);
    }
    if (E->type() == QEvent::MouseButtonDblClick &&
        (O == ui.ZipInfoPanel || O == ui.ZipInfoLabel || O == ui.FirstRunInfoPanel || O == ui.FirstRunInfoLabel)) {
        ZipInfoPanelDblClick(O);
    }
    return QMainWindow::eventFilter(O, E);
}

static int KeyStateOf(QDropEvent *E)
{
    Q_UNUSED(E);
    return 0;
}

void TDFendRebornMainForm::dragEnterEvent(QDragEnterEvent *E)
{
    int Effect = cDROPEFFECT_NONE;
    if (DragEnter(E->mimeData(), KeyStateOf(E), E->position().toPoint(), Effect) == cS_OK) {
        E->setDropAction(Qt::CopyAction);
        E->accept();
    } else {
        E->ignore();
    }
}

void TDFendRebornMainForm::dragMoveEvent(QDragMoveEvent *E)
{
    int Effect = cDROPEFFECT_NONE;
    DragOver(KeyStateOf(E), E->position().toPoint(), Effect);
    if (Effect != cDROPEFFECT_NONE) { E->setDropAction(Qt::CopyAction); E->accept(); } else E->ignore();
}

void TDFendRebornMainForm::dragLeaveEvent(QDragLeaveEvent *E)
{
    DragLeave();
    E->accept();
}

void TDFendRebornMainForm::dropEvent(QDropEvent *E)
{
    int Effect = cDROPEFFECT_NONE;
    QPoint P = E->position().toPoint();
    long R = Drop(E->mimeData(), KeyStateOf(E), P, Effect);
    if (R == 2) {
        // WM_DROPFILES handling of the controls (TListViewAcceptingFiles / TTreeViewAcceptingFiles)
        TStringList FileList;
        for (const QUrl &U : E->mimeData()->urls()) if (U.isLocalFile()) FileList.Add(QDir::toNativeSeparators(U.toLocalFile()));
        if (FileList.Count() == 0) { E->ignore(); return; }
        QWidget *W = childAt(P);
        QWidget *Target = nullptr;
        while (W && W != this) {
            if (W == ui.ListView || W == ui.TreeView || W == ui.ScreenshotListView || W == ui.SoundListView || W == ui.VideoListView) { Target = W; break; }
            W = W->parentWidget();
        }
        if (!Target) { E->ignore(); return; }
        E->acceptProposedAction();
        QPoint LP = Target->mapFrom(this, P);
        if (Target == ui.ListView || Target == ui.TreeView) MainDragDrop(Target, &FileList, LP.x(), LP.y());
        else CaptureDragDrop(Target, &FileList, LP.x(), LP.y());
        return;
    }
    if (R == cS_OK) E->acceptProposedAction(); else E->ignore();
}

// ---------------------------------------------------------------------------
// Port helpers (members)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::SetFormEnabled(bool Enabled)
{
    // Form.Enabled:=False; disabling the QMainWindow itself would also disable
    // the dialogs that use it as parent, so only the contents are disabled.
    ui.centralwidget->setEnabled(Enabled);
    menuBar()->setEnabled(Enabled);
}

void TDFendRebornMainForm::SetSearchEditText(const QString &S)
{
    bool Old = FSearchEditOnChange;
    FSearchEditOnChange = false;
    ui.SearchEdit->setText(S);
    FSearchEditOnChange = Old;
}

void TDFendRebornMainForm::SetSearchEditGray(bool Gray)
{
    FSearchEditGray = Gray;
    QPalette P = ui.SearchEdit->palette();
    P.setColor(QPalette::Text, Gray ? TColorToQColor(clGray) : TColorToQColor(clWindowText));
    ui.SearchEdit->setPalette(P);
}

void TDFendRebornMainForm::SetListViewShowHint(bool B)
{
    if (B) {
        ui.ListView->OnInfoTip = [this](TListItem *Item) {
            QString InfoTip;
            ListViewInfoTip(ui.ListView, Item, InfoTip);
            return InfoTip;
        };
    } else {
        ui.ListView->OnInfoTip = nullptr;
    }
}

void TDFendRebornMainForm::SetListViewCustomDraw(bool B)
{
    if (B) {
        ui.ListView->OnCustomDrawItem = [this](TListItem *Item) {
            bool DefaultDraw = true;
            ListViewAdvancedCustomDrawItem(ui.ListView, Item, 0, 0, DefaultDraw);
        };
    } else {
        ui.ListView->OnCustomDrawItem = nullptr;
    }
}

void TDFendRebornMainForm::UpdateImageListIcons()
{
    // VCL menu items / tool buttons reference the image list by ImageIndex,
    // Qt actions hold a copy of the icon: copy the (changed) images again.
    for (QAction *A : findChildren<QAction *>()) {
        QVariant V = A->property("vclImageIndex");
        if (V.isValid()) {
            int I = V.toInt();
            if (I >= 0 && I < ImageList->Count()) A->setIcon(ImageList->Icon(I)); else if (I < 0) A->setIcon(QIcon());
        }
        V = A->property("vclGameNotesImageIndex");
        if (V.isValid()) {
            int I = V.toInt();
            if (I >= 0 && I < GameNotesImageList->Count()) A->setIcon(GameNotesImageList->Icon(I));
        }
    }
    for (QMenu *M : findChildren<QMenu *>()) {
        QVariant V = M->property("vclImageIndex");
        if (!V.isValid()) continue;
        int I = V.toInt();
        if (I >= 0 && I < ImageList->Count()) M->setIcon(ImageList->Icon(I));
    }
    for (int T = 0; T < ui.CapturePageControl->count(); T++) {
        QVariant V = ui.CapturePageControl->widget(T)->property("vclImageIndex");
        if (!V.isValid()) continue;
        int I = V.toInt();
        if (I >= 0 && I < ImageList->Count()) ui.CapturePageControl->setTabIcon(T, ImageList->Icon(I));
    }
    // The nodes of the games tree hold copies of the icons, too (image index
    // in Qt::UserRole+1, see SetNodeImage in GameDBToolsUnit). The tree is
    // built in FormCreate, before the icon set is loaded in FormShow.
    for (QTreeWidgetItemIterator It(ui.TreeView); *It; ++It) {
        const QVariant V = (*It)->data(0, Qt::UserRole + 1);
        if (!V.isValid()) continue;
        const int I = V.toInt();
        if (I >= 0 && I < ImageList->Count()) (*It)->setIcon(0, ImageList->Icon(I));
    }
}

void TDFendRebornMainForm::ApplicationRestore()
{
    // Application.Restore
    Qt::WindowStates S = windowState() & ~Qt::WindowMinimized;
    setWindowState(S);
    show();
    raise();
    activateWindow();
}

// ---------------------------------------------------------------------------
// FormCreate / FormShow / FormDestroy
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::FormCreate(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S;
    bool B;

    if (PrgSetup->SingleInstance()) {
        if (InstanceRunning(hEvent)) { FFormCreateAborted = true; return; }   // TerminateProcess(GetCurrentProcess,0)
    }

    LogInfo("### Start of FormCreate ###");

    /*Caption:=Caption+' THIS IS A TEST VERSION ! (Beta 1 of version 1.5)';*/
    /*Caption:=Caption+' (Release candidate 1 of version 1.4.4)';*/

    MI_Count = ImageList->Count();
    resize(Min(width(), 790), 790);
    JustStarted = true;

    /*Turn off events so window movements etc. do not slow down the start up*/
    FApplicationEventsOnIdle = false; IdleAddonTimer->stop(); FTreeViewOnChange = false;
    SetListViewCustomDraw(false); FListViewOnSelectItem = false;

    FileConflictCheckRunning = false;
    StartTrayMinimize = false;
    HTMLhelpRouter = nullptr;
    LastSelectedGame = nullptr;
    GameLinkList = nullptr;

    ListSort = slbName;
    ListSortReverse = false;
    OldGamesMenuUsed = false;

    DeleteOnExit = new TStringList();

    ZipManager->SetOnInfoBarNotify([this](QObject *Sender) { ZipInfoNotify(Sender); });

    LogInfo("Setting up capture directory change notify");
    try { ForceDirectories(MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir())); } catch (...) {}
    StartCaptureChangeNotify();

    LogInfo("Loading games data base");
    GameDB = new TGameDB(PrgDataDir() + GameListSubDir);
    ScummVMGamesList = new TScummVMGamesList();

    LogInfo("Loading Links.txt and SearchLinks.txt");
    HelpMenuLinkFile = new TLinkFile("Links.txt", true, ID_HelpOldGames);
    SearchLinkFile = new TLinkFile("SearchLinks.txt", false, ID_ProfileSearchGame);
    // Lists edited by the user: addresses of D-Fend Reloaded that changed
    // (search pages of the sites, https, sites that no longer exist or no
    // longer offer what the link was for)
    HelpMenuLinkFile->UpgradeLinks({
        {QStringLiteral("http://vogons.zetafleet.com"), QStringLiteral("https://www.vogons.org")},
        {QStringLiteral("http://www.mobygames.com"), QStringLiteral("https://www.mobygames.com")},
        {QStringLiteral("http://www.squakenet.com/"), QString()},
        {QStringLiteral("http://www.thelegacy.de/"), QString()},
        {QStringLiteral("http://www.3drealms.com/downloads.html"), QString()},
        {QStringLiteral("http://liberatedgames.org/"), QString()},
        {QStringLiteral("http://en.wikipedia.org/wiki/List_of_commercial_games_released_as_freeware"), QStringLiteral("https://en.wikipedia.org/wiki/List_of_commercial_games_released_as_freeware")},
        {QStringLiteral("http://en.wikipedia.org/wiki/List_of_freeware_games"), QStringLiteral("https://en.wikipedia.org/wiki/List_of_freeware_games")},
        {QStringLiteral("http://en.wikipedia.org/wiki/List_of_open_source_games"), QStringLiteral("https://en.wikipedia.org/wiki/List_of_open_source_games")}
    });
    SearchLinkFile->UpgradeLinks({
        {QStringLiteral("http://thegamesdb.net/search/?string=<GAMENAME>&function=Search"), QStringLiteral("https://thegamesdb.net/search.php?name=<GAMENAME>")},
        {QStringLiteral("http://www.mobygames.com/search/quick?game=<GAMENAME>&p=-1&search=Go"), QStringLiteral("https://www.mobygames.com/search/?q=<GAMENAME>")},
        {QStringLiteral("http://www.squakenet.com/pc_games/?q=<GAMENAME>"), QString()},
        {QStringLiteral("http://www.thelegacy.de/Museum/SQLFrameset.php3?SearchValue=<GAMENAME>&searchEntity=TITLE&misc=yes&quick=yes&list=SQLlist_games.php3"), QString()},
        {QStringLiteral("http://www.google.com/search?q=<GAMENAME>"), QStringLiteral("https://www.google.com/search?q=<GAMENAME>")},
        {QStringLiteral("http://en.wikipedia.org/wiki/<GAMENAME>"), QStringLiteral("https://en.wikipedia.org/w/index.php?search=<GAMENAME>")}
    });

    LogInfo("Initializating of data viewer area");
    ViewFilesFrame = new TViewFilesFrame(ui.DataFilesTab);
    {
        auto *L = new QVBoxLayout(ui.DataFilesTab);   // ViewFilesFrame.Align = alClient
        L->setContentsMargins(0, 0, 0, 0);
        L->setSpacing(0);
        L->addWidget(ViewFilesFrame);
    }
    ViewFilesFrame->SetOnUpdateGamesList([this](QObject *Sender) { UpdateGamesListByViewFilesFrame(Sender); });

    LogInfo("Loading language");
    LoadMenuLanguage();

    LogInfo("Calling InitGUI");
    InitGUI(true);

    LogInfo("Updating template menu");
    UpdateAddFromTemplateMenu();

    if (PrgSetup->FirstRun()) {
        LogInfo("First run: Searching DOSBox");
        if (SearchDosBox(this, S, !PrgSetup->DOSBoxFullSearchDone())) PrgSetup->DOSBoxSettings(0)->SetDosBoxDir(S);
        LogInfo("First run: Initializating templates");
        ReBuildTemplates(true);
        LogInfo("First run: Showing first run wizard");
        B = false;
        ShowFirstRunWizardDialog(this, B);
        if (B) {
            LogInfo("First run: Starting update check");
            RunProgramStartSilentUpdateCheck(this, true);
        }
    }

    LogInfo("Initializing media player library");
    // MediaInterface (external player DLL) is replaced by Qt Multimedia; there
    // is no library that has to be loaded (and no "halt" if it is missing).

    // ToolBar.PopupMenu:=ToolbarPopupMenu
    ui.ToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.ToolBar, &QWidget::customContextMenuRequested, this, [this](const QPoint &P) {
        ui.ToolbarPopupMenu->popup(ui.ToolBar->mapToGlobal(P));
    });

    LogInfo("### End of CreateForm ###\r");
}

void TDFendRebornMainForm::FormShow(QObject *Sender)
{
    Q_UNUSED(Sender);
    TGame *G;
    QString S;

    if (!JustStarted) return;
    JustStarted = false;

    LogInfo("### Start of FormShow ###");
    LastDragDropEffect = cDROPEFFECT_NONE;
    setAcceptDrops(true);   // RegisterDragDrop(Handle,Self); RegisterDragDrop(GameNotesEdit.Handle,Self)

    LogInfo("Setting up application icon");
    if (!qApp->windowIcon().isNull()) setWindowIcon(qApp->windowIcon()); /*Window icon is only 16x16, application icon is larger; Win7 is using the window icon in the taskbar*/

    /*Turn on events again*/
    FApplicationEventsOnIdle = true; IdleAddonTimer->start(); FTreeViewOnChange = true;
    SetListViewCustomDraw(true); FListViewOnSelectItem = true;

    LogInfo("Setting up drag&drop to window components");
    // The list views and the tree view accept dropped files through the
    // form's dropEvent (TListViewAcceptingFiles / TTreeViewAcceptingFiles).

    FTreeViewOnChange = false;

    LogInfo("Loading links and quick searching DOSBox");
    S = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
    // (the full search of all drives is only done once - it can be started
    // again from the program options)
    if (SearchDosBox(this, S, !PrgSetup->DOSBoxFullSearchDone())) PrgSetup->DOSBoxSettings(0)->SetDosBoxDir(S);
    LogInfo("DOSBox folder: " + PrgSetup->DOSBoxSettings(0)->DosBoxDir());
    LoadHelpLinks();
    LoadAbandonLinks();
    LoadSearchLinks();

    LogInfo("Loading GUI icons");
    {
        TStringList St;
        for (int I = 0; I < PrgSetup->WindowsBasedEmulatorsPrograms()->Count(); I++)
            St.Add(MakeAbsPath((*PrgSetup->WindowsBasedEmulatorsPrograms())[I], PrgSetup->BaseDir()));
        UserIconLoader->RegisterImageList(ImageList, "Main", &St, 40);
    }
    UserIconLoader->LoadIcons();
    UpdateImageListIcons();

    if (PrgSetup->FirstRun()) {
        LogInfo("First run: Building default template");
        BuildDefaultProfile();
        LogInfo("First run: Building \"DOSBox DOS\" profile (+ coping files from \"NewUserData\" to profile directory - if not portable)");
        G = BuildDefaultDosProfile(GameDB);
        LogInfo("First run: Searching tools");
        FastSearchAllTools();
        if (OperationMode != omPortable) {
            LogInfo("First run: Searching pack programs");
            FirstRunPackerAutoSetup();
        }
        LogInfo("First run: Load language");
        LoadLanguage(PrgSetup->Language());
        LogInfo("First run: Load menu language");
        LoadMenuLanguage();
        LogInfo("First run: Calling LoadGUISetup");
        LoadGUISetup(false);
        LogInfo("First run: Selecting new \"DOSBox DOS\" profile");
        SelectGame(G);
        PrgSetup->SetDFendVersion(GetNormalFileVersionAsString());
    } else {
        if (PrgSetup->DFendVersion() != GetNormalFileVersionAsString()) {
            LogInfo("First run after update: Updating settings");
            SetFormEnabled(false);
            UpdateUserDataFolderAndSettingsAfterUpgrade(GameDB, VersionToInt(PrgSetup->DFendVersion()));
            SetFormEnabled(true);
            PrgSetup->SetDFendVersion(GetNormalFileVersionAsString());
            LogInfo("First run after update: Searching tools");
            FastSearchAllTools();
            LogInfo("First run after update: Calling LoadGUISetup");
            LoadGUISetup(false);
        } else {
            LogInfo("Check helper program directories");
            FastSearchAllTools();
            LogInfo("Calling LoadGUISetup");
            LoadGUISetup(true);
        }
    }

    ui.FirstRunInfoPanel->setVisible((GameDB->Count() <= 1) && PrgSetup->ShowAddGameInfoOnEmptyGamesList());

    LogInfo("Update checker");
    RunProgramStartSilentUpdateCheck(this, false);

    LogInfo("Setting up data viewer area");
    ViewFilesFrame->Init();

    LogInfo("Deleting old temp files");
    TempZipFilesStartCleanUp();

    QTimer::singleShot(0, this, [this]() { PostShow(); });   // PostMessage(Handle,WM_USER+1,0,0)

    LogInfo("### End of ShowForm ###\r");
}

void TDFendRebornMainForm::FormDestroy(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S1, S2;

    while (FreeDOSInitThreadRunning || TemplatesInitThreadRunning) { Sleep(50); ProcessMessages(); }

    UpdateGameNotes(); LastSelectedGame = nullptr;

    try {
        for (int I = 0; I < DeleteOnExit->Count(); I++) if (FileExists((*DeleteOnExit)[I])) ExtDeleteFile((*DeleteOnExit)[I], ftTemp);
        delete DeleteOnExit; DeleteOnExit = nullptr;
        ExtDeleteFile(PrgDataDir() + PackageDBSubFolder + PathDelim + "DFRTemp.prof", ftTemp);
        ExtDeleteFile(TempDir() + TempSubFolder, ftTemp);
        if (ParamCount() == 0) ExtDeleteFile(TempDir() + DosBoxConfFileName, ftTemp);
    } catch (...) {}

    GamesListSaveColWidths(ui.ListView);
    if (ui.ListView->Selected() != nullptr && GameOf(ui.ListView->Selected())) PrgSetup->SetLastSelectedProfile(GameOf(ui.ListView->Selected())->CacheName); else PrgSetup->SetLastSelectedProfile("");
    StopCaptureChangeNotify();
    PrgSetup->SetMainMaximized(isMaximized());
    QRect R = (isMaximized() || isMinimized()) ? normalGeometry() : geometry();
    if (!R.isValid()) R = geometry();
    PrgSetup->SetMainLeft(pos().x());
    PrgSetup->SetMainTop(pos().y());
    PrgSetup->SetMainWidth(R.width());
    PrgSetup->SetMainHeight(R.height());
    PrgSetup->SetTreeWidth(ui.TreeView->width());
    PrgSetup->SetScreenshotHeight(ui.CapturePageControl->height());

    QTreeWidgetItem *Sel = ui.TreeView->currentItem();
    if (Sel == nullptr) { S1 = ""; S2 = ""; } else {
        if (Sel->parent() == nullptr) {
            S1 = Sel->text(0); S2 = "";
        } else {
            S1 = Sel->parent()->text(0); S2 = Sel->text(0);
        }
    }
    PrgSetup->SetFilterMain(S1);
    PrgSetup->SetFilterSub(S2);
    PrgSetup->SetPreviewerCategory(ui.CapturePageControl->currentIndex());

    FListViewOnSelectItem = false;
    ui.ListView->SetSelected(nullptr);
    if (ViewFilesFrame) ViewFilesFrame->SetGame(nullptr);
    delete GameDB; GameDB = nullptr;
    delete ScummVMGamesList; ScummVMGamesList = nullptr;

    delete HelpMenuLinkFile; HelpMenuLinkFile = nullptr;
    delete SearchLinkFile; SearchLinkFile = nullptr;

    if (GameLinkList) { delete GameLinkList; GameLinkList = nullptr; }

    setAcceptDrops(false);   // RevokeDragDrop

    if (hEvent != 0) CloseInstanceEvent(hEvent);
}

// ---------------------------------------------------------------------------
// Language
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::LoadMenuLanguage()
{
    LogInfo("### Start of LoadMenuLanguage ###");
    // Font.Charset:=CharsetNameToFontCharSet(LanguageSetup.CharsetName) - Qt is Unicode, no charset needed

    ui.MenuFile->setTitle(LanguageSetup->MenuFile());
    ui.MenuFileImport->setTitle(LanguageSetup->MenuFileImport());
    ui.MenuFileImportConfFile->setText(LanguageSetup->MenuFileImportConf());
    ui.MenuFileImportProfFile->setText(LanguageSetup->MenuFileImportProf());
    ui.MenuFileImportZIPFile->setText(LanguageSetup->MenuFileImportZIP());
    ui.MenuFileImportZIPFileWithInstallationSupport->setText(LanguageSetup->MenuFileImportZIPWithInstallationSupport());
    ui.MenuFileImportFolder->setText(LanguageSetup->MenuFileImportFolder());
    ui.MenuFileImportFolderWithInstallationSupport->setText(LanguageSetup->MenuFileImportFolderWithInstallationSupport());
    ui.MenuFileImportDownload->setText(LanguageSetup->MenuFileImportDownload());
    ui.MenuFileExport->setTitle(LanguageSetup->MenuFileExport());
    ui.MenuFileExportGamesList->setText(LanguageSetup->MenuFileExportGamesList());
    ui.MenuFileExportTransferProfiles->setText(LanguageSetup->MenuExtrasTransferProfiles());
    ui.MenuFileCreateConf->setText(LanguageSetup->MenuFileCreateConf());
    ui.MenuFileCreateProf->setText(LanguageSetup->MenuFileCreateProf());
    ui.MenuFileCreateZIP->setText(LanguageSetup->MenuFileCreateZIP());
    ui.MenuFileCreateInstaller->setText(LanguageSetup->MenuFileCreateInstaller());
    ui.MenuFileCreateXMlPackageList->setText(LanguageSetup->MenuFileExportPackageList());
    ui.MenuFileExportBuildInstaller->setText(LanguageSetup->MenuFileExportBuildInstaller());
    ui.MenuFileExportBuildZipPackages->setText(LanguageSetup->MenuFileExportBuildZipPackages());
    ui.MenuFileSetup->setText(LanguageSetup->MenuFileSetup());
    ui.MenuFileQuit->setText(LanguageSetup->MenuFileQuit());
    ui.MenuView->setTitle(LanguageSetup->MenuView());
    ui.MenuViewShowMenubar->setText(LanguageSetup->MenuViewShowMenubar());
    ui.MenuViewShowToolbar->setText(LanguageSetup->MenuViewShowToolbar());
    ui.MenuViewsShowTree->setText(LanguageSetup->MenuViewShowTree());
    ui.MenuViewsShowScreenshots->setText(LanguageSetup->MenuViewShowScreenshots());
    ui.MenuViewsShowSearchBox->setText(LanguageSetup->MenuViewShowSearchBox());
    ui.MenuViewsShowExtraInfo->setText(LanguageSetup->MenuViewShowExtraInfo());
    ui.MenuViewsShowTooltips->setText(LanguageSetup->MenuViewShowTooltips());
    ui.MenuViewsSimpleListNoIcons->setText(LanguageSetup->MenuViewListNoIcons());
    ui.MenuViewsSimpleList->setText(LanguageSetup->MenuViewList());
    ui.MenuViewsListNoIcons->setText(LanguageSetup->MenuViewReportNoIcons());
    ui.MenuViewsList->setText(LanguageSetup->MenuViewReport());
    ui.MenuViewsSmallIcons->setText(LanguageSetup->MenuViewSmallIcons());
    ui.MenuViewsIcons->setText(LanguageSetup->MenuViewIcons());
    ui.MenuViewsScreenshots->setText(LanguageSetup->MenuViewScreenshots());
    ui.MenuViewsQuickStarter->setText(LanguageSetup->MenuViewQuickStarter());
    ui.MenuRun->setTitle(LanguageSetup->MenuRun());
    ui.MenuRunGame->setText(LanguageSetup->MenuRunGame());
    ui.MenuRunSetup->setText(LanguageSetup->MenuRunSetup());
    ui.MenuRunExtraFile->setText(LanguageSetup->MenuRunExtraFile());
    ui.MenuRunRunDosBox->setText(LanguageSetup->MenuRunRunDOSBox());
    ui.MenuRunRunDosBoxKeyMapper->setText(LanguageSetup->MenuRunRunDOSBoxKeyMapper());
    ui.MenuRunOpenDosBoxConfig->setText(LanguageSetup->MenuRunOpenDOSBoxConfig());
    ui.MenuRunDOSBoxOutputTest->setText(LanguageSetup->MenuRunDOSBoxOutputTest());
    ui.MenuRunRunScummVM->setText(LanguageSetup->MenuRunRunScummVM());
    ui.MenuRunOpenScummVMConfig->setText(LanguageSetup->MenuRunOpenScummVMConfig());
    ui.MenuProfile->setTitle(LanguageSetup->MenuProfile());
    ui.MenuProfileAddManually->setTitle(LanguageSetup->MenuProfileAddManually());
    ui.MenuProfileAdd->setText(LanguageSetup->MenuProfileAdd());
    ui.MenuProfileAddScummVM->setText(LanguageSetup->MenuProfileAddScummVM());
    ui.MenuProfileAddWindows->setText(LanguageSetup->MenuProfileAddWindows());
    ui.MenuProfileAddOther->setText(LanguageSetup->MenuProfileAddOther());
    ui.MenuProfileAddFromTemplate->setText(LanguageSetup->MenuProfileAddFromTemplate());
    ui.MenuProfileAddWithWizard->setText(LanguageSetup->MenuProfileAddWithWizard());
    ui.MenuProfileImportZip->setText(LanguageSetup->MenuFileImportZIP());
    ui.MenuProfileAddInstallationSupport->setText(LanguageSetup->MenuProfileAddWithInstallationSupport());
    ui.MenuProfileDownloadPackages->setText(LanguageSetup->MenuFileImportDownload());
    ui.MenuProfileEdit->setText(LanguageSetup->MenuProfileEdit());
    ui.MenuProfileCopy->setText(LanguageSetup->MenuProfileCopy());
    if (PrgSetup->OldDeleteMenuItem()) {
        ui.MenuProfileDelete->setVisible(true);
        ui.MenuProfileDelete->setText(LanguageSetup->MenuProfileDelete());
        ui.MenuProfileDeinstall->setText(LanguageSetup->MenuProfileDeinstall());
        ui.MenuProfileDeinstall->setProperty("vclImageIndex", -1);
        ui.MenuProfileDeinstall->setIcon(QIcon());
        ui.MenuProfileDelete->setShortcut(QKeySequence(Qt::Key_Delete));
        ui.MenuProfileDeinstall->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    } else {
        ui.MenuProfileDelete->setVisible(false);
        ui.MenuProfileDelete->setText(LanguageSetup->MenuProfileDelete());
        ui.MenuProfileDeinstall->setText(LanguageSetup->MenuProfileDelete());
        ui.MenuProfileDeinstall->setProperty("vclImageIndex", ui.MenuProfileDelete->property("vclImageIndex"));
        ui.MenuProfileDeinstall->setIcon(ui.MenuProfileDelete->icon());
        ui.MenuProfileDelete->setShortcut(QKeySequence());
        ui.MenuProfileDeinstall->setShortcut(QKeySequence(Qt::Key_Delete));
    }
    ui.MenuProfileMakeInstaller->setText(LanguageSetup->MenuProfileMakeInstaller());
    ui.MenuProfileMakeZipArchive->setText(LanguageSetup->MenuProfileMakeZipArchive());
    ui.MenuProfileViewConfFile->setText(LanguageSetup->MenuProfileViewConfFile());
    ui.MenuProfileViewIniFile->setText(LanguageSetup->MenuProfileViewIniFile());
    ui.MenuProfileFilesFolders->setTitle(LanguageSetup->MenuProfileFilesAndFolders());
    ui.MenuProfileOpenFolder->setText(LanguageSetup->MenuProfileOpenFolder());
    ui.MenuProfileOpenFolderDOSBox->setText(LanguageSetup->MenuProfileOpenFolderDOSBox());
    ui.MenuProfileOpenCaptureFolder->setText(LanguageSetup->MenuProfileOpenCaptureFolder());
    ui.MenuProfileOpenDataFolder->setText(LanguageSetup->MenuProfileOpenDataFolder());
    ui.MenuProfileOpenFileInProgramFolder->setText(LanguageSetup->MenuProfileOpenFileInProgramFolder());
    ui.MenuProfileOpenFileInDataFolder->setText(LanguageSetup->MenuProfileOpenFileInDataFolder());
    ui.MenuProfileWWW->setText(LanguageSetup->GameWWW());
    ui.MenuProfileMarkAsFavorite->setText(LanguageSetup->MenuProfileMarkAsFavorite());
    ui.MenuProfileCreateShortcut->setText(LanguageSetup->MenuProfileCreateShortcut());
    ui.MenuProfileSearchGame->setText(LanguageSetup->MenuProfileSearchGame());
    ui.MenuProfileCheating->setText(LanguageSetup->MenuProfileCheating());
    ui.MenuExtras->setTitle(LanguageSetup->MenuExtras());
    ui.MenuExtrasIconManager->setText(LanguageSetup->MenuExtrasIconManager());
    ui.MenuExtrasViewLogs->setText(LanguageSetup->MenuExtrasViewLogs());
    ui.MenuExtrasOpenGamesFolder->setText(LanguageSetup->MenuExtrasViewLogsOpenGamesFolder());
    ui.MenuExtrasTemplates->setText(LanguageSetup->MenuExtrasTemplates());
    ui.MenuExtrasCheating->setTitle(LanguageSetup->MenuExtrasCheating());
    ui.MenuExtrasCheatingApply->setText(LanguageSetup->MenuExtrasCheatingApply());
    ui.MenuExtrasCheatingEdit->setText(LanguageSetup->MenuExtrasCheatingEdit());
    ui.MenuExtrasCheatingSearch->setText(LanguageSetup->MenuExtrasCheatingSearch());
    ui.MenuExtrasDeinstallMultipleGames->setText(LanguageSetup->MenuExtrasDeinstallMultipleGames());
    ui.MenuExtrasImageFiles->setTitle(LanguageSetup->MenuExtrasImageFiles());
    ui.MenuExtrasCreateIMGImage->setText(LanguageSetup->MenuExtrasCreateIMGImageFile());
    ui.MenuExtrasWriteIMGImage->setText(LanguageSetup->MenuExtrasWriteIMGImageFile());
    ui.MenuExtrasCreateImage->setText(LanguageSetup->MenuExtrasCreateImageFile());
    ui.MenuExtrasCreateISOImage->setText(LanguageSetup->MenuExtrasCreateISOImageFile());
    ui.MenuExtrasExtractImage->setText(LanguageSetup->MenuExtrasExtractImage());
    ui.MenuExtrasImageFromFolder->setText(LanguageSetup->MenuExtrasImageFromFolder());
    ui.MenuExtrasImageFromProfile->setText(LanguageSetup->MenuExtrasImageFromProfile());
    ui.MenuExtrasScanGamesFolder->setText(LanguageSetup->MenuExtrasScanGamesFolder());
    ui.MenuExtrasTransferProfiles->setText(LanguageSetup->MenuExtrasTransferProfiles());
    ui.MenuExtrasChangeProfiles->setText(LanguageSetup->MenuExtrasChangeProfiles());
    ui.MenuExtrasCreateShortcuts->setText(LanguageSetup->MenuExtrasCreateShortcuts());
    ui.MenuExtrasCheckProfiles->setText(LanguageSetup->MenuExtrasCheckProfiles());
    ui.MenuExtrasTranslation->setTitle(LanguageSetup->MenuExtrasTranslationEditor());
    ui.MenuExtrasTranslationDFendReborn->setText(LanguageSetup->MenuExtrasTranslationEditorDFendReborn());
    ui.MenuExtrasTranslationDOSBox->setText(LanguageSetup->MenuExtrasTranslationEditorDOSBox());
    ui.MenuHelp->setTitle(LanguageSetup->MenuHelp());
    ui.MenuHelpDosBox->setTitle(LanguageSetup->MenuHelpDOSBox());
    ui.MenuHelpDosBoxReadme->setText(LanguageSetup->MenuHelpDOSBoxReadme());
    ui.MenuHelpDosBoxFAQ->setText(LanguageSetup->MenuHelpDOSBoxFAQ());
    ui.MenuHelpDosBoxIntro->setText(LanguageSetup->MenuHelpDOSBoxIntro());
    ui.MenuHelpDosBoxIntroDosBox->setText(LanguageSetup->MenuHelpDOSBoxIntroDOSBox());
    ui.MenuHelpDosBoxHotkeys->setText(LanguageSetup->MenuHelpDOSBoxHotkeys());
    ui.MenuHelpDosBoxHotkeysDosbox->setText(LanguageSetup->MenuHelpDOSBoxHotkeysDOSBox());
    ui.MenuHelpDosBoxHomepage->setText(LanguageSetup->MenuHelpDOSBoxHomepage());
    ui.MenuHelpDosBoxCompatibility->setText(LanguageSetup->MenuHelpDOSBoxCompatibility());
    ui.MenuHelpDemoCompatibility->setText(LanguageSetup->MenuHelpDemoCompatibility());
    ui.MenuHelpScummVM->setTitle(LanguageSetup->MenuHelpScummVM());
    ui.MenuHelpScummVMReadme->setText(LanguageSetup->MenuHelpScummVMReadme());
    ui.MenuHelpScummVMFAQ->setText(LanguageSetup->MenuHelpScummVMFAQ());
    ui.MenuHelpScummVMHomepage->setText(LanguageSetup->MenuHelpScummVMHomepage());
    ui.MenuHelpScummVMCompatibility->setText(LanguageSetup->MenuHelpScummVMCompatibility());
    ui.MenuHelpScummVMCompatibilityIntern->setText(LanguageSetup->MenuHelpScummVMCompatibilityList());
    ui.MenuHelpAbandonware->setText(LanguageSetup->MenuHelpAbandonware());
    ui.MenuHelpHomepage->setText(LanguageSetup->MenuHelpHomepage());
    ui.MenuHelpForum->setText(LanguageSetup->MenuHelpForum());
    ui.MenuHelpUpdates->setText(LanguageSetup->MenuHelpUpdates());
    ui.MenuHelpStatistics->setText(LanguageSetup->MenuHelpStatistics());
    ui.MenuHelpOperationMode->setText(LanguageSetup->MenuHelpOperationMode());
    ui.MenuHelpHelp->setText(LanguageSetup->MenuHelpHelp());
    ui.MenuHelpHelpContent->setText(LanguageSetup->MenuHelpContent());
    ui.MenuHelpIntroduction->setText(LanguageSetup->MenuHelpIntroduction());
    ui.MenuHelpAddingGames->setText(LanguageSetup->MenuHelpAddingGames());
    ui.MenuHelpGetGames->setText(LanguageSetup->MenuHelpGetGames());
    ui.MenuHelpAbout->setText(LanguageSetup->MenuHelpAbout());

    ui.AddButtonMenuAddManually->setTitle(LanguageSetup->MenuProfileAddManually());
    ui.AddButtonMenuAdd->setText(LanguageSetup->MenuProfileAdd());
    ui.AddButtonMenuAddScummVM->setText(LanguageSetup->MenuProfileAddScummVM());
    ui.AddButtonMenuAddWindows->setText(LanguageSetup->MenuProfileAddWindows());
    ui.AddButtonMenuAddOther->setText(LanguageSetup->MenuProfileAddOther());
    ui.AddButtonMenuAddFromTemplate->setText(LanguageSetup->MenuProfileAddFromTemplate());
    ui.AddButtonMenuAddWithWizard->setText(LanguageSetup->MenuProfileAddWithWizard());
    ui.AddButtonMenuImportZip->setText(LanguageSetup->MenuFileImportZIP());
    ui.AddButtonMenuAddInstallationSupport->setText(LanguageSetup->MenuProfileAddWithInstallationSupport());
    ui.AddButtonMenuDownloadPackages->setText(LanguageSetup->MenuFileImportDownload());
    ui.AddButtonMenuAddGetGames->setText(LanguageSetup->MenuHelpGetGames());

    SetupToolbarHints();

    ui.PopupRunGame->setText(LanguageSetup->PopupRunGame());
    ui.PopupRunSetup->setText(LanguageSetup->PopupRunSetup());
    ui.PopupRunExtraFile->setText(LanguageSetup->PopupRunExtraFile());
    ui.PopupEdit->setText(LanguageSetup->PopupEdit());
    ui.PopupCopy->setText(LanguageSetup->PopupCopy());
    if (PrgSetup->OldDeleteMenuItem()) {
        ui.PopupDelete->setVisible(true);
        ui.PopupDelete->setText(LanguageSetup->MenuProfileDelete());
        ui.PopupDeinstall->setText(LanguageSetup->MenuProfileDeinstall());
        ui.PopupDeinstall->setProperty("vclImageIndex", -1);
        ui.PopupDeinstall->setIcon(QIcon());
        ui.PopupDelete->setShortcut(QKeySequence(Qt::Key_Delete));
        ui.PopupDeinstall->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    } else {
        ui.PopupDelete->setVisible(false);
        ui.PopupDelete->setText(LanguageSetup->MenuProfileDelete());
        ui.PopupDeinstall->setText(LanguageSetup->MenuProfileDelete());
        ui.PopupDeinstall->setProperty("vclImageIndex", ui.PopupDelete->property("vclImageIndex"));
        ui.PopupDeinstall->setIcon(ui.PopupDelete->icon());
        ui.PopupDelete->setShortcut(QKeySequence());
        ui.PopupDeinstall->setShortcut(QKeySequence(Qt::Key_Delete));
    }
    ui.PopupMakeInstaller->setText(LanguageSetup->PopupMakeInstaller());
    ui.PopupMakeZipArchive->setText(LanguageSetup->PopupMakeZipArchive());
    ui.PopupViewConfFile->setText(LanguageSetup->MenuProfileViewConfFile());
    ui.PopupViewINIFile->setText(LanguageSetup->MenuProfileViewIniFile());
    ui.PopupFilesFolders->setTitle(LanguageSetup->PopupFilesFolder());
    ui.PopupOpenFolder->setText(LanguageSetup->PopupOpenFolder());
    ui.PopupOpenFolderDOSBox->setText(LanguageSetup->PopupOpenFolderDOSBox());
    ui.PopupOpenCaptureFolder->setText(LanguageSetup->PopupOpenCaptureFolder());
    ui.PopupOpenDataFolder->setText(LanguageSetup->PopupOpenDataFolder());
    ui.PopupOpenFileInProgramFolder->setText(LanguageSetup->PopupOpenFileInProgramFolder());
    ui.PopupOpenFileInDataFolder->setText(LanguageSetup->PopupOpenFileInDataFolder());
    ui.PopupWWW->setText(LanguageSetup->GameWWW());
    ui.PopupMarkAsFavorite->setText(LanguageSetup->PopupMarkAsFavorite());
    ui.PopupCreateShortcut->setText(LanguageSetup->PopupCreateShortcut());
    ui.PopupSearchGame->setText(LanguageSetup->PopupSearchGame());
    ui.PopupCheating->setText(LanguageSetup->MenuProfileCheating());
    ui.PopupViews->setTitle(LanguageSetup->PopupView());
    ui.PopupSimpleListNoIcons->setText(LanguageSetup->MenuViewListNoIcons());
    ui.PopupSimpleList->setText(LanguageSetup->MenuViewList());
    ui.PopupListNoIcons->setText(LanguageSetup->MenuViewReportNoIcons());
    ui.PopupList->setText(LanguageSetup->MenuViewReport());
    ui.PopupSmallIcons->setText(LanguageSetup->MenuViewSmallIcons());
    ui.PopupIcons->setText(LanguageSetup->MenuViewIcons());
    ui.PopupScreenshots->setText(LanguageSetup->MenuViewScreenshots());

    QTabWidget *PC = ui.CapturePageControl;
    PC->setTabText(PC->indexOf(ui.CaptureScreenshotsTab), LanguageSetup->CaptureScreenshots());
    if (ui.ListView->Selected() != nullptr) {
        if (ScummVMMode(GameOf(ui.ListView->Selected()))) {
            ui.ScreenshotsInfoPanel_Caption->setText(LanguageSetup->CaptureScreenshotsInfoScumm());
        } else {
            if (WindowsExeMode(GameOf(ui.ListView->Selected())))
                ui.ScreenshotsInfoPanel_Caption->setText("");
            else
                ui.ScreenshotsInfoPanel_Caption->setText(LanguageSetup->CaptureScreenshotsInfo());
        }
    } else {
        ui.ScreenshotsInfoPanel_Caption->setText("");
    }
    PC->setTabText(PC->indexOf(ui.CaptureSoundTab), LanguageSetup->CaptureSounds());
    PC->setTabText(PC->indexOf(ui.CaptureVideoTab), LanguageSetup->CaptureVideos());
    ui.SoundInfoPanel_Caption->setText(LanguageSetup->CaptureSoundsInfo());
    ui.VideoInfoPanel_Caption->setText(LanguageSetup->CaptureVideosInfo());
    PC->setTabText(PC->indexOf(ui.GameNotesPanel), LanguageSetup->CaptureNotes());
    PC->setTabText(PC->indexOf(ui.DataFilesTab), LanguageSetup->CaptureDataFiles());

    ui.ScreenshotPopupOpen->setText(LanguageSetup->ScreenshotPopupOpen());
    ui.ScreenshotPopupOpenExternal->setText(LanguageSetup->ScreenshotPopupOpenExternal());
    ui.ScreenshotPopupOpenCaptureFolder->setText(LanguageSetup->PopupOpenCaptureFolder());
    ui.ScreenshotPopupRefresh->setText(LanguageSetup->ScreenshotPopupRefresh());
    ui.ScreenshotPopupImport->setText(LanguageSetup->ScreenshotPopupImport());
    ui.ScreenshotPopupCopy->setText(LanguageSetup->ScreenshotPopupCopy());
    ui.ScreenshotPopupSave->setText(LanguageSetup->ScreenshotPopupSave());
    ui.ScreenshotPopupRename->setText(LanguageSetup->ScreenshotPopupRename());
    ui.ScreenshotPopupRenameAll->setText(LanguageSetup->ScreenshotPopupRenameAll());
    ui.ScreenshotPopupDelete->setText(LanguageSetup->ScreenshotPopupDelete());
    ui.ScreenshotPopupDeleteAll->setText(LanguageSetup->ScreenshotPopupDeleteAll());
    ui.ScreenshotPopupUseAsBackground->setText(LanguageSetup->ScreenshotPopupUseAsBackground());
    ui.ScreenshotPopupUseInScreenshotList->setText(LanguageSetup->ScreenshotPopupUseInScreenshotList());

    ui.SoundPopupOpen->setText(LanguageSetup->ScreenshotPopupOpen());
    ui.SoundPopupOpenExternal->setText(LanguageSetup->SoundsPopupOpenExternal());
    ui.SoundPopupOpenCaptureFolder->setText(LanguageSetup->PopupOpenCaptureFolder());
    ui.SoundPopupRefresh->setText(LanguageSetup->ScreenshotPopupRefresh());
    ui.SoundPopupImport->setText(LanguageSetup->ScreenshotPopupImport());
    ui.SoundPopupSave->setText(LanguageSetup->ScreenshotPopupSave());
    ui.SoundPopupSaveMp3->setText(LanguageSetup->SoundsPopupSaveMp3());
    ui.SoundPopupSaveOgg->setText(LanguageSetup->SoundsPopupSaveOgg());
    ui.SoundPopupRename->setText(LanguageSetup->ScreenshotPopupRename());
    ui.SoundPopupRenameAll->setText(LanguageSetup->ScreenshotPopupRenameAll());
    ui.SoundPopupDelete->setText(LanguageSetup->ScreenshotPopupDelete());
    ui.SoundPopupDeleteAll->setText(LanguageSetup->ScreenshotPopupDeleteAll());
    ui.SoundPopupSaveMp3All->setText(LanguageSetup->SoundsPopupSaveMp3All());
    ui.SoundPopupSaveOggAll->setText(LanguageSetup->SoundsPopupSaveOggAll());

    ui.VideoPopupOpen->setText(LanguageSetup->ScreenshotPopupOpen());
    ui.VideoPopupOpenExternal->setText(LanguageSetup->SoundsPopupOpenExternal());
    ui.VideoPopupOpenCaptureFolder->setText(LanguageSetup->PopupOpenCaptureFolder());
    ui.VideoPopupRefresh->setText(LanguageSetup->ScreenshotPopupRefresh());
    ui.VideoPopupImport->setText(LanguageSetup->ScreenshotPopupImport());
    ui.VideoPopupSave->setText(LanguageSetup->ScreenshotPopupSave());
    ui.VideoPopupRename->setText(LanguageSetup->ScreenshotPopupRename());
    ui.VideoPopupRenameAll->setText(LanguageSetup->ScreenshotPopupRenameAll());
    ui.VideoPopupDelete->setText(LanguageSetup->ScreenshotPopupDelete());
    ui.VideoPopupDeleteAll->setText(LanguageSetup->ScreenshotPopupDeleteAll());

    ui.TrayIconPopupRestore->setText(LanguageSetup->TrayPopupRestore());
    ui.TrayIconPopupRun->setText(LanguageSetup->TrayPopupRun());
    ui.TrayIconPopupRecentlyStarted->setText(LanguageSetup->GameRecentlyPlayed());
    ui.TrayIconPopupAddProfile->setText(LanguageSetup->TrayPopupAdd());
    ui.TrayIconPopupAddScummVMProfile->setText(LanguageSetup->MenuProfileAddScummVM());
    ui.TrayIconPopupAddWindowsProfile->setText(LanguageSetup->MenuProfileAddWindows());
    ui.TrayIconPopupAddOther->setText(LanguageSetup->MenuProfileAddOther());
    ui.TrayIconPopupAddFromTemplate->setText(LanguageSetup->TrayPopupAddFromTemplate());
    ui.TrayIconPopupAddWithWizard->setText(LanguageSetup->TrayPopupAddWithWizard());
    ui.TrayIconPopupClose->setText(LanguageSetup->TrayPopupClose());

    ui.TreeViewPopupEditUserFilters->setText(LanguageSetup->TreeViewPopupUserDefinedCategories());
    ui.ToolbarPopupSetupButtons->setText(LanguageSetup->ToolbarPopupSetupButtons());
    ui.ToolbarPopupSetupIconSet->setText(LanguageSetup->ToolbarPopupSetupIconSet());
    ui.ToolbarPopupShowMenubar->setText(LanguageSetup->MenuViewShowMenubar());

    {
        int H = Max(20, Min(500, PrgSetup->ScreenshotPreviewSize()));
        ScreenshotImageList->SetSize(H * 16 / 10, H);
        ui.ScreenshotListView->SetLargeImages(ScreenshotImageList);
    }

    // MenuKeyCaps[...]:=LanguageSetup.Key... - the key names in menu short cuts
    // are translated by Qt itself (QKeySequence::NativeText).

    MsgDlgCaptions.Warning = LanguageSetup->MsgDlgWarning();
    MsgDlgCaptions.Error = LanguageSetup->MsgDlgError();
    MsgDlgCaptions.Information = LanguageSetup->MsgDlgInformation();
    MsgDlgCaptions.Confirmation = LanguageSetup->MsgDlgConfirm();
    MsgDlgCaptions.Yes = LanguageSetup->MsgDlgYes();
    MsgDlgCaptions.No = LanguageSetup->MsgDlgNo();
    MsgDlgCaptions.OK = LanguageSetup->MsgDlgOK();
    MsgDlgCaptions.Cancel = LanguageSetup->MsgDlgCancel();
    MsgDlgCaptions.Abort = LanguageSetup->MsgDlgAbort();
    MsgDlgCaptions.Retry = LanguageSetup->MsgDlgRetry();
    MsgDlgCaptions.Ignore = LanguageSetup->MsgDlgIgnore();
    MsgDlgCaptions.All = LanguageSetup->MsgDlgAll();
    MsgDlgCaptions.NoToAll = LanguageSetup->MsgDlgNoToAll();
    MsgDlgCaptions.YesToAll = LanguageSetup->MsgDlgYesToAll();

    ui.ZipInfoLabel->setText(LanguageSetup->ZipFormWaitInfo());

    ui.FirstRunInfoLabel->setText(LanguageSetup->FirstRunInfoBar());
    ui.FirstRunInfoButton->setToolTip(LanguageSetup->FirstRunInfoBarCloseHint());

    ViewFilesFrame->LoadLanguage();
    SelectHelpFile();

    SetSearchEditText(LanguageSetup->Search());
    SetSearchEditGray(true);

    SetRichEditPopup(ui.GameNotesEdit);

    if (ViewImageForm) {
        LogInfo("Loading image viewer language");
        ViewImageForm->LoadLanguage();
    }

    LogInfo("Loading old software and search links");
    LoadAbandonLinks();
    LoadSearchLinks();

    if (QuickStartForm) QuickStartForm->LoadLanguage();

    LogInfo("### End of LoadMenuLanguage ###");
}

void TDFendRebornMainForm::SelectHelpFile()
{
    // Port note: the help is a folder of HTML pages (Help/<Language>/) instead
    // of a .chm file in the language folder. The "HelpFile" setting of the
    // language file names the folder ("English"; the extension of the old
    // "English.chm" values is ignored).
    QString HelpFileName;
    bool B;

    const QString HelpName = ChangeFileExt(Trim(LanguageSetup->HelpFile()), "");
    auto HelpFolder = [](const QString &Dir, const QString &Name) { return Dir + "Help" + PathDelim + Name; };
    auto Exists = [](const QString &Folder) { return FileExists(IncludeTrailingPathDelimiter(Folder) + "Index.html"); };

    B = false;
    HelpFileName = HelpFolder(PrgDataDir(), HelpName); if (!HelpName.isEmpty() && Exists(HelpFileName)) B = true;
    if (!B) { HelpFileName = HelpFolder(PrgDir(), HelpName); if (!HelpName.isEmpty() && Exists(HelpFileName)) B = true; }
    if (!B) { HelpFileName = HelpFolder(PrgDataDir(), "English"); if (Exists(HelpFileName)) B = true; }
    if (!B) { HelpFileName = HelpFolder(PrgDir(), "English"); if (Exists(HelpFileName)) B = true; }

    if (B) {
        if (HTMLhelpRouter == nullptr) HTMLhelpRouter = InitHTMLHelp(HelpFileName); else HTMLhelpRouter->SetHelpfile(HelpFileName);
    }
}

// ---------------------------------------------------------------------------
// GUI initialization
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::InitGUI(bool FirstInit)
{
    QTreeWidgetItem *TN;
    QString S;

    LogInfo("### Start of InitGUI ###");
    // DoubleBuffered / SetVistaFonts: not needed with Qt

    LogInfo("Setting up hotkeys");
    ui.MenuRunGame->setShortcut(QKeySequence(Qt::Key_Return));
    ui.MenuRunSetup->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.MenuProfileEdit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    ui.PopupRunGame->setShortcut(QKeySequence(Qt::Key_Return));
    ui.PopupRunSetup->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.PopupEdit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    ui.ScreenshotPopupOpen->setShortcut(QKeySequence(Qt::Key_Return));
    ui.SoundPopupOpen->setShortcut(QKeySequence(Qt::Key_Return));
    ui.SoundPopupOpenExternal->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.VideoPopupOpen->setShortcut(QKeySequence(Qt::Key_Return));
    ui.VideoPopupOpenExternal->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));

    LogInfo("Init tree and list view");
    if (!FirstInit) S = GamesListSaveColWidthsToString(ui.ListView); else S = "";
    InitListViewForGamesList(ui.ListView, PrgSetup->ShowExtraInfo());
    InitTreeViewForGamesList(ui.TreeView, GameDB);
    if (FirstInit) {
        if (PrgSetup->StoreColumnWidths()) GamesListLoadColWidths(ui.ListView);
    } else {
        GamesListLoadColWidthsFromString(ui.ListView, S);
    }

    if (PrgSetup->RestoreFilter() && Trim(PrgSetup->FilterMain()) != "") {
        TN = nullptr;
        const QList<QTreeWidgetItem *> Items = AllTreeItems(ui.TreeView);
        for (QTreeWidgetItem *Item : Items) if (ExtUpperCase(Item->text(0)) == ExtUpperCase(Trim(PrgSetup->FilterMain()))) {
            TN = Item;
            if (Trim(PrgSetup->FilterMain()) != "") for (int J = 0; J < TN->childCount(); J++) if (ExtUpperCase(TN->child(J)->text(0)) == ExtUpperCase(Trim(PrgSetup->FilterSub()))) {
                TN = TN->child(J); break;
            }
            break;
        }
        bool TreeViewChangeEvent = FTreeViewOnChange;
        if (FirstInit) FTreeViewOnChange = false;
        if (TN != nullptr) ui.TreeView->setCurrentItem(TN);
        FTreeViewOnChange = TreeViewChangeEvent;
    }

    if (PrgSetup->RestorePreviewerCategory()) ui.CapturePageControl->setCurrentIndex(Max(0, Min(PrgSetup->PreviewerCategory(), ui.CapturePageControl->count() - 1)));

    LogInfo("Set window state (1)");
    QRect ScreenRect = screen() ? screen()->geometry() : QRect(0, 0, 1024, 768);
    switch (PrgSetup->StartWindowSize()) {
        case 0:
            /*nothing*/
            break;
        case 1: {
            if (PrgSetup->MainMaximized()) {
                /*Maximize in OnShow*/
            } else {
                int L = x(), T = y();
                if (PrgSetup->MainLeft() >= 0 && PrgSetup->MainLeft() < 9 * ScreenRect.width() / 10) L = PrgSetup->MainLeft();
                if (PrgSetup->MainTop() >= 0 && PrgSetup->MainTop() < 9 * ScreenRect.height() / 10) T = PrgSetup->MainTop();
                move(L, T);
                int W = width(), H = height();
                if (PrgSetup->MainWidth() >= 0 && L + PrgSetup->MainWidth() <= ScreenRect.width()) W = PrgSetup->MainWidth();
                if (PrgSetup->MainHeight() >= 0 && T + PrgSetup->MainHeight() <= ScreenRect.height()) H = PrgSetup->MainHeight();
                resize(W, H);
                // Menu:=nil; Position:=poDesigned; Menu:=MainMenu;
                FDesignedPosition = true;
                FDesignedPos = QPoint(L, T);
            }
            if (PrgSetup->TreeWidth() >= 50 && PrgSetup->TreeWidth() < 9 * width() / 10) VclLayout::setAlignedExtent(ui.TreeView, PrgSetup->TreeWidth());
            if (PrgSetup->ScreenshotHeight() >= 50 && PrgSetup->ScreenshotHeight() < 9 * height() / 10) VclLayout::setAlignedExtent(ui.CapturePageControl, PrgSetup->ScreenshotHeight());
            break;
        }
        case 2: /*Minimize in OnShow*/ break;
        case 3: /*Maximize in OnShow*/ break;
        case 4:
            FBorderStyleNone = true;
            setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
            setWindowState(Qt::WindowMaximized);
            break;
    }

    LogInfo("Making GUI elements visible");
    ui.CapturePageControl->setVisible(PrgSetup->ShowScreenshots());
    ui.Splitter1->setVisible(ui.CapturePageControl->isVisibleTo(this));
    if (ui.Splitter1->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter1, ui.Splitter1->x(), ui.CapturePageControl->y() - ui.Splitter1->height(), ui.Splitter1->width(), ui.Splitter1->height());
    ui.MenuViewsShowScreenshots->setChecked(PrgSetup->ShowScreenshots());

    ui.TreeView->setVisible(PrgSetup->ShowTree());
    ui.Splitter->setVisible(ui.TreeView->isVisibleTo(this));
    if (ui.Splitter->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter, ui.TreeView->x() + ui.TreeView->width(), ui.Splitter->y(), ui.Splitter->width(), ui.Splitter->height());
    ui.MenuViewsShowTree->setChecked(PrgSetup->ShowTree());

    ui.ButtonClose->setVisible(PrgSetup->ShowToolbarButtonClose());
    ui.ButtonRun->setVisible(PrgSetup->ShowToolbarButtonRun());
    ui.ButtonRunSetup->setVisible(PrgSetup->ShowToolbarButtonRunSetup());
    ui.ButtonAdd->setVisible(PrgSetup->ShowToolbarButtonAdd());
    ui.ButtonEdit->setVisible(PrgSetup->ShowToolbarButtonEdit());
    ui.ButtonDelete->setVisible(PrgSetup->ShowToolbarButtonDelete());
    ui.ButtonHelp->setVisible(PrgSetup->ShowToolbarButtonHelp());

    if (QAction *SA = findChild<QAction *>("SearchEdit_ToolbarAction")) SA->setVisible(PrgSetup->ShowSearchBox()); else ui.SearchEdit->setVisible(PrgSetup->ShowSearchBox());
    ui.MenuViewsShowSearchBox->setChecked(PrgSetup->ShowSearchBox());

    ui.MenuViewsShowExtraInfo->setChecked(PrgSetup->ShowExtraInfo());
    if (!FirstInit) InitViewStyle();

    SetListViewShowHint(PrgSetup->ShowTooltips());
    ui.MenuViewsShowTooltips->setChecked(PrgSetup->ShowTooltips());

    SetSearchEditText(LanguageSetup->Search());
    SetSearchEditGray(true);

    ui.MenuFileImportXMLFile->setVisible(PrgSetup->ShowXMLImportMenuItem());
    ui.MenuFileCreateXML->setVisible(PrgSetup->ShowXMLExportMenuItem());

    ui.MenuFileCreateInstaller->setVisible(GetNSISPath() != "");
    ui.MenuProfileMakeInstaller->setVisible(ui.MenuFileCreateInstaller->isVisible());
    ui.MenuFileExportBuildInstaller->setVisible(ui.MenuFileCreateInstaller->isVisible());
    ui.PopupMakeInstaller->setVisible(ui.MenuFileCreateInstaller->isVisible());

    LogInfo("### End of InitGUI ###");
}

void TDFendRebornMainForm::InitViewStyle()
{
    QString S;
    int I;
    TViewStyle VS;
    TListView *LV = ui.ListView;

    S = Trim(ExtUpperCase(PrgSetup->ListViewStyle()));
    I = 3;
    if (S == "SIMPLELISTNOICONS") I = 0;
    if (S == "SIMPLELIST") I = 1;
    if (S == "LISTNOICONS") I = 2;
    if (S == "LIST") I = 3;
    if (S == "SMALLICONS") I = 4;
    if (S == "ICONS") I = 5;
    if (S == "SCREENSHOTS") I = 6;

    switch (I) {
        case 0:
            ui.MenuViewsSimpleListNoIcons->setChecked(true);
            ui.PopupSimpleListNoIcons->setChecked(true);
            if (LV->SmallImages() != nullptr && LV->ViewStyle() == vsList) LV->SetViewStyle(vsReport);
            LV->SetLargeImages(ListviewIconImageList);
            LV->SetSmallImages(nullptr);
            LV->SetViewStyle(vsList);
            break;
        case 1:
            ui.MenuViewsSimpleList->setChecked(true);
            ui.PopupSimpleList->setChecked(true);
            LV->SetLargeImages(ListviewIconImageList);
            LV->SetSmallImages(ListviewImageList);
            LV->SetViewStyle(vsList);
            break;
        case 2:
            ui.MenuViewsListNoIcons->setChecked(true);
            ui.PopupListNoIcons->setChecked(true);
            LV->SetLargeImages(ListviewIconImageList);
            if (LV->SmallImages() != nullptr && LV->ViewStyle() == vsReport) LV->SetViewStyle(vsList);
            LV->SetSmallImages(nullptr);
            LV->SetViewStyle(vsReport);
            break;
        case 3:
            ui.MenuViewsList->setChecked(true);
            ui.PopupList->setChecked(true);
            LV->SetLargeImages(ListviewIconImageList);
            LV->SetSmallImages(ListviewImageList);
            LV->SetViewStyle(vsReport);
            break;
        case 4:
            ui.MenuViewsSmallIcons->setChecked(true);
            ui.PopupSmallIcons->setChecked(true);
            LV->SetLargeImages(ListviewIconImageList);
            LV->SetSmallImages(ListviewImageList);
            LV->SetViewStyle(vsSmallIcon);
            break;
        case 5:
            ui.MenuViewsIcons->setChecked(true);
            ui.PopupIcons->setChecked(true);
            LV->SetLargeImages(ListviewIconImageList);
            LV->SetSmallImages(ListviewImageList);
            LV->SetViewStyle(vsIcon);
            break;
        case 6:
            ui.MenuViewsScreenshots->setChecked(true);
            ui.PopupScreenshots->setChecked(true);
            LV->SetLargeImages(ListviewScreenshotImageList);
            LV->SetSmallImages(nullptr);
            LV->SetViewStyle(vsIcon);
            break;
    }

    TreeViewChange(ui.TreeView, ui.TreeView->currentItem());
    VS = LV->ViewStyle();
    if (LV->ViewStyle() == vsReport) LV->SetViewStyle(vsSmallIcon); else LV->SetViewStyle(vsReport);
    LV->SetViewStyle(VS);

    if (LV->ViewStyle() == vsSmallIcon) LV->Arrange();
}

void TDFendRebornMainForm::LoadAbandonLinks()
{
    HelpMenuLinkFile->AddLinksToMenu(ui.MenuHelpAbandonware, 6102, 6103, [this](QObject *Sender) { MenuWork(Sender); }, 4);
}

void TDFendRebornMainForm::LoadHelpLinks()
{
    TSearchRec Rec;
    int I;
    QString Dir = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir());

    I = FindFirst(Dir + "DOSBox *.txt", faAnyFile, Rec);
    while (I == 0) {
        QAction *M = AddMenuItem(ui.MenuHelpDosBoxReadme, MaskUnderlineAmpersand(Trim(Rec.Name)), 6101, 24, ImageList);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        I = FindNext(Rec);
    }
    FindClose(Rec);
    I = FindFirst(Dir + "Readme*.txt", faAnyFile, Rec);
    while (I == 0) {
        QAction *M = AddMenuItem(ui.MenuHelpDosBoxReadme, MaskUnderlineAmpersand(Trim(Rec.Name)), 6101, 24, ImageList);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void TDFendRebornMainForm::LoadSearchLinks()
{
    SearchLinkFile->AddLinksToMenu(ui.MenuProfileSearchGame, 4015, 4016, [this](QObject *Sender) { MenuWork(Sender); }, 4);
    SearchLinkFile->AddLinksToMenu(ui.PopupSearchGame, 4015, 4016, [this](QObject *Sender) { MenuWork(Sender); }, 4);
}

void TDFendRebornMainForm::PostShow()
{
    LogInfo("### Start of PostShow ###");

    VclLayout::setBounds(ui.Splitter1, ui.Splitter1->x(), ui.CapturePageControl->y() - ui.Splitter1->height(), ui.Splitter1->width(), ui.Splitter1->height());

    LogInfo("Set window state (2)");
    int WorkAreaHeight = screen() ? screen()->availableGeometry().height() : height();
    switch (PrgSetup->StartWindowSize()) {
        case 0:
            /*nothing*/
            /*Fix height for smaller screens*/
            resize(width(), Min(height(), WorkAreaHeight));
            if (y() + height() > WorkAreaHeight) move(x(), WorkAreaHeight - height());
            /*To fix misaligend ListView when stating maximized*/
            if (isMaximized()) {
                setWindowState(windowState() & ~Qt::WindowMaximized);
                setWindowState(windowState() | Qt::WindowMaximized);
            }
            break;
        case 1:
            if (PrgSetup->MainMaximized()) {
                setWindowState(windowState() | Qt::WindowMaximized);
            } else {
                /*Done in InitGUI*/
                /*Fix height for smaller screens*/
                resize(width(), Min(height(), WorkAreaHeight));
                if (y() + height() > WorkAreaHeight) move(x(), WorkAreaHeight - height());
                FormResize(this);
            }
            break;
        case 2:
            if (PrgSetup->MinimizeToTray()) {
                setWindowState(windowState() | Qt::WindowMinimized);
                ApplicationEventsMinimize(this);
                StartTrayMinimize = true;
            } else {
                setWindowState(windowState() | Qt::WindowMinimized);
            }
            break;
        case 3: setWindowState(windowState() | Qt::WindowMaximized); break;
        case 4: /*Fullscreen*/ break;
    }

    /*If ListView.Items.Count=0 then begin
      TreeViewChange(TreeView,nil);
    end else begin
      ListViewSelectItem(ListView,nil,False);
    end;*/

    LogInfo("Fix misaligned listview if started maximized");
    /*To fix misaligend ListView when stating maximized*/
    VclLayout::relayout(ui.centralwidget);
    VclLayout::relayout(ui.Panel1);

    LogInfo("Init view style for games list");
    InitViewStyle();
    LogInfo("Init view style for data area");
    ui.ScreenshotListView->SetViewStyle(vsReport); ui.ScreenshotListView->SetViewStyle(vsIcon);
    ui.SoundListView->SetViewStyle(vsReport); ui.SoundListView->SetViewStyle(vsList);
    ui.VideoListView->SetViewStyle(vsReport); ui.VideoListView->SetViewStyle(vsList);

    LogInfo("Process parameters");
    ProcessParams();

    if (PrgSetup->RestoreLastSelectedProfile()) {
        LogInfo("Restoring last selected profile");
        SelectGame(PrgSetup->LastSelectedProfile());
    }

    LogInfo("Set GUI for selected game");
    if (ui.ListView->Items()->Count() > 0 && ui.ListView->Selected() == nullptr) {
        ui.ListView->SetSelected(ui.ListView->Items()->Item(0));
        SelectGame(GameOf(ui.ListView->Selected()));
    }
    ListViewSelectItem(ui.ListView, ui.ListView->Selected(), true);

    LogInfo("Free unsed memory");
    MainWindowShowComplete = true;
    TrimWorkingSet();

    LogInfo("### End of PostShow ###\r");

    LogInfo("*** Turning log off ***\r", false);
}

void TDFendRebornMainForm::ProcessParams()
{
    QString S, T;
    bool CloseOK;

    if (ParamCount() == 0) return;

    S = ParamStr(1);

    T = Trim(ExtUpperCase(S));
    if (Pos("WINEMODE", T) > 0 || Pos("WINESUPPORTENABLED", T) > 0 || Pos("WINDOWSMODE", T) > 0 || Pos("NOWINESUPPORT", T) > 0 || Pos("WINESUPPORTDISABLED", T) > 0) return;

    for (int I = 2; I <= ParamCount(); I++) S = S + " " + ParamStr(I);
    T = ExtUpperCase(S);
    LogInfo("Trying to find \"" + S + "\" in game db.");

    for (int I = 0; I < GameDB->Count(); I++) if (Trim(GameDB->Game(I)->CacheNameUpper) == T) {
        TGame *G = GameDB->Game(I);
        CloseOK = true;
        if (ScummVMMode(G)) {
            LogInfo("Found ScummVM game \"" + S + "\" in game db.");
            LogInfo("Running profile " + G->SetupFile());
            RunScummVMGame(G);
            CloseOK = (ZipManager->Count() == 0);
        } else {
            if (WindowsExeMode(G)) {
                LogInfo("Found Windows game \"" + S + "\" in game db.");
                LogInfo("Running profile " + G->SetupFile());
                RunWindowsGame(G);
            } else {
                LogInfo("Found DOSBox game \"" + S + "\" in game db.");
                LogInfo("Running profile " + G->SetupFile());
                if (G->StartFullscreen()) LogInfo("Fullscreen mode: yes"); else LogInfo("Fullscreen mode: no");
                RunGame(G, DeleteOnExit);
                CloseOK = (ZipManager->Count() == 0);
            }
        }
        CloseOK = CloseOK && (RunPrgManager->Count() == 0);

        if (CloseOK) QTimer::singleShot(0, this, [this]() { close(); });   // PostMessage(Handle,WM_Close,0,0)
        return;
    }

    MessageDlg(Format(LanguageSetup->MessageCouldNotFindGame(), {S}), mtError, mbOK, 0);
}

// ---------------------------------------------------------------------------
// Change notification (FindFirstChangeNotification -> QFileSystemWatcher)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::StartCaptureChangeNotify()
{
    StopCaptureChangeNotify();

    QString Dir = MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir());
    if (DirectoryExists(Dir)) {
        ScreenshotsWatcher = new QFileSystemWatcher(this);
        AddDirTreeToWatcher(ScreenshotsWatcher, ExcludeTrailingPathDelimiter(Dir));
        auto Changed = [this]() { ScreenshotsChanged = true; };
        connect(ScreenshotsWatcher, &QFileSystemWatcher::directoryChanged, this, Changed);
        connect(ScreenshotsWatcher, &QFileSystemWatcher::fileChanged, this, Changed);
        ScreenshotsChanged = false;
        hScreenshotsChangeNotification = 1;
    } else {
        hScreenshotsChangeNotification = cINVALID_HANDLE_VALUE;
    }

    Dir = PrgDataDir() + GameListSubDir;
    if (DirectoryExists(Dir)) {
        ConfsWatcher = new QFileSystemWatcher(this);
        AddDirTreeToWatcher(ConfsWatcher, ExcludeTrailingPathDelimiter(Dir));
        auto Changed = [this]() { ConfsChanged = true; };
        connect(ConfsWatcher, &QFileSystemWatcher::directoryChanged, this, Changed);
        connect(ConfsWatcher, &QFileSystemWatcher::fileChanged, this, Changed);
        ConfsChanged = false;
        hConfsChangeNotification = 1;
    } else {
        hConfsChangeNotification = cINVALID_HANDLE_VALUE;
    }
}

void TDFendRebornMainForm::StopCaptureChangeNotify()
{
    if (hScreenshotsChangeNotification != cINVALID_HANDLE_VALUE && ScreenshotsWatcher) delete ScreenshotsWatcher;
    ScreenshotsWatcher = nullptr;
    hScreenshotsChangeNotification = cINVALID_HANDLE_VALUE;

    if (hConfsChangeNotification != cINVALID_HANDLE_VALUE && ConfsWatcher) delete ConfsWatcher;
    ConfsWatcher = nullptr;
    hConfsChangeNotification = cINVALID_HANDLE_VALUE;
}

// ---------------------------------------------------------------------------
// Search / selection / games list
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::SearchEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    SearchEditTimer->stop();
    SearchEditTimer->start();
    if (FSearchEditGray) {
        SetSearchEditGray(false);
        bool Old = FSearchEditOnChange;
        FSearchEditOnChange = true;   // VCL: OnChange is still assigned here
        ui.SearchEdit->setText("");
        FSearchEditOnChange = Old;
        return;
    }
}

void TDFendRebornMainForm::SearchEditTimerTimer(QObject *Sender)
{
    SearchEditTimer->stop();
    TreeViewChange(Sender, ui.TreeView->currentItem());
}

void TDFendRebornMainForm::SelectGame(TGame *AGame)
{
    if (AGame == nullptr) return;
    for (int I = 0; I < ui.ListView->Items()->Count(); I++) if (GameOf(ui.ListView->Items()->Item(I)) == AGame) {
        ui.ListView->SetSelected(ui.ListView->Items()->Item(I));
        ui.ListView->Selected()->MakeVisible(false);
        break;
    }
    ListViewSelectItem(this, ui.ListView->Selected(), ui.ListView->Selected() != nullptr);
}

void TDFendRebornMainForm::SelectGame(const QString &AGameName)
{
    QString S;

    if (Trim(AGameName) == "") return;
    S = Trim(ExtUpperCase(AGameName));
    for (int I = 0; I < ui.ListView->Items()->Count(); I++) {
        TGame *G = GameOf(ui.ListView->Items()->Item(I));
        if (G && G->CacheNameUpper == S) {
            ui.ListView->SetSelected(ui.ListView->Items()->Item(I));
            SelectGame(G);
        }
    }
}

static bool TreeUpdateNeededAgain = false;
static bool TreeUpdateInProgress = false;

void TDFendRebornMainForm::TreeViewChange(QObject *Sender, QTreeWidgetItem *Node)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Node);
    TGame *G;
    QString S;
    TImageList *IL1, *IL2;

    if (TreeUpdateInProgress) {
        LogInfo("TreeViewChange: Tree update in progress not rebuilding games list yet");
        TreeUpdateNeededAgain = true;
        return;
    }
    LogInfo("### Start of TreeViewChange ###");

    TreeUpdateInProgress = true;
    try {
        do {
            TreeUpdateNeededAgain = false;

            if (GameDB->Count() > 1) ui.FirstRunInfoPanel->setVisible(false);

            if (ui.ListView->LargeImages() == nullptr) IL1 = ListviewIconImageList; else IL1 = ui.ListView->LargeImages();
            if (ui.ListView->SmallImages() == nullptr) IL2 = ListviewImageList; else IL2 = ui.ListView->SmallImages();

            LogInfo("Update game notes");
            UpdateGameNotes(); LastSelectedGame = nullptr;

            ui.ListView->Items()->BeginUpdate();
            try {
                if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected()); else G = nullptr;

                if (!FSearchEditGray) S = ui.SearchEdit->text(); else S = "";

                QTreeWidgetItem *Sel = ui.TreeView->currentItem();
                if (Sel == nullptr) {
                    LogInfo("Adding all games to games list");
                    AddGamesToList(ui.ListView, IL2, IL1, ImageList, GameDB, RemoveUnderline(LanguageSetup->All()), "", S, PrgSetup->ShowExtraInfo(), ListSort, ListSortReverse, false, false, false, ui.MenuViewsScreenshots->isChecked());
                } else {
                    if (Sel->parent() == nullptr) {
                        LogInfo("Adding filter matching games to games list (main category)");
                        AddGamesToList(ui.ListView, IL2, IL1, ImageList, GameDB, Sel->text(0), "", S, PrgSetup->ShowExtraInfo(), ListSort, ListSortReverse, false, false, false, ui.MenuViewsScreenshots->isChecked());
                    } else {
                        LogInfo("Adding filter matching games to games list (sub category)");
                        AddGamesToList(ui.ListView, IL2, IL1, ImageList, GameDB, Sel->parent()->text(0), Sel->text(0), S, PrgSetup->ShowExtraInfo(), ListSort, ListSortReverse, false, false, false, ui.MenuViewsScreenshots->isChecked());
                    }
                }
                if (G != nullptr) {
                    LogInfo("Select last selected game");
                    SelectGame(G);
                } else {
                    if (ui.ListView->Items()->Count() > 0) {
                        LogInfo("Select first game in list");
                        SelectGame(GameOf(ui.ListView->Items()->Item(0)));
                    }
                }
            } catch (...) {
                ui.ListView->Items()->EndUpdate();
                throw;
            }
            ui.ListView->Items()->EndUpdate();
        } while (TreeUpdateNeededAgain);
    } catch (...) {
        TreeUpdateInProgress = false;
        throw;
    }
    TreeUpdateInProgress = false;
    LogInfo("### End of TreeViewChange ###");
}

void TDFendRebornMainForm::ListViewAdvancedCustomDrawItem(TListView *Sender, TListItem *Item, int State, int Stage, bool &DefaultDraw)
{
    Q_UNUSED(State);
    Q_UNUSED(Stage);
    bool Bold = false, Italic = false, Underline = false;

    DefaultDraw = true;
    if (Item != nullptr && Item->Data() != nullptr) {
        if (GameOf(Item)->Favorite()) {
            if (PrgSetup->FavoritesBold()) Bold = true;
            if (PrgSetup->FavoritesItalic()) Italic = true;
            if (PrgSetup->FavoritesUnderline()) Underline = true;
        } else {
            if (PrgSetup->NonFavoritesBold()) Bold = true;
            if (PrgSetup->NonFavoritesItalic()) Italic = true;
            if (PrgSetup->NonFavoritesUnderline()) Underline = true;
        }
    }
    if (Item == nullptr) return;
    // TListview(Sender).Canvas.Font.Style:=FS
    QFont F = Sender->font();
    F.setBold(Bold);
    F.setItalic(Italic);
    F.setUnderline(Underline);
    if (!Item->HasFont() || Item->Font() != F) Item->SetFont(F);
}

void TDFendRebornMainForm::ListViewColumnClick(QObject *Sender, TListColumn *Column)
{
    if (Column == nullptr) return;
    SetSortTypeByListViewCol(Column->Index(), ListSort, ListSortReverse);
    TreeViewChange(Sender, ui.TreeView->currentItem());
}

void TDFendRebornMainForm::FormKeyDown(QObject *Sender, int &Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Key == Qt::Key_F2 && Shift == Qt::NoModifier) MenuWork(ui.MenuProfileEdit);
    if (Key == Qt::Key_F5 && Shift == Qt::NoModifier) { EditDefaultProfile(this, GameDB, SearchLinkFile, DeleteOnExit); LoadSearchLinks(); }
}

void TDFendRebornMainForm::ListViewDblClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (ui.ListView->Selected() != nullptr) MenuWork(ui.ButtonRun);
}

void TDFendRebornMainForm::ListViewInfoTip(QObject *Sender, TListItem *Item, QString &InfoTip)
{
    Q_UNUSED(Sender);
    TGame *G;
    QString S;
    int J;

    if (Item == nullptr) return;
    G = GameOf(Item); if (G == nullptr) return;
    InfoTip = G->CacheName;

    if (G->Genre() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameGenre() + ": " + GetCustomGenreName(G->CacheGenre);
    if (G->Developer() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameDeveloper() + ": " + G->CacheDeveloper;
    if (G->Publisher() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GamePublisher() + ": " + G->CachePublisher;
    if (G->Year() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameYear() + ": " + G->CacheYear;
    if (G->Language() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameLanguage() + ": " + GetCustomLanguageName(G->CacheLanguage);
    for (int I = 1; I <= 9; I++) if (G->WWW(I) != "") {
        S = G->WWWNamePlain(I);
        if (Trim(S) == "") S = LanguageSetup->GameWWW();
        InfoTip = InfoTip + "\n" + S + ": " + G->WWW(I);
    }
    if (G->License() != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameLicense() + ": " + GetCustomLicenseName(G->License());

    S = GetLastModificationDate(G); if (S != "") InfoTip = InfoTip + "\n" + LanguageSetup->GameLastModification() + ": " + S;
    InfoTip = InfoTip + "\n" + Format(LanguageSetup->GameStartCountTooltip(), {History->GameStartCount(G->CacheName)});

    J = Max(1, Min(100, PrgSetup->NoteLinesInTooltips()));
    if (G->Notes() != "") {
        TStringList *St = StringToStringList(G->Notes());
        if (St->Count() > 0) InfoTip = InfoTip + "\n";
        for (int I = 0; I <= Min(J - 1, St->Count() - 1); I++) InfoTip = InfoTip + "\n" + (*St)[I];
        delete St;
    }
    if (Trim(G->DataDir()) != "") InfoTip = InfoTip + "\n\n" + LanguageSetup->GameDataDirInfo();
}

void TDFendRebornMainForm::ListViewSelectItem(QObject *Sender, TListItem *Item, bool Selected)
{
    Q_UNUSED(Sender);
    bool B, B2, B3, CaptureFolder;
    QString S;
    int J;
    TGame *G = nullptr;

    B = Selected && Item != nullptr && Item->Data() != nullptr;
    if (B) {
        G = GameOf(Item);
        B2 = ScummVMMode(G);
        B3 = WindowsExeMode(G);
        S = G->CaptureFolder();
        CaptureFolder = (Trim(S) != "");
        if (CaptureFolder) { S = MakeAbsPath(S, PrgSetup->BaseDir()); CaptureFolder = DirectoryExists(S); }
    } else {
        B2 = false;
        B3 = false;
        CaptureFolder = false;
    }

    ui.MenuRunGame->setEnabled(B);
    ui.MenuRunSetup->setEnabled(B && Trim(G->SetupExe()) != "");
    ui.MenuProfileEdit->setEnabled(B);
    ui.MenuProfileCopy->setEnabled(B);
    ui.MenuProfileDelete->setEnabled(B);
    ui.MenuProfileDeinstall->setEnabled(B && (!B3 || !PrgSetup->OldDeleteMenuItem()));
    ui.MenuProfileMakeInstaller->setEnabled(B && !B3);
    ui.MenuProfileMakeZipArchive->setEnabled(B && (!B3 || PrgSetup->AllowPackingWindowsGames()));
    ui.MenuFileCreateZIP->setEnabled(B && (!B3 || PrgSetup->AllowPackingWindowsGames()));
    ui.MenuFileCreateInstaller->setEnabled(B && !B3);
    if (!B) {
        ui.MenuProfileViewConfFile->setVisible(true);
        ui.MenuProfileViewConfFile->setEnabled(false);
        ui.MenuProfileViewIniFile->setVisible(false);
    } else {
        ui.MenuProfileViewConfFile->setVisible(!B2 && !B3);
        ui.MenuProfileViewConfFile->setEnabled(!B2 && !B3);
        ui.MenuProfileViewIniFile->setVisible(B2);
        ui.MenuProfileViewIniFile->setEnabled(B2);
    }
    ui.MenuProfileOpenFolder->setEnabled(B && (Trim(G->GameExe()) != "" || (B2 && Trim(G->ScummVMPath()) != "")));
    ui.MenuProfileOpenFolderDOSBox->setVisible(B && !B2 && !B3);
    ui.MenuProfileOpenFolderDOSBox->setEnabled(B && Trim(G->GameExe()) != "" && !B2 && !B3);
    ui.MenuProfileOpenCaptureFolder->setEnabled(CaptureFolder);
    ui.MenuProfileOpenDataFolder->setEnabled(B && Trim(G->DataDir()) != "");
    ui.MenuProfileMarkAsFavorite->setEnabled(B);
    if (B) {
        if (G->Favorite())
            ui.MenuProfileMarkAsFavorite->setText(LanguageSetup->MenuProfileUnMarkAsFavorite());
        else
            ui.MenuProfileMarkAsFavorite->setText(LanguageSetup->MenuProfileMarkAsFavorite());
    } else {
        ui.MenuProfileMarkAsFavorite->setText(LanguageSetup->MenuProfileMarkAsFavorite());
    }
    ui.MenuProfileWWW->setEnabled(B);
    if (GameLinkList) { delete GameLinkList; GameLinkList = nullptr; }
    if (B) {
        GameLinkList = new TGameLinks(G);
        GameLinkList->AddLinksToMenu(ui.MenuProfileWWW, 4);
    } else {
        TGameLinks::ClearLinksMenu(ui.MenuProfileWWW);
    }
    ui.MenuProfileCreateShortcut->setEnabled(B);
    ui.MenuProfileSearchGame->setEnabled(B);
    ui.MenuProfileCheating->setEnabled(B);

    ui.PopupRunGame->setEnabled(B);
    ui.PopupRunSetup->setEnabled(B && Trim(G->SetupExe()) != "");
    ui.PopupEdit->setEnabled(B);
    ui.PopupCopy->setEnabled(B);
    ui.PopupDelete->setEnabled(B);
    ui.PopupDeinstall->setEnabled(B && (!B3 || !PrgSetup->OldDeleteMenuItem()));
    ui.PopupMakeInstaller->setEnabled(B && !B3);
    ui.PopupMakeZipArchive->setEnabled(B && (!B3 || PrgSetup->AllowPackingWindowsGames()));
    if (!B) {
        ui.PopupViewConfFile->setVisible(true);
        ui.PopupViewConfFile->setEnabled(false);
        ui.PopupViewINIFile->setVisible(false);
    } else {
        ui.PopupViewConfFile->setVisible(!B2 && !B3);
        ui.PopupViewConfFile->setEnabled(!B2 && !B3);
        ui.PopupViewINIFile->setVisible(B2);
        ui.PopupViewINIFile->setEnabled(B2);
    }
    ui.PopupOpenFolder->setEnabled(B && (Trim(G->GameExe()) != "" || (B2 && Trim(G->ScummVMPath()) != "")));
    ui.PopupOpenFolderDOSBox->setVisible(B && !B2 && !B3);
    ui.PopupOpenFolderDOSBox->setEnabled(B && Trim(G->GameExe()) != "" && !B2 && !B3);
    ui.PopupOpenCaptureFolder->setEnabled(CaptureFolder);
    ui.PopupOpenDataFolder->setEnabled(B && Trim(G->DataDir()) != "");
    ui.PopupWWW->setEnabled(B);
    if (B) {
        GameLinkList->AddLinksToMenu(ui.PopupWWW, 4);
    } else {
        TGameLinks::ClearLinksMenu(ui.PopupWWW);
    }
    ui.PopupMarkAsFavorite->setEnabled(B);
    if (B) {
        if (G->Favorite())
            ui.PopupMarkAsFavorite->setText(LanguageSetup->MenuProfileUnMarkAsFavorite());
        else
            ui.PopupMarkAsFavorite->setText(LanguageSetup->MenuProfileMarkAsFavorite());
    } else {
        ui.PopupMarkAsFavorite->setText(LanguageSetup->MenuProfileMarkAsFavorite());
    }
    ui.PopupCreateShortcut->setEnabled(B);
    ui.PopupSearchGame->setEnabled(B);
    ui.PopupCheating->setEnabled(B);

    ui.ScreenshotPopupOpenCaptureFolder->setEnabled(CaptureFolder);
    ui.ScreenshotPopupRefresh->setEnabled(CaptureFolder);
    ui.ScreenshotPopupImport->setEnabled(CaptureFolder);
    ui.ScreenshotPopupRenameAll->setEnabled(CaptureFolder);
    ui.ScreenshotPopupDeleteAll->setEnabled(CaptureFolder);

    ui.SoundPopupOpenCaptureFolder->setEnabled(CaptureFolder);
    ui.SoundPopupRefresh->setEnabled(CaptureFolder);
    ui.SoundPopupImport->setEnabled(CaptureFolder);
    ui.SoundPopupRenameAll->setEnabled(CaptureFolder);
    ui.SoundPopupDeleteAll->setEnabled(CaptureFolder);

    ui.VideoPopupOpenCaptureFolder->setEnabled(CaptureFolder);
    ui.VideoPopupRefresh->setEnabled(CaptureFolder);
    ui.VideoPopupImport->setEnabled(CaptureFolder);
    ui.VideoPopupRenameAll->setEnabled(CaptureFolder);
    ui.VideoPopupDeleteAll->setEnabled(CaptureFolder);

    ui.ButtonRun->setEnabled(B);
    ui.ButtonRunSetup->setEnabled(B && Trim(G->SetupExe()) != "");
    ui.ButtonEdit->setEnabled(B);
    ui.ButtonDelete->setEnabled(B);

    UpdateScreenshotList();

    UpdateGameNotes();

    UpdateOpenFileInDataFolderMenu();

    if (Selected && Item != nullptr) {
        ViewFilesFrame->setVisible(true);
        ViewFilesFrame->SetGame(GameOf(Item));
    } else {
        ViewFilesFrame->setVisible(false);
    }

    ClearMenuItem(ui.MenuRunExtraFile);
    ClearMenuItem(ui.PopupRunExtraFile);
    ui.MenuRunExtraFile->setVisible(false);
    ui.PopupRunExtraFile->setVisible(false);
    if (B && !B2) for (int I = 0; I <= 49; I++) {
        S = Trim(G->ExtraPrgFile(I));
        J = Pos(";", S);
        if (S == "" || J == 0) continue;
        QAction *M = AddMenuItem(ui.MenuRunExtraFile, MaskUnderlineAmpersand(Copy(S, 1, J - 1)), 3010 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        M = AddMenuItem(ui.PopupRunExtraFile, MaskUnderlineAmpersand(Copy(S, 1, J - 1)), 3010 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        ui.MenuRunExtraFile->setVisible(true);
        ui.PopupRunExtraFile->setVisible(true);
    }

    if (PrgSetup->NonModalViewer() && ViewImageForm) {
        if (!B) {
            ShowNonModalImageDialog(this, "", nullptr, nullptr);
        } else {
            TStringList St1, St2;
            S = Trim(GameOf(ui.ListView->Selected())->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (ui.ScreenshotListView->Selected() != nullptr) J = ui.ScreenshotListView->Selected()->Index(); else J = -1;
            for (int I = 0; I < ui.ScreenshotListView->Items()->Count(); I++) {
                if (I < J) St1.Add(IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(I)->Caption());
                if (I > J) St2.Add(IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(I)->Caption());
            }
            if (J < 0) {
                if (St2.Count() > 0) { S = St2[0]; St2.Delete(0); } else S = "";
            } else {
                S = IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(J)->Caption();
            }
            ShowNonModalImageDialog(this, S, &St1, &St2);
        }
    }
}

void TDFendRebornMainForm::UpdateScreenshotList()
{
    QString S, SelItem;
    TListItem *Sel = ui.ListView->Selected();

    SelItem = "";
    if (ui.ScreenshotListView->Selected() != nullptr) SelItem = ui.ScreenshotListView->Selected()->Caption();
    ui.ScreenshotListView->Items()->BeginUpdate();
    ui.ScreenshotListView->Items()->Clear();
    ScreenshotImageList->Clear();
    if (Sel != nullptr && Sel->Data() != nullptr) {
        S = Trim(GameOf(Sel)->CaptureFolder());
        if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
        if (S != "") AddScreenshotsToList(ui.ScreenshotListView, ScreenshotImageList, S);
    }
    ui.ScreenshotListView->Items()->EndUpdate();
    for (int I = 0; I < ui.ScreenshotListView->Items()->Count(); I++) if (ui.ScreenshotListView->Items()->Item(I)->Caption() == SelItem) {
        ui.ScreenshotListView->SetSelected(ui.ScreenshotListView->Items()->Item(I));
        break;
    }
    ui.ScreenshotPopupRenameAll->setEnabled(ui.ScreenshotListView->Items()->Count() > 0);
    ui.ScreenshotPopupDeleteAll->setEnabled(ui.ScreenshotListView->Items()->Count() > 0);

    ui.ScreenshotsInfoPanel->setVisible(ui.ScreenshotListView->Items()->Count() == 0 && Sel != nullptr);
    VclLayout::setAlignedExtent(ui.ScreenshotsInfoPanel, ui.ScreenshotsInfoPanel->isVisibleTo(ui.CaptureScreenshotsTab) ? 17 : 0);
    if (ui.ScreenshotsInfoPanel->isVisibleTo(ui.CaptureScreenshotsTab)) {
        if (ScummVMMode(GameOf(Sel))) {
            ui.ScreenshotsInfoPanel_Caption->setText(LanguageSetup->CaptureScreenshotsInfoScumm());
        } else {
            if (WindowsExeMode(GameOf(Sel)))
                ui.ScreenshotsInfoPanel_Caption->setText("");
            else
                ui.ScreenshotsInfoPanel_Caption->setText(LanguageSetup->CaptureScreenshotsInfo());
        }
    }
    ScreenshotListViewSelectItem(this, ui.ScreenshotListView->Selected(), true);

    SelItem = "";
    if (ui.SoundListView->Selected() != nullptr) SelItem = ui.SoundListView->Selected()->Caption();
    ui.SoundListView->Items()->BeginUpdate();
    ui.SoundListView->Items()->Clear();
    if (Sel != nullptr && Sel->Data() != nullptr) {
        S = Trim(GameOf(Sel)->CaptureFolder());
        if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
        if (S != "") AddSoundsToList(ui.SoundListView, S, 33);
    }
    ui.SoundListView->Items()->EndUpdate();
    for (int I = 0; I < ui.SoundListView->Items()->Count(); I++) if (ui.SoundListView->Items()->Item(I)->Caption() == SelItem) {
        ui.SoundListView->SetSelected(ui.SoundListView->Items()->Item(I));
        break;
    }
    ui.SoundPopupRenameAll->setEnabled(ui.SoundListView->Items()->Count() > 0);
    ui.SoundPopupDeleteAll->setEnabled(ui.SoundListView->Items()->Count() > 0);

    ui.SoundInfoPanel->setVisible(ui.SoundListView->Items()->Count() == 0 && Sel != nullptr && !ScummVMMode(GameOf(Sel)) && !WindowsExeMode(GameOf(Sel)));
    VclLayout::setAlignedExtent(ui.SoundInfoPanel, ui.SoundInfoPanel->isVisibleTo(ui.CaptureSoundTab) ? 17 : 0);
    SoundListViewSelectItem(this, ui.SoundListView->Selected(), true);

    SelItem = "";
    if (ui.VideoListView->Selected() != nullptr) SelItem = ui.VideoListView->Selected()->Caption();
    ui.VideoListView->Items()->BeginUpdate();
    ui.VideoListView->Items()->Clear();
    if (Sel != nullptr && Sel->Data() != nullptr) {
        S = Trim(GameOf(Sel)->CaptureFolder());
        if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
        if (S != "") AddVideosToList(ui.VideoListView, S, 38);
    }
    ui.VideoListView->Items()->EndUpdate();
    for (int I = 0; I < ui.VideoListView->Items()->Count(); I++) if (ui.VideoListView->Items()->Item(I)->Caption() == SelItem) {
        ui.VideoListView->SetSelected(ui.VideoListView->Items()->Item(I));
        break;
    }
    ui.VideoPopupRenameAll->setEnabled(ui.VideoListView->Items()->Count() > 0);
    ui.VideoPopupDeleteAll->setEnabled(ui.VideoListView->Items()->Count() > 0);

    ui.VideoInfoPanel->setVisible(ui.VideoListView->Items()->Count() == 0 && Sel != nullptr && !ScummVMMode(GameOf(Sel)) && !WindowsExeMode(GameOf(Sel)));
    VclLayout::setAlignedExtent(ui.VideoInfoPanel, ui.VideoInfoPanel->isVisibleTo(ui.CaptureVideoTab) ? 17 : 0);
    VideoListViewSelectItem(this, ui.VideoListView->Selected(), true);
}

void TDFendRebornMainForm::UpdateGameNotes()
{
    QString S;
    TListItem *Sel = ui.ListView->Selected();

    ui.GameNotesEdit->setVisible(Sel != nullptr && Sel->Data() != nullptr);
    ui.GameNotesToolBar->setEnabled(ui.GameNotesEdit->isVisibleTo(ui.GameNotesPanel));

    if (LastSelectedGame != nullptr && ui.ListView->Items()->Count() > 0) {
        S = StringListToString(ui.GameNotesEdit->toPlainText());
        if (LastSelectedGame->Notes() != S) {
            LastSelectedGame->SetNotes(S);
            LastSelectedGame->StoreAllValues();
        }
    }

    if (Sel != nullptr) LastSelectedGame = GameOf(Sel); else LastSelectedGame = nullptr;

    if (!ui.GameNotesEdit->isVisibleTo(ui.GameNotesPanel)) return;

    TStringList *St = StringToStringList(GameOf(Sel)->Notes());
    ui.GameNotesEdit->setPlainText(St->ToQStringList().join("\n"));
    delete St;
}

void TDFendRebornMainForm::UpdateOpenFileInDataFolderMenu()
{
    bool B, B2, FilesAdded;
    QString Dir;
    TGame *G;

    B = ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr;
    B2 = B && Trim(GameOf(ui.ListView->Selected())->DataDir()) != "";

    ui.MenuProfileOpenFileInDataFolder->setEnabled(false);
    ui.PopupOpenFileInDataFolder->setEnabled(false);
    ui.MenuProfileOpenFileInProgramFolder->setEnabled(false);
    ui.PopupOpenFileInProgramFolder->setEnabled(false);

    ClearMenuItem(ui.MenuProfileOpenFileInDataFolder);
    ClearMenuItem(ui.PopupOpenFileInDataFolder);
    ClearMenuItem(ui.MenuProfileOpenFileInProgramFolder);
    ClearMenuItem(ui.PopupOpenFileInProgramFolder);

    if (B) G = GameOf(ui.ListView->Selected()); else G = nullptr;

    if (B) {
        Dir = "";
        if (DOSBoxMode(G)) Dir = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
        if (ScummVMMode(G)) Dir = MakeAbsPath(G->ScummVMPath(), PrgSetup->BaseDir());
        if (WindowsExeMode(G)) Dir = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
        if (Trim(Dir) != "") {
            Dir = IncludeTrailingPathDelimiter(Dir);
            if (DirectoryExists(Dir)) {
                FilesAdded = AddDirToMenu(Dir, false, ui.MenuProfileOpenFileInProgramFolder, ui.PopupOpenFileInProgramFolder, 0);
                ui.MenuProfileOpenFileInProgramFolder->setEnabled(FilesAdded);
                ui.PopupOpenFileInProgramFolder->setEnabled(FilesAdded);
            }
        }
    }

    if (B2) {
        Dir = IncludeTrailingPathDelimiter(MakeAbsPath(G->DataDir(), PrgSetup->BaseDir()));
        if (DirectoryExists(Dir)) {
            FilesAdded = AddDirToMenu(Dir, true, ui.MenuProfileOpenFileInDataFolder, ui.PopupOpenFileInDataFolder, 0);
            ui.MenuProfileOpenFileInDataFolder->setEnabled(FilesAdded);
            ui.PopupOpenFileInDataFolder->setEnabled(FilesAdded);
        }
    }
}

bool TDFendRebornMainForm::AddDirToMenu(const QString &Dir, bool DataFolder, QAction *Menu1, QAction *Menu2, int Level)
{
    static const QStringList DataFiles = {".TXT", ".DIZ", ".1ST", ".NFO", ".PDF", ".INI", ".HTM", ".HTML", ".BMP", ".JPG", ".JPEG", ".PNG", ".GIF", ".TIF", ".TIFF"};
    TSearchRec Rec;
    int I, Count;
    bool B;
    QAction *M1, *M2, *M;
    QString S;
    bool result = false;

    auto ConnectWork = [this, DataFolder](QAction *A) {
        if (DataFolder) connect(A, &QAction::triggered, this, [this, A]() { OpenFileInDataFolderMenuWork(A); });
        else connect(A, &QAction::triggered, this, [this, A]() { OpenFileInDataProgramFolderMenuWork(A); });
    };

    /*Subdirectories*/
    Count = 0;
    I = FindFirst(Dir + "*", faDirectory, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == faDirectory && Rec.Name != "." && Rec.Name != "..") {
            if (Count == 20) {
                M1 = AddMenuItem(Menu1, "(...)", 2, 8, ImageList);
                M2 = AddMenuItem(Menu2, "(...)", 2, 8, ImageList);
                ConnectWork(M1);
                ConnectWork(M2);
                break;
            }
            M1 = AddMenuItem(Menu1, MaskUnderlineAmpersand(Rec.Name), 1, 11, ImageList);
            M2 = AddMenuItem(Menu2, MaskUnderlineAmpersand(Rec.Name), 1, 11, ImageList);
            if (Level < 2) {
                result = AddDirToMenu(Dir + Rec.Name + PathDelim, DataFolder, M1, M2, Level + 1) || result;
            } else {
                M = AddMenuItem(M1, "(...)", 2, 8, ImageList);
                ConnectWork(M);
                M = AddMenuItem(M2, "(...)", 2, 8, ImageList);
                ConnectWork(M);
            }
            Count++;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    /*Files*/
    Count = 0;
    I = FindFirst(Dir + "*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            if (DataFolder) B = true; else {
                S = ExtUpperCase(ExtractFileExt(Rec.Name));
                B = DataFiles.contains(S);
            }
            if (B) {
                result = true;
                if (Count == 20) {
                    M1 = AddMenuItem(Menu1, "(...)", 2, 8, ImageList);
                    M2 = AddMenuItem(Menu2, "(...)", 2, 8, ImageList);
                    ConnectWork(M1);
                    ConnectWork(M2);
                    break;
                }
                M1 = AddMenuItem(Menu1, MaskUnderlineAmpersand(Rec.Name), 1, 24, ImageList);
                M2 = AddMenuItem(Menu2, MaskUnderlineAmpersand(Rec.Name), 1, 24, ImageList);
                ConnectWork(M1);
                ConnectWork(M2);
                Count++;
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    return result;
}

void TDFendRebornMainForm::RunFile(const QString &FileName)
{
    QString T;

    T = ExtUpperCase(ExtractFileExt(FileName));
    if (T == ".JPG" || T == ".JPEG" || T == ".BMP" || T == ".PNG" || T == ".GIF" || T == ".TIF" || T == ".TIFF") {
        T = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
        if (T != "" && T != "INTERNAL") {
            if (PrgSetup->NonModalViewer())
                ShowNonModalImageDialog(this, FileName, nullptr, nullptr);
            else
                ShowImageDialog(this, FileName, nullptr, nullptr);
            return;
        }
    }
    if (T == ".WAV" || T == ".MP3" || T == ".OGG") {
        T = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
        if (T != "" && T != "INTERNAL") { PlaySoundDialog(this, FileName, nullptr, nullptr); return; }
    }
    if (T == ".AVI" || T == ".MPG" || T == ".MPEG" || T == ".WMV" || T == ".ASF") {
        T = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
        if (T != "" && T != "INTERNAL") { PlayVideoDialog(this, FileName, nullptr, nullptr); return; }
    }
    if (T == ".TXT" || T == ".NFO" || T == ".DIZ" || T == ".1ST" || T == ".INI") {
        OpenFileInEditor(FileName);
        return;
    }

    ShellOpen(FileName, QString(), ExtractFilePath(FileName));
}

void TDFendRebornMainForm::OpenFileInDataFolderMenuWork(QObject *Sender)
{
    int I;
    QString Dir, S;
    QAction *M = qobject_cast<QAction *>(Sender);
    if (!M) return;

    I = Tag(M);
    if (I != 1 && I != 2) return;
    if (I == 1) S = RemoveUnderline(M->text()); else S = "";

    M = ParentItem(M);
    while (M != ui.MenuProfileOpenFileInDataFolder && M != ui.PopupOpenFileInDataFolder) {
        if (M == nullptr) return;
        S = RemoveUnderline(M->text()) + PathDelim + S;
        M = ParentItem(M);
    }

    if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr || Trim(GameOf(ui.ListView->Selected())->DataDir()) == "") return;
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(GameOf(ui.ListView->Selected())->DataDir(), PrgSetup->BaseDir()));
    if (!DirectoryExists(Dir)) return;

    /*I=2 open folder, I=1 open file*/
    if (I == 2)
        ShellOpen(Dir + S, QString(), Dir + S);
    else
        RunFile(Dir + S);
}

void TDFendRebornMainForm::OpenFileInDataProgramFolderMenuWork(QObject *Sender)
{
    int I;
    QString Dir, S;
    TGame *G;
    QAction *M = qobject_cast<QAction *>(Sender);
    if (!M) return;

    I = Tag(M);
    if (I != 1 && I != 2) return;
    if (I == 1) S = RemoveUnderline(M->text()); else S = "";

    M = ParentItem(M);
    while (M != ui.MenuProfileOpenFileInProgramFolder && M != ui.PopupOpenFileInProgramFolder) {
        if (M == nullptr) return;
        S = RemoveUnderline(M->text()) + PathDelim + S;
        M = ParentItem(M);
    }

    if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) return;

    G = GameOf(ui.ListView->Selected());
    Dir = "";
    if (DOSBoxMode(G)) Dir = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
    if (ScummVMMode(G)) Dir = MakeAbsPath(G->ScummVMPath(), PrgSetup->BaseDir());
    if (WindowsExeMode(G)) Dir = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
    if (Trim(Dir) == "") return;
    Dir = IncludeTrailingPathDelimiter(Dir);
    if (!DirectoryExists(Dir)) return;

    /*I=2 open folder, I=1 open file*/
    if (I == 2)
        ShellOpen(Dir + S, QString(), Dir + S);
    else
        RunFile(Dir + S);
}

void TDFendRebornMainForm::UpdateAddFromTemplateMenu()
{
    TGameDB *TemplateDB;

    ClearMenuItem(ui.MenuProfileAddFromTemplate);
    ClearMenuItem(ui.AddButtonMenuAddFromTemplate);
    ClearMenuItem(ui.TrayIconPopupAddFromTemplate);

    TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    ui.MenuProfileAddFromTemplate->setVisible(TemplateDB->Count() > 0);
    ui.AddButtonMenuAddFromTemplate->setVisible(TemplateDB->Count() > 0);
    for (int I = 0; I < TemplateDB->Count(); I++) {
        QAction *M = AddMenuItem(ui.MenuProfileAddFromTemplate, MaskUnderlineAmpersand(TemplateDB->Game(I)->Name()), I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { AddFromTemplateClick(M); });

        M = AddMenuItem(ui.AddButtonMenuAddFromTemplate, MaskUnderlineAmpersand(TemplateDB->Game(I)->Name()), I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { AddFromTemplateClick(M); });

        M = AddMenuItem(ui.TrayIconPopupAddFromTemplate, MaskUnderlineAmpersand(TemplateDB->Game(I)->Name()), I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { AddFromTemplateClick(M); });
    }
    delete TemplateDB;

    if (QuickStartForm) QuickStartForm->LoadTemplateList();
}

void TDFendRebornMainForm::UpdateGamesListByViewFilesFrame(QObject *Sender)
{
    TreeViewChange(Sender, ui.TreeView->currentItem());
}

// ---------------------------------------------------------------------------
// Appearance
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::LoadListViewGUISetup(TListView *ListView, const QString &Background, const QString &FontColor, int FontSize)
{
    QString S;
    bool IsColor;

    S = Trim(Background);
    if (S == "") { ListView->SetColor(TColorToQColor(clWindow)); SetListViewImage(ListView, ""); } else {
        QColor C = ColorFromSetting(S, clWindow, IsColor);
        if (IsColor) {
            ListView->SetColor(C); SetListViewImage(ListView, "");
        } else {
            ListView->SetColor(TColorToQColor(clWindow)); SetListViewImage(ListView, MakeAbsPath(S, PrgSetup->BaseDir()));
        }
    }

    S = Trim(FontColor);
    if (S == "") ListView->SetFontColor(TColorToQColor(clWindowText)); else {
        ListView->SetFontColor(ColorFromSetting(FontColor, clWindowText, IsColor));
    }
    SetFontSize(ListView, FontSize);
}

void TDFendRebornMainForm::LoadLabelGUISetup(QFrame *L, const QString &Background, const QString &FontColor, int FontSize)
{
    bool IsColor;
    QPalette P = L->palette();

    P.setColor(QPalette::Window, ColorFromSetting(Background, clWindow, IsColor));
    P.setColor(QPalette::WindowText, ColorFromSetting(FontColor, clWindowText, IsColor));
    L->setPalette(P);
    L->setAutoFillBackground(true);
    for (QLabel *Lbl : L->findChildren<QLabel *>()) Lbl->setPalette(P);
    SetFontSize(L, FontSize);
}

void TDFendRebornMainForm::LoadRichEditGUISetup(QTextEdit *R, const QString &Background, const QString &FontColor, int FontSize)
{
    bool IsColor;
    QPalette P = R->palette();

    P.setColor(QPalette::Base, ColorFromSetting(Background, clWindow, IsColor));
    P.setColor(QPalette::Text, ColorFromSetting(FontColor, clWindowText, IsColor));
    R->setPalette(P);
    SetFontSize(R, FontSize);
}

void TDFendRebornMainForm::LoadTreeViewGUISetup(QTreeWidget *TreeView, const QString &Background, const QString &FontColor, int FontSize)
{
    bool IsColor;
    QPalette P = TreeView->palette();

    P.setColor(QPalette::Base, ColorFromSetting(Background, clWindow, IsColor));
    P.setColor(QPalette::Text, ColorFromSetting(FontColor, clWindowText, IsColor));
    TreeView->setPalette(P);
    SetFontSize(TreeView, FontSize);
}

void TDFendRebornMainForm::LoadCoolBarGUISetup(QFrame *CoolBar, const QString &Background, int FontSize)
{
    QString S;
    QPixmap P;

    S = Trim(Background);
    // CoolBar.Bitmap:=nil
    CoolBar->setPalette(QPalette());
    CoolBar->setAutoFillBackground(false);
    ui.ToolBar->setPalette(QPalette());
    ui.ToolBar->setAutoFillBackground(false);
    if (S != "") {
        S = MakeAbsPath(S, PrgSetup->BaseDir());
        if (FileExists(S)) {
            P = LoadImageFromFile(S);
            if (!P.isNull()) {
                QPalette Pal = CoolBar->palette();
                Pal.setBrush(QPalette::Window, QBrush(P));
                CoolBar->setPalette(Pal);
                CoolBar->setAutoFillBackground(true);
                ui.ToolBar->setPalette(Pal);
                ui.ToolBar->setAutoFillBackground(true);
            }
        }
    }

    SetFontSize(CoolBar, FontSize);
}

void TDFendRebornMainForm::SetupToolbarHints()
{
    bool ShowCaptions = PrgSetup->ShowToolbarTexts();
    ui.ToolBar->setToolButtonStyle(ShowCaptions ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);

    ui.ButtonClose->setText(LanguageSetup->ButtonClose());
    ui.ButtonRun->setText(LanguageSetup->ButtonRun());
    ui.ButtonRunSetup->setText(LanguageSetup->ButtonRunSetup());
    ui.ButtonAdd->setText(LanguageSetup->ButtonAdd());
    ui.ButtonEdit->setText(LanguageSetup->ButtonEdit());
    ui.ButtonDelete->setText(LanguageSetup->ButtonDelete());
    if (PrgSetup->OldDeleteMenuItem()) SetTag(ui.ButtonDelete, Tag(ui.MenuProfileDelete)); else SetTag(ui.ButtonDelete, Tag(ui.MenuProfileDeinstall));
    ui.ButtonHelp->setText(LanguageSetup->ButtonHelp());

    if (ShowCaptions) {
        ui.ButtonClose->setToolTip(LanguageSetup->ButtonCloseHintMain());
        ui.ButtonRun->setToolTip(LanguageSetup->ButtonRunHintMain());
        ui.ButtonRunSetup->setToolTip(LanguageSetup->ButtonRunSetupHintMain());
        ui.ButtonAdd->setToolTip(LanguageSetup->ButtonAddHintMain());
        ui.ButtonEdit->setToolTip(LanguageSetup->ButtonEditHintMain());
        ui.ButtonDelete->setToolTip(LanguageSetup->ButtonDeleteHintMain());
        ui.ButtonHelp->setToolTip(LanguageSetup->ButtonHelpHintMain());
    } else {
        ui.ButtonClose->setToolTip(RemoveUnderline(LanguageSetup->ButtonClose()) + "\n\n" + LanguageSetup->ButtonCloseHintMain());
        ui.ButtonRun->setToolTip(RemoveUnderline(LanguageSetup->ButtonRun()) + "\n\n" + LanguageSetup->ButtonRunHintMain());
        ui.ButtonRunSetup->setToolTip(RemoveUnderline(LanguageSetup->ButtonRunSetup()) + "\n\n" + LanguageSetup->ButtonRunSetupHintMain());
        ui.ButtonAdd->setToolTip(RemoveUnderline(LanguageSetup->ButtonAdd()) + "\n\n" + LanguageSetup->ButtonAddHintMain());
        ui.ButtonEdit->setToolTip(RemoveUnderline(LanguageSetup->ButtonEdit()) + "\n\n" + LanguageSetup->ButtonEditHintMain());
        ui.ButtonDelete->setToolTip(RemoveUnderline(LanguageSetup->ButtonDelete()) + "\n\n" + LanguageSetup->ButtonDeleteHintMain());
        ui.ButtonHelp->setToolTip(RemoveUnderline(LanguageSetup->ButtonHelp()) + "\n\n" + LanguageSetup->ButtonHelpHintMain());
    }

    bool GameNotesShowCaptions = PrgSetup->ShowToolbarTexts();
    ui.GameNotesToolBar->setToolButtonStyle(GameNotesShowCaptions ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);

    ui.GameNotesCutButton->setText(LanguageSetup->Cut());
    ui.GameNotesCutButton->setToolTip(LanguageSetup->CutHintMain());
    ui.GameNotesCopyButton->setText(LanguageSetup->Copy());
    ui.GameNotesCopyButton->setToolTip(LanguageSetup->CopyHintMain());
    ui.GameNotesPasteButton->setText(LanguageSetup->Paste());
    ui.GameNotesPasteButton->setToolTip(LanguageSetup->PasteHintMain());
    ui.GameNotesUndoButton->setText(LanguageSetup->Undo());
    ui.GameNotesUndoButton->setToolTip(LanguageSetup->UndoHintMain());

    if (GameNotesShowCaptions) {
        ui.GameNotesCutButton->setToolTip(LanguageSetup->CutHintMain());
        ui.GameNotesCopyButton->setToolTip(LanguageSetup->CopyHintMain());
        ui.GameNotesPasteButton->setToolTip(LanguageSetup->PasteHintMain());
        ui.GameNotesUndoButton->setToolTip(LanguageSetup->UndoHintMain());
    } else {
        ui.GameNotesCutButton->setToolTip(RemoveUnderline(LanguageSetup->Cut()) + "\n\n" + LanguageSetup->CutHintMain());
        ui.GameNotesCopyButton->setToolTip(RemoveUnderline(LanguageSetup->Copy()) + "\n\n" + LanguageSetup->CopyHintMain());
        ui.GameNotesPasteButton->setToolTip(RemoveUnderline(LanguageSetup->Paste()) + "\n\n" + LanguageSetup->PasteHintMain());
        ui.GameNotesUndoButton->setToolTip(RemoveUnderline(LanguageSetup->Undo()) + "\n\n" + LanguageSetup->UndoHintMain());
    }
}

void TDFendRebornMainForm::LoadGUISetup(bool PrgStart)
{
    QString S;

    LogInfo("### Start of LoadGUISetup ###");
    ApplyColorScheme(PrgSetup->ColorScheme());   // (light / dark colours, program options)
    LogInfo("Setting visible state of menu and toolbar icons");
    if (!PrgSetup->ShowMainMenu() && !PrgSetup->ShowToolbar()) PrgSetup->SetShowMainMenu(true);

    if (!PrgSetup->ShowToolbarButtonClose() && !PrgSetup->ShowToolbarButtonRun() && !PrgSetup->ShowToolbarButtonRunSetup()
        && !PrgSetup->ShowToolbarButtonAdd() && !PrgSetup->ShowToolbarButtonEdit() && !PrgSetup->ShowToolbarButtonDelete()
        && !PrgSetup->ShowToolbarButtonHelp() && !PrgSetup->ShowSearchBox()) PrgSetup->SetShowToolbarButtonRun(true);

    LogInfo("Loading icons for buttons");
    {
        TStringList St;
        for (int I = 0; I < PrgSetup->WindowsBasedEmulatorsPrograms()->Count(); I++)
            St.Add(MakeAbsPath((*PrgSetup->WindowsBasedEmulatorsPrograms())[I], PrgSetup->BaseDir()));
        UserIconLoader->UpdateIconsFromPath(ImageList, &St);
    }
    UserIconLoader->SetIconSet(PrgSetup->IconSet());
    UserIconLoader->DialogImage(DI_CloseX, ui.FirstRunInfoButton);
    UserIconLoader->DialogImage(DI_CutToClipboard, GameNotesImageList, 0);
    UserIconLoader->DialogImage(DI_CopyToClipboard, GameNotesImageList, 1);
    UserIconLoader->DialogImage(DI_PasteFromClipboard, GameNotesImageList, 2);
    UserIconLoader->DialogImage(DI_Undo, GameNotesImageList, 3);
    UpdateImageListIcons();
    ViewFilesFrame->LoadLanguage(); /*to update icons right after program start*/

    LogInfo("Setting list view size");
    ListviewScreenshotImageList->SetSize(PrgSetup->ScreenshotListViewWidth(), PrgSetup->ScreenshotListViewHeight());
    ListviewIconImageList->SetSize(PrgSetup->IconSize(), PrgSetup->IconSize());
    if (ui.ListView->LargeImages() != nullptr) ui.ListView->SetLargeImages(ui.ListView->LargeImages());

    ui.ListView->SetGridLines(PrgSetup->GridLinesInGamesList());

    if (!PrgStart) {
        ui.ListView->Items()->BeginUpdate();
        S = GamesListSaveColWidthsToString(ui.ListView);
        ui.ListView->Items()->Clear();
        InitListViewForGamesList(ui.ListView, PrgSetup->ShowExtraInfo());
        GamesListLoadColWidthsFromString(ui.ListView, S);
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        if (!PrgStart) TreeViewChange(this, ui.TreeView->currentItem());
        ui.ListView->Items()->EndUpdate();
    }

    LogInfo("Load list view background");
    LoadListViewGUISetup(ui.ListView, PrgSetup->GamesListViewBackground(), PrgSetup->GamesListViewFontColor(), PrgSetup->GamesListViewFontSize());
    LoadLabelGUISetup(ui.ScreenshotsInfoPanel, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadListViewGUISetup(ui.ScreenshotListView, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadLabelGUISetup(ui.SoundInfoPanel, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadListViewGUISetup(ui.SoundListView, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadLabelGUISetup(ui.VideoInfoPanel, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadListViewGUISetup(ui.VideoListView, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadRichEditGUISetup(ui.GameNotesEdit, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadTreeViewGUISetup(ui.TreeView, PrgSetup->GamesTreeViewBackground(), PrgSetup->GamesTreeViewFontColor(), PrgSetup->GamesTreeViewFontSize());
    LoadCoolBarGUISetup(ui.CoolBar, PrgSetup->ToolbarBackground(), PrgSetup->ToolbarFontSize());
    LoadTreeViewGUISetup(ViewFilesFrame->ui.Tree, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());
    LoadListViewGUISetup(ViewFilesFrame->ui.List, PrgSetup->ScreenshotsListViewBackground(), PrgSetup->ScreenshotsListViewFontColor(), PrgSetup->ScreenshotsListViewFontSize());

    LogInfo("Setting up checked menu items");
    ui.CoolBar->setVisible(PrgSetup->ShowToolbar());
    SetupToolbarHints();

    ui.MenuFile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
    ui.MenuView->menuAction()->setVisible(PrgSetup->ShowMainMenu());
    ui.MenuRun->menuAction()->setVisible(PrgSetup->ShowMainMenu());
    ui.MenuProfile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
    ui.MenuExtras->menuAction()->setVisible(PrgSetup->ShowMainMenu());
    ui.MenuHelp->menuAction()->setVisible(PrgSetup->ShowMainMenu());

    ui.MenuViewShowMenubar->setChecked(true); /*if menu is invisible this menu item also*/
    ui.MenuViewShowToolbar->setChecked(PrgSetup->ShowToolbar());
    ui.ToolbarPopupShowMenubar->setVisible(!PrgSetup->ShowMainMenu());

    ui.ButtonClose->setVisible(PrgSetup->ShowToolbarButtonClose());
    ui.ButtonRun->setVisible(PrgSetup->ShowToolbarButtonRun());
    ui.ButtonRunSetup->setVisible(PrgSetup->ShowToolbarButtonRunSetup());
    ui.ButtonAdd->setVisible(PrgSetup->ShowToolbarButtonAdd());
    ui.ButtonEdit->setVisible(PrgSetup->ShowToolbarButtonEdit());
    ui.ButtonDelete->setVisible(PrgSetup->ShowToolbarButtonDelete());
    ui.ButtonHelp->setVisible(PrgSetup->ShowToolbarButtonHelp());

    if (QAction *SA = findChild<QAction *>("SearchEdit_ToolbarAction")) SA->setVisible(PrgSetup->ShowSearchBox()); else ui.SearchEdit->setVisible(PrgSetup->ShowSearchBox());
    ui.MenuViewsShowExtraInfo->setChecked(PrgSetup->ShowExtraInfo());
    ui.MenuViewsShowSearchBox->setChecked(PrgSetup->ShowSearchBox());
    ui.MenuViewsShowTooltips->setChecked(PrgSetup->ShowTooltips());
    SetListViewShowHint(PrgSetup->ShowTooltips());

    FTreeViewOnChange = true;

    ui.MenuRunRunScummVM->setVisible(Trim(PrgSetup->ScummVMPath()) != "");
    ui.MenuRunOpenScummVMConfig->setVisible(Trim(PrgSetup->ScummVMPath()) != "");
    ui.MenuProfileAddScummVM->setVisible(Trim(PrgSetup->ScummVMPath()) != "");
    ui.AddButtonMenuAddScummVM->setVisible(Trim(PrgSetup->ScummVMPath()) != "");
    ui.MenuHelpScummVMCompatibilityIntern->setVisible(Trim(PrgSetup->ScummVMPath()) != "");
    ui.TrayIconPopupAddScummVMProfile->setVisible(Trim(PrgSetup->ScummVMPath()) != "");

    GameDB->SetCreateConfFilesOnSave(OperationMode != omPortable && PrgSetup->CreateConfFilesForProfiles());
    if (OperationMode != omPortable && PrgSetup->CreateConfFilesForProfiles()) CreateConfFilesForProfiles(GameDB, PrgDataDir() + GameListSubDir + PathDelim);

    ui.MenuProfileAddOther->setVisible(PrgSetup->WindowsBasedEmulatorsNames()->Count() > 0);
    ui.AddButtonMenuAddOther->setVisible(PrgSetup->WindowsBasedEmulatorsNames()->Count() > 0);
    ui.TrayIconPopupAddOther->setVisible(PrgSetup->WindowsBasedEmulatorsNames()->Count() > 0);
    ClearMenuItem(ui.MenuProfileAddOther);
    ClearMenuItem(ui.AddButtonMenuAddOther);
    ClearMenuItem(ui.TrayIconPopupAddOther);

    LogInfo("Setting up tray menu");
    for (int I = 0; I < PrgSetup->WindowsBasedEmulatorsNames()->Count(); I++) {
        QString Caption = Format(LanguageSetup->MenuProfileAddOtherBasedGame(), {(*PrgSetup->WindowsBasedEmulatorsNames())[I]});
        QAction *M = AddMenuItem(ui.MenuProfileAddOther, Caption, 4100 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        M = AddMenuItem(ui.AddButtonMenuAddOther, Caption, 4100 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
        M = AddMenuItem(ui.TrayIconPopupAddOther, Caption, 4100 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { MenuWork(M); });
    }

    LogInfo("### End of LoadGUISetup");
}

// ---------------------------------------------------------------------------
// Adding profiles
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::StartWizard(const QString &ExeFile, bool WindowsExeFile)
{
    TGame *G;
    bool Ok, B1 = false, B2 = false;

    G = nullptr;
    try {
        Ok = ShowWizardDialog(this, GameDB, ExeFile, WindowsExeFile, G, B1, B2, SearchLinkFile);
    } catch (...) {
        LoadSearchLinks();
        throw;
    }
    LoadSearchLinks();
    if (Ok) {
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        TreeViewChange(this, ui.TreeView->currentItem());
        SelectGame(G);
        if (B1) MenuWork(ui.ButtonEdit);
    } else {
        if (B2) MenuWork(ui.MenuProfileAddInstallationSupport);
    }
}

void TDFendRebornMainForm::AddProfile(const QString &ExeFile, const QString &Mode, int TemplateNr)
{
    TGame *G, *DefaultGame;
    TGameDB *TemplateDB;
    bool Ok;

    G = nullptr;
    TemplateDB = nullptr;

    if (TemplateNr >= 0) {
        TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
        DefaultGame = TemplateDB->Game(TemplateNr);
    } else {
        DefaultGame = new TGame(PrgSetup);
    }

    SetFormEnabled(false);
    if (Mode != "") {
        DefaultGame->SetProfileMode(Mode);
        Ok = ModernEditGameProfil(this, GameDB, G, DefaultGame, SearchLinkFile, DeleteOnExit, "", ExeFile);
    } else {
        if (PrgSetup->DFendStyleProfileEditor()) {
            Ok = EditGameProfil(this, GameDB, G, DefaultGame, SearchLinkFile, DeleteOnExit, "", ExeFile);
        } else {
            Ok = ModernEditGameProfil(this, GameDB, G, DefaultGame, SearchLinkFile, DeleteOnExit, "", ExeFile);
        }
    }
    // finally
    SetFormEnabled(true);
    LoadSearchLinks();
    if (TemplateNr >= 0) {
        delete TemplateDB;
    } else {
        DefaultGame->SetProfileMode("DOSBox");
        delete DefaultGame;
    }
    if (!Ok) return;

    InitTreeViewForGamesList(ui.TreeView, GameDB);
    TreeViewChange(this, ui.TreeView->currentItem());
    SelectGame(G);
}

void TDFendRebornMainForm::AddProfileForWindowsEmulator(int Nr)
{
    QString S;
    TGame *G, *DefaultGame;
    QString ExeFile, Parameters;
    bool Ok;

    if (!SelectProgramFile(S, "", "", true, Nr, this)) return;

    DefaultGame = new TGame(PrgSetup);
    G = nullptr;

    SetFormEnabled(false);
    DefaultGame->SetProfileMode("Windows");
    ExeFile = MakeRelPath((*PrgSetup->WindowsBasedEmulatorsPrograms())[Nr], PrgSetup->BaseDir());
    Parameters = Format((*PrgSetup->WindowsBasedEmulatorsParameters())[Nr], {S});
    DefaultGame->SetGameParameters(Parameters);
    Ok = ModernEditGameProfil(this, GameDB, G, DefaultGame, SearchLinkFile, DeleteOnExit, ChangeFileExt(ExtractFileName(S), ""), ExeFile);
    // finally
    SetFormEnabled(true);
    LoadSearchLinks();
    DefaultGame->SetProfileMode("DOSBox");
    DefaultGame->SetGameParameters("");
    delete DefaultGame;
    if (!Ok) return;

    InitTreeViewForGamesList(ui.TreeView, GameDB);
    TreeViewChange(this, ui.TreeView->currentItem());
    SelectGame(G);
}

void TDFendRebornMainForm::AfterSetupDialog(const QString &ColWidthsParam, const QString &UserCols)
{
    TGame *G;
    QString ColWidths = ColWidthsParam;

    TTraceScope TAll("AfterSetupDialog");
    G = nullptr;
    if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected());
    { TTraceScope T("AfterSetupDialog: LoadLanguage"); LoadLanguage(PrgSetup->Language()); }
    if (QuickStartForm) QuickStartForm->BuildTree();
    { TTraceScope T("AfterSetupDialog: LoadMenuLanguage"); LoadMenuLanguage(); }
    if (FSearchEditGray) {
        SetSearchEditText(LanguageSetup->Search());
    }
    { TTraceScope T("AfterSetupDialog: LoadGUISetup"); LoadGUISetup(false); }
    if (ColWidths != "") {
        TStringList *St1 = ValueToList(UserCols), *St2 = ValueToList(GetUserCols()), *St = ValueToList(ColWidths);
        for (int I = 0; I < Min(St1->Count(), St2->Count()); I++) if (ExtUpperCase((*St1)[I]) != ExtUpperCase((*St2)[I])) {
            if (8 + I < St->Count()) St->SetStrings(8 + I, "-1");
        }
        ColWidths = ListToValue(St);
        delete St; delete St1; delete St2;
        GamesListLoadColWidthsFromString(ui.ListView, ColWidths);
    }
    SelectGame(G);
}

// ---------------------------------------------------------------------------
// MenuWork
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::MenuWork(QObject *Sender)
{
    TGame *G, *G2, *DefaultGame;
    QString S, T;
    bool B = false, C = false;
    TGameDB *AutoSetupDB;
    TListItem *Sel;

    auto NoGameSelected = [this]() -> bool {
        if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) {
            MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0);
            return true;
        }
        return false;
    };
    auto ReloadAndSelect = [this, Sender](TGame *AG) {
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        TreeViewChange(Sender, ui.TreeView->currentItem());
        SelectGame(AG);
    };
    auto ClearListBeforeReload = [this]() {
        LastSelectedGame = nullptr;
        ui.ListView->SetSelected(nullptr);
        ui.ListView->Items()->Clear();
        ViewFilesFrame->SetGame(nullptr);
    };
    QString DOSBoxExe = DOSBoxExeFileOfInstallation(0);

    StopCaptureChangeNotify();
    struct RestartNotify {
        TDFendRebornMainForm *F;
        ~RestartNotify() { F->StartCaptureChangeNotify(); }
    } Finally{this};

    int ATag = Tag(Sender);
    switch (ATag) {
        /*File*/
        case 1001: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "conf";
            OpenDialog->Title = LanguageSetup->MenuFileImportConfTitle();
            OpenDialog->Filter = LanguageSetup->MenuFileImportConfFilter();
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                G = ImportConfFile(GameDB, OpenDialog->Files[I]);
                if (G != nullptr) ReloadAndSelect(G);
            }
            break;
        }
        case 1002:
            UpdateGameNotes();
            ShowExportGamesListDialog(this, GameDB);
            break;
        case 1003: UpdateGameNotes(); ExportConfFiles(this, GameDB); break;
        case 1004: UpdateGameNotes(); ExportProfFiles(this, GameDB); break;
        case 1005: UpdateGameNotes(); ExportXMLFiles(this, GameDB); break;
        case 1006:
            S = GamesListSaveColWidthsToString(ui.ListView);
            T = GetUserCols();
            if (!ShowSetupDialog(this, GameDB, SearchLinkFile)) return;
            AfterSetupDialog(S, T);
            break;
        case 1007: close(); break;
        case 1008: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "prof";
            OpenDialog->Title = LanguageSetup->MenuFileImportProfTitle();
            OpenDialog->Filter = LanguageSetup->MenuFileImportProfFilter();
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                S = GameDB->ProfFileName(ExtractFileName(OpenDialog->Files[I]), true);
                if (!CopyFile(OpenDialog->Files[I], S, true)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->Files[I], S}), mtError, mbOK, 0);
                    continue;
                }
                G = new TGame(S);
                GameDB->Add(G);
                ReloadAndSelect(G);
            }
            break;
        }
        case 1009:
            OpenDialog->DefaultExt = "xml";
            /*OpenDialog.Title:=LanguageSetup.MenuFileImportXMLTitle;*/
            OpenDialog->Title = "XML import";
            /*OpenDialog.Filter:=LanguageSetup.MenuFileImportXMLFilter;*/
            OpenDialog->Filter = "XML files (*.xml)|*xml|All files|*.*";
            if (!OpenDialog->Execute()) return;
            if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected()); else G = nullptr;
            S = ImportXMLFile(GameDB, OpenDialog->FileName);
            if (S != "") { MessageDlg(S, mtError, mbOK, 0); return; }
            ReloadAndSelect(G);
            break;
        case 1010: {
            OpenDialog->AllowMultiSelect = true;
            OpenDialog->DefaultExt = "zip";
            OpenDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            bool Ok = OpenDialog->Execute();
            OpenDialog->AllowMultiSelect = false;
            if (!Ok) return;
            G = nullptr;
            for (int I = 0; I < OpenDialog->Files.Count(); I++) {
                G2 = ImportZipPackage(this, OpenDialog->Files[I], GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
                if (G2 != nullptr) G = G2;
            }
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(G);
            break;
        }
        case 1011:
            SetFormEnabled(false);
            AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
            S = ShowPackageManagerDialog(this, GameDB, AutoSetupDB);
            if (S != "") {
                PrgSetup->SetLanguage(ExtractFileName(S));
                AfterSetupDialog("", "");
            }
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            delete AutoSetupDB;
            SetFormEnabled(true);
            TrimWorkingSet();
            break;
        case 1012: ShowPackageCreationDialog(this, GameDB); break;
        case 1013: UpdateGameNotes(); ShowBuildZipPackagesDialog(this, GameDB); break;
        case 1014: UpdateGameNotes(); BuildInstaller(this, GameDB); break;
        case 1015:
            OpenDialog->DefaultExt = "zip";
            OpenDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            if (!OpenDialog->Execute()) return;
            G = RunInstallationFromZipFile(this, GameDB, OpenDialog->FileName);
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(G);
            break;
        case 1016:
            S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SelectDirectory(THandle(0), LanguageSetup->MenuFileImportFolderCaption(), S)) return;
            G = ImportFolder(this, S, GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(G);
            break;
        case 1017:
            S = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SelectDirectory(THandle(0), LanguageSetup->MenuFileImportFolderCaption(), S)) return;
            G = RunInstallationFromFolder(this, GameDB, S);
            if (G == nullptr) return;
            ClearListBeforeReload();
            ReloadAndSelect(G);
            break;
        /*View*/
        case 2001:
            PrgSetup->SetShowTree(!PrgSetup->ShowTree());
            ui.TreeView->setVisible(PrgSetup->ShowTree());
            if (ui.TreeView->isVisibleTo(this) && ui.TreeView->width() < 50) VclLayout::setAlignedExtent(ui.TreeView, 50);
            ui.Splitter->setVisible(ui.TreeView->isVisibleTo(this));
            if (ui.Splitter->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter, ui.TreeView->x() + ui.TreeView->width(), ui.Splitter->y(), ui.Splitter->width(), ui.Splitter->height());
            ui.MenuViewsShowTree->setChecked(PrgSetup->ShowTree());
            if (!PrgSetup->ShowTree()) {
                bool Old = FTreeViewOnChange;
                FTreeViewOnChange = false;   // the change is processed by the explicit call below
                ui.TreeView->setCurrentItem(nullptr);
                FTreeViewOnChange = Old;
                TreeViewChange(Sender, nullptr);
            }
            break;
        case 2002:
            PrgSetup->SetShowScreenshots(!PrgSetup->ShowScreenshots());
            ui.CapturePageControl->setVisible(PrgSetup->ShowScreenshots());
            if (ui.CapturePageControl->isVisibleTo(this) && ui.CapturePageControl->height() < 50) VclLayout::setAlignedExtent(ui.CapturePageControl, 200);
            ui.Splitter1->setVisible(ui.CapturePageControl->isVisibleTo(this));
            if (ui.Splitter1->isVisibleTo(this)) VclLayout::setBounds(ui.Splitter1, ui.Splitter1->x(), ui.CapturePageControl->y() - ui.Splitter1->height(), ui.Splitter1->width(), ui.Splitter1->height());
            ui.MenuViewsShowScreenshots->setChecked(PrgSetup->ShowScreenshots());
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 2003:
            PrgSetup->SetShowSearchBox(!PrgSetup->ShowSearchBox());
            if (QAction *SA = findChild<QAction *>("SearchEdit_ToolbarAction")) SA->setVisible(PrgSetup->ShowSearchBox()); else ui.SearchEdit->setVisible(PrgSetup->ShowSearchBox());
            ui.MenuViewsShowSearchBox->setChecked(PrgSetup->ShowSearchBox());

            if (FSearchEditGray) {
                SetSearchEditText(LanguageSetup->Search());
            }
            LoadGUISetup(false);
            break;
        case 2004:
            PrgSetup->SetShowExtraInfo(!PrgSetup->ShowExtraInfo());
            ui.MenuViewsShowExtraInfo->setChecked(PrgSetup->ShowExtraInfo());
            S = GamesListSaveColWidthsToString(ui.ListView);
            InitListViewForGamesList(ui.ListView, PrgSetup->ShowExtraInfo());
            GamesListLoadColWidthsFromString(ui.ListView, S);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 2005:
            PrgSetup->SetShowTooltips(!PrgSetup->ShowTooltips());
            ui.MenuViewsShowTooltips->setChecked(PrgSetup->ShowTooltips());
            SetListViewShowHint(PrgSetup->ShowTooltips());
            break;
        case 2006: PrgSetup->SetListViewStyle("SimpleListNoIcons"); InitViewStyle(); break;
        case 2007: PrgSetup->SetListViewStyle("SimpleList"); InitViewStyle(); break;
        case 2008: PrgSetup->SetListViewStyle("ListNoIcons"); InitViewStyle(); break;
        case 2009: PrgSetup->SetListViewStyle("List"); InitViewStyle(); break;
        case 2010: PrgSetup->SetListViewStyle("SmallIcons"); InitViewStyle(); break;
        case 2011: PrgSetup->SetListViewStyle("Icons"); InitViewStyle(); break;
        case 2012: PrgSetup->SetListViewStyle("Screenshots"); InitViewStyle(); break;
        case 2013:
            if (!JustShowingOrHidingQuickStartForm) {
                JustShowingOrHidingQuickStartForm = true;
                if (QuickStartForm) {
                    if (!QuickStartForm->Working) { delete QuickStartForm; QuickStartForm = nullptr; }
                } else {
                    QuickStartForm = new TQuickStartForm(this);
                    QuickStartForm->SetOnAddProfile([this](QObject *S, const QString &F, bool W, bool U, int T) { AddProfileFromQuickStarter(S, F, W, U, T); });
                    QuickStartForm->SetOnMenuCloseNotify([this](QObject *S) { QuickStarterCloseNotify(S); });
                    QuickStartForm->show();
                }
                // MenuViewsQuickStarter.Checked:=not MenuViewsQuickStarter.Checked
                // (the checkable QAction has already toggled itself when it was triggered)
                if (Sender != ui.MenuViewsQuickStarter || !ui.MenuViewsQuickStarter->isCheckable())
                    ui.MenuViewsQuickStarter->setChecked(!ui.MenuViewsQuickStarter->isChecked());
                ProcessMessages();
                JustShowingOrHidingQuickStartForm = false;
            }
            break;
        case 2014:
            if (!PrgSetup->ShowToolbar() && PrgSetup->ShowMainMenu()) {
                MessageDlg(LanguageSetup->MenuViewShowMenubarWarning(), mtError, mbOK, 0);
                if (ui.MenuViewShowMenubar->isCheckable()) ui.MenuViewShowMenubar->setChecked(true);
                return;
            }
            PrgSetup->SetShowMainMenu(!PrgSetup->ShowMainMenu());
            ui.MenuFile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuView->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuRun->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuProfile->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuExtras->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.MenuHelp->menuAction()->setVisible(PrgSetup->ShowMainMenu());
            ui.ToolbarPopupShowMenubar->setVisible(!PrgSetup->ShowMainMenu());
            if (ui.MenuViewShowMenubar->isCheckable()) ui.MenuViewShowMenubar->setChecked(true);
            if (!PrgSetup->ShowMainMenu()) MessageDlg(LanguageSetup->MenuViewShowMenubarRestoreInfo(), mtInformation, mbOK, 0);
            break;
        case 2015:
            if (!PrgSetup->ShowMainMenu() && PrgSetup->ShowToolbar()) {
                MessageDlg(LanguageSetup->MenuViewShowMenubarWarning(), mtError, mbOK, 0);
                ui.MenuViewShowToolbar->setChecked(PrgSetup->ShowToolbar());
                return;
            }
            PrgSetup->SetShowToolbar(!PrgSetup->ShowToolbar());
            ui.CoolBar->setVisible(PrgSetup->ShowToolbar());
            ui.MenuViewShowToolbar->setChecked(PrgSetup->ShowToolbar());
            break;
        /*Run*/
        case 3001:
            if (NoGameSelected()) return;
            /*For fixing problems with not hidden tooltips*/ QToolTip::hideText();
            G = GameOf(ui.ListView->Selected());
            if (ScummVMMode(G)) {
                RunScummVMGame(G);
            } else {
                if (WindowsExeMode(G))
                    RunWindowsGame(G);
                else
                    RunGame(G, DeleteOnExit);
            }
            break;
        case 3002:
            if (NoGameSelected()) return;
            /*For fixing problems with not hidden tooltips*/ QToolTip::hideText();
            G = GameOf(ui.ListView->Selected());
            if (WindowsExeMode(G)) RunWindowsGame(G, true); else RunGame(G, DeleteOnExit, true);
            break;
        case 3003: RunDOSBoxWithDefaultConfFile(); break;
        case 3004:
            if (!FileExists(DOSBoxExe)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
                return;
            }
            DefaultGame = new TGame(PrgSetup);
            G = new TGame(TempDir() + "TempGameRec.prof");
            G->AssignFrom(DefaultGame); G->SetGameExe(""); G->SetAutoexec(""); G->SetNrOfMounts(0);
            RunWithCommandline(G, DeleteOnExit, "-startmapper", true);
            delete G;
            delete DefaultGame;
            ExtDeleteFile(TempDir() + "TempGameRec.prof", ftTemp);
            break;
        case 3005: OpenDOSBoxDefaultConfFile(); break;
        case 3006:
            if (FileExists(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile))
                StartProcess(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile, QStringList(), IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()));
            else
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile}), mtError, mbOK, 0);
            break;
        case 3007:
            S = FindScummVMIni();
            if (S != "")
                OpenFileInEditor(S);
            else
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {S}), mtError, mbOK, 0);
            break;
        case 3008: ShowDOSBoxOutputTestDialog(this, GameDB->ConfOpt()); break;
        /*Profile*/
        case 4000:
            UpdateGameNotes();
            switch (PrgSetup->AddButtonFunction()) {
                case 0: MenuWork(ui.MenuProfileAdd); break;
                case 1: MenuWork(ui.MenuProfileAddWithWizard); break;
                case 2: {
                    QWidget *Btn = ui.ToolBar->widgetForAction(ui.ButtonAdd);
                    QPoint P = Btn ? Btn->mapToGlobal(QPoint(0, Btn->height())) : QCursor::pos();
                    ui.AddButtonPopupMenu->popup(P);
                    break;
                }
            }
            break;
        case 4001: UpdateGameNotes(); AddProfile("", "", -1); TrimWorkingSet(); break;
        case 4002: UpdateGameNotes(); StartWizard("", false); TrimWorkingSet(); break;
        case 4003: {
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            TList L;
            for (int I = 0; I < ui.ListView->Items()->Count(); I++) L.append(ui.ListView->Items()->Item(I)->Data());
            G = GameOf(ui.ListView->Selected());
            SetFormEnabled(false);
            bool Ok;
            if (PrgSetup->DFendStyleProfileEditor()) {
                Ok = EditGameProfil(this, GameDB, G, nullptr, SearchLinkFile, DeleteOnExit, "", "", &L);
                if (!Ok) { SetFormEnabled(true); LoadSearchLinks(); return; }
            } else {
                Ok = ModernEditGameProfil(this, GameDB, G, nullptr, SearchLinkFile, DeleteOnExit, "", "", &L);
                if (!Ok) {
                    SelectGame(G); /*Always update screenshots list (a new image could have been downloaded)*/
                    SetFormEnabled(true); LoadSearchLinks();
                    return;
                }
            }
            SetFormEnabled(true);
            ReloadAndSelect(G);
            LoadSearchLinks();
            break;
        }
        case 4004:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            Sel = ui.ListView->Selected();
            S = GameOf(Sel)->Name();
            if (!ShowCopyProfileDialog(this, Trim(GameOf(Sel)->DataDir()) != "", S, B, C)) return;
            G = GameDB->Game(GameDB->Add(S));
            G->AssignFrom(GameOf(Sel));
            G->SetName(S);
            if (B) {
                S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + PathDelim;
                int I = 0;
                while (!PrgSetup->IgnoreDirectoryCollisions() && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
                    I++;
                    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(G->Name()) + IntToStr(I) + PathDelim;
                }
                G->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
            }
            if (C) G->SetDataDir("");
            G->LoadCache();
            ReloadAndSelect(G);
            break;
        case 4005:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            G = GameOf(ui.ListView->Selected());
            if (PrgSetup->AskBeforeDelete()) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecordOnly(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            }
            ui.ListView->Items()->Clear();
            GameDB->Delete(G);
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 4006:
            if (NoGameSelected()) return;
            UpdateGameNotes(); LastSelectedGame = nullptr;
            G = GameOf(ui.ListView->Selected());
            if (WindowsExeMode(G)) {
                if (PrgSetup->AskBeforeDelete()) {
                    if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteRecordOnly(), {G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                }
                ui.ListView->Items()->Clear();
                GameDB->Delete(G);
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            } else {
                SetListViewCustomDraw(false);
                ViewFilesFrame->SetGame(nullptr);
                if (!UninstallGame(this, GameDB, G)) {
                    if (ui.ListView->Selected() != nullptr) ViewFilesFrame->SetGame(GameOf(ui.ListView->Selected()));
                    SetListViewCustomDraw(true);
                    return;
                }
                LastSelectedGame = nullptr;
                ui.ListView->SetSelected(nullptr);
                ui.ListView->Items()->Clear();
                SetListViewCustomDraw(true);
                ReloadAndSelect(G);
            }
            break;
        case 4007:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                S = ""; G = GameOf(ui.ListView->Selected());
                if (DOSBoxMode(G)) S = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
                if (ScummVMMode(G)) S = MakeAbsPath(G->ScummVMPath(), PrgSetup->BaseDir());
                if (WindowsExeMode(G)) S = ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()));
                ShellOpen(S, QString(), S);
            }
            break;
        case 4008:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                S = MakeAbsPath(GameOf(ui.ListView->Selected())->DataDir(), PrgSetup->BaseDir());
                ShellOpen(S, QString(), S);
            }
            break;
        case 4010:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected());
                G->SetFavorite(!G->Favorite());
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 4011:
            if (NoGameSelected()) return;
            if (WineSupportEnabled() && PrgSetup->LinuxLinkMode()) {
                CreateLinuxShortCut(this, GameOf(ui.ListView->Selected()));
            } else {
                G = GameOf(ui.ListView->Selected());
                if (WindowsExeMode(G))
                    ShowCreateShortcutDialog(this, G->Name(), G->SetupFile(), G->Icon(), G->GameExe(), G->GameParameters(), false, true);
                else
                    ShowCreateShortcutDialog(this, G->Name(), G->SetupFile(), G->Icon(), "", "", ScummVMMode(G), false);
            }
            break;
        case 4012:
            if (NoGameSelected()) return;
            UpdateGameNotes();
            BuildInstallerForSingleGame(this, GameOf(ui.ListView->Selected()));
            break;
        case 4013:
            if (NoGameSelected()) return;
            G = GameOf(ui.ListView->Selected());
            OpenConfigurationFile(G, DeleteOnExit);
            break;
        case 4014: AddProfile("", "ScummVM", -1); break;
        case 4015:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr)
                OpenLink(ActionHint(Sender), "<GAMENAME>", GameOf(ui.ListView->Selected())->Name());
            break;
        case 4016: if (SearchLinkFile->EditFile(false)) LoadSearchLinks(); break;
        case 4017:
            if (NoGameSelected()) return;
            UpdateGameNotes();
            SaveDialog->DefaultExt = "zip";
            SaveDialog->FileName = ExtractFileName(ChangeFileExt(GameOf(ui.ListView->Selected())->SetupFile(), ""));
            SaveDialog->Title = LanguageSetup->ProfileMountingZipFileGeneral();
            SaveDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
            SaveDialog->InitialDir = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SaveDialog->Execute()) return;
            BuildZipPackage(this, GameOf(ui.ListView->Selected()), SaveDialog->FileName, false);
            break;
        case 4018: AddProfile("", "Windows", -1); break;
        case 4019:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected()); if (Trim(G->CaptureFolder()) == "") return;
                S = MakeAbsPath(G->CaptureFolder(), PrgSetup->BaseDir());
                if (DirectoryExists(S)) OpenFolder(S);
            }
            break;
        case 4020:
            SetFormEnabled(false);
            G = ShowInstallationSupportDialog(this, GameDB);
            if (G != nullptr) {
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(this, ui.TreeView->currentItem());
                SelectGame(G);
            }
            SetFormEnabled(true);
            break;
        case 4021:
            if (NoGameSelected()) return;
            S = Trim(GameOf(ui.ListView->Selected())->GameExe());
            if (S == "") S = Trim(GameOf(ui.ListView->Selected())->SetupExe());
            if (S == "") S = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()); else S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
            B = false;
            if (ShowCheatApplyDialog(this, S, B) && B) {
                ShowCheatDBEditDialog(this, B);
                if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            }
            break;
        case 4022:
            if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                G = GameOf(ui.ListView->Selected());
                RunDOSBoxCommandLineOnFolder(MakeAbsPath(ExtractFilePath(G->GameExe()), PrgSetup->BaseDir()));
            }
            break;
        /*Extras*/
        case 5001: S = ""; ShowIconManager(this, S, PrgSetup->GameDir(), true); break;
        case 5002: ShowHistoryDialog(this); break;
        case 5003:
            if (PrgSetup->GameDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
            OpenFolder(S);
            break;
        case 5005:
            SetFormEnabled(false);
            G = ShowTemplateDialog(this, GameDB, SearchLinkFile, DeleteOnExit);
            SetFormEnabled(true);
            TrimWorkingSet();
            LoadSearchLinks();
            UpdateAddFromTemplateMenu();
            if (G == nullptr) return;
            ui.ListView->Items()->Clear();
            ReloadAndSelect(G);
            break;
        case 5006:
            SetListViewCustomDraw(false);
            UpdateGameNotes();
            ViewFilesFrame->SetGame(nullptr);
            if (!UninstallMultipleGames(this, GameDB)) {
                if (ui.ListView->Selected() != nullptr) ViewFilesFrame->SetGame(GameOf(ui.ListView->Selected()));
                SetListViewCustomDraw(true);
                return;
            }
            LastSelectedGame = nullptr;
            ui.ListView->SetSelected(nullptr);
            ui.ListView->Items()->Clear();
            SetListViewCustomDraw(true);
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
        case 5007:
            ShowCreateImageFileDialog(this, true, true, GameDB);
            if (NewProfileFromImageCreator != nullptr) ReloadAndSelect(NewProfileFromImageCreator);
            break;
        case 5008: UpdateGameNotes(); TransferGames(this, GameDB); break;
        case 5010:
            if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) G = nullptr; else G = GameOf(ui.ListView->Selected());
            if (!ShowMultipleProfilesEditorDialog(this, GameDB)) return;
            ReloadAndSelect(G);
            break;
        case 5011: ShowCreateISOImageDialog(this, false); break;
        case 5012: ShowCreateISOImageDialog(this, true); break;
        case 5013: ShowWriteIMGImageDialog(this); break;
        case 5014: ShowExpandImageDialog(this); break;
        case 5015: ShowBuildImageFromFolderDialog(this); break;
        case 5016: {
            TStringList *St;
#ifdef CheckDoubleChecksums
            AutoSetupDB = new TGameDB(PrgDataDir() + AutoSetupSubDir, false);
            SetFormEnabled(false);
            St = CheckDoubleChecksums(AutoSetupDB);
            SetFormEnabled(true);
            delete AutoSetupDB;
#else
            SetFormEnabled(false);
            St = CheckGameDB(GameDB);
            SetFormEnabled(true);
#endif
            if (St->Count() == 0) {
                MessageDlg(LanguageSetup->MissingFilesOK(), mtInformation, mbOK, 0);
            } else {
                ShowInfoTextDialog(this, LanguageSetup->MissingFilesCaption(), St);
            }
            delete St;
            break;
        }
        case 5018:
            if (ShowScanGamesFolderResultsDialog(this, GameDB)) {
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 5019: ShowCreateShortcutsDialog(this, GameDB); break;
        case 5020:
            if (!ShowLanguageEditorStartDialog(this, S)) return;
            ShowLanguageEditorDialog(this, S);
            if (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(LanguageSetup->SetupFile()))) {
                LanguageSetup->ReloadINI();
                LoadLanguage(PrgSetup->Language());
                LoadMenuLanguage();
                if (FSearchEditGray) {
                    SetSearchEditText(LanguageSetup->Search());
                }
                LoadGUISetup(false);
            }
            break;
        case 5021:
            SetFormEnabled(false);
            if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected()); else G = nullptr;
            G = ShowMakeBootImageFromProfileDialog(this, GameDB, G);
            SetFormEnabled(true);
            if (G != nullptr) {
                ui.ListView->Items()->Clear();
                ReloadAndSelect(G);
            }
            break;
        case 5022:
            B = false;
            if (ShowCheatApplyDialog(this, MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()), B) && B) {
                ShowCheatDBEditDialog(this, B);
                if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            }
            break;
        case 5023:
            B = false;
            ShowCheatDBEditDialog(this, B);
            if (B) ShowUpdateCheckDialog(this, GameDB, SearchLinkFile);
            break;
        case 5024: ShowCheatSearchDialog(this); break;
        case 5025:
            SetFormEnabled(false);
            if (ShowDOSBoxLangStartDialog(this, S, T)) ShowDOSBoxLangEditDialog(this, S, T);
            SetFormEnabled(true);
            break;
        /*Help*/
        // DOSBox Staging (the DOSBox included with D-Fend Reborn); the manual
        // pages are versioned, DFR_STAGING_DOCS_VERSION: included version
        case 6001: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/getting-started/introduction/")); break;
        case 6002: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/" DFR_STAGING_DOCS_VERSION "/manual/appendices/shortcuts/")); break;
        case 6003:
            if (FileExists(DOSBoxExe)) {
                DefaultGame = new TGame(PrgSetup);
                RunCommand(DefaultGame, DeleteOnExit, "intro special", true);
                delete DefaultGame;
            } else {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
            }
            break;
        case 6004: ShellOpen(QStringLiteral("https://www.dosbox.com/comp_list.php?letter=a")); break;   // (DOSBox Staging has no compatibility list)
        case 6005: ShellOpen(QString("http:/") + "/pain.scene.org/service_dosbox.php"); break;
        case 6006: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/manual/")); break;
        case 6007:
            if (FileExists(DOSBoxExe)) {
                DefaultGame = new TGame(PrgSetup);
                RunCommand(DefaultGame, DeleteOnExit, "intro", true);
                delete DefaultGame;
            } else {
                MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxExe}), mtError, mbOK, 0);
            }
            break;
        case 6008: ShellOpen(LanguageSetup->MenuHelpHomepageURL()); break;
        case 6009: // D-Fend Reborn: issue tracker of the project (the original opened its forum help page)
            ShellOpen(DFRebornHomepage + "/issues");
            break;
        case 6010: ShowUpdateCheckDialog(this, GameDB, SearchLinkFile); break;
        case 6014: ShowStatisticsDialog(this, GameDB); break;
        case 6015: ShowInfoDialog(this); break;
        case 6016: ShellOpen(QStringLiteral("https://www.dosbox-staging.org/")); break;
        case 6017:
            if (Trim(PrgSetup->ScummVMPath()) != "" && FileExists(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "Readme.txt"))
                OpenFileInEditor(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "Readme.txt");
            else
                ShellOpen(QString("http:/") + "/www.scummvm.org/documentation.php");
            break;
        case 6018: ShellOpen(QString("http:/") + "/www.scummvm.org/faq.php"); break;
        case 6019: ShellOpen(QString("http:/") + "/www.scummvm.org/"); break;
        case 6020: ShellOpen(QString("http:/") + "/www.scummvm.org/compatibility.php"); break;
        case 6021: ShowListScummVMGamesDialog(this); break;
        case 6022: ShowOperationModeInfoDialog(this); break;
        case 6024: ApplicationHelpCommand(HELP_CONTEXT, ID_Menu); break;
        case 6025: ApplicationHelpCommand(HELP_CONTEXT, ID_Index); break;
        case 6026: ApplicationHelpCommand(HELP_CONTEXT, ID_Introduction); break;
        case 6027: ApplicationHelpCommand(HELP_CONTEXT, ID_Introduction2); break;
        case 6028: ApplicationHelpCommand(HELP_CONTEXT, ID_HelpGetGames); break;
        case 6101:
            if (auto *A = qobject_cast<QAction *>(Sender))
                OpenFileInEditor(IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + RemoveUnderline(A->text()));
            break;
        case 6102:
            if (Trim(ActionHint(Sender)) != "") {
                if (!OldGamesMenuUsed) {
                    MessageDlg(LanguageSetup->MenuHelpAbandonwareInfo(), mtInformation, mbOK, 0);
                    OldGamesMenuUsed = true;
                }
                ShellOpen(ActionHint(Sender));
            }
            break;
        case 6103: if (HelpMenuLinkFile->EditFile(true)) LoadAbandonLinks(); break;
        default:
            if (ATag >= 3010 && ATag <= 3059) {
                if (ui.ListView->Selected() != nullptr && ui.ListView->Selected()->Data() != nullptr) {
                    G = GameOf(ui.ListView->Selected());
                    if (ScummVMMode(G)) return;
                    if (WindowsExeMode(G))
                        RunWindowssExtraFile(G, ATag - 3010);
                    else
                        RunExtraFile(G, DeleteOnExit, ATag - 3010);
                }
            } else if (ATag >= 4100 && ATag <= 4199) {
                AddProfileForWindowsEmulator(ATag - 4100);
            }
            break;
    }
}

// D-Fend Reborn: primary configuration file of DOSBox Staging (portable
// layout: dosbox-staging.conf next to the program file) and of DOSBox-X
// (dosbox-x.conf in the program folder)
static QString OtherDOSBoxUserConfFile(const QString& DOSBoxFile, TDOSBoxKind Kind, WId Window)
{
    const QString Dir = IncludeTrailingPathDelimiter(ExtractFilePath(DOSBoxFile));
    if (Kind == dkDOSBoxX) return Dir + "dosbox-x.conf";
    if (FileExists(Dir + "dosbox-staging.conf")) return Dir + "dosbox-staging.conf";
#ifdef Q_OS_WIN
    return IncludeTrailingPathDelimiter(GetSpecialFolder(Window, CSIDL_LOCAL_APPDATA)) + "DOSBox\\dosbox-staging.conf";
#else
    Q_UNUSED(Window);
    QString Config = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (Config.isEmpty()) Config = IncludeTrailingPathDelimiter(QDir::homePath()) + ".config";
    return IncludeTrailingPathDelimiter(Config) + "dosbox/dosbox-staging.conf";
#endif
}

void TDFendRebornMainForm::RunDOSBoxWithDefaultConfFile()
{
    double D;
    QString VerString, S, DOSBoxFile;

    DOSBoxFile = DOSBoxExeFileOfInstallation(0);
    if (!FileExists(DOSBoxFile)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {DOSBoxFile}), mtError, mbOK, 0);
        return;
    }

    // DOSBox Staging and DOSBox-X load their own configuration file
    if (DOSBoxKindOfFile(DOSBoxFile) != dkDOSBox) {
        StartProcess(DOSBoxFile, QStringList(), ExtractFilePath(DOSBoxFile));
        return;
    }

    VerString = CheckDOSBoxVersion(0);
    S = VerString; S.replace(',', '.');
    bool Ok;
    D = QLocale::c().toDouble(S, &Ok);
    if (!Ok) D = 0.72;

#ifdef Q_OS_WIN
    S = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_LOCAL_APPDATA)) + "DOSBox\\dosbox-" + VerString + ".conf";
#else
    // DOSBox 0.74: ~/.dosbox, newer versions (and forks): ~/.config/dosbox
    S = IncludeTrailingPathDelimiter(QDir::homePath()) + ".dosbox/dosbox-" + VerString + ".conf";
    if (!FileExists(S)) {
        QString T = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_APPDATA)) + "dosbox/dosbox-" + VerString + ".conf";
        if (FileExists(T)) S = T;
    }
#endif

    if (!FileExists(S) || D <= 0.72) {
        StartProcess(DOSBoxFile, QStringList());
        return;
    }

    if (D <= 0.73) {
        StartProcess(DOSBoxFile, QStringList() << "-c" << S);
        return;
    }

    StartProcess(DOSBoxFile, QStringList() << "-userconf");
}

void TDFendRebornMainForm::OpenDOSBoxDefaultConfFile()
{
    double D;
    QString VerString, S;
    QString ConfFile = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + DosBoxConfFileName;

    const QString DOSBoxFile = DOSBoxExeFileOfInstallation(0);
    const TDOSBoxKind Kind = DOSBoxKindOfFile(DOSBoxFile);
    if (Kind != dkDOSBox) {
        S = OtherDOSBoxUserConfFile(DOSBoxFile, Kind, winId());
        if (FileExists(S)) OpenFileInEditor(S); else MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {S}), mtError, mbOK, 0);
        return;
    }

    VerString = CheckDOSBoxVersion(0);
    S = VerString; S.replace(',', '.');
    bool Ok;
    D = QLocale::c().toDouble(S, &Ok);
    if (!Ok) D = 0.72;

    if (D <= 0.72) {
        if (FileExists(ConfFile))
            OpenFileInEditor(ConfFile);
        else
            MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {ConfFile}), mtError, mbOK, 0);
        return;
    }

#ifdef Q_OS_WIN
    S = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_LOCAL_APPDATA)) + "DOSBox\\dosbox-" + VerString + ".conf";
#else
    // DOSBox 0.74: ~/.dosbox, newer versions (and forks): ~/.config/dosbox
    S = IncludeTrailingPathDelimiter(QDir::homePath()) + ".dosbox/dosbox-" + VerString + ".conf";
    if (!FileExists(S)) {
        QString T = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_APPDATA)) + "dosbox/dosbox-" + VerString + ".conf";
        if (FileExists(T)) S = T;
    }
#endif
    if (FileExists(S)) { OpenFileInEditor(S); return; }
    if (FileExists(ConfFile))
        OpenFileInEditor(ConfFile);
    else
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {ConfFile}), mtError, mbOK, 0);
}

// ---------------------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::TrayIconPopupClick(QObject *Sender)
{
    TGame *G;
    int T = Tag(Sender);

    switch (T) {
        case 0: TrayIconDblClick(Sender); break;
        case 1: TrayIconDblClick(Sender); MenuWork(ui.MenuProfileAdd); break;
        case 2: TrayIconDblClick(Sender); MenuWork(ui.MenuProfileAddScummVM); break;
        case 3: TrayIconDblClick(Sender); MenuWork(ui.MenuProfileAddWithWizard); break;
        case 4: close(); break;
        case 5: ShowMiniRunDialog(this, GameDB); break;
        case 6: TrayIconDblClick(Sender); MenuWork(ui.MenuProfileAddWindows); break;
        default:
            if (T >= 10 && T <= 20) {
                auto *A = qobject_cast<QAction *>(Sender);
                if (!A) break;
                int I = GameDB->IndexOf(RemoveUnderline(A->text()));
                G = (I >= 0) ? GameDB->Game(I) : nullptr;
                if (G != nullptr) {
                    if (ScummVMMode(G)) {
                        RunScummVMGame(G);
                    } else {
                        if (WindowsExeMode(G))
                            RunWindowsGame(G);
                        else
                            RunGame(G, DeleteOnExit);
                    }
                }
            }
            break;
    }
}

void TDFendRebornMainForm::TrayIconPopupMenuPopup(QObject *Sender)
{
    Q_UNUSED(Sender);
    TStringList *St;

    ClearMenuItem(ui.TrayIconPopupRecentlyStarted);

    St = History->GetRecentlyStarted(10, false);
    for (int I = 0; I < St->Count(); I++) {
        QAction *M = AddMenuItem(ui.TrayIconPopupRecentlyStarted, (*St)[I], 10 + I, -1, nullptr);
        connect(M, &QAction::triggered, this, [this, M]() { TrayIconPopupClick(M); });
    }
    delete St;

    if (MiniRunForm != nullptr) {
        MiniRunForm->hide();
        MiniRunForm->close();
        ProcessMessages();
    }
}

void TDFendRebornMainForm::AddFromTemplateClick(QObject *Sender)
{
    auto *A = qobject_cast<QAction *>(Sender);
    if (A && ParentItem(A) == ui.TrayIconPopupAddFromTemplate) TrayIconDblClick(Sender);
    AddProfile("", "", Tag(Sender));
}

// ---------------------------------------------------------------------------
// Capture lists (screenshots, sounds, videos)
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::ScreenshotListViewSelectItem(QObject *Sender, TListItem *Item, bool Selected)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Selected);
    bool B;
    QString S;
    int J;

    B = (Item != nullptr);

    ui.ScreenshotPopupOpen->setEnabled(B);
    ui.ScreenshotPopupOpenExternal->setEnabled(B);
    ui.ScreenshotPopupCopy->setEnabled(B);
    ui.ScreenshotPopupSave->setEnabled(B);
    ui.ScreenshotPopupRename->setEnabled(B);
    ui.ScreenshotPopupDelete->setEnabled(B);
    ui.ScreenshotPopupUseAsBackground->setEnabled(B);
    ui.ScreenshotPopupUseInScreenshotList->setEnabled(B);
    if (!ui.ScreenshotPopupUseInScreenshotList->isEnabled()) ui.ScreenshotPopupUseInScreenshotList->setChecked(false); else {
        ui.ScreenshotPopupUseInScreenshotList->setChecked(ui.ScreenshotListView->Selected() != nullptr && ui.ListView->Selected() != nullptr && GameOf(ui.ListView->Selected()) &&
            Trim(ExtUpperCase(ui.ScreenshotListView->Selected()->Caption())) == Trim(ExtUpperCase(GameOf(ui.ListView->Selected())->ScreenshotListScreenshot())));
    }

    S = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
    ui.ScreenshotPopupOpenExternal->setVisible(S == "" || S == "INTERNAL");

    if (PrgSetup->NonModalViewer() && ViewImageForm) {
        if (ui.ListView->Selected() == nullptr) {
            ShowNonModalImageDialog(this, "", nullptr, nullptr);
        } else {
            TStringList St1, St2;
            S = Trim(GameOf(ui.ListView->Selected())->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (ui.ScreenshotListView->Selected() != nullptr) J = ui.ScreenshotListView->Selected()->Index(); else J = -1;
            for (int I = 0; I < ui.ScreenshotListView->Items()->Count(); I++) {
                if (I < J) St1.Add(IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(I)->Caption());
                if (I > J) St2.Add(IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(I)->Caption());
            }
            if (J < 0) {
                if (St2.Count() > 0) { S = St2[0]; St2.Delete(0); } else S = "";
            } else {
                S = IncludeTrailingPathDelimiter(S) + ui.ScreenshotListView->Items()->Item(J)->Caption();
            }
            ShowNonModalImageDialog(this, S, &St1, &St2);
        }
    }
}

void TDFendRebornMainForm::SoundListViewSelectItem(QObject *Sender, TListItem *Item, bool Selected)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Selected);
    bool B;
    QString S;

    B = (Item != nullptr);

    if (B) S = ExtUpperCase(ExtractFileExt(Item->Caption())); else S = "";

    ui.SoundPopupOpen->setEnabled(B);
    ui.SoundPopupOpenExternal->setEnabled(B);
    ui.SoundPopupSave->setEnabled(B);
    ui.SoundPopupSaveMp3->setEnabled(B && S == ".WAV" && FileExists(PrgSetup->WaveEncMp3()));
    ui.SoundPopupSaveOgg->setEnabled(B && S == ".WAV" && FileExists(PrgSetup->WaveEncOgg()));
    ui.SoundPopupRename->setEnabled(B);
    ui.SoundPopupDelete->setEnabled(B);

    B = false;
    for (int I = 0; I < ui.SoundListView->Items()->Count(); I++) if (ExtUpperCase(ExtractFileExt(ui.SoundListView->Items()->Item(I)->Caption())) == ".WAV") { B = true; break; }
    ui.SoundPopupSaveMp3All->setEnabled(B && FileExists(PrgSetup->WaveEncMp3()));
    ui.SoundPopupSaveOggAll->setEnabled(B && FileExists(PrgSetup->WaveEncOgg()));

    S = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
    ui.SoundPopupOpenExternal->setVisible(S == "" || S == "INTERNAL");
}

void TDFendRebornMainForm::VideoListViewSelectItem(QObject *Sender, TListItem *Item, bool Selected)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Selected);
    bool B;
    QString S;

    B = (Item != nullptr);

    ui.VideoPopupOpen->setEnabled(B);
    ui.VideoPopupOpenExternal->setEnabled(B);
    ui.VideoPopupSave->setEnabled(B);
    ui.VideoPopupRename->setEnabled(B);
    ui.VideoPopupDelete->setEnabled(B);

    S = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
    ui.VideoPopupOpenExternal->setVisible(S == "" || S == "INTERNAL");
}

void TDFendRebornMainForm::ScreenshotListViewDblClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ScreenshotMenuWork(ui.ScreenshotPopupOpen);
}

void TDFendRebornMainForm::SoundListViewDblClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    SoundMenuWork(ui.SoundPopupOpen);
}

void TDFendRebornMainForm::VideoListViewDblClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    VideoMenuWork(ui.VideoPopupOpen);
}

void TDFendRebornMainForm::ScreenshotListViewKeyDown(QObject *Sender, int &Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::ShiftModifier && Key == Qt::Key_Delete) { ScreenshotMenuWork(ui.ScreenshotPopupDeleteAll); return; }
    if (Shift == Qt::ControlModifier && Key == Qt::Key_C) { ScreenshotMenuWork(ui.ScreenshotPopupCopy); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Return: ScreenshotMenuWork(ui.ScreenshotPopupOpen); break;
        case Qt::Key_Insert: ScreenshotMenuWork(ui.ScreenshotPopupImport); break;
        case Qt::Key_F5: ScreenshotMenuWork(ui.ScreenshotPopupRefresh); break;
        case Qt::Key_Delete: ScreenshotMenuWork(ui.ScreenshotPopupDelete); break;
    }
}

void TDFendRebornMainForm::SoundListViewKeyDown(QObject *Sender, int &Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::ShiftModifier && Key == Qt::Key_Delete) { SoundMenuWork(ui.SoundPopupDeleteAll); return; }
    if (Shift == Qt::ShiftModifier && Key == Qt::Key_Return) { SoundMenuWork(ui.SoundPopupOpenExternal); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Return: SoundMenuWork(ui.SoundPopupOpen); break;
        case Qt::Key_Insert: SoundMenuWork(ui.SoundPopupImport); break;
        case Qt::Key_F5: SoundMenuWork(ui.SoundPopupRefresh); break;
        case Qt::Key_Delete: SoundMenuWork(ui.SoundPopupDelete); break;
    }
}

void TDFendRebornMainForm::VideoListViewKeyDown(QObject *Sender, int &Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::ShiftModifier && Key == Qt::Key_Delete) { VideoMenuWork(ui.VideoPopupDeleteAll); return; }
    if (Shift == Qt::ShiftModifier && Key == Qt::Key_Return) { VideoMenuWork(ui.VideoPopupOpenExternal); return; }
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Return: VideoMenuWork(ui.VideoPopupOpen); break;
        case Qt::Key_Insert: VideoMenuWork(ui.VideoPopupImport); break;
        case Qt::Key_F5: VideoMenuWork(ui.VideoPopupRefresh); break;
        case Qt::Key_Delete: VideoMenuWork(ui.VideoPopupDelete); break;
    }
}

// Capture folder of the selected game (Trim + MakeAbsPath), '' if none
static QString SelectedCaptureFolder(TListView *ListView)
{
    TListItem *Sel = ListView->Selected();
    if (Sel == nullptr || Sel->Data() == nullptr) return QString();
    QString S = Trim(GameOf(Sel)->CaptureFolder());
    if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
    return S;
}

void TDFendRebornMainForm::ScreenshotMenuWork(QObject *Sender)
{
    QString S, T;
    TGame *G;
    TWallpaperStyle WPStype = WSStretch;
    TListView *SL = ui.ScreenshotListView;

    switch (Tag(Sender)) {
        case 0:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                {
                    TStringList St1, St2;
                    for (int I = 0; I < SL->Items()->Count(); I++) {
                        if (I < SL->Selected()->Index()) St1.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                        if (I > SL->Selected()->Index()) St2.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                    }
                    if (PrgSetup->NonModalViewer())
                        ShowNonModalImageDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), &St1, &St2);
                    else
                        ShowImageDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), &St1, &St2);
                }
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 1: ListViewSelectItem(Sender, ui.ListView->Selected(), true); break;
        case 2:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                QPixmap P = LoadImageFromFile(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption());
                if (!P.isNull()) QGuiApplication::clipboard()->setPixmap(P);
            }
            break;
        case 3:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                ScreenshotSaveDialog->FileName = "";
                ScreenshotSaveDialog->Title = LanguageSetup->ViewImageFormSaveTitle();
                ScreenshotSaveDialog->Filter = LanguageSetup->ViewImageFormSaveFilter();
                if (!ScreenshotSaveDialog->Execute()) return;
                SaveImageToFile(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), ScreenshotSaveDialog->FileName);
            }
            break;
        case 4:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), ftMediaViewer))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption()}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 5:
            S = SelectedCaptureFolder(ui.ListView);
            if (S == "") return;
            for (int I = 0; I < SL->Items()->Count(); I++) if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption(), ftMediaViewer)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption()}), mtError, mbOK, 0);
                break;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 6:
            if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) {
                MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
            }
            G = GameOf(ui.ListView->Selected());
            S = Trim(G->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (S == "") return;
            OpenDialog->Title = LanguageSetup->ScreenshotPopupImportTitle();
            OpenDialog->Filter = LanguageSetup->ScreenshotPopupImportFilter();
            if (!OpenDialog->Execute()) return;
            S = IncludeTrailingPathDelimiter(S) + ExtractFileName(OpenDialog->FileName);
            if (!CopyFile(OpenDialog->FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->FileName, S}), mtError, mbOK, 0);
                return;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 7:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                if (!ShowWallpaperStyleDialog(this, WPStype)) return;
                SetDesktopWallpaper(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), WPStype);
            }
            break;
        case 8:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S);
                T = ChangeFileExt(SL->Selected()->Caption(), "");
                if (!InputQuery(LanguageSetup->ScreenshotPopupRenameCaption(), LanguageSetup->ScreenshotPopupRenameLabel(), T)) return;
                T = T + ExtractFileExt(SL->Selected()->Caption());
                if (!RenameFile(S + SL->Selected()->Caption(), S + T))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {S + SL->Selected()->Caption(), S + T}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
                T = Trim(ExtUpperCase(T));
                for (int I = 0; I < SL->Items()->Count(); I++)
                    if (Trim(ExtUpperCase(SL->Items()->Item(I)->Caption())) == T) {
                        SL->SetSelected(SL->Items()->Item(I));
                        break;
                    }
                ScreenshotListViewSelectItem(Sender, SL->Selected(), SL->Selected() != nullptr);
            }
            break;
        case 9:
            if (SL->Selected() != nullptr && ui.ListView->Selected() != nullptr && GameOf(ui.ListView->Selected())) {
                S = SL->Selected()->Caption();
                G = GameOf(ui.ListView->Selected());
                if (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(G->ScreenshotListScreenshot())))
                    G->SetScreenshotListScreenshot("");
                else
                    G->SetScreenshotListScreenshot(S);
                TreeViewChange(Sender, ui.TreeView->currentItem());
            }
            break;
        case 10:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                T = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
                if (T != "" && T != "INTERNAL") {
                    if (PrgSetup->NonModalViewer())
                        ShowNonModalImageDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), nullptr, nullptr);
                    else
                        ShowImageDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), nullptr, nullptr);
                    return;
                }
                ShellOpen(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption());
            }
            break;
        case 11:
            if (ui.ListView->Selected() && RenameMediaFiles(this, GameOf(ui.ListView->Selected()), GameDB, rfScreenshots)) TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
    }
}

void TDFendRebornMainForm::SoundMenuWork(QObject *Sender)
{
    QString S, T;
    TGame *G;
    TListView *SL = ui.SoundListView;

    switch (Tag(Sender)) {
        case 0:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                {
                    TStringList St1, St2;
                    for (int I = 0; I < SL->Items()->Count(); I++) {
                        if (I < SL->Selected()->Index()) St1.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                        if (I > SL->Selected()->Index()) St2.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                    }
                    PlaySoundDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), &St1, &St2);
                }
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 1: ListViewSelectItem(Sender, ui.ListView->Selected(), true); break;
        case 2:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                T = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
                if (T != "" && T != "INTERNAL") {
                    PlaySoundDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), nullptr, nullptr);
                    return;
                }
                ShellOpen(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption());
            }
            break;
        case 3:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption();
                TSaveDialog SaveDialog(this);
                T = ExtractFileExt(S);
                if (T != "" && T[0] == '.') T = Copy(T, 2, MaxInt);
                SaveDialog.DefaultExt = T;
                SaveDialog.FileName = "";
                SaveDialog.Title = LanguageSetup->SoundCaptureSaveTitle();
                T = Trim(ExtUpperCase(T));
                SaveDialog.Filter = LanguageSetup->SoundCaptureSaveWAVFilter();
                if (T == "MP3") SaveDialog.Filter = LanguageSetup->SoundCaptureSaveMP3Filter();
                if (T == "OGG") SaveDialog.Filter = LanguageSetup->SoundCaptureSaveOGGFilter();
                if (T == "MID" || T == "MIDI") SaveDialog.Filter = LanguageSetup->SoundCaptureSaveMIDFilter();
                if (!SaveDialog.Execute()) return;
                T = SaveDialog.FileName;
                if (!CopyFile(S, T, true)) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {S, T}), mtError, mbOK, 0);
            }
            break;
        case 4:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption();
                TSaveDialog SaveDialog(this);
                SaveDialog.DefaultExt = "mp3";
                SaveDialog.FileName = "";
                SaveDialog.Title = LanguageSetup->SoundCaptureSaveTitle();
                SaveDialog.Filter = LanguageSetup->SoundCaptureSaveMP3Filter();
                if (!SaveDialog.Execute()) return;
                T = SaveDialog.FileName;
                StartProcess(PrgSetup->WaveEncMp3(), SplitCommandLine(ProcessEncoderParameters(PrgSetup->WaveEncMp3Parameters(), S, T)));
            }
            break;
        case 5:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption();
                TSaveDialog SaveDialog(this);
                SaveDialog.DefaultExt = "ogg";
                SaveDialog.FileName = "";
                SaveDialog.Title = LanguageSetup->SoundCaptureSaveTitle();
                SaveDialog.Filter = LanguageSetup->SoundCaptureSaveOGGFilter();
                if (!SaveDialog.Execute()) return;
                T = SaveDialog.FileName;
                StartProcess(PrgSetup->WaveEncOgg(), SplitCommandLine(ProcessEncoderParameters(PrgSetup->WaveEncOggParameters(), S, T)));
            }
            break;
        case 6:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), ftMediaViewer))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption()}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 7:
            S = SelectedCaptureFolder(ui.ListView);
            if (S == "") return;
            for (int I = 0; I < SL->Items()->Count(); I++) if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption(), ftMediaViewer)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption()}), mtError, mbOK, 0);
                break;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 8:
            if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) {
                MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
            }
            G = GameOf(ui.ListView->Selected());
            S = Trim(G->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (S == "") return;
            OpenDialog->Title = LanguageSetup->SoundCaptureImportTitle();
            OpenDialog->Filter = LanguageSetup->SoundCaptureImportFilter();
            if (!OpenDialog->Execute()) return;
            S = IncludeTrailingPathDelimiter(S) + ExtractFileName(OpenDialog->FileName);
            if (!CopyFile(OpenDialog->FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->FileName, S}), mtError, mbOK, 0);
                return;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 9:
            S = SelectedCaptureFolder(ui.ListView);
            if (S == "") return;
            S = IncludeTrailingPathDelimiter(S);
            for (int I = 0; I < SL->Items()->Count(); I++) if (ExtUpperCase(ExtractFileExt(SL->Items()->Item(I)->Caption())) == ".WAV") {
                T = SL->Items()->Item(I)->Caption();
                StartProcess(PrgSetup->WaveEncMp3(), SplitCommandLine(ProcessEncoderParameters(PrgSetup->WaveEncMp3Parameters(), S + T, S + ChangeFileExt(T, ".mp3"))));
            }
            break;
        case 10:
            S = SelectedCaptureFolder(ui.ListView);
            if (S == "") return;
            S = IncludeTrailingPathDelimiter(S);
            for (int I = 0; I < SL->Items()->Count(); I++) if (ExtUpperCase(ExtractFileExt(SL->Items()->Item(I)->Caption())) == ".WAV") {
                T = SL->Items()->Item(I)->Caption();
                StartProcess(PrgSetup->WaveEncOgg(), SplitCommandLine(ProcessEncoderParameters(PrgSetup->WaveEncOggParameters(), S + T, S + ChangeFileExt(T, ".ogg"))));
            }
            break;
        case 11:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S);
                T = ChangeFileExt(SL->Selected()->Caption(), "");
                if (!InputQuery(LanguageSetup->ScreenshotPopupRenameCaption(), LanguageSetup->ScreenshotPopupRenameLabel(), T)) return;
                T = T + ExtractFileExt(SL->Selected()->Caption());
                if (!RenameFile(S + SL->Selected()->Caption(), S + T))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {S + SL->Selected()->Caption(), S + T}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
                T = Trim(ExtUpperCase(T));
                for (int I = 0; I < SL->Items()->Count(); I++)
                    if (Trim(ExtUpperCase(SL->Items()->Item(I)->Caption())) == T) {
                        SL->SetSelected(SL->Items()->Item(I));
                        break;
                    }
                SoundListViewSelectItem(Sender, SL->Selected(), SL->Selected() != nullptr);
            }
            break;
        case 12:
            if (ui.ListView->Selected() && RenameMediaFiles(this, GameOf(ui.ListView->Selected()), GameDB, rfSounds)) TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
    }
}

void TDFendRebornMainForm::VideoMenuWork(QObject *Sender)
{
    QString S, T;
    TGame *G;
    TListView *SL = ui.VideoListView;

    switch (Tag(Sender)) {
        case 0:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                {
                    TStringList St1, St2;
                    for (int I = 0; I < SL->Items()->Count(); I++) {
                        if (I < SL->Selected()->Index()) St1.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                        if (I > SL->Selected()->Index()) St2.Add(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption());
                    }
                    PlayVideoDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), &St1, &St2);
                }
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 1: ListViewSelectItem(Sender, ui.ListView->Selected(), true); break;
        case 2:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                T = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
                if (T != "" && T != "INTERNAL") {
                    PlayVideoDialog(this, IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), nullptr, nullptr);
                    return;
                }
                ShellOpen(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption());
            }
            break;
        case 3:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption();
                TSaveDialog SaveDialog(this);
                T = ExtractFileExt(S);
                if (T != "" && T[0] == '.') T = Copy(T, 2, MaxInt);
                SaveDialog.DefaultExt = T;
                SaveDialog.FileName = "";
                SaveDialog.Title = LanguageSetup->VideoCaptureSaveTitle();
                SaveDialog.Filter = LanguageSetup->VideoCaptureSaveFilter();
                if (!SaveDialog.Execute()) return;
                T = SaveDialog.FileName;
                if (!CopyFile(S, T, true)) MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {S, T}), mtError, mbOK, 0);
            }
            break;
        case 6:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption(), ftMediaViewer))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Selected()->Caption()}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            }
            break;
        case 7:
            S = SelectedCaptureFolder(ui.ListView);
            if (S == "") return;
            for (int I = 0; I < SL->Items()->Count(); I++) if (!ExtDeleteFile(IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption(), ftMediaViewer)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {IncludeTrailingPathDelimiter(S) + SL->Items()->Item(I)->Caption()}), mtError, mbOK, 0);
                break;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 8:
            if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) {
                MessageDlg(LanguageSetup->MessageNoGameSelected(), mtError, mbOK, 0); return;
            }
            G = GameOf(ui.ListView->Selected());
            S = Trim(G->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (S == "") return;
            OpenDialog->Title = LanguageSetup->VideoCaptureImportTitle();
            OpenDialog->Filter = LanguageSetup->VideoCaptureImportFilter();
            if (!OpenDialog->Execute()) return;
            S = IncludeTrailingPathDelimiter(S) + ExtractFileName(OpenDialog->FileName);
            if (!CopyFile(OpenDialog->FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->FileName, S}), mtError, mbOK, 0);
                return;
            }
            ListViewSelectItem(Sender, ui.ListView->Selected(), true);
            break;
        case 11:
            if (SL->Selected() != nullptr) {
                S = SelectedCaptureFolder(ui.ListView);
                if (S == "") return;
                S = IncludeTrailingPathDelimiter(S);
                T = ChangeFileExt(SL->Selected()->Caption(), "");
                if (!InputQuery(LanguageSetup->ScreenshotPopupRenameCaption(), LanguageSetup->ScreenshotPopupRenameLabel(), T)) return;
                T = T + ExtractFileExt(SL->Selected()->Caption());
                if (!RenameFile(S + SL->Selected()->Caption(), S + T))
                    MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {S + SL->Selected()->Caption(), S + T}), mtError, mbOK, 0);
                ListViewSelectItem(Sender, ui.ListView->Selected(), true);
                T = Trim(ExtUpperCase(T));
                for (int I = 0; I < SL->Items()->Count(); I++)
                    if (Trim(ExtUpperCase(SL->Items()->Item(I)->Caption())) == T) {
                        SL->SetSelected(SL->Items()->Item(I));
                        break;
                    }
                VideoListViewSelectItem(Sender, SL->Selected(), SL->Selected() != nullptr);
            }
            break;
        case 12:
            if (ui.ListView->Selected() && RenameMediaFiles(this, GameOf(ui.ListView->Selected()), GameDB, rfVideos)) TreeViewChange(Sender, ui.TreeView->currentItem());
            break;
    }
}

// ---------------------------------------------------------------------------
// Popup menus of tree view and tool bar, timers, application events
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::TreeViewPopupEditUserFiltersClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S, T;

    S = GamesListSaveColWidthsToString(ui.ListView);
    T = GetUserCols();
    if (!ShowTreeListSetupDialog(this, GameDB, SearchLinkFile)) return;
    AfterSetupDialog(S, T);
}

void TDFendRebornMainForm::ToolbarPopupClick(QObject *Sender)
{
    QString S, T;

    S = GamesListSaveColWidthsToString(ui.ListView);
    T = GetUserCols();
    switch (Tag(Sender)) {
        case 0: if (!ShowToolbarSetupDialog(this, GameDB, SearchLinkFile)) return; break;
        case 1: if (!ShowIconSetSetupDialog(this, GameDB, SearchLinkFile)) return; break;
        case 2: MenuWork(ui.MenuViewShowMenubar); return;
    }
    AfterSetupDialog(S, T);
}

void TDFendRebornMainForm::IdleAddonTimerTimer(QObject *Sender)
{
    bool Done;

    Done = false;
    ApplicationEventsIdle(Sender, Done);
}

bool TDFendRebornMainForm::ApplicationEventsHelp(quint16 Command, int Data, bool &CallHelp)
{
    if (Command == HELP_CONTEXT && Data == 0) {
        CallHelp = false;
        ApplicationHelpCommand(HELP_CONTEXT, ID_Index);
    }
    return false;
}

static void ProcessObj(TGameDB *&Obj)
{
    delete Obj;
    Obj = nullptr;
}

static bool GameDBCheck = false;

void TDFendRebornMainForm::ApplicationEventsIdle(QObject *Sender, bool &Done)
{
    Q_UNUSED(Done);
    if (GameDB == nullptr) return;

    RunUpdateCheckIdleCloseHandle();

    if (DOSBoxCounter->Count() > LastDOSBoxCount) {
        LastTop = y();
        LastLeft = x();
        LastDOSBoxCount = DOSBoxCounter->Count();
    }

    if (DOSBoxCounter->Count() < LastDOSBoxCount) {
        if (LastDOSBoxStartTime > 0 && LastDOSBoxStartTime + quint32(Min(60, Max(1, PrgSetup->DOSBoxStartFailedTimeout()))) * 1000 > GetTickCount()) {
            LastDOSBoxStartTime = 0;
            ShowDOSBoxFailedDialog(this, GameDB, LastDOSBoxProfile);
            LastDOSBoxProfile = nullptr;
        } else {
            LastDOSBoxCount = DOSBoxCounter->Count();
        }
    }

    if (PrgSetup->MinimizeOnDosBoxStart() && PrgSetup->RestoreWhenDOSBoxCloses() && isMinimized() && MinimizedAtDOSBoxStart) {
        if (DOSBoxCounter->Count() > 0) return;
        MinimizedAtDOSBoxStart = false;
        if (PrgSetup->MinimizeToTray()) {
            TrayIconDblClick(Sender);
        } else {
            ApplicationRestore();
        }
    }

    if (PrgSetup->MinimizeOnScummVMStart() && PrgSetup->RestoreWhenScummVMCloses() && isMinimized() && MinimizedAtScummVMStart) {
        if (ScummVMCounter->Count() > 0) return;
        MinimizedAtScummVMStart = false;
        if (PrgSetup->MinimizeToTray()) {
            TrayIconDblClick(Sender);
        } else {
            ApplicationRestore();
        }
    }

    if (PrgSetup->MinimizeOnWindowsGameStart() && PrgSetup->RestoreWhenWindowsGameCloses() && isMinimized() && MinimizedAtWindowsGameStart) {
        if (WindowsGameCounter->Count() > 0) return;
        MinimizedAtWindowsGameStart = false;
        if (Now() - MinimizedAtWindowsGameStartTime < 5.0 / 86400) return;
        if (PrgSetup->MinimizeToTray()) {
            TrayIconDblClick(Sender);
        } else {
            ApplicationRestore();
        }
    }

    if (FileConflictCheckRunning) return;
    FileConflictCheckRunning = true;
    try {
        if (hScreenshotsChangeNotification != cINVALID_HANDLE_VALUE && ScreenshotsChanged) {
            ScreenshotsChanged = false;
            UpdateScreenshotList();
            PictureCache->Clear();
            // FindNextChangeNotification: watch new sub folders, too
            if (ScreenshotsWatcher) AddDirTreeToWatcher(ScreenshotsWatcher, ExcludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir())));
        }

        if (hConfsChangeNotification != cINVALID_HANDLE_VALUE && ConfsChanged) {
            ConfsChanged = false;
            ConfFileCheck();
        }
    } catch (...) {
        FileConflictCheckRunning = false;
        throw;
    }
    FileConflictCheckRunning = false;

    if (InstanceEventSignaled(hEvent)) {
        if (TrayIcon->isVisible()) TrayIconDblClick(Sender); else {
            if (isMinimized()) ApplicationRestore(); else {
                raise();
                activateWindow();
                QWidget *TopWnd = QApplication::activeModalWidget();
                if (TopWnd != nullptr && TopWnd != this && TopWnd->isVisible() && TopWnd->isEnabled()) {
                    TopWnd->raise();
                    TopWnd->activateWindow();
                }
            }
        }
    }

    if (!GameDBCheck && Pos(QString(QChar(68)) + "-" + QString(QChar(70)) + "end", windowTitle()) < 1) { ProcessObj(GameDB); GameDBCheck = true; }
}

void TDFendRebornMainForm::PostResize()
{
    ui.ListView->Arrange();
    ui.ScreenshotListView->Arrange();
    ui.SoundListView->Arrange();
    ui.VideoListView->Arrange();
}

void TDFendRebornMainForm::FormResize(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (JustStarted) return;

    QTimer::singleShot(0, this, [this]() { PostResize(); });   // PostMessage(Handle,WM_User+2,0,0)

    if (isMinimized() || isMaximized()) return;
    QRect BoundsRect = frameGeometry();
    int WorkAreaWidth = screen() ? screen()->availableGeometry().width() : 0;
    int WorkAreaHeight = screen() ? screen()->availableGeometry().height() : 0;
    if (BoundsRect.left() < 0 && BoundsRect.top() < 0 && BoundsRect.right() + 1 >= WorkAreaWidth - 2 && BoundsRect.bottom() + 1 >= WorkAreaHeight - 2) return;

    SaveBoundsRect = geometry();
    SaveMaximizedState = isMaximized();

    if (ui.ListView != nullptr && ui.ListView->ViewStyle() == vsSmallIcon) ui.ListView->Arrange();
}

void TDFendRebornMainForm::ApplicationEventsMinimize(QObject *Sender)
{
    Q_UNUSED(Sender);
    // D-Fend Reborn: without a system tray (e.g. GNOME without a tray
    // extension) the window is only minimized - a hidden window could not be
    // restored then
    if (PrgSetup->MinimizeToTray() && QSystemTrayIcon::isSystemTrayAvailable()) {
        TrayIcon->setVisible(true);
        if (QuickStartForm) QuickStartForm->setVisible(false);
        QTimer::singleShot(0, this, [this]() { if (isMinimized()) hide(); });   // Visible:=False
    }

    TrimWorkingSet();
}

void TDFendRebornMainForm::ApplicationEventsRestore(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (PrgSetup->MinimizeToTray()) {
        TrayIcon->setVisible(false);
        show();
    }
    ForceForegroundWindow(winId());
    raise();
    activateWindow();
    if (FBorderStyleNone) {
        move(0, 0);
    } else {
        if (!SaveMaximizedState && !isMaximized() && SaveBoundsRect.isValid()) setGeometry(SaveBoundsRect);
    }
}

void TDFendRebornMainForm::TrayIconDblClick(QObject *Sender)
{
    ApplicationRestore();
    if (StartTrayMinimize) {
        StartTrayMinimize = false;
        ProcessMessages();
        ApplicationEventsRestore(Sender);
        setWindowState(windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized));
    }
}

void TDFendRebornMainForm::CapturePageControlChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    QTabWidget *PC = ui.CapturePageControl;
    if (PC->currentIndex() == PC->indexOf(ui.DataFilesTab)) {
        if (ui.ListView->Selected() != nullptr) {
            ViewFilesFrame->setVisible(false);
            ViewFilesFrame->setVisible(true);
            ViewFilesFrame->SetGame(GameOf(ui.ListView->Selected()));
        } else {
            ViewFilesFrame->setVisible(false);
        }
    }
    if (PC->currentIndex() == PC->indexOf(ui.GameNotesPanel)) {
        if (ui.ListView->Selected() != nullptr) {
            ui.GameNotesEdit->setVisible(true);
            ui.GameNotesToolBar->setEnabled(true);
        } else {
            ui.GameNotesEdit->setVisible(true);
            ui.GameNotesEdit->setVisible(false);
            ui.GameNotesToolBar->setEnabled(false);
        }
    }
}

// ---------------------------------------------------------------------------
// Game notes
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::GameNotesButtonWork(QObject *Sender)
{
    switch (Tag(Sender)) {
        case 0: ui.GameNotesEdit->cut(); break;
        case 1: ui.GameNotesEdit->copy(); break;
        case 2: ui.GameNotesEdit->paste(); break;
        case 3: ui.GameNotesEdit->undo(); break;
    }
}

void TDFendRebornMainForm::GameNotesEditEnter(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.MenuRunGame->setShortcut(QKeySequence());
    ui.MenuRunSetup->setShortcut(QKeySequence());
    ui.MenuProfileAdd->setShortcut(QKeySequence());
    ui.MenuProfileEdit->setShortcut(QKeySequence());
    ui.MenuProfileCopy->setShortcut(QKeySequence());
    ui.MenuProfileDeinstall->setShortcut(QKeySequence());
    ui.PopupRunGame->setShortcut(QKeySequence());
    ui.PopupRunSetup->setShortcut(QKeySequence());
    ui.PopupEdit->setShortcut(QKeySequence());
    ui.PopupCopy->setShortcut(QKeySequence());
    ui.PopupDeinstall->setShortcut(QKeySequence());
}

void TDFendRebornMainForm::GameNotesEditExit(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.MenuRunGame->setShortcut(QKeySequence(Qt::Key_Return));
    ui.MenuRunSetup->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.MenuProfileAdd->setShortcut(QKeySequence(Qt::Key_Insert));
    ui.MenuProfileEdit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    ui.MenuProfileCopy->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Insert));
    ui.MenuProfileDeinstall->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    ui.PopupRunGame->setShortcut(QKeySequence(Qt::Key_Return));
    ui.PopupRunSetup->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.PopupEdit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    ui.PopupCopy->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Insert));
    ui.PopupDeinstall->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
}

// ---------------------------------------------------------------------------
// Profile files changed outside of the program
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::ConfFileCheck()
{
    TGame *G, *Game;
    TSearchRec Rec;
    bool NeedListUpdate;
    int I;

    {
        TStringList St, St2;
        for (I = 0; I < GameDB->Count(); I++) switch (GameDB->Game(I)->CheckAndUpdateTimeStamp()) {
            case fcsChanged: St.AddObject(GameDB->Game(I)->CacheName, GameDB->Game(I)); break;
            case fcsDeleted: St2.AddObject(GameDB->Game(I)->CacheName, GameDB->Game(I)); break;
            default: break;
        }
        if (St.Count() > 0 || St2.Count() > 0) {
            G = nullptr; if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected());
            if (St2.Count() > 0) ui.ListView->Items()->Clear();
            if (St.Count() > 0) ShowCacheChooseDialog(this, &St, GameDB);
            if (St2.Count() > 0) ShowRestoreDeletedDialog(this, &St2, GameDB);
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(this, ui.TreeView->currentItem());
            SelectGame(G);
        }
    }

    {
        TStringList St;
        for (I = 0; I < GameDB->Count(); I++) St.Add(Trim(ExtUpperCase(ExtractFileName(GameDB->Game(I)->SetupFile()))));
        NeedListUpdate = false;
        G = nullptr; if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected());
        I = FindFirst(PrgDataDir() + GameListSubDir + PathDelim + "*.prof", faAnyFile, Rec);
        while (I == 0) {
            if (St.IndexOf(Trim(ExtUpperCase(Rec.Name))) < 0) {
                Game = new TGame(PrgDataDir() + GameListSubDir + PathDelim + Rec.Name);
                GameDB->Add(Game);
                NeedListUpdate = true;
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);
        if (NeedListUpdate) {
            InitTreeViewForGamesList(ui.TreeView, GameDB);
            TreeViewChange(this, ui.TreeView->currentItem());
            SelectGame(G);
        }
    }
}

// ---------------------------------------------------------------------------
// Drag & drop
// ---------------------------------------------------------------------------

void TDFendRebornMainForm::DropImportFile(const QString &FileName, QString &ErrorCode)
{
    QString FileExt;
    TGame *G;
    QString S;
    bool B;

    StopCaptureChangeNotify();
    struct RestartNotify {
        TDFendRebornMainForm *F;
        ~RestartNotify() { F->StartCaptureChangeNotify(); }
    } Finally{this};

    auto ImportFinished = [this](TGame *AG) {
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        TreeViewChange(this, ui.TreeView->currentItem());
        SelectGame(AG);
    };
    auto ClearList = [this]() {
        LastSelectedGame = nullptr;
        ui.ListView->SetSelected(nullptr);
        ui.ListView->Items()->Clear();
        ViewFilesFrame->SetGame(nullptr);
    };
    auto CopyToCaptureFolder = [&](const QString &NoProfileError) {
        if (G != nullptr) {
            S = Trim(G->CaptureFolder());
            if (S != "") S = MakeAbsPath(S, PrgSetup->BaseDir());
            if (S == "") return;
            S = IncludeTrailingPathDelimiter(S) + ExtractFileName(FileName);
            if (FileExists(S)) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            }
            if (!CopyFile(FileName, S, false))
                ErrorCode = Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S});
        } else {
            ErrorCode = Format(NoProfileError, {ExtractFileName(FileName)});
        }
    };

    ErrorCode = "";

    FileExt = Trim(ExtUpperCase(ExtractFileExt(FileName)));
    G = nullptr; if (ui.ListView->Selected() != nullptr) G = GameOf(ui.ListView->Selected());

    if (DirectoryExists(FileName)) {
        B = false;
        if (G != nullptr && Trim(G->DataDir()) != "" && DirectoryExists(MakeAbsPath(G->DataDir(), PrgSetup->BaseDir()))) {
            B = (MessageDlg(Format(LanguageSetup->DragDropImportFolderConfirmation(), {FileName, G->Name()}), mtConfirmation, mbYes | mbNo, 0) != mrYes);
        }
        if (B) {
            /*Copy to DataDir*/
            S = IncludeTrailingPathDelimiter(MakeAbsPath(G->DataDir(), PrgSetup->BaseDir())) + ExtractFileName(ExcludeTrailingPathDelimiter(FileName));
            if (!ForceDirectories(S)) { ErrorCode = Format(LanguageSetup->MessageCouldNotCreateDir(), {S}); return; }
            if (!CopyFiles(FileName, S, true, true)) ErrorCode = Format(LanguageSetup->MessageCouldNotCopyFiles(), {FileName, S});
        } else {
            /*Import folder as game*/
            SetListViewCustomDraw(false);
            G = ImportFolder(this, FileName, GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
            if (G == nullptr) { SetListViewCustomDraw(true); return; }
            ClearList();
            SetListViewCustomDraw(true);
            ImportFinished(G);
        }
        return;
    }

    if (FileExt == ".CONF") {
        G = ImportConfFile(GameDB, FileName);
        if (G == nullptr) return;
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        TreeViewChange(nullptr, ui.TreeView->currentItem());
        SelectGame(G);
        return;
    }

    if (FileExt == ".PROF") {
        S = GameDB->ProfFileName(ExtractFileName(FileName), true);
        if (!CopyFile(FileName, S, true)) {
            ErrorCode = Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S});
            return;
        }
        G = new TGame(S);
        GameDB->Add(G);
        InitTreeViewForGamesList(ui.TreeView, GameDB);
        TreeViewChange(nullptr, ui.TreeView->currentItem());
        SelectGame(G);
        return;
    }

    if (FileExt == ".JPG" || FileExt == ".JPEG" || FileExt == ".PNG" || FileExt == ".GIF" || FileExt == ".BMP") {
        CopyToCaptureFolder(LanguageSetup->DragDropErrorNoProfileSelectedForScreenshots());
        return;
    }

    if (FileExt == ".WAV" || FileExt == ".MP3" || FileExt == ".OGG" || FileExt == ".MID" || FileExt == ".MIDI") {
        CopyToCaptureFolder(LanguageSetup->DragDropErrorNoProfileSelectedForSoundFiles());
        return;
    }

    if (FileExt == ".AVI" || FileExt == ".MPEG" || FileExt == ".MPG" || FileExt == ".WMV" || FileExt == ".ASF") {
        CopyToCaptureFolder(LanguageSetup->DragDropErrorNoProfileSelectedForVideoFiles());
        return;
    }

    if (FileExt == ".EXE" || FileExt == ".COM" || FileExt == ".BAT" || FileExt == ".BAS") {
        if (FileExt == ".EXE") B = IsWindowsExe(FileName); else B = false;
        StartWizard(FileName, B);
        return;
    }

    B = (FileExt == ".ZIP" || FileExt == ".7Z");
    if (!B) for (int I = 0; I < PrgSetup->PackerSettingsCount(); I++) if (ExtensionInList(FileExt, PrgSetup->PackerSettings(I)->FileExtensions())) { B = true; break; }
    if (B) {
        SetListViewCustomDraw(false);
        G = ImportZipPackage(this, FileName, GameDB, PrgSetup->ImportZipWithoutDialogIfPossible());
        if (G == nullptr) { SetListViewCustomDraw(true); return; }
        ClearList();
        SetListViewCustomDraw(true);
        ImportFinished(G);
        return;
    }

    if (FileExt == ".IMG" || FileExt == ".IMA") {
        SetListViewCustomDraw(false);
        G = RunInstallationFromDiskImage(this, GameDB, FileName);
        if (G == nullptr) { SetListViewCustomDraw(true); return; }
        ClearList();
        SetListViewCustomDraw(true);
        ImportFinished(G);
        return;
    }

    if (FileExt == ".ISO" || FileExt == ".CUE" || FileExt == ".BIN") {
        SetListViewCustomDraw(false);
        G = RunInstallationFromCDImage(this, GameDB, FileName);
        if (G == nullptr) { SetListViewCustomDraw(true); return; }
        ClearList();
        SetListViewCustomDraw(true);
        ImportFinished(G);
        return;
    }

    /*User-Interpreters*/
    for (int I = 0; I < PrgSetup->DOSBoxBasedUserInterpretersExtensions()->Count(); I++) {
        TStringList *St = ValueToList((*PrgSetup->DOSBoxBasedUserInterpretersExtensions())[I]);
        for (int J = 0; J < St->Count(); J++) {
            S = Trim(ExtUpperCase((*St)[J])); if (S != "" && S[0] != '.') S = "." + S;
            if (S == FileExt) {
                delete St;
                StartWizard(FileName, false);
                return;
            }
        }
        delete St;
    }

    /*Otherwise try to copy the file to the game DataDir*/
    if (G != nullptr) {
        if (Trim(G->DataDir()) != "" && DirectoryExists(MakeAbsPath(G->DataDir(), PrgSetup->BaseDir()))) {
            S = IncludeTrailingPathDelimiter(MakeAbsPath(G->DataDir(), PrgSetup->BaseDir())) + ExtractFileName(FileName);
            if (FileExists(S)) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            }
            if (!CopyFile(FileName, S, false))
                ErrorCode = Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S});
            return;
        }
    } else {
        ErrorCode = Format(LanguageSetup->DragDropErrorUnknownExtension(), {ExtractFileName(FileName)});
    }
}

void TDFendRebornMainForm::MainDragDrop(QObject *Sender, TStringList *Source, int X, int Y)
{
    Q_UNUSED(Sender);
    Q_UNUSED(X);
    Q_UNUSED(Y);
    TStringList *FileList;
    QString S;

    if (Source == nullptr) return;
    FileList = Source;

    TStringList Errors;
    for (int I = 0; I < FileList->Count(); I++) {
        S = ""; DropImportFile((*FileList)[I], S);
        if (S != "") Errors.Add(S);
    }
    if (Errors.Count() > 0) ShowDragNDropErrorDialog(this, &Errors);
}

void TDFendRebornMainForm::CaptureDragDrop(QObject *Sender, TStringList *Source, int X, int Y)
{
    Q_UNUSED(Sender);
    Q_UNUSED(X);
    Q_UNUSED(Y);
    TStringList *FileList;
    QString Dir;

    if (Source == nullptr) return;
    FileList = Source;

    if (ui.ListView->Selected() == nullptr || ui.ListView->Selected()->Data() == nullptr) return;
    Dir = Trim(GameOf(ui.ListView->Selected())->CaptureFolder());
    if (Dir == "") return;
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(Dir, PrgSetup->BaseDir()));
    if (!ForceDirectories(Dir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dir}), mtError, mbOK, 0);
        return;
    }

    TStringList Errors;
    for (int I = 0; I < FileList->Count(); I++) {
        const QString F = (*FileList)[I];
        if (DirectoryExists(F)) {
            Errors.Add(Format(LanguageSetup->MessageCouldNotCopyDirectoriesToCapture(), {F}));
            continue;
        }
        if (FileExists(Dir + ExtractFileName(F))) {
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {Dir + ExtractFileName(F)}), mtConfirmation, mbYes | mbNo, 0) != mrYes) continue;
        }
        if (!CopyFile(F, Dir + ExtractFileName(F), false)) {
            Errors.Add(Format(LanguageSetup->MessageCouldNotCopyFile(), {F, Dir + ExtractFileName(F)}));
        }
    }
    if (Errors.Count() > 0) ShowDragNDropErrorDialog(this, &Errors);
}

void TDFendRebornMainForm::WMMove()
{
    bool Restore = (DOSBoxCounter->Count() == 0 && LastDOSBoxCount > 0 && LastTop >= 0 && LastLeft >= 0 && !SaveMaximizedState);
    if (Restore) {
        LastDOSBoxCount = DOSBoxCounter->Count();
        if (x() != LastLeft || y() != LastTop) move(LastLeft, LastTop);
    }
    LastDOSBoxCount = DOSBoxCounter->Count();
    LastTop = y();
    LastLeft = x();
    FormResize(this);
}

void TDFendRebornMainForm::ZipInfoNotify(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.ZipInfoPanel->setVisible(ZipManager->Count() > 0);
}

void TDFendRebornMainForm::ZipInfoPanelDblClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ShowZipWaitInfoDialog(this);
}

void TDFendRebornMainForm::FormClose(QObject *Sender, TCloseAction &Action)
{
    Q_UNUSED(Sender);
    if (Action == caHide) { delete HTMLhelpRouter; HTMLhelpRouter = nullptr; }
}

void TDFendRebornMainForm::FormCloseQuery(QObject *Sender, bool &CanClose)
{
    Q_UNUSED(Sender);
    if (ZipManager->Count() > 0) {
        if (MessageDlg(LanguageSetup->ZipFormCloseWarning(), mtConfirmation, mbYes | mbNo, 0) != mrYes) CanClose = false;
    }
}

void TDFendRebornMainForm::AddProfileFromQuickStarter(QObject *Sender, const QString &FileName, bool WindowsProfile, bool UseWizard, int TemplateNr)
{
    Q_UNUSED(Sender);
    QString S;

    if (UseWizard) {
        StartWizard(MakeRelPath(FileName, PrgSetup->BaseDir()), WindowsProfile);
    } else {
        if (WindowsProfile) S = "Windows"; else S = "";
        AddProfile(MakeRelPath(FileName, PrgSetup->BaseDir()), S, TemplateNr);
    }
}

void TDFendRebornMainForm::QuickStarterCloseNotify(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.MenuViewsQuickStarter->setChecked(false);
}

void TDFendRebornMainForm::DisplayResolutionChange(int BitsPerPixel)
{
    if (BitsPerPixel <= 8) return;
    ListViewSelectItem(this, ui.ListView->Selected(), ui.ListView->Selected() != nullptr);
}

void TDFendRebornMainForm::FirstRunInfoButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.FirstRunInfoPanel->setVisible(false);
    PrgSetup->SetShowAddGameInfoOnEmptyGamesList(false);
}

// IDropTarget implementation (links dragged from a web browser)

static bool DragDropExHasText(const QMimeData *DataObj)
{
    return DataObj && DataObj->hasText();
}

static bool DragDropExHasFiles(const QMimeData *DataObj)
{
    if (!DataObj || !DataObj->hasUrls()) return false;
    for (const QUrl &U : DataObj->urls()) if (!U.isLocalFile()) return false;
    return !DataObj->urls().isEmpty();
}

long TDFendRebornMainForm::DragEnter(const QMimeData *dataObj, int grfKeyState, QPoint pt, int &dwEffect)
{
    Q_UNUSED(grfKeyState);
    Q_UNUSED(pt);
    bool B1, B2, B3, B5;
    long Result;

    QWidget *Modal = QApplication::activeModalWidget();
    B1 = (Modal == nullptr || Modal == this);
    B2 = (QuickStartForm != nullptr && Modal == QuickStartForm);
    B3 = DragDropExHasText(dataObj);
    B5 = DragDropExHasFiles(dataObj);

    if ((B1 || B2) && (B3 || B5)) {
        dwEffect = cDROPEFFECT_LINK;
        Result = cS_OK;
        LastDragDropEffect = cDROPEFFECT_LINK;
    } else {
        Result = cS_FALSE;
        LastDragDropEffect = cDROPEFFECT_NONE;
    }
    return Result;
}

long TDFendRebornMainForm::DragOver(int grfKeyState, QPoint pt, int &dwEffect)
{
    Q_UNUSED(grfKeyState);
    Q_UNUSED(pt);
    dwEffect = LastDragDropEffect;
    return cS_OK;
}

long TDFendRebornMainForm::DragLeave()
{
    LastDragDropEffect = cDROPEFFECT_NONE;
    return cS_OK;
}

long TDFendRebornMainForm::Drop(const QMimeData *dataObj, int grfKeyState, QPoint pt, int &dwEffect)
{
    Q_UNUSED(grfKeyState);
    Q_UNUSED(pt);
    QString S, T;
    TGame *G;
    long Result;

    // Windows Explorer does not offer CF_TEXT for files, but most Linux file
    // managers add the file URLs as text: dropped local files always take the
    // file path (CF_HDROP).
    if (DragDropExHasFiles(dataObj)) return 2; /*if not a link use the normal handler to handle droped files etc.*/

    if (DragDropExHasText(dataObj)) {
        dwEffect = cDROPEFFECT_COPY;
        Result = cS_OK;
        S = dataObj->text().trimmed();
        T = ExtUpperCase(S);
        if (Copy(T, 1, 7) == QString("HTTP:/") + "/" || Copy(T, 1, 8) == QString("HTTPS:/") + "/" || Copy(T, 1, 6) == QString("FTP:/") + "/") {
            G = DonwloadAndInstall(this, S, GameDB);
            if (G != nullptr) {
                InitTreeViewForGamesList(ui.TreeView, GameDB);
                TreeViewChange(this, ui.TreeView->currentItem());
                SelectGame(G);
            }
        }
    } else {
        Result = cS_FALSE;
    }
    return Result;
}
