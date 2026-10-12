// C++ port of ModernProfileEditorHelperProgramsFrameUnit.pas (implementation section).
#include "ModernProfileEditorHelperProgramsFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

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

TModernProfileEditorHelperProgramsFrame::TModernProfileEditorHelperProgramsFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.AddButton1, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton1); });
    connect(ui.DelButton1, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton1); });
    connect(ui.DelButton2, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton2); });
    connect(ui.AddButton2, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton2); });
    // OnSetEditText: called when the user edits a cell
    connect(ui.Command1Tab->itemDelegate(), &QAbstractItemDelegate::commitData, this, [this]() {
        const int C = ui.Command1Tab->currentColumn() + GridFixedCols(ui.Command1Tab), R = GridRow(ui.Command1Tab);
        Command1TabSetEditText(ui.Command1Tab, C, R, GridCell(ui.Command1Tab, C, R));
    });
    connect(ui.Command2Tab->itemDelegate(), &QAbstractItemDelegate::commitData, this, [this]() {
        const int C = ui.Command2Tab->currentColumn() + GridFixedCols(ui.Command2Tab), R = GridRow(ui.Command2Tab);
        Command2TabSetEditText(ui.Command2Tab, C, R, GridCell(ui.Command2Tab, C, R));
    });
}

TModernProfileEditorHelperProgramsFrame::~TModernProfileEditorHelperProgramsFrame() = default;

void TModernProfileEditorHelperProgramsFrame::resizeEvent(QResizeEvent* E)
{
    QWidget::resizeEvent(E);
    FrameResize(this);  // OnResize
}

// { TModernProfileEditorHelperProgramsFrame }

void TModernProfileEditorHelperProgramsFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.AllowDefaultValueReset = false;

    ui.Command1CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsRunBefore());
    SetGridCell(ui.Command1Tab, 0, 0, LanguageSetup->ProfileEditorHelperProgramsRunBeforeEditLabel());
    ui.RunMinimized1CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsRunMinimized());
    ui.InOrder1CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsInOrder());
    ui.Wait1CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsRunWait());
    ui.Command2CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsRunAfter());
    SetGridCell(ui.Command2Tab, 0, 0, LanguageSetup->ProfileEditorHelperProgramsRunAfterEditLabel());
    ui.RunMinimized2CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsRunMinimized());
    ui.InOrder2CheckBox->setText(LanguageSetup->ProfileEditorHelperProgramsInOrder());

    ui.AddButton1->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton1->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.AddButton2->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton2->setToolTip(RemoveUnderline(LanguageSetup->Del()));

    UserIconLoader->DialogImage(DI_Add, ui.AddButton1);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton1);
    UserIconLoader->DialogImage(DI_Add, ui.AddButton2);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton2);

    FrameResize(this);

    setProperty("HelpContext", ID_ProfileEditHelperPrograms);  // HelpContext:=...
}

void TModernProfileEditorHelperProgramsFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;
    TStringList* St;
    int I, J;
    bool B;

    B = false; J = 0;
    St = StringToStringList(Trim(Game->CommandBeforeExecution()));
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]); if (S.isEmpty()) continue;
        if (J == 0) SetGridCell(ui.Command1Tab, 0, 1, S); else {
            SetGridRowCount(ui.Command1Tab, J + 2); SetGridCell(ui.Command1Tab, 0, J + 1, S);
        }
        B = true; J++;
    }
    delete St;
    ui.Command1CheckBox->setChecked(B);

    ui.RunMinimized1CheckBox->setChecked(Game->CommandBeforeExecutionMinimized());
    ui.InOrder1CheckBox->setChecked(!Game->CommandBeforeExecutionParallel());
    ui.Wait1CheckBox->setChecked(Game->CommandBeforeExecutionWait());

    B = false; J = 0;
    St = StringToStringList(Trim(Game->CommandAfterExecution()));
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]); if (S.isEmpty()) continue;
        if (J == 0) SetGridCell(ui.Command2Tab, 0, 1, S); else {
            SetGridRowCount(ui.Command2Tab, J + 2); SetGridCell(ui.Command2Tab, 0, J + 1, S);
        }
        B = true; J++;
    }
    delete St;
    ui.Command2CheckBox->setChecked(B);

    ui.RunMinimized2CheckBox->setChecked(Game->CommandAfterExecutionMinimized());
    ui.InOrder2CheckBox->setChecked(!Game->CommandAfterExecutionParallel());
}

void TModernProfileEditorHelperProgramsFrame::FrameResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Command1Tab->setColumnWidth(0, ui.Command1Tab->viewport()->width() - 10);
    ui.Command2Tab->setColumnWidth(0, ui.Command2Tab->viewport()->width() - 10);
}

void TModernProfileEditorHelperProgramsFrame::Command1TabSetEditText(QObject* Sender, int ACol, int ARow, const QString& Value)
{
    Q_UNUSED(Sender); Q_UNUSED(ACol); Q_UNUSED(ARow); Q_UNUSED(Value);
    ui.Command1CheckBox->setChecked(true);
}

void TModernProfileEditorHelperProgramsFrame::Command2TabSetEditText(QObject* Sender, int ACol, int ARow, const QString& Value)
{
    Q_UNUSED(Sender); Q_UNUSED(ACol); Q_UNUSED(ARow); Q_UNUSED(Value);
    ui.Command2CheckBox->setChecked(true);
}

void TModernProfileEditorHelperProgramsFrame::ButtonWork(QObject* Sender)
{
    int I;
    QTableWidget* Command1Tab = ui.Command1Tab;
    QTableWidget* Command2Tab = ui.Command2Tab;

    switch (Sender->property("tag").toInt()) {
        case 0:
            SetGridRowCount(Command1Tab, GridRowCount(Command1Tab) + 1);
            SetGridRow(Command1Tab, GridRowCount(Command1Tab) - 1);
            break;
        case 1:
            if (GridRowCount(Command1Tab) > 2) {
                if (GridRow(Command1Tab) > 0) {
                    for (I = GridRow(Command1Tab) + 1; I < GridRowCount(Command1Tab); I++) SetGridCell(Command1Tab, 0, I - 1, GridCell(Command1Tab, 0, I));
                    SetGridRowCount(Command1Tab, GridRowCount(Command1Tab) - 1);
                } else {
                    SetGridRowCount(Command1Tab, GridRowCount(Command1Tab) - 1);
                    SetGridRow(Command1Tab, GridRowCount(Command1Tab) - 1);
                }
            } else SetGridCell(Command1Tab, 0, 1, "");
            break;
        case 2:
            SetGridRowCount(Command2Tab, GridRowCount(Command2Tab) + 1);
            SetGridRow(Command2Tab, GridRowCount(Command2Tab) - 1);
            break;
        case 3:
            // (the original code mixes Command1Tab and Command2Tab here - kept as is)
            if (GridRowCount(Command2Tab) > 2) {
                if (GridRow(Command1Tab) > 0) {
                    for (I = GridRow(Command2Tab) + 1; I < GridRowCount(Command2Tab); I++) SetGridCell(Command2Tab, 0, I - 1, GridCell(Command2Tab, 0, I));
                    SetGridRowCount(Command2Tab, GridRowCount(Command2Tab) - 1);
                } else {
                    SetGridRowCount(Command1Tab, GridRowCount(Command1Tab) - 1);
                    SetGridRow(Command1Tab, GridRowCount(Command1Tab) - 1);
                }
            } else SetGridCell(Command2Tab, 0, 1, "");
            break;
    }
}

void TModernProfileEditorHelperProgramsFrame::GetGame(TGame* Game)
{
    TStringList* St;
    int I;

    if (!ui.Command1CheckBox->isChecked()) Game->SetCommandBeforeExecution(""); else {
        St = new TStringList();
        for (I = 1; I < GridRowCount(ui.Command1Tab); I++) if (!Trim(GridCell(ui.Command1Tab, 0, I)).isEmpty()) St->Add(Trim(GridCell(ui.Command1Tab, 0, I)));
        Game->SetCommandBeforeExecution(StringListToString(St));
        delete St;
    }
    Game->SetCommandBeforeExecutionMinimized(ui.RunMinimized1CheckBox->isChecked());
    Game->SetCommandBeforeExecutionWait(ui.Wait1CheckBox->isChecked());
    Game->SetCommandBeforeExecutionParallel(!ui.InOrder1CheckBox->isChecked());

    if (!ui.Command2CheckBox->isChecked()) Game->SetCommandAfterExecution(""); else {
        St = new TStringList();
        for (I = 1; I < GridRowCount(ui.Command2Tab); I++) if (!Trim(GridCell(ui.Command2Tab, 0, I)).isEmpty()) St->Add(Trim(GridCell(ui.Command2Tab, 0, I)));
        Game->SetCommandAfterExecution(StringListToString(St));
        delete St;
    }
    Game->SetCommandAfterExecutionMinimized(ui.RunMinimized2CheckBox->isChecked());
    Game->SetCommandAfterExecutionParallel(!ui.InOrder2CheckBox->isChecked());
}
