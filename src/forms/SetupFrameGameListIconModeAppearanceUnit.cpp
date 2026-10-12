// C++ port of SetupFrameGameListIconModeAppearanceUnit.pas (implementation section).
#include "SetupFrameGameListIconModeAppearanceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameGameListIconModeAppearance::TSetupFrameGameListIconModeAppearance(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameGameListIconModeAppearance::~TSetupFrameGameListIconModeAppearance() = default;

// { TSetupFrameGameListIconModeAppearance }

QString TSetupFrameGameListIconModeAppearance::GetName()
{
    return LanguageSetup->SetupFormGameListIconModeAppearance();
}

void TSetupFrameGameListIconModeAppearance::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    const int I = Max(0, Min(256, PrgSetup->IconSize()));
    if (I <= 47) ui.IconSizeRadioGroup->setItemIndex(0);
    else if (I <= 95) ui.IconSizeRadioGroup->setItemIndex(1);
    else if (I <= 191) ui.IconSizeRadioGroup->setItemIndex(2);
    else ui.IconSizeRadioGroup->setItemIndex(3);
}

void TSetupFrameGameListIconModeAppearance::BeforeChangeLanguage()
{
}

void TSetupFrameGameListIconModeAppearance::LoadLanguage()
{
    ui.IconSizeRadioGroup->setTitle(LanguageSetup->SetupFormGameListIconModeAppearanceIconSize());

    setProperty("HelpContext", ID_FileOptionsGameListInIconMode);
}

void TSetupFrameGameListIconModeAppearance::DOSBoxDirChanged()
{
}

void TSetupFrameGameListIconModeAppearance::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameGameListIconModeAppearance::HideFrame()
{
}

void TSetupFrameGameListIconModeAppearance::RestoreDefaults()
{
    ui.IconSizeRadioGroup->setItemIndex(1);
}

void TSetupFrameGameListIconModeAppearance::SaveSetup()
{
    switch (ui.IconSizeRadioGroup->itemIndex()) {
        case 0: PrgSetup->SetIconSize(32); break;
        case 1: PrgSetup->SetIconSize(64); break;
        case 2: PrgSetup->SetIconSize(128); break;
        case 3: PrgSetup->SetIconSize(256); break;
    }
}
