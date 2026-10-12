// C++ port of ExportGamesListFormUnit.pas (implementation section).
#include "ExportGamesListFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "Process.h"

TExportGamesListForm* ExportGamesListForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

TExportGamesListForm::TExportGamesListForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "txt";

    connect(ui.SelectFileButton, &QToolButton::clicked, this, [this]() { SelectFileButtonClick(ui.SelectFileButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.FileNameEdit, &QLineEdit::textChanged, this, [this]() { FileNameEditChange(ui.FileNameEdit); });
    connect(ui.ListBox, &QListWidget::itemChanged, this, [this]() { ListBoxClickCheck(ui.ListBox); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.ExportFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ExportFormatComboBoxChange(ui.ExportFormatComboBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TExportGamesListForm::~TExportGamesListForm()
{
    delete SaveDialog;
}

void TExportGamesListForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TExportGamesListForm::done(int R)
{
    TCloseAction Action = caHide;
    FormClose(this, Action);
    TVclForm::done(R);
}

void TExportGamesListForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    TStringList* St;
    int I;
    QString S;

    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->MenuFileExportGamesListDialog());
    ui.ListBoxLabel->setText(LanguageSetup->MenuFileExportGamesListListBoxLabel());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.FileNameEdit_EditLabel->setText(LanguageSetup->MenuFileExportGamesListFileNameEdit());
    ui.SelectFileButton->setToolTip(LanguageSetup->ChooseFile());
    SaveDialog->Title = LanguageSetup->MenuFileExportGamesListDialog();
    SaveDialog->Filter = LanguageSetup->MenuFileExportGamesListFilter();

    ui.ExportFormatLabel->setText(LanguageSetup->MenuFileExportGamesListFormat());
    {
        const QSignalBlocker Block(ui.ExportFormatComboBox);
        ui.ExportFormatComboBox->clear();
        ui.ExportFormatComboBox->addItem(LanguageSetup->MenuFileExportGamesListFormatAuto());
        ui.ExportFormatComboBox->addItem(LanguageSetup->MenuFileExportGamesListFormatText());
        ui.ExportFormatComboBox->addItem(LanguageSetup->MenuFileExportGamesListFormatTable());
        ui.ExportFormatComboBox->addItem(LanguageSetup->MenuFileExportGamesListFormatHTML());
        ui.ExportFormatComboBox->addItem(LanguageSetup->MenuFileExportGamesListFormatXML());
        ui.ExportFormatComboBox->setCurrentIndex(0);
    }

    ui.InfoLabel->setText(LanguageSetup->MenuFileExportGamesListFileNameInfo());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectFileButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    const QSignalBlocker Block(ui.ListBox);   // no OnClickCheck while filling the list

    auto AddCheckItem = [this](const QString& Caption, int Obj) {
        QListWidgetItem* It = new QListWidgetItem(Caption, ui.ListBox);
        It->setFlags(It->flags() | Qt::ItemIsUserCheckable);
        It->setCheckState(Qt::Unchecked);
        It->setData(Qt::UserRole, Obj);
    };
    St = GetGamesListExportLanguageColumns();
    for (I = 0; I < St->Count(); I++) AddCheckItem((*St)[I], I);
    AddCheckItem(LanguageSetup->ProfileEditorUserDefinedInfo(), glecUserFields);
    AddCheckItem(LanguageSetup->GameNotes(), glecUserFields);
    delete St;

    S = Trim(ExtUpperCase(PrgSetup->ExportColumns()));
    while (Length(S) < ui.ListBox->count() - 1) S = S + "X";
    ui.ListBox->item(0)->setCheckState(Qt::Checked);
    for (I = 1; I < ui.ListBox->count(); I++) ui.ListBox->item(I)->setCheckState((S[I - 1] == QChar('X')) ? Qt::Checked : Qt::Unchecked);
}

void TExportGamesListForm::FormShow(QObject* Sender)
{
    ui.FileNameEdit->setText(IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY)) + "List.txt");
    FileNameEditChange(Sender);
}

void TExportGamesListForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender); Q_UNUSED(Action);
    QString S;
    int I;
    S = "";
    for (I = 1; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked) S = S + "X"; else S = S + "-";
    PrgSetup->SetExportColumns(S);
}

void TExportGamesListForm::ListBoxClickCheck(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.ListBox->count() > 0 && ui.ListBox->item(0)->checkState() != Qt::Checked) ui.ListBox->item(0)->setCheckState(Qt::Checked);
}

void TExportGamesListForm::SelectButtonClick(QObject* Sender)
{
    int I;
    for (I = 1; I < ui.ListBox->count(); I++) ui.ListBox->item(I)->setCheckState((Sender->property("tag").toInt() == 0) ? Qt::Checked : Qt::Unchecked);
}

void TExportGamesListForm::SelectFileButtonClick(QObject* Sender)
{
    SaveDialog->FileName = ui.FileNameEdit->text();
    if (!SaveDialog->Execute()) return;
    ui.FileNameEdit->setText(SaveDialog->FileName);
    FileNameEditChange(Sender);
}

void TExportGamesListForm::ExportFormatComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, Ext;
    switch (ui.ExportFormatComboBox->currentIndex()) {
        case 1: Ext = ".txt"; break;
        case 2: Ext = ".csv"; break;
        case 3: Ext = ".html"; break;
        case 4: Ext = ".xml"; break;
        default: return;
    }
    S = Trim(ui.FileNameEdit->text()); if (S == "") return;
    ui.FileNameEdit->setText(ChangeFileExt(S, Ext));
}

void TExportGamesListForm::FileNameEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.OKButton->setEnabled(Trim(ui.FileNameEdit->text()) != "");
}

void TExportGamesListForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TGamesListExportColumns Columns;
    int I;
    for (I = 1; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked) Columns.insert(I);
    ExportGamesList(GameDB, ui.FileNameEdit->text(), Columns, TGamesListExportFormat(ui.ExportFormatComboBox->currentIndex()));
}

void TExportGamesListForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_FileExportList);
}

void TExportGamesListForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowExportGamesListDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    ExportGamesListForm = new TExportGamesListForm(AOwner);
    ExportGamesListForm->GameDB = AGameDB;
    result = (ExportGamesListForm->ShowModal() == mrOk);
    delete ExportGamesListForm;
    ExportGamesListForm = nullptr;
    return result;
}
