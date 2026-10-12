// C++ port of FullscreenInfoFormUnit.pas (implementation section).
#include "FullscreenInfoFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

TFullscreenInfoForm* FullscreenInfoForm = nullptr;

TFullscreenInfoForm::TFullscreenInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    FormCreate(this);   // OnCreate
}

TFullscreenInfoForm::~TFullscreenInfoForm() = default;

void TFullscreenInfoForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->MsgDlgInformation());
    ui.InfoLabel->setText(LanguageSetup->FullscreenInfoInfo());
    ui.CheckBox->setText(LanguageSetup->FullscreenInfoCheckbox());
    ui.OKButton->setText(LanguageSetup->OK());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);

    QFont F = ui.InfoLabel->font(); F.setBold(true); ui.InfoLabel->setFont(F);
}

void TFullscreenInfoForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    PrgSetup->SetFullscreenInfo(!ui.CheckBox->isChecked());
}

// { global }

void ShowFullscreenInfoDialog(QWidget* AOwner)
{
    if (!PrgSetup->FullscreenInfo()) return;

    TFullscreenInfoForm* LocalFullscreenInfoForm = new TFullscreenInfoForm(AOwner);
    LocalFullscreenInfoForm->ShowModal();
    delete LocalFullscreenInfoForm;
}
