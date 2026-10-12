// C++ port of CheatApplyFormUnit.pas (implementation section).
#include "CheatApplyFormUnit.h"

#include "PrgConsts.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "CheatDBToolsUnit.h"
#include "Process.h"

TCheatApplyForm* CheatApplyForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

// Application.MainForm
static QWidget* ApplicationMainForm()
{
    for (QWidget* W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow*>(W) != nullptr) return W;
    return QApplication::activeWindow();
}

static QListWidgetItem* AddCheckItem(QListWidget* ListBox, const QString& S)
{
    QListWidgetItem* It = new QListWidgetItem(S, ListBox);
    It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
    It->setCheckState(Qt::Unchecked);
    return It;
}

/* TCheatApplyForm */

TCheatApplyForm::TCheatApplyForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.FilesListBox, &QListWidget::itemClicked, this, [this]() { FilesListBoxClick(ui.FilesListBox); });
    connect(ui.FilesListBox, &QListWidget::currentRowChanged, this, [this]() { FilesListBoxClick(ui.FilesListBox); });
    connect(ui.GameComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { GameComboBoxChange(ui.GameComboBox); });
    connect(ui.SelectButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectButton); });
    connect(ui.EditButton, &QPushButton::clicked, this, [this]() { EditButtonClick(ui.EditButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TCheatApplyForm::~TCheatApplyForm()
{
    FormDestroy(this);  // OnDestroy
}

void TCheatApplyForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TCheatApplyForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    EditButtonClicked = false;
    JustGameComboBoxChanging = false;
    GameFolders = nullptr;
    CheatDB = SmartLoadCheatsDB();

    setWindowTitle(LanguageSetup->ApplyCheat());
    ui.InfoLabel->setText(LanguageSetup->ApplyCheatInfo());
    ui.GameLabel->setText(LanguageSetup->ApplyCheatGamesList());
    ui.CheatsLabel->setText(LanguageSetup->ApplyCheatCheatsList());
    ui.FilesLabel->setText(LanguageSetup->ApplyCheatFilesList());
    ui.SelectButton->setText(LanguageSetup->ApplyCheatFilesListSelectLastChanged());
    ui.WarningLabel->setText(LanguageSetup->ApplyCheatWarning());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.EditButton->setText(LanguageSetup->ApplyCheatEditButton());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_Edit, ui.EditButton);
}

void TCheatApplyForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete CheatDB; CheatDB = nullptr;
    delete GameFolders; GameFolders = nullptr;
}

void TCheatApplyForm::FormShow(QObject* Sender)
{
    TStringList* MatchingGameRecords;
    int I;

    if (CheatDB == nullptr) {
        MessageDlg(LanguageSetup->ApplyCheatDataBaseError(), mtError, mbOK, 0, this);
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);   // PostMessage(Handle,WM_Close,0,0)
        return;
    }

    delete GameFolders;
    GameFolders = GetGameFolders(GameDir);
    MatchingGameRecords = FindMatchingGameRecords(CheatDB, GameFolders);
    if (MatchingGameRecords->Count() == 0) {
        MessageDlg(LanguageSetup->ApplyCheatNoMatchingCheatRecord(), mtError, mbOK, 0, this);
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        delete MatchingGameRecords;
        return;
    }

    JustGameComboBoxChanging = true;
    ui.GameComboBox->clear();
    for (I = 0; I < MatchingGameRecords->Count(); I++) ui.GameComboBox->addItem((*MatchingGameRecords)[I], QVariant::fromValue<void*>(MatchingGameRecords->Objects(I)));
    if (ui.GameComboBox->count() > 0) ui.GameComboBox->setCurrentIndex(0);
    JustGameComboBoxChanging = false;
    GameComboBoxChange(Sender);
    delete MatchingGameRecords;
}

void TCheatApplyForm::GameComboBoxChange(QObject* Sender)
{
    int I, J, K;
    TCheatGameRecord* G;
    TSearchRec Rec;
    int MostRecentTime;
    QString S;

    if (JustGameComboBoxChanging) return;

    if (ui.GameComboBox->currentIndex() < 0) return;
    G = static_cast<TCheatGameRecord*>(ui.GameComboBox->itemData(ui.GameComboBox->currentIndex()).value<void*>());

    TStringList FileMasks;
    ui.CheatsListBox->clear();
    for (I = 0; I < G->Count(); I++) {
        QListWidgetItem* It = AddCheckItem(ui.CheatsListBox, G->Action(I)->Name());
        It->setData(Qt::UserRole, QVariant::fromValue<void*>(G->Action(I)));
        It->setCheckState(Qt::Checked);
        S = ExtUpperCase(G->Action(I)->FileMask());
        if (FileMasks.IndexOf(S) < 0) FileMasks.Add(S);
    }

    MostRecentNr = -1; MostRecentTime = -1;
    {
        const QSignalBlocker Block(ui.FilesListBox);
        ui.FilesListBox->clear();
        for (I = 0; I < GameFolders->Count(); I++) for (J = 0; J < FileMasks.Count(); J++) {
            K = FindFirst((*GameFolders)[I] + FileMasks[J], faAnyFile, Rec);
            while (K == 0) {
                if ((Rec.Attr & faDirectory) == 0) {
                    S = MakeRelPath((*GameFolders)[I] + Rec.Name, PrgSetup->BaseDir());
                    bool Found = false;
                    for (int L = 0; L < ui.FilesListBox->count(); L++) if (SameText(ui.FilesListBox->item(L)->text(), S)) { Found = true; break; }
                    if (!Found) {
                        AddCheckItem(ui.FilesListBox, S);
                        if (Rec.Time > MostRecentTime) { MostRecentTime = Rec.Time; MostRecentNr = ui.FilesListBox->count() - 1; }
                    }
                }
                K = FindNext(Rec);
            }
            FindClose(Rec);
        }
    }
    SelectButtonClick(Sender);
}

void TCheatApplyForm::SelectButtonClick(QObject* Sender)
{
    int I;
    {
        const QSignalBlocker Block(ui.FilesListBox);
        for (I = 0; I < ui.FilesListBox->count(); I++) ui.FilesListBox->item(I)->setCheckState((I == MostRecentNr) ? Qt::Checked : Qt::Unchecked);
        ui.FilesListBox->setCurrentRow(MostRecentNr);
    }
    if (ui.FilesListBox->currentItem() != nullptr) ui.FilesListBox->scrollToItem(ui.FilesListBox->currentItem(), QAbstractItemView::PositionAtTop);
    FilesListBoxClick(Sender);
}

void TCheatApplyForm::FilesListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    bool B;

    B = false;
    const QSignalBlocker Block(ui.FilesListBox);
    for (I = 0; I < ui.FilesListBox->count(); I++) {
        ui.FilesListBox->item(I)->setCheckState((ui.FilesListBox->currentRow() == I) ? Qt::Checked : Qt::Unchecked);
        if (ui.FilesListBox->item(I)->checkState() == Qt::Checked) B = true;
    }
    ui.OKButton->setEnabled(B);
}

void TCheatApplyForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    TCheatAction* A;
    QString FileName;
    bool B;

    if (ui.FilesListBox->currentRow() < 0) return;
    FileName = MakeAbsPath(ui.FilesListBox->item(ui.FilesListBox->currentRow())->text(), PrgSetup->BaseDir());

    TStringList Backuped;
    for (I = 0; I < ui.CheatsListBox->count(); I++) if (ui.CheatsListBox->item(I)->checkState() == Qt::Checked) {
        A = static_cast<TCheatAction*>(ui.CheatsListBox->item(I)->data(Qt::UserRole).value<void*>());
        if (!FileMatchingMask(ExtractFileName(FileName), A->FileMask())) continue;
        B = (Backuped.IndexOf(FileName) > 0);   // sic (original code)
        if (A->Apply(FileName, !B)) {
            if (!B) Backuped.Add(FileName);
        }
    }
}

void TCheatApplyForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ProfileCheating);
}

void TCheatApplyForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

void TCheatApplyForm::EditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    EditButtonClicked = true;
    setModalResult(mrOk);
}

/* global */

bool ShowCheatApplyDialog(QWidget* AOwner, const QString& GameDir, bool& EditButtonClicked)
{
    bool result;
    CheatsDBUpdateCheckIfSetup(ApplicationMainForm());
    CheatApplyForm = new TCheatApplyForm(AOwner);
    CheatApplyForm->GameDir = IncludeTrailingPathDelimiter(GameDir);
    result = (CheatApplyForm->ShowModal() == mrOk);
    if (result) EditButtonClicked = CheatApplyForm->EditButtonClicked; else EditButtonClicked = false;
    delete CheatApplyForm;
    CheatApplyForm = nullptr;
    return result;
}
