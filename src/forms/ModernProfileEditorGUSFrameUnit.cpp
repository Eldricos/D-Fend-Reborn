// C++ port of ModernProfileEditorGUSFrameUnit.pas (implementation section).
#include "ModernProfileEditorGUSFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorGUSFrame::TModernProfileEditorGUSFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorGUSFrame::~TModernProfileEditorGUSFrame() = default;

// { TFrame1 }

void TModernProfileEditorGUSFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.ActivateGUSCheckBox->setText(LanguageSetup->ProfileEditorSoundGUSEnabled());
    ui.AddressLabel->setText(LanguageSetup->ProfileEditorSoundGUSAddress());
    St = ValueToList(InitData.GameDB->ConfOpt()->GUSBase(), ";,"); ui.AddressComboBox->addItems(St->ToQStringList()); delete St;
    ui.SampleRateLabel->setText(LanguageSetup->ProfileEditorSoundGUSRate());
    St = ValueToList(InitData.GameDB->ConfOpt()->GUSRate(), ";,"); ui.SampleRateComboBox->addItems(St->ToQStringList()); delete St;
    ui.Interrupt1Label->setText(LanguageSetup->ProfileEditorSoundGUSIRQ());
    St = ValueToList(InitData.GameDB->ConfOpt()->GUSIRQ(), ";,"); ui.Interrupt1ComboBox->addItems(St->ToQStringList()); delete St;
    ui.DMA1Label->setText(LanguageSetup->ProfileEditorSoundGUSDMA());
    St = ValueToList(InitData.GameDB->ConfOpt()->GUSDma(), ";,"); ui.DMA1ComboBox->addItems(St->ToQStringList()); delete St;
    ui.PathEdit_EditLabel->setText(LanguageSetup->ProfileEditorSoundGUSPath());

    AddDefaultValueHint(ui.AddressComboBox);
    AddDefaultValueHint(ui.SampleRateComboBox);
    AddDefaultValueHint(ui.Interrupt1ComboBox);
    AddDefaultValueHint(ui.DMA1ComboBox);

    setProperty("HelpContext", ID_ProfileEditSoundGUS);  // HelpContext:=...
}

static void SetComboBox(QComboBox* ComboBox, const QString& Value, int Default)
{
    QString S;
    if (Default >= 0 && Default < ComboBox->count()) ComboBox->setCurrentIndex(Default);
    S = Trim(ExtUpperCase(Value));
    for (int I = 0; I < ComboBox->count(); I++) if (Trim(ExtUpperCase(ComboBox->itemText(I))) == S) {
        ComboBox->setCurrentIndex(I); break;
    }
}

static void SetComboBox(QComboBox* ComboBox, const QString& Value, const QString& Default)
{
    SetComboBox(ComboBox, Default, 0);
    SetComboBox(ComboBox, Value, ComboBox->currentIndex());
}

void TModernProfileEditorGUSFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.ActivateGUSCheckBox->setChecked(Game->GUS());
    SetComboBox(ui.AddressComboBox, Game->GUSBase(), QStringLiteral("240"));
    SetComboBox(ui.SampleRateComboBox, IntToStr(Game->GUSRate()), QStringLiteral("22050"));
    SetComboBox(ui.Interrupt1ComboBox, IntToStr(Game->GUSIRQ()), QStringLiteral("5"));
    SetComboBox(ui.DMA1ComboBox, IntToStr(Game->GUSDMA()), QStringLiteral("1"));
    ui.PathEdit->setText(Game->GUSUltraDir());
}

void TModernProfileEditorGUSFrame::GetGame(TGame* Game)
{
    int I;
    Game->SetGUS(ui.ActivateGUSCheckBox->isChecked());
    Game->SetGUSBase(ui.AddressComboBox->currentText());
    if (TryStrToInt(ui.SampleRateComboBox->currentText(), I)) Game->SetGUSRate(I);
    if (TryStrToInt(ui.Interrupt1ComboBox->currentText(), I)) Game->SetGUSIRQ(I);
    if (TryStrToInt(ui.DMA1ComboBox->currentText(), I)) Game->SetGUSDMA(I);
    Game->SetGUSUltraDir(ui.PathEdit->text());
}
