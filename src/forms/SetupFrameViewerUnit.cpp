// C++ port of SetupFrameViewerUnit.pas (implementation section).
#include "SetupFrameViewerUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameViewer::TSetupFrameViewer(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    PrgOpenDialog = new TOpenDialog(this);
    PrgOpenDialog->DefaultExt = "exe";
    // DFM event wiring
    connect(ui.VideosButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.VideosButton); });
    connect(ui.VideosEdit, &QLineEdit::textChanged, this, [this]() { VideosEditChange(ui.VideosEdit); });
    connect(ui.SoundsButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.SoundsButton); });
    connect(ui.SoundsEdit, &QLineEdit::textChanged, this, [this]() { SoundsEditChange(ui.SoundsEdit); });
    connect(ui.ImagesButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ImagesButton); });
    connect(ui.ImagesEdit, &QLineEdit::textChanged, this, [this]() { ImagesEditChange(ui.ImagesEdit); });
}

TSetupFrameViewer::~TSetupFrameViewer() = default;

// { TSetupFrameViewer }

QString TSetupFrameViewer::GetName()
{
    return LanguageSetup->SetupFormTextMediaViewer();
}

void TSetupFrameViewer::InitGUIAndLoadSetup(TInitData &InitData)
{
    Q_UNUSED(InitData);
    QString S;
    S = Trim(ExtUpperCase(PrgSetup->ImageViewer()));
    if ((S == "INTERNAL") || (S.isEmpty())) ui.ImagesRadioButton1->setChecked(true); else {
        if (S == "DEFAULT") ui.ImagesRadioButton2->setChecked(true); else {
            ui.ImagesRadioButton3->setChecked(true);
            ui.ImagesEdit->setText(MakeRelPath(PrgSetup->ImageViewer(), PrgSetup->BaseDir()));
        }
    }
    ui.ModalImageViewerCheckBox->setChecked(!PrgSetup->NonModalViewer());
    S = Trim(ExtUpperCase(PrgSetup->SoundPlayer()));
    if ((S == "INTERNAL") || (S.isEmpty())) ui.SoundsRadioButton1->setChecked(true); else {
        if (S == "DEFAULT") ui.SoundsRadioButton2->setChecked(true); else {
            ui.SoundsRadioButton3->setChecked(true);
            ui.SoundsEdit->setText(MakeRelPath(PrgSetup->SoundPlayer(), PrgSetup->BaseDir()));
        }
    }
    S = Trim(ExtUpperCase(PrgSetup->VideoPlayer()));
    if ((S == "INTERNAL") || (S.isEmpty())) ui.VideosRadioButton1->setChecked(true); else {
        if (S == "DEFAULT") ui.VideosRadioButton2->setChecked(true); else {
            ui.VideosRadioButton3->setChecked(true);
            ui.VideosEdit->setText(MakeRelPath(PrgSetup->VideoPlayer(), PrgSetup->BaseDir()));
        }
    }
    UserIconLoader->DialogImage(DI_SelectFile, ui.ImagesButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SoundsButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.VideosButton);
}

void TSetupFrameViewer::BeforeChangeLanguage()
{
}

void TSetupFrameViewer::LoadLanguage()
{
    ui.ImagesGroupBox->setTitle(LanguageSetup->SetupFormTextMediaViewerImages());
    ui.SoundsGroupBox->setTitle(LanguageSetup->SetupFormTextMediaViewerSounds());
    ui.VideosGroupBox->setTitle(LanguageSetup->SetupFormTextMediaViewerVideos());
    ui.ImagesRadioButton1->setText(LanguageSetup->SetupFormTextMediaViewerInterval());
    ui.ImagesRadioButton2->setText(LanguageSetup->SetupFormTextMediaViewerDefault());
    ui.ImagesRadioButton3->setText(LanguageSetup->SetupFormTextMediaViewerCustom());
    ui.ImagesButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ModalImageViewerCheckBox->setText(LanguageSetup->SetupFormTextMediaViewerModal());
    ui.SoundsRadioButton1->setText(LanguageSetup->SetupFormTextMediaViewerInterval());
    ui.SoundsRadioButton2->setText(LanguageSetup->SetupFormTextMediaViewerDefault());
    ui.SoundsRadioButton3->setText(LanguageSetup->SetupFormTextMediaViewerCustom());
    ui.SoundsButton->setToolTip(LanguageSetup->ChooseFile());
    ui.VideosRadioButton1->setText(LanguageSetup->SetupFormTextMediaViewerInterval());
    ui.VideosRadioButton2->setText(LanguageSetup->SetupFormTextMediaViewerDefault());
    ui.VideosRadioButton3->setText(LanguageSetup->SetupFormTextMediaViewerCustom());
    ui.VideosButton->setToolTip(LanguageSetup->ChooseFile());
    PrgOpenDialog->Title = LanguageSetup->SetupFormTextMediaViewerCustomOpenCaption();
    PrgOpenDialog->Filter = LanguageSetup->SetupFormExeFilter();
    setProperty("HelpContext", ID_FileOptionsMediaViewers);
}

void TSetupFrameViewer::DOSBoxDirChanged()
{
}

void TSetupFrameViewer::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameViewer::HideFrame()
{
}

void TSetupFrameViewer::RestoreDefaults()
{
    ui.ImagesEdit->setText(QString());
    ui.ImagesRadioButton1->setChecked(true);
    ui.ModalImageViewerCheckBox->setChecked(true);
    ui.SoundsEdit->setText(QString());
    ui.SoundsRadioButton1->setChecked(true);
    ui.VideosEdit->setText(QString());
    ui.VideosRadioButton1->setChecked(true);
}

void TSetupFrameViewer::SaveSetup()
{
    if (ui.ImagesRadioButton1->isChecked()) PrgSetup->SetImageViewer("Internal");
    if (ui.ImagesRadioButton2->isChecked()) PrgSetup->SetImageViewer("Default");
    if (ui.ImagesRadioButton3->isChecked()) PrgSetup->SetImageViewer(MakeRelPath(ui.ImagesEdit->text(), PrgSetup->BaseDir()));
    PrgSetup->SetNonModalViewer(!ui.ModalImageViewerCheckBox->isChecked());
    if (ui.SoundsRadioButton1->isChecked()) PrgSetup->SetSoundPlayer("Internal");
    if (ui.SoundsRadioButton2->isChecked()) PrgSetup->SetSoundPlayer("Default");
    if (ui.SoundsRadioButton3->isChecked()) PrgSetup->SetSoundPlayer(MakeRelPath(ui.SoundsEdit->text(), PrgSetup->BaseDir()));
    if (ui.VideosRadioButton1->isChecked()) PrgSetup->SetVideoPlayer("Internal");
    if (ui.VideosRadioButton2->isChecked()) PrgSetup->SetVideoPlayer("Default");
    if (ui.VideosRadioButton3->isChecked()) PrgSetup->SetVideoPlayer(MakeRelPath(ui.VideosEdit->text(), PrgSetup->BaseDir()));
}

void TSetupFrameViewer::ImagesEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.ImagesRadioButton3->setChecked(true);
}

void TSetupFrameViewer::SoundsEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.SoundsRadioButton3->setChecked(true);
}

void TSetupFrameViewer::VideosEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    ui.VideosRadioButton3->setChecked(true);
}

void TSetupFrameViewer::ButtonWork(QObject *Sender)
{
    QString S;
    const int Tag = Sender->property("tag").toInt();
    switch (Tag) {
        case 0: S = ui.ImagesEdit->text(); break;
        case 1: S = ui.SoundsEdit->text(); break;
        case 2: S = ui.VideosEdit->text(); break;
    }
    S = Trim(S); if (!S.isEmpty()) S = ExtractFilePath(S);
    if (S.isEmpty()) S = GetSpecialFolder(window()->winId(), CSIDL_PROGRAM_FILES);
    if (!S.isEmpty()) PrgOpenDialog->InitialDir = S;
    if (!PrgOpenDialog->Execute()) return;
    switch (Tag) {
        case 0: ui.ImagesRadioButton3->setChecked(true); ui.ImagesEdit->setText(PrgOpenDialog->FileName); break;
        case 1: ui.SoundsRadioButton3->setChecked(true); ui.SoundsEdit->setText(PrgOpenDialog->FileName); break;
        case 2: ui.VideosRadioButton3->setChecked(true); ui.VideosEdit->setText(PrgOpenDialog->FileName); break;
    }
}
