// C++ port of ModernProfileEditorBaseFrameUnit.pas (implementation section).
#include "ModernProfileEditorBaseFrameUnit.h"

#include <algorithm>

#include "LanguageSetupUnit.h"
#include "IconManagerFormUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "CommonTools.h"
#include "GameDBToolsUnit.h"
#include "ScummVMToolsUnit.h"
#include "ExtraExeEditFormUnit.h"
#include "HelpConsts.h"
#include "ZipInfoFormUnit.h"
#include "IconLoaderUnit.h"
#include "ImageStretch.h"
#include "DosBoxUnit.h"

// TStrings.IndexOf (case insensitive)
static int ComboIndexOf(QComboBox* C, const QString& S)
{
    for (int I = 0; I < C->count(); I++) if (C->itemText(I).compare(S, Qt::CaseInsensitive) == 0) return I;
    return -1;
}

// Edit.Color:=Color (parent color, used for read only edits)
static void SetParentColor(QLineEdit* E, QWidget* Parent)
{
    QPalette P = E->palette();
    P.setColor(QPalette::Base, Parent->palette().color(QPalette::Window));
    E->setPalette(P);
}

/* TModernProfileEditorBaseFrame */

TModernProfileEditorBaseFrame::TModernProfileEditorBaseFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);

    connect(ui.IconSelectButton, &QPushButton::clicked, this, [this]() { IconButtonClick(ui.IconSelectButton); });
    connect(ui.IconDeleteButton, &QPushButton::clicked, this, [this]() { IconButtonClick(ui.IconDeleteButton); });
    connect(ui.ProfileNameEdit, &QLineEdit::textChanged, this, [this]() { ProfileNameEditChange(ui.ProfileNameEdit); });
    connect(ui.GameExeButton, &QToolButton::clicked, this, [this]() { ExeSelectButtonClick(ui.GameExeButton); });
    connect(ui.InfoButton1, &QToolButton::clicked, this, [this]() { GameRelPathButton(ui.InfoButton1); });
    connect(ui.GameExeEdit, &QLineEdit::textChanged, this, [this]() { GameExeEditChange(ui.GameExeEdit); });
    connect(ui.GameRelPathCheckBox, &QCheckBox::toggled, this, [this]() { RelPathCheckBoxClick(ui.GameRelPathCheckBox); });
    connect(ui.SetupExeButton, &QToolButton::clicked, this, [this]() { ExeSelectButtonClick(ui.SetupExeButton); });
    connect(ui.InfoButton2, &QToolButton::clicked, this, [this]() { GameRelPathButton(ui.InfoButton2); });
    connect(ui.SetupExeEdit, &QLineEdit::textChanged, this, [this]() { SetupExeEditChange(ui.SetupExeEdit); });
    connect(ui.SetupRelPathCheckBox, &QCheckBox::toggled, this, [this]() { RelPathCheckBoxClick(ui.SetupRelPathCheckBox); });
    connect(ui.GameButton, &QToolButton::clicked, this, [this]() { ExeSelectButtonClick(ui.GameButton); });
    connect(ui.GameZipButton, &QToolButton::clicked, this, [this]() { ExeSelectButtonClick(ui.GameZipButton); });
    connect(ui.GameComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { GameComboBoxChange(ui.GameComboBox); });
    connect(ui.GameZipEdit, &QLineEdit::textChanged, this, [this]() { GameZipEditChange(ui.GameZipEdit); });
    connect(ui.ExtraExeFilesButton, &QPushButton::clicked, this, [this]() { ExtraExeFilesButtonClick(ui.ExtraExeFilesButton); });

    ExtraExeFiles = new TStringList();
    ExtraExeFilesParameters = new TStringList();
}

TModernProfileEditorBaseFrame::~TModernProfileEditorBaseFrame()
{
    delete ExtraExeFiles;
    delete ExtraExeFilesParameters;
    delete OpenDialog;
}

void TModernProfileEditorBaseFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    InitData.OnCheckValue = [this](QObject* Sender, bool& OK) { CheckValue(Sender, OK); };
    InitData.OnShowFrame = [this](QObject* Sender) { ShowFrame(Sender); };
    InitData.AllowDefaultValueReset = false;
    InitData.OnProfileNameChangedCallback = [this](QObject* Sender) { ProfileNameChangedCallback(Sender); };

    ScummVM = false;
    WindowsMode = false;

    // NoFlicker(...): not needed with Qt

    FOnProfileNameChange = InitData.OnProfileNameChange;
    FGameDB = InitData.GameDB;

    ui.IconSelectButton->setText(LanguageSetup->ProfileEditorIconSelect());
    ui.IconDeleteButton->setText(LanguageSetup->ProfileEditorIconDelete());
    ui.ProfileNameEdit_EditLabel->setText(LanguageSetup->ProfileEditorProfileName());
    ui.ProfileFileNameEdit_EditLabel->setText(LanguageSetup->ProfileEditorFilename());

    ui.GameExeGroup->setTitle(LanguageSetup->ProfileEditorGame());
    ui.GameExeEdit_EditLabel->setText(LanguageSetup->ProfileEditorGameEXE());
    ui.GameExeButton->setToolTip(LanguageSetup->ChooseFile());
    ui.GameRelPathCheckBox->setText(LanguageSetup->ProfileEditorRelPath());
    ui.GameRelPathCheckBox->setToolTip(LanguageSetup->ProfileEditorRelPathHint());
    ui.RunAsAdminCheckBox->setText(LanguageSetup->ProfileEditorRunAsAdmin());
    ui.GameParameterEdit_EditLabel->setText(LanguageSetup->ProfileEditorGameParameters());
    UserIconLoader->DialogImage(DI_SelectFile, ui.GameExeButton);

    ui.SetupExeGroup->setTitle(LanguageSetup->ProfileEditorSetup());
    ui.SetupExeEdit_EditLabel->setText(LanguageSetup->ProfileEditorSetupEXE());
    ui.SetupExeButton->setToolTip(LanguageSetup->ChooseFile());
    ui.SetupRelPathCheckBox->setText(LanguageSetup->ProfileEditorRelPath());
    ui.SetupRelPathCheckBox->setToolTip(LanguageSetup->ProfileEditorRelPathHint());
    ui.SetupParameterEdit_EditLabel->setText(LanguageSetup->ProfileEditorSetupParameters());
    ui.InfoLabel->setText(LanguageSetup->ProfileEditorSetupInfo());
    ui.ExtraExeFilesButton->setText(LanguageSetup->ProfileEditorExtraExeFiles());
    UserIconLoader->DialogImage(DI_SelectFile, ui.SetupExeButton);
    UserIconLoader->DialogImage(DI_ExtraPrgFiles, ui.ExtraExeFilesButton);

    ui.GameGroup->setTitle(LanguageSetup->ProfileEditorScummVMGame());
    ui.GameEdit_EditLabel->setText(LanguageSetup->ProfileEditorScummVMGameFolder());
    ui.GameButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.GameZipCheckBox->setText(LanguageSetup->ProfileEditorScummVMGameZipFile());
    ui.GameZipButton->setToolTip(LanguageSetup->ChooseFile());
    ui.GameZipLabel->setText(LanguageSetup->ProfileEditorScummVMGameZipFileInfo());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.GameButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.GameZipButton);

    ProfileName = InitData.CurrentProfileName;
    ProfileExe = InitData.CurrentProfileExe;
    ProfileSetup = InitData.CurrentProfileSetup;
    ProfileScummVMGameName = InitData.CurrentScummVMGameName;
    ProfileScummVMPath = InitData.CurrentScummVMPath;
    ProfileDOSBoxInstallation = InitData.CurrentDOSBoxInstallation;
    ProfileCaptureDir = InitData.CurrentCaptureDir;

    for (QToolButton* B : {ui.InfoButton1, ui.InfoButton2}) {
        QFont F = B->font();
        if (F.pointSizeF() > 2) F.setPointSizeF(F.pointSizeF() - 2); else if (F.pixelSize() > 2) F.setPixelSize(F.pixelSize() - 2);
        B->setFont(F);
    }
    ui.InfoButton1->setText(LanguageSetup->ProfileEditorRelPathInfo());
    ui.InfoButton2->setText(LanguageSetup->ProfileEditorRelPathInfo());

    ui.IgnoreWindowsWarningsCheckBox->setText(LanguageSetup->ProfileEditorIgnoreWindowsWarnings());
    ui.IgnoreWindowsWarningsCheckBox->setToolTip(LanguageSetup->ProfileEditorIgnoreWindowsWarningsHints());

    ui.TurnOffDOSBoxFailedWarningCheckBox->setText(LanguageSetup->TurnOffDOSBoxFailedWarning());
    ui.TurnOffDOSBoxFailedWarningCheckBox->setToolTip(LanguageSetup->TurnOffDOSBoxFailedWarningHints());

    setProperty("HelpContext", ID_ProfileEditProfile);  // HelpContext
}

void TModernProfileEditorBaseFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    QString S;
    int I;

    FLoadFromTemplate = LoadFromTemplate && (Game != nullptr);

    if (Game == nullptr) {
        ui.ProfileFileNameEdit->setText(LanguageSetup->ProfileEditorNoFilename());
        return;
    }

    OldFileName = "";

    IconName = Game->Icon();
    LoadIcon();
    if (!LoadFromTemplate) {
        ui.ProfileNameEdit->setText(Game->Name());
    }
    if (LoadFromTemplate) {
        ui.ProfileFileNameEdit->setText(LanguageSetup->ProfileEditorNoFilename());
        SetParentColor(ui.ProfileFileNameEdit, this);
    } else {
        if (Game->SetupFile() != "") {
            OldFileName = Game->SetupFile();
            ui.ProfileFileNameEdit->setText(ChangeFileExt(ExtractFileName(Game->SetupFile()), ""));
            if (PrgSetup->RenameProfFileOnRenamingProfile()) {
                SetParentColor(ui.ProfileFileNameEdit, this);
            } else {
                ui.ProfileFileNameEdit->setReadOnly(false);
            }
        } else {
            /* Main Template */
            ui.ProfileNameEdit->setVisible(false);
            ui.ProfileNameEdit_EditLabel->setVisible(false);
            ui.ProfileFileNameEdit->setText(LanguageSetup->ProfileEditorNoFilename());
            SetParentColor(ui.ProfileFileNameEdit, this);
            ui.ProfileNameEdit->setReadOnly(true);
        }
    }

    if (ScummVMMode(Game)) {
        /* ScummVM mode */
        ScummVM = true;

        if (ScummVMGamesList->Count() == 0) ScummVMGamesList->LoadListFromScummVM(true);
        ui.GameComboBox->clear();
        {
            // Sorted:=False; AddStrings; Sorted:=True
            QStringList L = ScummVMGamesList->DescriptionList()->ToQStringList();
            std::stable_sort(L.begin(), L.end(), [](const QString& A, const QString& B) { return A.compare(B, Qt::CaseInsensitive) < 0; });
            ui.GameComboBox->addItems(L);
        }
        if (ui.GameComboBox->count() > 0) ui.GameComboBox->setCurrentIndex(0);
        S = Trim(ExtUpperCase(Game->ScummVMGame()));
        for (I = 0; I < ScummVMGamesList->NamesList()->Count(); I++) if (Trim(ExtUpperCase((*ScummVMGamesList->NamesList())[I])) == S) {
            ui.GameComboBox->setCurrentIndex(ComboIndexOf(ui.GameComboBox, (*ScummVMGamesList->DescriptionList())[I]));
            break;
        }
        ui.GameEdit->setText(Game->ScummVMPath());
        ui.GameZipEdit->setText(Trim(Game->ScummVMZip()));
        ui.GameZipCheckBox->setChecked(ui.GameZipEdit->text() != "");
    } else {
        if (WindowsExeMode(Game)) {
            /* Windows mode */
            WindowsMode = true;
            ui.GameExeEdit->setText(Game->GameExe());
            ui.GameParameterEdit->setText(Game->GameParameters());
            ui.SetupExeEdit->setText(Game->SetupExe());
            ui.SetupParameterEdit->setText(Game->SetupParameters());
            ui.RunAsAdminCheckBox->setChecked(Game->RunAsAdmin());
        } else {
            /* DOSBox mode */
            ui.GameRelPathCheckBox->setVisible(true);
            S = Trim(ExtUpperCase(Game->GameExe()));
            if (Copy(S, 1, 7) == "DOSBOX:") {
                ui.GameRelPathCheckBox->setChecked(true);
                ui.GameExeEdit->setText(Copy(Trim(Game->GameExe()), 8, MaxInt));
            } else {
                ui.GameExeEdit->setText(Game->GameExe());
            }
            ui.GameParameterEdit->setText(Game->GameParameters());

            S = Trim(ExtUpperCase(Game->SetupExe()));
            if (Copy(S, 1, 7) == "DOSBOX:") {
                ui.SetupRelPathCheckBox->setChecked(true);
                ui.SetupExeEdit->setText(Copy(Trim(Game->SetupExe()), 8, MaxInt));
            } else {
                ui.SetupExeEdit->setText(Game->SetupExe());
            }
            ui.SetupParameterEdit->setText(Game->SetupParameters());

            ui.IgnoreWindowsWarningsCheckBox->setChecked(Game->IgnoreWindowsFileWarnings());
            ui.TurnOffDOSBoxFailedWarningCheckBox->setChecked(Game->NoDOSBoxFailedDialog());
        }
        /* Windows and DOSBox mode */
        for (I = 0; I <= 49; I++) if (Trim(Game->ExtraPrgFile(I)) != "") {
            ExtraExeFiles->Add(Game->ExtraPrgFile(I));
            ExtraExeFilesParameters->Add(Game->ExtraPrgFileParameter(I));
        }
    }
}

void TModernProfileEditorBaseFrame::ShowFrame(QObject* Sender)
{
    if (ScummVM) {
        /* ScummVM mode */
        ui.GameExeGroup->setVisible(false);
        ui.SetupExeGroup->setVisible(false);
        ui.ExtraExeFilesButton->setVisible(false);
        ui.GameGroup->setVisible(true);
        GameComboBoxChange(this);
        ui.IgnoreWindowsWarningsCheckBox->setVisible(false);
        ui.TurnOffDOSBoxFailedWarningCheckBox->setVisible(false);
        ui.RunAsAdminCheckBox->setVisible(false);
    } else {
        ui.GameGroup->setVisible(false);  // (the layout moves the other groups up)
        if (WindowsMode) {
            /* Windows mode */
            ui.GameRelPathCheckBox->setVisible(false);
            ui.SetupRelPathCheckBox->setVisible(false);
            ui.InfoButton1->setVisible(false);
            ui.InfoButton2->setVisible(false);
            ui.IgnoreWindowsWarningsCheckBox->setVisible(false);
            ui.TurnOffDOSBoxFailedWarningCheckBox->setVisible(false);
            ui.RunAsAdminCheckBox->setVisible(PrgSetup->OfferRunAsAdmin());
        } else {
            /* DOSBox mode */
            GameExeEditChange(Sender); /* Make IgnoreWindowsWarningsCheckBox visible if needed */
            ui.TurnOffDOSBoxFailedWarningCheckBox->setVisible(ui.TurnOffDOSBoxFailedWarningCheckBox->isChecked());
            ui.RunAsAdminCheckBox->setVisible(false);
        }
    }
}

void TModernProfileEditorBaseFrame::ProfileNameChangedCallback(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ProfileNameEdit->setText(*ProfileName);
}

void TModernProfileEditorBaseFrame::CheckValue(QObject* Sender, bool& OK)
{
    Q_UNUSED(Sender);
    OK = true;

    if (Trim(ui.ProfileNameEdit->text()) == "" && (!ui.ProfileNameEdit->isReadOnly())) {
        MessageDlg(LanguageSetup->MessageNoProfileName(), mtError, mbOK, 0);
        OK = false;
        return;
    }
}

void TModernProfileEditorBaseFrame::GameComboBoxChange(QObject* Sender)
{
    if (ui.GameComboBox->currentIndex() >= 0)
        FOnProfileNameChange(Sender, ui.ProfileNameEdit->text(), *ProfileExe, *ProfileSetup, ScummVMGamesList->NameFromDescription(ui.GameComboBox->currentText()), *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);
}

void TModernProfileEditorBaseFrame::GetGame(TGame* Game)
{
    QString NewFileName;
    QString S;
    int I;

    Game->SetIcon(IconName);
    if (!ui.ProfileNameEdit->isReadOnly()) Game->SetName(ui.ProfileNameEdit->text());

    if (ScummVM) {
        /* ScummVM mode */
        Game->SetProfileMode("ScummVM");
        if (ui.GameComboBox->currentIndex() >= 0)
            Game->SetScummVMGame(ScummVMGamesList->NameFromDescription(ui.GameComboBox->currentText()));
        else
            Game->SetScummVMGame("");
        Game->SetScummVMPath(ui.GameEdit->text());
        if (ui.GameZipCheckBox->isChecked()) {
            Game->SetScummVMZip(Trim(ui.GameZipEdit->text()));
            GameZipEditChange(this);
        } else {
            Game->SetScummVMZip("");
        }
    } else {
        if (WindowsMode) {
            /* Windows mode */
            Game->SetProfileMode("Windows");
            ProfileEditorCloseCheck(Game, ui.GameExeEdit->text(), ui.SetupExeEdit->text());
            Game->SetGameExe(ui.GameExeEdit->text());
            Game->SetGameParameters(ui.GameParameterEdit->text());
            Game->SetSetupExe(ui.SetupExeEdit->text());
            Game->SetSetupParameters(ui.SetupParameterEdit->text());
            Game->SetRunAsAdmin(ui.RunAsAdminCheckBox->isChecked());
        } else {
            /* DOSBox mode */
            Game->SetProfileMode("DOSBox");
            ProfileEditorCloseCheck(Game, ui.GameExeEdit->text(), ui.SetupExeEdit->text());
            if (ui.GameRelPathCheckBox->isChecked()) S = "DOSBox:"; else S = "";
            Game->SetGameExe(S + ui.GameExeEdit->text());
            Game->SetGameParameters(ui.GameParameterEdit->text());
            if (ui.SetupRelPathCheckBox->isChecked()) S = "DOSBox:"; else S = "";
            Game->SetSetupExe(S + ui.SetupExeEdit->text());
            Game->SetSetupParameters(ui.SetupParameterEdit->text());

            Game->SetIgnoreWindowsFileWarnings(ui.IgnoreWindowsWarningsCheckBox->isChecked());
            Game->SetNoDOSBoxFailedDialog(ui.TurnOffDOSBoxFailedWarningCheckBox->isChecked());
        }

        for (I = 0; I <= 49; I++) { Game->SetExtraPrgFile(I, ""); Game->SetExtraPrgFileParameter(I, ""); }
        for (I = 0; I <= Min(49, ExtraExeFiles->Count() - 1); I++) {
            Game->SetExtraPrgFile(I, Trim((*ExtraExeFiles)[I]));
            Game->SetExtraPrgFileParameter(I, Trim((*ExtraExeFilesParameters)[I]));
        }
        for (I = Min(49, ExtraExeFiles->Count() - 1) + 1; I <= ExtraExeFiles->Count() - 1; I++) {
            Game->SetExtraPrgFile(I, "");
            Game->SetExtraPrgFileParameter(I, "");
        }
    }

    if (!ui.ProfileNameEdit->isReadOnly()) {
        if (PrgSetup->RenameProfFileOnRenamingProfile()) {
            ui.ProfileFileNameEdit->setText(Game->Name());
        }

        if (OldFileName != "" && ui.ProfileFileNameEdit->text() != ChangeFileExt(ExtractFileName(OldFileName), "")) {
            NewFileName = ui.ProfileFileNameEdit->text();
            if (ExtUpperCase(FGameDB->ProfFileName(NewFileName, false)) != ExtUpperCase(OldFileName))
                Game->RenameINI(FGameDB->ProfFileName(NewFileName, true));
        }
    }
}

void TModernProfileEditorBaseFrame::ProfileNameEditChange(QObject* Sender)
{
    if (!FOnProfileNameChange) return;  // OnChange during setupUi (before InitGUI)
    FOnProfileNameChange(Sender, ui.ProfileNameEdit->text(), *ProfileExe, *ProfileSetup, *ProfileScummVMGameName, *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);
}

void TModernProfileEditorBaseFrame::GameExeEditChange(QObject* Sender)
{
    QString S;

    if (!FOnProfileNameChange) return;  // OnChange during setupUi (before InitGUI)
    if (ui.GameRelPathCheckBox->isChecked()) S = "DOSBox:"; else S = "";
    FOnProfileNameChange(Sender, *ProfileName, S + ui.GameExeEdit->text(), *ProfileSetup, *ProfileScummVMGameName, *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);

    ui.IgnoreWindowsWarningsCheckBox->setVisible((!ui.GameRelPathCheckBox->isChecked()) && (IsWindowsExe(MakeAbsPath(ui.GameExeEdit->text(), PrgSetup->BaseDir())) || IsWindowsExe(MakeAbsPath(ui.SetupExeEdit->text(), PrgSetup->BaseDir()))));
}

void TModernProfileEditorBaseFrame::GameRelPathButton(QObject* Sender)
{
    Q_UNUSED(Sender);
    MessageDlg(LanguageSetup->ProfileEditorRelPathHint(), mtInformation, mbOK, 0);
}

void TModernProfileEditorBaseFrame::SetupExeEditChange(QObject* Sender)
{
    QString S;

    if (!FOnProfileNameChange) return;  // OnChange during setupUi (before InitGUI)
    if (ui.SetupRelPathCheckBox->isChecked()) S = "DOSBox:"; else S = "";
    FOnProfileNameChange(Sender, *ProfileName, *ProfileExe, S + ui.SetupExeEdit->text(), *ProfileScummVMGameName, *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);

    ui.IgnoreWindowsWarningsCheckBox->setVisible((!ui.GameRelPathCheckBox->isChecked()) && (IsWindowsExe(MakeAbsPath(ui.GameExeEdit->text(), PrgSetup->BaseDir())) || IsWindowsExe(MakeAbsPath(ui.SetupExeEdit->text(), PrgSetup->BaseDir()))));
}

void TModernProfileEditorBaseFrame::RelPathCheckBoxClick(QObject* Sender)
{
    QString S, T;

    switch (Sender->property("tag").toInt()) {
    case 0: ui.GameExeButton->setVisible(!ui.GameRelPathCheckBox->isChecked()); break;
    case 1: ui.SetupExeButton->setVisible(!ui.SetupRelPathCheckBox->isChecked()); break;
    }

    if (!FOnProfileNameChange) return;  // OnClick before InitGUI
    if (ui.GameRelPathCheckBox->isChecked()) S = "DOSBox:"; else S = "";
    if (ui.SetupRelPathCheckBox->isChecked()) T = "DOSBox:"; else T = "";
    FOnProfileNameChange(Sender, *ProfileName, S + ui.GameExeEdit->text(), T + ui.SetupExeEdit->text(), *ProfileScummVMGameName, *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);
}

void TModernProfileEditorBaseFrame::GameZipEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (Trim(ui.GameZipEdit->text()) != "") ui.GameZipCheckBox->setChecked(true);
}

void TModernProfileEditorBaseFrame::IconButtonClick(QObject* Sender)
{
    QString S;

    switch (Sender->property("tag").toInt()) {
    case 0:
        S = "";
        if (!ui.GameRelPathCheckBox->isChecked()) S = MakeAbsPath(ui.GameExeEdit->text(), PrgSetup->BaseDir());
        if (S == "" && (!ui.SetupRelPathCheckBox->isChecked())) S = MakeAbsPath(ui.SetupExeEdit->text(), PrgSetup->BaseDir());
        if (ShowIconManager(this, IconName, ExtractFilePath(S))) LoadIcon();
        break;
    case 1: IconName = ""; LoadIcon(); break;
    }
}

void TModernProfileEditorBaseFrame::LoadIcon()
{
    QString S;
    QPixmap P;

    if (IconName == "") {
        ui.IconImage->clearPicture();
        return;
    }

    S = MakeAbsIconName(IconName);
    if (!FileExists(S)) return;

    try {
        P = LoadImageFromFile(S); if (P.isNull()) return;
        // B.SetSize(IconImage.Width,IconImage.Height); ScaleImage(P,B)
        QPixmap B = P.scaled(ui.IconImage->width(), ui.IconImage->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ui.IconImage->setPicture(B);
    } catch (...) {}
}

void TModernProfileEditorBaseFrame::ExeSelectButtonClick(QObject* Sender)
{
    QString S;

    switch (Sender->property("tag").toInt()) {
    case 0:
        S = ui.GameExeEdit->text();
        if (!SelectProgramFile(S, ui.GameExeEdit->text(), ui.SetupExeEdit->text(), WindowsMode, -1, this)) return;
        ui.GameExeEdit->setText(S);
        GameExeEditChange(Sender);
        break;
    case 1:
        S = ui.SetupExeEdit->text();
        if (!SelectProgramFile(S, ui.SetupExeEdit->text(), ui.GameExeEdit->text(), WindowsMode, -1, this)) return;
        ui.SetupExeEdit->setText(S);
        SetupExeEditChange(Sender);
        break;
    case 2:
        if (Trim(ui.GameEdit->text()) == "") {
            if (PrgSetup->GameDir() == "")
                S = PrgSetup->BaseDir();
            else
                S = PrgSetup->GameDir();
        } else {
            S = MakeAbsPath(ui.GameEdit->text(), PrgSetup->BaseDir());
        }
        if (SelectDirectory(THandle(0), LanguageSetup->ProfileEditorScummVMGameFolderCaption(), S)) ui.GameEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
        break;
    case 3:
        if (Trim(ui.GameZipEdit->text()) == "") {
            if (PrgSetup->GameDir() == "") S = PrgSetup->BaseDir(); else S = PrgSetup->GameDir();
        } else S = ui.GameZipEdit->text();
        S = MakeAbsPath(S, PrgSetup->BaseDir());
        OpenDialog->DefaultExt = "zip";
        OpenDialog->InitialDir = ExtractFilePath(S);
        OpenDialog->Title = LanguageSetup->ProfileMountingZipFile();
        OpenDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());
        if (!OpenDialog->Execute()) return;
        ui.GameZipEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
        GameZipEditChange(Sender);
        break;
    }
}

void TModernProfileEditorBaseFrame::ExtraExeFilesButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShowExtraExeEditDialog(this, ExtraExeFiles, ExtraExeFilesParameters, WindowsMode, ui.GameExeEdit->text(), ui.SetupExeEdit->text());
}
