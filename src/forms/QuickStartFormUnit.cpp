// C++ port of QuickStartFormUnit.pas (implementation section).
#include "QuickStartFormUnit.h"
#include "VirtualKeys.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "DosBoxUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "ViewImageFormUnit.h"
#include "PlaySoundFormUnit.h"
#include "PlayVideoFormUnit.h"
#include "GameDBToolsUnit.h"
#include "DOSBoxTempUnit.h"
#include "MainUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TQuickStartForm* QuickStartForm = nullptr;
bool JustShowingOrHidingQuickStartForm = false;

namespace {

enum TOpenType { otNone, otOpen, otDOSBox, otImageViewer, otSoundPlayer, otVideoPlayer, otEditor };
enum TFileTypeOptions { ftAddEditItem = 1, ftCanMakeProfileOf = 2, ftRunWithParameters = 4, ftDOSExe = 8, ftWindowsExe = 16 };
using TFileTypeOptionsSet = int;

struct TFileTypeInfo {
    const char *Ext;   // uppercase and with leading "."
    int NameID;
    int ImageIndex;
    TOpenType OpenType;
    TFileTypeOptionsSet Options;
};

const TFileTypeInfo FileTypes[55] = {
    {".EXE", NR_QuickStarterColumnsTypeDOS, 9, otDOSBox, ftCanMakeProfileOf | ftDOSExe | ftRunWithParameters},
    {".EXE", NR_QuickStarterColumnsTypeWindows, 10, otOpen, ftCanMakeProfileOf | ftWindowsExe | ftRunWithParameters},
    {".COM", NR_QuickStarterColumnsTypeDOS, 9, otDOSBox, ftCanMakeProfileOf | ftRunWithParameters},
    {".BAT", NR_QuickStarterColumnsTypeBatch, 9, otDOSBox, ftCanMakeProfileOf | ftAddEditItem | ftRunWithParameters},

    {".TXT", NR_QuickStarterColumnsTypeText, 8, otEditor, 0},
    {".NFO", NR_QuickStarterColumnsTypeText, 8, otEditor, 0},
    {".DIZ", NR_QuickStarterColumnsTypeText, 8, otEditor, 0},
    {".1ST", NR_QuickStarterColumnsTypeText, 8, otEditor, 0},

    {".INI", NR_QuickStarterColumnsTypeConfiguration, 8, otEditor, 0},
    {".CFG", NR_QuickStarterColumnsTypeConfiguration, 8, otEditor, 0},

    {".JPG", NR_QuickStarterColumnsTypeImage, 11, otImageViewer, 0},
    {".JPEG", NR_QuickStarterColumnsTypeImage, 11, otImageViewer, 0},
    {".BMP", NR_QuickStarterColumnsTypeImage, 11, otImageViewer, 0},
    {".PNG", NR_QuickStarterColumnsTypeImage, 11, otImageViewer, 0},
    {".GIF", NR_QuickStarterColumnsTypeImage, 11, otImageViewer, 0},

    {".ICO", NR_QuickStarterColumnsTypeIcon, 11, otOpen, 0},

    {".WAV", NR_QuickStarterColumnsTypeSound, 12, otSoundPlayer, 0},
    {".MP3", NR_QuickStarterColumnsTypeSound, 12, otSoundPlayer, 0},
    {".OGG", NR_QuickStarterColumnsTypeSound, 12, otSoundPlayer, 0},
    {".MID", NR_QuickStarterColumnsTypeSound, 12, otSoundPlayer, 0},
    {".MIDI", NR_QuickStarterColumnsTypeSound, 12, otSoundPlayer, 0},

    {".AVI", NR_QuickStarterColumnsTypeVideo, 19, otVideoPlayer, 0},
    {".MPG", NR_QuickStarterColumnsTypeVideo, 19, otVideoPlayer, 0},
    {".MPEG", NR_QuickStarterColumnsTypeVideo, 19, otVideoPlayer, 0},
    {".WMV", NR_QuickStarterColumnsTypeVideo, 19, otVideoPlayer, 0},
    {".ASF", NR_QuickStarterColumnsTypeVideo, 19, otVideoPlayer, 0},

    {".PDF", NR_QuickStarterColumnsTypePDF, 8, otOpen, 0},

    {".HTM", NR_QuickStarterColumnsTypeHTML, 14, otOpen, ftAddEditItem},
    {".HTML", NR_QuickStarterColumnsTypeHTML, 14, otOpen, ftAddEditItem},

    {".RTF", NR_QuickStarterColumnsTypeRTF, 8, otOpen, 0},

    {".DOC", NR_QuickStarterColumnsTypeText, 8, otOpen, 0},

    {".ZIP", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".7Z", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".RAR", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".ARJ", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".LHA", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".LZH", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".BZ2", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".BZIP2", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".CAB", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".GZ", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".GZIP", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".TAR", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".ZOO", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},
    {".ARC", NR_QuickStarterColumnsTypeArchive, 13, otOpen, 0},

    {".IMG", NR_QuickStarterColumnsTypeHDImage, 7, otOpen, 0},
    {".IMA", NR_QuickStarterColumnsTypeHDImage, 7, otOpen, 0},

    {".ISO", NR_QuickStarterColumnsTypeCDImage, 15, otOpen, 0},
    {".BIN", NR_QuickStarterColumnsTypeCDImage, 15, otOpen, 0},
    {".CUE", NR_QuickStarterColumnsTypeCDImage, 15, otOpen, 0},

    {".DLL", NR_QuickStarterColumnsTypeDLL, 8, otNone, 0},

    {".SYS", NR_QuickStarterColumnsTypeDOSDriver, 9, otNone, 0},
    {".DRV", NR_QuickStarterColumnsTypeDOSDriver, 9, otNone, 0},

    {"" /*no extension*/, 0, 8, otEditor, 0},

    {"" /*unknown extension*/, 0, 8, otOpen, ftAddEditItem}   // must be last element in array
};
constexpr int FileTypesHigh = 54;

// TTreeNode.ImageIndex / SelectedIndex / HasChildren
constexpr int RoleImageIndex = Qt::UserRole + 1;
constexpr int RoleSelectedIndex = Qt::UserRole + 2;
constexpr int RoleHasChildren = Qt::UserRole + 3;

int ItemType(TListItem *Item) { return static_cast<int>(reinterpret_cast<intptr_t>(Item->Data())); }

QString MakePathShort(const QString &S)
{
#ifdef Q_OS_WIN
    wchar_t Temp[MAX_PATH + 10];
    if (GetShortPathNameW(reinterpret_cast<const wchar_t *>(S.utf16()), Temp, MAX_PATH) != 0)
        return Trim(ExtUpperCase(QString::fromWCharArray(Temp)));
    return S;
#else
    return S;
#endif
}

bool IsDelim(QChar C) { return C == '\\' || C == '/'; }

}  // namespace

// TQuickStartForm

TQuickStartForm::TQuickStartForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    // Position:=poDesigned (FormCreate) - must be known before the layout engine evaluates vclPosition
    if (PrgSetup != nullptr && !PrgSetup->QuickStarterMaximized()) setProperty("vclPosition", "poDesigned");
    VclLayout::setupForm(this);

    connect(ui.CloseButton, &QAction::triggered, this, [this]() { ButtonWork(ui.CloseButton); });
    connect(ui.UpdateButton, &QAction::triggered, this, [this]() { ButtonWork(ui.UpdateButton); });
    connect(ui.StartButton, &QAction::triggered, this, [this]() { ButtonWork(ui.StartButton); });
    connect(ui.HelpButton, &QAction::triggered, this, [this]() { ButtonWork(ui.HelpButton); });
    connect(ui.TreeTypeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { TreeTypeComboBoxChange(ui.TreeTypeComboBox); });
    connect(ui.Tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *Current, QTreeWidgetItem *Previous) {
        // SelectedIndex image of the selected node
        if (Previous != nullptr && ImageList != nullptr) {
            int I = Previous->data(0, RoleImageIndex).toInt();
            if (I >= 0 && I < ImageList->Count()) Previous->setIcon(0, ImageList->Icon(I));
        }
        if (Current != nullptr && ImageList != nullptr) {
            int I = Current->data(0, RoleSelectedIndex).toInt();
            if (I >= 0 && I < ImageList->Count()) Current->setIcon(0, ImageList->Icon(I));
        }
        if (DisableCounter == 0) TreeChange(ui.Tree, Current);
    });
    connect(ui.Tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *Node) {
        bool AllowExpansion = true;
        TreeExpanding(ui.Tree, Node, AllowExpansion);
    });
    connect(ui.List, &TListView::dblClick, this, [this]() { ListDblClick(ui.List); });
    connect(ui.List, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        ListKeyDown(ui.List, K, Mods & ~Qt::KeypadModifier);
    });
    connect(ui.List, &TListView::selectItem, this, [this](TListItem *Item, bool Selected) { ListSelectItem(ui.List, Item, Selected); });
    connect(ui.TreePopupMenu, &QMenu::aboutToShow, this, [this]() { TreePopupMenuPopup(ui.TreePopupMenu); });
    connect(ui.ListPopupMenu, &QMenu::aboutToShow, this, [this]() { ListPopupMenuPopup(ui.ListPopupMenu); });
    for (QAction *A : {ui.TreeUpdate, ui.TreeCreateFolder, ui.TreeRename, ui.TreeDelete, ui.TreeExplorer, ui.TreeDOSBox,
                       ui.TreeViewModeGames, ui.TreeViewModeCapture, ui.TreeViewModeData, ui.TreeViewModeFull,
                       ui.ListRun, ui.ListRunWithParameters, ui.ListOpenInDefaultViewer, ui.ListOpenInEditor,
                       ui.ListMakeProfile, ui.ListMakeProfileWizard, ui.ListUpdate, ui.ListCreateFolder, ui.ListRename,
                       ui.ListDelete, ui.ListExplorer, ui.ListDOSBox})
        connect(A, &QAction::triggered, this, [this, A]() { MenuWork(A); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = QtKeyToVK(Key);
        if (K == 0) return false;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return K == 0;
    });

    // Tree:=TTreeViewAcceptingFiles / List:=TListViewAcceptingFiles (ActivateAcceptFiles)
    setAcceptDrops(true);

    FormCreate(this);
}

TQuickStartForm::~TQuickStartForm()
{
    FormDestroy(this);
    delete ImageList;
}

void TQuickStartForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    Working = true;
    CurrentPath.clear();
    PopupSelNode = nullptr;
    TreeBaseDir.clear();
    DisableCounter = 0;

    // HelpContext:=ID_ViewQuickStarter; (F1 is handled in FormKeyDown)

    hChangeNotification = -1;   // INVALID_HANDLE_VALUE

    Timer = new QTimer(this);
    Timer->setInterval(500);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerWork(Timer); });

    ImageList = TImageList::FromResource("QuickStartFormUnit", "ImageList");
    if (ImageList == nullptr) ImageList = new TImageList();

    // SetVistaFonts(self); NoFlicker(...) - not needed

    if (PrgSetup->QuickStarterMaximized()) {
        setWindowState(windowState() | Qt::WindowMaximized);
    } else {
        QRect Screen(0, 0, 1024, 768);
        if (QScreen *Sc = QGuiApplication::primaryScreen()) Screen = Sc->geometry();
        int L = x(), T = y(), W = width(), H = height();
        if ((PrgSetup->QuickStarterLeft() > 0) && (PrgSetup->QuickStarterLeft() < 9 * Screen.width() / 10)) L = PrgSetup->QuickStarterLeft();
        if ((PrgSetup->QuickStarterTop() > 0) && (PrgSetup->QuickStarterTop() < 9 * Screen.height() / 10)) T = PrgSetup->QuickStarterTop();
        if ((PrgSetup->QuickStarterWidth() > 0) && (L + PrgSetup->QuickStarterWidth() <= Screen.width())) W = PrgSetup->QuickStarterWidth();
        if ((PrgSetup->QuickStarterHeight() > 0) && (T + PrgSetup->QuickStarterHeight() <= Screen.height())) H = PrgSetup->QuickStarterHeight();

        if ((PrgSetup->QuickStarterLeft() < 0) && (PrgSetup->QuickStarterTop() < 0) && (PrgSetup->QuickStarterWidth() < 0) && (PrgSetup->QuickStarterHeight() < 0) && DFendReloadedMainForm != nullptr) {
            L = DFendReloadedMainForm->x() + DFendReloadedMainForm->width() / 2 - (W / 2);
            T = DFendReloadedMainForm->y() + DFendReloadedMainForm->y() / 2 - (T / 2);
        }

        resize(W, H);
        move(L, T);
        // Position:=poDesigned; (see constructor)
    }

    // GlassFrame.Enabled:=True; GlassFrame.Bottom:=BottomPanel.Height; - Vista glass, not needed
}

void TQuickStartForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (E->spontaneous() || FFormShown) return;
    FFormShown = true;
    FormShow(this);
}

void TQuickStartForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    TListColumn* C;
    int I;

    // Tree / List accept dropped files (see dragEnterEvent / dropEvent)

    C = ui.List->Columns()->Add(); C->SetCaption("Name"); C->SetWidth(-1);
    C = ui.List->Columns()->Add(); C->SetCaption("Size"); C->SetWidth(-1);
    C = ui.List->Columns()->Add(); C->SetCaption("Type"); C->SetWidth(-1);
    ui.List->SetSmallImages(ImageList);

    LoadLanguage();

    UserIconLoader->RegisterImageList(ImageList, "QuickStart");

    LoadTemplateList();
    BuildTree();

    ui.SetupComboBox1->setCurrentIndex(PrgSetup->QuickStarterDOSBoxFullscreen() ? 0 : 1);
    ui.SetupComboBox2->setCurrentIndex(PrgSetup->QuickStarterDOSBoxAutoClose() ? 0 : 1);
    I = ui.TemplateComboBox->findText(PrgSetup->QuickStarterDOSBoxTemplate(), Qt::MatchFixedString);
    if (I >= 0) ui.TemplateComboBox->setCurrentIndex(I);
}

bool TQuickStartForm::closeQuery()
{
    bool CanClose = true;
    FormCloseQuery(this, CanClose);
    return CanClose;
}

void TQuickStartForm::closeEvent(QCloseEvent *E)
{
    if (!closeQuery()) { E->ignore(); return; }
    TCloseAction Action = caHide;
    FormClose(this, Action);
    FInClose = true;
    TVclForm::closeEvent(E);
    FInClose = false;
    if (Action == caFree) deleteLater();
}

void TQuickStartForm::reject()
{
    // A VCL form without a cancel button is closed via Close (OnCloseQuery/OnClose)
    if (FInClose) { TVclForm::reject(); return; }
    close();
}

void TQuickStartForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender);
    if (FOnMenuCloseNotify) FOnMenuCloseNotify(this);

    if (Action == caHide) Action = caFree;
}

void TQuickStartForm::FormCloseQuery(QObject* Sender, bool& CanClose)
{
    Q_UNUSED(Sender);
    CanClose = (DisableCounter == 0);
}

void TQuickStartForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    UserIconLoader->UnRegisterImageList(ImageList);

    PrgSetup->SetQuickStarterMaximized(isMaximized());
    PrgSetup->SetQuickStarterLeft(x());
    PrgSetup->SetQuickStarterTop(y());
    PrgSetup->SetQuickStarterWidth(width());
    PrgSetup->SetQuickStarterHeight(height());

    PrgSetup->SetQuickStarterDOSBoxFullscreen(ui.SetupComboBox1->currentIndex() == 0);
    PrgSetup->SetQuickStarterDOSBoxAutoClose(ui.SetupComboBox2->currentIndex() == 0);
    if (ui.TemplateComboBox->currentIndex() == 0) PrgSetup->SetQuickStarterDOSBoxTemplate(QString()); else PrgSetup->SetQuickStarterDOSBoxTemplate(ui.TemplateComboBox->currentText());

#ifdef Q_OS_WIN
    if (hChangeNotification != -1) FindCloseChangeNotification(reinterpret_cast<HANDLE>(hChangeNotification));
#endif
    hChangeNotification = -1;
    if (QuickStartForm == this) QuickStartForm = nullptr;
}

void TQuickStartForm::ListPopupMenuPopup(QObject* Sender)
{
    ListSelectItem(Sender, ui.List->Selected(), true);
}

void TQuickStartForm::ListSelectItem(QObject* Sender, TListItem* Item, bool Selected)
{
    Q_UNUSED(Sender); Q_UNUSED(Selected);
    QString S;

    LoadStartButtonLanguage(Item);

    ui.ListRunWithParameters->setVisible((Item != nullptr) && (FileTypes[ItemType(Item)].Options & ftRunWithParameters));

    if ((Item != nullptr) && (FileTypes[ItemType(Item)].OpenType == otImageViewer || FileTypes[ItemType(Item)].OpenType == otSoundPlayer || FileTypes[ItemType(Item)].OpenType == otVideoPlayer)) {
        switch (FileTypes[ItemType(Item)].OpenType) {
        case otImageViewer: S = Trim(ExtUpperCase(PrgSetup->ImageViewer())); break;
        case otSoundPlayer: S = Trim(ExtUpperCase(PrgSetup->SoundPlayer())); break;
        case otVideoPlayer: S = Trim(ExtUpperCase(PrgSetup->VideoPlayer())); break;
        default: S.clear(); break;
        }
        ui.ListOpenInDefaultViewer->setVisible(S.isEmpty() || (S == "INTERNAL"));
    } else {
        ui.ListOpenInDefaultViewer->setVisible(false);
    }

    ui.ListOpenInEditor->setVisible((Item != nullptr) && (FileTypes[ItemType(Item)].Options & ftAddEditItem));

    ui.ListRename->setVisible(Item != nullptr);
    ui.ListDelete->setVisible(Item != nullptr);

    ui.ListMakeProfile->setVisible((Item != nullptr) && (FileTypes[ItemType(Item)].Options & ftCanMakeProfileOf));
    ui.ListMakeProfileWizard->setVisible((Item != nullptr) && (FileTypes[ItemType(Item)].Options & ftCanMakeProfileOf));
}

void TQuickStartForm::LoadStartButtonLanguage(TListItem* Item)
{
    if (Item == nullptr) Item = ui.List->Selected();

    ui.StartButton->setEnabled((Item != nullptr) && (FileTypes[ItemType(Item)].OpenType != otNone));

    auto SetImageIndex = [this](int I) {
        if (ImageList != nullptr && I >= 0 && I < ImageList->Count()) {
            ui.StartButton->setIcon(ImageList->Icon(I));
            ui.ListRun->setIcon(ImageList->Icon(I));
        }
    };

    if (ui.StartButton->isEnabled()) {
        SetImageIndex(FileTypes[ItemType(Item)].ImageIndex);
        if ((FileTypes[ItemType(Item)].OpenType == otDOSBox) || ((FileTypes[ItemType(Item)].OpenType == otOpen) && (FileTypes[ItemType(Item)].Options & ftWindowsExe))) {
            ui.StartButton->setText(LanguageSetup->ButtonRun());
            ui.ListRun->setText(LanguageSetup->ButtonRun());
        } else {
            ui.StartButton->setText(LanguageSetup->ButtonOpen());
            ui.ListRun->setText(LanguageSetup->ButtonOpen());
        }
    } else {
        SetImageIndex(2);
        ui.ListRun->setText(LanguageSetup->ButtonRun());
        ui.StartButton->setText(LanguageSetup->ButtonRun());
    }
    ui.ListRun->setVisible(ui.StartButton->isEnabled());
}

void TQuickStartForm::LoadLanguage()
{
    int I;

    // Font.Charset:=CharsetNameToFontCharSet(LanguageSetup.CharsetName); - not needed

    setWindowTitle(LanguageSetup->QuickStarterCaption());

    ui.CloseButton->setText(LanguageSetup->Close());
    ui.CloseButton->setToolTip(LanguageSetup->CloseHintWindow());
    ui.UpdateButton->setText(LanguageSetup->QuickStarterUpdate());
    ui.UpdateButton->setToolTip(LanguageSetup->QuickStarterUpdateInfo());
    ui.StartButton->setText(LanguageSetup->ButtonRun());
    ui.StartButton->setToolTip(LanguageSetup->ButtonRunHintFile());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.HelpButton->setToolTip(LanguageSetup->HelpHint());

    while (ui.SetupComboBox1->count() < 2) ui.SetupComboBox1->addItem(QString());
    while (ui.SetupComboBox2->count() < 2) ui.SetupComboBox2->addItem(QString());
    I = ui.SetupComboBox1->currentIndex();
    ui.SetupComboBox1->setItemText(0, LanguageSetup->QuickStarterSetupFullscreen());
    ui.SetupComboBox1->setItemText(1, LanguageSetup->QuickStarterSetupWindow());
    ui.SetupComboBox1->setCurrentIndex(Max(0, I));
    I = ui.SetupComboBox2->currentIndex();
    ui.SetupComboBox2->setItemText(0, LanguageSetup->QuickStarterSetupAutoClose());
    ui.SetupComboBox2->setItemText(1, LanguageSetup->QuickStarterSetupNoAutoClose());
    ui.SetupComboBox2->setCurrentIndex(Max(0, I));

    I = ui.TreeTypeComboBox->currentIndex();
    // TreeTypeComboBox.OnChange:=nil; (OnChange is only connected to user activation)
    while (ui.TreeTypeComboBox->count() < 4) ui.TreeTypeComboBox->addItem(QString());
    ui.TreeTypeComboBox->setItemText(0, LanguageSetup->QuickStarterTypeGamesDir());
    ui.TreeTypeComboBox->setItemText(1, LanguageSetup->QuickStarterTypeCaptureDir());
    ui.TreeTypeComboBox->setItemText(2, LanguageSetup->QuickStarterTypeDataDir());
    ui.TreeTypeComboBox->setItemText(3, LanguageSetup->QuickStarterTypeFull());
    ui.TreeTypeComboBox->setCurrentIndex(Max(0, I));

    ui.List->Column(0)->SetCaption(LanguageSetup->QuickStarterColumnsName());
    ui.List->Column(1)->SetCaption(LanguageSetup->QuickStarterColumnsSize());
    ui.List->Column(2)->SetCaption(LanguageSetup->QuickStarterColumnsType());

    I = ui.TemplateComboBox->currentIndex();
    if (ui.TemplateComboBox->count() > 0) ui.TemplateComboBox->setItemText(0, LanguageSetup->TemplateFormDefault());
    ui.TemplateComboBox->setCurrentIndex(I);

    ui.TreeUpdate->setText(LanguageSetup->QuickStarterUpdate());
    ui.TreeViewMode->setTitle(LanguageSetup->QuickStarterMenuViewMode());
    ui.TreeViewModeGames->setText(LanguageSetup->QuickStarterMenuViewModeGames());
    ui.TreeViewModeCapture->setText(LanguageSetup->QuickStarterMenuViewModeCapture());
    ui.TreeViewModeData->setText(LanguageSetup->QuickStarterMenuViewModeData());
    ui.TreeViewModeFull->setText(LanguageSetup->QuickStarterMenuViewModeFull());
    ui.TreeExplorer->setText(LanguageSetup->QuickStarterMenuExplorer());
    ui.TreeDOSBox->setText(LanguageSetup->QuickStarterMenuDOSBox());
    ui.TreeCreateFolder->setText(LanguageSetup->QuickStarterMenuCreateFolder());
    ui.TreeRename->setText(LanguageSetup->QuickStarterMenuRename());
    ui.TreeDelete->setText(LanguageSetup->QuickStarterMenuDelete());
    ui.ListRun->setText(LanguageSetup->ButtonRun());
    ui.ListRunWithParameters->setText(LanguageSetup->QuickStarterMenuRunWithParameters());
    ui.ListOpenInDefaultViewer->setText(LanguageSetup->QuickStarterMenuOpenInDefaultViewer());
    ui.ListOpenInEditor->setText(LanguageSetup->QuickStarterMenuOpenInTextEditor());
    ui.ListCreateFolder->setText(LanguageSetup->QuickStarterMenuCreateFolder());
    ui.ListRename->setText(LanguageSetup->QuickStarterMenuRename());
    ui.ListDelete->setText(LanguageSetup->QuickStarterMenuDelete());
    ui.ListMakeProfile->setText(LanguageSetup->QuickStarterMenuMakeProfile());
    ui.ListMakeProfileWizard->setText(LanguageSetup->QuickStarterMenuMakeProfileWizard());
    ui.ListUpdate->setText(LanguageSetup->QuickStarterUpdate());
    ui.ListExplorer->setText(LanguageSetup->QuickStarterMenuExplorer());
    ui.ListDOSBox->setText(LanguageSetup->QuickStarterMenuDOSBox());

    // Shortcuts are only displayed in the menu, the keys are handled by ListKeyDown
    ui.ListRun->setShortcut(QKeySequence(Qt::Key_Return));
    ui.ListRunWithParameters->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    ui.ListOpenInEditor->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    ui.ListRun->setShortcutContext(Qt::WidgetShortcut);
    ui.ListRunWithParameters->setShortcutContext(Qt::WidgetShortcut);
    ui.ListOpenInEditor->setShortcutContext(Qt::WidgetShortcut);

    ui.TemplateComboBox->update();
    ui.SetupComboBox1->update();
    ui.SetupComboBox2->update();

    LoadStartButtonLanguage(nullptr);

    {
        QFont F = ui.CoolBar->font();
        if (PrgSetup->ToolbarFontSize() > 0) F.setPointSize(PrgSetup->ToolbarFontSize());
        ui.CoolBar->setFont(F);
        ui.ToolBar->setFont(F);
    }

    BuildList();
}

void TQuickStartForm::LoadTemplateList()
{
    QString SaveTemplateName = ui.TemplateComboBox->currentText();

    ui.TemplateComboBox->setUpdatesEnabled(false);
    ui.TemplateComboBox->clear();

    ui.TemplateComboBox->addItem(LanguageSetup->TemplateFormDefault());
    TGameDB* TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
    for (int I = 0; I < TemplateDB->Count(); I++) ui.TemplateComboBox->addItem(TemplateDB->Game(I)->CacheName);
    delete TemplateDB;
    ui.TemplateComboBox->setUpdatesEnabled(true);

    ui.TemplateComboBox->setCurrentIndex(Max(0, ui.TemplateComboBox->findText(SaveTemplateName, Qt::MatchFixedString)));
}

void TQuickStartForm::BuildTree()
{
    QTreeWidgetItem *N, *SelNode, *BackupSelNode;
    QString S, SaveCurrentPath;

    GameDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    CaptureDir = MakeAbsPath(PrgSetup->CaptureDir(), PrgDataDir());
    DataDir = MakeAbsPath(PrgSetup->DataDir(), PrgSetup->BaseDir());

    BackupSelNode = nullptr;

    auto NewRootNode = [this](const QString &Text, int ImageIndex) {
        auto *Node = new QTreeWidgetItem(ui.Tree, QStringList() << Text);
        Node->setData(0, RoleImageIndex, ImageIndex);
        Node->setData(0, RoleSelectedIndex, ImageIndex);
        if (ImageList != nullptr && ImageIndex < ImageList->Count()) Node->setIcon(0, ImageList->Icon(ImageIndex));
        return Node;
    };
    auto SubPath = [](const QString &SavePath, const QString &Dir) {
        QString R;
        if (Copy(SavePath, 1, Length(Dir)) == Dir) {
            R = Copy(SavePath, Length(Dir) + 1, MaxInt);
            if ((!R.isEmpty()) && (!IsDelim(R[0]))) R = PathDelim + R;
        }
        return R;
    };

    DisableGUI();
    ui.Tree->setUpdatesEnabled(false);
    {
        SaveCurrentPath = CurrentPath;
        SelNode = nullptr;

        ui.Tree->clear();
        ui.Tree->setSortingEnabled(false);   // Tree.SortType:=stNone

        switch (ui.TreeTypeComboBox->currentIndex()) {
        case 0:
            TreeBaseDir = GameDir;
            N = NewRootNode(LanguageSetup->QuickStarterTypeGamesDir(), 6);
            BackupSelNode = N;
            S = SubPath(SaveCurrentPath, GameDir);
            SelNode = AddSubfolders(GameDir, N, S, false);
            break;
        case 1:
            TreeBaseDir = CaptureDir;
            N = NewRootNode(LanguageSetup->QuickStarterTypeCaptureDir(), 6);
            BackupSelNode = N;
            S = SubPath(SaveCurrentPath, CaptureDir);
            SelNode = AddSubfolders(CaptureDir, N, S, false);
            break;
        case 2:
            TreeBaseDir = DataDir;
            N = NewRootNode(LanguageSetup->QuickStarterTypeDataDir(), 6);
            BackupSelNode = N;
            S = SubPath(SaveCurrentPath, DataDir);
            SelNode = AddSubfolders(DataDir, N, S, false);
            break;
        case 3:
            TreeBaseDir.clear();
            BackupSelNode = nullptr;
#ifdef Q_OS_WIN
            for (QChar C = 'A'; C <= 'Z'; C = QChar(C.unicode() + 1)) {
                const QString Root = QString(C) + ":\\";
                if (GetDriveTypeW(reinterpret_cast<const wchar_t *>(Root.utf16())) == DRIVE_NO_ROOT_DIR) continue;
                wchar_t VolName[512]; DWORD I1 = 0, I2 = 0, I3 = 0;
                if (!GetVolumeInformationW(reinterpret_cast<const wchar_t *>(Root.utf16()), VolName, 500, &I1, &I2, &I3, nullptr, 0)) continue;
                if (!DirectoryExists(Root)) continue;

                N = NewRootNode(QString(C) + ":", 7);
                if (C == 'C') BackupSelNode = N;
                if (Copy(SaveCurrentPath, 1, 3) == Root) {
                    S = Copy(SaveCurrentPath, 4, MaxInt);
                    if ((!S.isEmpty()) && (!IsDelim(S[0]))) S = "\\" + S;
                } else S.clear();
                N = AddSubfolders(Root, N, S, false);
                if (N != nullptr) SelNode = N;
            }
#else
            // Port note: no drive letters outside Windows - the file system root is the only top level node
            N = NewRootNode("/", 7);
            BackupSelNode = N;
            if (Copy(SaveCurrentPath, 1, 1) == "/") S = Copy(SaveCurrentPath, 1, MaxInt); else S.clear();
            N = AddSubfolders("/", N, S, false);
            if (N != nullptr) SelNode = N;
#endif
            if (BackupSelNode == nullptr && ui.Tree->topLevelItemCount() > 0) BackupSelNode = ui.Tree->topLevelItem(0);
            break;
        }
    }
    ui.Tree->setUpdatesEnabled(true);
    EnableGUI();

    ui.Tree->setSortingEnabled(true);   // Tree.SortType:=stText
    ui.Tree->sortByColumn(0, Qt::AscendingOrder);
    if (SelNode != nullptr) ui.Tree->setCurrentItem(SelNode); else ui.Tree->setCurrentItem(BackupSelNode);
    if (ui.Tree->currentItem() != nullptr) ui.Tree->scrollToItem(ui.Tree->currentItem());

    BuildList();
}

QTreeWidgetItem* TQuickStartForm::AddSubfolders(const QString& AFolder, QTreeWidgetItem* ParentNode, const QString& AExpandPath, bool UpdateSort)
{
    TSearchRec Rec, Rec2;
    int I, J;
    QTreeWidgetItem* N;
    QString FirstPathPart;
    QString Folder = AFolder, ExpandPath = AExpandPath;

    QTreeWidgetItem* result = nullptr;
    if (UpdateSort) ui.Tree->setSortingEnabled(false);

    Folder = IncludeTrailingPathDelimiter(Folder);

    FirstPathPart.clear();
    if ((!ExpandPath.isEmpty()) && IsDelim(ExpandPath[0])) ExpandPath = Copy(ExpandPath, 2, MaxInt);
    if (!ExpandPath.isEmpty()) {
        I = 0;
        for (int K = 0; K < ExpandPath.size(); K++) if (IsDelim(ExpandPath[K])) { I = K + 1; break; }
        if (I == 0) {
            FirstPathPart = ExcludeTrailingPathDelimiter(ExpandPath); ExpandPath.clear();
        } else {
            FirstPathPart = Copy(ExpandPath, 1, I - 1); ExpandPath = Copy(ExpandPath, I, MaxInt);
        }
    }

    I = FindFirst(Folder + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != ".") && (Rec.Name != "..")) {
            N = new QTreeWidgetItem(QStringList() << Rec.Name);
            ParentNode->insertChild(0, N);   // AddChildFirst
            N->setData(0, RoleImageIndex, 4); N->setData(0, RoleSelectedIndex, 5);
            if (ImageList != nullptr && ImageList->Count() > 4) N->setIcon(0, ImageList->Icon(4));

            ProcessMessages();

            if (Rec.Name == FirstPathPart) {
                // Add subdirs
                result = AddSubfolders(Folder + Rec.Name + PathDelim, N, ExpandPath, false);
                if ((result == nullptr) && (ExpandPath.isEmpty() || (ExpandPath.size() == 1 && IsDelim(ExpandPath[0])))) result = N;
            } else {
                // Check if expandable
                J = FindFirst(Folder + Rec.Name + PathDelim + "*.*", faDirectory, Rec2);
                while (J == 0) {
                    if (((Rec2.Attr & faDirectory) != 0) && (Rec2.Name != ".") && (Rec2.Name != "..")) {
                        N->setData(0, RoleHasChildren, true);
                        N->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
                        break;
                    }
                    J = FindNext(Rec2);
                }
                FindClose(Rec2);
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    ParentNode->setExpanded(true);
    if (UpdateSort) { ui.Tree->setSortingEnabled(true); ui.Tree->sortByColumn(0, Qt::AscendingOrder); }
    return result;
}

void TQuickStartForm::BuildList()
{
    TSearchRec Rec;
    int I, J;
    TListItem *N, *NewSel;
    QString S, T, SaveSel;

    Timer->stop();
#ifdef Q_OS_WIN
    if (hChangeNotification != -1) FindCloseChangeNotification(reinterpret_cast<HANDLE>(hChangeNotification));
#endif
    hChangeNotification = -1;
    delete ChangeWatcher; ChangeWatcher = nullptr;
    ChangeDetected = false;

    DisableGUI();
    ui.List->Items()->BeginUpdate();
    {
        if (ui.List->Selected() == nullptr) SaveSel.clear(); else SaveSel = ui.List->Selected()->Caption();
        NewSel = nullptr;

        ui.List->SetSortType(stNone);
        ui.List->Items()->Clear();

        if (CurrentPath.isEmpty()) {
            ui.List->Items()->EndUpdate();
            EnableGUI();
            return;
        }

        I = FindFirst(CurrentPath + "*.*", faAnyFile, Rec);
        while (I == 0) {
            ProcessMessages();

            if ((Rec.Attr & faDirectory) == 0) {
                N = ui.List->Items()->Add();
                N->SetCaption(Rec.Name);
                if (Rec.Name == SaveSel) NewSel = N;
                S = ExtUpperCase(ExtractFileExt(Rec.Name));
                N->SubItems().Add(FileSizeToStr(Rec.Size));

                N->SetImageIndex(8);
                T.clear();
                N->SetData(reinterpret_cast<void *>(static_cast<intptr_t>(FileTypesHigh)));

                for (J = 0; J <= FileTypesHigh; J++) if (S == QLatin1String(FileTypes[J].Ext)) {
                    if ((FileTypes[J].Options & ftDOSExe) && IsWindowsExe(CurrentPath + Rec.Name)) continue;
                    if ((FileTypes[J].Options & ftWindowsExe) && (!IsWindowsExe(CurrentPath + Rec.Name))) continue;

                    if (FileTypes[J].NameID > 0) T = LanguageSetup->GetString(FileTypes[J].NameID);
                    N->SetImageIndex(FileTypes[J].ImageIndex);
                    N->SetData(reinterpret_cast<void *>(static_cast<intptr_t>(J)));
                    break;
                }
                N->SubItems().Add(T);
            }
            I = FindNext(Rec);
        }
        FindClose(Rec);

        ui.List->SetSortType(stText);

        ui.List->SetSelected(NewSel);
    }
    ui.List->Items()->EndUpdate();
    for (I = 0; I < ui.List->Columns()->Count(); I++) ui.List->reportView()->resizeColumnToContents(I);   // Width:=-1
    EnableGUI();

    const QString WatchDir = TreeBaseDir.isEmpty() ? CurrentPath : TreeBaseDir;
#ifdef Q_OS_WIN
    HANDLE H = FindFirstChangeNotificationW(
        reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(WatchDir).utf16()),
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY
    );
    hChangeNotification = (H == INVALID_HANDLE_VALUE) ? -1 : reinterpret_cast<THandle>(H);
#else
    // TODO(port): FindFirstChangeNotification watches the whole sub tree; QFileSystemWatcher
    // watches the base folder and the current folder only.
    ChangeWatcher = new QFileSystemWatcher(this);
    QStringList Dirs;
    if (DirectoryExists(WatchDir)) Dirs << ExcludeTrailingPathDelimiter(WatchDir);
    if (DirectoryExists(CurrentPath) && ExcludeTrailingPathDelimiter(CurrentPath) != ExcludeTrailingPathDelimiter(WatchDir)) Dirs << ExcludeTrailingPathDelimiter(CurrentPath);
    if (!Dirs.isEmpty()) ChangeWatcher->addPaths(Dirs);
    connect(ChangeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() { ChangeDetected = true; });
    hChangeNotification = 0;
#endif
    Timer->start();
}

void TQuickStartForm::TreeTypeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    switch (ui.TreeTypeComboBox->currentIndex()) {
    case 0: ui.TreeViewModeGames->setChecked(true); break;
    case 1: ui.TreeViewModeCapture->setChecked(true); break;
    case 2: ui.TreeViewModeData->setChecked(true); break;
    case 3: ui.TreeViewModeFull->setChecked(true); break;
    }

    BuildTree();
}

QString TQuickStartForm::PathFromNode(QTreeWidgetItem* Node)
{
    QString result;
    if (Node == nullptr) return result;
    result = PathDelim;
    QTreeWidgetItem* N = Node;
    while (N->parent() != nullptr) { result = PathDelim + N->text(0) + result; N = N->parent(); }
    if ((Length(N->text(0)) >= 2) && (N->text(0)[1] == ':')) {
        result = N->text(0) + result;
    } else if (N->text(0) == "/") {
        // file system root (non Windows "full" view)
    } else {
        switch (ui.TreeTypeComboBox->currentIndex()) {
        case 0: result = ExcludeTrailingPathDelimiter(GameDir) + result; break;
        case 1: result = ExcludeTrailingPathDelimiter(CaptureDir) + result; break;
        case 2: result = ExcludeTrailingPathDelimiter(DataDir) + result; break;
        }
    }
    return result;
}

void TQuickStartForm::TimerWork(QObject* Sender)
{
    Q_UNUSED(Sender);
#ifdef Q_OS_WIN
    if ((hChangeNotification != -1) && (WaitForSingleObject(reinterpret_cast<HANDLE>(hChangeNotification), 0) == WAIT_OBJECT_0)) {
        const THandle H = hChangeNotification;
        if (!TreeBaseDir.isEmpty()) BuildTree(); else BuildList();
        if (hChangeNotification == H) FindNextChangeNotification(reinterpret_cast<HANDLE>(hChangeNotification));
    }
#else
    if ((hChangeNotification != -1) && ChangeDetected) {
        ChangeDetected = false;
        if (!TreeBaseDir.isEmpty()) BuildTree(); else BuildList();
    }
#endif
}

void TQuickStartForm::TreeChange(QObject* Sender, QTreeWidgetItem* Node)
{
    Q_UNUSED(Sender);
    CurrentPath = PathFromNode(Node);
    setWindowTitle(LanguageSetup->QuickStarterCaption() + " [" + ExcludeTrailingPathDelimiter(CurrentPath) + "]");
    BuildList();

    ui.TreeRename->setVisible((Node != nullptr) && (Node->parent() != nullptr));
    ui.TreeDelete->setVisible((Node != nullptr) && (Node->parent() != nullptr));
}

void TQuickStartForm::TreeExpanding(QObject* Sender, QTreeWidgetItem* Node, bool& AllowExpansion)
{
    Q_UNUSED(Sender); Q_UNUSED(AllowExpansion);
    if (Node->data(0, RoleHasChildren).toBool() && (Node->childCount() == 0))
        AddSubfolders(PathFromNode(Node), Node, QString(), true);
}

void TQuickStartForm::TreePopupMenuPopup(QObject* Sender)
{
    Q_UNUSED(Sender);
    QTreeWidgetItem *Sel = ui.Tree->currentItem();
    CurrentPath = PathFromNode(Sel);
    ui.TreeRename->setVisible((Sel != nullptr) && (Sel->parent() != nullptr));
    ui.TreeDelete->setVisible((Sel != nullptr) && (Sel->parent() != nullptr));
    PopupSelNode = Sel;
}

void TQuickStartForm::ButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: close(); break;
    case 1: BuildTree(); break;
    case 2: MenuWork(ui.ListRun); break;
    case 3: ApplicationHelpCommand(HELP_CONTEXT, ID_ViewQuickStarter); break;
    }
}

void TQuickStartForm::MenuWork(QObject* Sender)
{
    QString S, T;

    if (PopupSelNode != nullptr) ui.Tree->setCurrentItem(PopupSelNode);
    PopupSelNode = nullptr;

    QTreeWidgetItem *Sel = ui.Tree->currentItem();
    TListItem *LSel = ui.List->Selected();

    switch (Sender->property("tag").toInt()) {
    case 10: BuildTree(); break;
    case 11: ui.TreeTypeComboBox->setCurrentIndex(0); TreeTypeComboBoxChange(Sender); break;
    case 12: ui.TreeTypeComboBox->setCurrentIndex(1); TreeTypeComboBoxChange(Sender); break;
    case 13: ui.TreeTypeComboBox->setCurrentIndex(2); TreeTypeComboBoxChange(Sender); break;
    case 14: ui.TreeTypeComboBox->setCurrentIndex(3); TreeTypeComboBoxChange(Sender); break;
    case 15: OpenFolder(CurrentPath); TreeChange(Sender, ui.Tree->currentItem()); break;   // ShellExecute(...,'explore',...)
    case 16: OpenFolderInDOSBox(); TreeChange(Sender, ui.Tree->currentItem()); break;
    case 17:
        if ((Sel != nullptr) && (Sel->parent() != nullptr)) {
            S = Sel->text(0);
            if (!InputQuery(LanguageSetup->QuickStarterMenuRenameCaptionFolder(), LanguageSetup->QuickStarterMenuRenameLabel(), S, this)) return;
            if (!RenameFile(PathFromNode(Sel->parent()) + Sel->text(0), PathFromNode(Sel->parent()) + S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {PathFromNode(Sel->parent()) + Sel->text(0), PathFromNode(Sel->parent()) + S}), mtError, mbOK, 0);
                return;
            }
            Sel->setText(0, S);
            BuildTree();
        }
        break;
    case 18:
        if ((Sel != nullptr) && (Sel->parent() != nullptr)) {
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteFolder(), {PathFromNode(Sel)}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            if (!ExtDeleteFolder(PathFromNode(Sel), ftQuickStart)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {PathFromNode(Sel)}), mtError, mbOK, 0);
                return;
            }
            ui.Tree->setCurrentItem(Sel->parent());
            BuildTree();
        }
        break;
    case 19:
        S = LanguageSetup->QuickStarterMenuCreateFolderDefaultName();
        if (!InputQuery(LanguageSetup->QuickStarterMenuCreateFolderCaption(), LanguageSetup->QuickStarterMenuCreateFolderLabel(), S, this)) return;
        T = IncludeTrailingPathDelimiter(CurrentPath) + S;
        if (DirectoryExists(T)) {
            MessageDlg(Format(LanguageSetup->MessageDirectoryAlreadyExists(), {T}), mtError, mbOK, 0);
            return;
        }
        if (!ForceDirectories(T)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {T}), mtError, mbOK, 0);
            return;
        }
        ButtonWork(ui.UpdateButton);
        break;
    case 21: if (LSel != nullptr) RunFile(CurrentPath + LSel->Caption(), ItemType(LSel), false); break;
    case 22: if (LSel != nullptr) AddProfile(CurrentPath + LSel->Caption(), FileTypes[ItemType(LSel)].Options & ftWindowsExe, false); break;
    case 23: if (LSel != nullptr) AddProfile(CurrentPath + LSel->Caption(), FileTypes[ItemType(LSel)].Options & ftWindowsExe, true); break;
    case 24: if ((LSel != nullptr) && (FileTypes[ItemType(LSel)].Options & ftAddEditItem)) OpenFileInEditor(CurrentPath + LSel->Caption()); break;
    case 25:
        if (LSel != nullptr) {
            S = LSel->Caption();
            if (!InputQuery(LanguageSetup->QuickStarterMenuRenameCaptionFile(), LanguageSetup->QuickStarterMenuRenameLabel(), S, this)) return;
            if (!RenameFile(CurrentPath + LSel->Caption(), CurrentPath + S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {CurrentPath + LSel->Caption(), CurrentPath + S}), mtError, mbOK, 0);
                return;
            }
            LSel->SetCaption(S);
            BuildList();
        }
        break;
    case 26:
        if (LSel != nullptr) {
            if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteFile(), {CurrentPath + LSel->Caption()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            if (!ExtDeleteFile(CurrentPath + LSel->Caption(), ftQuickStart)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {CurrentPath + LSel->Caption()}), mtError, mbOK, 0);
                return;
            }
            BuildList();
        }
        break;
    case 27: if (LSel != nullptr) RunFile(CurrentPath + LSel->Caption(), ItemType(LSel), true); break;
    case 28:
        if (LSel != nullptr) {
            S = CurrentPath + LSel->Caption();
            switch (FileTypes[ItemType(LSel)].OpenType) {
            case otImageViewer:
                T = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
                if ((!T.isEmpty()) && (T != "INTERNAL")) {
                    if (PrgSetup->NonModalViewer())
                        ShowNonModalImageDialog(this, S, nullptr, nullptr);
                    else
                        ShowImageDialog(this, S, nullptr, nullptr);
                    return;
                }
                break;
            case otSoundPlayer:
                T = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
                if ((!T.isEmpty()) && (T != "INTERNAL")) { PlaySoundDialog(this, S, nullptr, nullptr); return; }
                break;
            case otVideoPlayer:
                T = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
                if ((!T.isEmpty()) && (T != "INTERNAL")) { PlayVideoDialog(this, S, nullptr, nullptr); return; }
                break;
            default:
                break;
            }
            ShellOpen(S);   // ShellExecute(Handle,'open',...)
        }
        break;
    }
}

void TQuickStartForm::ListDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    MenuWork(ui.ListRun);
}

void TQuickStartForm::ListKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Key == VK_RETURN) {
        if (Shift == Qt::NoModifier) MenuWork(ui.ListRun);
        if (Shift == Qt::ShiftModifier) MenuWork(ui.ListRunWithParameters);
        if (Shift == Qt::ControlModifier) MenuWork(ui.ListOpenInEditor);
    }
}

void TQuickStartForm::FormKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::NoModifier) switch (Key) {
    case VK_F1: ButtonWork(ui.HelpButton); break;
    case VK_F5: ButtonWork(ui.UpdateButton); break;
    case VK_F2: if (ui.Tree->hasFocus()) MenuWork(ui.TreeRename); else MenuWork(ui.ListRename); break;
    case VK_DELETE: if (ui.Tree->hasFocus()) MenuWork(ui.TreeDelete); else MenuWork(ui.ListDelete); break;
    }
}

void TQuickStartForm::OpenFolderInDOSBox()
{
    OpenDOSBox(IncludeTrailingPathDelimiter(CurrentPath), QString(), QString());
}

void TQuickStartForm::GetFilesInFolder(const QList<QString>& Extensions, const QString& FileName, TStringList*& St1, TStringList*& St2)
{
    TSearchRec Rec;
    int I, J;
    QString S;

    QString Path = IncludeTrailingPathDelimiter(ExtractFilePath(FileName));

    St1 = new TStringList;
    St2 = new TStringList;

    TStringList St;
    // Search files
    I = FindFirst(Path + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            S = ExtUpperCase(ExtractFileExt(Rec.Name));
            for (J = 0; J < Extensions.size(); J++) if (Extensions[J] == S) { St.Add(Rec.Name); break; }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    // Build lists
    St.Sort();
    J = St.IndexOf(ExtractFileName(FileName));
    for (I = 0; I < St.Count(); I++) {
        if (I == J) continue;
        if (I < J) St1->Add(Path + St[I]);
        if (I > J) St2->Add(Path + St[I]);
    }
}

void TQuickStartForm::RunFile(const QString& FileName, int TypeNr, bool WithParameters)
{
    QString Param;
    TStringList *St1 = nullptr, *St2 = nullptr;

    switch (FileTypes[TypeNr].OpenType) {
    case otNone: /* do not try to run file */ break;
    case otOpen:
        Param.clear();
        if (WithParameters) {
            if (!InputQuery(LanguageSetup->QuickStarterMenuRunWithParametersCaption(), LanguageSetup->QuickStarterMenuRunWithParametersLabel(), Param, this)) return;
            Param = Trim(Param);
        }
        if (Param.isEmpty())
            ShellOpen(FileName, QString(), ExtractFilePath(FileName));
        else
            ShellOpen(FileName, Param, ExtractFilePath(FileName));
        break;
    case otDOSBox:
        Param.clear();
        if (WithParameters) {
            if (!InputQuery(LanguageSetup->QuickStarterMenuRunWithParametersCaption(), LanguageSetup->QuickStarterMenuRunWithParametersLabel(), Param, this)) return;
            Param = Trim(Param);
        }
        OpenDOSBox(IncludeTrailingPathDelimiter(ExtractFilePath(FileName)), FileName, Param);
        break;
    case otImageViewer:
        GetFilesInFolder({".BMP", ".JPG", ".JPEG", ".PNG", ".GIF"}, FileName, St1, St2);
        if (PrgSetup->NonModalViewer())
            ShowNonModalImageDialog(this, FileName, St1, St2);
        else
            ShowImageDialog(this, FileName, St1, St2);
        delete St1; delete St2;
        break;
    case otSoundPlayer:
        GetFilesInFolder({".WAV", ".MP3", ".OGG"}, FileName, St1, St2);
        PlaySoundDialog(this, FileName, St1, St2);
        delete St1; delete St2;
        break;
    case otVideoPlayer:
        GetFilesInFolder({".AVI", ".MPG", ".MPEG", ".WMV", ".ASF"}, FileName, St1, St2);
        PlayVideoDialog(this, FileName, St1, St2);
        delete St1; delete St2;
        break;
    case otEditor:
        OpenFileInEditor(FileName);
        break;
    }
}

void TQuickStartForm::OpenDOSBox(const QString& Path, const QString& FileName, const QString& Parameters)
{
    if (ui.TemplateComboBox->currentIndex() < 0) return;

    TTempGame* TempGame = new TTempGame();
    if (ui.TemplateComboBox->currentIndex() == 0) {
        // Default template
        TGame* DefaultTemplate = new TGame(PrgSetup);
        TempGame->Game()->AssignFrom(DefaultTemplate);
        delete DefaultTemplate;
    } else {
        // Template from DB
        TGameDB* TemplateDB = new TGameDB(PrgDataDir() + TemplateSubDir, false);
        // literal translation of TemplateDB[TemplateComboBox.ItemIndex+1]
        const int Nr = ui.TemplateComboBox->currentIndex() + 1;
        if (Nr >= 0 && Nr < TemplateDB->Count()) TempGame->Game()->AssignFrom(TemplateDB->Game(Nr));
        delete TemplateDB;
    }

    TempGame->Game()->SetStartFullscreen(ui.SetupComboBox1->currentIndex() == 0);
    TempGame->Game()->SetCloseDosBoxAfterGameExit(ui.SetupComboBox2->currentIndex() == 0);
    if (!Trim(Parameters).isEmpty()) TempGame->Game()->SetGameParameters(Trim(Parameters));

    CheckPathAvialableInProfile(TempGame->Game(), Path);

    if (FileName.isEmpty()) {
        TempGame->Game()->SetCloseDosBoxAfterGameExit(false);
        AddAutoexecCDCommands(TempGame->Game(), Path);
    }

    TempGame->Game()->SetGameExe(FileName);

    RunGame(TempGame->Game(), DFendReloadedMainForm != nullptr ? DFendReloadedMainForm->DeleteOnExit : nullptr);

    Sleep(100);
    delete TempGame;
}

void TQuickStartForm::CheckPathAvialableInProfile(TGame* Game, const QString& Path)
{
    int I, J;
    QString S, T;
    QString Letters = "CDEFGHIJKLMNOPQRSTUVWXYZ";

    T = MakePathShort(Trim(ExtractFilePath(MakeAbsPath(Path, PrgSetup->BaseDir()))));

    for (I = 0; I < Game->NrOfMounts(); I++) {
        TStringList* St = ValueToList(Game->Mount(I));
        if (St->Count() == 3) St->Add("false");
        if (St->Count() == 4) St->Add(QString());
        if (St->Count() < 5) { delete St; continue; }

        if ((Trim(ExtUpperCase((*St)[1])) != "DRIVE") && (Trim(ExtUpperCase((*St)[1])) != "CDROM") && (Trim(ExtUpperCase((*St)[1])) != "FLOPPY")) { delete St; continue; }
        S = MakePathShort(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[0]), PrgSetup->BaseDir()));
        if (Copy(T, 1, Length(S)) == S) {
            // everything ok
            delete St;
            return;
        }

        J = Pos((*St)[2], Letters);
        if (J > 0) Letters[J - 1] = '-';
        delete St;
    }

    // RealFolder;DRIVE;Letter;False;;FreeSpace

    for (J = 1; J <= Length(Letters); J++) if (Letters[J - 1] != '-') {
        S = Path + ";DRIVE;" + Letters[J - 1] + ";False;;";
        Game->SetMount(Game->NrOfMounts(), S);
        Game->SetNrOfMounts(Game->NrOfMounts() + 1);
        return;
    }
}

void TQuickStartForm::AddAutoexecCDCommands(TGame* Game, const QString& Path)
{
    QString S;
    QString T = MakePathShort(Trim(ExtractFilePath(MakeAbsPath(Path, PrgSetup->BaseDir()))));

    for (int I = 0; I < Game->NrOfMounts(); I++) {
        TStringList* St = ValueToList(Game->Mount(I));
        if (St->Count() == 3) St->Add("false");
        if (St->Count() == 4) St->Add(QString());
        if (St->Count() < 5) { delete St; continue; }

        if ((Trim(ExtUpperCase((*St)[1])) != "DRIVE") && (Trim(ExtUpperCase((*St)[1])) != "CDROM") && (Trim(ExtUpperCase((*St)[1])) != "FLOPPY")) { delete St; continue; }
        S = MakePathShort(MakeAbsPath(IncludeTrailingPathDelimiter((*St)[0]), PrgSetup->BaseDir()));
        if (Copy(T, 1, Length(S)) == S) {
            T = ExcludeTrailingPathDelimiter(Copy(T, Length(S) + 1, MaxInt));
            TStringList* St2 = StringToStringList(Game->Autoexec());
            St2->Add((*St)[2] + ":");
            T.replace('/', '\\');   // DOS path inside DOSBox
            if (T.isEmpty() || (T[0] != '\\')) T = "\\" + T;
            St2->Add("cd " + T);
            Game->SetAutoexec(StringListToString(St2));
            delete St2;
            delete St;
            return;
        }
        delete St;
    }
}

void TQuickStartForm::AddProfile(const QString& FileName, bool WindowsProfile, bool UseWizard)
{
    if (FOnAddProfile) FOnAddProfile(this, FileName, WindowsProfile, UseWizard, ui.TemplateComboBox->currentIndex() - 1);
}

void TQuickStartForm::ListDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    Q_UNUSED(Sender); Q_UNUSED(X); Q_UNUSED(Y);
    int I;
    QString FileName, S;

    if (Source == nullptr) return;
    TStringList* FileList = Source;

    for (I = 0; I < FileList->Count(); I++) {
        FileName = (*FileList)[I];
        if (DirectoryExists(FileName)) {
            S = CurrentPath + ExtractFileName(ExcludeTrailingPathDelimiter(FileName));
            if (!ForceDirectories(S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
            if (!CopyFiles(FileName, S, true, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFiles(), {FileName, S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
        } else {
            S = CurrentPath + ExtractFileName(FileName);
            if (!CopyFile(FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
        }
    }
    BuildTree();
}

void TQuickStartForm::TreeDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    int I;
    QString FileName, S;

    if (Source == nullptr) return;
    TStringList* FileList = Source;

    QTreeWidgetItem* N = ui.Tree->itemAt(ui.Tree->viewport()->mapFrom(ui.Tree, QPoint(X, Y)));
    if (N != nullptr) {
        ui.Tree->setCurrentItem(N);
        TreeChange(Sender, N);
    }

    for (I = 0; I < FileList->Count(); I++) {
        FileName = (*FileList)[I];
        if (DirectoryExists(FileName)) {
            S = CurrentPath + ExtractFileName(ExcludeTrailingPathDelimiter(FileName));
            if (!ForceDirectories(S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
            if (!CopyFiles(FileName, S, true, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFiles(), {FileName, S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
        } else {
            S = CurrentPath + ExtractFileName(FileName);
            if (!CopyFile(FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
                BuildTree();
                return;
            }
        }
    }
    BuildTree();
}

// Files dropped from the file manager (TTreeViewAcceptingFiles / TListViewAcceptingFiles)

void TQuickStartForm::dragEnterEvent(QDragEnterEvent *E)
{
    QWidget *W = childAt(E->position().toPoint());
    if (E->mimeData()->hasUrls() && W != nullptr && (ui.Tree->isAncestorOf(W) || W == ui.Tree || ui.List->isAncestorOf(W) || W == ui.List)) E->acceptProposedAction();
    else E->ignore();
}

void TQuickStartForm::dragMoveEvent(QDragMoveEvent *E)
{
    QWidget *W = childAt(E->position().toPoint());
    if (E->mimeData()->hasUrls() && W != nullptr && (ui.Tree->isAncestorOf(W) || W == ui.Tree || ui.List->isAncestorOf(W) || W == ui.List)) E->acceptProposedAction();
    else E->ignore();
}

void TQuickStartForm::dropEvent(QDropEvent *E)
{
    QWidget *W = childAt(E->position().toPoint());
    if (!E->mimeData()->hasUrls() || W == nullptr) { E->ignore(); return; }
    TStringList FileList;
    for (const QUrl &U : E->mimeData()->urls()) if (U.isLocalFile()) FileList.Add(QDir::toNativeSeparators(U.toLocalFile()));
    if (FileList.Count() == 0) { E->ignore(); return; }
    E->acceptProposedAction();
    if (W == ui.Tree || ui.Tree->isAncestorOf(W)) {
        const QPoint P = ui.Tree->mapFrom(this, E->position().toPoint());
        TreeDragDrop(ui.Tree, &FileList, P.x(), P.y());
    } else if (W == ui.List || ui.List->isAncestorOf(W)) {
        const QPoint P = ui.List->mapFrom(this, E->position().toPoint());
        ListDragDrop(ui.List, &FileList, P.x(), P.y());
    }
}

void TQuickStartForm::DisableGUI()
{
    if (DisableCounter == 0) {
        ui.ToolBar->setEnabled(false);
        ui.TreeTypeComboBox->setEnabled(false);
        ui.Tree->setEnabled(false);
        ui.List->setEnabled(false);
        ui.TemplateComboBox->setEnabled(false);
        ui.SetupComboBox1->setEnabled(false);
        ui.SetupComboBox2->setEnabled(false);
    }
    DisableCounter++;
    Working = true;
}

void TQuickStartForm::EnableGUI()
{
    DisableCounter--;
    if (DisableCounter == 0) {
        ui.ToolBar->setEnabled(true);
        ui.TreeTypeComboBox->setEnabled(true);
        ui.Tree->setEnabled(true);
        ui.List->setEnabled(true);
        ui.TemplateComboBox->setEnabled(true);
        ui.SetupComboBox1->setEnabled(true);
        ui.SetupComboBox2->setEnabled(true);
        Working = false;
    }
}
