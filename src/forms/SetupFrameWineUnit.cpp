// C++ port of SetupFrameWineUnit.pas (implementation section).
#include "SetupFrameWineUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameWine::TSetupFrameWine(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameWine::~TSetupFrameWine() = default;

// { TSetupFrameWineFrame }

QString TSetupFrameWine::GetName()
{
    return LanguageSetup->SetupFormWine();
}

void TSetupFrameWine::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    int I;
    ui.MainCheckBox->setChecked(PrgSetup->EnableWineMode());
    for (I = 0; I <= 25; I++) ui.List->insertRow(QString(QChar('A' + I)) + ":", PrgSetup->LinuxRemap(QChar('A' + I)));
    ui.RemapMountsCheckBox->setChecked(PrgSetup->RemapMounts());
    ui.RemapScreenshotFolderCheckBox->setChecked(PrgSetup->RemapScreenShotFolder());
    ui.RemapMapperFileCheckBox->setChecked(PrgSetup->RemapMapperFile());
    ui.RemapDOSBoxFolderCheckBox->setChecked(PrgSetup->RemapDOSBoxFolder());
    ui.LinuxLinkModeCheckBox->setChecked(PrgSetup->LinuxLinkMode());
    ui.ShellScriptPreambleEdit->setText(PrgSetup->LinuxShellScriptPreamble());
}

void TSetupFrameWine::BeforeChangeLanguage()
{
}

void TSetupFrameWine::LoadLanguage()
{
    ui.MainCheckBox->setText(LanguageSetup->SetupFormWineEnable());
    ui.MainInfoLabel->setText(LanguageSetup->SetupFormWineEnableInfo());
    ui.ListInfoLabel->setText(LanguageSetup->SetupFormWineRemap());
    ui.List->setTitleCaptions({LanguageSetup->SetupFormWineRemapDriveLetter(), LanguageSetup->SetupFormWineRemapLinuxPath()});
    ui.RemapMountsCheckBox->setText(LanguageSetup->SetupFormWineRemapMounts());
    ui.RemapScreenshotFolderCheckBox->setText(LanguageSetup->SetupFormWineRemapScreenshots());
    ui.RemapMapperFileCheckBox->setText(LanguageSetup->SetupFormWineRemapMapperFile());
    ui.RemapDOSBoxFolderCheckBox->setText(LanguageSetup->SetupFormWineRemapDOSBox());
    ui.LinuxLinkModeCheckBox->setText(LanguageSetup->SetupFormWineLinuxLinkMode());
    ui.ShellScriptPreambleEdit_EditLabel->setText(LanguageSetup->SetupFormWineShellPreamble());
    setProperty("HelpContext", ID_FileOptionsWineSupport);
}

void TSetupFrameWine::DOSBoxDirChanged()
{
}

void TSetupFrameWine::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameWine::HideFrame()
{
}

void TSetupFrameWine::RestoreDefaults()
{
    int I;
    ui.MainCheckBox->setChecked(false);
    for (I = 0; I <= 25; I++) {
        ui.List->setKey(I, QString(QChar('A' + I)) + ":");
        ui.List->setValue(I, QString());
    }
    ui.RemapMountsCheckBox->setChecked(false);
    ui.RemapScreenshotFolderCheckBox->setChecked(false);
    ui.RemapMapperFileCheckBox->setChecked(false);
    ui.RemapDOSBoxFolderCheckBox->setChecked(false);
    ui.LinuxLinkModeCheckBox->setChecked(false);
    ui.ShellScriptPreambleEdit->setText("#!/bin/bash");
}

void TSetupFrameWine::SaveSetup()
{
    int I;
    PrgSetup->SetEnableWineMode(ui.MainCheckBox->isChecked());
    for (I = 0; I <= 25; I++) PrgSetup->SetLinuxRemap(QChar('A' + I), ui.List->value(I));
    PrgSetup->SetRemapMounts(ui.RemapMountsCheckBox->isChecked());
    PrgSetup->SetRemapScreenShotFolder(ui.RemapScreenshotFolderCheckBox->isChecked());
    PrgSetup->SetRemapMapperFile(ui.RemapMapperFileCheckBox->isChecked());
    PrgSetup->SetRemapDOSBoxFolder(ui.RemapDOSBoxFolderCheckBox->isChecked());
    PrgSetup->SetLinuxLinkMode(ui.LinuxLinkModeCheckBox->isChecked());
    PrgSetup->SetLinuxShellScriptPreamble(ui.ShellScriptPreambleEdit->text());
}
