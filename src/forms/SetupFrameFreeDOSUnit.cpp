// C++ port of SetupFrameFreeDOSUnit.pas (implementation section).
#include "SetupFrameFreeDOSUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

namespace {
// VCL TLabel.OnClick: QLabel has no clicked signal, so a small event filter
// forwards left mouse button releases to the handler.
class TLabelClickFilter : public QObject
{
public:
    TLabelClickFilter(QLabel *Label, std::function<void()> Handler) : QObject(Label), FHandler(std::move(Handler)) { Label->installEventFilter(this); }
protected:
    bool eventFilter(QObject *Watched, QEvent *Event) override
    {
        if (Event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(Event)->button() == Qt::LeftButton) {
            FHandler();
            return true;
        }
        return QObject::eventFilter(Watched, Event);
    }
private:
    std::function<void()> FHandler;
};

// with Label.Font do begin Color:=clBlue; Style:=[fsUnderline]; end; Label.Cursor:=crHandPoint;
void MakeLinkLabel(QLabel *Label)
{
    QPalette P = Label->palette();
    P.setColor(QPalette::WindowText, TColorToQColor(clBlue));
    Label->setPalette(P);
    QFont F = Label->font();
    F.setUnderline(true);
    Label->setFont(F);
    Label->setCursor(Qt::PointingHandCursor);
}
} // namespace

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameFreeDOS::TSetupFrameFreeDOS(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.PathFREEDOSButton, &QToolButton::clicked, this, [this]() { PathFREEDOSButtonClick(ui.PathFREEDOSButton); });
    new TLabelClickFilter(ui.FreeDOSDownloadURL, [this]() { FreeDOSDownloadURLClick(ui.FreeDOSDownloadURL); });
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
    MakeLinkLabel(ui.FreeDOSDownloadURL);
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
