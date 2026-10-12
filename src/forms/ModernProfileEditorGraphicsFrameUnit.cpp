// C++ port of ModernProfileEditorGraphicsFrameUnit.pas (implementation section).
#include "ModernProfileEditorGraphicsFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "GameDBToolsUnit.h"

// ComboBox.Items.AddStrings(ValueToList(Value,';,'))
static void AddValueList(QComboBox* C, const QString& Value)
{
    TStringList* St = ValueToList(Value, ";,");
    C->addItems(St->ToQStringList());
    delete St;
}

/* TModernProfileEditorGraphicsFrame */

TModernProfileEditorGraphicsFrame::TModernProfileEditorGraphicsFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.PixelShaderComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { PixelShaderComboBoxChange(ui.PixelShaderComboBox); });
    if (ui.PixelShaderComboBox->lineEdit() != nullptr)
        connect(ui.PixelShaderComboBox->lineEdit(), &QLineEdit::textEdited, this, [this]() { PixelShaderComboBoxChange(ui.PixelShaderComboBox); });
}

TModernProfileEditorGraphicsFrame::~TModernProfileEditorGraphicsFrame()
{
}

void TModernProfileEditorGraphicsFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;
    int I;
    QString S;

    InitData.OnShowFrame = [this](QObject* Sender) { ShowFrame(Sender); };

    // NoFlicker(...): not needed with Qt

    ui.WindowResolutionLabel->setText(LanguageSetup->GameWindowResolution());
    ui.FullscreenResolutionLabel->setText(LanguageSetup->GameFullscreenResolution());
    AddValueList(ui.WindowResolutionComboBox, InitData.GameDB->ConfOpt()->ResolutionWindow());
    AddValueList(ui.FullscreenResolutionComboBox, InitData.GameDB->ConfOpt()->ResolutionFullscreen());
    ui.ResolutionInfoLabel->setText(LanguageSetup->GameResolutionInfo());
    ui.StartFullscreenCheckBox->setText(LanguageSetup->GameStartFullscreen());
    ui.FullscreenInfoLabel->setText("(" + LanguageSetup->GameStartFullscreenInfo() + ")");
    ui.DoublebufferingCheckBox->setText(LanguageSetup->GameUseDoublebuffering());
    ui.KeepAspectRatioCheckBox->setText(LanguageSetup->GameAspectCorrection());
    ui.GlideEmulationLabel->setText(LanguageSetup->GameGlideEmulation());
    ui.GlideEmulationPortLabel->setText(LanguageSetup->GameGlideEmulationPort());
    ui.GlideEmulationLFBLabel->setText(LanguageSetup->GameGlideEmulationLFB());

    St = ValueToList(InitData.GameDB->ConfOpt()->GlideEmulation(), ";,");
    for (I = 0; I < St->Count(); I++) {
        S = Trim(ExtUpperCase((*St)[I]));
        if (S == "FALSE") (*St)[I] = LanguageSetup->Off();
        if (S == "TRUE") (*St)[I] = LanguageSetup->On();
    }
    ui.GlideEmulationComboBox->clear();
    ui.GlideEmulationComboBox->addItems(St->ToQStringList());
    delete St;
    ui.GlideEmulationPortComboBox->clear();
    AddValueList(ui.GlideEmulationPortComboBox, InitData.GameDB->ConfOpt()->GlideEmulationPort());
    ui.GlideEmulationLFBComboBox->clear();
    AddValueList(ui.GlideEmulationLFBComboBox, InitData.GameDB->ConfOpt()->GlideEmulationLFB());
    ui.RenderLabel->setText(LanguageSetup->GameRender());
    AddValueList(ui.RenderComboBox, InitData.GameDB->ConfOpt()->Render());
    ui.VideoCardLabel->setText(LanguageSetup->GameVideoCard());
    AddValueList(ui.VideoCardComboBox, InitData.GameDB->ConfOpt()->Video());
    ui.VGASettingsGroupBox->setTitle(LanguageSetup->GameVGASettings());
    ui.VGAChipsetLabel->setText(LanguageSetup->GameVGAChipset());
    AddValueList(ui.VGAChipsetComboBox, InitData.GameDB->ConfOpt()->VGAChipsets());
    ui.VideoRamLabel->setText(LanguageSetup->GameVideoRam());
    AddValueList(ui.VideoRamComboBox, InitData.GameDB->ConfOpt()->VGAVideoRAM());
    ui.VGASettingsLabel->setText(LanguageSetup->GameVGASettingsInfo());
    ui.ScaleLabel->setText(LanguageSetup->GameScale());
    AddValueList(ui.ScaleComboBox, InitData.GameDB->ConfOpt()->Scale());
    ui.FrameSkipLabel->setText(LanguageSetup->GameFrameskip());
    ui.TextModeLinesRadioGroup->setTitle(LanguageSetup->GameTextModeLines());
    ui.PixelShaderLabel->setText(LanguageSetup->GamePixelShader());

    ProfileDOSBoxInstallation = InitData.CurrentDOSBoxInstallation;

    AddDefaultValueHint(ui.WindowResolutionComboBox);
    AddDefaultValueHint(ui.FullscreenResolutionComboBox);
    AddDefaultValueHint(ui.RenderComboBox);
    AddDefaultValueHint(ui.VideoCardComboBox);
    AddDefaultValueHint(ui.VGAChipsetComboBox);
    AddDefaultValueHint(ui.VideoRamComboBox);
    AddDefaultValueHint(ui.ScaleComboBox);

    setProperty("HelpContext", ID_ProfileEditGraphics);  // HelpContext
}

void TModernProfileEditorGraphicsFrame::PixelShaderComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    PixelShaderChanged = true;
}

void TModernProfileEditorGraphicsFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    int I;
    QString S, T;

    S = Trim(ExtUpperCase(Game->WindowResolution()));
    ui.WindowResolutionComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.WindowResolutionComboBox->count(); I++) if (Trim(ExtUpperCase(ui.WindowResolutionComboBox->itemText(I))) == S) {
        ui.WindowResolutionComboBox->setCurrentIndex(I); break;
    }

    S = Trim(ExtUpperCase(Game->FullscreenResolution()));
    ui.FullscreenResolutionComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.FullscreenResolutionComboBox->count(); I++) if (Trim(ExtUpperCase(ui.FullscreenResolutionComboBox->itemText(I))) == S) {
        ui.FullscreenResolutionComboBox->setCurrentIndex(I); break;
    }

    ui.StartFullscreenCheckBox->setChecked(Game->StartFullscreen());
    ui.DoublebufferingCheckBox->setChecked(Game->UseDoublebuffering());
    ui.KeepAspectRatioCheckBox->setChecked(Game->AspectCorrection());
    ui.GlideEmulationLabel->setVisible(PrgSetup->AllowGlideSettings());
    ui.GlideEmulationComboBox->setVisible(PrgSetup->AllowGlideSettings());
    ui.GlideEmulationPortLabel->setVisible(PrgSetup->AllowGlideSettings());
    ui.GlideEmulationPortComboBox->setVisible(PrgSetup->AllowGlideSettings());
    ui.GlideEmulationLFBLabel->setVisible(PrgSetup->AllowGlideSettings());
    ui.GlideEmulationLFBComboBox->setVisible(PrgSetup->AllowGlideSettings());
    if (PrgSetup->AllowGlideSettings()) {
        S = Trim(ExtUpperCase(Game->GlideEmulation()));
        if (S == "0" || S == "FALSE") S = LanguageSetup->Off();
        if (S == "1" || S == "TRUE") S = LanguageSetup->On();
        S = ExtUpperCase(S);
        ui.GlideEmulationComboBox->setCurrentIndex(0);
        for (I = 0; I < ui.GlideEmulationComboBox->count(); I++) if (Trim(ExtUpperCase(ui.GlideEmulationComboBox->itemText(I))) == S) {
            ui.GlideEmulationComboBox->setCurrentIndex(I); break;
        }
        S = Trim(ExtUpperCase(Game->GlidePort()));
        ui.GlideEmulationPortComboBox->setCurrentIndex(0);
        for (I = 0; I < ui.GlideEmulationPortComboBox->count(); I++) if (Trim(ExtUpperCase(ui.GlideEmulationPortComboBox->itemText(I))) == S) {
            ui.GlideEmulationPortComboBox->setCurrentIndex(I); break;
        }
        S = Trim(ExtUpperCase(Game->GlideLFB()));
        ui.GlideEmulationLFBComboBox->setCurrentIndex(0);
        for (I = 0; I < ui.GlideEmulationLFBComboBox->count(); I++) if (Trim(ExtUpperCase(ui.GlideEmulationLFBComboBox->itemText(I))) == S) {
            ui.GlideEmulationLFBComboBox->setCurrentIndex(I); break;
        }
    }

    S = Trim(ExtUpperCase(Game->Render()));
    ui.RenderComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.RenderComboBox->count(); I++) if (Trim(ExtUpperCase(ui.RenderComboBox->itemText(I))) == S) {
        ui.RenderComboBox->setCurrentIndex(I); break;
    }

    S = Trim(ExtUpperCase(Game->VideoCard()));
    ui.VideoCardComboBox->setCurrentIndex(ui.VideoCardComboBox->count() - 1);
    for (I = 0; I < ui.VideoCardComboBox->count(); I++) {
        T = Trim(ExtUpperCase(ui.VideoCardComboBox->itemText(I)));
        if (Pos("(", T) > 0) T = Trim(Copy(T, 1, Pos("(", T) - 1));
        if (T == S) { ui.VideoCardComboBox->setCurrentIndex(I); break; }
    }

    LastPixelShader = Game->PixelShader();
    ui.PixelShaderComboBox->clear();

    ui.VGASettingsGroupBox->setVisible(PrgSetup->AllowVGAChipsetSettings());
    if (PrgSetup->AllowVGAChipsetSettings()) {
        if (ui.VGAChipsetComboBox->count() > 0) ui.VGAChipsetComboBox->setCurrentIndex(0);
        S = Trim(ExtUpperCase(Game->VGAChipset()));
        for (I = 0; I < ui.VGAChipsetComboBox->count(); I++) if (Trim(ExtUpperCase(ui.VGAChipsetComboBox->itemText(I))) == S) {
            ui.VGAChipsetComboBox->setCurrentIndex(I);
            break;
        }
        if (ui.VideoRamComboBox->count() > 0) ui.VideoRamComboBox->setCurrentIndex(0);
        S = Trim(ExtUpperCase(IntToStr(Game->VideoRam())));
        for (I = 0; I < ui.VideoRamComboBox->count(); I++) {
            T = Trim(ExtUpperCase(ui.VideoRamComboBox->itemText(I)));
            if (T == S) { ui.VideoRamComboBox->setCurrentIndex(I); break; }
            if (T == "2048") ui.VideoRamComboBox->setCurrentIndex(I);
        }
    }

    S = Trim(ExtUpperCase(Game->Scale()));
    ui.ScaleComboBox->setCurrentIndex(0);
    for (I = 0; I < ui.ScaleComboBox->count(); I++) {
        T = Trim(ExtUpperCase(ui.ScaleComboBox->itemText(I)));
        if (Pos("(", T) == 0) continue;
        T = Copy(T, Pos("(", T) + 1, MaxInt);
        if (Pos(")", T) == 0) continue;
        T = Copy(T, 1, Pos(")", T) - 1);
        if (Trim(T) == S) { ui.ScaleComboBox->setCurrentIndex(I); break; }
    }

    ui.FrameSkipEdit->setValue(Game->FrameSkip());

    ui.TextModeLinesRadioGroup->setVisible(PrgSetup->AllowTextModeLineChange());
    if (PrgSetup->AllowTextModeLineChange()) switch (Game->TextModeLines()) {
        case 28: ui.TextModeLinesRadioGroup->setItemIndex(1); break;
        case 50: ui.TextModeLinesRadioGroup->setItemIndex(2); break;
        default: ui.TextModeLinesRadioGroup->setItemIndex(0); break;
    }

    PixelShaderChanged = LoadFromTemplate;
}

QString TModernProfileEditorGraphicsFrame::GetDOSBoxDir()
{
    QString result;
    QString S;
    int I;

    result = "";

    S = Trim(ExtUpperCase(*ProfileDOSBoxInstallation));
    if (S == "" || S == "DEFAULT") {
        result = PrgSetup->DOSBoxSettings(0)->DosBoxDir();
    } else {
        for (I = 1; I < PrgSetup->DOSBoxSettingsCount(); I++) if (Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(I)->Name())) == S) { result = PrgSetup->DOSBoxSettings(I)->DosBoxDir(); break; }
    }

    if (result == "") result = *ProfileDOSBoxInstallation;

    result = IncludeTrailingPathDelimiter(MakeAbsPath(result, PrgSetup->BaseDir()));
    return result;
}

void TModernProfileEditorGraphicsFrame::ShowFrame(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    QString S;
    TStringList* St;

    ui.PixelShaderLabel->setVisible(PrgSetup->AllowPixelShader());
    ui.PixelShaderComboBox->setVisible(PrgSetup->AllowPixelShader());

    if (ui.PixelShaderComboBox->count() > 0) LastPixelShader = ui.PixelShaderComboBox->currentText();
    ui.PixelShaderComboBox->clear();

    St = GetPixelShaders(GetDOSBoxDir());
    ui.PixelShaderComboBox->addItems(St->ToQStringList());
    delete St;

    ui.PixelShaderComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(LastPixelShader));
    for (I = 0; I < ui.PixelShaderComboBox->count(); I++) if (Trim(ExtUpperCase(ui.PixelShaderComboBox->itemText(I))) == S) {
        ui.PixelShaderComboBox->setCurrentIndex(I); break;
    }

    PixelShaderChanged = false;
}

void TModernProfileEditorGraphicsFrame::GetGame(TGame* Game)
{
    QString S;

    Game->SetWindowResolution(ui.WindowResolutionComboBox->currentText());
    Game->SetFullscreenResolution(ui.FullscreenResolutionComboBox->currentText());

    Game->SetStartFullscreen(ui.StartFullscreenCheckBox->isChecked());
    Game->SetUseDoublebuffering(ui.DoublebufferingCheckBox->isChecked());
    Game->SetAspectCorrection(ui.KeepAspectRatioCheckBox->isChecked());
    if (PrgSetup->AllowGlideSettings()) {
        S = ui.GlideEmulationComboBox->itemText(ui.GlideEmulationComboBox->currentIndex());
        if (S == LanguageSetup->On()) S = "true";
        if (S == LanguageSetup->Off()) S = "false";
        Game->SetGlideEmulation(S);
        Game->SetGlidePort(ui.GlideEmulationPortComboBox->currentText());
        Game->SetGlideLFB(ui.GlideEmulationLFBComboBox->itemText(Max(0, ui.GlideEmulationLFBComboBox->currentIndex())));
    }
    Game->SetRender(ui.RenderComboBox->currentText());
    S = Trim(ui.VideoCardComboBox->currentText());
    if (Pos("(", S) != 0) S = Trim(Copy(S, 1, Pos("(", S) - 1));
    Game->SetVideoCard(S);

    if (PixelShaderChanged) Game->SetPixelShader(ui.PixelShaderComboBox->currentText());

    if (PrgSetup->AllowVGAChipsetSettings()) {
        Game->SetVGAChipset(ui.VGAChipsetComboBox->currentText());
        try { Game->SetVideoRam(StrToInt(ui.VideoRamComboBox->currentText())); } catch (...) { Game->SetVideoRam(2048); }
    }

    S = ui.ScaleComboBox->currentText();
    if (Pos("(", S) == 0) Game->SetScale(""); else {
        S = Copy(S, Pos("(", S) + 1, MaxInt);
        if (Pos(")", S) == 0) Game->SetScale(""); else Game->SetScale(Copy(S, 1, Pos(")", S) - 1));
    }
    Game->SetFrameSkip(Min(100, Max(0, ui.FrameSkipEdit->value())));

    if (PrgSetup->AllowTextModeLineChange()) switch (ui.TextModeLinesRadioGroup->itemIndex()) {
        case 0: Game->SetTextModeLines(25); break;
        case 1: Game->SetTextModeLines(28); break;
        case 2: Game->SetTextModeLines(50); break;
    }
}
