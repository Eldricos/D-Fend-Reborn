// C++ port of ModernProfileEditorSoundBlasterFrameUnit.pas (implementation section).
#include "ModernProfileEditorSoundBlasterFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorSoundBlasterFrame::TModernProfileEditorSoundBlasterFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TModernProfileEditorSoundBlasterFrame::~TModernProfileEditorSoundBlasterFrame() = default;

// { TModernProfileEditorSoundBlasterFrame }

void TModernProfileEditorSoundBlasterFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.TypeLabel->setText(LanguageSetup->ProfileEditorSoundSBType());
    St = ValueToList(InitData.GameDB->ConfOpt()->Sblaster(), ";,"); ui.TypeComboBox->addItems(St->ToQStringList()); delete St;
    ui.AddressLabel->setText(LanguageSetup->ProfileEditorSoundSBAddress());
    St = ValueToList(InitData.GameDB->ConfOpt()->SBBase(), ";,"); ui.AddressComboBox->addItems(St->ToQStringList()); delete St;
    ui.InterruptLabel->setText(LanguageSetup->ProfileEditorSoundSBIRQ());
    St = ValueToList(InitData.GameDB->ConfOpt()->IRQ(), ";,"); ui.InterruptComboBox->addItems(St->ToQStringList()); delete St;
    ui.DMALabel->setText(LanguageSetup->ProfileEditorSoundSBDMA());
    St = ValueToList(InitData.GameDB->ConfOpt()->Dma(), ";,"); ui.DMAComboBox->addItems(St->ToQStringList()); delete St;
    ui.HDMALabel->setText(LanguageSetup->ProfileEditorSoundSBHDMA());
    St = ValueToList(InitData.GameDB->ConfOpt()->HDMA(), ";,"); ui.HDMAComboBox->addItems(St->ToQStringList()); delete St;
    ui.OplModeLabel->setText(LanguageSetup->ProfileEditorSoundSBOplMode());
    St = ValueToList(InitData.GameDB->ConfOpt()->Oplmode(), ";,"); ui.OplModeComboBox->addItems(St->ToQStringList()); delete St;
    ui.OplEmuLabel->setText(LanguageSetup->GameOplemu());
    St = ValueToList(InitData.GameDB->ConfOpt()->OplEmu(), ";,"); ui.OplEmuComboBox->addItems(St->ToQStringList()); delete St;
    ui.OplSampleRateLabel->setText(LanguageSetup->ProfileEditorSoundSBOplRate());
    St = ValueToList(InitData.GameDB->ConfOpt()->OPLRate(), ";,"); ui.OplSampleRateComboBox->addItems(St->ToQStringList()); delete St;
    ui.UseMixerCheckBox->setText(LanguageSetup->ProfileEditorSoundSBUseMixer());

    AddDefaultValueHint(ui.TypeComboBox);
    AddDefaultValueHint(ui.AddressComboBox);
    AddDefaultValueHint(ui.InterruptComboBox);
    AddDefaultValueHint(ui.DMAComboBox);
    AddDefaultValueHint(ui.HDMAComboBox);
    AddDefaultValueHint(ui.OplModeComboBox);
    AddDefaultValueHint(ui.OplEmuComboBox);
    AddDefaultValueHint(ui.OplSampleRateComboBox);

    setProperty("helpContext", ID_ProfileEditSoundSoundBlaster);  // HelpContext:=...
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

void TModernProfileEditorSoundBlasterFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    SetComboBox(ui.TypeComboBox, Game->SBType(), QStringLiteral("sb16"));
    SetComboBox(ui.AddressComboBox, Game->SBBase(), QStringLiteral("220"));
    SetComboBox(ui.InterruptComboBox, IntToStr(Game->SBIRQ()), QStringLiteral("7"));
    SetComboBox(ui.DMAComboBox, IntToStr(Game->SBDMA()), QStringLiteral("1"));
    SetComboBox(ui.HDMAComboBox, IntToStr(Game->SBHDMA()), QStringLiteral("5"));
    SetComboBox(ui.OplModeComboBox, Game->SBOplMode(), QStringLiteral("auto"));
    SetComboBox(ui.OplEmuComboBox, Game->SBOplEmu(), QStringLiteral("default"));
    SetComboBox(ui.OplSampleRateComboBox, IntToStr(Game->SBOplRate()), QStringLiteral("22050"));
    ui.UseMixerCheckBox->setChecked(Game->SBMixer());
}

void TModernProfileEditorSoundBlasterFrame::GetGame(TGame* Game)
{
    int I;
    Game->SetSBType(ui.TypeComboBox->currentText());
    Game->SetSBBase(ui.AddressComboBox->currentText());
    if (TryStrToInt(ui.InterruptComboBox->currentText(), I)) Game->SetSBIRQ(I);
    if (TryStrToInt(ui.DMAComboBox->currentText(), I)) Game->SetSBDMA(I);
    if (TryStrToInt(ui.HDMAComboBox->currentText(), I)) Game->SetSBHDMA(I);
    Game->SetSBOplMode(ui.OplModeComboBox->currentText());
    Game->SetSBOplEmu(ui.OplEmuComboBox->currentText());
    if (TryStrToInt(ui.OplSampleRateComboBox->currentText(), I)) Game->SetSBOplRate(I);
    Game->SetSBMixer(ui.UseMixerCheckBox->isChecked());
}
