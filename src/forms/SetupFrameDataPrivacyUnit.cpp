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

TSetupFrameDataPrivacy::TSetupFrameDataPrivacy(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    new TLabelClickFilter(ui.OpenFileLabel, [this]() { OpenFileLabelClick(ui.OpenFileLabel); });
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
    MakeLinkLabel(ui.OpenFileLabel);
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
