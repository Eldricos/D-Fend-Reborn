// C++ port of ModernProfileEditorMIDIFrameUnit.pas (implementation section).
#include "ModernProfileEditorMIDIFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "PrgSetupUnit.h"
#include "MIDITools.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorMIDIFrame::TModernProfileEditorMIDIFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.DeviceComboBox, &QComboBox::currentTextChanged, this, [this]() { DeviceComboBoxChange(ui.DeviceComboBox); });
    connect(ui.MIDISelectButton, &QPushButton::clicked, this, [this]() { MIDISelectButtonClick(ui.MIDISelectButton); });
    connect(ui.MIDISelectListBox, &QListWidget::itemClicked, this, [this]() { MIDISelectListBoxClick(ui.MIDISelectListBox); });
    connect(ui.MIDISelectListBox, &QListWidget::currentRowChanged, this, [this]() { MIDISelectListBoxClick(ui.MIDISelectListBox); });  // OnClick also fires on keyboard selection
}

TModernProfileEditorMIDIFrame::~TModernProfileEditorMIDIFrame() = default;

// { TModernProfileEditorMIDIFrame }

void TModernProfileEditorMIDIFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.InfoLabel->setText(LanguageSetup->ProfileEditorSoundMIDIInfo());
    ui.TypeLabel->setText(LanguageSetup->ProfileEditorSoundMIDIType());
    St = ValueToList(InitData.GameDB->ConfOpt()->MPU401(), ";,"); ui.TypeComboBox->addItems(St->ToQStringList()); delete St;
    ui.DeviceLabel->setText(LanguageSetup->ProfileEditorSoundMIDIDevice());
    St = ValueToList(InitData.GameDB->ConfOpt()->MIDIDevice(), ";,"); ui.DeviceComboBox->addItems(St->ToQStringList()); delete St;
    ui.AdditionalSettingsEdit_EditLabel->setText(LanguageSetup->ProfileEditorSoundMIDIConfigInfo());
    ui.MIDISelectButton->setText(LanguageSetup->ProfileEditorSoundMIDIConfigButton());
    ui.MIDISelectLabel1->setText(LanguageSetup->ProfileEditorSoundMIDIConfigButtonInfo());
    ui.MIDISelectLabel2->setText(LanguageSetup->ProfileEditorSoundMIDIConfigIDInfo());
    AddDefaultValueHint(ui.TypeComboBox);
    AddDefaultValueHint(ui.DeviceComboBox);

    ui.MT32SettingsGroupBox->setTitle(LanguageSetup->ProfileEditorSoundMIDIMT32());
    ui.MT32ModeLabel->setText(LanguageSetup->ProfileEditorSoundMIDIMT32Mode());
    ui.MT32TimeLabel->setText(LanguageSetup->ProfileEditorSoundMIDIMT32Time());
    ui.MT32LevelLabel->setText(LanguageSetup->ProfileEditorSoundMIDIMT32Level());

    St = ValueToList(InitData.GameDB->ConfOpt()->MT32ReverbMode(), ";,");
    ui.MT32ModeComboBox->clear();
    ui.MT32ModeComboBox->addItems(St->ToQStringList());
    delete St;

    St = ValueToList(InitData.GameDB->ConfOpt()->MT32ReverbTime(), ";,");
    ui.MT32TimeComboBox->clear();
    ui.MT32TimeComboBox->addItems(St->ToQStringList());
    delete St;

    St = ValueToList(InitData.GameDB->ConfOpt()->MT32ReverbLevel(), ";,");
    ui.MT32LevelComboBox->clear();
    ui.MT32LevelComboBox->addItems(St->ToQStringList());
    delete St;

    setProperty("HelpContext", ID_ProfileEditSoundMIDI);  // HelpContext:=...
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

// TStrings.IndexOf (case insensitive like the VCL)
static int ComboIndexOf(QComboBox* ComboBox, const QString& S)
{
    for (int I = 0; I < ComboBox->count(); I++) if (ComboBox->itemText(I).compare(S, Qt::CaseInsensitive) == 0) return I;
    return -1;
}

void TModernProfileEditorMIDIFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    SetComboBox(ui.TypeComboBox, Game->MIDIType(), QStringLiteral("intelligent"));
    SetComboBox(ui.DeviceComboBox, Game->MIDIDevice(), QStringLiteral("default"));
    ui.AdditionalSettingsEdit->setText(Game->MIDIConfig());

    ui.MT32ModeComboBox->setCurrentIndex(std::max(0, ComboIndexOf(ui.MT32ModeComboBox, Game->MIDIMT32Mode())));
    ui.MT32TimeComboBox->setCurrentIndex(std::max(0, ComboIndexOf(ui.MT32TimeComboBox, Game->MIDIMT32Time())));
    ui.MT32LevelComboBox->setCurrentIndex(std::max(0, ComboIndexOf(ui.MT32LevelComboBox, Game->MIDIMT32Level())));

    DeviceComboBoxChange(this);
}

void TModernProfileEditorMIDIFrame::DeviceComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.MT32SettingsGroupBox->setVisible(ExtUpperCase(ui.DeviceComboBox->currentText()) == "MT32");
}

void TModernProfileEditorMIDIFrame::GetGame(TGame* Game)
{
    Game->SetMIDIType(ui.TypeComboBox->currentText());
    Game->SetMIDIDevice(ui.DeviceComboBox->currentText());
    Game->SetMIDIConfig(ui.AdditionalSettingsEdit->text());

    Game->SetMIDIMT32Mode(ui.MT32ModeComboBox->currentText());
    Game->SetMIDIMT32Time(ui.MT32TimeComboBox->currentText());
    Game->SetMIDIMT32Level(ui.MT32LevelComboBox->currentText());
}

void TModernProfileEditorMIDIFrame::MIDISelectButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TStringList* St;

    St = GetMIDIDevices();
    ui.MIDISelectListBox->setVisible(St->Count() > 0);
    ui.MIDISelectLabel2->setVisible(St->Count() > 0);
    ui.MIDISelectListBox->clear();
    ui.MIDISelectListBox->addItems(St->ToQStringList());
    delete St;
}

void TModernProfileEditorMIDIFrame::MIDISelectListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (ui.MIDISelectListBox->currentRow() < 0) return;
    S = ui.MIDISelectListBox->item(ui.MIDISelectListBox->currentRow())->text();
    while ((!S.isEmpty()) && (S[S.length() - 1] != QChar('('))) S.chop(1);
    S.chop(1);
    ui.AdditionalSettingsEdit->setText(Trim(S));
}
