// C++ port of SetupFrameDOSBoxGlobalUnit.pas (implementation section).
#include "SetupFrameDOSBoxGlobalUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameDOSBoxGlobal::TSetupFrameDOSBoxGlobal(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.MinimizeDFendCheckBox, &QCheckBox::clicked, this, [this]() { MinimizeDFendCheckBoxClick(ui.MinimizeDFendCheckBox); });
}

TSetupFrameDOSBoxGlobal::~TSetupFrameDOSBoxGlobal() = default;

// { TSetupFrameDOSBoxGlobal }

QString TSetupFrameDOSBoxGlobal::GetName()
{
    return LanguageSetup->SetupFormDOSBoxGlobalSheet();
}

void TSetupFrameDOSBoxGlobal::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.GlobalGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxInstallationGlobal());
    ui.MinimizeDFendCheckBox->setChecked(PrgSetup->MinimizeOnDosBoxStart());
    ui.RestoreWindowCheckBox->setChecked(PrgSetup->RestoreWhenDOSBoxCloses());
    ui.UseShortPathNamesCheckBox->setChecked(PrgSetup->UseShortFolderNames());
    ui.ShortNameWarningsCheckBox->setChecked(PrgSetup->ShowShortNameWarnings());
    ui.CreateConfFilesCheckBox->setChecked(PrgSetup->CreateConfFilesForProfiles() && (OperationMode != omPortable));
    ui.DOSBoxFailedEdit->setValue(Min(60, Max(1, PrgSetup->DOSBoxStartFailedTimeout())));
    MinimizeDFendCheckBoxClick(this);
    setProperty("HelpContext", ID_FileOptionsDOSBoxGlobal);
}

void TSetupFrameDOSBoxGlobal::BeforeChangeLanguage()
{
}

void TSetupFrameDOSBoxGlobal::LoadLanguage()
{
    ui.MinimizeDFendCheckBox->setText(LanguageSetup->SetupFormMinimizeDFend());
    ui.RestoreWindowCheckBox->setText(LanguageSetup->SetupFormRestoreWindow());
    ui.UseShortPathNamesCheckBox->setText(LanguageSetup->SetupFormUseShortPathNames());
    ui.ShortNameWarningsCheckBox->setText(LanguageSetup->SetupFormShortNameWarnings());
    ui.CreateConfFilesCheckBox->setText(LanguageSetup->SetupFormAlwaysCreateConfFiles());
    ui.DOSBoxFailedLabel->setText(LanguageSetup->SetupFormDOSBoxStartFailedTimeout());
}

void TSetupFrameDOSBoxGlobal::MinimizeDFendCheckBoxClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.RestoreWindowCheckBox->setEnabled(ui.MinimizeDFendCheckBox->isChecked());
}

void TSetupFrameDOSBoxGlobal::DOSBoxDirChanged()
{
}

void TSetupFrameDOSBoxGlobal::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
    ui.CreateConfFilesCheckBox->setEnabled(OperationMode != omPortable);
}

void TSetupFrameDOSBoxGlobal::HideFrame()
{
}

void TSetupFrameDOSBoxGlobal::RestoreDefaults()
{
    ui.MinimizeDFendCheckBox->setChecked(false);
    ui.RestoreWindowCheckBox->setChecked(false);
    ui.UseShortPathNamesCheckBox->setChecked(true);
    ui.ShortNameWarningsCheckBox->setChecked(true);
    ui.CreateConfFilesCheckBox->setChecked(false);
    ui.DOSBoxFailedEdit->setValue(3);
    MinimizeDFendCheckBoxClick(this);
}

void TSetupFrameDOSBoxGlobal::SaveSetup()
{
    PrgSetup->SetMinimizeOnDosBoxStart(ui.MinimizeDFendCheckBox->isChecked());
    PrgSetup->SetRestoreWhenDOSBoxCloses(ui.RestoreWindowCheckBox->isChecked());
    PrgSetup->SetUseShortFolderNames(ui.UseShortPathNamesCheckBox->isChecked());
    PrgSetup->SetShowShortNameWarnings(ui.ShortNameWarningsCheckBox->isChecked());
    PrgSetup->SetCreateConfFilesForProfiles(ui.CreateConfFilesCheckBox->isChecked());
    PrgSetup->SetDOSBoxStartFailedTimeout(ui.DOSBoxFailedEdit->value());
}
