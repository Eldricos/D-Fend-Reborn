// C++ port of RenameAllScreenshotsFormUnit.pas (implementation section).
#include "RenameAllScreenshotsFormUnit.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "PrgSetupUnit.h"

TRenameAllScreenshotsForm* RenameAllScreenshotsForm = nullptr;

TRenameAllScreenshotsForm::TRenameAllScreenshotsForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
}

TRenameAllScreenshotsForm::~TRenameAllScreenshotsForm()
{
}

void TRenameAllScreenshotsForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FShown) { FShown = true; FormShow(this); }
}

void TRenameAllScreenshotsForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    ui.InfoLabel->setText(LanguageSetup->RenameAllScheme());
    ui.Edit->setText(PrgSetup->RenameAllExpression());
    ui.ThisProfileRadioButton->setText(LanguageSetup->RenameAllThisProfile());
    ui.AllProfilesRadioButton->setText(LanguageSetup->RenameAllAllProfiles());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

void TRenameAllScreenshotsForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    PrgSetup->SetRenameAllExpression(ui.Edit->text());
}

// global

static bool ShowRenameAllScreenshotsDialog(QWidget* AOwner, const QString& Caption, QString& FileMask, bool& AllProfiles, bool OnlyAllProfiles)
{
    bool result;
    RenameAllScreenshotsForm = new TRenameAllScreenshotsForm(AOwner);
    RenameAllScreenshotsForm->setWindowTitle(Caption);

    if (OnlyAllProfiles) {
        RenameAllScreenshotsForm->ui.AllProfilesRadioButton->setChecked(true);
        RenameAllScreenshotsForm->ui.ThisProfileRadioButton->setEnabled(false);
        RenameAllScreenshotsForm->ui.AllProfilesRadioButton->setEnabled(false);
    }

    result = (RenameAllScreenshotsForm->ShowModal() == mrOk);
    if (result) {
        FileMask = RenameAllScreenshotsForm->ui.Edit->text();
        AllProfiles = RenameAllScreenshotsForm->ui.AllProfilesRadioButton->isChecked();
    }
    delete RenameAllScreenshotsForm;
    RenameAllScreenshotsForm = nullptr;
    return result;
}

bool RenameMediaFiles(QWidget* AOwner, TGame* Game, TGameDB* GameDB, TRenameFiles RenameFiles)
{
    QString Folder, Caption, FileMask;
    bool AllProfiles = false;
    bool result = false;

    switch (RenameFiles) {
        case rfScreenshots: Caption = LanguageSetup->RenameAllScreenshots(); break;
        case rfSounds: Caption = LanguageSetup->RenameAllSounds(); break;
        case rfVideos: Caption = LanguageSetup->RenameAllVideos(); break;
        default: Caption = LanguageSetup->RenameAllScreenshots(); break;
    }

    if (!ShowRenameAllScreenshotsDialog(AOwner, Caption, FileMask, AllProfiles, Game == nullptr)) return result;

    if (AllProfiles) {
        result = (GameDB->Count() > 0);
        for (int I = 0; I < GameDB->Count(); I++) {
            Folder = Trim(GameDB->Game(I)->CaptureFolder()); if (Folder != "") Folder = MakeAbsPath(Folder, PrgSetup->BaseDir()); if (Folder == "") return result;
            RenameAllFiles(Folder, FileMask, GameDB->Game(I)->CacheName, RenameFiles, false);
        }
    } else {
        Folder = Trim(Game->CaptureFolder()); if (Folder != "") Folder = MakeAbsPath(Folder, PrgSetup->BaseDir()); if (Folder == "") return result;
        result = RenameAllFiles(Folder, FileMask, Game->CacheName, RenameFiles, true);
    }
    return result;
}
