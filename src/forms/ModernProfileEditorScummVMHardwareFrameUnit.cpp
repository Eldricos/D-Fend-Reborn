// C++ port of ModernProfileEditorScummVMHardwareFrameUnit.pas (implementation section).
#include "ModernProfileEditorScummVMHardwareFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

/* TModernProfileEditorScummVMHardwareFrame */

TModernProfileEditorScummVMHardwareFrame::TModernProfileEditorScummVMHardwareFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.CDEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { CDEditChange(ui.CDEdit); });
    connect(ui.JoystickEdit, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { JoystickEditChange(ui.JoystickEdit); });
}

TModernProfileEditorScummVMHardwareFrame::~TModernProfileEditorScummVMHardwareFrame()
{
}

void TModernProfileEditorScummVMHardwareFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    // NoFlicker(...): not needed with Qt

    ui.CDGroupBox->setTitle(LanguageSetup->ProfileEditorScummVMCDAudioDrive());
    ui.CDRadioButton1->setText(LanguageSetup->ProfileEditorScummVMCDAudioOff());
    ui.CDRadioButton2->setText(LanguageSetup->ProfileEditorScummVMCDAudioNr());
    ui.JoystickGroupBox->setTitle(LanguageSetup->ProfileEditorScummVMJoystick());
    ui.JoystickRadioButton1->setText(LanguageSetup->ProfileEditorScummVMJoystickOff());
    ui.JoystickRadioButton2->setText(LanguageSetup->ProfileEditorScummVMJoystickNr());
    ui.PlatformLabel->setText(LanguageSetup->ProfileEditorScummVMPlatform());
    St = ValueToList(InitData.GameDB->ConfOpt()->ScummVMPlatform(), ";,"); ui.PlatformComboBox->addItems(St->ToQStringList()); delete St;

    AddDefaultValueHint(ui.PlatformComboBox);

    setProperty("HelpContext", ID_ProfileEditHardware);  // HelpContext
}

void TModernProfileEditorScummVMHardwareFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;
    int I;

    ui.CDEdit->setValue(Min(3, Max(0, Game->ScummVMCDROM())));
    ui.CDRadioButton1->setChecked(Game->ScummVMCDROM() == -1);
    ui.CDRadioButton2->setChecked(Game->ScummVMCDROM() >= 0);

    ui.JoystickEdit->setValue(Min(3, Max(0, Game->ScummVMJoystickNum())));
    ui.JoystickRadioButton1->setChecked(Game->ScummVMJoystickNum() == -1);
    ui.JoystickRadioButton2->setChecked(Game->ScummVMJoystickNum() >= 0);

    S = Trim(ExtUpperCase(Game->ScummVMPlatform()));
    ui.PlatformComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.PlatformComboBox->count(); I++) if (S == Trim(ExtUpperCase(ui.PlatformComboBox->itemText(I)))) {
        ui.PlatformComboBox->setCurrentIndex(I); break;
    }
}

void TModernProfileEditorScummVMHardwareFrame::GetGame(TGame* Game)
{
    if (ui.CDRadioButton2->isChecked()) Game->SetScummVMCDROM(ui.CDEdit->value()); else Game->SetScummVMCDROM(-1);
    if (ui.JoystickRadioButton2->isChecked()) Game->SetScummVMJoystickNum(ui.JoystickEdit->value()); else Game->SetScummVMJoystickNum(-1);
    if (ui.PlatformComboBox->currentIndex() <= 0) Game->SetScummVMPlatform(""); else Game->SetScummVMPlatform(ui.PlatformComboBox->currentText());
}

void TModernProfileEditorScummVMHardwareFrame::CDEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CDRadioButton2->setChecked(true);
}

void TModernProfileEditorScummVMHardwareFrame::JoystickEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.JoystickRadioButton2->setChecked(true);
}
