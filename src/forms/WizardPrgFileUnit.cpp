// C++ port of WizardPrgFileUnit.pas (implementation section).
#include "WizardPrgFileUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"

namespace {

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

// Edit.Color:=Color (read only looking edit)
void SetEditColorToParent(QLineEdit *E, QWidget *Parent)
{
    QPalette P = E->palette();
    P.setColor(QPalette::Base, Parent->palette().color(QPalette::Window));
    E->setPalette(P);
}

}  // namespace

// TWizardPrgFileFrame

TWizardPrgFileFrame::TWizardPrgFileFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.SetupButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.SetupButton); });
    connect(ui.ProgramButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ProgramButton); });
    connect(ui.GamesFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.GamesFolderButton); });
    connect(ui.BaseDataFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.BaseDataFolderButton); });
    connect(ui.DataFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DataFolderButton); });
    connect(ui.FolderInfoButton, &QToolButton::clicked, this, [this]() { FolderInfoButtonClick(ui.FolderInfoButton); });
    connect(ui.FolderInfoButton2, &QToolButton::clicked, this, [this]() { FolderInfoButtonClick(ui.FolderInfoButton2); });
    connect(ui.DataFolderShowButton, &QPushButton::clicked, this, [this]() { DataFolderShowButtonClick(ui.DataFolderShowButton); });
    connect(ui.DataFolderAutomaticButton, &QPushButton::clicked, this, [this]() { DataFolderAutomaticButtonClick(ui.DataFolderAutomaticButton); });
}

TWizardPrgFileFrame::~TWizardPrgFileFrame()
{
}

void TWizardPrgFileFrame::Init(TGameDB* GameDB)
{
    Q_UNUSED(GameDB);
    // SetVistaFonts(self); - not needed

    SetBold(ui.InfoLabel);
    SetBold(ui.BaseInfoLabel3);

    ui.InfoLabel->setText(LanguageSetup->WizardFormPage2Info());
    ui.ProgramEdit_EditLabel->setText(LanguageSetup->WizardFormProgram());
    ui.ProgramButton->setToolTip(LanguageSetup->ChooseFile());
    ui.SetupEdit_EditLabel->setText(LanguageSetup->WizardFormSetup());
    ui.SetupButton->setToolTip(LanguageSetup->ChooseFile());
    ui.GamesFolderEdit_EditLabel->setText(LanguageSetup->WizardFormGamesFolder());
    ui.GamesFolderButton->setText(LanguageSetup->WizardFormExplorer());
    ui.DataFolderShowButton->setText(LanguageSetup->WizardFormDataFolderButton());
    ui.DataFolderAutomaticCheckBox->setText(LanguageSetup->WizardFormDataFolderCheckbox());
    ui.BaseInfoLabel3->setText(LanguageSetup->WizardFormInfoLabel3());
    ui.DataFolderEdit_EditLabel->setText(LanguageSetup->WizardFormDataFolder());
    ui.DataFolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.BaseDataFolderEdit_EditLabel->setText(LanguageSetup->WizardFormBaseDataFolder());
    ui.BaseDataFolderButton->setText(LanguageSetup->WizardFormExplorer());
    ui.DataFolderAutomaticButton->setText(LanguageSetup->WizardFormDataFolderAutomaticButton());

    ui.GamesFolderEdit->setText(PrgSetup->GameDir());
    ui.BaseDataFolderEdit->setText(PrgSetup->DataDir());

    UserIconLoader->DialogImage(DI_SelectFile, ui.ProgramButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SetupButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.GamesFolderButton);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.FolderInfoButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DataFolderButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.BaseDataFolderButton);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.FolderInfoButton2);
    UserIconLoader->DialogImage(DI_Down, ui.DataFolderShowButton);
    UserIconLoader->DialogImage(DI_Up, ui.DataFolderAutomaticButton);

    SetEditColorToParent(ui.GamesFolderEdit, this);
    SetEditColorToParent(ui.BaseDataFolderEdit, this);
}

void TWizardPrgFileFrame::ButtonWork(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
    case 0: OpenFolder(ui.GamesFolderEdit->text()); break;   // ShellExecute(...,'explore',...)
    case 1:
        S = MakeAbsPath(ui.ProgramEdit->text(), PrgSetup->BaseDir());
        if (SelectProgramFile(S, ui.ProgramEdit->text(), ui.SetupEdit->text(), ui.GamesFolderEdit->isHidden(), OtherEmulator, this)) ui.ProgramEdit->setText(S);
        break;
    case 2:
        S = MakeAbsPath(ui.SetupEdit->text(), PrgSetup->BaseDir());
        if (SelectProgramFile(S, ui.SetupEdit->text(), ui.ProgramEdit->text(), ui.GamesFolderEdit->isHidden(), OtherEmulator, this)) ui.SetupEdit->setText(S);
        break;
    case 3: OpenFolder(ui.BaseDataFolderEdit->text()); break;   // ShellExecute(...,'explore',...)
    case 4:
        if (Trim(ui.DataFolderEdit->text()).isEmpty())
            S = PrgSetup->DataDir();
        else
            S = MakeAbsPath(ui.DataFolderEdit->text(), PrgSetup->BaseDir());
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        ui.DataFolderEdit->setText(S);
        break;
    }
}

void TWizardPrgFileFrame::WriteDataToGame(TGame* Game)
{
    int I;
    QString S, T;

    if (ui.GamesFolderEdit->isHidden() && (OtherEmulator >= 0)) {
        if (Trim(ui.ProgramEdit->text()).isEmpty()) { Game->SetGameExe(QString()); Game->SetGameParameters(QString()); } else {
            Game->SetGameExe(MakeRelPath((*PrgSetup->WindowsBasedEmulatorsPrograms())[OtherEmulator], PrgSetup->BaseDir()));
            Game->SetGameParameters(Format((*PrgSetup->WindowsBasedEmulatorsParameters())[OtherEmulator], {ui.ProgramEdit->text()}));
        }
        if (Trim(ui.SetupEdit->text()).isEmpty()) { Game->SetSetupExe(QString()); Game->SetSetupParameters(QString()); } else {
            Game->SetSetupExe(MakeRelPath((*PrgSetup->WindowsBasedEmulatorsPrograms())[OtherEmulator], PrgSetup->BaseDir()));
            Game->SetSetupParameters(Format((*PrgSetup->WindowsBasedEmulatorsParameters())[OtherEmulator], {ui.SetupEdit->text()}));
        }
    } else {
        Game->SetGameExe(ui.ProgramEdit->text());
        Game->SetSetupExe(ui.SetupEdit->text());
    }

    if ((!ui.DataFolderButton->isHidden()) && Trim(ui.DataFolderEdit->text()).isEmpty()) ui.DataFolderButton->setVisible(false);

    if (!ui.DataFolderButton->isHidden()) {
        Game->SetDataDir(MakeRelPath(ui.DataFolderEdit->text(), PrgSetup->BaseDir()));
    } else {
        if (ui.DataFolderAutomaticCheckBox->isChecked()) {
            if (PrgSetup->DataDir().isEmpty()) T = PrgSetup->BaseDir(); else T = PrgSetup->DataDir();
            S = IncludeTrailingPathDelimiter(T) + MakeFileSysOKFolderName(Game->Name()) + PathDelim;
            I = 0;
            while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
                I++;
                S = IncludeTrailingPathDelimiter(T) + MakeFileSysOKFolderName(Game->Name()) + IntToStr(I) + PathDelim;
            }
            Game->SetDataDir(MakeRelPath(S, PrgSetup->BaseDir()));
        } else {
            Game->SetDataDir(QString());
        }
    }
}

void TWizardPrgFileFrame::FolderInfoButtonClick(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
    case 0: MessageDlg(Format(LanguageSetup->WizardFormGamesFolderInfo(), {PrgSetup->BaseDir()}), mtInformation, mbOK, 0); break;
    case 1: MessageDlg(Format(LanguageSetup->WizardFormBaseDataFolderInfo(), {PrgSetup->BaseDir()}), mtInformation, mbOK, 0); break;
    }
}

void TWizardPrgFileFrame::DataFolderShowButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DataFolderShowButton->setVisible(false);
    ui.DataFolderAutomaticCheckBox->setVisible(false);

    ui.BaseInfoLabel3->setVisible(true);
    ui.BaseDataFolderEdit->setVisible(true);
    ui.BaseDataFolderEdit_EditLabel->setVisible(true);
    ui.BaseDataFolderButton->setVisible(true);
    ui.DataFolderEdit->setVisible(true);
    ui.DataFolderEdit_EditLabel->setVisible(true);
    ui.DataFolderButton->setVisible(true);
    ui.FolderInfoButton2->setVisible(true);
    ui.DataFolderAutomaticButton->setVisible(true);
}

void TWizardPrgFileFrame::DataFolderAutomaticButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DataFolderShowButton->setVisible(true);
    ui.DataFolderAutomaticCheckBox->setVisible(true);
    ui.DataFolderAutomaticCheckBox->setChecked(true);

    ui.BaseInfoLabel3->setVisible(false);
    ui.BaseDataFolderEdit->setVisible(false);
    ui.BaseDataFolderEdit_EditLabel->setVisible(false);
    ui.BaseDataFolderButton->setVisible(false);
    ui.DataFolderEdit->setVisible(false);
    ui.DataFolderEdit_EditLabel->setVisible(false);
    ui.DataFolderButton->setVisible(false);
    ui.FolderInfoButton2->setVisible(false);
    ui.DataFolderAutomaticButton->setVisible(false);
}
