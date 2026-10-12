// C++ port of ModernProfileEditorSoundFrameUnit.pas (implementation section).
#include "ModernProfileEditorSoundFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorSoundFrame::TModernProfileEditorSoundFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorSoundFrame::~TModernProfileEditorSoundFrame() = default;

// { TModernProfileEditorSoundFrame }

void TModernProfileEditorSoundFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    InitData.OnCheckValue = [this](QObject* Sender, bool& OK) { CheckValue(Sender, OK); };

    ui.ActivateSoundCheckBox->setText(LanguageSetup->ProfileEditorSoundEnableSound());
    ui.MixerGroupBox->setTitle(LanguageSetup->ProfileEditorSoundMixer());
    ui.SampleRateLabel->setText(LanguageSetup->ProfileEditorSoundSampleRate());
    St = ValueToList(InitData.GameDB->ConfOpt()->Rate(), ";,"); ui.SampleRateComboBox->addItems(St->ToQStringList()); delete St;
    ui.BlockSizeLabel->setText(LanguageSetup->ProfileEditorSoundBlockSize());
    St = ValueToList(InitData.GameDB->ConfOpt()->Blocksize(), ";,"); ui.BlockSizeComboBox->addItems(St->ToQStringList()); delete St;
    ui.PreBufferLabel->setText(LanguageSetup->ProfileEditorSoundPrebuffer());
    ui.PreBufferComboBox->addItems(QStringList{"1", "5", "10", "15", "20", "25", "30"});

    ui.ActivatePCSpeakerCheckBox->setText(LanguageSetup->ProfileEditorSoundMiscEnablePCSpeaker());
    ui.PCSpeakerSampleRateLabel->setText(LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate());
    St = ValueToList(InitData.GameDB->ConfOpt()->PCRate(), ";,"); ui.PCSpeakerSampleRateComboBox->addItems(St->ToQStringList()); delete St;

    ui.TandyRadioGroup->setTitle(LanguageSetup->ProfileEditorSoundMiscEnableTandy());
    ui.TandyRadioGroup->setItemText(0, LanguageSetup->ProfileEditorSoundMiscEnableTandyAuto());
    ui.TandyRadioGroup->setItemText(1, LanguageSetup->On());
    ui.TandyRadioGroup->setItemText(2, LanguageSetup->Off());

    ui.TandySampleRateLabel->setText(LanguageSetup->ProfileEditorSoundMiscTandyRate());
    St = ValueToList(InitData.GameDB->ConfOpt()->TandyRate(), ";,"); ui.TandyComboBox->addItems(St->ToQStringList()); delete St;

    ui.ActivateDisneyCheckBox->setText(LanguageSetup->ProfileEditorSoundMiscEnableDisneySoundsSource());

    AddDefaultValueHint(ui.SampleRateComboBox);
    AddDefaultValueHint(ui.BlockSizeComboBox);

    setProperty("helpContext", ID_ProfileEditSound);  // HelpContext:=...
}

void TModernProfileEditorSoundFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;

    ui.ActivateSoundCheckBox->setChecked(!Game->MixerNosound());
    ui.SampleRateComboBox->setCurrentIndex(ui.SampleRateComboBox->count() - 2);
    for (int I = 0; I < ui.SampleRateComboBox->count(); I++) if (IntToStr(Game->MixerRate()) == ui.SampleRateComboBox->itemText(I)) {
        ui.SampleRateComboBox->setCurrentIndex(I); break;
    }
    FOldSampleRate = Game->MixerRate();
    ui.BlockSizeComboBox->setCurrentText(IntToStr(Game->MixerBlocksize()));
    FOldBlockSize = Game->MixerBlocksize();
    ui.PreBufferComboBox->setCurrentText(IntToStr(Game->MixerPrebuffer()));
    FOldPreBuffer = Game->MixerPrebuffer();

    ui.ActivatePCSpeakerCheckBox->setChecked(Game->SpeakerPC());
    ui.PCSpeakerSampleRateComboBox->setCurrentIndex(ui.PCSpeakerSampleRateComboBox->count() - 2);
    for (int I = 0; I < ui.PCSpeakerSampleRateComboBox->count(); I++) if (IntToStr(Game->SpeakerRate()) == ui.PCSpeakerSampleRateComboBox->itemText(I)) {
        ui.PCSpeakerSampleRateComboBox->setCurrentIndex(I); break;
    }
    FOldSpeakerRate = Game->SpeakerRate();

    S = Trim(ExtUpperCase(Game->SpeakerTandy()));
    ui.TandyRadioGroup->setItemIndex(0);
    if ((S == "AUTO") || (S == "DEFAULT") || (S.isEmpty())) ui.TandyRadioGroup->setItemIndex(0);
    if (S == "ON") ui.TandyRadioGroup->setItemIndex(1);
    if (S == "OFF") ui.TandyRadioGroup->setItemIndex(2);
    ui.TandyComboBox->setCurrentIndex(ui.TandyComboBox->count() - 2);
    for (int I = 0; I < ui.TandyComboBox->count(); I++) if (IntToStr(Game->SpeakerTandyRate()) == ui.TandyComboBox->itemText(I)) {
        ui.TandyComboBox->setCurrentIndex(I); break;
    }
    FOldTandyRate = Game->SpeakerTandyRate();

    ui.ActivateDisneyCheckBox->setChecked(Game->SpeakerDisney());
}

void TModernProfileEditorSoundFrame::CheckValue(QObject* Sender, bool& OK)
{
    Q_UNUSED(Sender);
    int I;

    OK = true;

    if ((!TryStrToInt(Trim(ui.SampleRateComboBox->currentText()), I)) || (I < 1) || (I > 65536)) {
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.SampleRateComboBox->currentText(), LanguageSetup->ProfileEditorSoundSampleRate(), IntToStr(FOldSampleRate)}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            OK = false; return;
        }
    }

    if ((!TryStrToInt(Trim(ui.BlockSizeComboBox->currentText()), I)) || (I < 1) || (I > 65536)) {
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.BlockSizeComboBox->currentText(), LanguageSetup->ProfileEditorSoundBlockSize(), IntToStr(FOldBlockSize)}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            OK = false; return;
        }
    }

    if ((!TryStrToInt(Trim(ui.PreBufferComboBox->currentText()), I)) || (I < 1) || (I > 65536)) {
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.PreBufferComboBox->currentText(), LanguageSetup->ProfileEditorSoundPrebuffer(), IntToStr(FOldPreBuffer)}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            OK = false; return;
        }
    }

    if ((!TryStrToInt(Trim(ui.PCSpeakerSampleRateComboBox->currentText()), I)) || (I < 1) || (I > 65536)) {
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.PCSpeakerSampleRateComboBox->currentText(), LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate(), IntToStr(FOldSpeakerRate)}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            OK = false; return;
        }
    }

    if ((!TryStrToInt(Trim(ui.TandyComboBox->currentText()), I)) || (I < 1) || (I > 65536)) {
        if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.TandyComboBox->currentText(), LanguageSetup->ProfileEditorSoundMiscTandyRate(), IntToStr(FOldTandyRate)}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            OK = false; return;
        }
    }
}

void TModernProfileEditorSoundFrame::GetGame(TGame* Game)
{
    int I;

    Game->SetMixerNosound(!ui.ActivateSoundCheckBox->isChecked());
    if (TryStrToInt(Trim(ui.SampleRateComboBox->currentText()), I) && (I >= 1) && (I <= 65536)) Game->SetMixerRate(I);
    if (TryStrToInt(Trim(ui.BlockSizeComboBox->currentText()), I) && (I >= 1) && (I <= 65536)) Game->SetMixerBlocksize(I);
    if (TryStrToInt(Trim(ui.PreBufferComboBox->currentText()), I) && (I >= 1) && (I <= 65536)) Game->SetMixerPrebuffer(I);

    Game->SetSpeakerPC(ui.ActivatePCSpeakerCheckBox->isChecked());
    if (TryStrToInt(Trim(ui.PCSpeakerSampleRateComboBox->currentText()), I) && (I >= 1) && (I <= 65536)) Game->SetSpeakerRate(I);

    switch (ui.TandyRadioGroup->itemIndex()) {
        case 0: Game->SetSpeakerTandy("auto"); break;
        case 1: Game->SetSpeakerTandy("on"); break;
        case 2: Game->SetSpeakerTandy("off"); break;
    }
    if (TryStrToInt(Trim(ui.TandyComboBox->currentText()), I) && (I >= 1) && (I <= 65536)) Game->SetSpeakerTandyRate(I);

    Game->SetSpeakerDisney(ui.ActivateDisneyCheckBox->isChecked());
}
