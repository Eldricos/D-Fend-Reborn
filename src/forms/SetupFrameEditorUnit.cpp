// C++ port of SetupFrameEditorUnit.pas (implementation section).
#include "SetupFrameEditorUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameEditor::TSetupFrameEditor(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    PrgOpenDialog = new TOpenDialog(this);
    PrgOpenDialog->DefaultExt = "exe";
    // DFM event wiring
    connect(ui.CustomButton, &QToolButton::clicked, this, [this]() { CustomButtonClick(ui.CustomButton); });
    connect(ui.RadioButtonDefault, &QRadioButton::clicked, this, [this]() { RadioButtonClick(ui.RadioButtonDefault); });
    connect(ui.RadioButtonNotepad, &QRadioButton::clicked, this, [this]() { RadioButtonClick(ui.RadioButtonNotepad); });
    connect(ui.RadioButtonCustom, &QRadioButton::clicked, this, [this]() { RadioButtonClick(ui.RadioButtonCustom); });
    connect(ui.CustomEdit, &QLineEdit::textChanged, this, [this]() { CustomEditChange(ui.CustomEdit); });
}

TSetupFrameEditor::~TSetupFrameEditor() = default;

// { TSetupFrameEditor }

QString TSetupFrameEditor::GetName()
{
    return LanguageSetup->SetupFormTextEditor();
}

void TSetupFrameEditor::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    QString S;
    S = Trim(ExtUpperCase(PrgSetup->TextEditor()));
    if (S.isEmpty()) ui.RadioButtonDefault->setChecked(true); else {
        if ((S == "NOTEPAD") || (S == "NOTEPAD.EXE")) ui.RadioButtonNotepad->setChecked(true); else {
            ui.RadioButtonCustom->setChecked(true);
            ui.CustomEdit->setText(MakeRelPath(PrgSetup->TextEditor(), PrgSetup->BaseDir()));
        }
    }
    ui.FallbackCheckBox->setChecked(PrgSetup->FileTypeFallbackForEditor());
    ui.FixLineWrapCheckBox->setChecked(PrgSetup->AutoFixLineWrap());
    UserIconLoader->DialogImage(DI_SelectFile, ui.CustomButton);
    RadioButtonClick(this);
}

void TSetupFrameEditor::BeforeChangeLanguage()
{
}

void TSetupFrameEditor::LoadLanguage()
{
    ui.RadioButtonDefault->setText(LanguageSetup->SetupFormTextEditorDefault());
    ui.FallbackCheckBox->setText(LanguageSetup->SetupFormTextEditorDefaultFallback());
    ui.RadioButtonNotepad->setText(LanguageSetup->SetupFormTextEditorNotepad());
    ui.RadioButtonCustom->setText(LanguageSetup->SetupFormTextEditorCustom());
    ui.CustomButton->setToolTip(LanguageSetup->ChooseFile());
    ui.FixLineWrapCheckBox->setText(LanguageSetup->SetupFormTextEditorFixLineWrap());
    PrgOpenDialog->Title = LanguageSetup->SetupFormTextEditorCustomOpenCaption();
    PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
    setProperty("HelpContext", ID_FileOptionsTextEditor);
}

void TSetupFrameEditor::DOSBoxDirChanged()
{
}

void TSetupFrameEditor::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameEditor::HideFrame()
{
}

void TSetupFrameEditor::RestoreDefaults()
{
    ui.CustomEdit->setText(QString());
    ui.RadioButtonDefault->setChecked(true);
    ui.FallbackCheckBox->setChecked(true);
    ui.FixLineWrapCheckBox->setChecked(false);
    RadioButtonClick(this);
}

void TSetupFrameEditor::SaveSetup()
{
    if (ui.RadioButtonDefault->isChecked()) PrgSetup->SetTextEditor(QString());
    if (ui.RadioButtonNotepad->isChecked()) PrgSetup->SetTextEditor("Notepad.exe");
    if (ui.RadioButtonCustom->isChecked()) PrgSetup->SetTextEditor(MakeRelPath(ui.CustomEdit->text(), PrgSetup->BaseDir()));
    PrgSetup->SetFileTypeFallbackForEditor(ui.FallbackCheckBox->isChecked());
    PrgSetup->SetAutoFixLineWrap(ui.FixLineWrapCheckBox->isChecked());
}

void TSetupFrameEditor::RadioButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.FallbackCheckBox->setEnabled(ui.RadioButtonDefault->isChecked());
}

void TSetupFrameEditor::CustomEditChange(QObject *Sender)
{
    ui.RadioButtonCustom->setChecked(true);
    RadioButtonClick(Sender);
}

void TSetupFrameEditor::CustomButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.CustomEdit->text()); if (!S.isEmpty()) S = ExtractFilePath(S);
    if (S.isEmpty()) S = GetSpecialFolder(window()->winId(), CSIDL_PROGRAM_FILES);
    if (!S.isEmpty()) PrgOpenDialog->InitialDir = S;
    if (PrgOpenDialog->Execute()) {
        ui.RadioButtonCustom->setChecked(true);
        ui.CustomEdit->setText(PrgOpenDialog->FileName);
    }
}
