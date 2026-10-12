// C++ port of ModernProfileEditorStartFrameUnit.pas (implementation section).
#include "ModernProfileEditorStartFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

// TStringGrid helpers: VCL row/column indices include the fixed rows/columns,
// which are the Qt header sections (see PORTING.md).
static int GridFixedRows(QTableWidget* T) { return T->property("vclFixedRows").toInt(); }
static int GridFixedCols(QTableWidget* T) { return T->property("vclFixedCols").toInt(); }

static QString GridCell(QTableWidget* T, int C, int R)  // Cells[C,R]
{
    const int FR = GridFixedRows(T), FC = GridFixedCols(T);
    if (R < FR) { QTableWidgetItem* I = T->horizontalHeaderItem(C - FC); return I ? I->text() : QString(); }
    QTableWidgetItem* I = T->item(R - FR, C - FC);
    return I ? I->text() : QString();
}

static void SetGridCell(QTableWidget* T, int C, int R, const QString& S)  // Cells[C,R]:=S
{
    const int FR = GridFixedRows(T), FC = GridFixedCols(T);
    if (R < FR) { T->setHorizontalHeaderItem(C - FC, new QTableWidgetItem(S)); return; }
    QTableWidgetItem* I = T->item(R - FR, C - FC);
    if (I == nullptr) T->setItem(R - FR, C - FC, new QTableWidgetItem(S)); else I->setText(S);
}

static int GridRowCount(QTableWidget* T) { return T->rowCount() + GridFixedRows(T); }
static void SetGridRowCount(QTableWidget* T, int N) { T->setRowCount(std::max(0, N - GridFixedRows(T))); }
static int GridRow(QTableWidget* T) { return T->currentRow() + GridFixedRows(T); }
static void SetGridRow(QTableWidget* T, int R) { T->setCurrentCell(R - GridFixedRows(T), std::max(0, T->currentColumn())); }

// Memo.Lines
static TStringList* MemoLines(QTextEdit* Memo)
{
    TStringList* St = new TStringList();
    St->SetText(Memo->toPlainText());
    return St;
}

static void SetMemoLines(QTextEdit* Memo, TStringList* St)
{
    Memo->setPlainText(St->ToQStringList().join('\n'));
}

TModernProfileEditorStartFrame::TModernProfileEditorStartFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM: OpenDialog: TOpenDialog, SaveDialog: TSaveDialog
    OpenDialog = new TOpenDialog(this);
    SaveDialog = new TSaveDialog(this);

    // DFM wiring
    for (QToolButton* B : {ui.AutoexecBootFloppyImageButton, ui.AutoexecBootFloppyImageAddButton, ui.AutoexecBootFloppyImageDelButton})
        connect(B, &QToolButton::clicked, this, [this, B]() { ButtonWork(B); });
    for (QPushButton* B : {ui.AutoexecClearButton, ui.AutoexecLoadButton, ui.AutoexecSaveButton, ui.FinalizationClearButton, ui.FinalizationLoadButton, ui.FinalizationSaveButton})
        connect(B, &QPushButton::clicked, this, [this, B]() { ButtonWork(B); });
    connect(ui.AutoexecBootHDImageComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { AutoexecBootHDImageComboBoxChange(ui.AutoexecBootHDImageComboBox); });
}

TModernProfileEditorStartFrame::~TModernProfileEditorStartFrame()
{
    delete OpenDialog;
    delete SaveDialog;
}

// { TModernProfileEditorStartFrame }

void TModernProfileEditorStartFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);
    QString S;
    QFont F;

    SetRichEditPopup(ui.AutoexecMemo);
    SetRichEditPopup(ui.FinalizationMemo);

    ui.AutoexecOverrideGameStartCheckBox->setText(LanguageSetup->ProfileEditorAutoexecOverrideGameStart());
    ui.AutoexecOverrideMountingCheckBox->setText(LanguageSetup->ProfileEditorAutoexecOverrideMounting());
    S = LanguageSetup->ProfileEditorAutoexecBat();
    if ((!S.isEmpty()) && (S[S.length() - 1] == QChar(':'))) S = Trim(Copy(S, 1, S.length() - 1));
    ui.AutoexecPageControl->setTabText(ui.AutoexecPageControl->indexOf(ui.AutoexecSheet1), S);
    F = ui.AutoexecMemo->font(); F.setFamily("Courier New"); F.setStyleHint(QFont::Monospace); ui.AutoexecMemo->setFont(F);
    ui.AutoexecClearButton->setText(LanguageSetup->Del());
    ui.AutoexecLoadButton->setText(LanguageSetup->Load());
    ui.AutoexecSaveButton->setText(LanguageSetup->Save());
    ui.AutoexecPageControl->setTabText(ui.AutoexecPageControl->indexOf(ui.AutoexecSheet2), LanguageSetup->ProfileEditorFinalization());
    F = ui.FinalizationMemo->font(); F.setFamily("Courier New"); F.setStyleHint(QFont::Monospace); ui.FinalizationMemo->setFont(F);
    ui.FinalizationClearButton->setText(LanguageSetup->Del());
    ui.FinalizationLoadButton->setText(LanguageSetup->Load());
    ui.FinalizationSaveButton->setText(LanguageSetup->Save());
    ui.AutoexecBootNormal->setText(LanguageSetup->ProfileEditorAutoexecBootNormal());
    ui.AutoexecBootHDImage->setText(LanguageSetup->ProfileEditorAutoexecBootHDImage());
    ui.AutoexecBootFloppyImage->setText(LanguageSetup->ProfileEditorAutoexecBootFloppyImage());
    ui.AutoexecBootFloppyImageInfoLabel->setText(LanguageSetup->ProfileMountingSwitchImage());
    ui.AutoexecBootFloppyImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.AutoexecBootFloppyImageAddButton->setToolTip(LanguageSetup->ProfileMountingAddImage());
    ui.AutoexecBootFloppyImageDelButton->setToolTip(LanguageSetup->ProfileMountingDelImage());

    UserIconLoader->DialogImage(DI_Clear, ui.AutoexecClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.AutoexecLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.AutoexecSaveButton);
    UserIconLoader->DialogImage(DI_Clear, ui.FinalizationClearButton);
    UserIconLoader->DialogImage(DI_Load, ui.FinalizationLoadButton);
    UserIconLoader->DialogImage(DI_Save, ui.FinalizationSaveButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.AutoexecBootFloppyImageButton);
    UserIconLoader->DialogImage(DI_Add, ui.AutoexecBootFloppyImageAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.AutoexecBootFloppyImageDelButton);

    ui.AutoexecBootFloppyImageTab->setColumnWidth(0, ui.AutoexecBootFloppyImageTab->viewport()->width() - 25);

    setProperty("HelpContext", ID_ProfileEditStarting);  // HelpContext:=...
}

void TModernProfileEditorStartFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    TStringList *St, *St2;
    QString S;
    int I;

    ui.AutoexecOverrideGameStartCheckBox->setChecked(Game->AutoexecOverridegamestart());
    ui.AutoexecOverrideMountingCheckBox->setChecked(Game->AutoexecOverrideMount());
    St = StringToStringList(Game->Autoexec()); SetMemoLines(ui.AutoexecMemo, St); delete St;
    St = StringToStringList(Game->AutoexecFinalization()); SetMemoLines(ui.FinalizationMemo, St); delete St;
    S = Trim(Game->AutoexecBootImage());
    if ((S == "2") || (S == "3")) {
        ui.AutoexecBootHDImage->setChecked(true);
        if (S == "2") ui.AutoexecBootHDImageComboBox->setCurrentIndex(0); else ui.AutoexecBootHDImageComboBox->setCurrentIndex(1);
    } else if (!S.isEmpty()) {
        ui.AutoexecBootFloppyImage->setChecked(true);
        St2 = new TStringList();
        S = Trim(S);
        I = Pos("$", S);
        while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos("$", S); }
        St2->Add(S);
        SetGridRowCount(ui.AutoexecBootFloppyImageTab, St2->Count());
        for (I = 0; I < St2->Count(); I++) SetGridCell(ui.AutoexecBootFloppyImageTab, 0, I, (*St2)[I]);
        delete St2;
    }
}

void TModernProfileEditorStartFrame::AutoexecBootHDImageComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.AutoexecBootHDImage->setChecked(true);
}

void TModernProfileEditorStartFrame::ButtonWork(QObject* Sender)
{
    int I;
    QString S;
    TStringList* St;
    QTableWidget* Tab = ui.AutoexecBootFloppyImageTab;

    switch (Sender->property("tag").toInt()) {
        case 0: ui.AutoexecMemo->clear(); break;
        case 1:
            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorAutoexecLoadTitle();
            OpenDialog->InitialDir = PrgDataDir();
            if (!OpenDialog->Execute()) return;
            St = new TStringList();
            if (St->LoadFromFile(OpenDialog->FileName)) SetMemoLines(ui.AutoexecMemo, St);
            else MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            delete St;
            break;
        case 2:
            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorAutoexecSaveTitle();
            SaveDialog->InitialDir = PrgDataDir();
            if (!SaveDialog->Execute()) return;
            St = MemoLines(ui.AutoexecMemo);
            if (!St->SaveToFile(SaveDialog->FileName))
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            delete St;
            break;
        case 3:
            if (GridRow(Tab) < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            S = MakeAbsPath(GridCell(Tab, 0, GridRow(Tab)), PrgSetup->BaseDir());
            OpenDialog->DefaultExt = "img";
            OpenDialog->InitialDir = ExtractFilePath(S);
            OpenDialog->Title = LanguageSetup->ProfileMountingFile();
            OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
            if (!OpenDialog->Execute()) return;
            SetGridCell(Tab, 0, GridRow(Tab), MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
            ui.AutoexecBootFloppyImage->setChecked(true);
            break;
        case 4:
            SetGridRowCount(Tab, GridRowCount(Tab) + 1);
            SetGridRow(Tab, GridRowCount(Tab) - 1);
            break;
        case 5:
            if (GridRow(Tab) < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            if (GridRowCount(Tab) == 1) {
                SetGridCell(Tab, 0, 0, "");
            } else {
                for (I = GridRow(Tab) + 1; I < GridRowCount(Tab); I++) SetGridCell(Tab, 0, I - 1, GridCell(Tab, 0, I));
                SetGridRowCount(Tab, GridRowCount(Tab) - 1);
            }
            break;
        case 6: ui.FinalizationMemo->clear(); break;
        case 7:
            OpenDialog->DefaultExt = "txt";
            OpenDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            OpenDialog->Title = LanguageSetup->ProfileEditorFinalizationLoadTitle();
            OpenDialog->InitialDir = PrgDataDir();
            if (!OpenDialog->Execute()) return;
            St = new TStringList();
            if (St->LoadFromFile(OpenDialog->FileName)) SetMemoLines(ui.FinalizationMemo, St);
            else MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0);
            delete St;
            break;
        case 8:
            SaveDialog->DefaultExt = "txt";
            SaveDialog->Filter = LanguageSetup->ProfileEditorAutoexecFilter();
            SaveDialog->Title = LanguageSetup->ProfileEditorFinalizationSaveTitle();
            SaveDialog->InitialDir = PrgDataDir();
            if (!SaveDialog->Execute()) return;
            St = MemoLines(ui.FinalizationMemo);
            if (!St->SaveToFile(SaveDialog->FileName))
                MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0);
            delete St;
            break;
    }
}

void TModernProfileEditorStartFrame::GetGame(TGame* Game)
{
    QString S;
    TStringList* St;
    QTableWidget* Tab = ui.AutoexecBootFloppyImageTab;

    Game->SetAutoexecOverridegamestart(ui.AutoexecOverrideGameStartCheckBox->isChecked());
    Game->SetAutoexecOverrideMount(ui.AutoexecOverrideMountingCheckBox->isChecked());
    St = MemoLines(ui.AutoexecMemo); Game->SetAutoexec(StringListToString(St)); delete St;
    St = MemoLines(ui.FinalizationMemo); Game->SetAutoexecFinalization(StringListToString(St)); delete St;
    if (ui.AutoexecBootNormal->isChecked()) Game->SetAutoexecBootImage("");
    if (ui.AutoexecBootHDImage->isChecked()) {
        if (ui.AutoexecBootHDImageComboBox->currentIndex() == 0) Game->SetAutoexecBootImage("2"); else Game->SetAutoexecBootImage("3");
    }
    if (ui.AutoexecBootFloppyImage->isChecked()) {
        S = GridCell(Tab, 0, 0);
        for (int I = 1; I < GridRowCount(Tab); I++) S = S + "$" + GridCell(Tab, 0, I);
        Game->SetAutoexecBootImage(S);
    }
}
