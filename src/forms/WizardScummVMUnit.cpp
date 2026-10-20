// C++ port of WizardScummVMUnit.pas (implementation section).
#include "WizardScummVMUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "ScummVMToolsUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"

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

// TWizardScummVMFrame

TWizardScummVMFrame::TWizardScummVMFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    connect(ui.ProgramButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.ProgramButton); });
    // the game in a folder that was typed in is recognized too
    connect(ui.ProgramEdit, &QLineEdit::editingFinished, this, [this]() { if (ui.ProgramEdit->isModified()) { ui.ProgramEdit->setModified(false); DetectGame(); } });
    connect(ui.GamesFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.GamesFolderButton); });
    connect(ui.FolderInfoButton, &QToolButton::clicked, this, [this]() { FolderInfoButtonClick(ui.FolderInfoButton); });
    connect(ui.BaseDataFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.BaseDataFolderButton); });
    connect(ui.DataFolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DataFolderButton); });
    connect(ui.DataFolderShowButton, &QPushButton::clicked, this, [this]() { DataFolderShowButtonClick(ui.DataFolderShowButton); });
}

TWizardScummVMFrame::~TWizardScummVMFrame()
{
}

void TWizardScummVMFrame::Init(TGameDB* GameDB)
{
    Q_UNUSED(GameDB);
    // SetVistaFonts(self); - not needed

    SetBold(ui.InfoLabel);
    SetBold(ui.BaseInfoLabel3);

    ui.InfoLabel->setText(LanguageSetup->WizardFormPage2Info());
    ui.ProgramEdit_EditLabel->setText(LanguageSetup->WizardFormProgramFolder());
    ui.ProgramButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.GamesFolderEdit_EditLabel->setText(LanguageSetup->WizardFormGamesFolder());
    ui.GamesFolderButton->setText(LanguageSetup->WizardFormExplorer());
    ui.GameNameLabel->setText(LanguageSetup->ProfileEditorScummVMGame());
    ui.DataFolderShowButton->setText(LanguageSetup->WizardFormDataFolderButton());
    ui.BaseInfoLabel3->setText(LanguageSetup->WizardFormInfoLabel3());
    ui.DataFolderAutomaticCheckBox->setText(LanguageSetup->WizardFormDataFolderCheckbox());
    ui.DataFolderEdit_EditLabel->setText(LanguageSetup->WizardFormDataFolder());
    ui.DataFolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.BaseDataFolderEdit_EditLabel->setText(LanguageSetup->WizardFormBaseDataFolder());
    ui.BaseDataFolderButton->setText(LanguageSetup->WizardFormExplorer());

    ui.GamesFolderEdit->setText(PrgSetup->GameDir());
    ui.BaseDataFolderEdit->setText(PrgSetup->DataDir());

    UserIconLoader->DialogImage(DI_SelectFolder, ui.ProgramButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.GamesFolderButton);
    UserIconLoader->DialogImage(DI_ToolbarHelp, ui.FolderInfoButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DataFolderButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.BaseDataFolderButton);
    UserIconLoader->DialogImage(DI_Down, ui.DataFolderShowButton);

    SetEditColorToParent(ui.GamesFolderEdit, this);
    SetEditColorToParent(ui.BaseDataFolderEdit, this);
}

void TWizardScummVMFrame::ShowScummVMFrame()
{
    if (ScummVMGamesList->Count() == 0) ScummVMGamesList->LoadListFromScummVM(true);
    ui.GameNameComboBox->clear();
    // Sorted:=False; AddStrings; Sorted:=True
    QStringList L = ScummVMGamesList->DescriptionList()->ToQStringList();
    std::sort(L.begin(), L.end(), [](const QString &A, const QString &B) { return QString::compare(A, B, Qt::CaseInsensitive) < 0; });
    ui.GameNameComboBox->addItems(L);
    if (ui.GameNameComboBox->count() > 0) ui.GameNameComboBox->setCurrentIndex(0);
}

void TWizardScummVMFrame::ButtonWork(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
    case 0: OpenFolder(ui.GamesFolderEdit->text()); break;   // ShellExecute(...,'explore',...)
    case 1:
        if (Trim(ui.ProgramEdit->text()).isEmpty())
            S = PrgSetup->GameDir();
        else
            S = MakeAbsPath(ui.ProgramEdit->text(), PrgSetup->BaseDir());
        if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
        S = MakeRelPath(S, PrgSetup->BaseDir(), true);
        ui.ProgramEdit->setText(S);
        DetectGame();
        break;
    case 2: OpenFolder(ui.BaseDataFolderEdit->text()); break;   // ShellExecute(...,'explore',...)
    case 3:
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

void TWizardScummVMFrame::WriteDataToGame(TGame* Game)
{
    QString S, T;
    int I;

    Game->SetScummVMPath(ui.ProgramEdit->text());
    if (ui.GameNameComboBox->currentIndex() >= 0)
        Game->SetScummVMGame(ScummVMGamesList->NameFromDescription(ui.GameNameComboBox->currentText()));
    else
        Game->SetScummVMGame(QString());
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

void TWizardScummVMFrame::DetectGame()
{
    // ScummVM recognizes the game in the folder (scummvm --detect); the game is selected
    ui.DetectInfoLabel->clear();
    if (Trim(ui.ProgramEdit->text()).isEmpty()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<TScummVMDetectedGame> Games = DetectScummVMGames(ui.ProgramEdit->text());
    QApplication::restoreOverrideCursor();
    if (Games.isEmpty()) {
        ui.DetectInfoLabel->setText(LanguageSetup->WizardFormScummVMNotDetected());
        return;
    }
    ui.DetectInfoLabel->setText(Format(LanguageSetup->WizardFormScummVMDetected(), {Games[0].Description}));
    const QString Description = ScummVMGamesList->DescriptionFromName(ScummVMGamesList->NameFromDetectedID(Games[0].ID));
    const int I = ui.GameNameComboBox->findText(Description);
    if (I >= 0) ui.GameNameComboBox->setCurrentIndex(I);
}

void TWizardScummVMFrame::FolderInfoButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    MessageDlg(Format(LanguageSetup->WizardFormGamesFolderInfo(), {PrgSetup->BaseDir()}), mtInformation, mbOK, 0);
}

QString TWizardScummVMFrame::GetScummVMGameName()
{
    if (ui.GameNameComboBox->currentIndex() >= 0)
        return ScummVMGamesList->NameFromDescription(ui.GameNameComboBox->currentText());
    return QString();
}

void TWizardScummVMFrame::DataFolderShowButtonClick(QObject* Sender)
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
}
