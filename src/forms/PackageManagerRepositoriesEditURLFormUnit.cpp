// C++ port of PackageManagerRepositoriesEditURLFormUnit.pas (implementation section).
#include "PackageManagerRepositoriesEditURLFormUnit.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"

TPackageManagerRepositoriesEditURLForm *PackageManagerRepositoriesEditURLForm = nullptr;

TPackageManagerRepositoriesEditURLForm::TPackageManagerRepositoriesEditURLForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // Non visual DFM components
    ClipboardTimer = new QTimer(this);
    ClipboardTimer->setInterval(100);
    connect(ClipboardTimer, &QTimer::timeout, this, [this]() { ClipboardTimerTimer(ClipboardTimer); });
    ClipboardTimer->start();
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "xml";

    connect(ui.SelectFileButton, &QToolButton::clicked, this, [this]() { SelectFileButtonClick(ui.SelectFileButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.RadioButtonURL, &QRadioButton::clicked, this, [this]() { RadioButtonOrEditChange(ui.RadioButtonURL); });
    connect(ui.RadioButtonLocalFile, &QRadioButton::clicked, this, [this]() { RadioButtonOrEditChange(ui.RadioButtonLocalFile); });
    connect(ui.URLEdit, &QLineEdit::textChanged, this, [this]() { EditChange(ui.URLEdit); });
    connect(ui.PasteURLButton, &QPushButton::clicked, this, [this]() { PasteURLButtonClick(ui.PasteURLButton); });
    connect(ui.LocalFileEdit, &QLineEdit::textChanged, this, [this]() { EditChange(ui.LocalFileEdit); });
}

TPackageManagerRepositoriesEditURLForm::~TPackageManagerRepositoriesEditURLForm()
{
    delete OpenDialog;
}

void TPackageManagerRepositoriesEditURLForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TPackageManagerRepositoriesEditURLForm::FormShow(QObject *Sender)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    if (Trim(URL).isEmpty()) setWindowTitle(LanguageSetup->RepositoriesEditorAddSourceCaption()); else setWindowTitle(LanguageSetup->RepositoriesEditorEditSourceCaption());
    ui.RadioButtonURL->setText(LanguageSetup->RepositoriesEditorURL());
    ui.PasteURLButton->setText(LanguageSetup->RepositoriesEditorURLPaste());
    ui.RadioButtonLocalFile->setText(LanguageSetup->RepositoriesEditorLocalFile());
    ui.SelectFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.WarningGroupBox->setTitle(LanguageSetup->MsgDlgWarning());
    ui.WarningLabel->setText(LanguageSetup->RepositoriesEditorAddSourceWarning());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    OpenDialog->Title = LanguageSetup->RepositoriesEditorLocalFileTitle();
    OpenDialog->Filter = LanguageSetup->RepositoriesEditorLocalFileFilter();

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_PasteFromClipboard, ui.PasteURLButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectFileButton);

    QString S = Trim(ExtUpperCase(URL));
    if ((Copy(S, 1, 7) == QString("HTTP:/") + "/") || (Copy(S, 1, 7) == QString("HTTPS:/") + "/") || S.isEmpty()) {
        ui.URLEdit->setText(URL);
        ui.RadioButtonURL->setChecked(true);
    } else {
        ui.LocalFileEdit->setText(URL);
        ui.RadioButtonLocalFile->setChecked(true);
    }

    if (Trim(URL).isEmpty()) {
        ClipboardTimerTimer(Sender);
        if (ui.PasteURLButton->isEnabled()) PasteURLButtonClick(Sender);
    }

    RadioButtonOrEditChange(Sender);
}

void TPackageManagerRepositoriesEditURLForm::EditChange(QObject *Sender)
{
    if (Sender == ui.URLEdit) ui.RadioButtonURL->setChecked(true);
    if (Sender == ui.LocalFileEdit) ui.RadioButtonLocalFile->setChecked(true);

    RadioButtonOrEditChange(Sender);
}

void TPackageManagerRepositoriesEditURLForm::RadioButtonOrEditChange(QObject * /*Sender*/)
{
    QString S;
    if (ui.RadioButtonURL->isChecked()) S = ui.URLEdit->text(); else S = ui.LocalFileEdit->text();
    ui.OKButton->setEnabled(!Trim(S).isEmpty());
}

void TPackageManagerRepositoriesEditURLForm::ClipboardTimerTimer(QObject * /*Sender*/)
{
    const QMimeData *M = QGuiApplication::clipboard()->mimeData();
    bool B = (M != nullptr) && M->hasText();
    if (B) {
        QString S = Trim(ExtUpperCase(QGuiApplication::clipboard()->text()));
        B = (Copy(S, 1, 7) == QString("HTTP:/") + "/") || (Copy(S, 1, 7) == QString("HTTPS:/") + "/");
    }
    ui.PasteURLButton->setEnabled(B);
}

void TPackageManagerRepositoriesEditURLForm::PasteURLButtonClick(QObject * /*Sender*/)
{
    ui.URLEdit->setText(QGuiApplication::clipboard()->text());
}

void TPackageManagerRepositoriesEditURLForm::SelectFileButtonClick(QObject * /*Sender*/)
{
    if (Trim(ui.LocalFileEdit->text()).isEmpty()) OpenDialog->InitialDir = ""; else OpenDialog->InitialDir = ExtractFilePath(ui.LocalFileEdit->text());
    if (OpenDialog->Execute()) {
        ProcessMessages();   // Otherwise the radiobutton won't turn
        ui.LocalFileEdit->setText(OpenDialog->FileName);
    }
}

void TPackageManagerRepositoriesEditURLForm::OKButtonClick(QObject * /*Sender*/)
{
    if (ui.RadioButtonURL->isChecked()) URL = ui.URLEdit->text(); else URL = ui.LocalFileEdit->text();
}

// global

bool ShowPackageManagerRepositoriesEditURLDialog(QWidget *AOwner, QString &AURL)
{
    PackageManagerRepositoriesEditURLForm = new TPackageManagerRepositoriesEditURLForm(AOwner);
    PackageManagerRepositoriesEditURLForm->URL = AURL;
    bool result = (PackageManagerRepositoriesEditURLForm->ShowModal() == mrOk);
    if (result) AURL = PackageManagerRepositoriesEditURLForm->URL;
    delete PackageManagerRepositoriesEditURLForm;
    PackageManagerRepositoriesEditURLForm = nullptr;
    return result;
}
