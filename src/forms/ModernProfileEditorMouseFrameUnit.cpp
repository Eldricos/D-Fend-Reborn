// C++ port of ModernProfileEditorMouseFrameUnit.pas (implementation section).
#include "ModernProfileEditorMouseFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

// Label.Font.Color:=Color
static void SetFontColor(QWidget* W, TColor Color)
{
    QPalette P = W->palette();
    P.setColor(QPalette::WindowText, TColorToQColor(Color));
    P.setColor(QPalette::Text, TColorToQColor(Color));
    W->setPalette(P);
}

TModernProfileEditorMouseFrame::TModernProfileEditorMouseFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorMouseFrame::~TModernProfileEditorMouseFrame() = default;

// { TModernProfileEditorMouseFrame }

void TModernProfileEditorMouseFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);

    ui.LockMouseCheckBox->setText(LanguageSetup->GameAutoLockMouse());
    ui.LockMouseLabel->setText(LanguageSetup->GameAutoLockMouseInfo());
    ui.MouseSensitivityLabel->setText(LanguageSetup->GameMouseSensitivity());
    ui.Force2ButtonsCheckBox->setText(LanguageSetup->GameForce2ButtonMouseMode());
    ui.Force2ButtonsInfoLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetFontColor(ui.Force2ButtonsInfoLabel, clGrayText);
    } else {
        SetFontColor(ui.Force2ButtonsInfoLabel, clRed);
    }
    ui.SwapButtonsCheckBox->setText(LanguageSetup->GameSwapMouseButtons());
    ui.SwapButtonsInfoLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetFontColor(ui.SwapButtonsInfoLabel, clGrayText);
    } else {
        SetFontColor(ui.SwapButtonsInfoLabel, clRed);
    }

    setProperty("helpContext", ID_ProfileEditMouse);  // HelpContext:=...
}

void TModernProfileEditorMouseFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.LockMouseCheckBox->setChecked(Game->AutoLockMouse());
    ui.MouseSensitivityEdit->setValue(Game->MouseSensitivity());
    ui.Force2ButtonsCheckBox->setChecked(Game->Force2ButtonMouseMode());
    ui.SwapButtonsCheckBox->setChecked(Game->SwapMouseButtons());
}

void TModernProfileEditorMouseFrame::GetGame(TGame* Game)
{
    Game->SetAutoLockMouse(ui.LockMouseCheckBox->isChecked());
    Game->SetMouseSensitivity(std::min(1000, std::max(1, ui.MouseSensitivityEdit->value())));
    Game->SetForce2ButtonMouseMode(ui.Force2ButtonsCheckBox->isChecked());
    Game->SetSwapMouseButtons(ui.SwapButtonsCheckBox->isChecked());
}
