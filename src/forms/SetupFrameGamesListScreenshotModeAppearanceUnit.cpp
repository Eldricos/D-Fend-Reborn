// C++ port of SetupFrameGamesListScreenshotModeAppearanceUnit.pas (implementation section).
#include "SetupFrameGamesListScreenshotModeAppearanceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameGamesListScreenshotModeAppearance::TSetupFrameGamesListScreenshotModeAppearance(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameGamesListScreenshotModeAppearance::~TSetupFrameGamesListScreenshotModeAppearance() = default;

// { TSetupFrameGamesListScreenshotModeAppearance }

QString TSetupFrameGamesListScreenshotModeAppearance::GetName()
{
    return LanguageSetup->SetupFormListViewSheet2b();
}

void TSetupFrameGamesListScreenshotModeAppearance::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.WidthEdit->setValue(PrgSetup->ScreenshotListViewWidth());
    ui.HeightEdit->setValue(PrgSetup->ScreenshotListViewHeight());
    ui.UseFirstScreenshotCheckBox->setChecked(PrgSetup->ScreenshotListUseFirstScreenshot());
    ui.UseFirstScreenshotEdit->setValue(Max(1, Min(99, PrgSetup->ScreenshotListUseFirstScreenshotNr())));
}

void TSetupFrameGamesListScreenshotModeAppearance::BeforeChangeLanguage()
{
}

void TSetupFrameGamesListScreenshotModeAppearance::LoadLanguage()
{
    ui.WidthLabel->setText(LanguageSetup->ScreenshotListViewWidth());
    ui.HeightLabel->setText(LanguageSetup->ScreenshotListViewHeight());
    ui.UseFirstScreenshotCheckBox->setText(LanguageSetup->ScreenshotListViewUseFirstScreenshot());
    ui.UseFirstScreenshotLabel->setText(LanguageSetup->ScreenshotListViewUseFirstScreenshotInfo());

    setProperty("HelpContext", ID_FileOptionsListInScreenshotMode);
}

void TSetupFrameGamesListScreenshotModeAppearance::DOSBoxDirChanged()
{
}

void TSetupFrameGamesListScreenshotModeAppearance::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameGamesListScreenshotModeAppearance::HideFrame()
{
}

void TSetupFrameGamesListScreenshotModeAppearance::RestoreDefaults()
{
    ui.WidthEdit->setValue(150);
    ui.HeightEdit->setValue(100);
    ui.UseFirstScreenshotCheckBox->setChecked(true);
    ui.UseFirstScreenshotEdit->setValue(1);
}

void TSetupFrameGamesListScreenshotModeAppearance::SaveSetup()
{
    PrgSetup->SetScreenshotListViewWidth(ui.WidthEdit->value());
    PrgSetup->SetScreenshotListViewHeight(ui.HeightEdit->value());
    PrgSetup->SetScreenshotListUseFirstScreenshot(ui.UseFirstScreenshotCheckBox->isChecked());
    PrgSetup->SetScreenshotListUseFirstScreenshotNr(ui.UseFirstScreenshotEdit->value());
}
