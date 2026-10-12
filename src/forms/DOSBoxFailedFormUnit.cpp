// C++ port of DOSBoxFailedFormUnit.pas (implementation section).
#include "DOSBoxFailedFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "PrgSetupUnit.h"

TDOSBoxFailedForm* DOSBoxFailedForm = nullptr;

TDOSBoxFailedForm::TDOSBoxFailedForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
}

TDOSBoxFailedForm::~TDOSBoxFailedForm() = default;

void TDOSBoxFailedForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TDOSBoxFailedForm::done(int R)
{
    // OnClose: closing a QDialog (ModalResult button, Esc, close box) always ends in done()
    TCloseAction Action = caHide;
    FormClose(this, Action);
    if (Action == caNone) return;
    TVclForm::done(R);
}

void TDOSBoxFailedForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    TStringList* St;

    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    setWindowTitle(LanguageSetup->DOSBoxStartFailed());
    ui.InfoLabel->setText(LanguageSetup->DOSBoxStartFailedInfo());

    ui.ProfileGroupBox->setTitle(LanguageSetup->DOSBoxStartFailedProfileSettings());
    ui.GlobalGroupBox->setTitle(LanguageSetup->DOSBoxStartFailedGlobalSettings());
    ui.CloseDOSBoxCheckBox->setText(LanguageSetup->GameCloseDOSBoxAfterGameExit());
    ui.ShowConsoleCheckBox->setText(LanguageSetup->DOSBoxStartFailedShowConsole());
    ui.RenderLabel->setText(LanguageSetup->GameRender());
    St = ValueToList(GameDB->ConfOpt()->Render(), ";,");
    ui.RenderComboBox->addItems(St->ToQStringList());
    delete St;
    ui.SDLLabel->setText(LanguageSetup->SetupFormDOSBoxSDLVideodriver());
    ui.SDLComboBox->setItemText(0, ui.SDLComboBox->itemText(0) + " (" + LanguageSetup->Default() + ")");
    ui.RemoteLabel->setText(LanguageSetup->DOSBoxStartFailedRemoteInfo());
    ui.DoNotShowAgainCheckBox->setText(LanguageSetup->DOSBoxStartFailedTurnOff());

    ui.OKButton->setText(LanguageSetup->OK());
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);

    LoadSettings();
    ui.DoNotShowAgainCheckBox->setChecked(false);
}

void TDOSBoxFailedForm::LoadSettings()
{
    QString S, T;

    ui.CloseDOSBoxCheckBox->setChecked(Game->CloseDosBoxAfterGameExit());

    DOSBoxNr = -1;
    S = Trim(ExtUpperCase(Game->CustomDOSBoxDir()));
    if (S.isEmpty()) S = "DEFAULT";
    for (int I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) {
        if (I == 0) T = "DEFAULT"; else T = PrgSetup->DOSBoxSettings(I)->Name();
        if (S == Trim(ExtUpperCase(T))) { DOSBoxNr = I; break; }
    }
    if (DOSBoxNr < 0) DOSBoxNr = 0; // {Get setting from default DOSBox profile if custom path}

    switch (Game->ShowConsoleWindow()) {
        case 0: ui.ShowConsoleCheckBox->setChecked(false); break;
        case 2: ui.ShowConsoleCheckBox->setChecked(true); break;
        default: /* 1 */ ui.ShowConsoleCheckBox->setChecked(!PrgSetup->DOSBoxSettings(DOSBoxNr)->HideDosBoxConsole()); break;
    }

    S = Trim(ExtUpperCase(Game->Render()));
    ui.RenderComboBox->setCurrentIndex(0);
    for (int I = 0; I < ui.RenderComboBox->count(); I++) if (Trim(ExtUpperCase(ui.RenderComboBox->itemText(I))) == S) {
        ui.RenderComboBox->setCurrentIndex(I); break;
    }

    if (Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(0)->SDLVideodriver())) == "WINDIB") ui.SDLComboBox->setCurrentIndex(1); else ui.SDLComboBox->setCurrentIndex(0);
    ui.RemoteLabel->setVisible(IsRemoteSession() && (ui.SDLComboBox->currentIndex() == 0));
}

void TDOSBoxFailedForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Sender);
    Q_UNUSED(Action);
    Game->SetCloseDosBoxAfterGameExit(ui.CloseDOSBoxCheckBox->isChecked());
    if (PrgSetup->DOSBoxSettings(DOSBoxNr)->HideDosBoxConsole()) {
        if (ui.ShowConsoleCheckBox->isChecked()) Game->SetShowConsoleWindow(2); else Game->SetShowConsoleWindow(1);
    } else {
        if (ui.ShowConsoleCheckBox->isChecked()) Game->SetShowConsoleWindow(1); else Game->SetShowConsoleWindow(0);
    }
    Game->SetRender(ui.RenderComboBox->currentText());
    Game->SetNoDOSBoxFailedDialog(ui.DoNotShowAgainCheckBox->isChecked());
    Game->StoreAllValues();

    if (ui.SDLComboBox->currentIndex() == 1) PrgSetup->DOSBoxSettings(0)->SetSDLVideodriver("WinDIB"); else PrgSetup->DOSBoxSettings(0)->SetSDLVideodriver("DirectX");
}

// { global }

void ShowDOSBoxFailedDialog(QWidget* AOwner, TGameDB* AGameDB, TGame* AGame)
{
    if (AGame->NoDOSBoxFailedDialog()) return;
    DOSBoxFailedForm = new TDOSBoxFailedForm(AOwner);
    DOSBoxFailedForm->GameDB = AGameDB;
    DOSBoxFailedForm->Game = AGame;
    DOSBoxFailedForm->ShowModal();
    delete DOSBoxFailedForm;
    DOSBoxFailedForm = nullptr;
}
