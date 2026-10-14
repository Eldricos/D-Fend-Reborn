// C++ port of SetupFrameGamesListAppearanceUnit.pas (implementation section).
#include "SetupFrameGamesListAppearanceUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"

// Control.Font.Style:=[...]
static void SetFontStyle(QWidget *W, bool Bold, bool Italic, bool Underline)
{
    QFont F = W->font();
    F.setBold(Bold);
    F.setItalic(Italic);
    F.setUnderline(Underline);
    W->setFont(F);
}

TSetupFrameGamesListAppearance::TSetupFrameGamesListAppearance(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageOpenDialog = new TOpenDialog(this);
    ImageOpenDialog->DefaultExt = "jpeg";

    // DFM event wiring
    connect(ui.GamesListBackgroundButton, &QToolButton::clicked, this, [this]() { GamesListBackgroundButtonClick(ui.GamesListBackgroundButton); });
    connect(ui.GamesListBackgroundColorBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { GamesListBackgroundColorBoxChange(ui.GamesListBackgroundColorBox); });
    connect(ui.GamesListBackgroundEdit, &QLineEdit::textChanged, this, [this]() { GamesListBackgroundEditChange(ui.GamesListBackgroundEdit); });
    connect(ui.NotSetEdit, &QLineEdit::textChanged, this, [this]() { NotSetEditChange(ui.NotSetEdit); });
}

TSetupFrameGamesListAppearance::~TSetupFrameGamesListAppearance()
{
    delete ImageOpenDialog;
}

/* TSetupFrameGamesListAppearance */

QString TSetupFrameGamesListAppearance::GetName()
{
    return LanguageSetup->SetupFormListViewSheet2();
}

void TSetupFrameGamesListAppearance::InitGUIAndLoadSetup(TInitData& InitData)
{
    QString S;
    TColor C;

    ui.WindowsExeIconsCheckBox->setVisible(true);

    PBaseDir = InitData.PBaseDir;

    // NoFlicker(...): not needed in Qt

    S = Trim(PrgSetup->GamesListViewBackground());
    if (S == "") ui.GamesListBackgroundRadioButton1->setChecked(true); else {
        try {
            C = StringToColor(S); ui.GamesListBackgroundRadioButton2->setChecked(true); ui.GamesListBackgroundColorBox->setSelectedTColor(C);
        } catch (...) {
            ui.GamesListBackgroundRadioButton3->setChecked(true); ui.GamesListBackgroundEdit->setText(S);
        }
    }
    S = Trim(PrgSetup->GamesListViewFontColor());
    if (S == "") ui.GamesListFontColorBox->setSelectedTColor(clWindowText); else {
        try { ui.GamesListFontColorBox->setSelectedTColor(StringToColor(S)); } catch (...) { ui.GamesListFontColorBox->setSelectedTColor(clWindowText); }
    }
    ui.GamesListFontSizeEdit->setValue(PrgSetup->GamesListViewFontSize());
    S = Trim(PrgSetup->ValueForNotSet());
    ui.NotSetRadioButton1->setChecked(S == "");
    ui.NotSetRadioButton2->setChecked(S == "-");
    ui.NotSetRadioButton3->setChecked((S != "") && (S != "-"));
    if (ui.NotSetRadioButton3->isChecked()) ui.NotSetEdit->setText(S);

    SetFontStyle(ui.NormalCheckBoxBold, true, false, false);
    SetFontStyle(ui.NormalCheckBoxItalic, false, true, false);
    SetFontStyle(ui.NormalCheckBoxUnderline, false, false, true);

    SetFontStyle(ui.CheckBoxBold, true, false, false);
    SetFontStyle(ui.CheckBoxItalic, false, true, false);
    SetFontStyle(ui.CheckBoxUnderline, false, false, true);

    // Font.Charset:=CharsetNameToFontCharSet(LanguageSetup.CharsetName): Qt uses Unicode fonts

    ui.NormalCheckBoxBold->setChecked(PrgSetup->NonFavoritesBold());
    ui.NormalCheckBoxItalic->setChecked(PrgSetup->NonFavoritesItalic());
    ui.NormalCheckBoxUnderline->setChecked(PrgSetup->NonFavoritesUnderline());

    ui.CheckBoxBold->setChecked(PrgSetup->FavoritesBold());
    ui.CheckBoxItalic->setChecked(PrgSetup->FavoritesItalic());
    ui.CheckBoxUnderline->setChecked(PrgSetup->FavoritesUnderline());

    ui.WindowsExeIconsCheckBox->setChecked(PrgSetup->UseWindowsExeIcons());
    ui.ShowTooltipsCheckBox->setChecked(PrgSetup->ShowTooltips());
    ui.NoteInTooltipEdit->setValue(qMin(100, qMax(1, PrgSetup->NoteLinesInTooltips())));

    ui.RestoreSelectedCheckBox->setChecked(PrgSetup->RestoreLastSelectedProfile());

    UserIconLoader->DialogImage(DI_SelectFile, ui.GamesListBackgroundButton);
}

void TSetupFrameGamesListAppearance::BeforeChangeLanguage()
{
}

void TSetupFrameGamesListAppearance::LoadLanguage()
{
    bool GermanColorNames;

    GermanColorNames = (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "DEUTSCH.INI") || (ExtUpperCase(ExtractFileName(LanguageSetup->SetupFile())) == "GERMAN.INI");
    ui.GamesListBackgroundColorBox->setPrettyNames(GermanColorNames);
    ui.GamesListFontColorBox->setPrettyNames(GermanColorNames);

    ui.GamesListBackgroundRadioButton1->setText(LanguageSetup->SetupFormBackgroundColorDefault());
    ui.GamesListBackgroundRadioButton2->setText(LanguageSetup->SetupFormBackgroundColorColor());
    ui.GamesListBackgroundRadioButton3->setText(LanguageSetup->SetupFormBackgroundColorFile());
    ui.GamesListBackgroundButton->setToolTip(LanguageSetup->ChooseFile());
    ui.GamesListFontSizeLabel->setText(LanguageSetup->SetupFormFontSize());
    ui.GamesListFontColorLabel->setText(LanguageSetup->SetupFormFontColor());
    ui.NotSetGroupBox->setTitle(LanguageSetup->ValueForNotSet() + " \"" + LanguageSetup->NotSet() + "\"");
    ui.NotSetRadioButton1->setText("\"" + LanguageSetup->NotSet() + "\"");

    ui.NormalEntriesGroupBox->setTitle(LanguageSetup->FontStyleForNonFavorites());
    ui.NormalCheckBoxBold->setText(LanguageSetup->FontStyleForFavoritesBold());
    ui.NormalCheckBoxItalic->setText(LanguageSetup->FontStyleForFavoritesItalic());
    ui.NormalCheckBoxUnderline->setText(LanguageSetup->FontStyleForFavoritesUnderline());
    ui.FavoriteGroupBox->setTitle(LanguageSetup->FontStyleForFavorites());
    ui.CheckBoxBold->setText(LanguageSetup->FontStyleForFavoritesBold());
    ui.CheckBoxItalic->setText(LanguageSetup->FontStyleForFavoritesItalic());
    ui.CheckBoxUnderline->setText(LanguageSetup->FontStyleForFavoritesUnderline());

    ui.WindowsExeIconsCheckBox->setText(LanguageSetup->SetupFormUseWindowsExeIcons());
    ui.ShowTooltipsCheckBox->setText(LanguageSetup->MenuViewShowTooltips());
    ui.NoteInTooltipLabel->setText(LanguageSetup->SetupFormNoteLinesInTooltips());
    ui.RestoreSelectedCheckBox->setText(LanguageSetup->SetupFormRestoreSelectedProfile());

    setProperty("HelpContext", ID_FileOptionsAppearanceList);
}

void TSetupFrameGamesListAppearance::DOSBoxDirChanged()
{
}

void TSetupFrameGamesListAppearance::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameGamesListAppearance::HideFrame()
{
}

void TSetupFrameGamesListAppearance::RestoreDefaults()
{
    QString S;

    ui.GamesListBackgroundColorBox->setSelectedTColor(clBlack);
    ui.GamesListBackgroundEdit->setText("");
    ui.GamesListBackgroundRadioButton1->setChecked(true);

    ui.GamesListFontSizeEdit->setValue(9);
    ui.GamesListFontColorBox->setSelectedTColor(clBlack);

    ui.NotSetEdit->setText("");
    S = Trim(LanguageSetup->NotSetLanguageDefault());
    if (S == "") ui.NotSetRadioButton1->setChecked(true); else {
        if (S == "-") ui.NotSetRadioButton2->setChecked(true); else {
            ui.NotSetRadioButton3->setChecked(true);
            ui.NotSetEdit->setText(S);
        }
    }

    ui.NormalCheckBoxBold->setChecked(false);
    ui.NormalCheckBoxItalic->setChecked(false);
    ui.NormalCheckBoxUnderline->setChecked(false);
    ui.CheckBoxBold->setChecked(true);
    ui.CheckBoxItalic->setChecked(false);
    ui.CheckBoxUnderline->setChecked(false);

    ui.WindowsExeIconsCheckBox->setChecked(true);
    ui.ShowTooltipsCheckBox->setChecked(true);
    ui.NoteInTooltipEdit->setValue(5);
    ui.RestoreSelectedCheckBox->setChecked(false);
}

void TSetupFrameGamesListAppearance::SaveSetup()
{
    if (ui.GamesListBackgroundRadioButton1->isChecked()) PrgSetup->SetGamesListViewBackground("");
    if (ui.GamesListBackgroundRadioButton2->isChecked()) PrgSetup->SetGamesListViewBackground(ColorToString(ui.GamesListBackgroundColorBox->selectedTColor()));
    if (ui.GamesListBackgroundRadioButton3->isChecked()) PrgSetup->SetGamesListViewBackground(ui.GamesListBackgroundEdit->text());
    PrgSetup->SetGamesListViewFontSize(ui.GamesListFontSizeEdit->value());
    PrgSetup->SetGamesListViewFontColor(ColorToString(ui.GamesListFontColorBox->selectedTColor()));
    if (ui.NotSetRadioButton1->isChecked()) PrgSetup->SetValueForNotSet("");
    if (ui.NotSetRadioButton2->isChecked()) PrgSetup->SetValueForNotSet("-");
    if (ui.NotSetRadioButton3->isChecked()) PrgSetup->SetValueForNotSet(ui.NotSetEdit->text());
    PrgSetup->SetNonFavoritesBold(ui.NormalCheckBoxBold->isChecked());
    PrgSetup->SetNonFavoritesItalic(ui.NormalCheckBoxItalic->isChecked());
    PrgSetup->SetNonFavoritesUnderline(ui.NormalCheckBoxUnderline->isChecked());
    PrgSetup->SetFavoritesBold(ui.CheckBoxBold->isChecked());
    PrgSetup->SetFavoritesItalic(ui.CheckBoxItalic->isChecked());
    PrgSetup->SetFavoritesUnderline(ui.CheckBoxUnderline->isChecked());
    PrgSetup->SetUseWindowsExeIcons(ui.WindowsExeIconsCheckBox->isChecked());
    PrgSetup->SetShowTooltips(ui.ShowTooltipsCheckBox->isChecked());
    PrgSetup->SetNoteLinesInTooltips(ui.NoteInTooltipEdit->value());
    PrgSetup->SetRestoreLastSelectedProfile(ui.RestoreSelectedCheckBox->isChecked());
}

void TSetupFrameGamesListAppearance::GamesListBackgroundColorBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.GamesListBackgroundRadioButton2->setChecked(true);
}

void TSetupFrameGamesListAppearance::GamesListBackgroundEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.GamesListBackgroundRadioButton3->setChecked(true);
}

void TSetupFrameGamesListAppearance::GamesListBackgroundButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    ImageOpenDialog->Title = LanguageSetup->ScreenshotPopupImportTitle();
    ImageOpenDialog->Filter = LanguageSetup->ScreenshotPopupImportFilter();
    S = ui.GamesListBackgroundEdit->text();
    if (Trim(S) == "") S = IncludeTrailingPathDelimiter(*PBaseDir);
    S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
    ImageOpenDialog->InitialDir = ExtractFilePath(S);
    if (ImageOpenDialog->Execute()) {
        ui.GamesListBackgroundEdit->setText(MakeRelPath(ImageOpenDialog->FileName, IncludeTrailingPathDelimiter(*PBaseDir)));
    }
}

void TSetupFrameGamesListAppearance::NotSetEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.NotSetRadioButton3->setChecked(true);
}
