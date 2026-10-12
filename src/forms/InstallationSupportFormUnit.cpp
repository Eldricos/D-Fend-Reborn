// C++ port of InstallationSupportFormUnit.pas (implementation section).
#include "InstallationSupportFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "ZipInfoFormUnit.h"
#include "InstallationRunFormUnit.h"
#include "ZipPackageUnit.h"
#include "PrgConsts.h"
#include "ImageTools.h"
#include "PrgSetupUnit.h"
#include "SmallWaitFormUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TInstallationSupportForm* InstallationSupportForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT,ContextID) - routed through
// THTMLhelpRouter.OnRouteHelp in the original program.
static void ApplicationHelpContext(int ContextID)
{
    if (HtmlHelpA && GLOBAL_HELPROUTER != nullptr) {
        HtmlHelpA(0, GLOBAL_HELPROUTER->Helpfile(), HH_HELP_CONTEXT, ContextID);
    }
    // TODO(port): no HTML help (.chm) viewer outside of Windows
}

static bool SystemHasFloppyDiskDrive()
{
#ifdef Q_OS_WIN
    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) == DRIVE_REMOVABLE) return true;
    }
    return false;
#else
    // TODO(port): no drive letters / floppy drive detection outside of Windows
    return false;
#endif
}

// InstallTypeComboBox.Items.Objects[I]
static int ComboObject(QComboBox* C, int I) { return C->itemData(I).toInt(); }

TInstallationSupportForm::TInstallationSupportForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);

    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton); });
    connect(ui.UpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.UpButton); });
    connect(ui.DownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DownButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.InstallTypeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { InstallTypeComboBoxChange(ui.InstallTypeComboBox); });
    connect(ui.ListBox, &QListWidget::itemSelectionChanged, this, [this]() { ListBoxClick(ui.ListBox); });
    ui.ListBox->installEventFilter(this);
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });

    FormCreate(this);
}

TInstallationSupportForm::~TInstallationSupportForm()
{
    delete OpenDialog;
}

void TInstallationSupportForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TInstallationSupportForm::eventFilter(QObject* Watched, QEvent* E)
{
    if (Watched == ui.ListBox) {
        switch (E->type()) {
            case QEvent::KeyPress: {  // ListBox.OnKeyDown
                QKeyEvent* KE = static_cast<QKeyEvent*>(E);
                int Key = KE->key();
                ListBoxKeyDown(ui.ListBox, Key, KE->modifiers());
                if (Key == 0) return true;
                break;
            }
            case QEvent::DragEnter:
            case QEvent::DragMove:  // TListBoxAcceptingFiles: accept files from the file manager
                if (AcceptFilesActivated && static_cast<QDropEvent*>(E)->mimeData()->hasUrls()) {
                    static_cast<QDropEvent*>(E)->acceptProposedAction();
                    return true;
                }
                break;
            case QEvent::Drop:
                if (AcceptFilesActivated && static_cast<QDropEvent*>(E)->mimeData()->hasUrls()) {
                    QDropEvent* DE = static_cast<QDropEvent*>(E);
                    TStringList St;
                    for (const QUrl& U : DE->mimeData()->urls()) if (U.isLocalFile()) St.Add(QDir::toNativeSeparators(U.toLocalFile()));
                    DE->acceptProposedAction();
                    const QPoint P = DE->position().toPoint();
                    ListBoxDragDrop(ui.ListBox, &St, P.x(), P.y());
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return TVclForm::eventFilter(Watched, E);
}

void TInstallationSupportForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->InstallationSupport());
    ui.InstallTypeComboBox->clear();
    if (SystemHasFloppyDiskDrive()) ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceFloppy(), 0);
    ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceFolder(), 1);
    ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceArchive(), 2);
    ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceFloppyImage(), 3);
    ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceCD(), 4);
    ui.InstallTypeComboBox->addItem(LanguageSetup->InstallationSupportSourceCDImage(), 5);
    ui.AlwaysMountSourceCheckBox->setText(LanguageSetup->InstallationSupportAlwaysMountSource());
    ui.AlwaysMountSourceComboBox->clear();
    ui.AlwaysMountSourceComboBox->addItem(LanguageSetup->InstallationSupportAlwaysMountSourceCopy());
    ui.AlwaysMountSourceComboBox->addItem(LanguageSetup->InstallationSupportAlwaysMountSourceLink());
    ui.AlwaysMountSourceComboBox->setCurrentIndex(0);

    ui.UpButton->setToolTip(LanguageSetup->MoveUp());
    ui.DownButton->setToolTip(LanguageSetup->MoveDown());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.CDImageTypeLabel->setText(LanguageSetup->ProfileMountingImageTypeInfo());

    OpenDialog->Title = LanguageSetup->ChooseFile();

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    UserIconLoader->DialogImage(DI_Up, ui.UpButton);
    UserIconLoader->DialogImage(DI_Down, ui.DownButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    if (ui.InstallTypeComboBox->count() == 6) ui.InstallTypeComboBox->setCurrentIndex(2); else ui.InstallTypeComboBox->setCurrentIndex(1);
    LastInstallType = -1;
    InstallTypeComboBoxChange(this);
}

void TInstallationSupportForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // ListBox:=TListBox(NewWinControlType(ListBox,TListBoxAcceptingFiles,ctcmDangerousMagic));
    // TListBoxAcceptingFiles(ListBox).ActivateAcceptFiles;
    // -> the drop events of the list box are handled in eventFilter
    if (!AcceptFilesActivated) {
        ui.ListBox->setAcceptDrops(true);
        ui.ListBox->viewport()->setAcceptDrops(true);
        ui.ListBox->setDragDropMode(QAbstractItemView::DropOnly);
        AcceptFilesActivated = true;
    }
}

void TInstallationSupportForm::InstallTypeComboBoxChange(QObject* Sender)
{
    int I;

    FormShow(Sender);

    if ((LastInstallType >= 0) && (ui.InstallTypeComboBox->currentIndex() != LastInstallType) && ui.ListBox->isVisible() && (ui.ListBox->count() > 0)) {
        if (MessageDlg(LanguageSetup->InstallationSupportSourceClearList(), mtWarning, mbYes | mbNo, 0) != mrYes) {
            ui.InstallTypeComboBox->setCurrentIndex(LastInstallType); return;
        }
        ui.ListBox->clear();
    }
    if (LastInstallType == ui.InstallTypeComboBox->currentIndex()) return;
    LastInstallType = ui.InstallTypeComboBox->currentIndex();

    if (ui.InstallTypeComboBox->currentIndex() < 0) {
        ui.ListBox->setVisible(false);
        ui.AddButton->setVisible(false);
        ui.DelButton->setVisible(false);
        ui.CDImageTypeLabel->setVisible(false);
        ui.DropInfoLabel->setVisible(false);
        ui.AlwaysMountSourceCheckBox->setVisible(false);
        ui.OKButton->setEnabled(false);
        return;
    }

    I = ComboObject(ui.InstallTypeComboBox, ui.InstallTypeComboBox->currentIndex());

    ui.ListBox->setVisible((I != 0) && (I != 4));
    ui.AddButton->setVisible((I != 0) && (I != 4));
    ui.DelButton->setVisible((I != 0) && (I != 4));
    ui.UpButton->setVisible((I != 0) && (I != 4));
    ui.DownButton->setVisible((I != 0) && (I != 4));
    ui.DropInfoLabel->setVisible((I != 0) && (I != 4));
    ui.AlwaysMountSourceCheckBox->setVisible((I == 4) || (I == 5));
    ui.AlwaysMountSourceComboBox->setVisible(I == 5);
    ui.InsertMediaLabel->setVisible((I == 0) || (I == 4));
    ui.CDImageTypeLabel->setVisible(I == 5);

    // ListBox.Visible is evaluated before the form is shown -> use isVisibleTo
    ui.OKButton->setEnabled(!ui.ListBox->isVisibleTo(this));
    ui.UpButton->setEnabled(false);
    ui.DownButton->setEnabled(false);

    QWidget* LB = ui.ListBox;
    QLabel* DIL = ui.DropInfoLabel;
    auto SetTop = [](QWidget* W, int Top) { VclLayout::setBounds(W, W->x(), Top, W->width(), W->height()); };
    auto SetHeight = [](QWidget* W, int Height) { VclLayout::setBounds(W, W->x(), W->y(), W->width(), Height); };

    switch (I) {
        case 0: /* Install from real floppy disks */
            ui.InsertMediaLabel->setText(LanguageSetup->InstallationSupportInsertFirstFloppy());
            break;
        case 1: /* Install from folder */
            ui.AddButton->setToolTip(LanguageSetup->InstallationSupportAddFolder());
            ui.DelButton->setToolTip(LanguageSetup->InstallationSupportDelFolder());
            DIL->setText(LanguageSetup->InstallationSupportDragDropFolder());
            SetTop(DIL, ui.AlwaysMountSourceComboBox->y());
            SetHeight(LB, DIL->y() - 2 - LB->y());
            break;
        case 2: /* Install from archive files */
            ui.AddButton->setToolTip(LanguageSetup->InstallationSupportAddArchive());
            ui.DelButton->setToolTip(LanguageSetup->InstallationSupportDelArchive());
            DIL->setText(LanguageSetup->InstallationSupportDragDropArchive());
            SetTop(DIL, ui.AlwaysMountSourceComboBox->y());
            SetHeight(LB, DIL->y() - 2 - LB->y());
            break;
        case 3: /* Install from floppy image */
            ui.AddButton->setToolTip(LanguageSetup->InstallationSupportAddFloppyImage());
            ui.DelButton->setToolTip(LanguageSetup->InstallationSupportDelFloppyImage());
            DIL->setText(LanguageSetup->InstallationSupportDragDropFloppyImage());
            SetTop(DIL, ui.AlwaysMountSourceComboBox->y());
            SetHeight(LB, DIL->y() - 2 - LB->y());
            break;
        case 4: /* Install from CD drive */
            ui.InsertMediaLabel->setText(LanguageSetup->InstallationSupportInsertFirstCD());
            break;
        case 5: /* Install from CD image */
            ui.AddButton->setToolTip(LanguageSetup->InstallationSupportAddCDImage());
            ui.DelButton->setToolTip(LanguageSetup->InstallationSupportDelCDImage());
            DIL->setText(LanguageSetup->InstallationSupportDragDropCDImage());
            SetTop(DIL, ui.AlwaysMountSourceCheckBox->y() - 5 - DIL->height());
            SetTop(ui.CDImageTypeLabel, DIL->y() - ui.CDImageTypeLabel->height());
            SetHeight(LB, ui.CDImageTypeLabel->y() - 2 - LB->y());
            break;
    }

    ListBoxClick(Sender);
}

void TInstallationSupportForm::ListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DelButton->setEnabled(ui.ListBox->currentRow() >= 0);
    ui.UpButton->setEnabled(ui.ListBox->currentRow() >= 1);
    ui.DownButton->setEnabled((ui.ListBox->currentRow() >= 0) && (ui.ListBox->currentRow() < ui.ListBox->count() - 1));
}

void TInstallationSupportForm::ListBoxKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift == Qt::NoModifier) {
        switch (Key) {
            case Qt::Key_Insert: ButtonWork(ui.AddButton); Key = 0; break;
            case Qt::Key_Delete: ButtonWork(ui.DelButton); Key = 0; break;
        }
    }
}

void TInstallationSupportForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    QString S;

    I = ComboObject(ui.InstallTypeComboBox, ui.InstallTypeComboBox->currentIndex());

    switch (I) {
        case 3: /* Install from floppy image */
            if ((ui.ListBox->count() > 1) && (!PrgSetup->AllowMultiFloppyImagesMount())) {
                MessageDlg(LanguageSetup->InstallationSupportMultiDiskError(), mtError, mbOK, 0);
                setModalResult(mrNone); return;
            }
            break;
        case 5: /* Install from CD image */
            for (I = 0; I < ui.ListBox->count(); I++) {
                S = MakeAbsPath(ui.ListBox->item(I)->text(), PrgSetup->BaseDir());
                if (!CheckCDImage(S)) {
                    if (MessageDlg(Format(LanguageSetup->ProfileMountingImageTypeWarning(), {S}), mtWarning, mbYes | mbNo, 0) != mrYes) { setModalResult(mrNone); return; }
                }
            }
            break;
    }
}

void TInstallationSupportForm::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
        case 0:
            switch (ComboObject(ui.InstallTypeComboBox, ui.InstallTypeComboBox->currentIndex())) {
                case 1: /* Install from folder */
                    if (SelectDirectory((THandle)winId(), LanguageSetup->ChooseFolder(), S)) {
                        if (!EntryInList(S)) {
                            ui.ListBox->addItem(S);
                            ui.ListBox->setCurrentRow(ui.ListBox->count() - 1);
                        }
                    }
                    break;
                case 2: /* Install from archive files */
                    OpenDialog->DefaultExt = "zip";
                    OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
                    if (OpenDialog->Execute() && !EntryInList(OpenDialog->FileName)) {
                        ui.ListBox->addItem(OpenDialog->FileName);
                        ui.ListBox->setCurrentRow(ui.ListBox->count() - 1);
                    }
                    break;
                case 3: /* Install from floppy image */
                    OpenDialog->DefaultExt = "img";
                    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilterImgOnly();
                    if (OpenDialog->Execute() && !EntryInList(OpenDialog->FileName)) {
                        ui.ListBox->addItem(OpenDialog->FileName);
                        ui.ListBox->setCurrentRow(ui.ListBox->count() - 1);
                    }
                    break;
                case 5: /* Install from CD image */
                    OpenDialog->DefaultExt = "iso";
                    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
                    if (OpenDialog->Execute() && !EntryInList(OpenDialog->FileName)) {
                        ui.ListBox->addItem(OpenDialog->FileName);
                        ui.ListBox->setCurrentRow(ui.ListBox->count() - 1);
                    }
                    break;
            }
            break;
        case 1:
            if (ui.ListBox->currentRow() >= 0) { delete ui.ListBox->takeItem(ui.ListBox->currentRow()); ListBoxClick(Sender); }
            break;
        case 2:
            if (ui.ListBox->currentRow() >= 1) {
                I = ui.ListBox->currentRow();
                S = ui.ListBox->item(I - 1)->text(); ui.ListBox->item(I - 1)->setText(ui.ListBox->item(I)->text()); ui.ListBox->item(I)->setText(S);
                ui.ListBox->setCurrentRow(I - 1);
            }
            break;
        case 3:
            if ((ui.ListBox->currentRow() >= 0) && (ui.ListBox->currentRow() < ui.ListBox->count() - 1)) {
                I = ui.ListBox->currentRow();
                S = ui.ListBox->item(I + 1)->text(); ui.ListBox->item(I + 1)->setText(ui.ListBox->item(I)->text()); ui.ListBox->item(I)->setText(S);
                ui.ListBox->setCurrentRow(I + 1);
            }
            break;
    }
    ui.OKButton->setEnabled((!ui.ListBox->isVisible()) || (ui.ListBox->count() > 0));
    ListBoxClick(Sender);
}

void TInstallationSupportForm::ListBoxDragDrop(QObject* Sender, TStringList* Source, int X, int Y)
{
    Q_UNUSED(X); Q_UNUSED(Y);
    int I, J, Nr;
    TStringList* St;
    QString S;

    if (Source == nullptr) return;
    St = Source;

    if ((St->Count() > 0) && (ui.ListBox->count() == 0)) {
        S = (*St)[0]; Nr = -1;
        if (DirectoryExists(S)) Nr = 1; else {
            S = ExtUpperCase(ExtractFileExt(S));
            if ((Nr < 0) && ((S == ".ISO") || (S == ".BIN") || (S == ".CUE"))) Nr = 5;
            if ((Nr < 0) && ((S == ".IMG") || (S == ".IMA"))) Nr = 3;
            if ((Nr < 0) && ((S == ".ZIP") || (S == ".7Z"))) Nr = 2;
            if (Nr < 0) {
                for (I = 0; I < PrgSetup->PackerSettingsCount(); I++) {
                    if (ExtensionInList(S, PrgSetup->PackerSettings(I)->FileExtensions())) { Nr = 2; break; }
                }
            }
        }
        for (I = 0; I < ui.InstallTypeComboBox->count(); I++) {
            if (ComboObject(ui.InstallTypeComboBox, I) == Nr) {
                if (I != ui.InstallTypeComboBox->currentIndex()) {
                    ui.InstallTypeComboBox->setCurrentIndex(I);
                    InstallTypeComboBoxChange(Sender);
                }
                break;
            }
        }
    }

    for (I = 0; I < St->Count(); I++) {
        if (EntryInList((*St)[I])) continue;
        if (ui.InstallTypeComboBox->currentIndex() >= 0) {
            J = ComboObject(ui.InstallTypeComboBox, ui.InstallTypeComboBox->currentIndex());
            switch (J) {
                case 1: /* Install from folder */
                    if (!DirectoryExists((*St)[I])) continue;
                    break;
                case 2: /* Install from archive files */
                    if (DirectoryExists((*St)[I])) continue;
                    if (!FileExists((*St)[I])) continue;
                    break;
                case 3: /* Install from floppy image */
                    if (DirectoryExists((*St)[I])) continue;
                    if (!FileExists((*St)[I])) continue;
                    break;
                case 5: /* Install from CD image */
                    if (DirectoryExists((*St)[I])) continue;
                    if (!FileExists((*St)[I])) continue;
                    break;
            }
        }
        ui.ListBox->addItem((*St)[I]);
    }
    if (ui.ListBox->count() >= 0) ui.ListBox->setCurrentRow(ui.ListBox->count() - 1);
    ui.OKButton->setEnabled((!ui.ListBox->isVisible()) || (ui.ListBox->count() > 0));
    ListBoxClick(Sender);
}

bool TInstallationSupportForm::EntryInList(const QString& S)
{
    int I;
    QString T;
    T = ExtUpperCase(S);
    for (I = 0; I < ui.ListBox->count(); I++) if (ExtUpperCase(ui.ListBox->item(I)->text()) == T) return true;
    return false;
}

void TInstallationSupportForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpContext(ID_ProfileInstallationSupport);
}

void TInstallationSupportForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

/* global */

static void AddDriveToGame(TGame* Game, const QString& DriveSetup)
{
    int I, J, K;
    QString S;
    TStringList* St;
    QString FreeLetters;

    FreeLetters = "CDEFGHIJKLMNOPQRSTUVWXYZ";

    J = Game->NrOfMounts();

    for (I = 0; I < J; I++) {
        St = ValueToList(Game->Mount(I));
        if (St->Count() >= 3) {
            K = Pos((*St)[2], FreeLetters); if (K > 0) FreeLetters[K - 1] = '-';
        }
        delete St;
    }

    for (I = 1; I <= Length(FreeLetters); I++) if (FreeLetters[I - 1] != '-') { S = FreeLetters[I - 1]; break; }

    Game->SetNrOfMounts(J + 1);
    Game->SetMount(J, Format(DriveSetup, {S}));

    Game->StoreAllValues();
    Game->LoadCache();
}

static void AddCDDrive(TGame* Game)
{
    AddDriveToGame(Game, "ASK;CDROM;%s;true;;");
}

static void AddCDImages(TGame* Game, TStringList* St, bool CopyFiles)
{
    QString Path;
    int I, J;
    QString Images, S, T;

    Images = "";

    if (CopyFiles) {
        Path = IncludeTrailingPathDelimiter(ExtractFilePath(MakeAbsPath(Game->GameExe(), PrgSetup->BaseDir()))) + "CDImages" + PathDelim;
        if (!ForceDirectories(Path)) return;
        LoadAndShowSmallWaitForm(LanguageSetup->InstallationSupportAlwaysMountSourceWait());
        for (I = 0; I < St->Count(); I++) {
            T = ExtractFileName((*St)[I]);
            S = Path + T;

            /* Check if destination file already exists, choose other destination name in this case */
            J = 1;
            while (FileExists(S)) {
                S = Path + ChangeFileExt(T, "-" + IntToStr(J) + "." + Copy(ExtractFileExt(T), 2, MaxInt));
                J++;
            }

            /* Copy image file */
            CopyFile((*St)[I], S, false);

            /* One cue/bin images copy other file */
            if ((ExtUpperCase(ExtractFileExt((*St)[I])) == ".CUE") && FileExists(ChangeFileExt((*St)[I], ".bin"))) {
                CopyFile(ChangeFileExt((*St)[I], ".bin"), ChangeFileExt(S, ".bin"), false);
            }
            if ((ExtUpperCase(ExtractFileExt((*St)[I])) == ".BIN") && FileExists(ChangeFileExt((*St)[I], ".cue"))) {
                CopyFile(ChangeFileExt((*St)[I], ".cue"), ChangeFileExt(S, ".cue"), false);
            }

            /* Add to mount command */
            S = MakeRelPath(S, PrgSetup->BaseDir());
            if (Images == "") Images = S; else Images = Images + "$" + S;
        }
        FreeSmallWaitForm();
    } else {
        for (I = 0; I < St->Count(); I++) {
            S = MakeRelPath((*St)[I], PrgSetup->BaseDir());
            if (Images == "") Images = S; else Images = Images + "$" + S;
        }
    }

    AddDriveToGame(Game, Images + ";CDROMIMAGE;%s;;;");
}

TGame* ShowInstallationSupportDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    int I;
    TGame* result = nullptr;

    InstallationSupportForm = new TInstallationSupportForm(AOwner);
    if (InstallationSupportForm->ShowModal() == mrOk) {
        InstallationRunForm = new TInstallationRunForm(AOwner);
        I = ComboObject(InstallationSupportForm->ui.InstallTypeComboBox, InstallationSupportForm->ui.InstallTypeComboBox->currentIndex());
        switch (I) {
            case 0: InstallationRunForm->InstallType = itFloppy; break;
            case 1: InstallationRunForm->InstallType = itFolder; break;
            case 2: InstallationRunForm->InstallType = itArchive; break;
            case 3: InstallationRunForm->InstallType = itFloppyImage; break;
            case 4: InstallationRunForm->InstallType = itCD; break;
            case 5: InstallationRunForm->InstallType = itCDImage; break;
        }
        for (int K = 0; K < InstallationSupportForm->ui.ListBox->count(); K++) InstallationRunForm->Sources->Add(InstallationSupportForm->ui.ListBox->item(K)->text());
        if (InstallationRunForm->ShowModal() == mrOk) result = CreateGameFromSimpleFolder(InstallationRunForm->NewGameDir, AGameDB, "", true, true);
        if ((result != nullptr) && ((I == 4) || (I == 5)) && InstallationSupportForm->ui.AlwaysMountSourceCheckBox->isChecked()) {
            switch (I) {
                case 4: AddCDDrive(result); break;
                case 5: AddCDImages(result, InstallationRunForm->Sources, InstallationSupportForm->ui.AlwaysMountSourceComboBox->currentIndex() == 0); break;
            }
        }
        delete InstallationRunForm;
        InstallationRunForm = nullptr;
    }
    delete InstallationSupportForm;
    InstallationSupportForm = nullptr;
    return result;
}

TGame* RunInstallationFromFolder(QWidget* AOwner, TGameDB* AGameDB, const QString& FolderName)
{
    TGame* result = nullptr;
    InstallationRunForm = new TInstallationRunForm(AOwner);
    InstallationRunForm->InstallType = itFolder;
    InstallationRunForm->Sources->Add(FolderName);
    if (InstallationRunForm->ShowModal() == mrOk) result = CreateGameFromSimpleFolder(InstallationRunForm->NewGameDir, AGameDB, "", true, true);
    delete InstallationRunForm;
    InstallationRunForm = nullptr;
    return result;
}

TGame* RunInstallationFromDiskImage(QWidget* AOwner, TGameDB* AGameDB, const QString& FileName)
{
    TGame* result = nullptr;
    InstallationRunForm = new TInstallationRunForm(AOwner);
    InstallationRunForm->InstallType = itFloppyImage;
    InstallationRunForm->Sources->Add(FileName);
    if (InstallationRunForm->ShowModal() == mrOk) result = CreateGameFromSimpleFolder(InstallationRunForm->NewGameDir, AGameDB, "", true, true);
    delete InstallationRunForm;
    InstallationRunForm = nullptr;
    return result;
}

TGame* RunInstallationFromCDImage(QWidget* AOwner, TGameDB* AGameDB, const QString& FileName)
{
    TGame* result = nullptr;
    InstallationRunForm = new TInstallationRunForm(AOwner);
    InstallationRunForm->InstallType = itCDImage;
    InstallationRunForm->Sources->Add(FileName);
    if (InstallationRunForm->ShowModal() == mrOk) result = CreateGameFromSimpleFolder(InstallationRunForm->NewGameDir, AGameDB, "", true, true);
    if (result != nullptr) AddCDImages(result, InstallationRunForm->Sources, true);
    delete InstallationRunForm;
    InstallationRunForm = nullptr;
    return result;
}

TGame* RunInstallationFromZipFile(QWidget* AOwner, TGameDB* AGameDB, const QString& FileName, const QString& FilesAlreadyInTempDir)
{
    TGame* result = nullptr;
    InstallationRunForm = new TInstallationRunForm(AOwner);
    InstallationRunForm->InstallType = itArchive;
    InstallationRunForm->Sources->Add(FileName);
    InstallationRunForm->FilesAlreadyInTempDir = FilesAlreadyInTempDir;
    if (InstallationRunForm->ShowModal() == mrOk) result = CreateGameFromSimpleFolder(InstallationRunForm->NewGameDir, AGameDB, "", true, true);
    delete InstallationRunForm;
    InstallationRunForm = nullptr;
    return result;
}

static void FindProgramFiles(const QString& Dir, TStringList* St)
{
    TSearchRec Rec;
    int I, J;
    for (J = 0; J < ProgramExts.size(); J++) {
        I = FindFirst(Dir + "*." + ProgramExts[J], faAnyFile, Rec);
        while (I == 0) { if ((Rec.Attr & faDirectory) == 0) St->Add(ExtUpperCase(ChangeFileExt(Rec.Name, ""))); I = FindNext(Rec); }
        FindClose(Rec);
    }
}

bool TestInstallationSupportNeeded(const QString& AFolder)
{
    TStringList St;
    TStringList* Names;
    int I, J;
    bool B;

    FindProgramFiles(IncludeTrailingPathDelimiter(AFolder), &St);
    I = 0;
    while (I < St.Count()) {
        B = true; for (J = 0; J < IgnoreGameExeFilesIgnore.size(); J++) if (St[I] == IgnoreGameExeFilesIgnore[J]) { B = false; break; }
        if (B) I++; else St.Delete(I);
    }
    if (St.Count() != 1) return false;
    Names = ValueToList(PrgSetup->InstallerNames());
    bool result = false;
    for (I = 0; I < Names->Count(); I++) if (ExtUpperCase((*Names)[I]) == St[0]) { result = true; break; }
    delete Names;
    return result;
}
