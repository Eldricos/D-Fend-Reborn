// C++ port of ExtraExeEditFormUnit.pas (implementation section).
#include "ExtraExeEditFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "Process.h"

TExtraExeEditForm* ExtraExeEditForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

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
static int GridRow(QTableWidget* T) { return T->currentRow() + GridFixedRows(T); }

TExtraExeEditForm::TExtraExeEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.Tab, &QTableWidget::currentCellChanged, this, [this]() { TabClick(ui.Tab); });  // OnClick (also fires on keyboard navigation in VCL)
    connect(ui.SelectButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectButton); });

    FormCreate(this);  // OnCreate
}

TExtraExeEditForm::~TExtraExeEditForm() = default;

void TExtraExeEditForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);  // OnShow
}

// { TExtraExeEditForm }

void TExtraExeEditForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=CharsetNameToFontCharSet(...) - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->ExtraExeEditCaption());
    SetGridCell(ui.Tab, 0, 0, LanguageSetup->ExtraExeEditDescription());
    SetGridCell(ui.Tab, 1, 0, LanguageSetup->ExtraExeEditFileName());
    SetGridCell(ui.Tab, 2, 0, LanguageSetup->ExtraExeEditParameters());
    ui.SelectButton->setText(LanguageSetup->ExtraExeEditFileNameSelect());
    ui.InfoLabel->setText(LanguageSetup->ExtraExeEditInfo());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    ui.Tab->setColumnWidth(0, (ui.Tab->viewport()->width() - 10) * 2 / 7);
    ui.Tab->setColumnWidth(1, (ui.Tab->viewport()->width() - 10) * 4 / 7);
    ui.Tab->setColumnWidth(2, (ui.Tab->viewport()->width() - 10) * 1 / 7);

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton);

    WindowsMode = false;
    GameExe = "";
    SetupExe = "";
}

void TExtraExeEditForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J, K;
    QString S;

    J = 1;
    for (I = 0; I <= std::min(GridRowCount(ui.Tab) - 2, ExeFiles->Count() - 1); I++) {
        S = Trim((*ExeFiles)[I]);
        if (!S.isEmpty()) {
            K = Pos(";", S);
            if (K > 0) {
                SetGridCell(ui.Tab, 0, J, Copy(S, 1, K - 1));
                SetGridCell(ui.Tab, 1, J, Copy(S, K + 1, MaxInt));
                SetGridCell(ui.Tab, 2, J, (*Parameters)[I]);
                J++;
            }
        }
    }
}

void TExtraExeEditForm::TabClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.SelectButton->setEnabled(GridRow(ui.Tab) >= 1);
}

void TExtraExeEditForm::SelectButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (GridRow(ui.Tab) < 1) return;
    S = MakeAbsPath(GridCell(ui.Tab, 1, GridRow(ui.Tab)), PrgSetup->BaseDir());
    if (SelectProgramFile(S, GameExe, SetupExe, WindowsMode, -1, this)) SetGridCell(ui.Tab, 1, GridRow(ui.Tab), S);
}

void TExtraExeEditForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, T;

    ExeFiles->Clear();
    Parameters->Clear();
    for (int I = 1; I < GridRowCount(ui.Tab); I++) {
        S = Trim(GridCell(ui.Tab, 0, I));
        T = Trim(GridCell(ui.Tab, 1, I));
        if ((!S.isEmpty()) && (!T.isEmpty())) {
            ExeFiles->Add(S + ";" + T);
            Parameters->Add(GridCell(ui.Tab, 2, I));
        }
    }
}

void TExtraExeEditForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ProfileEditProfile);
}

void TExtraExeEditForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

// { global }

bool ShowExtraExeEditDialog(QWidget* AOwner, TStringList* AExeFiles, TStringList* AParameters, bool AWindowsMode, const QString& AGameExe, const QString& AGameSetup)
{
    bool result;
    ExtraExeEditForm = new TExtraExeEditForm(AOwner);
    ExtraExeEditForm->ExeFiles = AExeFiles;
    ExtraExeEditForm->Parameters = AParameters;
    ExtraExeEditForm->WindowsMode = AWindowsMode;
    ExtraExeEditForm->GameExe = AGameExe;
    ExtraExeEditForm->SetupExe = AGameSetup;
    result = (ExtraExeEditForm->ShowModal() == mrOk);
    delete ExtraExeEditForm;
    ExtraExeEditForm = nullptr;
    return result;
}
