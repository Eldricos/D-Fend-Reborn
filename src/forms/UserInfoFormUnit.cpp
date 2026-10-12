// C++ port of UserInfoFormUnit.pas (implementation section).
#include "UserInfoFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TUserInfoForm* UserInfoForm = nullptr;

// Replacement for TStringGridEx (ClassExtensions): cells for which OnListForCell
// returns true get an editable combo box (esPickList) filled by OnGetListForCell.
namespace {
class TStringGridExDelegate : public QStyledItemDelegate
{
public:
    TStringGridExDelegate(QObject* Parent, std::function<void(int, int, bool&)> AOnListForCell, std::function<void(int, int, TStringList*)> AOnGetListForCell)
        : QStyledItemDelegate(Parent), OnListForCell(AOnListForCell), OnGetListForCell(AOnGetListForCell) {}

    QWidget* createEditor(QWidget* Parent, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override
    {
        bool B = false;
        if (OnListForCell) OnListForCell(Index.column(), Index.row() + 1, B);
        if (!B) return QStyledItemDelegate::createEditor(Parent, Option, Index);
        QComboBox* Combo = new QComboBox(Parent);
        Combo->setEditable(true);
        Combo->setInsertPolicy(QComboBox::NoInsert);
        TStringList St;
        if (OnGetListForCell) OnGetListForCell(Index.column(), Index.row() + 1, &St);
        Combo->addItems(St.ToQStringList());
        return Combo;
    }
    void setEditorData(QWidget* Editor, const QModelIndex& Index) const override
    {
        if (QComboBox* Combo = qobject_cast<QComboBox*>(Editor)) { Combo->setCurrentText(Index.data(Qt::EditRole).toString()); return; }
        QStyledItemDelegate::setEditorData(Editor, Index);
    }
    void setModelData(QWidget* Editor, QAbstractItemModel* Model, const QModelIndex& Index) const override
    {
        if (QComboBox* Combo = qobject_cast<QComboBox*>(Editor)) { Model->setData(Index, Combo->currentText(), Qt::EditRole); return; }
        QStyledItemDelegate::setModelData(Editor, Model, Index);
    }

private:
    std::function<void(int, int, bool&)> OnListForCell;
    std::function<void(int, int, TStringList*)> OnGetListForCell;
};
}  // namespace

// TStringGrid "Tab" (ColCount=2, FixedCols=0, FixedRows=1): row 0 is the horizontal header.

QString TUserInfoForm::Cells(int ACol, int ARow) const
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        return (H != nullptr) ? H->text() : QString();
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    return (It != nullptr) ? It->text() : QString();
}

void TUserInfoForm::SetCells(int ACol, int ARow, const QString& Value)
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        if (H == nullptr) { H = new QTableWidgetItem; ui.Tab->setHorizontalHeaderItem(ACol, H); }
        H->setText(Value);
        return;
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    if (It == nullptr) { It = new QTableWidgetItem; ui.Tab->setItem(ARow - 1, ACol, It); }
    It->setText(Value);
}

int TUserInfoForm::RowCount() const
{
    return ui.Tab->rowCount() + 1;
}

void TUserInfoForm::SetRowCount(int ARowCount)
{
    ui.Tab->setRowCount(ARowCount - 1);
}

void TUserInfoForm::CommitTabEditor()
{
    QWidget* Editor = QApplication::focusWidget();
    while (Editor != nullptr && Editor->parentWidget() != ui.Tab->viewport() && Editor != ui.Tab) Editor = Editor->parentWidget();
    if (Editor != nullptr && Editor != ui.Tab)
        QMetaObject::invokeMethod(ui.Tab, "commitData", Q_ARG(QWidget*, Editor));
}

TUserInfoForm::TUserInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ui.Tab->setEditTriggers(QAbstractItemView::AllEditTriggers);   // goEditing, goAlwaysShowEditor
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { DelButtonClick(ui.DelButton); });
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { AddButtonClick(ui.AddButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TUserInfoForm::~TUserInfoForm() = default;

void TUserInfoForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TUserInfoForm::resizeEvent(QResizeEvent* E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

void TUserInfoForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->ProfileEditorUserDefinedInfo());

    // Tab:=TStringGrid(NewWinControlType(Tab,TStringGridEx,ctcmCopyProperties));
    ui.Tab->setItemDelegate(new TStringGridExDelegate(ui.Tab,
        [this](int ACol, int ARow, bool& B) { TabListForCell(ACol, ARow, B); },
        [this](int ACol, int ARow, TStringList* L) { TabGetListForCell(ACol, ARow, L); }));
    SetRowCount(2);

    SetCells(0, 0, LanguageSetup->Key());
    SetCells(1, 0, LanguageSetup->Value());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.AddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TUserInfoForm::FormShow(QObject* Sender)
{
    TStringList* St;
    int I, J;
    QString S, T;

    St = StringToStringList(Game->UserInfo());
    if (St->Count() == 0) { delete St; return; }
    SetRowCount(St->Count() + 1);
    for (I = 0; I < St->Count(); I++) {
        S = (*St)[I];
        J = Pos("=", S);
        if (J == 0) T = ""; else { T = Trim(Copy(S, J + 1, MaxInt)); S = Trim(Copy(S, 1, J - 1)); }
        SetCells(0, I + 1, S);
        SetCells(1, I + 1, T);
    }
    delete St;

    FormResize(Sender);
}

void TUserInfoForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Tab->setColumnWidth(0, (ui.Tab->viewport()->width() - 10) / 2);
    ui.Tab->setColumnWidth(1, (ui.Tab->viewport()->width() - 10) / 2);
}

void TUserInfoForm::AddButtonClick(QObject* Sender)
{
    QAction* M;
    TStringList* St;
    int I, J;
    QPoint P;
    QString S;
    bool B;
    const int SenderTag = Sender->property("tag").toInt();

    if (SenderTag > 0) {
        if ((SenderTag == 1) || (Trim(Cells(0, RowCount() - 1)) != "") || (Trim(Cells(1, RowCount() - 1)) != "")) {
            SetRowCount(RowCount() + 1);
        }
        ui.Tab->setCurrentCell(RowCount() - 2, 0);   // Tab.Row:=Tab.RowCount-1; Tab.Col:=0;
        if (SenderTag == 2) {
            SetCells(0, RowCount() - 1, RemoveUnderline(qobject_cast<QAction*>(Sender)->text()));
            ui.Tab->setCurrentCell(RowCount() - 2, 1);   // Tab.Col:=1;
        }

        ui.Tab->setFocus();
        if (ui.Tab->currentItem() == nullptr) SetCells(ui.Tab->currentColumn(), ui.Tab->currentRow() + 1, "");
        ui.Tab->editItem(ui.Tab->currentItem());   // goAlwaysShowEditor
        return;
    }

    St = GameDB->GetUserKeys();
    ui.AddUserDataPopupMenu->clear();
    M = new QAction(LanguageSetup->AddNewEmptyLine(), ui.AddUserDataPopupMenu); M->setProperty("tag", 1);
    connect(M, &QAction::triggered, this, [this, M]() { AddButtonClick(M); });
    ui.AddUserDataPopupMenu->addAction(M);
    ui.AddUserDataPopupMenu->addSeparator();
    for (I = 0; I < St->Count(); I++) {
        S = ExtUpperCase((*St)[I]); B = true;
        for (J = 1; J < RowCount(); J++) if (ExtUpperCase(Cells(0, J)) == S) { B = false; break; }
        if (!B) continue;
        M = new QAction(MaskUnderlineAmpersand((*St)[I]), ui.AddUserDataPopupMenu); M->setProperty("tag", 2);
        connect(M, &QAction::triggered, this, [this, M]() { AddButtonClick(M); });
        ui.AddUserDataPopupMenu->addAction(M);
    }
    delete St;

    QWidget* C = qobject_cast<QWidget*>(Sender);
    P = ui.Panel1->mapToGlobal(QPoint(C->x(), C->y()));
    ui.AddUserDataPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TUserInfoForm::DelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (RowCount() == 2) {
        SetCells(0, 1, "");
        SetCells(1, 1, "");
        return;
    }

    const int Row = ui.Tab->currentRow() + 1;   // Tab.Row
    if (Row < 1) {
        MessageDlg(LanguageSetup->MessageNoRecordSelected(), mtError, mbOK, 0, this);
        return;
    }

    for (I = Row + 1; I < RowCount(); I++) {
        SetCells(0, I - 1, Cells(0, I));
        SetCells(1, I - 1, Cells(1, I));
    }
    SetRowCount(RowCount() - 1);
}

void TUserInfoForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    QString S, T;

    CommitTabEditor();

    TStringList St;
    for (I = 1; I < RowCount(); I++) {
        S = Trim(Cells(0, I)); T = Trim(Cells(1, I));
        if ((S != "") || (T != "")) St.Add(S + "=" + T);
    }
    if (St.Count() == 0) Game->SetUserInfo(""); else Game->SetUserInfo(StringListToString(&St));
}

void TUserInfoForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileEditProgramInformation);
}

void TUserInfoForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

void TUserInfoForm::TabListForCell(int ACol, int ARow, bool& UseDropdownListForCell)
{
    Q_UNUSED(ARow);
    UseDropdownListForCell = (ACol == 1);
}

void TUserInfoForm::TabGetListForCell(int ACol, int ARow, TStringList* DropdownListForCell)
{
    Q_UNUSED(ACol);
    TStringList* St;
    DropdownListForCell->Clear();
    St = GameDB->GetKeyValueList(Cells(0, ARow));
    DropdownListForCell->AddStrings(*St);
    delete St;
}

/* global */

bool ShowUserInfoDialog(QWidget* AOwner, TGame* AGame, TGameDB* AGameDB)
{
    bool result;
    UserInfoForm = new TUserInfoForm(AOwner);
    UserInfoForm->Game = AGame;
    UserInfoForm->GameDB = AGameDB;
    result = (UserInfoForm->ShowModal() == mrOk);
    delete UserInfoForm;
    UserInfoForm = nullptr;
    return result;
}
