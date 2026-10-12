// C++ port of SetupFrameDOSBoxUnit.pas (implementation section).
#include "SetupFrameDOSBoxUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "SetupDosBoxFormUnit.h"
#include "SetupFrameDOSBoxFormUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "Process.h"

static constexpr int CSIDL_PROGRAM_FILES_ = 0x0026;   // ShlObj.CSIDL_PROGRAM_FILES

namespace {

// ComboBox.Items.Exchange(I1,I2)
void ComboExchange(QComboBox *Combo, int I1, int I2)
{
    QString S = Combo->itemText(I1);
    Combo->setItemText(I1, Combo->itemText(I2));
    Combo->setItemText(I2, S);
}

// Control.Top:=Value
void SetTop(QWidget *W, int Top)
{
    QRect G = W->geometry();
    VclLayout::setBounds(W, G.x(), Top, G.width(), G.height());
}

}  // namespace

TSetupFrameDOSBox::TSetupFrameDOSBox(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DosBoxTxtOpenDialog (DFM TOpenDialog) is not used by the code

    // DFM event wiring
    connect(ui.DosBoxButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DosBoxButton); });
    connect(ui.FindDosBoxButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FindDosBoxButton); });
    VclLayout::onLabelClick(ui.DOSBoxDownloadURL, [this]() { DOSBoxDownloadURLClick(ui.DOSBoxDownloadURL); });
    connect(ui.DOSBoxAddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DOSBoxAddButton); });
    connect(ui.DOSBoxDeleteButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DOSBoxDeleteButton); });
    connect(ui.DOSBoxDownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DOSBoxDownButton); });
    connect(ui.DOSBoxUpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DOSBoxUpButton); });
    connect(ui.DOSBoxEditButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DOSBoxEditButton); });
    connect(ui.WarningButton, &QToolButton::clicked, this, [this]() { WarningButtonClick(ui.WarningButton); });
    connect(ui.DosBoxDirEdit, &QLineEdit::textChanged, this, [this]() { DosBoxDirEditChange(ui.DosBoxDirEdit); });
    connect(ui.DOSBoxInstallationComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { DOSBoxInstallationComboBoxChange(ui.DOSBoxInstallationComboBox); });
    connect(ui.MoreSettingsButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.MoreSettingsButton); });
    connect(ui.PortableModeInfoButton, &QPushButton::clicked, this, [this]() { PortableModeInfoButtonClick(ui.PortableModeInfoButton); });

    OrigPortableModeInfoButtonTop = -1;
}

TSetupFrameDOSBox::~TSetupFrameDOSBox()
{
}

/* TSetupFrameDOSBox */

QString TSetupFrameDOSBox::GetName()
{
    return LanguageSetup->SetupFormDOSBoxSheet();
}

void TSetupFrameDOSBox::InitGUIAndLoadSetup(TInitData& InitData)
{
    int I;

    PDosBoxDir = InitData.PDosBoxDir;
    PDOSBoxLang = InitData.PDOSBoxLang;
    DosBoxDirChange = InitData.DosBoxDirChangeNotify;
    GameDB = InitData.GameDB;

    // NoFlicker(...): not needed in Qt

    DOSBoxData.clear();
    for (I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) {
        TDOSBoxData D;
        DOSBoxSettingToDOSBoxData(PrgSetup->DOSBoxSettings(I), D);
        DOSBoxData.append(D);
        if (I == 0)
            ui.DOSBoxInstallationComboBox->addItem(LanguageSetup->Default());
        else
            ui.DOSBoxInstallationComboBox->addItem(DOSBoxData[I].Name);
    }

    LastIndex = -1;
    ui.DOSBoxInstallationComboBox->setCurrentIndex(0);
    DOSBoxInstallationComboBoxChange(this);

    UserIconLoader->DialogImage(DI_Edit, ui.DOSBoxEditButton);
    UserIconLoader->DialogImage(DI_Add, ui.DOSBoxAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DOSBoxDeleteButton);
    UserIconLoader->DialogImage(DI_Up, ui.DOSBoxUpButton);
    UserIconLoader->DialogImage(DI_Down, ui.DOSBoxDownButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DosBoxButton);
    UserIconLoader->DialogImage(DI_FindFile, ui.FindDosBoxButton);

    setProperty("HelpContext", ID_FileOptionsDOSBox);
}

void TSetupFrameDOSBox::BeforeChangeLanguage()
{
}

void TSetupFrameDOSBox::LoadLanguage()
{
    ui.DOSBoxInstallationLabel->setText(LanguageSetup->SetupFormDOSBoxInstallation());
    ui.DOSBoxEditButton->setToolTip(LanguageSetup->SetupFormDOSBoxInstallationEdit());
    ui.DOSBoxAddButton->setToolTip(LanguageSetup->SetupFormDOSBoxInstallationAdd());
    ui.DOSBoxDeleteButton->setToolTip(LanguageSetup->SetupFormDOSBoxInstallationDelete());
    ui.DOSBoxUpButton->setToolTip(LanguageSetup->SetupFormDOSBoxInstallationUp());
    ui.DOSBoxDownButton->setToolTip(LanguageSetup->SetupFormDOSBoxInstallationDown());
    ui.DosBoxDirEdit_EditLabel->setText(LanguageSetup->SetupFormDOSBoxDir());
    ui.WarningButton->setToolTip(LanguageSetup->MsgDlgWarning());
    ui.DosBoxButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.FindDosBoxButton->setToolTip(LanguageSetup->SetupFormSearchDOSBox());
    ui.MoreSettingsButton->setText(LanguageSetup->SetupFormDOSBoxInstallationMoreSettings());
    ui.PortableModeInfoButton->setText(LanguageSetup->SetupFormDOSBoxPortableModeInfo());

    ui.DOSBoxDownloadURLInfo->setText(LanguageSetup->SetupFormDOSBoxDownloadURL());
    ui.DOSBoxDownloadURL->setText(QString("http:/") + "/www.dosbox.com/download.php?main=1");
    VclLayout::makeLinkLabel(ui.DOSBoxDownloadURL);
}

void TSetupFrameDOSBox::PortableModeInfoButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    MessageDlg(LanguageSetup->FirstRunWizardInfoDOSBox3(), mtInformation, mbOK, 0);
}

void TSetupFrameDOSBox::DOSBoxDirChanged()
{
}

void TSetupFrameDOSBox::ShowFrame(bool AdvancedMode)
{
    ui.MoreSettingsButton->setVisible(AdvancedMode);
    ui.PortableModeInfoButton->setVisible(OperationMode == omPortable);

    if (OrigPortableModeInfoButtonTop < 0)
        OrigPortableModeInfoButtonTop = ui.PortableModeInfoButton->y();
    else
        SetTop(ui.PortableModeInfoButton, OrigPortableModeInfoButtonTop);

    if (AdvancedMode) {
        if (ui.PortableModeInfoButton->isVisibleTo(this))
            SetTop(ui.DOSBoxDownloadURLInfo, ui.PortableModeInfoButton->y() + ui.PortableModeInfoButton->height() + 10);
        else
            SetTop(ui.DOSBoxDownloadURLInfo, ui.MoreSettingsButton->y() + ui.MoreSettingsButton->height() + 10);
    } else {
        SetTop(ui.PortableModeInfoButton, ui.MoreSettingsButton->y());
        if (ui.PortableModeInfoButton->isVisibleTo(this))
            SetTop(ui.DOSBoxDownloadURLInfo, ui.PortableModeInfoButton->y() + ui.PortableModeInfoButton->height() + 10);
        else
            SetTop(ui.DOSBoxDownloadURLInfo, ui.DosBoxDirEdit->y() + ui.DosBoxDirEdit->height() + 15);
    }
    SetTop(ui.DOSBoxDownloadURL, ui.DOSBoxDownloadURLInfo->y() + 19);

    DosBoxDirEditChange(this);
}

void TSetupFrameDOSBox::HideFrame()
{
}

void TSetupFrameDOSBox::RestoreDefaults()
{
}

void TSetupFrameDOSBox::SaveSetup()
{
    int I;

    ui.DOSBoxInstallationComboBox->setCurrentIndex(-1);
    DOSBoxInstallationComboBoxChange(this);
    while (PrgSetup->DOSBoxSettingsCount() > DOSBoxData.count()) PrgSetup->DeleteDOSBoxSettings(PrgSetup->DOSBoxSettingsCount() - 1);
    while (PrgSetup->DOSBoxSettingsCount() < DOSBoxData.count()) PrgSetup->AddDOSBoxSettings("");
    DOSBoxData[0].DosBoxLanguage = *PDOSBoxLang;
    for (I = 0; I < DOSBoxData.count(); I++) DOSBoxDataToDOSBoxSetting(DOSBoxData[I], PrgSetup->DOSBoxSettings(I));
    for (I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) PrgSetup->DOSBoxSettings(I)->SetNr(I);
}

void TSetupFrameDOSBox::DOSBoxInstallationComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastIndex >= 0) {
        DOSBoxData[LastIndex].DosBoxDir = IncludeTrailingPathDelimiter(ui.DosBoxDirEdit->text());
    }

    LastIndex = ui.DOSBoxInstallationComboBox->currentIndex();

    {
        const QSignalBlocker Blocker(ui.DosBoxDirEdit);   // OnChange is called explicitly below
        if (LastIndex >= 0) {
            ui.DosBoxDirEdit->setText(DOSBoxData[LastIndex].DosBoxDir);
        } else {
            ui.DosBoxDirEdit->setText("");
        }
    }
    DosBoxDirEditChange(this);

    ui.DOSBoxEditButton->setEnabled(LastIndex >= 1);
    ui.DOSBoxDeleteButton->setEnabled(LastIndex >= 1);
    ui.DOSBoxUpButton->setEnabled(LastIndex >= 2);
    ui.DOSBoxDownButton->setEnabled((LastIndex >= 1) && (LastIndex < DOSBoxData.count() - 1));
}

void TSetupFrameDOSBox::DosBoxDirEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if ((*PDosBoxDir != ui.DosBoxDirEdit->text()) && (ui.DOSBoxInstallationComboBox->currentIndex() == 0)) {
        *PDosBoxDir = ui.DosBoxDirEdit->text();
        DosBoxDirChange();
    }

    ui.WarningButton->setVisible(OldDOSBoxVersion(CheckDOSBoxVersion(-1, ui.DosBoxDirEdit->text())));
    QRect G = ui.DosBoxDirEdit->geometry();
    QRect WB = ui.WarningButton->geometry();
    int W = (ui.WarningButton->isVisibleTo(this) ? WB.x() - 4 : WB.x() + WB.width()) - G.x();
    VclLayout::setBounds(ui.DosBoxDirEdit, G.x(), G.y(), W, G.height());
}

void TSetupFrameDOSBox::ButtonWork(QObject* Sender)
{
    QString S;
    int I, J;
    TDOSBoxData D;
    QComboBox *Combo = ui.DOSBoxInstallationComboBox;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (Combo->currentIndex() >= 1) {
                S = DOSBoxData[Combo->currentIndex()].Name;
                if (!InputQuery(LanguageSetup->SetupFormDOSBoxInstallationEditCaption(), LanguageSetup->SetupFormDOSBoxInstallationEditInfo(), S, this)) return;
                I = Combo->currentIndex();
                DOSBoxData[Combo->currentIndex()].Name = S;
                Combo->setItemText(Combo->currentIndex(), S);
                Combo->setCurrentIndex(I);
            }
            break;
        case 1:
            S = "";
            if (!InputQuery(LanguageSetup->SetupFormDOSBoxInstallationAddCaption(), LanguageSetup->SetupFormDOSBoxInstallationEditInfo(), S, this)) return;
            I = DOSBoxData.count();
            DOSBoxData.append(TDOSBoxData());
            InitDOSBoxData(DOSBoxData[I]);
            DOSBoxData[I].Name = S;
            Combo->addItem(S);
            Combo->setCurrentIndex(I);
            DOSBoxInstallationComboBoxChange(this);
            break;
        case 2:
            if (Combo->currentIndex() >= 1) {
                I = Combo->currentIndex();
                if (MessageDlg(Format(LanguageSetup->SetupFormDOSBoxInstallationDeleteConfirm(), {DOSBoxData[I].Name}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
                Combo->setCurrentIndex(I - 1);
                DOSBoxInstallationComboBoxChange(Sender);
                Combo->removeItem(I);
                for (J = I + 1; J < DOSBoxData.count(); J++) DOSBoxData[J - 1] = DOSBoxData[J];
                DOSBoxData.removeLast();
                DOSBoxInstallationComboBoxChange(Sender);
            }
            break;
        case 3:
            if (Combo->currentIndex() >= 2) {
                I = Combo->currentIndex();
                ComboExchange(Combo, I, I - 1);
                D = DOSBoxData[I]; DOSBoxData[I] = DOSBoxData[I - 1]; DOSBoxData[I - 1] = D;
                LastIndex = I - 1;
                Combo->setCurrentIndex(I - 1);
                DOSBoxInstallationComboBoxChange(Sender);
            }
            break;
        case 4:
            if ((Combo->currentIndex() >= 0) && (Combo->currentIndex() < DOSBoxData.count() - 1)) {
                I = Combo->currentIndex();
                ComboExchange(Combo, I, I + 1);
                D = DOSBoxData[I]; DOSBoxData[I] = DOSBoxData[I + 1]; DOSBoxData[I + 1] = D;
                LastIndex = I + 1;
                Combo->setCurrentIndex(I + 1);
                DOSBoxInstallationComboBoxChange(Sender);
            }
            break;
        case 5:
            if (Combo->currentIndex() >= 0) {
                S = ui.DosBoxDirEdit->text();
                if (S == "") S = GetSpecialFolder(0, CSIDL_PROGRAM_FILES_);
                if (SelectDirectory(THandle(0), LanguageSetup->SetupFormDOSBoxDir(), S)) {
                    ui.DosBoxDirEdit->setText(S);
                    DosBoxDirEditChange(Sender);
                }
            }
            break;
        case 6:
            if (Combo->currentIndex() >= 0) {
                if (SearchDosBox(this, S)) {
                    ui.DosBoxDirEdit->setText(S);
                    DosBoxDirEditChange(Sender);
                }
            }
            break;
        case 7:
            if (Combo->currentIndex() >= 0) {
                if (Combo->currentIndex() == 0) DOSBoxData[0].DosBoxLanguage = *PDOSBoxLang;
                DOSBoxData[Combo->currentIndex()].DosBoxDir = IncludeTrailingPathDelimiter(ui.DosBoxDirEdit->text());
                ShowSetupFrameDOSBoxDialog(this, DOSBoxData[Combo->currentIndex()], Combo->currentIndex() == 0, GameDB);
                ui.DosBoxDirEdit->setText(DOSBoxData[Combo->currentIndex()].DosBoxDir);
                if (Combo->currentIndex() == 0) *PDOSBoxLang = DOSBoxData[0].DosBoxLanguage;
            }
            break;
    }
}

void TSetupFrameDOSBox::DOSBoxDownloadURLClick(QObject* Sender)
{
    QLabel *L = qobject_cast<QLabel*>(Sender);
    if (L != nullptr) ShellOpen(L->text());
}

void TSetupFrameDOSBox::WarningButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    DOSBoxOutdatedWarning(ui.DosBoxDirEdit->text());
}
