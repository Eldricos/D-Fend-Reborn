// C++ port of WindowsFileWarningFormUnit.pas (implementation section).
#include "WindowsFileWarningFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TWindowsFileWarningForm* WindowsFileWarningForm = nullptr;

TWindowsFileWarningForm::TWindowsFileWarningForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);   // OnCreate
}

TWindowsFileWarningForm::~TWindowsFileWarningForm() = default;

void TWindowsFileWarningForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TWindowsFileWarningForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt
}

void TWindowsFileWarningForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    setWindowTitle(LanguageSetup->MsgDlgWarning());
    ui.InfoLabel->setText(Format(LanguageSetup->MessageWindowsExeExecuteWarning(), {FileName}));
    ui.TurnOffWarningsCheckBox->setText(LanguageSetup->MessageWindowsExeExecuteWarningTurnOff());

    ui.OKButton->setText(LanguageSetup->OK());
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
}

// { global }

void ShowWindowsFileWarningDialog(QWidget* AOwner, const QString& AFileName, bool& ATurnWarningsOff)
{
    WindowsFileWarningForm = new TWindowsFileWarningForm(AOwner);
    WindowsFileWarningForm->FileName = AFileName;
    WindowsFileWarningForm->ShowModal();
    ATurnWarningsOff = WindowsFileWarningForm->ui.TurnOffWarningsCheckBox->isChecked();
    delete WindowsFileWarningForm;
    WindowsFileWarningForm = nullptr;
}
