// C++ port of ModernProfileEditorNetworkFrameUnit.pas (implementation section).
#include "ModernProfileEditorNetworkFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "PrgSetupUnit.h"
#include "DosBoxUnit.h"
#include "DOSBoxTempUnit.h"
#include "MainUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorNetworkFrame::TModernProfileEditorNetworkFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.ActivateCheckBox, &QCheckBox::clicked, this, [this]() { ActivateCheckBoxClick(ui.ActivateCheckBox); });
    connect(ui.ConnectionsRadioGroup, &TRadioGroupWidget::clicked, this, [this]() { ActivateCheckBoxClick(ui.ConnectionsRadioGroup); });
    connect(ui.ResetMACButton, &QPushButton::clicked, this, [this]() { ResetMACButtonClick(ui.ResetMACButton); });
    connect(ui.ListNICButton, &QPushButton::clicked, this, [this]() { ListNICButtonClick(ui.ListNICButton); });
}

TModernProfileEditorNetworkFrame::~TModernProfileEditorNetworkFrame() = default;

// { TModernProfileEditorNetworkFrame }

void TModernProfileEditorNetworkFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;

    ui.ActivateCheckBox->setText(LanguageSetup->GameIPX());
    ui.ConnectionsRadioGroup->setTitle(LanguageSetup->GameIPXEstablishConnection());
    ui.ConnectionsRadioGroup->setItemText(0, LanguageSetup->GameIPXEstablishConnectionNone());
    ui.ConnectionsRadioGroup->setItemText(1, LanguageSetup->GameIPXEstablishConnectionClient());
    ui.ConnectionsRadioGroup->setItemText(2, LanguageSetup->GameIPXEstablishConnectionServer());
    ui.ServerAddressEdit_EditLabel->setText(LanguageSetup->GameIPXAddress());
    ui.PortLabel->setText(LanguageSetup->GameIPXPort());

    ui.NE2000CheckBox->setText(LanguageSetup->GameNE2000());
    ui.NE2000BaseAddressLabel->setText(LanguageSetup->GameNE2000BaseAddress());
    ui.NE2000InterruptLabel->setText(LanguageSetup->GameNE2000Interrupt());
    ui.NE2000MACAddressEdit_EditLabel->setText(LanguageSetup->GameNE2000MACAddress());
    ui.ResetMACButton->setText(LanguageSetup->GameNE2000MACAddressReset());
    ui.NE2000RealNICEdit_EditLabel->setText(LanguageSetup->GameNE2000RealInterface());
    ui.ListNICButton->setText(LanguageSetup->GameNE2000RealInterfaceList());

    ui.NE2000CheckBox->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000BaseAddressLabel->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000BaseAddressComboBox->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000InterruptLabel->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000InterruptComboBox->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000MACAddressEdit->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000MACAddressEdit_EditLabel->setVisible(PrgSetup->AllowNe2000());
    ui.ResetMACButton->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000RealNICEdit->setVisible(PrgSetup->AllowNe2000());
    ui.NE2000RealNICEdit_EditLabel->setVisible(PrgSetup->AllowNe2000());
    ui.ListNICButton->setVisible(PrgSetup->AllowNe2000());

    St = ValueToList(InitData.GameDB->ConfOpt()->NE2000EmulationBaseAddress(), ";,");
    ui.NE2000BaseAddressComboBox->clear();
    ui.NE2000BaseAddressComboBox->addItems(St->ToQStringList());
    delete St;

    St = ValueToList(InitData.GameDB->ConfOpt()->NE2000EmulationInterrupt(), ";,");
    ui.NE2000InterruptComboBox->clear();
    ui.NE2000InterruptComboBox->addItems(St->ToQStringList());
    delete St;

    ProfileDOSBoxInstallation = InitData.CurrentDOSBoxInstallation;

    setProperty("HelpContext", ID_ProfileEditNetwork);  // HelpContext:=...
}

void TModernProfileEditorNetworkFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S;
    int I;

    ui.ActivateCheckBox->setChecked(Game->IPX());
    ui.ConnectionsRadioGroup->setItemIndex(0);
    S = Trim(ExtUpperCase(Game->IPXType()));
    if ((S.isEmpty()) || (S == "NONE")) ui.ConnectionsRadioGroup->setItemIndex(0);
    if (S == "CLIENT") ui.ConnectionsRadioGroup->setItemIndex(1);
    if (S == "SERVER") ui.ConnectionsRadioGroup->setItemIndex(2);
    ui.ServerAddressEdit->setText(Game->IPXAddress());
    if (TryStrToInt(Game->IPXPort(), I)) ui.PortEdit->setValue(I); else ui.PortEdit->setValue(1);
    ActivateCheckBoxClick(this);

    ui.NE2000CheckBox->setChecked(Game->NE2000());
    ui.NE2000BaseAddressComboBox->setCurrentIndex(0);
    S = Trim(ExtUpperCase(Game->NE2000Base()));
    for (I = 0; I < ui.NE2000BaseAddressComboBox->count(); I++) if (Trim(ExtUpperCase(ui.NE2000BaseAddressComboBox->itemText(I))) == S) {
        ui.NE2000BaseAddressComboBox->setCurrentIndex(I); break;
    }
    ui.NE2000InterruptComboBox->setCurrentIndex(0);
    S = IntToStr(Game->NE2000IRQ());
    for (I = 0; I < ui.NE2000InterruptComboBox->count(); I++) if (Trim(ExtUpperCase(ui.NE2000InterruptComboBox->itemText(I))) == S) {
        ui.NE2000InterruptComboBox->setCurrentIndex(I); break;
    }
    ui.NE2000MACAddressEdit->setText(Game->NE2000MACAddress());
    ui.NE2000RealNICEdit->setText(Game->NE2000RealInterface());
}

void TModernProfileEditorNetworkFrame::ActivateCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ConnectionsRadioGroup->setEnabled(ui.ActivateCheckBox->isChecked());
    ui.ServerAddressEdit->setEnabled(ui.ActivateCheckBox->isChecked() && (ui.ConnectionsRadioGroup->itemIndex() == 1));
    ui.ServerAddressEdit_EditLabel->setEnabled(ui.ServerAddressEdit->isEnabled());
    ui.PortEdit->setEnabled(ui.ActivateCheckBox->isChecked());
}

void TModernProfileEditorNetworkFrame::GetGame(TGame* Game)
{
    int I;

    Game->SetIPX(ui.ActivateCheckBox->isChecked());
    switch (ui.ConnectionsRadioGroup->itemIndex()) {
        case 0: Game->SetIPXType("none"); break;
        case 1: Game->SetIPXType("client"); break;
        case 2: Game->SetIPXType("server"); break;
    }
    Game->SetIPXAddress(ui.ServerAddressEdit->text());
    Game->SetIPXPort(IntToStr(std::min(99999, std::max(1, ui.PortEdit->value()))));

    Game->SetNE2000(ui.NE2000CheckBox->isChecked());
    Game->SetNE2000Base(ui.NE2000BaseAddressComboBox->itemText(std::max(0, ui.NE2000BaseAddressComboBox->currentIndex())));
    if (TryStrToInt(ui.NE2000InterruptComboBox->itemText(std::max(0, ui.NE2000InterruptComboBox->currentIndex())), I)) Game->SetNE2000IRQ(I);
    Game->SetNE2000MACAddress(ui.NE2000MACAddressEdit->text());
    Game->SetNE2000RealInterface(ui.NE2000RealNICEdit->text());
}

void TModernProfileEditorNetworkFrame::ResetMACButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.NE2000MACAddressEdit->setText("AC:DE:48:88:99:AA");
}

void TModernProfileEditorNetworkFrame::ListNICButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TTempGame* TempGame;

    TempGame = new TTempGame();
    TempGame->SetSimpleDefaults();
    TempGame->Game()->SetNE2000(true);
    TempGame->Game()->SetNE2000Base("300");
    TempGame->Game()->SetNE2000IRQ(3);
    TempGame->Game()->SetNE2000MACAddress("AC:DE:48:88:99:AA");
    TempGame->Game()->SetNE2000RealInterface("list");
    TempGame->Game()->SetShowConsoleWindow(2);
    TempGame->Game()->SetCustomDOSBoxDir(*ProfileDOSBoxInstallation);
    RunCommand(TempGame->Game(), DFendRebornMainForm->DeleteOnExit, "", true);
    delete TempGame;
}
