// C++ port of ModernProfileEditorJoystickFrameUnit.pas (implementation section).
#include "ModernProfileEditorJoystickFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorJoystickFrame::TModernProfileEditorJoystickFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorJoystickFrame::~TModernProfileEditorJoystickFrame() = default;

// { TModernProfileEditorJoystickFrame }

void TModernProfileEditorJoystickFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.TypeLabel->setText(LanguageSetup->ProfileEditorSoundJoystickType());
    St = ValueToList(InitData.GameDB->ConfOpt()->Joysticks(), ";,"); ui.TypeComboBox->addItems(St->ToQStringList()); delete St;
    ui.TimedCheckBox->setText(LanguageSetup->ProfileEditorSoundJoystickTimed());
    ui.AutofireCheckBox->setText(LanguageSetup->ProfileEditorSoundJoystickAutoFire());
    ui.Swap34CheckBox->setText(LanguageSetup->ProfileEditorSoundJoystickSwap34());
    ui.WrapCheckBox->setText(LanguageSetup->ProfileEditorSoundJoystickButtonwrap());
    ui.WrapInfoLabel->setText(LanguageSetup->ProfileEditorSoundJoystickButtonwrapInfo());

    AddDefaultValueHint(ui.TypeComboBox);

    setProperty("HelpContext", ID_ProfileEditJoystick);  // HelpContext:=...
}

static void SetComboBox(QComboBox* ComboBox, const QString& Value, int Default)
{
    QString S;
    if (Default >= 0 && Default < ComboBox->count()) ComboBox->setCurrentIndex(Default);
    S = Trim(ExtUpperCase(Value));
    for (int I = 0; I < ComboBox->count(); I++) if (Trim(ExtUpperCase(ComboBox->itemText(I))) == S) {
        ComboBox->setCurrentIndex(I); break;
    }
}

static void SetComboBox(QComboBox* ComboBox, const QString& Value, const QString& Default)
{
    SetComboBox(ComboBox, Default, 0);
    SetComboBox(ComboBox, Value, ComboBox->currentIndex());
}

void TModernProfileEditorJoystickFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    SetComboBox(ui.TypeComboBox, Game->JoystickType(), QStringLiteral("none"));
    ui.TimedCheckBox->setChecked(Game->JoystickTimed());
    ui.AutofireCheckBox->setChecked(Game->JoystickAutoFire());
    ui.Swap34CheckBox->setChecked(Game->JoystickSwap34());
    ui.WrapCheckBox->setChecked(Game->JoystickButtonwrap());
}

void TModernProfileEditorJoystickFrame::GetGame(TGame* Game)
{
    Game->SetJoystickType(ui.TypeComboBox->currentText());
    Game->SetJoystickTimed(ui.TimedCheckBox->isChecked());
    Game->SetJoystickAutoFire(ui.AutofireCheckBox->isChecked());
    Game->SetJoystickSwap34(ui.Swap34CheckBox->isChecked());
    Game->SetJoystickButtonwrap(ui.WrapCheckBox->isChecked());
}
