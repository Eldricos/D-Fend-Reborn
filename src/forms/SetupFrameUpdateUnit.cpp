// C++ port of SetupFrameUpdateUnit.pas (implementation section).
#include "SetupFrameUpdateUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "CommonTools.h"
#include "UpdateCheckFormUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameUpdate::TSetupFrameUpdate(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.UpdateButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.UpdateButton); });
}

TSetupFrameUpdate::~TSetupFrameUpdate() = default;

// { TSetupFrameUpdate }

QString TSetupFrameUpdate::GetName()
{
    QString result = LanguageSetup->MenuHelpUpdates();
    while (!result.isEmpty() && (result[Length(result) - 1] == '.')) result.truncate(Length(result) - 1);
    return result;
}

void TSetupFrameUpdate::InitGUIAndLoadSetup(TInitData &InitData)
{
    ui.UpdateCheckBox->setChecked(PrgSetup->VersionSpecificUpdateCheck());
    UserIconLoader->DialogImage(DI_Update, ui.UpdateButton);
    ui.ProgramUpdateComboBox->clear();
    while (ui.ProgramUpdateComboBox->count() < 4) ui.ProgramUpdateComboBox->addItem(QString());
    ui.DataReaderComboBox->clear();
    while (ui.DataReaderComboBox->count() < 4) ui.DataReaderComboBox->addItem(QString());
    ui.PackagesComboBox->clear();
    while (ui.PackagesComboBox->count() < 4) ui.PackagesComboBox->addItem(QString());
    ui.CheatsComboBox->clear();
    while (ui.CheatsComboBox->count() < 3) ui.CheatsComboBox->addItem(QString());
    ui.ProgramUpdateComboBox->setCurrentIndex(Max(0, Min(3, PrgSetup->CheckForUpdates())));
    ui.DataReaderComboBox->setCurrentIndex(Max(0, Min(3, PrgSetup->DataReaderCheckForUpdates())));
    ui.PackagesComboBox->setCurrentIndex(Max(0, Min(3, PrgSetup->PackageListsCheckForUpdates())));
    ui.CheatsComboBox->setCurrentIndex(Max(0, Min(2, PrgSetup->CheatsDBCheckForUpdates())));
    GameDB = InitData.GameDB;
    SearchLinkFile = InitData.SearchLinkFile;
}

void TSetupFrameUpdate::BeforeChangeLanguage()
{
}

void TSetupFrameUpdate::LoadLanguage()
{
    int I;
    ui.UpdateCheckBox->setText(LanguageSetup->SetupFormUpdateVersionSpecific());
    ui.UpdateButton->setText(LanguageSetup->SetupFormUpdateButton());
    ui.UpdateButton->resize(Max(169, 40 + ui.UpdateButton->fontMetrics().horizontalAdvance(LanguageSetup->SetupFormUpdateButton())), ui.UpdateButton->height());
    ui.ProgramUpdateInfoLabel->setText(LanguageSetup->SetupFormUpdateProgram());
    I = ui.ProgramUpdateComboBox->currentIndex();
    ui.ProgramUpdateComboBox->setItemText(0, LanguageSetup->SetupFormUpdate0());
    ui.ProgramUpdateComboBox->setItemText(1, LanguageSetup->SetupFormUpdate1());
    ui.ProgramUpdateComboBox->setItemText(2, LanguageSetup->SetupFormUpdate2());
    ui.ProgramUpdateComboBox->setItemText(3, LanguageSetup->SetupFormUpdate3());
    ui.ProgramUpdateComboBox->setCurrentIndex(I);
    ui.DataReaderInfoLabel->setText(LanguageSetup->SetupFormUpdateDataReader());
    I = ui.DataReaderComboBox->currentIndex();
    ui.DataReaderComboBox->setItemText(0, LanguageSetup->SetupFormUpdateDataReader0());
    ui.DataReaderComboBox->setItemText(1, LanguageSetup->SetupFormUpdateDataReader1());
    ui.DataReaderComboBox->setItemText(2, LanguageSetup->SetupFormUpdateDataReader2());
    ui.DataReaderComboBox->setItemText(3, LanguageSetup->SetupFormUpdateDataReader3());
    ui.DataReaderComboBox->setCurrentIndex(I);
    // D-Fend Reborn: the MobyGames reader (configuration updates) is not used any more
    ui.DataReaderInfoLabel->setVisible(false);
    ui.DataReaderComboBox->setVisible(false);
    ui.PackagesLabel->setText(LanguageSetup->SetupFormUpdatePackages());
    I = ui.PackagesComboBox->currentIndex();
    ui.PackagesComboBox->setItemText(0, LanguageSetup->SetupFormUpdatePackages0());
    ui.PackagesComboBox->setItemText(1, LanguageSetup->SetupFormUpdatePackages1());
    ui.PackagesComboBox->setItemText(2, LanguageSetup->SetupFormUpdatePackages2());
    ui.PackagesComboBox->setItemText(3, LanguageSetup->SetupFormUpdatePackages3());
    ui.PackagesComboBox->setCurrentIndex(I);
    ui.CheatsLabel->setText(LanguageSetup->SetupFormUpdateCheats());
    I = ui.CheatsComboBox->currentIndex();
    ui.CheatsComboBox->setItemText(0, LanguageSetup->SetupFormUpdateCheats0());
    ui.CheatsComboBox->setItemText(1, LanguageSetup->SetupFormUpdateCheats1());
    ui.CheatsComboBox->setItemText(2, LanguageSetup->SetupFormUpdateCheats2());
    ui.CheatsComboBox->setCurrentIndex(I);
    setProperty("HelpContext", ID_FileOptionsSearchForUpdates);
}

void TSetupFrameUpdate::DOSBoxDirChanged()
{
}

void TSetupFrameUpdate::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameUpdate::HideFrame()
{
}

void TSetupFrameUpdate::RestoreDefaults()
{
    ui.UpdateCheckBox->setChecked(true);
    ui.ProgramUpdateComboBox->setCurrentIndex(0);
    ui.DataReaderComboBox->setCurrentIndex(2);
    ui.PackagesComboBox->setCurrentIndex(0);
    ui.CheatsComboBox->setCurrentIndex(0);
}

void TSetupFrameUpdate::SaveSetup()
{
    PrgSetup->SetVersionSpecificUpdateCheck(ui.UpdateCheckBox->isChecked());
    PrgSetup->SetCheckForUpdates(ui.ProgramUpdateComboBox->currentIndex());
    PrgSetup->SetDataReaderCheckForUpdates(ui.DataReaderComboBox->currentIndex());
    PrgSetup->SetPackageListsCheckForUpdates(ui.PackagesComboBox->currentIndex());
    PrgSetup->SetCheatsDBCheckForUpdates(ui.CheatsComboBox->currentIndex());
}

void TSetupFrameUpdate::ButtonWork(QObject *Sender)
{
    Q_UNUSED(Sender);
    ShowUpdateCheckDialog(this, GameDB, SearchLinkFile, true);
}
