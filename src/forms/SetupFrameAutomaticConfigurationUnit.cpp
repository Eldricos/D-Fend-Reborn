// C++ port of SetupFrameAutomaticConfigurationUnit.pas (implementation section).
#include "SetupFrameAutomaticConfigurationUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "PrgConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameAutomaticConfiguration::TSetupFrameAutomaticConfiguration(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameAutomaticConfiguration::~TSetupFrameAutomaticConfiguration() = default;

// Memo.Lines.Clear; Memo.Lines.AddStrings(St);
static void SetMemoLines(QPlainTextEdit *Memo, TStringList *St)
{
    Memo->clear();
    Memo->setPlainText(St->ToQStringList().join("\n"));
}

// { TSetupFrameAutomaticConfiguration }

QString TSetupFrameAutomaticConfiguration::GetName()
{
    return LanguageSetup->SetupFormAutomaticConfiguration();
}

void TSetupFrameAutomaticConfiguration::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    int I;
    TStringList *St;
    ui.WizardRadioGroup->clear();
    for (I = 0; I <= 2; I++) ui.WizardRadioGroup->addItem(QString());
    ui.ZipImportCheckBox->setChecked(PrgSetup->ImportZipWithoutDialogIfPossible());
    ui.WizardRadioGroup->setItemIndex(Max(0, Min(2, PrgSetup->LastWizardMode())));
    St = ValueToList(PrgSetup->InstallerNames());
    SetMemoLines(ui.InstallerNamesMemo, St);
    delete St;
    St = ValueToList(PrgSetup->ArchiveIDFiles());
    SetMemoLines(ui.ArchiveIDFilesMemo, St);
    delete St;
}

void TSetupFrameAutomaticConfiguration::BeforeChangeLanguage()
{
}

void TSetupFrameAutomaticConfiguration::LoadLanguage()
{
    int I;
    ui.ZipImportGroupBox->setTitle(LanguageSetup->SetupFormAutomaticConfigurationArchiveImport());
    ui.ZipImportCheckBox->setText(LanguageSetup->SetupFormAutomaticConfigurationArchiveImportNoDialog());
    I = ui.WizardRadioGroup->itemIndex();
    ui.WizardRadioGroup->setTitle(LanguageSetup->WizardFormWizardMode());
    ui.WizardRadioGroup->setItemText(0, LanguageSetup->WizardFormWizardModeAlwaysAutomatically());
    ui.WizardRadioGroup->setItemText(1, LanguageSetup->WizardFormWizardModeAutomaticallyIfAutoSetupTemplateExists());
    ui.WizardRadioGroup->setItemText(2, LanguageSetup->WizardFormWizardModeAlwaysAllPages());
    ui.WizardRadioGroup->setItemIndex(I);
    ui.InstallerNamesLabel->setText(LanguageSetup->SetupFormAutomaticConfigurationInstallerNames());
    ui.ArchiveIDFilesLabel->setText(LanguageSetup->SetupFormAutomaticConfigurationArchiveIDFiles());
    setProperty("HelpContext", ID_FileOptionsAutomaticConfiguration);
}

void TSetupFrameAutomaticConfiguration::DOSBoxDirChanged()
{
}

void TSetupFrameAutomaticConfiguration::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameAutomaticConfiguration::HideFrame()
{
}

void TSetupFrameAutomaticConfiguration::RestoreDefaults()
{
    TStringList *St;
    PrgSetup->SetImportZipWithoutDialogIfPossible(true);
    PrgSetup->SetLastWizardMode(1);
    St = ValueToList(DefaultInstallerNames);
    SetMemoLines(ui.InstallerNamesMemo, St);
    delete St;
    St = ValueToList(DefaultArchiveIDFiles);
    SetMemoLines(ui.ArchiveIDFilesMemo, St);
    delete St;
}

void TSetupFrameAutomaticConfiguration::SaveSetup()
{
    TStringList *St;
    QString S;
    QStringList Lines;
    int I;
    PrgSetup->SetImportZipWithoutDialogIfPossible(ui.ZipImportCheckBox->isChecked());
    PrgSetup->SetLastWizardMode(ui.WizardRadioGroup->itemIndex());
    St = new TStringList;
    Lines = ui.InstallerNamesMemo->toPlainText().split('\n');
    for (I = 0; I < Lines.count(); I++) {
        S = Trim(Lines[I]);
        if (!S.isEmpty()) St->Add(S);
    }
    PrgSetup->SetInstallerNames(ListToValue(St));
    delete St;
    St = new TStringList;
    Lines = ui.ArchiveIDFilesMemo->toPlainText().split('\n');
    for (I = 0; I < Lines.count(); I++) {
        S = Trim(Lines[I]);
        if (!S.isEmpty()) St->Add(S);
    }
    PrgSetup->SetArchiveIDFiles(ListToValue(St));
    delete St;
}
