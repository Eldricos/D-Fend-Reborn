// C++ port of SetupFrameProfileEditorUnit.pas (implementation section).
#include "SetupFrameProfileEditorUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameProfileEditor::TSetupFrameProfileEditor(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameProfileEditor::~TSetupFrameProfileEditor() = default;

// { TSetupFrameProfileEditor }

QString TSetupFrameProfileEditor::GetName()
{
    return LanguageSetup->ProfileEditor();
}

void TSetupFrameProfileEditor::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.ReopenLastActiveProfileSheetCheckBox->setChecked(PrgSetup->ReopenLastProfileEditorTab());
    ui.ProfileEditorDFendRadioButton->setChecked(PrgSetup->DFendStyleProfileEditor());
    ui.ProfileEditorModernRadioButton->setChecked(!PrgSetup->DFendStyleProfileEditor());
    if (PrgSetup->AlwaysSetScreenshotFolderAutomatically()) ui.AutoSetScreenshotFolderRadioGroup->setItemIndex(1); else ui.AutoSetScreenshotFolderRadioGroup->setItemIndex(0);
    ui.RenameProfFilesCheckBox->setChecked(PrgSetup->RenameProfFileOnRenamingProfile());
    if (PrgSetup->AddMountingDataAutomatically()) ui.AutoAddMountingsRadioGroup->setItemIndex(1); else ui.AutoAddMountingsRadioGroup->setItemIndex(0);
}

void TSetupFrameProfileEditor::BeforeChangeLanguage()
{
}

void TSetupFrameProfileEditor::LoadLanguage()
{
    ui.ReopenLastActiveProfileSheetCheckBox->setText(LanguageSetup->SetupFormReopenLastActiveProfileSheet());
    ui.ProfileEditorDFendRadioButton->setText(LanguageSetup->SetupFormProfileEditorDFendStyle());
    ui.ProfileEditorModernRadioButton->setText(LanguageSetup->SetupFormProfileEditorModern());
    ui.AutoSetScreenshotFolderRadioGroup->setTitle(LanguageSetup->SetupFormProfileEditorAutoSetScreenshotFolder());
    ui.AutoSetScreenshotFolderRadioGroup->setItemText(0, LanguageSetup->SetupFormProfileEditorAutoSetScreenshotFolderOnlyWizard());
    ui.AutoSetScreenshotFolderRadioGroup->setItemText(1, LanguageSetup->SetupFormProfileEditorAutoSetScreenshotFolderAlways());
    ui.RenameProfFilesCheckBox->setText(LanguageSetup->SetupFormProfileEditorRenameProfFilesOnRenamingProfile());
    ui.AutoAddMountingsRadioGroup->setTitle(LanguageSetup->SetupFormProfileEditorAutoAddMountings());
    ui.AutoAddMountingsRadioGroup->setItemText(0, LanguageSetup->SetupFormProfileEditorAutoAddMountingsOnlyWizard());
    ui.AutoAddMountingsRadioGroup->setItemText(1, LanguageSetup->SetupFormProfileEditorAutoAddMountingsAlways());
    setProperty("HelpContext", ID_FileOptionsProfileEditor);
}

void TSetupFrameProfileEditor::DOSBoxDirChanged()
{
}

void TSetupFrameProfileEditor::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameProfileEditor::HideFrame()
{
}

void TSetupFrameProfileEditor::RestoreDefaults()
{
    ui.ReopenLastActiveProfileSheetCheckBox->setChecked(false);
    ui.ProfileEditorModernRadioButton->setChecked(true);
    ui.AutoSetScreenshotFolderRadioGroup->setItemIndex(1);
    ui.RenameProfFilesCheckBox->setChecked(false);
    ui.AutoAddMountingsRadioGroup->setItemIndex(1);
}

void TSetupFrameProfileEditor::SaveSetup()
{
    PrgSetup->SetReopenLastProfileEditorTab(ui.ReopenLastActiveProfileSheetCheckBox->isChecked());
    PrgSetup->SetDFendStyleProfileEditor(ui.ProfileEditorDFendRadioButton->isChecked());
    PrgSetup->SetAlwaysSetScreenshotFolderAutomatically(ui.AutoSetScreenshotFolderRadioGroup->itemIndex() == 1);
    PrgSetup->SetRenameProfFileOnRenamingProfile(ui.RenameProfFilesCheckBox->isChecked());
    PrgSetup->SetAddMountingDataAutomatically(ui.AutoAddMountingsRadioGroup->itemIndex() == 1);
}
