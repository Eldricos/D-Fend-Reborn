// C++ port of SetupFrameDirectoriesUnit.pas (implementation section).
#include "SetupFrameDirectoriesUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "HelpConsts.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "Dialogs.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TSetupFrameDirectories::TSetupFrameDirectories(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.DataDirButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DataDirButton); });
    connect(ui.GameDirButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.GameDirButton); });
    connect(ui.BaseDirButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.BaseDirButton); });
    connect(ui.CaptureDirButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.CaptureDirButton); });
    connect(ui.BaseDirEdit, &QLineEdit::textChanged, this, [this]() { BaseDirEditChange(ui.BaseDirEdit); });
}

TSetupFrameDirectories::~TSetupFrameDirectories() = default;

// { TSetupFrameDirectories }

QString TSetupFrameDirectories::GetName()
{
    return LanguageSetup->SetupFormDirectoriesSheet();
}

void TSetupFrameDirectories::InitGUIAndLoadSetup(TInitData &InitData)
{
    PBaseDir = InitData.PBaseDir;
    ui.BaseDirEdit->setText(PrgSetup->BaseDir());
    ui.GameDirEdit->setText(PrgSetup->GameDir());
    ui.DataDirEdit->setText(PrgSetup->DataDir());
    ui.CaptureDirEdit->setText(MakeAbsPath(PrgSetup->CaptureDir(), PrgSetup->BaseDir()));
    UserIconLoader->DialogImage(DI_SelectFolder, ui.BaseDirButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.GameDirButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DataDirButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.CaptureDirButton);
    setProperty("HelpContext", ID_FileOptionsGeneral);
}

void TSetupFrameDirectories::BeforeChangeLanguage()
{
}

void TSetupFrameDirectories::LoadLanguage()
{
    ui.BaseDirEdit_EditLabel->setText(LanguageSetup->SetupFormBaseDir());
    ui.BaseDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.GameDirEdit_EditLabel->setText(LanguageSetup->SetupFormGameDir());
    ui.GameDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.DataDirEdit_EditLabel->setText(LanguageSetup->SetupFormDataDir());
    ui.DataDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.CaptureDirEdit_EditLabel->setText(LanguageSetup->SetupFormCaptureDir());
    ui.CaptureDirButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.InfoLabel->setText(LanguageSetup->SetupFormDirectoriesInfo());
}

void TSetupFrameDirectories::DOSBoxDirChanged()
{
}

void TSetupFrameDirectories::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
}

void TSetupFrameDirectories::HideFrame()
{
}

void TSetupFrameDirectories::RestoreDefaults()
{
    ui.BaseDirEdit->setText(PrgDataDir());
    ui.GameDirEdit->setText(PrgDataDir() + "VirtualHD" + PathDelim);
    ui.DataDirEdit->setText(PrgDataDir() + "GameData" + PathDelim);
    ui.DataDirEdit->setText(PrgDataDir() + "Capture" + PathDelim); // sic (original sets DataDirEdit twice)
}

void TSetupFrameDirectories::SaveSetup()
{
    PrgSetup->SetBaseDir(ui.BaseDirEdit->text());
    PrgSetup->SetGameDir(ui.GameDirEdit->text());
    PrgSetup->SetDataDir(ui.DataDirEdit->text());
    PrgSetup->SetCaptureDir(MakeRelPath(ui.CaptureDirEdit->text(), PrgSetup->BaseDir()));
}

void TSetupFrameDirectories::BaseDirEditChange(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (PBaseDir != nullptr) *PBaseDir = ui.BaseDirEdit->text();
}

void TSetupFrameDirectories::ButtonWork(QObject *Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 0:
            S = ui.BaseDirEdit->text(); if (S.isEmpty()) S = PrgDataDir();
            if (SelectDirectory(LanguageSetup->SetupFormBaseDir(), QString(), S, this)) ui.BaseDirEdit->setText(IncludeTrailingPathDelimiter(S));
            break;
        case 1:
            S = ui.GameDirEdit->text(); if (S.isEmpty()) S = ui.BaseDirEdit->text();
            if (SelectDirectory(LanguageSetup->SetupFormGameDir(), QString(), S, this)) ui.GameDirEdit->setText(IncludeTrailingPathDelimiter(S));
            break;
        case 2:
            S = ui.DataDirEdit->text(); if (S.isEmpty()) S = ui.BaseDirEdit->text();
            if (SelectDirectory(LanguageSetup->SetupFormDataDir(), QString(), S, this)) ui.DataDirEdit->setText(IncludeTrailingPathDelimiter(S));
            break;
        case 3:
            S = ui.CaptureDirEdit->text(); if (S.isEmpty()) S = ui.BaseDirEdit->text();
            if (SelectDirectory(LanguageSetup->SetupFormCaptureDirSelect(), QString(), S, this)) ui.CaptureDirEdit->setText(IncludeTrailingPathDelimiter(S));
            break;
    }
}
