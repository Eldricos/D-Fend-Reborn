// C++ port of ModernProfileEditorInnovaFrameUnit.pas (implementation section).
#include "ModernProfileEditorInnovaFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorInnovaFrame::TModernProfileEditorInnovaFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorInnovaFrame::~TModernProfileEditorInnovaFrame() = default;

// { TModernProfileEditorInnovaFrame }

void TModernProfileEditorInnovaFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.EnableInnovaCheckBox->setText(LanguageSetup->ProfileEditorSoundInnovaEnable());
    ui.InnovaSampleRateLabel->setText(LanguageSetup->ProfileEditorSoundInnovaSampleRate());
    ui.InnovaBaseLabel->setText(LanguageSetup->ProfileEditorSoundInnovaBaseAddress());
    ui.InnovaQualityLabel->setText(LanguageSetup->ProfileEditorSoundInnovaQuality());

    St = ValueToList(InitData.GameDB->ConfOpt()->InnovaEmulationSampleRate(), ";,");
    ui.InnovaSampleRateComboBox->clear();
    ui.InnovaSampleRateComboBox->addItems(St->ToQStringList());
    delete St;

    St = ValueToList(InitData.GameDB->ConfOpt()->InnovaEmulationBaseAddress(), ";,");
    ui.InnovaBaseComboBox->clear();
    ui.InnovaBaseComboBox->addItems(St->ToQStringList());
    delete St;

    St = ValueToList(InitData.GameDB->ConfOpt()->InnovaEmulationQuality(), ";,");
    ui.InnovaQualityComboBox->clear();
    ui.InnovaQualityComboBox->addItems(St->ToQStringList());
    delete St;

    setProperty("helpContext", ID_ProfileEditSoundInnova);  // HelpContext:=...
}

void TModernProfileEditorInnovaFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;

    ui.EnableInnovaCheckBox->setChecked(Game->Innova());

    ui.InnovaSampleRateComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(IntToStr(Game->InnovaRate())));
    for (int I = 0; I < ui.InnovaSampleRateComboBox->count(); I++) if (Trim(ExtUpperCase(ui.InnovaSampleRateComboBox->itemText(I))) == S) {
        ui.InnovaSampleRateComboBox->setCurrentIndex(I); break;
    }

    ui.InnovaBaseComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(Game->InnovaBase()));
    for (int I = 0; I < ui.InnovaBaseComboBox->count(); I++) if (Trim(ExtUpperCase(ui.InnovaBaseComboBox->itemText(I))) == S) {
        ui.InnovaBaseComboBox->setCurrentIndex(I); break;
    }

    ui.InnovaQualityComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(IntToStr(Game->InnovaQuality())));
    for (int I = 0; I < ui.InnovaQualityComboBox->count(); I++) if (Trim(ExtUpperCase(ui.InnovaQualityComboBox->itemText(I))) == S) {
        ui.InnovaQualityComboBox->setCurrentIndex(I); break;
    }
}

void TModernProfileEditorInnovaFrame::GetGame(TGame* Game)
{
    int I;
    Game->SetInnova(ui.EnableInnovaCheckBox->isChecked());
    if (TryStrToInt(ui.InnovaSampleRateComboBox->itemText(ui.InnovaSampleRateComboBox->currentIndex()), I)) Game->SetInnovaRate(I);
    Game->SetInnovaBase(ui.InnovaBaseComboBox->itemText(ui.InnovaBaseComboBox->currentIndex()));
    if (TryStrToInt(ui.InnovaQualityComboBox->itemText(ui.InnovaQualityComboBox->currentIndex()), I)) Game->SetInnovaQuality(I);
}
