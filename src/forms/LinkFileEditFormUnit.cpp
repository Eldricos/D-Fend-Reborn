// C++ port of LinkFileEditFormUnit.pas (implementation section).
#include "LinkFileEditFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "HelpTools.h"
#include "Process.h"

TLinkFileEditForm* LinkFileEditForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

/* TStringGrid "Tab" (ColCount=2, FixedCols=0, FixedRows=1) */

QString TLinkFileEditForm::Cells(int ACol, int ARow) const
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        return (H != nullptr) ? H->text() : QString();
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    return (It != nullptr) ? It->text() : QString();
}

void TLinkFileEditForm::SetCells(int ACol, int ARow, const QString& Value)
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

int TLinkFileEditForm::RowCount() const { return ui.Tab->rowCount() + 1; }
void TLinkFileEditForm::SetRowCount(int ARowCount) { ui.Tab->setRowCount(ARowCount - 1); }
int TLinkFileEditForm::Row() const { return Max(1, ui.Tab->currentRow() + 1); }
int TLinkFileEditForm::Col() const { return Max(0, ui.Tab->currentColumn()); }
void TLinkFileEditForm::SetRowCol(int ARow, int ACol) { ui.Tab->setCurrentCell(ARow - 1, ACol); }

void TLinkFileEditForm::CommitTabEditor()
{
    QWidget* Editor = QApplication::focusWidget();
    while (Editor != nullptr && Editor->parentWidget() != ui.Tab->viewport() && Editor != ui.Tab) Editor = Editor->parentWidget();
    if (Editor != nullptr && Editor != ui.Tab)
        QMetaObject::invokeMethod(ui.Tab, "commitData", Q_ARG(QWidget*, Editor));
}

/* TLinkFileEditForm */

TLinkFileEditForm::TLinkFileEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "txt";
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "txt";

    ui.Tab->setEditTriggers(QAbstractItemView::AllEditTriggers);   // goEditing, goAlwaysShowEditor
    SetRowCount(2);

    connect(ui.Tab, &QTableWidget::currentCellChanged, this, [this]() { TabClick(ui.Tab); });
    connect(ui.AddButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.DeleteButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.DeleteButton); });
    connect(ui.MoveUpButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MoveUpButton); });
    connect(ui.MoveDownButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MoveDownButton); });
    connect(ui.ImportButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.ImportButton); });
    connect(ui.ExportButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.ExportButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TLinkFileEditForm::~TLinkFileEditForm()
{
    delete OpenDialog;
    delete SaveDialog;
}

void TLinkFileEditForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TLinkFileEditForm::resizeEvent(QResizeEvent* E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

void TLinkFileEditForm::FormCreate(QObject* Sender)
{
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(...) - not needed with Qt

    setWindowTitle(LanguageSetup->EditLinkCaption());
    ui.AddButton->setText(LanguageSetup->Add());
    ui.DeleteButton->setText(LanguageSetup->Del());
    ui.MoveUpButton->setText(LanguageSetup->MoveUp());
    ui.MoveDownButton->setText(LanguageSetup->MoveDown());
    ui.ImportButton->setText(LanguageSetup->EditLinkImport());
    ui.ExportButton->setText(LanguageSetup->EditLinkExport());
    SetCells(0, 0, LanguageSetup->EditLinkName());
    SetCells(1, 0, LanguageSetup->EditLinkLink());
    ui.InfoLabel->setText(LanguageSetup->EditLinkInfo());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    OpenDialog->Title = LanguageSetup->EditLinkImportTitle();
    OpenDialog->Filter = LanguageSetup->EditLinkImportFilter();
    SaveDialog->Title = LanguageSetup->EditLinkExportTitle();
    SaveDialog->Filter = LanguageSetup->EditLinkExportFilter();

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton);
    UserIconLoader->DialogImage(DI_Up, ui.MoveUpButton);
    UserIconLoader->DialogImage(DI_Down, ui.MoveDownButton);
    UserIconLoader->DialogImage(DI_Import, ui.ImportButton);
    UserIconLoader->DialogImage(DI_Export, ui.ExportButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    FormResize(Sender);
}

void TLinkFileEditForm::FormShow(QObject* Sender)
{
    int I;

    if (!AllowLinesAndSubs) ui.InfoPanel->setVisible(false);

    SetRowCount(Max(2, Links->Count() + 1));
    if (Links->Count() == 0) return;
    for (I = 0; I < Names->Count(); I++) {
        SetCells(0, I + 1, (*Names)[I]);
        SetCells(1, I + 1, (*Links)[I]);
    }
    TabClick(Sender);

    ui.HelpButton->setVisible(HelpID >= 0);
    ui.AddButton->setEnabled(!FixedNumber);
    ui.DeleteButton->setEnabled(!FixedNumber);
}

void TLinkFileEditForm::TabClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DeleteButton->setEnabled((RowCount() > 2) && (!FixedNumber));
    ui.MoveUpButton->setEnabled(Row() > 1);
    ui.MoveDownButton->setEnabled(Row() < RowCount() - 1);
}

void TLinkFileEditForm::ButtonWork(QObject* Sender)
{
    int I, J;
    QString S;
    TStringList* St;

    CommitTabEditor();

    switch (Sender->property("tag").toInt()) {
        case 0:
            SetRowCount(RowCount() + 1);
            SetRowCol(RowCount() - 1, 0);
            TabClick(Sender);
            break;
        case 1:
            for (I = Row() + 1; I < RowCount(); I++) {
                SetCells(0, I - 1, Cells(0, I));
                SetCells(1, I - 1, Cells(1, I));
            }
            I = Row(); J = Col();
            SetRowCount(RowCount() - 1);
            SetRowCol(Max(1, I - 1), J);
            TabClick(Sender);
            ui.Tab->viewport()->update();
            break;
        case 2:
            I = Row(); J = Col();
            S = Cells(0, I); SetCells(0, I, Cells(0, I - 1)); SetCells(0, I - 1, S);
            S = Cells(1, I); SetCells(1, I, Cells(1, I - 1)); SetCells(1, I - 1, S);
            SetRowCol(I - 1, J);
            TabClick(Sender);
            break;
        case 3:
            I = Row(); J = Col();
            S = Cells(0, I); SetCells(0, I, Cells(0, I + 1)); SetCells(0, I + 1, S);
            S = Cells(1, I); SetCells(1, I, Cells(1, I + 1)); SetCells(1, I + 1, S);
            SetRowCol(I + 1, J);
            TabClick(Sender);
            break;
        case 4: {
            OpenDialog->InitialDir = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!OpenDialog->Execute()) return;
            TStringList StL;
            St = &StL;
            if (!St->LoadFromFile(OpenDialog->FileName)) MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {OpenDialog->FileName}), mtError, mbOK, 0, this);
            if (St->Count() == 0) return;
            I = RowCount();
            SetRowCount(I + St->Count());
            for (J = 0; J < St->Count(); J++) {
                S = Trim((*St)[J]);
                if ((S != "") && (Pos(";", S) > 0)) {
                    SetCells(0, J + I, Trim(Copy(S, 1, Pos(";", S) - 1)));
                    SetCells(1, J + I, Trim(Copy(S, Pos(";", S) + 1, MaxInt)));
                }
            }
            TabClick(Sender);
            break;
        }
        case 5:
            SaveDialog->InitialDir = GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY);
            if (!SaveDialog->Execute()) return;
            St = GetLinks();
            if (!St->SaveToFile(SaveDialog->FileName)) MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {SaveDialog->FileName}), mtError, mbOK, 0, this);
            delete St;
            break;
    }
    ui.Tab->setFocus();
}

TStringList* TLinkFileEditForm::GetLinks()
{
    TStringList* result = new TStringList;
    int I;

    for (I = 1; I < RowCount(); I++) if ((Trim(Cells(0, I)) == "") && (Trim(Cells(1, I)) == ""))
        result->Add("");
    else
        result->Add(Trim(Cells(0, I)) + ";" + Trim(Cells(1, I)));
    return result;
}

void TLinkFileEditForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    CommitTabEditor();
    Names->Clear();
    Links->Clear();
    for (I = 1; I < RowCount(); I++) {
        Names->Add(Trim(Cells(0, I)));
        Links->Add(Trim(Cells(1, I)));
    }
}

void TLinkFileEditForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (HelpID >= 0) HelpCommandContext(HelpID);
}

void TLinkFileEditForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

void TLinkFileEditForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Tab->setColumnWidth(0, (ui.Tab->viewport()->width() - 5) * 2 / 5);
    ui.Tab->setColumnWidth(1, (ui.Tab->viewport()->width() - 5) * 3 / 5);
}

/* global */

bool ShowLinkFileEditDialog(QWidget* AOwner, TStringList* ANames, TStringList* ALinks, bool AAllowLinesAndSubs, bool AFixedNumber, int AHelpID)
{
    bool result;
    LinkFileEditForm = new TLinkFileEditForm(AOwner);
    LinkFileEditForm->Names = ANames;
    LinkFileEditForm->Links = ALinks;
    LinkFileEditForm->AllowLinesAndSubs = AAllowLinesAndSubs;
    LinkFileEditForm->FixedNumber = AFixedNumber;
    LinkFileEditForm->HelpID = AHelpID;
    result = (LinkFileEditForm->ShowModal() == mrOk);
    delete LinkFileEditForm;
    LinkFileEditForm = nullptr;
    return result;
}
