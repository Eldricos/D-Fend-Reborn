// C++ port of SetupFrameBaseUnit.pas (implementation section).
#include "SetupFrameBaseUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

TSetupFrameBase::TSetupFrameBase(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameBase::~TSetupFrameBase()
{
}

/* TSetupFrameBase */

QString TSetupFrameBase::GetName()
{
    return LanguageSetup->SetupFormGeneralSheet();
}

void TSetupFrameBase::InitGUIAndLoadSetup(TInitData& InitData)
{
    Q_UNUSED(InitData);
    // NoFlicker(...): not needed in Qt

    ui.MinimizeToTrayCheckBox->setChecked(PrgSetup->MinimizeToTray());
    ui.StartWithWindowsCheckBox->setChecked(PrgSetup->StartWithWindows());
    ui.OnlySingleInstanceCheckBox->setChecked(PrgSetup->SingleInstance());

    setProperty("HelpContext", ID_FileOptionsGeneral);
}

void TSetupFrameBase::BeforeChangeLanguage()
{
}

void TSetupFrameBase::LoadLanguage()
{
    ui.MinimizeToTrayCheckBox->setText(LanguageSetup->SetupFormMinimizeToTray());
    ui.StartWithWindowsCheckBox->setText(LanguageSetup->SetupFormStartWithWindows());
    ui.OnlySingleInstanceCheckBox->setText(LanguageSetup->SetupFormSingleInstance());
}

void TSetupFrameBase::DOSBoxDirChanged()
{
}

void TSetupFrameBase::ShowFrame(bool AdvancedMode)
{
    ui.StartWithWindowsCheckBox->setVisible(AdvancedMode);
    ui.OnlySingleInstanceCheckBox->setVisible(AdvancedMode);
}

void TSetupFrameBase::HideFrame()
{
}

void TSetupFrameBase::RestoreDefaults()
{
    ui.MinimizeToTrayCheckBox->setChecked(false);
    ui.StartWithWindowsCheckBox->setChecked(false);
    ui.OnlySingleInstanceCheckBox->setChecked(true);
}

void TSetupFrameBase::SaveSetup()
{
    PrgSetup->SetMinimizeToTray(ui.MinimizeToTrayCheckBox->isChecked());
    PrgSetup->SetStartWithWindows(ui.StartWithWindowsCheckBox->isChecked());
    SetStartWithWindows(PrgSetup->StartWithWindows());
    PrgSetup->SetSingleInstance(ui.OnlySingleInstanceCheckBox->isChecked());
}
