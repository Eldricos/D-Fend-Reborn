// C++ port of ChecksumFormUnit.pas (implementation section).
#include "ChecksumFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

TChecksumForm* ChecksumForm = nullptr;

TChecksumForm::TChecksumForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.YesButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.YesButton); });
    connect(ui.NoButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.NoButton); });
    connect(ui.TurnOffButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.TurnOffButton); });
    FormCreate(this);   // OnCreate
}

TChecksumForm::~TChecksumForm() = default;

void TChecksumForm::ButtonWork(QObject* Sender)
{
    ButtonResult = Sender->property("tag").toInt();
}

void TChecksumForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->CheckSumCaption());
    ui.YesButton->setText(LanguageSetup->Yes());
    ui.NoButton->setText(LanguageSetup->No());
    ui.TurnOffButton->setText(LanguageSetup->CheckSumTurnOffForProfile());

    UserIconLoader->DialogImage(DI_Yes, ui.YesButton);
    UserIconLoader->DialogImage(DI_No, ui.NoButton);
    UserIconLoader->DialogImage(DI_Abort, ui.TurnOffButton);

    ButtonResult = 1;
}

// { global }

TChecksumDialogResult ShowCheckSumWarningDialog(QWidget* AOwner, const QString& Text)
{
    TChecksumDialogResult result = cdNo;
    ChecksumForm = new TChecksumForm(AOwner);
    ChecksumForm->ui.InfoLabel->setText(Text);
    ChecksumForm->ShowModal();
    switch (ChecksumForm->ButtonResult) {
        case 0: result = cdYes; break;
        case 1: result = cdNo; break;
        case 2: result = cdTurnOff; break;
    }
    delete ChecksumForm;
    ChecksumForm = nullptr;
    return result;
}
