// C++ port of ModernProfileEditorSerialPortFrameUnit.pas (implementation section).
#include "ModernProfileEditorSerialPortFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorSerialPortFrame::TModernProfileEditorSerialPortFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    connect(ui.SerialTypeDisabledRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDisabledRadioButton); });
    connect(ui.SerialTypeDummyRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDummyRadioButton); });
    connect(ui.SerialTypeModemRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeModemRadioButton); });
    connect(ui.SerialTypeNullmodemRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeNullmodemRadioButton); });
    connect(ui.SerialTypeDirectSerialRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDirectSerialRadioButton); });
    connect(ui.ModemListenCheckBox, &QCheckBox::clicked, this, [this]() { DeviceTypeClick(ui.ModemListenCheckBox); });
    connect(ui.NullModemServerRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.NullModemServerRadioButton); });
    connect(ui.NullModemClientRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.NullModemClientRadioButton); });
}

TModernProfileEditorSerialPortFrame::~TModernProfileEditorSerialPortFrame() = default;

// { TModernProfileEditorSerialPortFrame }

void TModernProfileEditorSerialPortFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);

    setObjectName("SerialPort" + IntToStr(PortNr) + "Frame");

    ui.DeviceTypeGroupBox->setTitle(LanguageSetup->SerialFormDeviceType());
    ui.SerialTypeDisabledRadioButton->setText(LanguageSetup->SerialFormDeviceTypeDisabled());
    ui.SerialTypeDummyRadioButton->setText(LanguageSetup->SerialFormDeviceTypeDummy());
    ui.SerialTypeModemRadioButton->setText(LanguageSetup->SerialFormDeviceTypeModem());
    ui.SerialTypeNullmodemRadioButton->setText(LanguageSetup->SerialFormDeviceTypeNullModem());
    ui.SerialTypeDirectSerialRadioButton->setText(LanguageSetup->SerialFormDeviceTypeDirectSerial());

    ui.IRQLabel->setText(LanguageSetup->SerialFormIRQ());
    ui.IRQComboBox->setItemText(0, LanguageSetup->SerialFormIRQNone());

    ui.ModemIPEdit_EditLabel->setText(LanguageSetup->SerialFormModemServerIP());
    ui.ModemPortLabel->setText(LanguageSetup->SerialFormModemPort());
    ui.ModemListenCheckBox->setText(LanguageSetup->SerialFormModemListen());

    ui.NullModemPortLabel->setText(LanguageSetup->SerialFormNullModemPort());
    ui.NullModemServerRadioButton->setText(LanguageSetup->SerialFormNullModemServerMode());
    ui.NullModemClientRadioButton->setText(LanguageSetup->SerialFormNullModemClientMode());
    ui.NullModemRXDelayLabel->setText(LanguageSetup->SerialFormRXDelay());
    ui.NullModemTXDelayLabel->setText(LanguageSetup->SerialFormTXDelay());
    ui.NullModemTelnetCheckBox->setText(LanguageSetup->SerialFormNullModemTelnet());
    ui.NullModemDTRCheckBox->setText(LanguageSetup->SerialFormNullModemDTR());
    ui.NullModemTransparentCheckBox->setText(LanguageSetup->SerialFormNullModemTransparent());

    ui.DirectSerialPortEdit_EditLabel->setText(LanguageSetup->SerialFormDirectSerialPort());
    ui.DirectSerialRXDelayLabel->setText(LanguageSetup->SerialFormRXDelay());

    setProperty("HelpContext", ID_ProfileEditSerialPorts);  // HelpContext:=...
}

static void Divide(const QString& Data, QString& Name, QString& Value)
{
    int I = Pos(":", Data);
    if (I == 0) { Name = Data; Value = ""; } else { Name = Trim(Copy(Data, 1, I - 1)); Value = Trim(Copy(Data, I + 1, MaxInt)); }
}

void TModernProfileEditorSerialPortFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    QString S, Key, DataKey, DataValue;
    TStringList* Data;
    int I, J;

    ui.IRQComboBox->setCurrentIndex(0);

    switch (PortNr) {
        case 1: S = Game->Serial1(); break;
        case 2: S = Game->Serial2(); break;
        case 3: S = Game->Serial3(); break;
        case 4: S = Game->Serial4(); break;
        default: S = Game->Serial1(); break;
    }
    S = Trim(ExtUpperCase(S));

    Data = new TStringList();
    {
        I = Pos(" ", S); if (I == 0) { Key = S; S = ""; } else { Key = Trim(Copy(S, 1, I - 1)); S = Trim(Copy(S, I + 1, MaxInt)); }

        while (!S.isEmpty()) {
            I = Pos(" ", S);
            if (I == 0) {
                Data->Add(S);
                S = "";
            } else {
                Data->Add(Trim(Copy(S, 1, I - 1)));
                S = Trim(Copy(S, I + 1, MaxInt));
            }
        }

        if (Key == "DUMMY") {
            ui.SerialTypeDummyRadioButton->setChecked(true);
            for (I = 0; I < Data->Count(); I++) {
                Divide((*Data)[I], DataKey, DataValue);
                if (DataKey == "IRQ") SetIRQ(DataValue);
            }
            goto Finally;
        }

        if (Key == "MODEM") {
            ui.SerialTypeModemRadioButton->setChecked(true);
            for (I = 0; I < Data->Count(); I++) {
                Divide((*Data)[I], DataKey, DataValue);
                /*If DataKey='IRQ' then SetIRQ(DataValue);*/
                /*If DataKey='SERVER' then ModemIPEdit.Text:=DataValue;
                If DataKey='PORT' then begin try J:=Min(65535,Max(1,StrToInt(DataValue))); except J:=5000; end; ModemPortEdit.Value:=J; end;*/
                if (DataKey == "LISTENPORT") {
                    if (Trim(DataValue) == "0") {
                        ui.ModemListenCheckBox->setChecked(false);
                    } else {
                        ui.ModemListenCheckBox->setChecked(true);
                        if (TryStrToInt(DataValue, J)) J = std::min(65535, std::max(1, J)); else J = 5000;
                        ui.ModemPortEdit->setValue(J);
                    }
                }
            }
            goto Finally;
        }

        if (Key == "NULLMODEM") {
            ui.SerialTypeNullmodemRadioButton->setChecked(true);
            for (I = 0; I < Data->Count(); I++) {
                Divide((*Data)[I], DataKey, DataValue);
                if (DataKey == "IRQ") SetIRQ(DataValue);
                if (DataKey == "PORT") { if (TryStrToInt(DataValue, J)) J = std::min(65535, std::max(1, J)); else J = 5000; ui.NullModemPortEdit->setValue(J); }
                if (DataKey == "SERVER") { ui.NullModemClientRadioButton->setChecked(true); ui.NullModemServerAddressEdit->setText(DataValue); }
                if (DataKey == "RXDELAY") { if (TryStrToInt(DataValue, J)) J = std::min(1000, std::max(0, J)); else J = 100; ui.NullModemRXDelayEdit->setValue(J); }
                if (DataKey == "TXDELAY") { if (TryStrToInt(DataValue, J)) J = std::min(1000, std::max(0, J)); else J = 12; ui.NullModemTXDelayEdit->setValue(J); }
                if (DataKey == "TELNET") ui.NullModemTelnetCheckBox->setChecked(DataValue != "0");
                if (DataKey == "USEDTR") ui.NullModemDTRCheckBox->setChecked(DataValue != "0");
                if (DataKey == "TRANSPARENT") ui.NullModemTransparentCheckBox->setChecked(DataValue != "0");
            }
            goto Finally;
        }

        if (Key == "DIRECTSERIAL") {
            ui.SerialTypeDirectSerialRadioButton->setChecked(true);
            for (I = 0; I < Data->Count(); I++) {
                Divide((*Data)[I], DataKey, DataValue);
                if (DataKey == "IRQ") SetIRQ(DataValue);
                if (DataKey == "REALPORT") ui.DirectSerialPortEdit->setText(DataValue);
                if (DataKey == "RXDELAY") { if (TryStrToInt(DataValue, J)) J = std::min(1000, std::max(0, J)); else J = 100; ui.DirectSerialRXDelayEdit->setValue(J); }
            }
            goto Finally;
        }

        /*KEY='DISABLED'*/
        ui.SerialTypeDisabledRadioButton->setChecked(true);
    }
Finally:  // try ... finally
    delete Data;
    DeviceTypeClick(this);
}

void TModernProfileEditorSerialPortFrame::SetIRQ(const QString& S)
{
    int I = ui.IRQComboBox->findText(S, Qt::MatchFixedString);  // Items.IndexOf (case insensitive)
    if (I >= 0) ui.IRQComboBox->setCurrentIndex(I);
}

// SetPanel(P): the panels share one cell of the layout below the IRQ setting;
// only the panel of the selected device type is visible.

void TModernProfileEditorSerialPortFrame::DeviceTypeClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.IRQComboBox->setEnabled(!ui.SerialTypeDisabledRadioButton->isChecked());

    ui.ModemPanel->setVisible(false);
    ui.NullModemPanel->setVisible(false);
    ui.DirectSerialPanel->setVisible(false);

    if (ui.SerialTypeModemRadioButton->isChecked()) ui.ModemPanel->setVisible(true);
    if (ui.SerialTypeNullmodemRadioButton->isChecked()) ui.NullModemPanel->setVisible(true);
    if (ui.SerialTypeDirectSerialRadioButton->isChecked()) ui.DirectSerialPanel->setVisible(true);

    ui.ModemIPEdit->setEnabled(!ui.ModemListenCheckBox->isChecked());
    ui.ModemIPEdit_EditLabel->setEnabled(ui.ModemIPEdit->isEnabled());
    ui.ModemPortEdit->setEnabled(ui.ModemListenCheckBox->isChecked());

    ui.NullModemServerAddressEdit->setEnabled(ui.NullModemClientRadioButton->isChecked());
}

void TModernProfileEditorSerialPortFrame::GetGame(TGame* Game)
{
    QString IRQ;
    QString S;

    if (ui.IRQComboBox->isEnabled() && (ui.IRQComboBox->currentIndex() > 0)) IRQ = " IRQ:" + ui.IRQComboBox->currentText(); else IRQ = "";

    if (ui.SerialTypeDisabledRadioButton->isChecked()) {
        S = "Disabled";
    }

    if (ui.SerialTypeDummyRadioButton->isChecked()) {
        S = "Dummy" + IRQ;
    }

    if (ui.SerialTypeModemRadioButton->isChecked()) {
        S = "Modem" + IRQ;
        if (ui.ModemListenCheckBox->isChecked()) {
            S = S + " Listenport:" + IntToStr(ui.ModemPortEdit->value());
        } else {
            S = S + " Listenport:0";
            /*S:=S+' Port:'+IntToStr(ModemPortEdit.Value)+' Server:'+ModemIPEdit.Text;*/
        }
    }

    if (ui.SerialTypeNullmodemRadioButton->isChecked()) {
        S = "NullModem" + IRQ + " Port:" + IntToStr(ui.NullModemPortEdit->value());
        if (ui.NullModemClientRadioButton->isChecked()) S = S + " Server:" + ui.NullModemServerAddressEdit->text();
        if (ui.NullModemRXDelayEdit->value() != 100) S = S + " RXDelay:" + IntToStr(ui.NullModemRXDelayEdit->value());
        if (ui.NullModemTXDelayEdit->value() != 12) S = S + " TXDelay:" + IntToStr(ui.NullModemTXDelayEdit->value());
        if (ui.NullModemTelnetCheckBox->isChecked()) S = S + " Telnet:1";
        if (ui.NullModemDTRCheckBox->isChecked()) S = S + " UseDTR:1";
        if (ui.NullModemTransparentCheckBox->isChecked()) S = S + " Transparent:1";
    }

    if (ui.SerialTypeDirectSerialRadioButton->isChecked()) {
        S = "DirectSerial" + IRQ + " Realport:" + ui.DirectSerialPortEdit->text();
        if (ui.DirectSerialRXDelayEdit->value() != 100) S = S + " RXDelay:" + IntToStr(ui.DirectSerialRXDelayEdit->value());
    }

    switch (PortNr) {
        case 1: Game->SetSerial1(S); break;
        case 2: Game->SetSerial2(S); break;
        case 3: Game->SetSerial3(S); break;
        case 4: Game->SetSerial4(S); break;
    }
}
