// C++ port of ModernProfileEditorScummVMSoundFrameUnit.pas (implementation section).
#include "ModernProfileEditorScummVMSoundFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

/* TModernProfileEditorScummVMSoundFrame */

TModernProfileEditorScummVMSoundFrame::TModernProfileEditorScummVMSoundFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorScummVMSoundFrame::~TModernProfileEditorScummVMSoundFrame()
{
}

void TModernProfileEditorScummVMSoundFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    // NoFlicker(...): not needed with Qt

    ui.MusicVolumeLabel->setText(LanguageSetup->ProfileEditorScummVMMusicVolume());
    ui.SpeechVolumeLabel->setText(LanguageSetup->ProfileEditorScummVMSpeechVolume());
    ui.SFXVolumeLabel->setText(LanguageSetup->ProfileEditorScummVMSFXVolume());
    ui.MIDIGainLabel->setText(LanguageSetup->ProfileEditorScummVMMIDIGain());
    ui.OutputRateLabel->setText(LanguageSetup->ProfileEditorSoundSampleRate());
    ui.MusicDriverLabel->setText(LanguageSetup->ProfileEditorScummVMMusicDriver());
    ui.NativeMT32CheckBox->setText(LanguageSetup->ProfileEditorScummVMNativeMT32());
    ui.EnableGSCheckBox->setText(LanguageSetup->ProfileEditorScummVMEnableGS());
    ui.MultiMIDICheckBox->setText(LanguageSetup->ProfileEditorScummVMMultiMIDI());
    ui.SpeechMuteCheckBox->setText(LanguageSetup->ProfileEditorScummVMSpeechMute());

    ui.OutputRateComboBox->addItem("11025"); ui.OutputRateComboBox->addItem("22050"); ui.OutputRateComboBox->addItem("44100");
    St = ValueToList(InitData.GameDB->ConfOpt()->ScummVMMusicDriver(), ";,"); ui.MusicDriverComboBox->addItems(St->ToQStringList()); delete St;

    AddDefaultValueHint(ui.MusicDriverComboBox);

    setProperty("HelpContext", ID_ProfileEditSound);  // HelpContext
}

void TModernProfileEditorScummVMSoundFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S, T;
    int I, J;

    ui.MusicVolumeEdit->setValue(Max(0, Min(255, Game->ScummVMMusicVolume())));
    ui.SpeechVolumeEdit->setValue(Max(0, Min(255, Game->ScummVMSpeechVolume())));
    ui.SFXVolumeEdit->setValue(Max(0, Min(255, Game->ScummVMSFXVolume())));
    ui.MIDIGainEdit->setValue(Max(0, Min(1000, Game->ScummVMMIDIGain())));
    switch (Game->ScummVMSampleRate()) {
    case 11025: ui.OutputRateComboBox->setCurrentIndex(0); break;
    case 22050: ui.OutputRateComboBox->setCurrentIndex(1); break;
    case 44100: ui.OutputRateComboBox->setCurrentIndex(2); break;
    default: ui.OutputRateComboBox->setCurrentIndex(1); break;
    }
    if (ui.MusicDriverComboBox->count() > 1) ui.MusicDriverComboBox->setCurrentIndex(1);
    S = Trim(ExtUpperCase(Game->ScummVMMusicDriver()));
    if (S != "" && S[Length(S) - 1] == ')') S.chop(1);
    for (I = 0; I < ui.MusicDriverComboBox->count(); I++) {
        T = Trim(ui.MusicDriverComboBox->itemText(I));
        if (T != "" && T[Length(T) - 1] == ')') {
            for (J = Length(T) - 1; J >= 1; J--) if (T[J - 1] == '(') { T = Trim(Copy(T, J + 1, MaxInt)); break; }
        }
        if (T != "" && T[Length(T) - 1] == ')') T.chop(1);
        if (ExtUpperCase(T) == S) { ui.MusicDriverComboBox->setCurrentIndex(I); break; }
    }
    ui.NativeMT32CheckBox->setChecked(Game->ScummVMNativeMT32());
    ui.EnableGSCheckBox->setChecked(Game->ScummVMEnableGS());
    ui.MultiMIDICheckBox->setChecked(Game->ScummVMMultiMIDI());
    ui.SpeechMuteCheckBox->setChecked(Game->ScummVMSpeechMute());
}

void TModernProfileEditorScummVMSoundFrame::GetGame(TGame* Game)
{
    QString S;
    int I;

    Game->SetScummVMMusicVolume(ui.MusicVolumeEdit->value());
    Game->SetScummVMSpeechVolume(ui.SpeechVolumeEdit->value());
    Game->SetScummVMSFXVolume(ui.SFXVolumeEdit->value());
    Game->SetScummVMMIDIGain(ui.MIDIGainEdit->value());
    switch (ui.OutputRateComboBox->currentIndex()) {
    case 0: Game->SetScummVMSampleRate(11025); break;
    case 1: Game->SetScummVMSampleRate(22050); break;
    case 2: Game->SetScummVMSampleRate(44100); break;
    }

    S = Trim(ui.MusicDriverComboBox->currentText());
    if (S != "" && S[Length(S) - 1] == ')') {
        for (I = Length(S) - 1; I >= 1; I--) if (S[I - 1] == '(') { S = Trim(Copy(S, I + 1, MaxInt)); break; }
    }
    if (S != "" && S[Length(S) - 1] == ')') S.chop(1);
    Game->SetScummVMMusicDriver(S);

    Game->SetScummVMNativeMT32(ui.NativeMT32CheckBox->isChecked());
    Game->SetScummVMEnableGS(ui.EnableGSCheckBox->isChecked());
    Game->SetScummVMMultiMIDI(ui.MultiMIDICheckBox->isChecked());
    Game->SetScummVMSpeechMute(ui.SpeechMuteCheckBox->isChecked());
}
