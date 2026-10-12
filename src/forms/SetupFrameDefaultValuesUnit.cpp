// C++ port of SetupFrameDefaultValuesUnit.pas (implementation section).
#include "SetupFrameDefaultValuesUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"

namespace {

// Emulates TComboBox.OnDropDown: called before the list of the combo box opens.
class TDropDownHook : public QObject
{
public:
    TDropDownHook(QComboBox *Combo, std::function<void()> Handler) : QObject(Combo), FHandler(std::move(Handler))
    {
        Combo->installEventFilter(this);
    }
    bool eventFilter(QObject *, QEvent *E) override
    {
        if (E->type() == QEvent::MouseButtonPress) FHandler();
        if (E->type() == QEvent::KeyPress) {
            auto *K = static_cast<QKeyEvent *>(E);
            if (K->key() == Qt::Key_F4 || K->key() == Qt::Key_Space ||
                ((K->modifiers() & Qt::AltModifier) && (K->key() == Qt::Key_Down || K->key() == Qt::Key_Up))) FHandler();
        }
        return false;
    }
private:
    std::function<void()> FHandler;
};

// Memo.Lines <-> TStringList
void MemoToList(QTextEdit *Memo, TStringList *St)
{
    St->Clear();
    QString S = Memo->toPlainText();
    if (S.isEmpty()) return;
    const QStringList L = S.split('\n');
    for (const QString &Line : L) St->Add(Line);
}

void ListToMemo(TStringList *St, QTextEdit *Memo)
{
    Memo->setPlainText(St->ToQStringList().join('\n'));
}

}  // namespace

TSetupFrameDefaultValues::TSetupFrameDefaultValues(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.DefaultValueComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { DefaultValueComboBoxChange(ui.DefaultValueComboBox); });
    new TDropDownHook(ui.DefaultValueComboBox, [this]() { DefaultValueComboBoxDropDown(ui.DefaultValueComboBox); });
    connect(ui.DefaultValueSpeedButton, &QPushButton::clicked, this, [this]() { DefaultValueSpeedButtonClick(ui.DefaultValueSpeedButton); });
    connect(ui.PopupThisValue, &QAction::triggered, this, [this]() { PopupMenuWork(ui.PopupThisValue); });
    connect(ui.PopupAllValues, &QAction::triggered, this, [this]() { PopupMenuWork(ui.PopupAllValues); });

    LastIndex = -1;
}

TSetupFrameDefaultValues::~TSetupFrameDefaultValues()
{
    for (int I = 0; I < DefaultValueLists.count(); I++) delete DefaultValueLists[I];
}

QString TSetupFrameDefaultValues::GetName()
{
    return LanguageSetup->SetupFormDefaultValueSheet();
}

void TSetupFrameDefaultValues::InitGUIAndLoadSetup(TInitData& InitData)
{
    GameDB = InitData.GameDB;

    // NoFlicker(DefaultValueComboBox): not needed in Qt

    SetRichEditPopup(ui.DefaultValueMemo);

    UserIconLoader->DialogImage(DI_ResetDefault, ui.DefaultValueSpeedButton);
}

void TSetupFrameDefaultValues::BeforeChangeLanguage()
{
}

void TSetupFrameDefaultValues::LoadLanguage()
{
    auto AddString = [this](const QString &Lang, const QString &S) {
        ui.DefaultValueComboBox->addItem(Lang);
        DefaultValueLists.append(ValueToList(S, ";,"));
    };
    int I, J;
    QString P1, P2, S1, S2, S3, S4;

    ui.DefaultValueLabel->setText(LanguageSetup->SetupFormDefaultValueLabel());
    ui.DefaultValueSpeedButton->setText(LanguageSetup->SetupFormDefaultValueReset());
    {
        QFont F = ui.DefaultValueMemo->font();
        F.setFamily("Courier New");
        F.setStyleHint(QFont::Monospace);
        ui.DefaultValueMemo->setFont(F);
    }
    ui.PopupThisValue->setText(LanguageSetup->SetupFormDefaultValueResetThis());
    ui.PopupAllValues->setText(LanguageSetup->SetupFormDefaultValueResetAll());

    I = ui.DefaultValueComboBox->currentIndex();
    ui.DefaultValueComboBox->setCurrentIndex(-1);
    DefaultValueComboBoxChange(this);

    for (J = 0; J < DefaultValueLists.count(); J++) delete DefaultValueLists[J];
    DefaultValueLists.clear();
    ui.DefaultValueComboBox->clear();

    P1 = "(DOSBox) ";
    P2 = "(ScummVM) ";
    S1 = "SoundBlaster ";
    S2 = "GUS ";
    S3 = "MIDI ";
    S4 = "NE2000 ";

    AddString(P1+LanguageSetup->GameCore(), GameDB->ConfOpt()->Core());
    AddString(P1+LanguageSetup->GameCPUType(), GameDB->ConfOpt()->CPUType());
    AddString(P1+LanguageSetup->GameCycles(), GameDB->ConfOpt()->Cycles());
    AddString(P1+LanguageSetup->GameCyclesDown(), GameDB->ConfOpt()->CyclesDown());
    AddString(P1+LanguageSetup->GameCyclesUp(), GameDB->ConfOpt()->CyclesUp());
    AddString(P1+LanguageSetup->GameVideo(), GameDB->ConfOpt()->Video());
    AddString(P1+LanguageSetup->GameMemory(), GameDB->ConfOpt()->Memory());
    AddString(P1+LanguageSetup->GameFrameskip(), GameDB->ConfOpt()->Frameskip());
    AddString(P1+LanguageSetup->GameWindowResolution(), GameDB->ConfOpt()->ResolutionWindow());
    AddString(P1+LanguageSetup->GameFullscreenResolution(), GameDB->ConfOpt()->ResolutionFullscreen());
    AddString(P1+LanguageSetup->GameScale(), GameDB->ConfOpt()->Scale());
    AddString(P1+LanguageSetup->GameRender(), GameDB->ConfOpt()->Render());
    AddString(P1+LanguageSetup->GameVGAChipset(), GameDB->ConfOpt()->VGAChipsets());
    AddString(P1+LanguageSetup->GameVideoRam(), GameDB->ConfOpt()->VGAVideoRAM());
    AddString(P1+LanguageSetup->GameGlideEmulation(), GameDB->ConfOpt()->GlideEmulation());
    AddString(P1+LanguageSetup->GameGlideEmulationPort(), GameDB->ConfOpt()->GlideEmulationPort());
    AddString(P1+LanguageSetup->GameGlideEmulationLFB(), GameDB->ConfOpt()->GlideEmulationLFB());
    AddString(P1+LanguageSetup->GameKeyboardLayout(), GameDB->ConfOpt()->KeyboardLayout());
    AddString(P1+LanguageSetup->GameKeyboardCodepage(), GameDB->ConfOpt()->Codepage());
    AddString(P1+LanguageSetup->GameMouseSensitivity(), GameDB->ConfOpt()->MouseSensitivity());
    AddString(P1+LanguageSetup->GameReportedDOSVersion(), GameDB->ConfOpt()->ReportedDOSVersion());
    AddString(P1+LanguageSetup->ProfileEditorSoundSampleRate(), GameDB->ConfOpt()->Rate());
    AddString(P1+LanguageSetup->ProfileEditorSoundBlockSize(), GameDB->ConfOpt()->Blocksize());
    AddString(P1+LanguageSetup->GameSblaster(), GameDB->ConfOpt()->Sblaster());
    AddString(P1+S1+LanguageSetup->ProfileEditorSoundSBAddress(), GameDB->ConfOpt()->SBBase());
    AddString(P1+S1+LanguageSetup->ProfileEditorSoundSBIRQ(), GameDB->ConfOpt()->IRQ());
    AddString(P1+S1+LanguageSetup->ProfileEditorSoundSBDMA(), GameDB->ConfOpt()->Dma());
    AddString(P1+S1+LanguageSetup->ProfileEditorSoundSBHDMA(), GameDB->ConfOpt()->HDMA());
    AddString(P1+LanguageSetup->GameOplmode(), GameDB->ConfOpt()->Oplmode());
    AddString(P1+LanguageSetup->GameOplemu(), GameDB->ConfOpt()->OplEmu());
    AddString(P1+LanguageSetup->ProfileEditorSoundSBOplRate(), GameDB->ConfOpt()->OPLRate());
    AddString(P1+S2+LanguageSetup->ProfileEditorSoundGUSAddress(), GameDB->ConfOpt()->GUSBase());
    AddString(P1+S2+LanguageSetup->ProfileEditorSoundGUSRate(), GameDB->ConfOpt()->GUSRate());
    AddString(P1+S2+LanguageSetup->ProfileEditorSoundGUSIRQ(), GameDB->ConfOpt()->GUSIRQ());
    AddString(P1+S2+LanguageSetup->ProfileEditorSoundGUSDMA(), GameDB->ConfOpt()->GUSDma());
    AddString(P1+LanguageSetup->ProfileEditorSoundMIDIDevice(), GameDB->ConfOpt()->MIDIDevice());
    AddString(P1+S3+LanguageSetup->ProfileEditorSoundMIDIType(), GameDB->ConfOpt()->MPU401());
    AddString(P1+S3+LanguageSetup->ProfileEditorSoundMIDIMT32()+" "+LanguageSetup->ProfileEditorSoundMIDIMT32Mode(), GameDB->ConfOpt()->MT32ReverbMode());
    AddString(P1+S3+LanguageSetup->ProfileEditorSoundMIDIMT32()+" "+LanguageSetup->ProfileEditorSoundMIDIMT32Time(), GameDB->ConfOpt()->MT32ReverbTime());
    AddString(P1+S3+LanguageSetup->ProfileEditorSoundMIDIMT32()+" "+LanguageSetup->ProfileEditorSoundMIDIMT32Level(), GameDB->ConfOpt()->MT32ReverbLevel());
    AddString(P1+LanguageSetup->ProfileEditorSoundInnovaSampleRate(), GameDB->ConfOpt()->InnovaEmulationSampleRate());
    AddString(P1+LanguageSetup->ProfileEditorSoundInnovaBaseAddress(), GameDB->ConfOpt()->InnovaEmulationBaseAddress());
    AddString(P1+LanguageSetup->ProfileEditorSoundInnovaQuality(), GameDB->ConfOpt()->InnovaEmulationQuality());
    AddString(P1+LanguageSetup->ProfileEditorSoundMiscPCSpeakerRate(), GameDB->ConfOpt()->PCRate());
    AddString(P1+LanguageSetup->ProfileEditorSoundMiscTandyRate(), GameDB->ConfOpt()->TandyRate());
    AddString(P1+LanguageSetup->GameJoysticks(), GameDB->ConfOpt()->Joysticks());
    AddString(P1+S4+LanguageSetup->GameNE2000BaseAddress(), GameDB->ConfOpt()->NE2000EmulationBaseAddress());
    AddString(P1+S4+LanguageSetup->GameNE2000Interrupt(), GameDB->ConfOpt()->NE2000EmulationInterrupt());
    AddString(P2+LanguageSetup->ProfileEditorScummVMPlatform(), GameDB->ConfOpt()->ScummVMPlatform());
    AddString(P2+LanguageSetup->ProfileEditorScummVMLanguageLong(), GameDB->ConfOpt()->ScummVMLanguages());
    AddString(P2+LanguageSetup->ProfileEditorScummVMRenderMode(), GameDB->ConfOpt()->ScummVMRenderMode());
    AddString(P2+LanguageSetup->ProfileEditorScummVMFilter(), GameDB->ConfOpt()->ScummVMFilter());
    AddString(P2+LanguageSetup->ProfileEditorScummVMMusicDriver(), GameDB->ConfOpt()->ScummVMMusicDriver());

    // SetLength(DefaultValueLists,...): DefaultValueLists already holds the objects of the combo box items

    ui.DefaultValueComboBox->setCurrentIndex(qMax(0, I));
    DefaultValueComboBoxChange(this);

    setProperty("HelpContext", ID_FileOptionsDefaultValues);
}

void TSetupFrameDefaultValues::DOSBoxDirChanged()
{
}

void TSetupFrameDefaultValues::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameDefaultValues::HideFrame()
{
}

void TSetupFrameDefaultValues::RestoreDefaults()
{
    PopupMenuWork(ui.PopupAllValues);
}

void TSetupFrameDefaultValues::SaveSetup()
{
    int I;
    auto GetString = [this, &I]() { QString result = ListToValue(DefaultValueLists[I], ','); I++; return result; };

    DefaultValueComboBoxChange(this);

    I = 0;
    GameDB->ConfOpt()->SetCore(GetString());
    GameDB->ConfOpt()->SetCPUType(GetString());
    GameDB->ConfOpt()->SetCycles(GetString());
    GameDB->ConfOpt()->SetCyclesDown(GetString());
    GameDB->ConfOpt()->SetCyclesUp(GetString());
    GameDB->ConfOpt()->SetVideo(GetString());
    GameDB->ConfOpt()->SetMemory(GetString());
    GameDB->ConfOpt()->SetFrameskip(GetString());
    GameDB->ConfOpt()->SetResolutionWindow(GetString());
    GameDB->ConfOpt()->SetResolutionFullscreen(GetString());
    GameDB->ConfOpt()->SetScale(GetString());
    GameDB->ConfOpt()->SetRender(GetString());
    GameDB->ConfOpt()->SetVGAChipsets(GetString());
    GameDB->ConfOpt()->SetVGAVideoRAM(GetString());
    GameDB->ConfOpt()->SetGlideEmulation(GetString());
    GameDB->ConfOpt()->SetGlideEmulationPort(GetString());
    GameDB->ConfOpt()->SetGlideEmulationLFB(GetString());
    GameDB->ConfOpt()->SetKeyboardLayout(GetString());
    GameDB->ConfOpt()->SetCodepage(GetString());
    GameDB->ConfOpt()->SetMouseSensitivity(GetString());
    GameDB->ConfOpt()->SetReportedDOSVersion(GetString());
    GameDB->ConfOpt()->SetRate(GetString());
    GameDB->ConfOpt()->SetBlocksize(GetString());
    GameDB->ConfOpt()->SetSblaster(GetString());
    GameDB->ConfOpt()->SetSBBase(GetString());
    GameDB->ConfOpt()->SetIRQ(GetString());
    GameDB->ConfOpt()->SetDma(GetString());
    GameDB->ConfOpt()->SetHDMA(GetString());
    GameDB->ConfOpt()->SetOplmode(GetString());
    GameDB->ConfOpt()->SetOplEmu(GetString());
    GameDB->ConfOpt()->SetOPLRate(GetString());
    GameDB->ConfOpt()->SetGUSBase(GetString());
    GameDB->ConfOpt()->SetGUSRate(GetString());
    GameDB->ConfOpt()->SetGUSIRQ(GetString());
    GameDB->ConfOpt()->SetGUSDma(GetString());
    GameDB->ConfOpt()->SetMIDIDevice(GetString());
    GameDB->ConfOpt()->SetMPU401(GetString());
    GameDB->ConfOpt()->SetMT32ReverbMode(GetString());
    GameDB->ConfOpt()->SetMT32ReverbTime(GetString());
    GameDB->ConfOpt()->SetMT32ReverbLevel(GetString());
    GameDB->ConfOpt()->SetInnovaEmulationSampleRate(GetString());
    GameDB->ConfOpt()->SetInnovaEmulationBaseAddress(GetString());
    GameDB->ConfOpt()->SetInnovaEmulationQuality(GetString());
    GameDB->ConfOpt()->SetPCRate(GetString());
    GameDB->ConfOpt()->SetTandyRate(GetString());
    GameDB->ConfOpt()->SetJoysticks(GetString());
    GameDB->ConfOpt()->SetNE2000EmulationBaseAddress(GetString());
    GameDB->ConfOpt()->SetNE2000EmulationInterrupt(GetString());
    GameDB->ConfOpt()->SetScummVMPlatform(GetString());
    GameDB->ConfOpt()->SetScummVMLanguages(GetString());
    GameDB->ConfOpt()->SetScummVMRenderMode(GetString());
    GameDB->ConfOpt()->SetScummVMFilter(GetString());
    GameDB->ConfOpt()->SetScummVMMusicDriver(GetString());
}

void TSetupFrameDefaultValues::DefaultValueComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastIndex >= 0) {
        DefaultValueLists[LastIndex]->Clear();
        MemoToList(ui.DefaultValueMemo, DefaultValueLists[LastIndex]);
    }

    LastIndex = ui.DefaultValueComboBox->currentIndex();
    ui.DefaultValueMemo->clear();
    if (LastIndex < 0) return;
    ListToMemo(DefaultValueLists[LastIndex], ui.DefaultValueMemo);

    SetComboHint(ui.DefaultValueComboBox);
}

void TSetupFrameDefaultValues::DefaultValueComboBoxDropDown(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetComboDropDownDropDownWidth(ui.DefaultValueComboBox);
}

void TSetupFrameDefaultValues::DefaultValueSpeedButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QPoint P = mapToGlobal(ui.DefaultValueSpeedButton->pos());
    ui.DefaultValuePopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TSetupFrameDefaultValues::PopupMenuWork(QObject* Sender)
{
    bool All;
    int I, J;
    auto Work = [this, &I, &J, &All](const QString &S) {
        if ((I != J) && (!All)) { J++; return; }
        delete DefaultValueLists[J];
        DefaultValueLists[J] = ValueToList(S, ";,");
        J++;
    };

    All = (Sender->property("tag").toInt() == 1);

    if (LastIndex < 0) LastIndex = ui.DefaultValueComboBox->currentIndex();

    I = LastIndex; LastIndex = -1; J = 0;

    Work(DefaultValuesCore);
    Work(DefaultValuesCPUType);
    Work(DefaultValueCycles);
    Work(DefaultValuesCyclesDown);
    Work(DefaultValuesCyclesUp);
    Work(DefaultValuesVideo);
    Work(DefaultValuesMemory);
    Work(DefaultValuesFrameSkip);
    Work(DefaultValuesResolutionWindow);
    Work(DefaultValuesResolutionFullscreen);
    Work(DefaultValuesScale);
    Work(DefaultValueRender);
    Work(DefaultValuesVGAChipsets);
    Work(DefaultValuesVGAVideoRAM);
    Work(DefaultValuesGlideEmulation);
    Work(DefaultValuesGlideEmulationPort);
    Work(DefaultValuesGlideEmulationLFB);
    Work(DefaultValuesKeyboardLayout);
    Work(DefaultValuesCodepage);
    Work(DefaultValuesMouseSensitivity);
    Work(DefaultValuesReportedDOSVersion);
    Work(DefaultValuesRate);
    Work(DefaultValuesBlocksize);
    Work(DefaultValuesSBlaster);
    Work(DefaultValuesSBBase);
    Work(DefaultValuesIRQ);
    Work(DefaultValuesDMA);
    Work(DefaultValuesHDMA);
    Work(DefaultValuesOPLModes);
    Work(DefaultValuesOplEmu);
    Work(DefaultValuesOPLRate);
    Work(DefaultValuesGUSBase);
    Work(DefaultValuesGUSRate);
    Work(DefaultValuesIRQ1);
    Work(DefaultValuesDMA1);
    Work(DefaultValuesMIDIDevice);
    Work(DefaultValuesMPU401);
    Work(DefaultValuesMT32ReverbMode);
    Work(DefaultValuesMT32ReverbTime);
    Work(DefaultValuesMT32ReverbLevel);
    Work(DefaultValuesInnovaEmulationSampleRate);
    Work(DefaultValuesInnovaEmulationBaseAddress);
    Work(DefaultValuesInnovaEmulationQuality);
    Work(DefaultValuesPCRate);
    Work(DefaultValuesTandyRate);
    Work(DefaultValuesJoysticks);
    Work(DefaultValuesNE2000EmulationBaseAddress);
    Work(DefaultValuesNE2000EmulationInterrupt);
    Work(DefaultValuesScummVMPlatform);
    Work(DefaultValuesScummVMLanguages);
    Work(DefaultValuesScummVMRenderMode);
    Work(DefaultValuesScummVMFilter);
    Work(DefaultValuesScummVMMusicDriver);

    ui.DefaultValueComboBox->setCurrentIndex(I); DefaultValueComboBoxChange(Sender);
}
