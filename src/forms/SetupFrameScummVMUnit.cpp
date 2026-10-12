// C++ port of SetupFrameScummVMUnit.pas (implementation section).
#include "SetupFrameScummVMUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "ScummVMToolsUnit.h"
#include "SetupDosBoxFormUnit.h"
#include "ListScummVMGamesFormUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameScummVM::TSetupFrameScummVM(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.ScummVMButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ScummVMButton); });
    connect(ui.FindScummVMButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FindScummVMButton); });
    VclLayout::onLabelClick(ui.ScummVMDownloadURL, [this]() { ScummVMDownloadURLClick(ui.ScummVMDownloadURL); });
    connect(ui.ScummVMReadList, &QPushButton::clicked, this, [this]() { ButtonWork(ui.ScummVMReadList); });
    connect(ui.MinimizeDFendScummVMCheckBox, &QCheckBox::clicked, this, [this]() { MinimizeDFendScummVMCheckBoxClick(ui.MinimizeDFendScummVMCheckBox); });
    connect(ui.ScummVMShowListButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.ScummVMShowListButton); });
}

TSetupFrameScummVM::~TSetupFrameScummVM() = default;

// { TSetupFrameScummVM }

QString TSetupFrameScummVM::GetName()
{
    return LanguageSetup->SetupFormScummVMSheet();
}

void TSetupFrameScummVM::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    ui.ScummVMDirEdit->setText(PrgSetup->ScummVMPath());
    ui.MinimizeDFendScummVMCheckBox->setChecked(PrgSetup->MinimizeOnScummVMStart());
    ui.RestoreWindowCheckBox->setChecked(PrgSetup->RestoreWhenScummVMCloses());
    ui.CenterScummVMCheckBox->setChecked(PrgSetup->CenterScummVMWindow());
    ui.HideConsoleCheckBox->setChecked(PrgSetup->HideScummVMConsole());
    ui.CommandLineEdit->setText(PrgSetup->ScummVMAdditionalCommandLine());
    UserIconLoader->DialogImage(DI_SelectFile, ui.ScummVMButton);
    UserIconLoader->DialogImage(DI_FindFile, ui.FindScummVMButton);
    UserIconLoader->DialogImage(DI_ScummVM, ui.ScummVMReadList);
    UserIconLoader->DialogImage(DI_Table, ui.ScummVMShowListButton);
    MinimizeDFendScummVMCheckBoxClick(this);
}

void TSetupFrameScummVM::BeforeChangeLanguage()
{
}

void TSetupFrameScummVM::LoadLanguage()
{
    ui.ScummVMDirEdit_EditLabel->setText(LanguageSetup->SetupFormScummVMDir());
    ui.ScummVMButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.FindScummVMButton->setToolTip(LanguageSetup->SetupFormSearchScummVM());
    ui.ScummVMReadList->setText(LanguageSetup->SetupFormReadScummVMGamesList());
    ui.ScummVMShowListButton->setText(LanguageSetup->WizardFormEmulationTypeListScummVMGames());
    ui.MinimizeDFendScummVMCheckBox->setText(LanguageSetup->SetupFormMinimizeDFendScummVM());
    ui.RestoreWindowCheckBox->setText(LanguageSetup->SetupFormRestoreWhenScummVMCloses());
    ui.CenterScummVMCheckBox->setText(LanguageSetup->SetupFormCenterScummVMWindow());
    ui.HideConsoleCheckBox->setText(LanguageSetup->SetupFormHideScummVMConsole());
    ui.CommandLineEdit_EditLabel->setText(LanguageSetup->SetupFormScummVMAdditionalParameters());
    ui.ScummVMDownloadURLInfo->setText(LanguageSetup->SetupFormScummVMDownloadURL());
    ui.ScummVMDownloadURL->setText(QString("http:/") + "/www.scummvm.org/downloads.php");
    VclLayout::makeLinkLabel(ui.ScummVMDownloadURL);
    setProperty("HelpContext", ID_FileOptionsScummVM);
}

void TSetupFrameScummVM::MinimizeDFendScummVMCheckBoxClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.RestoreWindowCheckBox->setEnabled(ui.MinimizeDFendScummVMCheckBox->isChecked());
}

void TSetupFrameScummVM::DOSBoxDirChanged()
{
}

void TSetupFrameScummVM::ShowFrame(bool AdvancedMode)
{
    ui.MinimizeDFendScummVMCheckBox->setVisible(AdvancedMode);
    ui.RestoreWindowCheckBox->setVisible(AdvancedMode);
    ui.CenterScummVMCheckBox->setVisible(AdvancedMode);
    ui.HideConsoleCheckBox->setVisible(AdvancedMode);
    ui.CommandLineEdit->setVisible(AdvancedMode);
    ui.CommandLineEdit_EditLabel->setVisible(AdvancedMode);
    QWidget *Info = ui.ScummVMDownloadURLInfo;
    if (AdvancedMode) {
        Info->move(Info->x(), ui.CommandLineEdit->y() + ui.CommandLineEdit->height() + 18);
    } else {
        Info->move(Info->x(), ui.MinimizeDFendScummVMCheckBox->y());
    }
    ui.ScummVMDownloadURL->move(ui.ScummVMDownloadURL->x(), Info->y() + 19);
}

void TSetupFrameScummVM::HideFrame()
{
}

void TSetupFrameScummVM::RestoreDefaults()
{
    ui.MinimizeDFendScummVMCheckBox->setChecked(false);
    ui.RestoreWindowCheckBox->setChecked(false);
    ui.CenterScummVMCheckBox->setChecked(false);
    ui.HideConsoleCheckBox->setChecked(false);
    MinimizeDFendScummVMCheckBoxClick(this);
    ui.CommandLineEdit->setText(QString());
}

void TSetupFrameScummVM::SaveSetup()
{
    if (!Trim(ui.ScummVMDirEdit->text()).isEmpty())
        PrgSetup->SetScummVMPath(IncludeTrailingPathDelimiter(ui.ScummVMDirEdit->text()));
    else PrgSetup->SetScummVMPath(QString());
    PrgSetup->SetMinimizeOnScummVMStart(ui.MinimizeDFendScummVMCheckBox->isChecked());
    PrgSetup->SetRestoreWhenScummVMCloses(ui.RestoreWindowCheckBox->isChecked());
    PrgSetup->SetCenterScummVMWindow(ui.CenterScummVMCheckBox->isChecked());
    PrgSetup->SetHideScummVMConsole(ui.HideConsoleCheckBox->isChecked());
    PrgSetup->SetScummVMAdditionalCommandLine(ui.CommandLineEdit->text());
}

void TSetupFrameScummVM::ButtonWork(QObject *Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 16:
            S = ui.ScummVMDirEdit->text();
            if (S.isEmpty()) S = GetSpecialFolder(window()->winId(), CSIDL_PROGRAM_FILES);
            if (SelectDirectory(LanguageSetup->SetupFormScummVMDir(), QString(), S, this)) ui.ScummVMDirEdit->setText(S);
            break;
        case 17:
            if (SearchScummVM(this)) ui.ScummVMDirEdit->setText(PrgSetup->ScummVMPath());
            break;
        case 18:
            ScummVMGamesList->LoadListFromScummVM(true, ui.ScummVMDirEdit->text());
            break;
        case 19:
            ShowListScummVMGamesDialog(this);
            break;
    }
}

void TSetupFrameScummVM::ScummVMDownloadURLClick(QObject *Sender)
{
    ShellOpen(qobject_cast<QLabel *>(Sender)->text());
}
