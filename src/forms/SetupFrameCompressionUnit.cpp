// C++ port of SetupFrameCompressionUnit.pas (implementation section).
#include "SetupFrameCompressionUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameCompression::TSetupFrameCompression(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameCompression::~TSetupFrameCompression() = default;

// { TSetupFrameCompression }

QString TSetupFrameCompression::GetName()
{
    return LanguageSetup->SetupFormCompression();
}

void TSetupFrameCompression::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.CompressRadioGroup->setItemIndex(Max(0, Min(4, PrgSetup->CompressionLevel())));
}

void TSetupFrameCompression::BeforeChangeLanguage()
{
}

void TSetupFrameCompression::LoadLanguage()
{
    ui.CompressRadioGroup->setTitle(LanguageSetup->SetupFormCompressionInfo());

    ui.CompressRadioGroup->setItemText(0, LanguageSetup->SetupFormCompressionStore());
    ui.CompressRadioGroup->setItemText(1, LanguageSetup->SetupFormCompressionFast());
    ui.CompressRadioGroup->setItemText(2, LanguageSetup->SetupFormCompressionNormal());
    ui.CompressRadioGroup->setItemText(3, LanguageSetup->SetupFormCompressionGood());
    ui.CompressRadioGroup->setItemText(4, LanguageSetup->SetupFormCompressionMaximum());
    ui.InfoLabel->setText(LanguageSetup->SetupFormCompressionInfoLabel());

    setProperty("HelpContext", ID_FileOptionsCompressionSettings);
}

void TSetupFrameCompression::DOSBoxDirChanged()
{
}

void TSetupFrameCompression::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameCompression::HideFrame()
{
}

void TSetupFrameCompression::RestoreDefaults()
{
    ui.CompressRadioGroup->setItemIndex(3);
}

void TSetupFrameCompression::SaveSetup()
{
    PrgSetup->SetCompressionLevel(ui.CompressRadioGroup->itemIndex());
}
