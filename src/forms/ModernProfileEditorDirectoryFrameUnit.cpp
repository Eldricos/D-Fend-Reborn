// C++ port of ModernProfileEditorDirectoryFrameUnit.pas (implementation section).
#include "ModernProfileEditorDirectoryFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"

// ListBox.Items as TStringList
static TStringList ListItems(QListWidget* L)
{
    TStringList St;
    for (int I = 0; I < L->count(); I++) St.Add(L->item(I)->text());
    return St;
}

/* TModernProfileEditorDirectoryFrame */

TModernProfileEditorDirectoryFrame::TModernProfileEditorDirectoryFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    Timer = new QTimer(this);
    Timer->setInterval(100);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    OpenDialog = new TOpenDialog(this);

    for (QToolButton* B : {ui.ScreenshotFolderEditButton, ui.DataFolderEditButton, ui.ExtraDirsAddButton, ui.ExtraDirsEditButton, ui.ExtraDirsDelButton, ui.ExtraFilesAddButton, ui.ExtraFilesEditButton, ui.ExtraFilesDelButton})
        connect(B, &QToolButton::clicked, this, [this, B]() { ButtonWork(B); });
    connect(ui.ScreenshotFolderEdit, &QLineEdit::textChanged, this, [this]() { ScreenshotFolderEditChange(ui.ScreenshotFolderEdit); });
    connect(ui.GenerateScreenshotFolderNameButton, &QPushButton::clicked, this, [this]() { GenerateScreenshotFolderNameButtonClick(ui.GenerateScreenshotFolderNameButton); });
    connect(ui.GenerateGameDataFolderNameButton, &QPushButton::clicked, this, [this]() { GenerateGameDataFolderNameButtonClick(ui.GenerateGameDataFolderNameButton); });
    connect(ui.ExtraDirsListBox, &QListWidget::currentRowChanged, this, [this]() { ExtraDirsListBoxClick(ui.ExtraDirsListBox); });
    connect(ui.ExtraDirsListBox, &QListWidget::itemDoubleClicked, this, [this]() { ExtraDirsListBoxDblClick(ui.ExtraDirsListBox); });
    connect(ui.ExtraFilesListBox, &QListWidget::currentRowChanged, this, [this]() { ExtraFilesListBoxClick(ui.ExtraFilesListBox); });
    connect(ui.ExtraFilesListBox, &QListWidget::itemDoubleClicked, this, [this]() { ExtraFilesListBoxDblClick(ui.ExtraFilesListBox); });
    ui.ExtraDirsListBox->installEventFilter(this);
    ui.ExtraFilesListBox->installEventFilter(this);
}

TModernProfileEditorDirectoryFrame::~TModernProfileEditorDirectoryFrame()
{
    delete OpenDialog;
}

bool TModernProfileEditorDirectoryFrame::eventFilter(QObject* O, QEvent* E)
{
    if (E->type() == QEvent::KeyPress && (O == ui.ExtraDirsListBox || O == ui.ExtraFilesListBox)) {
        auto* KE = static_cast<QKeyEvent*>(E);
        int Key = KE->key();
        TShiftState Shift = KE->modifiers() & ~Qt::KeypadModifier;
        if (Shift == Qt::NoModifier && (Key == Qt::Key_Insert || Key == Qt::Key_Return || Key == Qt::Key_Enter || Key == Qt::Key_Delete)) {
            if (Key == Qt::Key_Enter) Key = Qt::Key_Return;
            if (O == ui.ExtraDirsListBox) ExtraDirsListBoxKeyDown(O, Key, Shift); else ExtraFilesListBoxKeyDown(O, Key, Shift);
            return true;
        }
    }
    return QWidget::eventFilter(O, E);
}

void TModernProfileEditorDirectoryFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.OnShowFrame = [this](QObject* Sender) { ShowFrame(Sender); };
    InitData.AllowDefaultValueReset = false;

    // NoFlicker(...): not needed with Qt

    FCurrentProfileName = InitData.CurrentProfileName;
    FCurrentCaptureDir = InitData.CurrentCaptureDir;

    ui.ScreenshotFolderEdit_EditLabel->setText(LanguageSetup->ProfileEditorCaptureFolder());
    ui.ScreenshotFolderEditButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.GenerateScreenshotFolderNameButton->setText(LanguageSetup->ProfileEditorGenerateScreenshotFolderName());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.ScreenshotFolderEditButton);
    UserIconLoader->DialogImage(DI_Screenshot, ui.GenerateScreenshotFolderNameButton);

    ui.DataFolderEdit_EditLabel->setText(LanguageSetup->GameDataDir());
    ui.DataFolderEditButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.DataFolderInfoLabel->setText(LanguageSetup->GameDataDirEditInfo());
    ui.GenerateGameDataFolderNameButton->setText(LanguageSetup->ProfileEditorGenerateGameDataFolder());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DataFolderEditButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.GenerateGameDataFolderNameButton);

    ui.ExtraFilesLabel->setText(LanguageSetup->ProfileEditorExtraFiles());
    ui.ExtraFilesAddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.ExtraFilesEditButton->setToolTip(RemoveUnderline(LanguageSetup->Edit()));
    ui.ExtraFilesDelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    UserIconLoader->DialogImage(DI_Add, ui.ExtraFilesAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.ExtraFilesEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ExtraFilesDelButton);

    ui.ExtraDirsLabel->setText(LanguageSetup->ProfileEditorExtraDirs());
    ui.ExtraDirsAddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.ExtraDirsEditButton->setToolTip(RemoveUnderline(LanguageSetup->Edit()));
    ui.ExtraDirsDelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.ExtraDirsInfoLabel->setText(LanguageSetup->ProfileEditorExtraDirsEditInfo());
    UserIconLoader->DialogImage(DI_Add, ui.ExtraDirsAddButton);
    UserIconLoader->DialogImage(DI_Edit, ui.ExtraDirsEditButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ExtraDirsDelButton);

    OpenDialog->Title = LanguageSetup->ProfileEditorExtraFilesCaption();
    OpenDialog->Filter = LanguageSetup->ProfileEditorExtraFilesFilter();

    FLastCurrentProfileName = "";

    setProperty("HelpContext", ID_ProfileEditProgramDirectories);  // HelpContext
}

void TModernProfileEditorDirectoryFrame::ScreenshotFolderEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (FCurrentCaptureDir == nullptr) return;  // OnChange before InitGUI
    *FCurrentCaptureDir = ui.ScreenshotFolderEdit->text();
}

void TModernProfileEditorDirectoryFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    TStringList* St;
    int I;

    if (Trim(Game->CaptureFolder()) != "")
        ui.ScreenshotFolderEdit->setText(Game->CaptureFolder());
    else
        ui.ScreenshotFolderEdit->setText(IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()));
    if (LoadFromTemplate && PrgSetup->AlwaysSetScreenshotFolderAutomatically()) Timer->start();

    FLastCurrentProfileName = *FCurrentProfileName;

    ui.DataFolderEdit->setText(Game->DataDir());

    St = ValueToList(Game->ExtraFiles());
    I = 0; while (I < St->Count()) if (Trim((*St)[I]) == "") St->Delete(I); else I++;
    ui.ExtraFilesListBox->addItems(St->ToQStringList());
    delete St;
    if (ui.ExtraFilesListBox->count() > 0) ui.ExtraFilesListBox->setCurrentRow(0);
    ExtraFilesListBoxClick(nullptr);
    St = ValueToList(Game->ExtraDirs());
    I = 0; while (I < St->Count()) if (Trim((*St)[I]) == "") St->Delete(I); else I++;
    ui.ExtraDirsListBox->addItems(St->ToQStringList());
    delete St;
    if (ui.ExtraDirsListBox->count() > 0) ui.ExtraDirsListBox->setCurrentRow(0);
    ExtraDirsListBoxClick(nullptr);

    IsWindowsMode = WindowsExeMode(Game);
}

void TModernProfileEditorDirectoryFrame::ShowFrame(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (IsWindowsMode) {
        ui.ExtraFilesLabel->setVisible(false);
        ui.ExtraFilesLabel->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraFilesListBox->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraFilesAddButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraFilesEditButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraFilesDelButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsLabel->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsLabel->setText(LanguageSetup->ProfileEditorExtraDirsWindowsProfile());
        ui.ExtraDirsListBox->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsAddButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsEditButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsDelButton->setVisible(PrgSetup->AllowPackingWindowsGames());
        ui.ExtraDirsInfoLabel->setVisible(false);
    }
}

void TModernProfileEditorDirectoryFrame::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, DefaultFolder, OldFolder;

    if (FLastCurrentProfileName == *FCurrentProfileName) return;

    S = Trim(ExtUpperCase(MakeRelPath(ui.ScreenshotFolderEdit->text(), PrgSetup->BaseDir())));

    DefaultFolder = Trim(ExtUpperCase(MakeRelPath(IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()), PrgSetup->BaseDir())));
    OldFolder = Trim(ExtUpperCase(MakeRelPath(IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(FLastCurrentProfileName) + PathDelim, PrgSetup->BaseDir())));

    if (S == DefaultFolder || S == OldFolder) {
        ui.ScreenshotFolderEdit->setText(MakeRelPath(IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(*FCurrentProfileName) + PathDelim, PrgSetup->BaseDir()));
        FLastCurrentProfileName = *FCurrentProfileName;
    } else {
        /* Debug: ShowMessage(S+#13+DefaultFolder+#13+OldFolder); */
        Timer->stop();
    }
}

void TModernProfileEditorDirectoryFrame::GetGame(TGame* Game)
{
    int I;

    if (Timer->isActive()) {
        Timer->stop();
        TimerTimer(Timer);
    }

    Game->SetCaptureFolder(MakeRelPath(ui.ScreenshotFolderEdit->text(), PrgSetup->BaseDir()));
    Game->SetDataDir(MakeRelPath(ui.DataFolderEdit->text(), PrgSetup->BaseDir()));
    I = 0; while (I < ui.ExtraFilesListBox->count()) if (Trim(ui.ExtraFilesListBox->item(I)->text()) == "") delete ui.ExtraFilesListBox->takeItem(I); else I++;
    {
        TStringList St = ListItems(ui.ExtraFilesListBox);
        Game->SetExtraFiles(ListToValue(&St));
    }
    I = 0; while (I < ui.ExtraDirsListBox->count()) if (Trim(ui.ExtraDirsListBox->item(I)->text()) == "") delete ui.ExtraDirsListBox->takeItem(I); else I++;
    {
        TStringList St = ListItems(ui.ExtraDirsListBox);
        Game->SetExtraDirs(ListToValue(&St));
    }
}

void TModernProfileEditorDirectoryFrame::ExtraDirsListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ExtraDirsEditButton->setEnabled(ui.ExtraDirsListBox->currentRow() >= 0);
    ui.ExtraDirsDelButton->setEnabled(ui.ExtraDirsListBox->currentRow() >= 0);
}

void TModernProfileEditorDirectoryFrame::ExtraDirsListBoxDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.ExtraDirsEditButton);
}

void TModernProfileEditorDirectoryFrame::ExtraDirsListBoxKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case Qt::Key_Insert: ButtonWork(ui.ExtraDirsAddButton); break;
    case Qt::Key_Return: ButtonWork(ui.ExtraDirsEditButton); break;
    case Qt::Key_Delete: ButtonWork(ui.ExtraDirsDelButton); break;
    }
}

void TModernProfileEditorDirectoryFrame::ExtraFilesListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ExtraFilesEditButton->setEnabled(ui.ExtraFilesListBox->currentRow() >= 0);
    ui.ExtraFilesDelButton->setEnabled(ui.ExtraFilesListBox->currentRow() >= 0);
}

void TModernProfileEditorDirectoryFrame::ExtraFilesListBoxDblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ButtonWork(ui.ExtraFilesEditButton);
}

void TModernProfileEditorDirectoryFrame::ExtraFilesListBoxKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if (Shift != Qt::NoModifier) return;
    switch (Key) {
    case Qt::Key_Insert: ButtonWork(ui.ExtraFilesAddButton); break;
    case Qt::Key_Return: ButtonWork(ui.ExtraFilesEditButton); break;
    case Qt::Key_Delete: ButtonWork(ui.ExtraFilesDelButton); break;
    }
}

void TModernProfileEditorDirectoryFrame::ButtonWork(QObject* Sender)
{
    QString S;
    int I;

    switch (Sender->property("tag").toInt()) {
    case 0:
        S = PrgSetup->BaseDir();
        if (Trim(ui.ScreenshotFolderEdit->text()) == "")
            S = MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir());
        else
            S = MakeAbsPath(IncludeTrailingPathDelimiter(ui.ScreenshotFolderEdit->text()), S);
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        if (S == "") return;
        ui.ScreenshotFolderEdit->setText(IncludeTrailingPathDelimiter(S));
        break;
    case 1:
        if (PrgSetup->DataDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->DataDir();
        if (Trim(ui.DataFolderEdit->text()) != "")
            S = MakeAbsPath(ui.DataFolderEdit->text(), S);
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        if (S == "") return;
        ui.DataFolderEdit->setText(S);
        break;
    case 2:
        if (Trim(PrgSetup->GameDir()) == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
        OpenDialog->InitialDir = S;
        if (!OpenDialog->Execute()) return;
        S = OpenDialog->FileName;
        ui.ExtraFilesListBox->addItem(MakeRelPath(S, PrgSetup->BaseDir()));
        ui.ExtraFilesListBox->setCurrentRow(ui.ExtraFilesListBox->count() - 1);
        ExtraFilesListBoxClick(Sender);
        break;
    case 3:
        if (ui.ExtraFilesListBox->currentRow() < 0) return;
        S = MakeAbsPath(ui.ExtraFilesListBox->currentItem()->text(), PrgSetup->BaseDir());
        OpenDialog->InitialDir = ExtractFilePath(S);
        if (!OpenDialog->Execute()) return;
        S = OpenDialog->FileName;
        ui.ExtraFilesListBox->currentItem()->setText(MakeRelPath(S, PrgSetup->BaseDir()));
        break;
    case 4:
        I = ui.ExtraFilesListBox->currentRow();
        if (I < 0) return;
        delete ui.ExtraFilesListBox->takeItem(I);
        if (ui.ExtraFilesListBox->count() > 0) ui.ExtraFilesListBox->setCurrentRow(Max(0, I - 1));
        ExtraFilesListBoxClick(Sender);
        break;
    case 5:
        if (Trim(PrgSetup->GameDir()) == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        ui.ExtraDirsListBox->addItem(MakeRelPath(S, PrgSetup->BaseDir(), true));
        ui.ExtraDirsListBox->setCurrentRow(ui.ExtraDirsListBox->count() - 1);
        ExtraDirsListBoxClick(Sender);
        break;
    case 6:
        if (ui.ExtraDirsListBox->currentRow() >= 0) {
            S = MakeAbsPath(ui.ExtraDirsListBox->currentItem()->text(), PrgSetup->BaseDir());
            if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
            ui.ExtraDirsListBox->currentItem()->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
        }
        break;
    case 7:
        I = ui.ExtraDirsListBox->currentRow();
        if (I < 0) return;
        delete ui.ExtraDirsListBox->takeItem(I);
        if (ui.ExtraDirsListBox->count() > 0) ui.ExtraDirsListBox->setCurrentRow(Max(0, I - 1));
        ExtraDirsListBoxClick(Sender);
        break;
    }
}

void TModernProfileEditorDirectoryFrame::GenerateScreenshotFolderNameButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    QString S;

    if (Trim(ui.ScreenshotFolderEdit->text()) != "" && DirectoryExists(MakeAbsPath(ui.ScreenshotFolderEdit->text(), PrgSetup->BaseDir()))) return;

    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(*FCurrentProfileName) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(*FCurrentProfileName) + IntToStr(I) + PathDelim;
    }
    ui.ScreenshotFolderEdit->setText(S);
}

void TModernProfileEditorDirectoryFrame::GenerateGameDataFolderNameButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, T;
    int I;

    if (Trim(ui.DataFolderEdit->text()) != "" && DirectoryExists(MakeAbsPath(ui.DataFolderEdit->text(), PrgSetup->BaseDir()))) return;

    if (PrgSetup->DataDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->DataDir();

    T = MakeRelPath(IncludeTrailingPathDelimiter(S) + MakeFileSysOKFolderName(*FCurrentProfileName) + PathDelim, PrgSetup->BaseDir(), true);

    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(T, PrgSetup->BaseDir()))) {
        I++;
        T = MakeRelPath(IncludeTrailingPathDelimiter(S) + MakeFileSysOKFolderName(*FCurrentProfileName) + IntToStr(I) + PathDelim, PrgSetup->BaseDir(), true);
    }
    ui.DataFolderEdit->setText(T);

    S = MakeAbsPath(T, PrgSetup->BaseDir());
    if (MessageDlg(Format(LanguageSetup->MessageConfirmationCreateDir(), {S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
    ForceDirectories(S);
}
