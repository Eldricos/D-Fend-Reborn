// C++ port of SetupFrameToolbarUnit.pas (implementation section).
#include "SetupFrameToolbarUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "Dialogs.h"

namespace {
// TCheckListBox helpers
void AddCheckItem(QListWidget *L, const QString &S)
{
    auto *Item = new QListWidgetItem(S, L);
    Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
    Item->setCheckState(Qt::Unchecked);
}
bool IsChecked(QListWidget *L, int I) { return L->item(I)->checkState() == Qt::Checked; }
void SetChecked(QListWidget *L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
} // namespace

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameToolbar::TSetupFrameToolbar(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageOpenDialog = new TOpenDialog(this);
    ImageOpenDialog->DefaultExt = "jpeg";
    // DFM event wiring
    connect(ui.ToolbarImageButton, &QToolButton::clicked, this, [this]() { ToolbarImageButtonClick(ui.ToolbarImageButton); });
    connect(ui.ToolbarImageEdit, &QLineEdit::textChanged, this, [this]() { ToolbarImageEditChange(ui.ToolbarImageEdit); });
}

TSetupFrameToolbar::~TSetupFrameToolbar() = default;

// { TSetupFrameToolbar }

QString TSetupFrameToolbar::GetName()
{
    return LanguageSetup->SetupFormToolbar();
}

void TSetupFrameToolbar::InitGUIAndLoadSetup(TInitData &InitData)
{
    int I;
    QListWidget *L = ui.ButtonsListBox;
    PBaseDir = InitData.PBaseDir;
    ui.ShowToolbarCheckBox->setChecked(PrgSetup->ShowToolbar());
    ui.ShowToolbarCaptionsCheckBox->setChecked(PrgSetup->ShowToolbarTexts());
    if (L->count() == 0) for (I = 0; I <= 7; I++) AddCheckItem(L, QString());
    SetChecked(L, 0, PrgSetup->ShowToolbarButtonClose());
    SetChecked(L, 1, PrgSetup->ShowToolbarButtonRun());
    SetChecked(L, 2, PrgSetup->ShowToolbarButtonRunSetup());
    SetChecked(L, 3, PrgSetup->ShowToolbarButtonAdd());
    SetChecked(L, 4, PrgSetup->ShowToolbarButtonEdit());
    SetChecked(L, 5, PrgSetup->ShowToolbarButtonDelete());
    SetChecked(L, 6, PrgSetup->ShowSearchBox());
    SetChecked(L, 7, PrgSetup->ShowToolbarButtonHelp());
    ui.AddButtonFunctionComboBox->setCurrentIndex(Min(2, Max(0, PrgSetup->AddButtonFunction())));
    ui.ToolbarImageCheckBox->setChecked(!Trim(PrgSetup->ToolbarBackground()).isEmpty());
    ui.ToolbarImageEdit->setText(PrgSetup->ToolbarBackground());
    ui.ToolbarFontSizeEdit->setValue(PrgSetup->ToolbarFontSize());
    InitData.CloseCheckEvent = [this](bool &CloseOK) { CloseCheck(CloseOK); };
    UserIconLoader->DialogImage(DI_SelectFile, ui.ToolbarImageButton);
}

void TSetupFrameToolbar::LoadLanguage()
{
    int I;
    QListWidget *L = ui.ButtonsListBox;
    ui.ShowToolbarCheckBox->setText(LanguageSetup->SetupFormShowToolbar());
    ui.ShowToolbarCaptionsCheckBox->setText(LanguageSetup->SetupFormShowToolbarCaptions());
    ui.ButtonsLabel->setText(LanguageSetup->SetupFormShowToolbarButtons());
    if (L->count() == 0) for (I = 0; I <= 6; I++) AddCheckItem(L, QString());
    while (L->count() < 8) AddCheckItem(L, QString()); // Delphi would raise on Items[7] here
    L->item(0)->setText(RemoveUnderline(LanguageSetup->ButtonClose()));
    L->item(1)->setText(RemoveUnderline(LanguageSetup->ButtonRun()));
    L->item(2)->setText(RemoveUnderline(LanguageSetup->ButtonRunSetup()));
    L->item(3)->setText(RemoveUnderline(LanguageSetup->ButtonAdd()));
    L->item(4)->setText(RemoveUnderline(LanguageSetup->ButtonEdit()));
    L->item(5)->setText(RemoveUnderline(LanguageSetup->ButtonDelete()));
    L->item(6)->setText(RemoveUnderline(LanguageSetup->MenuViewShowSearchBox()));
    L->item(7)->setText(RemoveUnderline(LanguageSetup->ButtonHelp()));
    ui.AddButtonFunctionLabel->setText(LanguageSetup->SetupFormAddButtonFunctionLabel());
    I = ui.AddButtonFunctionComboBox->currentIndex();
    while (ui.AddButtonFunctionComboBox->count() < 3) ui.AddButtonFunctionComboBox->addItem(QString());
    ui.AddButtonFunctionComboBox->setItemText(0, LanguageSetup->SetupFormAddButtonFunctionAdd());
    ui.AddButtonFunctionComboBox->setItemText(1, LanguageSetup->SetupFormAddButtonFunctionWizard());
    ui.AddButtonFunctionComboBox->setItemText(2, LanguageSetup->SetupFormAddButtonFunctionMenu());
    ui.AddButtonFunctionComboBox->setCurrentIndex(I);
    ui.ToolbarImageCheckBox->setText(LanguageSetup->SetupFormBackgroundColorFile());
    ui.ToolbarImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ToolbarFontSizeLabel->setText(LanguageSetup->SetupFormFontSize());
    setProperty("HelpContext", ID_FileOptionsToolbar);
}

void TSetupFrameToolbar::DOSBoxDirChanged()
{
}

void TSetupFrameToolbar::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameToolbar::HideFrame()
{
}

void TSetupFrameToolbar::RestoreDefaults()
{
    QListWidget *L = ui.ButtonsListBox;
    ui.ShowToolbarCheckBox->setChecked(true);
    ui.ShowToolbarCaptionsCheckBox->setChecked(true);
    SetChecked(L, 0, false);
    SetChecked(L, 1, true);
    SetChecked(L, 2, false);
    SetChecked(L, 3, true);
    SetChecked(L, 4, true);
    SetChecked(L, 5, true);
    SetChecked(L, 6, true);
    SetChecked(L, 7, true);
    ui.AddButtonFunctionComboBox->setCurrentIndex(2);
    ui.ToolbarImageCheckBox->setChecked(false);
    ui.ToolbarImageEdit->setText(QString());
    ui.ToolbarFontSizeEdit->setValue(9);
}

void TSetupFrameToolbar::BeforeChangeLanguage()
{
}

void TSetupFrameToolbar::CloseCheck(bool &CloseOK)
{
    int I;
    for (I = 0; I < ui.ButtonsListBox->count(); I++) if (IsChecked(ui.ButtonsListBox, I)) return;
    MessageDlg(LanguageSetup->SetupFormShowToolbarButtonsError(), mtError, mbOK, 0, this);
    CloseOK = false;
}

void TSetupFrameToolbar::SaveSetup()
{
    QListWidget *L = ui.ButtonsListBox;
    PrgSetup->SetShowToolbar(ui.ShowToolbarCheckBox->isChecked());
    PrgSetup->SetShowToolbarTexts(ui.ShowToolbarCaptionsCheckBox->isChecked());
    PrgSetup->SetShowToolbarButtonClose(IsChecked(L, 0));
    PrgSetup->SetShowToolbarButtonRun(IsChecked(L, 1));
    PrgSetup->SetShowToolbarButtonRunSetup(IsChecked(L, 2));
    PrgSetup->SetShowToolbarButtonAdd(IsChecked(L, 3));
    PrgSetup->SetShowToolbarButtonEdit(IsChecked(L, 4));
    PrgSetup->SetShowToolbarButtonDelete(IsChecked(L, 5));
    PrgSetup->SetShowSearchBox(IsChecked(L, 6));
    PrgSetup->SetShowToolbarButtonHelp(IsChecked(L, 7));
    PrgSetup->SetAddButtonFunction(ui.AddButtonFunctionComboBox->currentIndex());
    if (ui.ToolbarImageCheckBox->isChecked()) PrgSetup->SetToolbarBackground(ui.ToolbarImageEdit->text()); else PrgSetup->SetToolbarBackground(QString());
    PrgSetup->SetToolbarFontSize(ui.ToolbarFontSizeEdit->value());
}

void TSetupFrameToolbar::ToolbarImageEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.ToolbarImageCheckBox->setChecked(true);
}

void TSetupFrameToolbar::ToolbarImageButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S;
    ImageOpenDialog->Title = LanguageSetup->ScreenshotPopupImportTitle();
    ImageOpenDialog->Filter = LanguageSetup->ScreenshotPopupImportFilter();
    S = ui.ToolbarImageEdit->text();
    if (Trim(S).isEmpty()) S = IncludeTrailingPathDelimiter(*PBaseDir);
    S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
    ImageOpenDialog->InitialDir = ExtractFilePath(S);
    if (ImageOpenDialog->Execute()) {
        ui.ToolbarImageEdit->setText(MakeRelPath(ImageOpenDialog->FileName, IncludeTrailingPathDelimiter(*PBaseDir)));
    }
}
