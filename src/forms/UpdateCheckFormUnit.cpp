// C++ port of UpdateCheckFormUnit.pas (implementation section).
#include "UpdateCheckFormUnit.h"
#include "ColorScheme.h"

#include <QGuiApplication>

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "ProgramUpdateCheckUnit.h"
#include "CheatDBToolsUnit.h"
#include "SetupFormUnit.h"
#include "PackageDBToolsUnit.h"
#include "PrgConsts.h"
#include "Process.h"

TUpdateCheckForm *UpdateCheckForm = nullptr;

static constexpr TColor clDarkYellow = TColor(0x00E0E0);

TUpdateCheckForm::TUpdateCheckForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ui.DFRHomepageLabel->installEventFilter(this);   // DFRHomepageLabel.OnClick
    connect(ui.UpdateButton, &QPushButton::clicked, this, [this]() { UpdateButtonClick(ui.UpdateButton); });
    FormCreate(this);   // OnCreate
}

TUpdateCheckForm::~TUpdateCheckForm() = default;

bool TUpdateCheckForm::eventFilter(QObject *O, QEvent *E)
{
    if (O == ui.DFRHomepageLabel && E->type() == QEvent::MouseButtonRelease) {
        auto *ME = static_cast<QMouseEvent *>(E);
        if (ME->button() == Qt::LeftButton) { DFRHomepageLabelClick(ui.DFRHomepageLabel); return true; }
    }
    return TVclForm::eventFilter(O, E);
}

void TUpdateCheckForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->UpdateCheckDialog());
    ui.CheckBoxProgram->setText(LanguageSetup->UpdateCheckDialogProgram());
    ui.InfoLabelProgram->setText(LanguageSetup->UpdateCheckDialogProgramInfo());
    ui.CheckBoxPackages->setText(LanguageSetup->UpdateCheckDialogPackages());
    ui.InfoLabelPackages->setText(LanguageSetup->UpdateCheckDialogPackagesInfo());
    ui.CheckBoxCheats->setText(LanguageSetup->UpdateCheckDialogCheats());
    ui.InfoLabelCheats->setText(LanguageSetup->UpdateCheckDialogCheatsInfo());
    ui.LabelDownload->setText(LanguageSetup->UpdateCheckDialogDownload());
    ui.UpdateButton->setText(LanguageSetup->UpdateCheckDialogUpdate());
    ui.CloseButton->setText(LanguageSetup->Close());
    ui.SetupButton->setText(LanguageSetup->UpdateCheckDialogAutoUpdate());

    UserIconLoader->DialogImage(DI_Update, ui.UpdateButton);
    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);
    UserIconLoader->DirectLoad("Main", 6, ui.SetupButton);

    for (QCheckBox *C : {ui.CheckBoxProgram, ui.CheckBoxPackages, ui.CheckBoxCheats}) {
        QFont F = C->font(); F.setBold(true); C->setFont(F);
    }

    SetStatus(ui.StatusLabelProgram, LanguageSetup->UpdateCheckDialogStatusNotYetChecked(), clGrayText);
    SetStatus(ui.StatusLabelPackages, LanguageSetup->UpdateCheckDialogStatusNotYetChecked(), clGrayText);
    SetStatus(ui.StatusLabelCheats, LanguageSetup->UpdateCheckDialogStatusNotYetChecked(), clGrayText);

    ui.DFRHomepageLabel->setText(LanguageSetup->MenuHelpUpdatesURL());
    VclLayout::makeLinkLabel(ui.DFRHomepageLabel);

    // (the layout makes the window wide enough for the download text; the
    // height is what the controls need)
    VclLayout::fitHeight(this);
}

void TUpdateCheckForm::SetStatus(QLabel *ALabel, const QString &AText, TColor AColor)
{
    ALabel->setText(AText);
    VclLayout::fitHeight(this);   // (longer status texts: the window gets wider if needed)
    QPalette P = ALabel->palette();
    P.setColor(QPalette::WindowText, TextColor(AColor));
    ALabel->setPalette(P);
    UpdateWindow();
}

void TUpdateCheckForm::UpdateWindow()
{
    update();
    repaint();
    ProcessMessages();
}

void TUpdateCheckForm::DFRHomepageLabelClick(QObject *Sender)
{
    auto *L = qobject_cast<QLabel *>(Sender);
    if (L != nullptr) ShellOpen(L->text());
}

void TUpdateCheckForm::UpdateButtonClick(QObject * /*Sender*/)
{
    setEnabled(false);
    ui.UpdateButton->setEnabled(false);
    ui.CloseButton->setEnabled(false);
    ui.SetupButton->setEnabled(false);
    try {
        if (ui.CheckBoxCheats->isChecked()) UpdateCheatsDB();
        if (ui.CheckBoxPackages->isChecked()) UpdatePackagesDB();
        if (ui.CheckBoxProgram->isChecked()) UpdateProgram();
    } catch (...) {
        setEnabled(true);
        ui.UpdateButton->setEnabled(true);
        ui.CloseButton->setEnabled(true);
        ui.SetupButton->setEnabled(true);
        throw;
    }
    setEnabled(true);
    ui.UpdateButton->setEnabled(true);
    ui.CloseButton->setEnabled(true);
    ui.SetupButton->setEnabled(true);
}

void TUpdateCheckForm::UpdateProgram()
{
    SetStatus(ui.StatusLabelProgram, LanguageSetup->UpdateCheckDialogStatusSearching(), clDarkYellow);
    switch (RunUpdateCheck(this, true, false)) {
        case urNoUpdatesAvailable: SetStatus(ui.StatusLabelProgram, LanguageSetup->UpdateCheckDialogStatusNoUpdates(), clGreen); break;
        case urUpdateAvailable: SetStatus(ui.StatusLabelProgram, LanguageSetup->UpdateCheckDialogStatusProgramNewVersionAvailable(), clDarkYellow); break;
        case urUpdateInstallCanceled: SetStatus(ui.StatusLabelProgram, LanguageSetup->UpdateCheckDialogStatusAborted(), clRed); break;
        default: break;
    }
    raise(); activateWindow();   // BringWindowToTop(Handle)
}

void TUpdateCheckForm::UpdatePackagesDB()
{
    SetStatus(ui.StatusLabelPackages, LanguageSetup->UpdateCheckDialogStatusSearching(), clDarkYellow);
    // GetKeyState(VK_LSHIFT/VK_RSHIFT): shift key pressed
    if (UpdatePackageDB(this, QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier), false)) {
        SetStatus(ui.StatusLabelPackages, LanguageSetup->UpdateCheckDialogStatusPackagesDone(), clGreen);
    } else {
        SetStatus(ui.StatusLabelPackages, LanguageSetup->UpdateCheckDialogStatusAborted(), clRed);
    }

    raise(); activateWindow();   // BringWindowToTop(Handle)
}

void TUpdateCheckForm::UpdateCheatsDB()
{
    SetStatus(ui.StatusLabelCheats, LanguageSetup->UpdateCheckDialogStatusSearching(), clDarkYellow);
    switch (CheatsDBUpdateCheck(this, false)) {
        case urNoUpdatesAvailable: SetStatus(ui.StatusLabelCheats, LanguageSetup->UpdateCheckDialogStatusNoUpdates(), clGreen); break;
        case urUpdateInstallCanceled: SetStatus(ui.StatusLabelCheats, LanguageSetup->UpdateCheckDialogStatusAborted(), clRed); break;
        case urUpdateInstalled: SetStatus(ui.StatusLabelCheats, LanguageSetup->UpdateCheckDialogStatusCheatsDone(), clGreen); break;
        default: break;
    }
    raise(); activateWindow();   // BringWindowToTop(Handle)
}

void ShowUpdateCheckDialog(QWidget *AOwner, TGameDB *AGameDB, TLinkFile *ASearchLinkFile, bool ANoConfigButton)
{
    bool OpenSetup;
    UpdateCheckForm = new TUpdateCheckForm(AOwner);
    UpdateCheckForm->GameDB = AGameDB;
    if (ANoConfigButton) UpdateCheckForm->ui.SetupButton->setVisible(false);
    OpenSetup = (UpdateCheckForm->ShowModal() == mrOk);
    delete UpdateCheckForm;
    UpdateCheckForm = nullptr;

    if (OpenSetup) ShowUpdateSetupDialog(AOwner, AGameDB, ASearchLinkFile);
}

void RunProgramStartSilentUpdateCheck(QWidget *AForm, bool ForceCheck)
{
    if (ForceCheck) {
        RunUpdateCheck(AForm, true, true);
    } else {
        RunUpdateCheckIfSetup(AForm);
    }
}
