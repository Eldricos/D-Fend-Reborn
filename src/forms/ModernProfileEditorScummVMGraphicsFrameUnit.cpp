// C++ port of ModernProfileEditorScummVMGraphicsFrameUnit.pas (implementation section).
#include "ModernProfileEditorScummVMGraphicsFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

/* TModernProfileEditorScummVMGraphicsFrame */

TModernProfileEditorScummVMGraphicsFrame::TModernProfileEditorScummVMGraphicsFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorScummVMGraphicsFrame::~TModernProfileEditorScummVMGraphicsFrame()
{
}

void TModernProfileEditorScummVMGraphicsFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    // NoFlicker(...): not needed with Qt

    ui.FilterLabel->setText(LanguageSetup->ProfileEditorScummVMFilter());
    St = ValueToList(InitData.GameDB->ConfOpt()->ScummVMFilter(), ";,"); ui.FilterComboBox->addItems(St->ToQStringList()); delete St;
    ui.RenderModeLabel->setText(LanguageSetup->ProfileEditorScummVMRenderMode());
    St = ValueToList(InitData.GameDB->ConfOpt()->ScummVMRenderMode(), ";,"); ui.RenderModeComboBox->addItems(St->ToQStringList()); delete St;

    AddDefaultValueHint(ui.FilterComboBox);
    AddDefaultValueHint(ui.RenderModeComboBox);

    ui.StartFullscreenCheckBox->setText(LanguageSetup->GameStartFullscreen());
    ui.KeepAspectRatioCheckBox->setText(LanguageSetup->GameAspectCorrection());

    setProperty("helpContext", ID_ProfileEditGraphics);  // HelpContext
}

void TModernProfileEditorScummVMGraphicsFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S, T;
    int I;

    S = Trim(ExtUpperCase(Game->ScummVMFilter()));
    ui.FilterComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.FilterComboBox->count(); I++) {
        T = Trim(ExtUpperCase(ui.FilterComboBox->itemText(I)));
        if (Pos("(", T) == 0) continue;
        T = Copy(T, Pos("(", T) + 1, MaxInt);
        if (Pos(")", T) == 0) continue;
        T = Copy(T, 1, Pos(")", T) - 1);
        if (Trim(T) == S) { ui.FilterComboBox->setCurrentIndex(I); break; }
    }

    S = Trim(ExtUpperCase(Game->ScummVMRenderMode()));
    ui.RenderModeComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.RenderModeComboBox->count(); I++) {
        T = Trim(ExtUpperCase(ui.RenderModeComboBox->itemText(I)));
        if (Pos("(", T) == 0) {
            if (Trim(T) == S) { ui.RenderModeComboBox->setCurrentIndex(I); break; }
        } else {
            T = Copy(T, Pos("(", T) + 1, MaxInt);
            if (Pos(")", T) == 0) continue;
            T = Copy(T, 1, Pos(")", T) - 1);
            if (Trim(T) == S) { ui.RenderModeComboBox->setCurrentIndex(I); break; }
        }
    }

    ui.StartFullscreenCheckBox->setChecked(Game->StartFullscreen());
    ui.KeepAspectRatioCheckBox->setChecked(Game->AspectCorrection());
}

void TModernProfileEditorScummVMGraphicsFrame::GetGame(TGame* Game)
{
    QString S;

    S = ui.FilterComboBox->currentText();
    if (Pos("(", S) == 0) Game->SetScummVMFilter(""); else {
        S = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) Game->SetScummVMFilter(""); else Game->SetScummVMFilter(Copy(S, 1, Pos(")", S) - 1));
    }

    S = ui.RenderModeComboBox->currentText();
    if (Pos("(", S) == 0) Game->SetScummVMRenderMode(S); else {
        S = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) Game->SetScummVMRenderMode(ui.RenderModeComboBox->currentText()); else Game->SetScummVMRenderMode(Copy(S, 1, Pos(")", S) - 1));
    }

    Game->SetStartFullscreen(ui.StartFullscreenCheckBox->isChecked());
    Game->SetAspectCorrection(ui.KeepAspectRatioCheckBox->isChecked());
}
