// C++ port of SetupFrameSurfaceUnit.pas (implementation section).
#include "SetupFrameSurfaceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "PrgConsts.h"
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

namespace {
// TCheckListBox helpers
void AddCheckItem(QListWidget *L, const QString &S)
{
    auto *Item = new QListWidgetItem(S, L);
    Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
    Item->setCheckState(Qt::Unchecked);
}
bool IsChecked(QListWidget *L, int I) { return L->item(I)->checkState() == Qt::Checked; }
void SetChecked(QListWidget *L, int I, bool B) { L->item(I)->setCheckState(B ? Qt::Checked : Qt::Unchecked); }
} // namespace

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameSurface::TSetupFrameSurface(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    new TLabelClickFilter(ui.IconCustomizeLabel, [this]() { IconCustomizeLabelClick(ui.IconCustomizeLabel); });
    connect(ui.IconSetListbox, &QListWidget::itemChanged, this, [this]() { IconSetListboxClickCheck(ui.IconSetListbox); });   // OnClickCheck
    connect(ui.IconSetListbox, &QListWidget::currentRowChanged, this, [this]() { IconSetListboxClick(ui.IconSetListbox); });  // OnClick
}

TSetupFrameSurface::~TSetupFrameSurface()
{
    delete ShortName;
    delete LongName;
    delete Author;
}

// { TSetupFrameSurface }

QString TSetupFrameSurface::GetName()
{
    return LanguageSetup->SetupFormGUI();
}

void TSetupFrameSurface::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    int I;
    QString S;
    QListWidget *L = ui.IconSetListbox;
    /* NoFlicker(IconSetListbox); - will cause trouble */
    ui.StartSizeComboBox->setCurrentIndex(Max(0, Min(ui.StartSizeComboBox->count() - 1, PrgSetup->StartWindowSize())));
    ShortName = new TStringList;
    LongName = new TStringList;
    Author = new TStringList;
    ListOfIconSets(ShortName, LongName, Author, nullptr);
    {
        // programmatic changes do not fire OnClick/OnClickCheck in the VCL
        const QSignalBlocker Blocker(L);
        for (I = 0; I < LongName->Count(); I++) AddCheckItem(L, (*LongName)[I]);
        if (L->count() > 0) L->setCurrentRow(0);
        S = ExtUpperCase(PrgSetup->IconSet());
        for (I = 0; I < ShortName->Count(); I++) if (S == ExtUpperCase((*ShortName)[I])) {
            SetChecked(L, I, true); L->setCurrentRow(I); break;
        }
    }
    IconSetListboxClick(this);
    MakeLinkLabel(ui.IconCustomizeLabel);
}

void TSetupFrameSurface::BeforeChangeLanguage()
{
}

void TSetupFrameSurface::LoadLanguage()
{
    int I;
    ui.StartSizeLabel->setText(LanguageSetup->SetupFormStartSizeLabel());
    I = ui.StartSizeComboBox->currentIndex();
    while (ui.StartSizeComboBox->count() < 5) ui.StartSizeComboBox->addItem(QString());
    ui.StartSizeComboBox->setItemText(0, LanguageSetup->SetupFormStartSizeNormal());
    ui.StartSizeComboBox->setItemText(1, LanguageSetup->SetupFormStartSizeLast());
    ui.StartSizeComboBox->setItemText(2, LanguageSetup->SetupFormStartSizeMinimized());
    ui.StartSizeComboBox->setItemText(3, LanguageSetup->SetupFormStartSizeMaximized());
    ui.StartSizeComboBox->setItemText(4, LanguageSetup->SetupFormStartSizeFullscreen());
    ui.StartSizeComboBox->setCurrentIndex(I);
    ui.IconSetLabel->setText(LanguageSetup->SetupFormIconSet());
    ui.IconSetPreviewLabel->setText(LanguageSetup->SetupFormIconSetPreview());
    ui.IconSetAuthorLabel->setText(LanguageSetup->SetupFormIconSetAuthor());
    ui.IconCustomizeLabel->setText(LanguageSetup->SetupFormIconSetCustomize());
    setProperty("HelpContext", ID_FileOptionsUserInterface);
}

void TSetupFrameSurface::DOSBoxDirChanged()
{
}

void TSetupFrameSurface::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameSurface::HideFrame()
{
}

void TSetupFrameSurface::RestoreDefaults()
{
    ui.StartSizeComboBox->setCurrentIndex(0);
}

void TSetupFrameSurface::SaveSetup()
{
    QString S;
    int I;
    PrgSetup->SetStartWindowSize(ui.StartSizeComboBox->currentIndex());
    S.clear();
    for (I = 0; I < ui.IconSetListbox->count(); I++) if (IsChecked(ui.IconSetListbox, I)) { S = (*ShortName)[I]; break; }
    if (!S.isEmpty()) PrgSetup->SetIconSet(S);
}

void TSetupFrameSurface::IconSetListboxClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    QImage B;
    const int Index = ui.IconSetListbox->currentRow();
    if (Index >= 0 && ShortName != nullptr && Index < ShortName->Count()) {
        ui.IconSetAuthorNameLabel->setText((*Author)[Index]);
        B = GetExampleImage((*ShortName)[Index]);
        if (B.isNull()) {
            ui.IconSetImage->clearPicture();
        } else {
            ui.IconSetImage->setPicture(QPixmap::fromImage(B));
        }
    } else {
        ui.IconSetAuthorNameLabel->setText(QString());
        ui.IconSetImage->clearPicture();
    }
}

void TSetupFrameSurface::IconSetListboxClickCheck(QObject *Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    bool B;
    QListWidget *L = ui.IconSetListbox;
    // programmatic Checked changes do not fire OnClickCheck in the VCL
    const QSignalBlocker Blocker(L);
    const int ItemIndex = L->currentRow();
    /* No entry checked */
    B = false;
    for (I = 0; I < L->count(); I++) if (IsChecked(L, I)) { B = true; break; }
    if (!B) {
        if (ItemIndex >= 0) SetChecked(L, ItemIndex, true); else {
            if (L->count() > 0) SetChecked(L, 0, true);
        }
        return;
    }
    /* Remove multiple checks */
    J = 0; for (I = 0; I < L->count(); I++) if (IsChecked(L, I)) J++;
    if (J > 1) {
        if ((ItemIndex >= 0) && IsChecked(L, ItemIndex)) {
            for (I = 0; I < L->count(); I++) SetChecked(L, I, I == ItemIndex);
        } else {
            B = false;
            for (I = 0; I < L->count(); I++) if (IsChecked(L, I)) {
                if (B) SetChecked(L, I, false); else B = true;
            }
        }
    }
}

void TSetupFrameSurface::IconCustomizeLabelClick(QObject *Sender)
{
    Q_UNUSED(Sender);
    const QString FileName = PrgDataDir() + SettingsFolder + PathDelim + IconsConfFile;
    if (FileExists(FileName))
        OpenFileInEditor(FileName);
    else MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {FileName}), mtError, mbOK, 0, this);
}
