// C++ port of SetupFrameQBasicUnit.pas (implementation section).
#include "SetupFrameQBasicUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "SetupDosBoxFormUnit.h"
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

TSetupFrameQBasic::TSetupFrameQBasic(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    PrgOpenDialog = new TOpenDialog(this);
    PrgOpenDialog->DefaultExt = "exe";
    // DFM event wiring
    connect(ui.QBasicButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.QBasicButton); });
    connect(ui.FindQBasicButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FindQBasicButton); });
    new TLabelClickFilter(ui.QBasicDownloadURL, [this]() { QBasicDownloadURLClick(ui.QBasicDownloadURL); });
}

TSetupFrameQBasic::~TSetupFrameQBasic() = default;

// { TSetupFrameQBasic }

QString TSetupFrameQBasic::GetName()
{
    return LanguageSetup->SetupFormQBasicSheet();
}

void TSetupFrameQBasic::InitGUIAndLoadSetup(TInitData &InitData)
{
    PBaseDir = InitData.PBaseDir;
    ui.QBasicEdit->setText(PrgSetup->QBasic());
    ui.QBasicParamEdit->setText(PrgSetup->QBasicParam());
}

void TSetupFrameQBasic::BeforeChangeLanguage()
{
}

void TSetupFrameQBasic::LoadLanguage()
{
    ui.QBasicEdit_EditLabel->setText(LanguageSetup->SetupFormQBasicFile());
    ui.QBasicButton->setToolTip(LanguageSetup->ChooseFile());
    ui.FindQBasicButton->setToolTip(LanguageSetup->SetupFormSearchQBasic());
    ui.QBasicParamEdit_EditLabel->setText(LanguageSetup->SetupFormQBasicParameters());
    ui.QBasicDownloadURLInfo->setText(LanguageSetup->SetupFormQBasicDownloadURL());
    ui.QBasicDownloadURL->setText(QString("http:/") + "/download.microsoft.com/download/win95upg/tool_s/1.0/w95/en-us/olddos.exe");
    MakeLinkLabel(ui.QBasicDownloadURL);
    UserIconLoader->DialogImage(DI_SelectFile, ui.QBasicButton);
    UserIconLoader->DialogImage(DI_FindFile, ui.FindQBasicButton);
    setProperty("HelpContext", ID_FileOptionsQBasic);
}

void TSetupFrameQBasic::DOSBoxDirChanged()
{
}

void TSetupFrameQBasic::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameQBasic::HideFrame()
{
}

void TSetupFrameQBasic::RestoreDefaults()
{
    ui.QBasicParamEdit->setText("/run %s");
}

void TSetupFrameQBasic::SaveSetup()
{
    PrgSetup->SetQBasic(ui.QBasicEdit->text());
    PrgSetup->SetQBasicParam(ui.QBasicParamEdit->text());
}

void TSetupFrameQBasic::ButtonWork(QObject *Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 19:
            S = Trim(ui.QBasicEdit->text()); if (!S.isEmpty()) S = ExtractFilePath(S);
            if (Trim(S).isEmpty()) S = IncludeTrailingPathDelimiter(*PBaseDir);
            S = MakeAbsPath(S, IncludeTrailingPathDelimiter(*PBaseDir));
            /* If S='' then S:=GetSpecialFolder(Application.MainForm.Handle,CSIDL_PROGRAM_FILES); */
            PrgOpenDialog->Title = LanguageSetup->SetupFormQBasicFile();
            PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
            if (!S.isEmpty()) PrgOpenDialog->InitialDir = S;
            if (PrgOpenDialog->Execute()) {
                ui.QBasicEdit->setText(PrgOpenDialog->FileName);
            }
            break;
        case 20:
            if (SearchQBasicVM(this)) ui.QBasicEdit->setText(PrgSetup->QBasic());
            break;
    }
}

void TSetupFrameQBasic::QBasicDownloadURLClick(QObject *Sender)
{
    ShellOpen(qobject_cast<QLabel *>(Sender)->text());
}
