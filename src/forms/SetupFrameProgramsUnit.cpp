// C++ port of SetupFrameProgramsUnit.pas (implementation section).
#include "SetupFrameProgramsUnit.h"
#include "LanguageSetupUnit.h"

TSetupFramePrograms::TSetupFramePrograms(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFramePrograms::~TSetupFramePrograms() = default;

// { TSetupFramePrograms }

QString TSetupFramePrograms::GetName()
{
    return LanguageSetup->SetupFormProgramsSheet();
}

void TSetupFramePrograms::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
}

void TSetupFramePrograms::BeforeChangeLanguage()
{
}

void TSetupFramePrograms::LoadLanguage()
{
    ui.InfoLabel->setText(LanguageSetup->SetupFormProgramsInfo());
}

void TSetupFramePrograms::DOSBoxDirChanged()
{
}

void TSetupFramePrograms::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFramePrograms::HideFrame()
{
}

void TSetupFramePrograms::RestoreDefaults()
{
}

void TSetupFramePrograms::SaveSetup()
{
}
