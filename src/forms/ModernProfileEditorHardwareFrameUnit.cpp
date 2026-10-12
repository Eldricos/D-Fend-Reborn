// C++ port of ModernProfileEditorHardwareFrameUnit.pas (implementation section).
#include "ModernProfileEditorHardwareFrameUnit.h"

#include "LanguageSetupUnit.h"

TModernProfileEditorHardwareFrame::TModernProfileEditorHardwareFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorHardwareFrame::~TModernProfileEditorHardwareFrame() = default;

// { TModernProfileEditorHardwareFrame }

void TModernProfileEditorHardwareFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.AllowDefaultValueReset = false;

    ui.InfoLabel->setText(LanguageSetup->ProfileEditorHardwareInfo());
}

void TModernProfileEditorHardwareFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(Game); Q_UNUSED(LoadFromTemplate);
}

void TModernProfileEditorHardwareFrame::GetGame(TGame* Game)
{
    Q_UNUSED(Game);
}
