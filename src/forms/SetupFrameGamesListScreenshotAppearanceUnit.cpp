// C++ port of SetupFrameGamesListScreenshotAppearanceUnit.pas (implementation section).
#include "SetupFrameGamesListScreenshotAppearanceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

namespace {
// TColorBox.Selected:=C
void SetColorBoxSelected(TColorBoxWidget *Box, TColor C)
{
    Box->setSelectedTColor(C);
}
} // namespace

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameGamesListScreenshotAppearance::TSetupFrameGamesListScreenshotAppearance(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageOpenDialog = new TOpenDialog(this);
    ImageOpenDialog->DefaultExt = "jpeg";
    // DFM event wiring
    connect(ui.ScreenshotsListBackgroundButton, &QToolButton::clicked, this, [this]() { ScreenshotsListBackgroundButtonClick(ui.ScreenshotsListBackgroundButton); });
    connect(ui.ScreenshotsListBackgroundColorBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { ScreenshotsListBackgroundColorBoxChange(ui.ScreenshotsListBackgroundColorBox); });
    connect(ui.ScreenshotsListBackgroundEdit, &QLineEdit::textChanged, this, [this]() { ScreenshotsListBackgroundEditChange(ui.ScreenshotsListBackgroundEdit); });
}

TSetupFrameGamesListScreenshotAppearance::~TSetupFrameGamesListScreenshotAppearance() = default;

// { TSetupFrameGamesListScreenshotAppearance }

QString TSetupFrameGamesListScreenshotAppearance::GetName()
{
    return LanguageSetup->SetupFormListViewSheet4();
}

void TSetupFrameGamesListScreenshotAppearance::InitGUIAndLoadSetup(TInitData &InitData)
{
    QString S;
    TColor C;
    PBaseDir = InitData.PBaseDir;
    ui.ReselectCategoryCheckBox->setChecked(PrgSetup->RestorePreviewerCategory());
    S = Trim(PrgSetup->ScreenshotsListViewBackground());
    if (S.isEmpty()) ui.ScreenshotsListBackgroundRadioButton1->setChecked(true); else {
        try {
            C = StringToColor(S); ui.ScreenshotsListBackgroundRadioButton2->setChecked(true); SetColorBoxSelected(ui.ScreenshotsListBackgroundColorBox, C);
        } catch (...) {
            ui.ScreenshotsListBackgroundRadioButton3->setChecked(true); ui.ScreenshotsListBackgroundEdit->setText(S);
        }
    }
    S = Trim(PrgSetup->ScreenshotsListViewFontColor());
    if (S.isEmpty()) SetColorBoxSelected(ui.ScreenshotsListFontColorBox, clWindowText); else {
        try { SetColorBoxSelected(ui.ScreenshotsListFontColorBox, StringToColor(S)); } catch (...) { SetColorBoxSelected(ui.ScreenshotsListFontColorBox, clWindowText); }
    }
    ui.ScreenshotsListFontSizeEdit->setValue(PrgSetup->ScreenshotsListViewFontSize());
    ui.ScreenshotPreviewEdit->setValue(Max(ui.ScreenshotPreviewEdit->minimum(), Min(ui.ScreenshotPreviewEdit->maximum(), PrgSetup->ScreenshotPreviewSize())));
    UserIconLoader->DialogImage(DI_SelectFile, ui.ScreenshotsListBackgroundButton);
}

void TSetupFrameGamesListScreenshotAppearance::BeforeChangeLanguage()
{
}

void TSetupFrameGamesListScreenshotAppearance::LoadLanguage()
{
    bool GermanColorNames;
    ui.ReselectCategoryCheckBox->setText(LanguageSetup->SetupFormStartRestorePreviewerCategory());
    GermanColorNames = (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "DEUTSCH.INI") || (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "GERMAN.INI");
    ui.ScreenshotsListBackgroundColorBox->setPrettyNames(GermanColorNames);
    ui.ScreenshotsListFontColorBox->setPrettyNames(GermanColorNames);
    ui.ScreenshotsListBackgroundRadioButton1->setText(LanguageSetup->SetupFormBackgroundColorDefault());
    ui.ScreenshotsListBackgroundRadioButton2->setText(LanguageSetup->SetupFormBackgroundColorColor());
    ui.ScreenshotsListBackgroundRadioButton3->setText(LanguageSetup->SetupFormBackgroundColorFile());
    ui.ScreenshotsListBackgroundButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ScreenshotsListFontSizeLabel->setText(LanguageSetup->SetupFormFontSize());
    ui.ScreenshotsListFontColorLabel->setText(LanguageSetup->SetupFormFontColor());
    ui.ScreenshotPreviewLabel->setText(LanguageSetup->SetupFormScreenshotPreviewSize());
    setProperty("HelpContext", ID_FileOptionsAppearanceScreenshots);
}

void TSetupFrameGamesListScreenshotAppearance::DOSBoxDirChanged()
{
}

void TSetupFrameGamesListScreenshotAppearance::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameGamesListScreenshotAppearance::HideFrame()
{
}

void TSetupFrameGamesListScreenshotAppearance::RestoreDefaults()
{
    ui.ReselectCategoryCheckBox->setChecked(true);
    ui.ScreenshotsListBackgroundRadioButton1->setChecked(true);
    SetColorBoxSelected(ui.ScreenshotsListBackgroundColorBox, clBlack);
    ui.ScreenshotsListBackgroundEdit->setText(QString());
    ui.ScreenshotsListFontSizeEdit->setValue(9);
    SetColorBoxSelected(ui.ScreenshotsListFontColorBox, clBlack);
    ui.ScreenshotPreviewEdit->setValue(100);
}

void TSetupFrameGamesListScreenshotAppearance::SaveSetup()
{
    PrgSetup->SetRestorePreviewerCategory(ui.ReselectCategoryCheckBox->isChecked());
    if (ui.ScreenshotsListBackgroundRadioButton1->isChecked()) PrgSetup->SetScreenshotsListViewBackground(QString());
    if (ui.ScreenshotsListBackgroundRadioButton2->isChecked()) PrgSetup->SetScreenshotsListViewBackground(ColorToString(ui.ScreenshotsListBackgroundColorBox->selectedTColor()));
    if (ui.ScreenshotsListBackgroundRadioButton3->isChecked()) PrgSetup->SetScreenshotsListViewBackground(ui.ScreenshotsListBackgroundEdit->text());
    PrgSetup->SetScreenshotsListViewFontSize(ui.ScreenshotsListFontSizeEdit->value());
    PrgSetup->SetScreenshotsListViewFontColor(ColorToString(ui.ScreenshotsListFontColorBox->selectedTColor()));
    PrgSetup->SetScreenshotPreviewSize(ui.ScreenshotPreviewEdit->value());
}

void TSetupFrameGamesListScreenshotAppearance::ScreenshotsListBackgroundColorBoxChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.ScreenshotsListBackgroundRadioButton2->setChecked(true);
}

void TSetupFrameGamesListScreenshotAppearance::ScreenshotsListBackgroundEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.ScreenshotsListBackgroundRadioButton3->setChecked(true);
}

void TSetupFrameGamesListScreenshotAppearance::ScreenshotsListBackgroundButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S;
    ImageOpenDialog->Title = LanguageSetup->ScreenshotPopupImportTitle();
    ImageOpenDialog->Filter = LanguageSetup->ScreenshotPopupImportFilter();
    S = ui.ScreenshotsListBackgroundEdit->text();
    if (Trim(S).isEmpty()) S = IncludeTrailingPathDelimiter(*PBaseDir);
    S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
    ImageOpenDialog->InitialDir = ExtractFilePath(S);
    if (ImageOpenDialog->Execute()) {
        ui.ScreenshotsListBackgroundEdit->setText(MakeRelPath(ImageOpenDialog->FileName, IncludeTrailingPathDelimiter(*PBaseDir)));
    }
}
