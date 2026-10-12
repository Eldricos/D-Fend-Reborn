// C++ port of SetupFrameDataPrivacyUnit.pas (implementation section).
#include "SetupFrameDataPrivacyUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "HistoryFormUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "HistoryUnit.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameDataPrivacy::TSetupFrameDataPrivacy(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    VclLayout::onLabelClick(ui.OpenFileLabel, [this]() { OpenFileLabelClick(ui.OpenFileLabel); });
    connect(ui.ShowHistoryButton, &QPushButton::clicked, this, [this]() { ShowHistoryButtonClick(ui.ShowHistoryButton); });
    connect(ui.DeleteHistoryButton, &QPushButton::clicked, this, [this]() { DeleteHistoryButtonClick(ui.DeleteHistoryButton); });
}

TSetupFrameDataPrivacy::~TSetupFrameDataPrivacy() = default;

// { TSetupFrameDataPrivacy }

QString TSetupFrameDataPrivacy::GetName()
{
    return LanguageSetup->SetupFormDataPrivacy();
}

void TSetupFrameDataPrivacy::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    UserIconLoader->DirectLoad("Main", 26, ui.ShowHistoryButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteHistoryButton);
    VclLayout::makeLinkLabel(ui.OpenFileLabel);
    ui.StoreHistoryCheckBox->setChecked(PrgSetup->StoreHistory());
}

void TSetupFrameDataPrivacy::BeforeChangeLanguage()
{
}

void TSetupFrameDataPrivacy::LoadLanguage()
{
    ui.StoreHistoryCheckBox->setText(LanguageSetup->SetupFormDataPrivacyStoreHistory());
    ui.ShowHistoryButton->setText(LanguageSetup->SetupFormDataPrivacyShowHistory());
    ui.DeleteHistoryButton->setText(LanguageSetup->SetupFormDataPrivacyDeleteHistory());
    ui.OpenFileLabel->setText(LanguageSetup->SetupFormDataPrivacyOpenHistoryFile());
    ui.InfoLabel->setText(LanguageSetup->SetupFormDataPrivacyInfo());
    setProperty("HelpContext", ID_FileOptionsDataPrivacy);
}

void TSetupFrameDataPrivacy::DOSBoxDirChanged()
{
}

void TSetupFrameDataPrivacy::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameDataPrivacy::HideFrame()
{
}

void TSetupFrameDataPrivacy::RestoreDefaults()
{
    ui.StoreHistoryCheckBox->setChecked(true);
}

void TSetupFrameDataPrivacy::SaveSetup()
{
    PrgSetup->SetStoreHistory(ui.StoreHistoryCheckBox->isChecked());
}

void TSetupFrameDataPrivacy::ShowHistoryButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ShowHistoryDialog(this);
}

void TSetupFrameDataPrivacy::DeleteHistoryButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (History->Clear()) MessageDlg(LanguageSetup->SetupFormDataPrivacyDeleteHistoryDone(), mtInformation, mbOK, 0, this);
}

void TSetupFrameDataPrivacy::OpenFileLabelClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    OpenFileInEditor(PrgDataDir() + SettingsFolder + PathDelim + HistoryFileName);
}
