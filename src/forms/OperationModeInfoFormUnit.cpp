// C++ port of OperationModeInfoFormUnit.pas (implementation section).
#include "OperationModeInfoFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TOperationModeInfoForm* OperationModeInfoForm = nullptr;

namespace {

#ifndef VK_F1
constexpr quint16 VK_F1 = 0x70;
#endif

// Edit.Color:=clBtnFace
void SetEditColorBtnFace(QLineEdit *E)
{
    QPalette P = E->palette();
    P.setColor(QPalette::Base, P.color(QPalette::Button));
    E->setPalette(P);
}

}  // namespace

TOperationModeInfoForm::TOperationModeInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        quint16 K = (Key == Qt::Key_F1) ? VK_F1 : 0;
        if (K == 0) return false;
        FormKeyDown(this, K, Mods & ~Qt::KeypadModifier);
        return false;
    });

    FormCreate(this);
}

TOperationModeInfoForm::~TOperationModeInfoForm()
{
}

void TOperationModeInfoForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed
    { QFont F = ui.OpModeLabel->font(); F.setBold(true); ui.OpModeLabel->setFont(F); }

    setWindowTitle(LanguageSetup->OperationModeCaption());
    ui.TopLabel->setText(LanguageSetup->OperationModeTopLabel());
    ui.CheckBox1->setText(LanguageSetup->OperationModeInfoPrgSettingsPrgDir());
    ui.CheckBox2->setText(LanguageSetup->OperationModeInfoPrgSettingsUserDir());
    ui.CheckBox3->setText(LanguageSetup->OperationModeInfoUsersShareSettings());
    ui.CheckBox4->setText(LanguageSetup->OperationModeInfoVistaCompatible());
    ui.CheckBox5->setText(LanguageSetup->OperationModeInfoDOSBoxRelative());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    switch (OperationMode) {
    case omPrgDir:
        ui.OpModeLabel->setText(LanguageSetup->OperationModeOpModePrgDir());
        ui.CheckBox1->setChecked(true);
        ui.CheckBox2->setChecked(false);
        ui.CheckBox3->setChecked(true);
        ui.CheckBox4->setChecked(false);
        ui.CheckBox5->setChecked(false);
        break;
    case omUserDir:
        ui.OpModeLabel->setText(LanguageSetup->OperationModeOpModeUserDir());
        ui.CheckBox1->setChecked(false);
        ui.CheckBox2->setChecked(true);
        ui.CheckBox3->setChecked(false);
        ui.CheckBox4->setChecked(true);
        ui.CheckBox5->setChecked(false);
        break;
    case omPortable:
        ui.OpModeLabel->setText(LanguageSetup->OperationModeOpModePortable());
        ui.CheckBox1->setChecked(true);
        ui.CheckBox2->setChecked(false);
        ui.CheckBox3->setChecked(true);
        ui.CheckBox4->setChecked(true);
        ui.CheckBox5->setChecked(true);
        break;
    }

    ui.PrgDirEdit_EditLabel->setText(LanguageSetup->OperationModePrgDir());
    ui.PrgDirEdit->setText(PrgDir());

    ui.PrgDataDirEdit_EditLabel->setText(LanguageSetup->OperationModeDataDir());
    ui.PrgDataDirEdit->setText(PrgDataDir());

    ui.BaseDirEdit_EditLabel->setText(LanguageSetup->SetupFormBaseDir());
    ui.BaseDirEdit->setText(PrgSetup->BaseDir());

    SetEditColorBtnFace(ui.PrgDirEdit);
    SetEditColorBtnFace(ui.PrgDataDirEdit);
    SetEditColorBtnFace(ui.BaseDirEdit);
}

void TOperationModeInfoForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_HelpOperationMode);
}

void TOperationModeInfoForm::FormKeyDown(QObject* Sender, quint16& Key, TShiftState Shift)
{
    if ((Key == VK_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

void ShowOperationModeInfoDialog(QWidget* AOwner)
{
    OperationModeInfoForm = new TOperationModeInfoForm(AOwner);
    OperationModeInfoForm->ShowModal();
    delete OperationModeInfoForm;
    OperationModeInfoForm = nullptr;
}
