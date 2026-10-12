// C++ port of ModernProfileEditorScummVMGameFrameUnit.pas (implementation section).
#include "ModernProfileEditorScummVMGameFrameUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "ScummVMToolsUnit.h"
#include "HelpConsts.h"

/* TModernProfileEditorScummVMGameFrame */

TModernProfileEditorScummVMGameFrame::TModernProfileEditorScummVMGameFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorScummVMGameFrame::~TModernProfileEditorScummVMGameFrame()
{
}

void TModernProfileEditorScummVMGameFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.OnShowFrame = [this](QObject* Sender) { ShowFrame(Sender); };

    LastGameName = "";

    // NoFlicker(...): not needed with Qt

    ui.AltIntroCheckBox->setText(LanguageSetup->ProfileEditorScummVMAltIntro());
    ui.GFXDetailsEditLabel->setText(LanguageSetup->ProfileEditorScummVMGFXDetails());
    ui.ObjectLabelsCheckBox->setText(LanguageSetup->ProfileEditorScummVMObjectLabels());
    ui.ReverseStereoCheckBox->setText(LanguageSetup->ProfileEditorScummVMReverseStereo());
    ui.MusicMuteCheckBox->setText(LanguageSetup->ProfileEditorScummVMMusicMute());
    ui.SFXMuteCheckBox->setText(LanguageSetup->ProfileEditorScummVMSFXMute());
    ui.WalkspeedEditLabel->setText(LanguageSetup->ProfileEditorScummVMWalkspeed());
    ui.AltIntroLabel->setText(GameNames({"sky", "queen"}));
    ui.GFXDetailsLabel->setText(GameNames({"sword2"}));
    ui.ObjectLabelsLabel->setText(GameNames({"sword2"}));
    ui.ReverseStereoLabel->setText(GameNames({"sword2"}));
    ui.MusicMuteLabel->setText(GameNames({"sword2", "queen", "simon1", "simon2"}));
    ui.SFXMuteLabel->setText(GameNames({"sword2", "queen", "simon1", "simon2"}));
    ui.WalkspeedLabel->setText(GameNames({"kyra1"}));

    CurrentGameName = InitData.CurrentScummVMGameName;
    EditingTemplate = InitData.EditingTemplate;

    setProperty("helpContext", ID_ProfileEditScummVMGame);  // HelpContext
}

QString TModernProfileEditorScummVMGameFrame::GameNames(const QList<QString>& Games)
{
    QString result;
    int I, Nr;

    result = "";
    for (I = 0; I < Games.size(); I++) {
        Nr = ScummVMGamesList->NamesList()->IndexOf(LowerCase(Games[I]));
        if (Nr < 0) continue;
        if (result == "")
            result = "(\"" + (*ScummVMGamesList->DescriptionList())[Nr] + "\"";
        else
            result = result + ", \"" + (*ScummVMGamesList->DescriptionList())[Nr] + "\"";
    }
    if (result != "") result = result + ")";
    return result;
}

void TModernProfileEditorScummVMGameFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.AltIntroCheckBox->setChecked(Game->ScummVMAltIntro());
    ui.GFXDetailsEdit->setValue(Max(0, Min(3, Game->ScummVMGFXDetails())));
    ui.ObjectLabelsCheckBox->setChecked(Game->ScummVMObjectLabels());
    ui.ReverseStereoCheckBox->setChecked(Game->ScummVMReverseStereo());
    ui.MusicMuteCheckBox->setChecked(Game->ScummVMMusicMute());
    ui.SFXMuteCheckBox->setChecked(Game->ScummVMSFXMute());
    ui.WalkspeedEdit->setValue(Max(0, Min(4, Game->ScummVMWalkspeed())));
}

void TModernProfileEditorScummVMGameFrame::ShowFrame(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastGameName == LowerCase(*CurrentGameName)) return;
    LastGameName = LowerCase(*CurrentGameName);

    ui.AltIntroCheckBox->setEnabled(EditingTemplate || LastGameName == "sky" || LastGameName == "queen");
    ui.GFXDetailsEdit->setEnabled(EditingTemplate || LastGameName == "sword2");
    ui.ObjectLabelsCheckBox->setEnabled(EditingTemplate || LastGameName == "sword2");
    ui.ReverseStereoCheckBox->setEnabled(EditingTemplate || LastGameName == "sword2");
    ui.MusicMuteCheckBox->setEnabled(EditingTemplate || LastGameName == "sword2" || LastGameName == "queen" || LastGameName == "simon1" || LastGameName == "simon2");
    ui.SFXMuteCheckBox->setEnabled(EditingTemplate || LastGameName == "sword2" || LastGameName == "queen" || LastGameName == "simon1" || LastGameName == "simon2");
    ui.WalkspeedEdit->setEnabled(EditingTemplate || LastGameName == "kyra1");
}

void TModernProfileEditorScummVMGameFrame::GetGame(TGame* Game)
{
    Game->SetScummVMAltIntro(ui.AltIntroCheckBox->isChecked());
    Game->SetScummVMGFXDetails(ui.GFXDetailsEdit->value());
    Game->SetScummVMObjectLabels(ui.ObjectLabelsCheckBox->isChecked());
    Game->SetScummVMReverseStereo(ui.ReverseStereoCheckBox->isChecked());
    Game->SetScummVMMusicMute(ui.MusicMuteCheckBox->isChecked());
    Game->SetScummVMSFXMute(ui.SFXMuteCheckBox->isChecked());
    Game->SetScummVMWalkspeed(ui.WalkspeedEdit->value());
}
