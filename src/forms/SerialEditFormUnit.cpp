// C++ port of SerialEditFormUnit.pas (implementation section).
#include "SerialEditFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TSerialEditForm* SerialEditForm = nullptr;

TSerialEditForm::TSerialEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM wiring
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.SerialTypeDisabledRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDisabledRadioButton); });
    connect(ui.SerialTypeDummyRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDummyRadioButton); });
    connect(ui.SerialTypeModemRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeModemRadioButton); });
    connect(ui.SerialTypeNullmodemRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeNullmodemRadioButton); });
    connect(ui.SerialTypeDirectSerialRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.SerialTypeDirectSerialRadioButton); });
    connect(ui.ModemListenCheckBox, &QCheckBox::clicked, this, [this]() { DeviceTypeClick(ui.ModemListenCheckBox); });
    connect(ui.NullModemServerRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.NullModemServerRadioButton); });
    connect(ui.NullModemClientRadioButton, &QRadioButton::clicked, this, [this]() { DeviceTypeClick(ui.NullModemClientRadioButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
}

TSerialEditForm::~TSerialEditForm() = default;

void TSerialEditForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);  // OnShow
}

void TSerialEditForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=CharsetNameToFontCharSet(...) - not needed with Qt (Unicode fonts)

    InitGUI();
    DecodeString();
}

void TSerialEditForm::InitGUI()
{
    setWindowTitle(LanguageSetup->SerialFormCaption());

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

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

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TSerialEditForm::SetIRQ(const QString& S)
{
    int I = ui.IRQComboBox->findText(S, Qt::MatchFixedString);  // Items.IndexOf (case insensitive)
    if (I >= 0) ui.IRQComboBox->setCurrentIndex(I);
}

static void Divide(const QString& Data, QString& Name, QString& Value)
{
    int I = Pos(":", Data);
    if (I == 0) { Name = Data; Value = ""; } else { Name = Trim(Copy(Data, 1, I - 1)); Value = Trim(Copy(Data, I + 1, MaxInt)); }
}

void TSerialEditForm::DecodeString()
{
    QString S, Key, DataKey, DataValue;
    TStringList* Data;
    int I, J;

    ui.IRQComboBox->setCurrentIndex(0);

    S = Trim(ExtUpperCase(ProfileString));

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
                if (DataKey == "IRQ") SetIRQ(DataValue);
                /*If DataKey='SERVER' then ModemIPEdit.Text:=DataValue;*/
                /*If DataKey='PORT' then begin try J:=Min(65535,Max(1,StrToInt(DataValue))); except J:=5000; end; ModemPortEdit.Value:=J; end;*/
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

void TSerialEditForm::DeviceTypeClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.IRQComboBox->setEnabled(!ui.SerialTypeDisabledRadioButton->isChecked());

    if (ui.SerialTypeDisabledRadioButton->isChecked() || ui.SerialTypeDummyRadioButton->isChecked()) ui.Notebook->setCurrentIndex(0);
    if (ui.SerialTypeModemRadioButton->isChecked()) ui.Notebook->setCurrentIndex(1);
    if (ui.SerialTypeNullmodemRadioButton->isChecked()) ui.Notebook->setCurrentIndex(2);
    if (ui.SerialTypeDirectSerialRadioButton->isChecked()) ui.Notebook->setCurrentIndex(3);

    ui.ModemIPEdit->setEnabled(!ui.ModemListenCheckBox->isChecked());
    ui.ModemIPEdit_EditLabel->setEnabled(ui.ModemIPEdit->isEnabled());
    ui.ModemPortEdit->setEnabled(ui.ModemListenCheckBox->isChecked());

    ui.NullModemServerAddressEdit->setEnabled(ui.NullModemClientRadioButton->isChecked());
}

void TSerialEditForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString IRQ;

    if (ui.IRQComboBox->isEnabled() && (ui.IRQComboBox->currentIndex() > 0)) IRQ = " IRQ:" + ui.IRQComboBox->currentText(); else IRQ = "";

    if (ui.SerialTypeDisabledRadioButton->isChecked()) {
        ProfileString = "Disabled";
        return;
    }

    if (ui.SerialTypeDummyRadioButton->isChecked()) {
        ProfileString = "Dummy" + IRQ;
        return;
    }

    if (ui.SerialTypeModemRadioButton->isChecked()) {
        ProfileString = "Modem" + IRQ;
        if (ui.ModemListenCheckBox->isChecked()) {
            ProfileString = ProfileString + " Listenport:" + IntToStr(ui.ModemPortEdit->value());
        } else {
            ProfileString = ProfileString + " Listenport:0";
            /*ProfileString:=ProfileString+' Port:'+IntToStr(ModemPortEdit.Value)+' Server:'+ModemIPEdit.Text;*/
        }
        return;
    }

    if (ui.SerialTypeNullmodemRadioButton->isChecked()) {
        ProfileString = "NullModem" + IRQ + " Port:" + IntToStr(ui.NullModemPortEdit->value());
        if (ui.NullModemClientRadioButton->isChecked()) ProfileString = ProfileString + " Server:" + ui.NullModemServerAddressEdit->text();
        if (ui.NullModemRXDelayEdit->value() != 100) ProfileString = ProfileString + " RXDelay:" + IntToStr(ui.NullModemRXDelayEdit->value());
        if (ui.NullModemTXDelayEdit->value() != 12) ProfileString = ProfileString + " TXDelay:" + IntToStr(ui.NullModemTXDelayEdit->value());
        if (ui.NullModemTelnetCheckBox->isChecked()) ProfileString = ProfileString + " Telnet:1";
        if (ui.NullModemDTRCheckBox->isChecked()) ProfileString = ProfileString + " UseDTR:1";
        if (ui.NullModemTransparentCheckBox->isChecked()) ProfileString = ProfileString + " Transparent:1";
        return;
    }

    if (ui.SerialTypeDirectSerialRadioButton->isChecked()) {
        ProfileString = "DirectSerial" + IRQ + " Realport:" + ui.DirectSerialPortEdit->text();
        if (ui.DirectSerialRXDelayEdit->value() != 100) ProfileString = ProfileString + " RXDelay:" + IntToStr(ui.DirectSerialRXDelayEdit->value());
        return;
    }
}

void TSerialEditForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileEditSerialPorts);
}

void TSerialEditForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

// { global }

bool ShowSerialEditDialog(QWidget* AOwner, QString& AProfileString)
{
    bool result;
    SerialEditForm = new TSerialEditForm(AOwner);
    SerialEditForm->ProfileString = AProfileString;
    result = (SerialEditForm->ShowModal() == mrOk);
    if (result) AProfileString = SerialEditForm->ProfileString;
    delete SerialEditForm;
    SerialEditForm = nullptr;
    return result;
}
