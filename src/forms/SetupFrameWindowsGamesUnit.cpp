// C++ port of SetupFrameWindowsGamesUnit.pas (implementation section).
#include "SetupFrameWindowsGamesUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameWindowsGames::TSetupFrameWindowsGames(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.MinimizeDFendCheckBox, &QCheckBox::clicked, this, [this]() { MinimizeDFendCheckBoxClick(ui.MinimizeDFendCheckBox); });
}

TSetupFrameWindowsGames::~TSetupFrameWindowsGames() = default;

// { TSetupFrameWindowsGames }

QString TSetupFrameWindowsGames::GetName()
{
    return LanguageSetup->SetupFormWindows();
}

void TSetupFrameWindowsGames::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.MinimizeDFendCheckBox->setChecked(PrgSetup->MinimizeOnWindowsGameStart());
    ui.RestoreWindowCheckBox->setChecked(PrgSetup->RestoreWhenWindowsGameCloses());
    ui.AllowWindowsPackagesCheckBox->setChecked(PrgSetup->AllowPackingWindowsGames());
    MinimizeDFendCheckBoxClick(this);
}

void TSetupFrameWindowsGames::BeforeChangeLanguage()
{
}

void TSetupFrameWindowsGames::LoadLanguage()
{
    ui.MinimizeDFendCheckBox->setText(LanguageSetup->SetupFormWindowsMinimizeDFendWindowsGame());
    ui.RestoreWindowCheckBox->setText(LanguageSetup->SetupFormWindowsRestoreWhenWindowsGameCloses());
    ui.AutoRestoreInfoLabel->setText(LanguageSetup->SetupFormWindowsRestoreWhenWindowsGameClosesInfo());
    ui.AllowWindowsPackagesCheckBox->setText(LanguageSetup->SetupFormWindowsAllowPackages());
    ui.AllowWindowsPackagesLabel->setText(LanguageSetup->SetupFormWindowsAllowPackagesInfo());
    setProperty("HelpContext", ID_FileOptionsWindows);
}

void TSetupFrameWindowsGames::MinimizeDFendCheckBoxClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.RestoreWindowCheckBox->setEnabled(ui.MinimizeDFendCheckBox->isChecked());
}

void TSetupFrameWindowsGames::DOSBoxDirChanged()
{
}

void TSetupFrameWindowsGames::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameWindowsGames::HideFrame()
{
}

void TSetupFrameWindowsGames::RestoreDefaults()
{
    ui.MinimizeDFendCheckBox->setChecked(false);
    ui.RestoreWindowCheckBox->setChecked(false);
    ui.AllowWindowsPackagesCheckBox->setChecked(false);
    MinimizeDFendCheckBoxClick(this);
}

void TSetupFrameWindowsGames::SaveSetup()
{
    PrgSetup->SetMinimizeOnWindowsGameStart(ui.MinimizeDFendCheckBox->isChecked());
    PrgSetup->SetRestoreWhenWindowsGameCloses(ui.RestoreWindowCheckBox->isChecked());
    PrgSetup->SetAllowPackingWindowsGames(ui.AllowWindowsPackagesCheckBox->isChecked());
}
