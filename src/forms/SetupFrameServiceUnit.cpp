// C++ port of SetupFrameServiceUnit.pas (implementation section).
#include "SetupFrameServiceUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "GameDBToolsUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "PackageDBToolsUnit.h"
#include "CommonTools.h"
#include "RenameAllScreenshotsFormUnit.h"
#include "ResetProfilesFormUnit.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameService::TSetupFrameService(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageList = TImageList::FromResource("SetupFrameServiceUnit", "ImageList");
    // DFM event wiring
    connect(ui.Service3Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service3Button); });
    connect(ui.Service4Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service4Button); });
    connect(ui.Service1Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service1Button); });
    connect(ui.Service2Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service2Button); });
    connect(ui.Service5Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service5Button); });
    connect(ui.Service6Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service6Button); });
    connect(ui.Service7Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service7Button); });
    connect(ui.Service8Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service8Button); });
    connect(ui.Service9Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service9Button); });
    connect(ui.Service10Button, &QPushButton::clicked, this, [this]() { ButtonWork(ui.Service10Button); });
    connect(ui.RenameScreenshots, &QAction::triggered, this, [this]() { ButtonWork(ui.RenameScreenshots); });
    connect(ui.RenameSounds, &QAction::triggered, this, [this]() { ButtonWork(ui.RenameSounds); });
    connect(ui.RenameVideos, &QAction::triggered, this, [this]() { ButtonWork(ui.RenameVideos); });
}

TSetupFrameService::~TSetupFrameService()
{
    delete ImageList;
}

// { TSetupFrameService }

QString TSetupFrameService::GetName()
{
    return LanguageSetup->SetupFormServiceSheet();
}

void TSetupFrameService::InitGUIAndLoadSetup(TInitData &InitData)
{
    GameDB = InitData.GameDB;
    SearchLinkFile = InitData.SearchLinkFile;
    UserIconLoader->DialogImage(DI_ResetProfile, ui.Service3Button);
    UserIconLoader->DialogImage(DI_ResetProfile, ui.Service4Button);
    UserIconLoader->DialogImage(DI_Clear, ui.Service1Button);
    UserIconLoader->DialogImage(DI_Folders, ui.Service2Button);
    UserIconLoader->DialogImage(DI_Calculator, ui.Service5Button);
    UserIconLoader->DialogImage(DI_Delete, ui.Service6Button);
    UserIconLoader->DialogImage(DI_Folders, ui.Service7Button);
    UserIconLoader->DialogImage(DI_Folders, ui.Service8Button);
    UserIconLoader->DialogImage(DI_Edit, ui.Service9Button);
    UserIconLoader->DialogImage(DI_Edit, ui.Service10Button);
    UserIconLoader->DialogImage(DI_Image, ImageList, 0);
    UserIconLoader->DialogImage(DI_Sound, ImageList, 1);
    UserIconLoader->DialogImage(DI_Video, ImageList, 2);
    // RenamePopupMenu.Images = ImageList (ImageIndex 0..2)
    if (ImageList->Count() > 0) ui.RenameScreenshots->setIcon(ImageList->Icon(0));
    if (ImageList->Count() > 1) ui.RenameSounds->setIcon(ImageList->Icon(1));
    if (ImageList->Count() > 2) ui.RenameVideos->setIcon(ImageList->Icon(2));
}

void TSetupFrameService::BeforeChangeLanguage()
{
}

void TSetupFrameService::LoadLanguage()
{
    ui.Service1Button->setText(LanguageSetup->SetupFormService1());
    ui.Service2Button->setText(LanguageSetup->SetupFormService2());
    ui.Service3Button->setText(LanguageSetup->SetupFormService3());
    ui.Service4Button->setText(LanguageSetup->SetupFormService4());
    ui.Service5Button->setText(LanguageSetup->SetupFormService5());
    ui.Service6Button->setText(LanguageSetup->SetupFormService6());
    ui.Service7Button->setText(LanguageSetup->SetupFormService7());
    ui.Service8Button->setText(LanguageSetup->SetupFormService8());
    ui.Service9Button->setText(LanguageSetup->SetupFormService9());
    ui.Service10Button->setText(LanguageSetup->SetupFormService10());
    ui.RenameScreenshots->setText(LanguageSetup->CaptureScreenshots());
    ui.RenameSounds->setText(LanguageSetup->CaptureSounds());
    ui.RenameVideos->setText(LanguageSetup->CaptureVideos());
    setProperty("HelpContext", ID_FileOptionsService);
}

void TSetupFrameService::DOSBoxDirChanged()
{
}

void TSetupFrameService::ShowFrame(bool AdvancedMode)
{
    ui.Service1Button->setVisible(AdvancedMode);
    ui.Service2Button->setVisible(AdvancedMode);
    ui.Service5Button->setVisible(AdvancedMode);
    ui.Service6Button->setVisible(AdvancedMode);
    ui.Service7Button->setVisible(AdvancedMode);
    ui.Service8Button->setVisible(AdvancedMode);
    ui.Service9Button->setVisible(AdvancedMode);
    ui.Service10Button->setVisible(AdvancedMode);
}

void TSetupFrameService::HideFrame()
{
}

void TSetupFrameService::RestoreDefaults()
{
}

void TSetupFrameService::SaveSetup()
{
}

void TSetupFrameService::ButtonWork(QObject *Sender)
{
    int I;
    QPoint P;
    switch (Sender->property("tag").toInt()) {
        case 0: DeleteOldFiles(); break;
        case 1: ReplaceAbsoluteDirs(GameDB); break;
        case 2: {
            setEnabled(false);
            I = GameDB->IndexOf(DosBoxDOSProfile);
            if (I >= 0) {
                if (MessageDlg(LanguageSetup->SetupFormService3Confirmation(), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) { setEnabled(true); return; }
                GameDB->Delete(I);
            }
            BuildDefaultDosProfile(GameDB);
            setEnabled(true);
            break;
        }
        case 3:
            BuildDefaultProfile();
            ReBuildTemplates(false);
            break;
        case 4: CreateCheckSumsForAllGames(GameDB); break;
        case 5: ClearPackageCache(); break;
        case 6: CreateFoldersForAllGames(GameDB, ftCapture); break;
        case 7: CreateFoldersForAllGames(GameDB, ftGameData); break;
        case 8:
            P = mapToGlobal(QPoint(ui.Service9Button->x(), ui.Service9Button->y()));
            ui.RenamePopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        case 9: ShowResetProfilesDialog(this, GameDB, SearchLinkFile); break;
        case 81: RenameMediaFiles(this, nullptr, GameDB, rfScreenshots); break;
        case 82: RenameMediaFiles(this, nullptr, GameDB, rfSounds); break;
        case 83: RenameMediaFiles(this, nullptr, GameDB, rfVideos); break;
    }
}
