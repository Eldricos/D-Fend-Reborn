// C++ port of ModernProfileEditorCPUFrameUnit.pas (implementation section).
#include "ModernProfileEditorCPUFrameUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "PrgSetupUnit.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorCPUFrame::TModernProfileEditorCPUFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.CyclesAutoRadioButton, &QRadioButton::clicked, this, [this]() { CPUCyclesChange(ui.CyclesAutoRadioButton); });
    connect(ui.CyclesMaxRadioButton, &QRadioButton::clicked, this, [this]() { CPUCyclesChange(ui.CyclesMaxRadioButton); });
    connect(ui.CyclesValueRadioButton, &QRadioButton::clicked, this, [this]() { CPUCyclesChange(ui.CyclesValueRadioButton); });
    connect(ui.CyclesMaxLimitCheckBox, &QCheckBox::clicked, this, [this]() { CPUCyclesChange(ui.CyclesMaxLimitCheckBox); });
    connect(ui.CyclesUpComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { CyclesUpComboBoxChange(ui.CyclesUpComboBox); });
    connect(ui.CyclesDownComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { CyclesDownComboBoxChange(ui.CyclesDownComboBox); });
}

TModernProfileEditorCPUFrame::~TModernProfileEditorCPUFrame() = default;

// { TModernProfileEditorCPUFrame }

void TModernProfileEditorCPUFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;
    QString S;

    InitData.OnCheckValue = [this](QObject* Sender, bool& OK) { CheckValue(Sender, OK); };

    ui.CPUCoreRadioGroup->setTitle(LanguageSetup->GameCore());
    ui.CPUCyclesGroupBox->setTitle(LanguageSetup->GameCycles());
    ui.CyclesAutoRadioButton->setText(LanguageSetup->GameCyclesAuto());
    ui.CyclesMaxRadioButton->setText(LanguageSetup->GameCyclesMax());
    ui.CyclesValueRadioButton->setText(LanguageSetup->Value());
    ui.CyclesMaxLimitCheckBox->setText(LanguageSetup->GameCyclesMaxLimitValue());

    St = ValueToList(InitData.GameDB->ConfOpt()->Core(), ";,");
    for (int I = 0; I < St->Count(); I++) ui.CPUCoreRadioGroup->addItem((*St)[I]);
    delete St;
    ui.CPUCoreRadioGroup->setItemIndex(0);

    St = ValueToList(InitData.GameDB->ConfOpt()->Cycles(), ";,");
    for (int I = 0; I < St->Count(); I++) {
        S = Trim(ExtUpperCase((*St)[I]));
        if ((S == "AUTO") || (S == "MAX")) continue;
        ui.CyclesComboBox->addItem((*St)[I]);
    }
    ui.CyclesComboBox->setCurrentText("3000");
    delete St;

    ui.CyclesUpLabel->setText(LanguageSetup->GameCyclesUp());
    ui.CyclesUpComboBox->clear();
    ui.CyclesUpComboBox->addItem(LanguageSetup->GameCyclesTypeValue());
    ui.CyclesUpComboBox->addItem(LanguageSetup->GameCyclesTypePercent());
    ui.CyclesDownLabel->setText(LanguageSetup->GameCyclesDown());
    ui.CyclesDownComboBox->clear();
    ui.CyclesDownComboBox->addItem(LanguageSetup->GameCyclesTypeValue());
    ui.CyclesDownComboBox->addItem(LanguageSetup->GameCyclesTypePercent());
    ui.CyclesInfoLabel->setText(LanguageSetup->GameCyclesInfo());

    ui.CPUTypeLabel->setText(LanguageSetup->GameCPUType());
    ui.InfoLabel->setText(LanguageSetup->GameCPUInfo());

    St = ValueToList(InitData.GameDB->ConfOpt()->CPUType(), ";,");
    ui.CPUTypeComboBox->clear();
    ui.CPUTypeComboBox->addItems(St->ToQStringList());
    delete St;

    AddDefaultValueHint(ui.CPUTypeComboBox);

    setProperty("helpContext", ID_ProfileEditCPU);  // HelpContext:=...
}

void TModernProfileEditorCPUFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S, T;

    S = Trim(ExtUpperCase(Game->Core()));
    for (int I = 0; I < ui.CPUCoreRadioGroup->count(); I++) {
        if (Trim(ExtUpperCase(ui.CPUCoreRadioGroup->items()[I])) == S) { ui.CPUCoreRadioGroup->setItemIndex(I); break; }
    }

    SaveCycles = Game->Cycles();
    S = Trim(ExtUpperCase(Game->Cycles()));
    ui.CyclesAutoRadioButton->setChecked(S == "AUTO");
    if (Copy(S, 1, 3) == "MAX") {
        ui.CyclesMaxRadioButton->setChecked(true);
        ui.CyclesMaxLimitCheckBox->setVisible(true);
        T = Trim(Copy(S, 4, MaxInt));
        if ((!T.isEmpty()) && (Copy(T, 1, 5) == "LIMIT")) T = Trim(Copy(T, 6, MaxInt));
        if (!T.isEmpty()) {
            ui.CyclesMaxLimitCheckBox->setChecked(true);
            ui.CyclesComboBox->setCurrentText(T);
        } else {
            ui.CyclesMaxLimitCheckBox->setChecked(false);
        }
    }
    if ((S != "AUTO") && (Copy(S, 1, 3) != "MAX")) {
        ui.CyclesMaxLimitCheckBox->setVisible(false);
        ui.CyclesValueRadioButton->setChecked(true);
        ui.CyclesComboBox->setCurrentText(Game->Cycles());
    }
    CPUCyclesChange(this);

    if (Game->CyclesUp() < 100) ui.CyclesUpComboBox->setCurrentIndex(1); else ui.CyclesUpComboBox->setCurrentIndex(0);
    CyclesUpComboBoxChange(this);
    ui.CyclesUpEdit->setValue(Game->CyclesUp());
    if (Game->CyclesDown() < 100) ui.CyclesDownComboBox->setCurrentIndex(1); else ui.CyclesDownComboBox->setCurrentIndex(0);
    CyclesDownComboBoxChange(this);
    ui.CyclesDownEdit->setValue(Game->CyclesDown());

    ui.CPUTypeComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(Game->CPUType()));
    for (int I = 0; I < ui.CPUTypeComboBox->count(); I++) if (S == Trim(ExtUpperCase(ui.CPUTypeComboBox->itemText(I)))) {
        ui.CPUTypeComboBox->setCurrentIndex(I); break;
    }
}

void TModernProfileEditorCPUFrame::CheckValue(QObject* Sender, bool& OK)
{
    Q_UNUSED(Sender);
    int I;
    bool B;
    QString S;

    OK = true;

    if (ui.CyclesValueRadioButton->isChecked()) {
        B = TryStrToInt(ui.CyclesComboBox->currentText(), I);
        if (!B) {
            S = Trim(ExtUpperCase(ui.CyclesComboBox->currentText()));
            for (I = 0; I < ui.CyclesComboBox->count(); I++) if (Trim(ExtUpperCase(ui.CyclesComboBox->itemText(I))) == S) { B = true; break; }
        }
        if (!B) {
            if (MessageDlg(Format(LanguageSetup->MessageInvalidValue(), {ui.CyclesComboBox->currentText(), LanguageSetup->GameCycles(), SaveCycles}), mtWarning, mbYes | mbNo, 0) != mrYes) {
                OK = false;
                return;
            } else {
                ui.CyclesComboBox->setCurrentText(SaveCycles);
            }
        }
    }
}

void TModernProfileEditorCPUFrame::CPUCyclesChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CyclesMaxLimitCheckBox->setVisible(ui.CyclesMaxRadioButton->isChecked());
    ui.CyclesComboBox->setEnabled(ui.CyclesValueRadioButton->isChecked() || (ui.CyclesMaxLimitCheckBox->isVisibleTo(this) && ui.CyclesMaxLimitCheckBox->isChecked()));
}

void TModernProfileEditorCPUFrame::CyclesUpComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.CyclesUpComboBox->currentIndex() == 0) {
        ui.CyclesUpEdit->setMinimum(100); ui.CyclesUpEdit->setMaximum(50000);
    } else {
        ui.CyclesUpEdit->setMinimum(1); ui.CyclesUpEdit->setMaximum(99);
    }
}

void TModernProfileEditorCPUFrame::CyclesDownComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.CyclesDownComboBox->currentIndex() == 0) {
        ui.CyclesDownEdit->setMinimum(100); ui.CyclesDownEdit->setMaximum(50000);
    } else {
        ui.CyclesDownEdit->setMinimum(1); ui.CyclesDownEdit->setMaximum(99);
    }
}

void TModernProfileEditorCPUFrame::GetGame(TGame* Game)
{
    Game->SetCore(ui.CPUCoreRadioGroup->items().value(ui.CPUCoreRadioGroup->itemIndex()));

    if (ui.CyclesAutoRadioButton->isChecked()) Game->SetCycles("auto");
    if (ui.CyclesMaxRadioButton->isChecked()) {
        Game->SetCycles("max");
        if (ui.CyclesMaxLimitCheckBox->isChecked()) Game->SetCycles(Game->Cycles() + " limit " + ui.CyclesComboBox->currentText());
    }
    if (ui.CyclesValueRadioButton->isChecked()) Game->SetCycles(ui.CyclesComboBox->currentText());
    Game->SetCyclesUp(std::min(30000, std::max(1, ui.CyclesUpEdit->value())));
    Game->SetCyclesDown(std::min(30000, std::max(1, ui.CyclesDownEdit->value())));

    Game->SetCPUType(ui.CPUTypeComboBox->currentText());
}
