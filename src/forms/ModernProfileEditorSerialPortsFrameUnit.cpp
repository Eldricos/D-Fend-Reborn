// C++ port of ModernProfileEditorSerialPortsFrameUnit.pas (implementation section).
#include "ModernProfileEditorSerialPortsFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "HelpConsts.h"

TModernProfileEditorSerialPortsFrame::TModernProfileEditorSerialPortsFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorSerialPortsFrame::~TModernProfileEditorSerialPortsFrame() = default;

// { TModernProfileEditorSerialPortsFrame }

void TModernProfileEditorSerialPortsFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.AllowDefaultValueReset = false;

    ui.InfoLabel->setText(LanguageSetup->ProfileEditorSerialPortsInfo());

    setProperty("helpContext", ID_ProfileEditSerialPorts);  // HelpContext:=...
}

void TModernProfileEditorSerialPortsFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(Game); Q_UNUSED(LoadFromTemplate);
}

void TModernProfileEditorSerialPortsFrame::GetGame(TGame* Game)
{
    Q_UNUSED(Game);
}
