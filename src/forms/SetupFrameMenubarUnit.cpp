// C++ port of SetupFrameMenubarUnit.pas (implementation section).
#include "SetupFrameMenubarUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "SetupFrameToolbarUnit.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameMenubar::TSetupFrameMenubar(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TSetupFrameMenubar::~TSetupFrameMenubar() = default;

// { TSetupFrameMenubar }

QString TSetupFrameMenubar::GetName()
{
    return LanguageSetup->SetupFormShowMenubar();
}

void TSetupFrameMenubar::InitGUIAndLoadSetup(TInitData &InitData)
{
    ui.ShowMenubarCheckBox->setChecked(PrgSetup->ShowMainMenu());
    InitData.CloseCheckEvent = [this](bool &CloseOK) { CloseCheck(CloseOK); };
    FGetFrame = InitData.GetFrame;
}

void TSetupFrameMenubar::BeforeChangeLanguage()
{
}

void TSetupFrameMenubar::LoadLanguage()
{
    ui.ShowMenubarCheckBox->setText(LanguageSetup->MenuViewShowMenubar());
    ui.ShowMenubarLabel->setText(LanguageSetup->MenuViewShowMenubarRestoreInfo());
    setProperty("HelpContext", ID_FileOptionsMenubar);
}

void TSetupFrameMenubar::DOSBoxDirChanged()
{
}

void TSetupFrameMenubar::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameMenubar::HideFrame()
{
}

void TSetupFrameMenubar::RestoreDefaults()
{
    ui.ShowMenubarCheckBox->setChecked(true);
}

void TSetupFrameMenubar::CloseCheck(bool &CloseOK)
{
    TSetupFrameToolbar *F;
    if (!ui.ShowMenubarCheckBox->isChecked()) {
        // Delphi: F:=FGetFrame(TSetupFrameToolbar) as TSetupFrameToolbar;
        // A std::function based "class reference" cannot be compared, so the
        // toolbar frame is looked up among the siblings in the setup dialog.
        F = window()->findChild<TSetupFrameToolbar *>();
        if (F != nullptr && !F->ui.ShowToolbarCheckBox->isChecked()) {
            MessageDlg(LanguageSetup->SetupFormShowMenubarError(), mtError, mbOK, 0, this);
            CloseOK = false;
        }
    }
}

void TSetupFrameMenubar::SaveSetup()
{
    PrgSetup->SetShowMainMenu(ui.ShowMenubarCheckBox->isChecked());
}
