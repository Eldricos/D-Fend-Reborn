// C++ port of ViewFilesFrameUnit.pas (implementation section).
#include "ViewFilesFrameUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "ViewImageFormUnit.h"
#include "PlaySoundFormUnit.h"
#include "PlayVideoFormUnit.h"
#include "GameDBToolsUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"
#include <QFileSystemWatcher>
#include <QMimeData>
#include <QDropEvent>
#include <QTreeView>
#include <QListView>

// TViewFilesFrame

TViewFilesFrame::TViewFilesFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("ViewFilesFrameUnit", "ImageList");
    ui.List->SetSmallImages(ImageList);

    PopupSelNode = nullptr;
    hChangeNotification = 0;   // INVALID_HANDLE_VALUE

    Timer = new QTimer(this);
    Timer->stop();
    Timer->setInterval(500);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerWork(Timer); });

    // FindFirstChangeNotification replacement
    FWatcher = new QFileSystemWatcher(this);
    connect(FWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() { FChangeSignaled = true; });
    connect(FWatcher, &QFileSystemWatcher::fileChanged, this, [this]() { FChangeSignaled = true; });

    // DFM event wiring
    connect(ui.UpdateButton, &QAction::triggered, this, [this]() { ButtonWork(ui.UpdateButton); });
    connect(ui.OpenButton, &QAction::triggered, this, [this]() { ButtonWork(ui.OpenButton); });
    connect(ui.OpenFileButton, &QAction::triggered, this, [this]() { ButtonWork(ui.OpenFileButton); });
    connect(ui.Tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* Cur) { if (!FTreeUpdating) TreeChange(ui.Tree, Cur); });
    connect(ui.List, &TListView::change, this, [this](TListItem* Item) { ListChange(ui.List, Item, 0); });
    connect(ui.List, &TListView::dblClick, this, [this]() { ListDblClick(ui.List); });
    ui.List->OnInfoTip = [this](TListItem* Item) { QString S; ListInfoTip(ui.List, Item, S); return S; };
    connect(ui.SetupDataDirButton, &QPushButton::clicked, this, [this]() { SetupDataDirButtonClick(ui.SetupDataDirButton); });
    connect(ui.TreePopupMenu, &QMenu::aboutToShow, this, [this]() { TreePopupMenuPopup(ui.TreePopupMenu); });
    QAction* MenuItems[] = {ui.TreeUpdate, ui.TreeCreateFolder, ui.TreeRename, ui.TreeDelete, ui.TreeExplorer, ui.ListRun,
                            ui.ListOpenInDefaultViewer, ui.ListUpdate, ui.ListCreateFolder, ui.ListRename, ui.ListDelete,
                            ui.ListExplorer, ui.ListUseInScreenshotList};
    for (QAction* A : MenuItems) connect(A, &QAction::triggered, this, [this, A]() { MenuWork(A); });
    // Tree.OnDragDrop / List.OnDragDrop (files dropped from the file manager, see ActivateAcceptFiles)
    ui.Tree->setAcceptDrops(true);
    ui.Tree->viewport()->setAcceptDrops(true);
    ui.Tree->viewport()->installEventFilter(this);
    ui.List->setAcceptDrops(true);
    ui.List->installEventFilter(this);
    for (QAbstractItemView* V : {static_cast<QAbstractItemView*>(ui.List->reportView()), static_cast<QAbstractItemView*>(ui.List->iconView())}) {
        if (V == nullptr) continue;
        V->setAcceptDrops(true);
        V->viewport()->setAcceptDrops(true);
        V->viewport()->installEventFilter(this);
    }
    // Popup menu short cuts (VCL evaluates the short cuts of the popup menus of the focused control)
    for (QAction* A : ui.ListPopupMenu->actions()) { if (!A->isSeparator()) { A->setShortcutContext(Qt::WidgetWithChildrenShortcut); ui.List->addAction(A); } }
}

TViewFilesFrame::~TViewFilesFrame()
{
    if (hChangeNotification != 0) { FWatcher->removePaths(FWatcher->directories()); hChangeNotification = 0; }
    delete ImageList;
}

bool TViewFilesFrame::eventFilter(QObject* O, QEvent* E)
{
    const bool IsTree = (O == ui.Tree->viewport());
    bool IsList = (O == ui.List);
    if (ui.List->reportView() && O == ui.List->reportView()->viewport()) IsList = true;
    if (ui.List->iconView() && O == ui.List->iconView()->viewport()) IsList = true;
    if (IsTree || IsList) {
        if (E->type() == QEvent::DragEnter || E->type() == QEvent::DragMove) {
            auto* DE = static_cast<QDropEvent*>(E);
            if (DE->mimeData()->hasUrls()) { DE->acceptProposedAction(); return true; }
        }
        if (E->type() == QEvent::Drop) {
            auto* DE = static_cast<QDropEvent*>(E);
            if (DE->mimeData()->hasUrls()) {
                TStringList FileList;
                for (const QUrl& U : DE->mimeData()->urls()) if (U.isLocalFile()) FileList.Add(ExcludeTrailingPathDelimiter(QDir::toNativeSeparators(U.toLocalFile())));
                DE->acceptProposedAction();
                const QPoint P = DE->position().toPoint();
                if (IsTree) TreeDragDrop(ui.Tree, &FileList, P.x(), P.y()); else ListDragDrop(ui.List, &FileList, P.x(), P.y());
                return true;
            }
        }
    }
    return QWidget::eventFilter(O, E);
}

void TViewFilesFrame::Init()
{
    // SetVistaFonts / Font.Charset / NoFlicker: not needed in Qt

    ui.InfoLabel->move(ui.InfoLabel->x(), 16);
    ui.InfoLabel->setWordWrap(true);
    VclLayout::setBounds(ui.SetupDataDirButton, ui.SetupDataDirButton->x(), 16 + ui.InfoLabel->height() + 8, ui.SetupDataDirButton->width(), ui.SetupDataDirButton->height());

    ui.ListRun->setShortcut(QKeySequence(Qt::Key_Return));

    // Tree / List: TTreeViewAcceptingFiles / TListViewAcceptingFiles -> drop support is set up in the constructor
}

void TViewFilesFrame::LoadLanguage()
{
    ui.SetupDataDirButton->setText(LanguageSetup->ViewDataFilesSetup());
    ui.InfoLabel->setText(LanguageSetup->ViewDataFilesSetupInfo());
    ui.UpdateButton->setText(LanguageSetup->ViewDataFilesUpdate());
    ui.UpdateButton->setToolTip(LanguageSetup->ViewDataFilesUpdateHint());
    ui.OpenButton->setText(LanguageSetup->ViewDataFilesOpen());
    ui.OpenButton->setToolTip(LanguageSetup->ViewDataFilesOpenHint());
    ui.OpenFileButton->setText(LanguageSetup->ViewDataFilesOpenFile());
    ui.OpenFileButton->setToolTip(LanguageSetup->ViewDataFilesOpenFileHint());
    ui.TreeUpdate->setText(LanguageSetup->ViewDataFilesUpdate());
    ui.TreeCreateFolder->setText(LanguageSetup->ViewDataFilesCreateFolder());
    ui.TreeRename->setText(LanguageSetup->ViewDataFilesRename());
    ui.TreeDelete->setText(LanguageSetup->ViewDataFilesDelete());
    ui.TreeExplorer->setText(LanguageSetup->ViewDataFilesOpen());
    ui.ListRun->setText(LanguageSetup->ViewDataFilesOpenFile());
    ui.ListOpenInDefaultViewer->setText(LanguageSetup->ViewDataFilesOpenInDefaultViewer());
    ui.ListUpdate->setText(LanguageSetup->ViewDataFilesUpdate());
    ui.ListCreateFolder->setText(LanguageSetup->ViewDataFilesCreateFolder());
    ui.ListRename->setText(LanguageSetup->ViewDataFilesRename());
    ui.ListDelete->setText(LanguageSetup->ViewDataFilesDelete());
    ui.ListExplorer->setText(LanguageSetup->ViewDataFilesOpen());
    ui.ListUseInScreenshotList->setText(LanguageSetup->ScreenshotPopupUseInScreenshotList());

    UserIconLoader->DialogImage(DI_Folder, ImageList, 0);
    UserIconLoader->DialogImage(DI_SelectFolder, ImageList, 1);
    UserIconLoader->DialogImage(DI_Update, ImageList, 2);
    UserIconLoader->DialogImage(DI_ExploreFolder, ImageList, 3);
    UserIconLoader->DialogImage(DI_TextFile, ImageList, 4);
    UserIconLoader->DialogImage(DI_Image, ImageList, 5);
    UserIconLoader->DialogImage(DI_Sound, ImageList, 6);
    UserIconLoader->DialogImage(DI_Video, ImageList, 7);
    UserIconLoader->DialogImage(DI_InternetPage, ImageList, 8);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 9);
    UserIconLoader->DialogImage(DI_Delete, ImageList, 10);
    // Apply the image list to the actions (ImageIndex values from the DFM)
    ui.UpdateButton->setIcon(ImageList->Icon(2));
    ui.OpenButton->setIcon(ImageList->Icon(3));
    ui.OpenFileButton->setIcon(ImageList->Icon(4));
    ui.TreeUpdate->setIcon(ImageList->Icon(2));
    ui.TreeRename->setIcon(ImageList->Icon(9));
    ui.TreeDelete->setIcon(ImageList->Icon(10));
    ui.TreeExplorer->setIcon(ImageList->Icon(3));
    ui.ListRun->setIcon(ImageList->Icon(4));
    ui.ListUpdate->setIcon(ImageList->Icon(2));
    ui.ListRename->setIcon(ImageList->Icon(9));
    ui.ListDelete->setIcon(ImageList->Icon(10));
    ui.ListExplorer->setIcon(ImageList->Icon(3));
    ui.List->refresh();

    QFont F = ui.ToolBar->font();
    if (PrgSetup->ToolbarFontSize() > 0) F.setPointSize(PrgSetup->ToolbarFontSize());
    ui.ToolBar->setFont(F);
}

QString TViewFilesFrame::GetNewDataFolderName()
{
    QString S, T;
    int I;
    QString result = "";
    if (Game == nullptr) return result;

    if (PrgSetup->DataDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->DataDir();
    T = IncludeTrailingPathDelimiter(S) + MakeFileSysOKFolderName(Game->Name()) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(T)) {
        I++; T = IncludeTrailingPathDelimiter(S) + MakeFileSysOKFolderName(Game->Name()) + IntToStr(I) + PathDelim;
    }
    result = MakeRelPath(T, PrgSetup->BaseDir());
    return result;
}

// Adds Dir and all sub directories to the file system watcher
static void WatchFolderTree(QFileSystemWatcher* W, const QString& Dir)
{
    W->addPath(Dir);
    QDir D(Dir);
    const QStringList L = D.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString& S : L) WatchFolderTree(W, IncludeTrailingPathDelimiter(Dir) + S);
}

static QIcon TreeNodeIcon(TImageList* L)
{
    // N.ImageIndex:=0; N.SelectedIndex:=1;
    QIcon I;
    I.addPixmap(L->Pixmap(0), QIcon::Normal);
    I.addPixmap(L->Pixmap(1), QIcon::Selected);
    return I;
}

void TViewFilesFrame::SetGame(TGame* AGame)
{
    bool B;
    QTreeWidgetItem *N, *SelSaveNode;
    QString SelSaveName;

    Timer->stop();
    if (hChangeNotification != 0) { const QStringList D = FWatcher->directories(); if (!D.isEmpty()) FWatcher->removePaths(D); }
    hChangeNotification = 0;
    FChangeSignaled = false;

    Game = AGame;
    if (Game == nullptr) return;

    B = (Trim(Game->DataDir()) != "");

    if (B) {
        if (!DirectoryExists(MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir()))) {
            if (!ForceDirectories(MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir()))) {
                Game->SetDataDir("");
                Game->StoreAllValues();
                B = false;
            }
        }
    }

    // To fix problems with buttons not hidden the right way when setting game while Frame is invisible
    ui.ToolBar->setVisible(true);
    ui.Tree->setVisible(true);
    ui.Splitter->setVisible(true);
    ui.List->setVisible(true);
    ui.InfoLabel->setVisible(true);
    ui.SetupDataDirButton->setVisible(true);
    ui.ToolBar->setVisible(false);
    ui.Tree->setVisible(false);
    ui.Splitter->setVisible(false);
    ui.List->setVisible(false);
    ui.InfoLabel->setVisible(false);
    ui.SetupDataDirButton->setVisible(false);

    if (!B) ui.InfoLabel->setText(Format(LanguageSetup->ViewDataFilesSetupInfo(), {ExcludeTrailingPathDelimiter(MakeAbsPath(GetNewDataFolderName(), PrgSetup->BaseDir()))}));
    ui.ToolBar->setVisible(B);
    ui.Tree->setVisible(B);
    ui.Splitter->setVisible(B);
    ui.List->setVisible(B);
    ui.InfoLabel->setVisible(!B);
    ui.SetupDataDirButton->setVisible(!B);

    if (ui.Splitter->isVisibleTo(this)) ui.Splitter->move(ui.Tree->width() + ui.Tree->x(), ui.Splitter->y());

    if (B) {
        WatchFolderTree(FWatcher, MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir()));
        hChangeNotification = 1;
        Timer->start();
    }

    if (ui.Tree->currentItem() != nullptr) SelSaveName = ui.Tree->currentItem()->text(0); else SelSaveName = "";

    FTreeUpdating = true;
    ui.Tree->setUpdatesEnabled(false);
    ui.Tree->clear();
    if (!B) { ui.Tree->setUpdatesEnabled(true); FTreeUpdating = false; return; }
    N = new QTreeWidgetItem(ui.Tree, QStringList{LanguageSetup->ViewDataFilesRoot()});
    N->setIcon(0, TreeNodeIcon(ImageList));
    SelSaveNode = ReadTree(N, IncludeTrailingPathDelimiter(MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir())), SelSaveName);
    ui.Tree->expandAll();

    if ((SelSaveNode != nullptr) && (SelSaveNode != N)) {
        ui.Tree->setCurrentItem(SelSaveNode);
    } else {
        ui.Tree->setCurrentItem(N);
        QTreeWidgetItemIterator It(ui.Tree);
        while (*It) {
            if (FileInDir(GetCurrentPath(*It))) { ui.Tree->setCurrentItem(*It); break; }
            ++It;
        }
    }
    FTreeUpdating = false;
    TreeChange(this, ui.Tree->currentItem());
    ui.Tree->setUpdatesEnabled(true);
}

QTreeWidgetItem* TViewFilesFrame::ReadTree(QTreeWidgetItem* ParentNode, const QString& Dir, const QString& OldSelNode)
{
    TSearchRec Rec;
    int I;
    QTreeWidgetItem* N;
    QTreeWidgetItem* result = nullptr;

    I = FindFirst(Dir + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) == faDirectory) && (Rec.Name != ".") && (Rec.Name != "..")) {
            N = new QTreeWidgetItem(ParentNode, QStringList{Rec.Name});
            N->setIcon(0, TreeNodeIcon(ImageList));
            if (Rec.Name == OldSelNode) result = N;
            N = ReadTree(N, IncludeTrailingPathDelimiter(Dir + Rec.Name), OldSelNode);
            if (result == nullptr) result = N;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TViewFilesFrame::SetupDataDirButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    Game->SetDataDir(MakeRelPath(GetNewDataFolderName(), PrgSetup->BaseDir()));
    ForceDirectories(MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir()));
    SetGame(Game);
}

bool TViewFilesFrame::FileInDir(const QString& Dir)
{
    TSearchRec Rec;
    int I;
    bool result = false;
    I = FindFirst(Dir + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) { result = true; break; }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

QString TViewFilesFrame::GetCurrentPath(QTreeWidgetItem* TreeNode)
{
    QTreeWidgetItem* N;
    QString result = "";
    if (TreeNode == nullptr) {
        if (ui.Tree->currentItem() == nullptr) return result;
        N = ui.Tree->currentItem();
    } else {
        N = TreeNode;
    }
    while (N->parent() != nullptr) {
        if (result != "") result = PathDelim + result;
        result = N->text(0) + result;
        N = N->parent();
    }
    result = IncludeTrailingPathDelimiter(IncludeTrailingPathDelimiter(MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir())) + result);
    return result;
}

void TViewFilesFrame::TreeChange(QObject* Sender, QTreeWidgetItem* Node)
{
    QString Path;
    TListItem *I, *SelSaveItem;
    int J;
    TSearchRec Rec;
    QString S, SelSaveText;

    ui.TreeDelete->setVisible(Node != nullptr);
    ui.TreeRename->setVisible((Node != nullptr) && (Node->parent() != nullptr));
    ui.TreeExplorer->setVisible(Node != nullptr);
    ui.ListExplorer->setVisible(Node != nullptr);

    if (ui.List->Selected() != nullptr) SelSaveText = ui.List->Selected()->Caption(); else SelSaveText = "";
    SelSaveItem = nullptr;

    ui.List->Items()->BeginUpdate();
    ui.List->Items()->Clear();
    if (ui.Tree->currentItem() != nullptr) {
        Path = GetCurrentPath();

        J = FindFirst(Path + "*.*", faAnyFile, Rec);
        while (J == 0) {
            if ((Rec.Attr & faDirectory) == 0) {
                I = ui.List->Items()->Add();
                I->SetCaption(Rec.Name);
                I->SetImageIndex(4);
                S = ExtUpperCase(ExtractFileExt(Rec.Name));
                if ((S == ".JPG") || (S == ".JPEG") || (S == ".BMP") || (S == ".PNG") || (S == ".GIF") || (S == ".TIF") || (S == ".TIFF")) I->SetImageIndex(5);
                if ((S == ".WAV") || (S == ".MP3") || (S == ".OGG")) I->SetImageIndex(6);
                if ((S == ".AVI") || (S == ".MPG") || (S == ".MPEG") || (S == ".WMV") || (S == ".ASF")) I->SetImageIndex(7);
                if ((S == ".HTML") || (S == ".HTM")) I->SetImageIndex(8);

                if (Rec.Name == SelSaveText) SelSaveItem = I;
            }
            J = FindNext(Rec);
        }
        FindClose(Rec);
    }
    ui.List->Items()->EndUpdate();
    if (ui.Tree->currentItem() == nullptr) return;

    if (SelSaveItem != nullptr) {
        ui.List->SetSelected(SelSaveItem);
    } else {
        if (ui.List->Items()->Count() > 0) ui.List->SetSelected(ui.List->Items()->Item(0));
    }

    ListChange(Sender, ui.List->Selected(), 0 /* ctState */);
}

void TViewFilesFrame::TreePopupMenuPopup(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.TreeRename->setVisible((ui.Tree->currentItem() != nullptr) && (ui.Tree->currentItem()->parent() != nullptr));
    ui.TreeDelete->setVisible(ui.Tree->currentItem() != nullptr);
    PopupSelNode = ui.Tree->currentItem();
}

void TViewFilesFrame::ListInfoTip(QObject* Sender, TListItem* Item, QString& InfoTip)
{
    Q_UNUSED(Sender);
    if (Item == nullptr || Game == nullptr) { InfoTip = ""; return; }
    InfoTip = GetCurrentPath() + Item->Caption();
}

void TViewFilesFrame::ListChange(QObject* Sender, TListItem* Item, int Change)
{
    Q_UNUSED(Sender); Q_UNUSED(Item); Q_UNUSED(Change);
    QString S, T;
    TListItem* Sel = ui.List->Selected();

    ui.ListRun->setVisible(Sel != nullptr);
    ui.ListDelete->setVisible(Sel != nullptr);
    ui.ListRename->setVisible(Sel != nullptr);
    ui.TreeExplorer->setVisible(ui.Tree->currentItem() != nullptr);

    ui.OpenFileButton->setEnabled(Sel != nullptr);
    if (ui.OpenFileButton->isEnabled()) {
        ui.OpenFileButton->setIcon(ImageList->Icon(Sel->ImageIndex()));
        ui.ListRun->setIcon(ImageList->Icon(Sel->ImageIndex()));
    } else {
        ui.OpenFileButton->setIcon(ImageList->Icon(4));
        ui.ListRun->setIcon(ImageList->Icon(4));
    }

    ui.ListOpenInDefaultViewer->setVisible(false);
    if (ui.OpenFileButton->isEnabled()) {
        S = ExtUpperCase(ExtractFileExt(Sel->Caption())); T = "no";
        if ((S == ".JPG") || (S == ".JPEG") || (S == ".BMP") || (S == ".PNG") || (S == ".GIF") || (S == ".TIF") || (S == ".TIFF")) T = PrgSetup->ImageViewer();
        if ((S == ".WAV") || (S == ".MP3") || (S == ".OGG")) T = PrgSetup->SoundPlayer();
        if ((S == ".AVI") || (S == ".MPG") || (S == ".MPEG") || (S == ".WMV") || (S == ".ASF")) T = PrgSetup->VideoPlayer();
        T = Trim(ExtUpperCase(T));
        ui.ListOpenInDefaultViewer->setVisible((T == "") || (T == "INTERNAL"));
    }

    if (ui.OpenFileButton->isEnabled()) {
        S = ExtUpperCase(ExtractFileExt(Sel->Caption()));
        ui.ListUseInScreenshotList->setVisible((S == ".JPG") || (S == ".JPEG") || (S == ".BMP") || (S == ".PNG") || (S == ".GIF"));
        if (ui.ListUseInScreenshotList->isVisible()) {
            S = GetCurrentPath() + Sel->Caption();
            ui.ListUseInScreenshotList->setCheckable(true);
            ui.ListUseInScreenshotList->setChecked(Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(MakeAbsPath(Game->ScreenshotListScreenshot(), PrgSetup->BaseDir()))));
        }
    } else {
        ui.ListUseInScreenshotList->setVisible(false);
    }
}

void TViewFilesFrame::ListDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.OpenFileButton);
    if (ui.List->Selected() == nullptr) return;
}

void TViewFilesFrame::RunFile(bool ForceExternalViewer)
{
    QString S, T;
    TListItem* Sel = ui.List->Selected();
    if (Sel == nullptr) return;
    QWidget* Owner = window();

    S = GetCurrentPath();
    T = ExtUpperCase(ExtractFileExt(Sel->Caption()));
    if ((T == ".JPG") || (T == ".JPEG") || (T == ".BMP") || (T == ".PNG") || (T == ".GIF") || (T == ".TIF") || (T == ".TIFF")) {
        T = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
        if ((!ForceExternalViewer) || ((T != "") && (T != "INTERNAL"))) {
            if (PrgSetup->NonModalViewer())
                ShowNonModalImageDialog(Owner, S + Sel->Caption(), nullptr, nullptr);
            else
                ShowImageDialog(Owner, S + Sel->Caption(), nullptr, nullptr);
            return;
        }
    }
    if ((T == ".WAV") || (T == ".MP3") || (T == ".OGG")) {
        T = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
        if ((!ForceExternalViewer) || ((T != "") && (T != "INTERNAL"))) { PlaySoundDialog(Owner, S + Sel->Caption(), nullptr, nullptr); return; }
    }
    if ((T == ".AVI") || (T == ".MPG") || (T == ".MPEG") || (T == ".WMV") || (T == ".ASF")) {
        T = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
        if ((!ForceExternalViewer) || ((T != "") && (T != "INTERNAL"))) { PlayVideoDialog(Owner, S + Sel->Caption(), nullptr, nullptr); return; }
    }
    if ((T == ".TXT") || (T == ".NFO") || (T == ".DIZ") || (T == ".1ST") || (T == ".INI")) {
        OpenFileInEditor(S + Sel->Caption());
        return;
    }

    ShellOpen(S + Sel->Caption(), QString(), S);
}

void TViewFilesFrame::ButtonWork(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 0: SetGame(Game); break;
        case 1:
            S = MakeAbsPath(Game->DataDir(), PrgSetup->BaseDir());
            OpenFolder(S);
            break;
        case 2: RunFile(false); break;
    }
}

void TViewFilesFrame::MenuWork(QObject* Sender)
{
    QString S, T;
    bool IsRoot;

    if (PopupSelNode != nullptr) ui.Tree->setCurrentItem(PopupSelNode);
    PopupSelNode = nullptr;

    QTreeWidgetItem* Sel = ui.Tree->currentItem();
    switch (Sender->property("tag").toInt()) {
        case 11: ButtonWork(ui.UpdateButton); break;
        case 12: ButtonWork(ui.OpenButton); break;
        case 21:
            if ((Sel != nullptr) && (Sel->parent() != nullptr)) {
                S = Sel->text(0);
                if (!InputQuery(LanguageSetup->ViewDataFilesRenameCaptionFolder(), LanguageSetup->ViewDataFilesRenameLabel(), S, this)) return;
                if (!RenameFile(GetCurrentPath(Sel->parent()) + Sel->text(0), GetCurrentPath(Sel->parent()) + S)) {
                    const QString Old = (ui.List->Selected() != nullptr) ? ui.List->Selected()->Caption() : Sel->text(0);
                    MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {GetCurrentPath(Sel->parent()) + Old, GetCurrentPath(Sel->parent()) + S}), mtError, mbOK, 0);
                    return;
                }
                Sel->setText(0, S);
                ButtonWork(ui.UpdateButton);
            }
            break;
        case 22:
            if (Sel != nullptr) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteFolder(), {GetCurrentPath()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                IsRoot = (Sel->parent() == nullptr);
                if (!ExtDeleteFolder(GetCurrentPath(), ftDataViewer)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteDir(), {GetCurrentPath()}), mtError, mbOK, 0);
                    return;
                }
                if (IsRoot) {
                    Game->SetDataDir("");
                    Game->StoreAllValues();
                } else {
                    ui.Tree->setCurrentItem(Sel->parent());
                }
                ButtonWork(ui.UpdateButton);
            }
            break;
        case 23:
            S = LanguageSetup->QuickStarterMenuCreateFolderDefaultName();
            if (!InputQuery(LanguageSetup->QuickStarterMenuCreateFolderCaption(), LanguageSetup->QuickStarterMenuCreateFolderLabel(), S, this)) return;
            T = IncludeTrailingPathDelimiter(GetCurrentPath()) + S;
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
        case 31: ButtonWork(ui.OpenFileButton); break;
        case 32:
            if (ui.List->Selected() != nullptr) {
                S = ui.List->Selected()->Caption();
                if (!InputQuery(LanguageSetup->ViewDataFilesRenameCaptionFile(), LanguageSetup->ViewDataFilesRenameLabel(), S, this)) return;
                if (!RenameFile(GetCurrentPath() + ui.List->Selected()->Caption(), GetCurrentPath() + S)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotRenameFile(), {GetCurrentPath() + ui.List->Selected()->Caption(), GetCurrentPath() + S}), mtError, mbOK, 0);
                    return;
                }
                ui.List->Selected()->SetCaption(S);
                ButtonWork(ui.UpdateButton);
            }
            break;
        case 33:
            if (ui.List->Selected() != nullptr) {
                if (MessageDlg(Format(LanguageSetup->MessageConfirmationDeleteFile(), {GetCurrentPath() + ui.List->Selected()->Caption()}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                if (!ExtDeleteFile(GetCurrentPath() + ui.List->Selected()->Caption(), ftDataViewer)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {GetCurrentPath() + ui.List->Selected()->Caption()}), mtError, mbOK, 0);
                    return;
                }
                ButtonWork(ui.UpdateButton);
            }
            break;
        case 34: RunFile(true); break;
        case 35:
            if (ui.List->Selected() != nullptr) {
                S = GetCurrentPath() + ui.List->Selected()->Caption();
                if (Trim(ExtUpperCase(S)) == Trim(ExtUpperCase(MakeAbsPath(Game->ScreenshotListScreenshot(), PrgSetup->BaseDir()))))
                    Game->SetScreenshotListScreenshot("");
                else
                    Game->SetScreenshotListScreenshot(MakeRelPath(S, PrgSetup->BaseDir()));
                if (FOnUpdateGamesList) FOnUpdateGamesList(this);
            }
            break;
    }
}

void TViewFilesFrame::TimerWork(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((hChangeNotification != 0) && FChangeSignaled) {
        FChangeSignaled = false;
        ButtonWork(ui.UpdateButton);   // re-creates the watcher (FindNextChangeNotification)
    }
}

void TViewFilesFrame::TreeDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    QString FileName, S, CurrentPath;
    TStringList* FileList;
    QTreeWidgetItem* N;

    if (Source == nullptr) return;
    FileList = Source;

    N = ui.Tree->itemAt(X, Y);
    if (N != nullptr) {
        ui.Tree->setCurrentItem(N);
        TreeChange(Sender, N);
    }
    CurrentPath = GetCurrentPath();

    for (int I = 0; I < FileList->Count(); I++) {
        FileName = (*FileList)[I];
        if (DirectoryExists(FileName)) {
            S = CurrentPath + ExtractFileName(FileName);
            if (!ForceDirectories(S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {S}), mtError, mbOK, 0);
                break;
            }
            if (!CopyFiles(FileName, S, true, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFiles(), {FileName, S}), mtError, mbOK, 0);
                break;
            }
        } else {
            S = CurrentPath + ExtractFileName(FileName);
            if (!CopyFile(FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
                break;
            }
        }
    }
    ButtonWork(ui.UpdateButton);
}

void TViewFilesFrame::ListDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    Q_UNUSED(Sender); Q_UNUSED(X); Q_UNUSED(Y);
    QString FileName, S, CurrentPath;
    TStringList* FileList;

    if (Source == nullptr) return;
    FileList = Source;

    CurrentPath = GetCurrentPath();
    for (int I = 0; I < FileList->Count(); I++) {
        FileName = (*FileList)[I];
        if (DirectoryExists(FileName)) {
            S = CurrentPath + ExtractFileName(FileName);
            if (!ForceDirectories(S)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {S}), mtError, mbOK, 0);
                break;
            }
            if (!CopyFiles(FileName, S, true, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFiles(), {FileName, S}), mtError, mbOK, 0);
                break;
            }
        } else {
            S = CurrentPath + ExtractFileName(FileName);
            if (!CopyFile(FileName, S, true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {FileName, S}), mtError, mbOK, 0);
                break;
            }
        }
    }
    ButtonWork(ui.UpdateButton);
}
