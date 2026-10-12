// C++ port of IconManagerFormUnit.pas (implementation section).
#include "IconManagerFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"
#include <QFileSystemWatcher>
#include <QMimeData>
#include <QDropEvent>
#include <QTreeView>
#include <QListView>

TIconManagerForm* IconManagerForm = nullptr;

static const QString IconFileExts[6] = {"ico", "png", "jpeg", "jpg", "gif", "bmp"};

// Application.HelpCommand(HELP_CONTEXT, ID)
static void HelpCommandContext(int ID)
{
    // TODO(port): no public help context entry point in HelpTools yet; open the help contents.
    Q_UNUSED(ID);
    if (GLOBAL_HELPROUTER != nullptr) GLOBAL_HELPROUTER->HelpContent();
}

// Adds Dir and all sub directories to the file system watcher
static void WatchFolderTree(QFileSystemWatcher* W, const QString& Dir)
{
    if (!QDir(Dir).exists()) return;
    W->addPath(Dir);
    const QStringList L = QDir(Dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString& S : L) WatchFolderTree(W, IncludeTrailingPathDelimiter(Dir) + S);
}

TIconManagerForm::TIconManagerForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = new TImageList(32, 32);
    ui.ListView->SetLargeImages(ImageList);
    ui.ListView->SetSmallImages(ImageList);
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "ico";
    Timer = new QTimer(this);
    Timer->setInterval(500);
    FWatcher = new QFileSystemWatcher(this);
    connect(FWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() { FChangeSignaled = true; });
    connect(FWatcher, &QFileSystemWatcher::fileChanged, this, [this]() { FChangeSignaled = true; });

    connect(ui.CustomIconButton, &QToolButton::clicked, this, [this]() { CustomIconButtonClick(ui.CustomIconButton); });
    connect(ui.ListView, &TListView::change, this, [this](TListItem* Item) { ListViewChange(ui.ListView, Item, 0); });
    connect(ui.ListView, &TListView::selectItem, this, [this](TListItem* Item, bool) { ListViewChange(ui.ListView, Item, 0); });
    connect(ui.ListView, &TListView::click, this, [this]() { ListViewClick(ui.ListView); });
    connect(ui.ListView, &TListView::dblClick, this, [this]() { ListViewDblClick(ui.ListView); });
    connect(ui.ListView, &TListView::keyDown, this, [this](int Key, Qt::KeyboardModifiers Mods) { int K = Key; ListViewKeyDown(ui.ListView, K, Mods); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.AddButton, &QPushButton::clicked, this, [this]() { AddButtonClick(ui.AddButton); });
    connect(ui.DelButton, &QPushButton::clicked, this, [this]() { DelButtonClick(ui.DelButton); });
    connect(ui.CustomIconEdit, &QLineEdit::textChanged, this, [this]() { CustomIconEditChange(ui.CustomIconEdit); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.ToolsButton, &QPushButton::clicked, this, [this]() { ToolsButtonClick(ui.ToolsButton); });
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    connect(ui.PopupAddIcon, &QAction::triggered, this, [this]() { AddButtonClick(ui.PopupAddIcon); });
    connect(ui.PopupAddIconsFromFolder, &QAction::triggered, this, [this]() { AddButtonClick(ui.PopupAddIconsFromFolder); });
    connect(ui.PopupAddAllIcons, &QAction::triggered, this, [this]() { AddButtonClick(ui.PopupAddAllIcons); });
    connect(ui.PopupToolsExplorer, &QAction::triggered, this, [this]() { ToolsButtonClick(ui.PopupToolsExplorer); });
    connect(ui.PopupToolsRenameUpper, &QAction::triggered, this, [this]() { ToolsButtonClick(ui.PopupToolsRenameUpper); });
    connect(ui.PopupToolsRenameLower, &QAction::triggered, this, [this]() { ToolsButtonClick(ui.PopupToolsRenameLower); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TIconManagerForm::~TIconManagerForm()
{
    FormDestroy(this);
    ui.ListView->SetLargeImages(nullptr);
    ui.ListView->SetSmallImages(nullptr);
    delete ImageList;
    delete OpenDialog;
}

void TIconManagerForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TIconManagerForm::resizeEvent(QResizeEvent *E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

bool TIconManagerForm::eventFilter(QObject *O, QEvent *E)
{
    if (E->type() == QEvent::DragEnter || E->type() == QEvent::DragMove) {
        auto* DE = static_cast<QDropEvent*>(E);
        if (DE->mimeData()->hasUrls()) { DE->acceptProposedAction(); return true; }
    }
    if (E->type() == QEvent::Drop) {
        auto* DE = static_cast<QDropEvent*>(E);
        if (DE->mimeData()->hasUrls()) {
            TStringList FileList;
            for (const QUrl& U : DE->mimeData()->urls()) if (U.isLocalFile()) FileList.Add(QDir::toNativeSeparators(U.toLocalFile()));
            DE->acceptProposedAction();
            const QPoint P = DE->position().toPoint();
            ListViewDragDrop(ui.ListView, &FileList, P.x(), P.y());
            return true;
        }
    }
    return TVclForm::eventFilter(O, E);
}

void TIconManagerForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    setWindowTitle(LanguageSetup->MenuExtrasIconManagerCaption());
    ui.LibraryIconRadioButton->setText(LanguageSetup->MenuExtrasIconManagerIconLibrary());
    ui.CustomIconRadioButton->setText(LanguageSetup->MenuExtrasIconManagerIconCustom());
    ui.CustomIconButton->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.AddButton->setText(LanguageSetup->Add());
    ui.PopupAddIcon->setText(LanguageSetup->MenuExtrasIconManagerAddSingle());
    ui.PopupAddIconsFromFolder->setText(LanguageSetup->MenuExtrasIconManagerAddIconsFromFolder());
    ui.PopupAddAllIcons->setText(LanguageSetup->MenuExtrasIconManagerAddAll());
    ui.DelButton->setText(LanguageSetup->Del());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.ToolsButton->setText(LanguageSetup->MenuExtrasIconManagerTools());
    ui.PopupToolsExplorer->setText(LanguageSetup->MenuExtrasIconManagerToolsExplorer());
    ui.PopupToolsRenameUpper->setText(LanguageSetup->MenuExtrasIconManagerToolsRenameUpper());
    ui.PopupToolsRenameLower->setText(LanguageSetup->MenuExtrasIconManagerToolsRenameLower());

    OpenDialog->Title = LanguageSetup->MenuExtrasIconManagerDialog();
    OpenDialog->Filter = LanguageSetup->MenuExtrasIconManagerFilter();

    Dir = IncludeTrailingPathDelimiter(PrgDataDir() + IconsSubDir);

    UserIconLoader->DialogImage(DI_SelectFile, ui.CustomIconButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_Tools, ui.ToolsButton);

    // FindFirstChangeNotification(PrgDataDir+IconsSubDir, True, ...)
    WatchFolderTree(FWatcher, PrgDataDir() + IconsSubDir);
    hIconsChangeNotification = FWatcher->directories().isEmpty() ? 0 : 1;
}

void TIconManagerForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    LastIcon = IconName;
    LoadIcons();

    // TListViewAcceptingFilesAndSpecialHint: accept files dropped from the file manager
    ui.ListView->setAcceptDrops(true);
    ui.ListView->installEventFilter(this);
    for (QAbstractItemView* V : {static_cast<QAbstractItemView*>(ui.ListView->reportView()), static_cast<QAbstractItemView*>(ui.ListView->iconView())}) {
        if (V == nullptr) continue;
        V->setAcceptDrops(true);
        V->viewport()->setAcceptDrops(true);
        V->viewport()->installEventFilter(this);
    }

    Timer->start();
}

void TIconManagerForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (hIconsChangeNotification != 0) {
        const QStringList D = FWatcher->directories();
        if (!D.isEmpty()) FWatcher->removePaths(D);
        hIconsChangeNotification = 0;
    }
}

void TIconManagerForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((hIconsChangeNotification != 0) && FChangeSignaled) {
        FChangeSignaled = false;
        if (ui.ListView->Selected() != nullptr) LastIcon = ui.ListView->Selected()->Caption(); else LastIcon = "";
        LoadIcons();
        // FindNextChangeNotification: watch new sub folders too
        WatchFolderTree(FWatcher, PrgDataDir() + IconsSubDir);
    }
}

static TStringList* GetIconsListForSingleDir(const QString& Folder)
{
    QString S;
    int I;
    TSearchRec Rec;
    S = IncludeTrailingPathDelimiter(Folder);
    TStringList* result = new TStringList();

    for (int J = 0; J <= 5; J++) {
        I = FindFirst(S + "*." + IconFileExts[J], faAnyFile, Rec);
        while (I == 0) {
            result->Add(S + Rec.Name);
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }
    return result;
}

static TStringList* GetIconsList(const QString& Folder)
{
    QString S;
    int I;
    TSearchRec Rec;
    TStringList* St;
    S = IncludeTrailingPathDelimiter(Folder);
    TStringList* result = GetIconsListForSingleDir(Folder);

    I = FindFirst(S + "*.*", faDirectory, Rec);
    while (I == 0) {
        if ((Rec.Name != ".") && (Rec.Name != "..") && ((Rec.Attr & faDirectory) != 0)) {
            St = GetIconsList(S + Rec.Name);
            result->AddStrings(*St);
            delete St;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TIconManagerForm::LoadIcons()
{
    TListItem* L;
    QIcon Icon;
    bool B;
    QString S;
    TStringList* St;

    ForceDirectories(Dir);

    ui.ListView->Items()->BeginUpdate();
    ui.ListView->Items()->Clear();
    ImageList->Clear();
    ui.ListView->SetSortType(stNone);

    St = GetIconsList(Dir);
    for (int I = 0; I < St->Count(); I++) {
        B = true;
        if (ExtUpperCase(ExtractFileExt((*St)[I])) == ".ICO") {
            Icon = QIcon((*St)[I]);
        } else {
            Icon = LoadImageAsIconFromFile((*St)[I]);
        }
        if (Icon.isNull() || Icon.availableSizes().isEmpty()) {
            // ImageList.AddIcon failed -> LoadIconFromExeFile
            Icon = LoadIconFromExeFile((*St)[I]);
            if (Icon.isNull()) B = false;
        }
        if (B) {
            ImageList->AddIcon(Icon);
            L = ui.ListView->Items()->Add();
            L->SetCaption(ExtractFileName((*St)[I]));
            L->SetImageIndex(ImageList->Count() - 1);
        }
    }
    delete St;

    if (ExtractFilePath(Trim(LastIcon)) == "") {
        S = ExtUpperCase(LastIcon);
        ui.LibraryIconRadioButton->setChecked(true);
        for (int I = 0; I < ui.ListView->Items()->Count(); I++) if (ExtUpperCase(ui.ListView->Items()->Item(I)->Caption()) == S) {
            ui.ListView->SetItemIndex(I); break;
        }
    } else {
        ui.CustomIconRadioButton->setChecked(true);
        ui.CustomIconEdit->setText(LastIcon);
    }

    ui.ListView->SetSortType(stText);
    ui.ListView->Items()->EndUpdate();
    if (ui.ListView->Selected() != nullptr) ui.ListView->Selected()->MakeVisible();
}

void TIconManagerForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.LibraryIconRadioButton->isChecked()) {
        if (ui.ListView->ItemIndex() >= 0) {
            IconName = ui.ListView->Items()->Item(ui.ListView->ItemIndex())->Caption();
        } else {
            IconName = "";
        }
    } else {
        IconName = ui.CustomIconEdit->text();
    }
}

void TIconManagerForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    // PostMessage(Handle,WM_User+2,0,0)
    QTimer::singleShot(0, this, [this]() { QEvent* Msg = nullptr; PostResize(Msg); });
}

void TIconManagerForm::PostResize(QEvent*& Msg)
{
    Q_UNUSED(Msg);
    ui.ListView->Arrange();
}

void TIconManagerForm::ListViewChange(QObject* Sender, TListItem* Item, int Change)
{
    Q_UNUSED(Sender); Q_UNUSED(Item); Q_UNUSED(Change);
    ui.DelButton->setEnabled(ui.ListView->ItemIndex() >= 0);
}

void TIconManagerForm::ListViewDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.LibraryIconRadioButton->setChecked(true);
    ui.OKButton->click();
}

void TIconManagerForm::ListViewDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    Q_UNUSED(Sender); Q_UNUSED(X); Q_UNUSED(Y);
    TStringList* FileList;
    if (Source == nullptr) return;
    FileList = Source;

    for (int I = 0; I < FileList->Count(); I++) {
        CopyFile((*FileList)[I], PrgDataDir() + IconsSubDir + PathDelim + ExtractFileName((*FileList)[I]), false);
    }
}

void TIconManagerForm::ListViewKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Shift &= ~Qt::KeypadModifier;
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
        case Qt::Key_Insert: AddButtonClick(Sender); break;
        case Qt::Key_Return: case Qt::Key_Enter: ListViewDblClick(Sender); break;
        case Qt::Key_Delete: DelButtonClick(Sender); break;
    }
}

void TIconManagerForm::AddButtonClick(QObject* Sender)
{
    QPoint P;
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            P = mapToGlobal(QPoint(ui.AddButton->x(), ui.AddButton->y()));
            ui.AddPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 1:
            S = ExtractFilePath(MakeAbsIconName(Trim(ui.CustomIconEdit->text())));
            if (S == "") S = DefaultCustomIconFolder;
            if (S == "") S = PrgSetup->BaseDir();
            if (S != "") OpenDialog->InitialDir = S;
            if (!OpenDialog->Execute()) return;
            if (!CopyFile(OpenDialog->FileName, Dir + ExtractFileName(OpenDialog->FileName), true)) {
                MessageDlg(Format(LanguageSetup->MessageCouldNotCopyFile(), {OpenDialog->FileName, Dir + ExtractFileName(OpenDialog->FileName)}), mtError, mbOK, 0);
                return;
            }
            LastIcon = ExtractFileName(OpenDialog->FileName);
            LoadIcons();
            break;
        case 2:
            setEnabled(false);
            ScanFolderForIcons(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));
            setEnabled(true);
            LoadIcons();
            break;
        case 3:
            setEnabled(false);
            S = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
            if (!SelectDirectory(LanguageSetup->MenuExtrasIconManagerAddIconsFromFolderCaption(), QString(), S, this)) { setEnabled(true); return; }
            ScanFolderForIcons(S);
            setEnabled(true);
            LoadIcons();
            break;
    }
}

void TIconManagerForm::ScanFolderForIcons(const QString& Folder)
{
    TSearchRec Rec;
    int I;

    ProcessMessages();

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.ico", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) AddIconFromFolder(IncludeTrailingPathDelimiter(Folder) + Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    I = FindFirst(IncludeTrailingPathDelimiter(Folder) + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) == faDirectory) && (Rec.Name != ".") && (Rec.Name != ".."))
            ScanFolderForIcons(IncludeTrailingPathDelimiter(Folder) + Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void TIconManagerForm::ListViewClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.LibraryIconRadioButton->setChecked(true);
}

void TIconManagerForm::CustomIconEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CustomIconRadioButton->setChecked(true);
}

void TIconManagerForm::AddIconFromFolder(const QString& IconFile)
{
    QString DestFile;
    int I;

    ProcessMessages();

    DestFile = PrgDataDir() + IconsSubDir + PathDelim + ExtractFileName(IconFile);

    if (!FileExists(DestFile)) {
        CopyFile(IconFile, DestFile, true);
        return;
    }

    if (CompareFiles(IconFile, DestFile)) return;

    I = 1;
    while (FileExists(ChangeFileExt(DestFile, IntToStr(I) + ".ico"))) {
        if (CompareFiles(IconFile, ChangeFileExt(DestFile, IntToStr(I) + ".ico"))) return;
        I++;
    }

    CopyFile(IconFile, ChangeFileExt(DestFile, IntToStr(I) + ".ico"), true);
}

void TIconManagerForm::CustomIconButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = ExtractFilePath(MakeAbsIconName(Trim(ui.CustomIconEdit->text())));
    if (S == "") S = DefaultCustomIconFolder;
    if (S == "") S = PrgSetup->BaseDir();
    if (S != "") OpenDialog->InitialDir = S;
    if (!OpenDialog->Execute()) return;
    ui.CustomIconEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
    ui.CustomIconRadioButton->setChecked(true);
}

void TIconManagerForm::DelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    if (ui.ListView->ItemIndex() < 0) return;

    S = Dir + ui.ListView->Items()->Item(ui.ListView->ItemIndex())->Caption();
    if (!ExtDeleteFile(S, ftProfile)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {S}), mtError, mbOK, 0);
        return;
    }

    ui.ListView->Items()->Delete(ui.ListView->ItemIndex());
}

void TIconManagerForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ExtrasManageIcons);
}

void TIconManagerForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

void TIconManagerForm::ToolsButtonClick(QObject* Sender)
{
    QPoint P;
    switch (Sender->property("tag").toInt()) {
        case 0:
            P = mapToGlobal(QPoint(ui.ToolsButton->x(), ui.ToolsButton->y()));
            ui.ToolsPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 1:
            OpenFolder(Dir);
            break;
        case 2: RenameIcons(true); break;
        case 3: RenameIcons(false); break;
    }
}

void TIconManagerForm::RenameIcons(bool UpperExt)
{
    TSearchRec Rec;
    int I;
    QString NewName;

    for (int J = 0; J <= 5; J++) {
        I = FindFirst(Dir + "*." + IconFileExts[J], faAnyFile, Rec);
        while (I == 0) {
            NewName = ExtractFileExt(Rec.Name);
            if (UpperExt) NewName = ExtUpperCase(NewName); else NewName = ExtLowerCase(NewName);
            NewName = ChangeFileExt(Rec.Name, NewName);
            RenameFile(Dir + Rec.Name, Dir + NewName);
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }
}

// global

bool ShowIconManager(QWidget* AOwner, QString& AIcon, const QString& ADefaultCustomIconFolder, bool NoCancel)
{
    bool result;
    IconManagerForm = new TIconManagerForm(AOwner);
    IconManagerForm->IconName = AIcon;
    IconManagerForm->DefaultCustomIconFolder = ADefaultCustomIconFolder;
    if (NoCancel) {
        IconManagerForm->ui.CancelButton->setVisible(false);
        IconManagerForm->ui.LibraryIconRadioButton->setVisible(false);
        IconManagerForm->ui.CustomIconRadioButton->setVisible(false);
        IconManagerForm->ui.CustomIconEdit->setVisible(false);
        IconManagerForm->ui.CustomIconButton->setVisible(false);
        VclLayout::setAlignedExtent(IconManagerForm->ui.ListView, IconManagerForm->ui.OKButton->y() - 10);
    }
    result = (IconManagerForm->ShowModal() == mrOk);
    if (result) AIcon = IconManagerForm->IconName;
    delete IconManagerForm;
    IconManagerForm = nullptr;
    return result;
}
