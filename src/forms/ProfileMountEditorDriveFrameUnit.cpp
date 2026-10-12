// C++ port of ProfileMountEditorDriveFrameUnit.pas (implementation section).
#include "ProfileMountEditorDriveFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "ModernProfileEditorBaseFrameUnit.h"
#include "ZipInfoFormUnit.h"
#include "PrgConsts.h"

TProfileMountEditorDriveFrame::TProfileMountEditorDriveFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // SaveDialog: TSaveDialog (DefaultExt = 'zip')
    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "zip";

    // DFM event wiring
    connect(ui.FolderButton, &QToolButton::clicked, this, [this]() { FolderButtonClick(ui.FolderButton); });
    connect(ui.FolderDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { FolderDriveLetterComboBoxChange(ui.FolderDriveLetterComboBox); });
    connect(ui.FolderFreeSpaceTrackBar, &QSlider::valueChanged, this, [this]() { FolderFreeSpaceTrackBarChange(ui.FolderFreeSpaceTrackBar); });
    connect(ui.MakeZipDriveButton, &QPushButton::clicked, this, [this]() { MakeZipDriveButtonClick(ui.MakeZipDriveButton); });
}

TProfileMountEditorDriveFrame::~TProfileMountEditorDriveFrame()
{
    delete SaveDialog;
}

// TProfileMountEditorDriveFrame

bool TProfileMountEditorDriveFrame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;
    SpecialSettings = "";

    ui.FolderFreeSpaceTrackBar->setValue(DefaultFreeHDSize);

    ui.FolderEdit_EditLabel->setText(LanguageSetup->ProfileMountingFolder());
    ui.FolderDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.FolderDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.FolderButton->setToolTip(LanguageSetup->ChooseFolder());

    for (char C = 'A'; C <= 'Y'; C++) ui.FolderDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.FolderDriveLetterComboBox->setCurrentIndex(2);

    ui.MakeZipDriveButton->setText(LanguageSetup->ProfileMountingZipMakeFromNormalMount());
    ui.MakeZipDriveButton->setToolTip(LanguageSetup->ProfileMountingZipMakeFromNormalMountHint());
    ui.MakeZipDriveButton->setVisible((InfoData.BaseGameFrame != nullptr) && (!InfoData.BaseGameFrame->ui.GameRelPathCheckBox->isChecked()));

    UserIconLoader->DialogImage(DI_SelectFolder, ui.FolderButton);
    UserIconLoader->DirectLoad("ModernProfileEditor", 4, ui.MakeZipDriveButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "DRIVE") {
        result = true;
        // RealFolder;DRIVE;Letter;False;;FreeSpace
        ui.FolderEdit->setText((*St)[0]);
        if (St->Count() >= 3) {
            I = ui.FolderDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.FolderDriveLetterComboBox->setCurrentIndex(I);
        }
        if (St->Count() >= 6) {
            try { I = StrToInt((*St)[5]); } catch (...) { I = 0; }
            if ((I >= 10) && (I <= 4000)) ui.FolderFreeSpaceTrackBar->setValue(I);
        }
    } else {
        result = false;
        for (I = 2; I <= ui.FolderDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.FolderDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.FolderDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    FolderDriveLetterComboBoxChange(this);
    FolderFreeSpaceTrackBarChange(this);
    return result;
}

void TProfileMountEditorDriveFrame::ShowFrame()
{
}

bool TProfileMountEditorDriveFrame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorDriveFrame::Done()
{
    QString S, T, result;

    if (SpecialSettings != "") return SpecialSettings;

    result = "";

    // RealFolder;DRIVE;Letter;False;;FreeSpace
    S = Trim(IncludeTrailingPathDelimiter(ui.FolderEdit->text()));
    if ((Length(S) == 3) && ((Copy(S, 2, 2) == ":\\") || (Copy(S, 2, 2) == ":/"))) {
        if (MessageDlg(LanguageSetup->MessageRootDirMountWaring(), mtWarning, mbYes | mbNo, 0) != mrYes) return result;
    }
    T = MakeRelPath(ui.FolderEdit->text(), PrgSetup->BaseDir()); if (T == "") T = "." + PathDelim;
    result = StringReplace(T, ";", "<semicolon>", rfReplaceAll) + ";Drive;" + ui.FolderDriveLetterComboBox->currentText() + ";False;;";
    if (ui.FolderFreeSpaceTrackBar->value() != DefaultFreeHDSize) result = result + IntToStr(ui.FolderFreeSpaceTrackBar->value());
    return result;
}

QString TProfileMountEditorDriveFrame::GetName()
{
    return LanguageSetup->ProfileMountingFolderSheet();
}

void TProfileMountEditorDriveFrame::FolderButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.FolderEdit->text()) == "") S = InfoData.DefaultInitialDir; else S = ui.FolderEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ProfileMountingFolder(), S)) return;
    ui.FolderEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TProfileMountEditorDriveFrame::FolderDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FolderDriveLetterWarningLabel->setVisible((ui.FolderDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.FolderDriveLetterComboBox->itemText(ui.FolderDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}

void TProfileMountEditorDriveFrame::FolderFreeSpaceTrackBarChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FolderFreeSpaceLabel->setText(Format(LanguageSetup->ProfileMountingFreeSpace(), {ui.FolderFreeSpaceTrackBar->value()}));
}

QString TProfileMountEditorDriveFrame::SelectZip()
{
    QString result = "";

    SaveDialog->InitialDir = PrgSetup->BaseDir();
    SaveDialog->Title = LanguageSetup->ProfileMountingZipMakeFromNormalMountSaveTitle();
    SaveDialog->Filter = ProcessFileNameFilter(LanguageSetup->ProfileMountingZipFileFilter2(), LanguageSetup->ProfileMountingZipFileFilter2ArchiveFiles());

    if (!SaveDialog->Execute()) return result;
    if (FileExists(SaveDialog->FileName)) {
        if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {SaveDialog->FileName}), mtWarning, mbYes | mbNo, 0) != mrYes) return result;
    }
    result = SaveDialog->FileName;
    return result;
}

bool TProfileMountEditorDriveFrame::UpdateGameExe(const QString& OldPath, const QString& DriveLetter, QString& Name)
{
    QString Old, S;
    bool result;

    Old = IncludeTrailingPathDelimiter(MakeAbsPath(OldPath, PrgSetup->BaseDir()));
    S = IncludeTrailingPathDelimiter(MakeAbsPath(ExtractFilePath(Name), PrgSetup->BaseDir()));
    result = (ExtUpperCase(Copy(S, 1, Length(Old))) == ExtUpperCase(Old)); if (!result) return result;
    // DOS path inside DOSBox: always '\'
    Name = DriveLetter + ":\\" + QString(Copy(S, Length(Old) + 1)).replace('/', '\\') + ExtractFileName(Name);
    return result;
}

void TProfileMountEditorDriveFrame::MakeZipDriveButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TModernProfileEditorBaseFrame* F;
    QString ThisGameDir, MountDir, ZipFile, S, T;
    int I, J;

    if ((Trim(ui.FolderEdit->text()) == "") || (Trim(ui.FolderEdit->text()) == "\\")) {
        MessageDlg(LanguageSetup->MessageNoFolderName(), mtError, mbOK, 0);
        return;
    }

    F = InfoData.BaseGameFrame;

    MountDir = IncludeTrailingPathDelimiter(MakeAbsPath(ui.FolderEdit->text(), PrgSetup->BaseDir()));

    if ((Trim(F->ui.GameExeEdit->text()) == "") || F->ui.GameRelPathCheckBox->isChecked()) ThisGameDir = ""; else ThisGameDir = IncludeTrailingPathDelimiter(MakeAbsPath(ExtractFilePath(F->ui.GameExeEdit->text()), PrgSetup->BaseDir()));

    if ((ThisGameDir == "") || (ExtUpperCase(Copy(ThisGameDir, 1, Length(MountDir))) != ExtUpperCase(MountDir))) {
        // Pack entry folder; no hints from game file, no game file changing
        if (ExtUpperCase(MountDir) == ExtUpperCase(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())))) {
            MessageDlg(LanguageSetup->ProfileMountingZipMakeFromNormalMountErrorGameDir(), mtError, mbOK, 0);
            return;
        }
        ZipFile = SelectZip(); if (ZipFile == "") return;
        if (!BaseDirSecuriryCheckOnly(MountDir)) {
            MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeFromNormalMountSecurityError(), {MountDir}), mtError, mbOK, 0);
            return;
        }
        if (MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeFromNormalMountConfirmation(), {MountDir, ZipFile}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
        if (!CreateZipFile(this, ZipFile, MountDir, dmFolder, GetCompressStrengthFromPrgSetup())) return;
        // RealFolder$ZipFile;ZIP;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)
        SpecialSettings = MakeRelPath(MountDir, PrgSetup->BaseDir()) + "$" + MakeRelPath(ZipFile, PrgSetup->BaseDir()) + ";ZIP;" + ui.FolderDriveLetterComboBox->currentText() + ";False;;" + IntToStr(ui.FolderFreeSpaceTrackBar->value()) + ";folder";
        InfoData.CloseRequest(this);
        return;
    }

    // Pack game folder (=folder of GameExe), change game file path
    if (ExtUpperCase(ThisGameDir) == ExtUpperCase(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir())))) {
        MessageDlg(LanguageSetup->ProfileMountingZipMakeFromNormalMountErrorGameDir(), mtError, mbOK, 0);
        return;
    }
    ZipFile = SelectZip();
    if (!BaseDirSecuriryCheckOnly(ThisGameDir)) {
        MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeFromNormalMountSecurityError(), {ThisGameDir}), mtError, mbOK, 0);
        return;
    }
    if (MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeFromNormalMountConfirmation(), {ThisGameDir, ZipFile}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
    if (!CreateZipFile(this, ZipFile, ThisGameDir, dmFolder, GetCompressStrengthFromPrgSetup())) return;

    S = F->ui.GameExeEdit->text();
    if (UpdateGameExe(ThisGameDir, ui.FolderDriveLetterComboBox->currentText(), S)) {
        F->ui.GameExeEdit->setText(S);
        F->ui.GameRelPathCheckBox->setChecked(true);
    }

    if ((Trim(F->ui.SetupExeEdit->text()) == "") || F->ui.SetupRelPathCheckBox->isChecked()) S = ""; else S = IncludeTrailingPathDelimiter(MakeAbsPath(ExtractFilePath(F->ui.SetupExeEdit->text()), PrgSetup->BaseDir()));
    if (ExtUpperCase(Copy(S, 1, Length(MountDir))) == ExtUpperCase(MountDir)) {
        S = F->ui.SetupExeEdit->text();
        if (UpdateGameExe(ThisGameDir, ui.FolderDriveLetterComboBox->currentText(), S)) {
            F->ui.SetupExeEdit->setText(S);
            F->ui.SetupRelPathCheckBox->setChecked(true);
        }
    }

    for (I = 0; I <= F->ExtraExeFiles->Count() - 1; I++) {
        S = (*F->ExtraExeFiles)[I];
        J = Pos(';', S);
        if (J > 0) {
            T = Copy(S, J + 1, MaxInt);
            if (UpdateGameExe(ThisGameDir, ui.FolderDriveLetterComboBox->currentText(), T)) S = Copy(S, 1, J) + "DOSBOX:" + T;
        }
        (*F->ExtraExeFiles)[I] = S;
    }

    // RealFolder$ZipFile;ZIP;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)
    SpecialSettings = MakeRelPath(ThisGameDir, PrgSetup->BaseDir()) + "$" + MakeRelPath(ZipFile, PrgSetup->BaseDir()) + ";ZIP;" + ui.FolderDriveLetterComboBox->currentText() + ";False;;" + IntToStr(ui.FolderFreeSpaceTrackBar->value()) + ";folder";
    InfoData.CloseRequest(this);
}
