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
    // D-Fend Reborn: program for starting Windows programs (Wine), only on
    // systems other than Windows
    connect(ui.WineCheckBox, &QCheckBox::toggled, ui.WineEdit, &QWidget::setEnabled);
#ifdef Q_OS_WIN
    ui.WineCheckBox->setVisible(false);
    ui.WineEdit->setVisible(false);
    ui.WineInfoLabel->setVisible(false);
#endif
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
    ui.WineCheckBox->setChecked(PrgSetup->UseWindowsProgramsRunner());
    ui.WineEdit->setText(PrgSetup->WindowsProgramsRunner());
    ui.WineEdit->setEnabled(ui.WineCheckBox->isChecked());
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
    ui.WineCheckBox->setText(LanguageSetup->SetupFormWindowsWine());
    ui.WineInfoLabel->setText(LanguageSetup->SetupFormWindowsWineInfo());
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
    ui.WineCheckBox->setChecked(true);
    ui.WineEdit->setText("wine");
    MinimizeDFendCheckBoxClick(this);
}

void TSetupFrameWindowsGames::SaveSetup()
{
    PrgSetup->SetMinimizeOnWindowsGameStart(ui.MinimizeDFendCheckBox->isChecked());
    PrgSetup->SetRestoreWhenWindowsGameCloses(ui.RestoreWindowCheckBox->isChecked());
    PrgSetup->SetAllowPackingWindowsGames(ui.AllowWindowsPackagesCheckBox->isChecked());
    PrgSetup->SetUseWindowsProgramsRunner(ui.WineCheckBox->isChecked());
    PrgSetup->SetWindowsProgramsRunner(Trim(ui.WineEdit->text()).isEmpty() ? QString("wine") : Trim(ui.WineEdit->text()));
}
