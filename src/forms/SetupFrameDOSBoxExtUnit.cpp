// C++ port of SetupFrameDOSBoxExtUnit.pas (implementation section).
#include "SetupFrameDOSBoxExtUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "Process.h"

namespace {

// TLabel.OnClick
class TClickHook : public QObject
{
public:
    TClickHook(QWidget *W, std::function<void()> Handler) : QObject(W), FHandler(std::move(Handler)) { W->installEventFilter(this); }
    bool eventFilter(QObject *, QEvent *E) override
    {
        if (E->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(E)->button() == Qt::LeftButton) FHandler();
        return false;
    }
private:
    std::function<void()> FHandler;
};

}  // namespace

TSetupFrameDOSBoxExt::TSetupFrameDOSBoxExt(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    for (bool &B : ValueChanged) B = false;

    // DFM event wiring (TCheckBox.OnClick is also fired by assignments to Checked)
    new TClickHook(ui.InfoURLLabel, [this]() { InfoURLLabelClick(ui.InfoURLLabel); });
    connect(ui.RenderModesCheckBox, &QCheckBox::stateChanged, this, [this]() { DefaultValueChanged(ui.RenderModesCheckBox); });
    // AllowGrayed=False: the grayed state can only be set by the program
    connect(ui.RenderModesCheckBox, &QCheckBox::clicked, this, [this]() { ui.RenderModesCheckBox->setTristate(false); });
    connect(ui.VideoModesCheckBox, &QCheckBox::stateChanged, this, [this]() { DefaultValueChanged(ui.VideoModesCheckBox); });
    connect(ui.PixelShaderCheckBox, &QCheckBox::stateChanged, this, [this]() { DefaultValueChanged(ui.PixelShaderCheckBox); });
    connect(ui.MIDICheckBox, &QCheckBox::stateChanged, this, [this]() { DefaultValueChanged(ui.MIDICheckBox); });
}

TSetupFrameDOSBoxExt::~TSetupFrameDOSBoxExt()
{
}

/* TSetupFrameDOSBoxExt */

QString TSetupFrameDOSBoxExt::GetName()
{
    return LanguageSetup->SetupFormDOSBoxCVSSheet();
}

void TSetupFrameDOSBoxExt::InitGUIAndLoadSetup(TInitData& InitData)
{
    int I;

    GameDB = InitData.GameDB;

    // NoFlicker(...): not needed in Qt

    ui.MultiFloppyImageCheckBox->setChecked(PrgSetup->AllowMultiFloppyImagesMount());
    ui.PhysFSCheckBox->setChecked(PrgSetup->AllowPhysFSUsage());
    ui.ExtendedTextModeCheckBox->setChecked(PrgSetup->AllowTextModeLineChange());
    ui.GlideEmulationCheckBox->setChecked(PrgSetup->AllowGlideSettings());
    ui.VGAChipsetCheckBox->setChecked(PrgSetup->AllowVGAChipsetSettings());

    I = 0;
    if (DefaultValueOnList(GameDB->ConfOpt()->Render(), "openglhq")) I++;
    if (DefaultValueOnList(GameDB->ConfOpt()->Render(), "direct3d")) I++;
    if (I == 1) ui.RenderModesCheckBox->setCheckState(Qt::PartiallyChecked); else ui.RenderModesCheckBox->setChecked(I == 2);
    ValueChanged[0] = false;

    ui.VideoModesCheckBox->setChecked(DefaultValueOnList(GameDB->ConfOpt()->Video(), "demovga"));
    ValueChanged[1] = false;

    ui.PixelShaderCheckBox->setChecked(PrgSetup->AllowPixelShader());
    ValueChanged[3] = false;

    ui.MIDICheckBox->setChecked(DefaultValueOnList(GameDB->ConfOpt()->MIDIDevice(), "mt32"));
    ValueChanged[2] = false;
    ui.InnovaCheckBox->setChecked(PrgSetup->AllowInnova());

    ui.PrinterCheckBox->setChecked(PrgSetup->AllowPrinterSettings());

    ui.NE2000CheckBox->setChecked(PrgSetup->AllowNe2000());
}

void TSetupFrameDOSBoxExt::DefaultValueChanged(QObject* Sender)
{
    int Tag = Sender->property("tag").toInt();
    if (Tag >= 0 && Tag <= 3) ValueChanged[Tag] = true;
}

bool TSetupFrameDOSBoxExt::DefaultValueOnList(const QString& List, const QString& Value)
{
    TStringList* St;
    int I;
    QString S;
    bool result = false;

    S = Trim(ExtUpperCase(Value));
    St = ValueToList(List, ";,");
    for (I = 0; I < St->Count(); I++) if (Trim(ExtUpperCase((*St)[I])) == S) { result = true; break; }
    delete St;
    return result;
}

void TSetupFrameDOSBoxExt::BeforeChangeLanguage()
{
}

void TSetupFrameDOSBoxExt::LoadLanguage()
{
    ui.WarningLabel->setText(LanguageSetup->SetupFormDOSBoxCVSWarning());
    ui.MountDialogGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxCVSMountGroup());
    ui.MultiFloppyImageCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSMultipleFloppyImages());
    ui.PhysFSCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSPhysFS());
    ui.GraphicsGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxCVSGraphicsGroup());
    ui.ExtendedTextModeCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSExtendedTextMode());
    ui.GlideEmulationCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSGlideEmulation());
    ui.VGAChipsetCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSVGAChipset());
    ui.RenderModesCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSRenderModes());
    ui.VideoModesCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSVideoModes());
    ui.PixelShaderCheckBox->setText(LanguageSetup->SetupFormDosBoxCVSPixelShader());
    ui.SoundGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxCVSSoundGroup());
    ui.MIDICheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSMidiModes());
    ui.InnovaCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSInnova());
    ui.PrinterGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxCVSPrinterGroup());
    ui.PrinterCheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSPrinter());
    ui.NetworkGroupBox->setTitle(LanguageSetup->SetupFormDOSBoxCVSNetworkGroup());
    ui.NE2000CheckBox->setText(LanguageSetup->SetupFormDOSBoxCVSNetworkNE2000());

    ui.InfoLabel->setText(LanguageSetup->SetupFormDOSBoxCVSInfo());
    ui.InfoURLLabel->setText(QString("http:/") + "/ykhwong.x-y.net/");
    {
        QFont F = ui.InfoURLLabel->font(); F.setUnderline(true); ui.InfoURLLabel->setFont(F);
        QPalette P = ui.InfoURLLabel->palette(); P.setColor(QPalette::WindowText, TColorToQColor(clBlue)); ui.InfoURLLabel->setPalette(P);
    }
    ui.InfoURLLabel->setCursor(Qt::PointingHandCursor);

    setProperty("HelpContext", ID_FileOptionsDOSBoxCVSFeatures);
}

void TSetupFrameDOSBoxExt::DOSBoxDirChanged()
{
}

void TSetupFrameDOSBoxExt::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameDOSBoxExt::HideFrame()
{
}

void TSetupFrameDOSBoxExt::RestoreDefaults()
{
    ui.MultiFloppyImageCheckBox->setChecked(false);
    ui.PhysFSCheckBox->setChecked(false);
    ui.ExtendedTextModeCheckBox->setChecked(false);
    ui.GlideEmulationCheckBox->setChecked(false);
    ui.VGAChipsetCheckBox->setChecked(false);
    ui.PixelShaderCheckBox->setChecked(false);
    ui.InnovaCheckBox->setChecked(false);
    ui.PrinterCheckBox->setChecked(false);
    ui.NE2000CheckBox->setChecked(false);
}

QString TSetupFrameDOSBoxExt::DefaultValueChange(const QString& OldList, const QString& Value, bool SetIt)
{
    TStringList* St;
    QString S;
    int I;
    QString result = OldList;

    S = Trim(ExtUpperCase(Value));
    St = ValueToList(OldList, ";,");
    bool Found = false;
    if (SetIt) {
        for (I = 0; I < St->Count(); I++) if (Trim(ExtUpperCase((*St)[I])) == S) { Found = true; break; }
        if (!Found) St->Add(Value);
    } else {
        I = 0; while (I < St->Count()) if (Trim(ExtUpperCase((*St)[I])) == S) St->Delete(I); else I++;
    }
    result = ListToValue(St, ';');   // finally result:=ListToValue(St,';')
    delete St;
    return result;
}

void TSetupFrameDOSBoxExt::SaveSetup()
{
    PrgSetup->SetAllowMultiFloppyImagesMount(ui.MultiFloppyImageCheckBox->isChecked());
    PrgSetup->SetAllowPhysFSUsage(ui.PhysFSCheckBox->isChecked());
    PrgSetup->SetAllowTextModeLineChange(ui.ExtendedTextModeCheckBox->isChecked());
    PrgSetup->SetAllowGlideSettings(ui.GlideEmulationCheckBox->isChecked());
    PrgSetup->SetAllowVGAChipsetSettings(ui.VGAChipsetCheckBox->isChecked());

    if (ValueChanged[0]) {
        bool B = (ui.RenderModesCheckBox->checkState() == Qt::Checked);
        GameDB->ConfOpt()->SetRender(DefaultValueChange(GameDB->ConfOpt()->Render(), "openglhq", B));
        GameDB->ConfOpt()->SetRender(DefaultValueChange(GameDB->ConfOpt()->Render(), "direct3d", B));
    }

    if (ValueChanged[1]) {
        GameDB->ConfOpt()->SetVideo(DefaultValueChange(GameDB->ConfOpt()->Video(), "demovga", ui.VideoModesCheckBox->isChecked()));
    }

    PrgSetup->SetAllowPixelShader(ui.PixelShaderCheckBox->isChecked());
    if (ValueChanged[3]) {
        GameDB->ConfOpt()->SetScale(DefaultValueChange(GameDB->ConfOpt()->Scale(), "Hardware 2x (hardware2x)", ui.PixelShaderCheckBox->isChecked()));
        GameDB->ConfOpt()->SetScale(DefaultValueChange(GameDB->ConfOpt()->Scale(), "Hardware 3x (hardware3x)", ui.PixelShaderCheckBox->isChecked()));
    }

    if (ValueChanged[2]) {
        GameDB->ConfOpt()->SetMIDIDevice(DefaultValueChange(GameDB->ConfOpt()->MIDIDevice(), "mt32", ui.MIDICheckBox->isChecked()));
    }
    PrgSetup->SetAllowInnova(ui.InnovaCheckBox->isChecked());

    PrgSetup->SetAllowPrinterSettings(ui.PrinterCheckBox->isChecked());

    PrgSetup->SetAllowNe2000(ui.NE2000CheckBox->isChecked());
}

void TSetupFrameDOSBoxExt::InfoURLLabelClick(QObject* Sender)
{
    QLabel *L = qobject_cast<QLabel*>(Sender);
    if (L != nullptr) ShellOpen(L->text());
}
