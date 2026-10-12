// C++ port of SetupFrameFreeDOSUnit.pas (implementation section).
#include "SetupFrameFreeDOSUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameFreeDOS::TSetupFrameFreeDOS(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.PathFREEDOSButton, &QToolButton::clicked, this, [this]() { PathFREEDOSButtonClick(ui.PathFREEDOSButton); });
    VclLayout::onLabelClick(ui.FreeDOSDownloadURL, [this]() { FreeDOSDownloadURLClick(ui.FreeDOSDownloadURL); });
}

TSetupFrameFreeDOS::~TSetupFrameFreeDOS() = default;

// { TSetupFrameFreeDOS }

QString TSetupFrameFreeDOS::GetName()
{
    return LanguageSetup->SetupFormFreeDOS();
}

void TSetupFrameFreeDOS::InitGUIAndLoadSetup(TInitData &InitData)
{
    PBaseDir = InitData.PBaseDir;
    ui.PathFREEDOSEdit->setText(PrgSetup->PathToFREEDOS());
}

void TSetupFrameFreeDOS::BeforeChangeLanguage()
{
}

void TSetupFrameFreeDOS::LoadLanguage()
{
    ui.PathFREEDOSEdit_EditLabel->setText(LanguageSetup->SetupFormDOSBoxFREEDOSPath());
    ui.PathFREEDOSButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.InfoLabel->setText(LanguageSetup->SetupFormFreeDOSInfo());
    ui.FreeDOSDownloadURLInfo->setText(LanguageSetup->SetupFormFreeDOSDownloadURL());
    ui.FreeDOSDownloadURL->setText(QString("http:/") + "/www.freedos.org/");
    VclLayout::makeLinkLabel(ui.FreeDOSDownloadURL);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.PathFREEDOSButton);
    setProperty("HelpContext", ID_FileOptionsFreeDOS);
}

void TSetupFrameFreeDOS::DOSBoxDirChanged()
{
}

void TSetupFrameFreeDOS::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameFreeDOS::HideFrame()
{
}

void TSetupFrameFreeDOS::RestoreDefaults()
{
}

void TSetupFrameFreeDOS::SaveSetup()
{
    PrgSetup->SetPathToFREEDOS(ui.PathFREEDOSEdit->text());
}

void TSetupFrameFreeDOS::PathFREEDOSButtonClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = ui.PathFREEDOSEdit->text();
    if (Trim(S).isEmpty()) S = IncludeTrailingPathDelimiter(*PBaseDir);
    S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
    if (SelectDirectory(LanguageSetup->SetupFormFreeDOSDir(), QString(), S, this)) {
        ui.PathFREEDOSEdit->setText(MakeRelPath(S, IncludeTrailingPathDelimiter(*PBaseDir), true));
    }
}

void TSetupFrameFreeDOS::FreeDOSDownloadURLClick(QObject *Sender)
{
    ShellOpen(qobject_cast<QLabel *>(Sender)->text());
}
