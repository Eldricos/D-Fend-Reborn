// C++ port of ProfileMountEditorPhysFSFrameUnit.pas (implementation section).
#include "ProfileMountEditorPhysFSFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "ZipInfoFormUnit.h"
#include "IconLoaderUnit.h"
#include "ModernProfileEditorBaseFrameUnit.h"

namespace {
// Event filter calling a function (used for TComboBox.OnDropDown)
class TEventHook : public QObject
{
public:
    TEventHook(QObject *Target, std::function<void(QEvent *)> AHandler) : QObject(Target), FHandler(std::move(AHandler)) { Target->installEventFilter(this); }
    bool eventFilter(QObject *, QEvent *E) override { FHandler(E); return false; }
private:
    std::function<void(QEvent *)> FHandler;
};
}

TProfileMountEditorPhysFSFrame::TProfileMountEditorPhysFSFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // OpenDialog: TOpenDialog (DefaultExt = 'iso')
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "iso";

    // DFM event wiring
    connect(ui.ZipFolderButton, &QToolButton::clicked, this, [this]() { ZipFolderButtonClick(ui.ZipFolderButton); });
    connect(ui.ZipFileButton, &QToolButton::clicked, this, [this]() { ZipFileButtonClick(ui.ZipFileButton); });
    connect(ui.ZipFolderCreateButton, &QToolButton::clicked, this, [this]() { ZipFolderCreateButtonClick(ui.ZipFolderCreateButton); });
    connect(ui.ZipFolderDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ZipFolderDriveLetterComboBoxChange(ui.ZipFolderDriveLetterComboBox); });
    connect(ui.ZipFolderFreeSpaceTrackbar, &QSlider::valueChanged, this, [this]() { ZipFolderFreeSpaceTrackbarChange(ui.ZipFolderFreeSpaceTrackbar); });
    connect(ui.RepackTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { RepackTypeComboBoxChange(ui.RepackTypeComboBox); });
    new TEventHook(ui.RepackTypeComboBox, [this](QEvent *E) {
        if (E->type() == QEvent::MouseButtonPress || (E->type() == QEvent::KeyPress && (static_cast<QKeyEvent *>(E)->key() == Qt::Key_F4 || (static_cast<QKeyEvent *>(E)->modifiers() & Qt::AltModifier))))
            RepackTypeComboBoxDropDown(ui.RepackTypeComboBox);
    });
    connect(ui.MakeNormalDriveButton, &QPushButton::clicked, this, [this]() { MakeNormalDriveButtonClick(ui.MakeNormalDriveButton); });
    connect(ui.PopupExtractHere, &QAction::triggered, this, [this]() { MakeNormalDriveButtonClick(ui.PopupExtractHere); });
    connect(ui.PopupExtractInGamesFolder, &QAction::triggered, this, [this]() { MakeNormalDriveButtonClick(ui.PopupExtractInGamesFolder); });
}

TProfileMountEditorPhysFSFrame::~TProfileMountEditorPhysFSFrame()
{
    delete OpenDialog;
}

// TProfileMountEditorPhysFSFrame

bool TProfileMountEditorPhysFSFrame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S, T;
    int I;
    bool result;

    InfoData = AInfoData;
    SpecialSettings = "";

    ui.ZipFolderFreeSpaceTrackbar->setValue(DefaultFreeHDSize);

    ui.ZipFolderEdit_EditLabel->setText(LanguageSetup->ProfileMountingFolder());
    ui.ZipFolderDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.ZipFolderDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.ZipFileEdit_EditLabel->setText(LanguageSetup->ProfileMountingZipFile());

    ui.ZipFolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.ZipFolderCreateButton->setToolTip(LanguageSetup->ProfileMountingZipFileAutoSetupFolder());
    ui.ZipFileButton->setToolTip(LanguageSetup->ChooseFile());

    for (char C = 'A'; C <= 'Y'; C++) ui.ZipFolderDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.ZipFolderDriveLetterComboBox->setCurrentIndex(2);

    ui.RepackTypeLabel->setText(LanguageSetup->ProfileMountingZipRepack());
    while (ui.RepackTypeComboBox->count() < 6) ui.RepackTypeComboBox->addItem("");
    ui.RepackTypeComboBox->setItemText(0, LanguageSetup->ProfileMountingZipRepackNoDelete());
    ui.RepackTypeComboBox->setItemText(1, LanguageSetup->ProfileMountingZipRepackDeleteFiles());
    ui.RepackTypeComboBox->setItemText(2, LanguageSetup->ProfileMountingZipRepackDeleteFolder());
    ui.RepackTypeComboBox->setItemText(3, LanguageSetup->ProfileMountingZipRepackNoDeleteNoRepack());
    ui.RepackTypeComboBox->setItemText(4, LanguageSetup->ProfileMountingZipRepackDeleteFilesNoRepack());
    ui.RepackTypeComboBox->setItemText(5, LanguageSetup->ProfileMountingZipRepackDeleteFolderNoRepack());
    ui.RepackTypeComboBox->setCurrentIndex(1);

    ui.MakeNormalDriveButton->setText(LanguageSetup->ProfileMountingZipMakeNormalMount());
    ui.MakeNormalDriveButton->setToolTip(LanguageSetup->ProfileMountingZipMakeNormalMountHint());
    ui.PopupExtractHere->setText(LanguageSetup->ProfileMountingZipMakeNormalMountHere());
    ui.PopupExtractInGamesFolder->setText(LanguageSetup->ProfileMountingZipMakeNormalMountGamesDir());

    UserIconLoader->DialogImage(DI_SelectFolder, ui.ZipFolderButton);
    UserIconLoader->DialogImage(DI_Create, ui.ZipFolderCreateButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.ZipFileButton);
    UserIconLoader->DirectLoad("ModernProfileEditor", 4, ui.MakeNormalDriveButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "PHYSFS") {
        // RealFolder$ZipFile;PHYSFS;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)
        result = true;
        S = Trim((*St)[0]); I = Pos('$', S); if (I == 0) T = ""; else { T = Trim(Copy(S, I + 1, MaxInt)); S = Trim(Copy(S, 1, I - 1)); }
        ui.ZipFolderEdit->setText(S); ui.ZipFileEdit->setText(T);
        if (St->Count() >= 3) {
            I = ui.ZipFolderDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.ZipFolderDriveLetterComboBox->setCurrentIndex(I);
        }
        if (St->Count() >= 6) {
            try { I = StrToInt((*St)[5]); } catch (...) { I = 0; }
            if ((I >= 10) && (I <= 4000)) ui.ZipFolderFreeSpaceTrackbar->setValue(I);
        }
        if (St->Count() >= 7) {
            S = Trim(ExtUpperCase((*St)[6]));
            if (S == "NO") ui.RepackTypeComboBox->setCurrentIndex(0);
            if (S == "FILES") ui.RepackTypeComboBox->setCurrentIndex(1);
            if (S == "FOLDER") ui.RepackTypeComboBox->setCurrentIndex(2);
            if (S == "NO-NOREPACK") ui.RepackTypeComboBox->setCurrentIndex(3);
            if (S == "FILES-NOREPACK") ui.RepackTypeComboBox->setCurrentIndex(4);
            if (S == "FOLDER-NOREPACK") ui.RepackTypeComboBox->setCurrentIndex(5);
        }
    } else {
        result = false;
        for (I = 2; I <= ui.ZipFolderDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.ZipFolderDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.ZipFolderDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    ZipFolderDriveLetterComboBoxChange(this);
    ZipFolderFreeSpaceTrackbarChange(this);
    RepackTypeComboBoxChange(this);
    return result;
}

void TProfileMountEditorPhysFSFrame::ShowFrame()
{
}

bool TProfileMountEditorPhysFSFrame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorPhysFSFrame::Done()
{
    QString S, T;
    bool B;

    if (SpecialSettings != "") return SpecialSettings;

    if (Trim(ui.ZipFolderEdit->text()) == "") {
        MessageDlg(LanguageSetup->MessageNoFolderNameForMounting(), mtError, mbOK, 0);
        return "";
    }

    if (Trim(ui.ZipFileEdit->text()) == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        return "";
    }

    B = true;

    S = IncludeTrailingPathDelimiter(Trim(ExtUpperCase(MakeRelPath(ui.ZipFolderEdit->text(), PrgSetup->BaseDir(), true))));
    T = IncludeTrailingPathDelimiter(Trim(ExtUpperCase(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true))));
    if (Copy(S, 1, 2) == "." + PathDelim) S = Trim(Copy(S, 3, MaxInt));
    if (Copy(T, 1, 2) == "." + PathDelim) T = Trim(Copy(T, 3, MaxInt));

    if ((ui.RepackTypeComboBox->currentIndex() > 0) && (S == T)) {
        if (MessageDlg(Format(LanguageSetup->ProfileMountingFolderDeleteWarning(), {PrgSetup->GameDir()}), mtWarning, mbYes | mbNo, 0) != mrYes) B = false;
    }

    // RealFolder$ZipFile;PHYSFS;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)
    S = StringReplace(MakeRelPath(ui.ZipFolderEdit->text(), PrgSetup->BaseDir()) + "$" + MakeRelPath(ui.ZipFileEdit->text(), PrgSetup->BaseDir()), ";", "<semicolon>", rfReplaceAll) + ";PhysFS;" + ui.ZipFolderDriveLetterComboBox->currentText() + ";False;";
    S = S + ";" + IntToStr(ui.ZipFolderFreeSpaceTrackbar->value());
    switch (ui.RepackTypeComboBox->currentIndex()) {
        case 0: S = S + ";no"; break;
        case 1: S = S + ";files"; break;
        case 2: S = S + ";folder"; break;
        case 3: S = S + ";no-norepack"; break;
        case 4: S = S + ";files-norepack"; break;
        case 5: S = S + ";folder-norepack"; break;
    }

    if (B) return S; else return "";
}

QString TProfileMountEditorPhysFSFrame::GetName()
{
    return LanguageSetup->ProfileMountingPhysFSSheet();
}

void TProfileMountEditorPhysFSFrame::ZipFolderButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.ZipFolderEdit->text()) == "") S = InfoData.DefaultInitialDir; else S = ui.ZipFolderEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ProfileMountingFolder(), S)) return;
    ui.ZipFolderEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TProfileMountEditorPhysFSFrame::ZipFolderDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ZipFolderDriveLetterWarningLabel->setVisible((ui.ZipFolderDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.ZipFolderDriveLetterComboBox->itemText(ui.ZipFolderDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}

void TProfileMountEditorPhysFSFrame::ZipFolderFreeSpaceTrackbarChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.ZipFolderFreeSpaceLabel->setText(Format(LanguageSetup->ProfileMountingFreeSpace(), {ui.ZipFolderFreeSpaceTrackbar->value()}));
}

void TProfileMountEditorPhysFSFrame::ZipFileButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.ZipFileEdit->text()) == "") S = InfoData.DefaultInitialDir; else S = ui.ZipFileEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    OpenDialog->DefaultExt = "zip";
    OpenDialog->InitialDir = ExtractFilePath(S);
    OpenDialog->Title = LanguageSetup->ProfileMountingZipFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingZipFileFilter();
    if (!OpenDialog->Execute()) return;
    ui.ZipFileEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
}

void TProfileMountEditorPhysFSFrame::ZipFolderCreateButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    const QString AllowedChars = QString::fromUtf8("ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜabcdefghijklmnopqrstuvwxyz01234567890-_=");
    QString S;
    int I;

    S = "";
    for (I = 1; I <= Length(InfoData.ProfileFileName); I++) if (Pos(InfoData.ProfileFileName[I - 1], AllowedChars) > 0) S = S + InfoData.ProfileFileName[I - 1];
    if (S == "") S = "Game";

    S = ExtUpperCase(S);
    if (Length(S) > 8) S.truncate(8);

    S = MakeAbsPath("." + PathDelim + PhysFSDefaultWriteDir + PathDelim + S, PrgSetup->BaseDir());
    if (!ForceDirectories(S)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {S}), mtError, mbOK, 0);
    }

    ui.ZipFolderEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TProfileMountEditorPhysFSFrame::MakeNormalDriveButtonClick(QObject* Sender)
{
    QPoint P;
    QString ZipFolder, ZipFile, GamesDir, S;

    if (Trim(ui.ZipFolderEdit->text()) == "") {
        MessageDlg(LanguageSetup->MessageNoFolderNameForMounting(), mtError, mbOK, 0);
        return;
    }

    if (Trim(ui.ZipFileEdit->text()) == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        return;
    }

    ZipFolder = IncludeTrailingPathDelimiter(MakeAbsPath(ui.ZipFolderEdit->text(), PrgSetup->BaseDir()));
    ZipFile = MakeAbsPath(ui.ZipFileEdit->text(), PrgSetup->BaseDir());
    GamesDir = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir()));

    if (!FileExists(ZipFile)) {
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {ZipFile}), mtError, mbOK, 0);
        return;
    }

    if (qobject_cast<QPushButton *>(Sender) != nullptr) {   // Sender is TBitBtn
        if (ExtUpperCase(Copy(ZipFolder, 1, Length(GamesDir))) == ExtUpperCase(GamesDir)) {
            Sender = ui.PopupExtractHere;
        } else {
            P = ui.MakeNormalDriveButton->mapToGlobal(QPoint(0, 0));  // ClientToScreen(Point(Left,Top))
            ui.PopupMenu->popup(P);
            return;
        }
    }

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeNormalMountConfirmation(), {ZipFile, ZipFolder}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            // Extract here
            ExtractZipDrive(this, ZipFile, ZipFolder, ZipFolder);
            // RealFolder;DRIVE;Letter;False;;FreeSpace
            if (ExtUpperCase(Copy(ZipFolder, 1, Length(GamesDir))) == ExtUpperCase(GamesDir)) S = GamesDir; else S = ZipFolder;
            SpecialSettings = MakeRelPath(S, PrgSetup->BaseDir()) + ";DRIVE;" + ui.ZipFolderDriveLetterComboBox->currentText() + ";False;;" + IntToStr(ui.ZipFolderFreeSpaceTrackbar->value());
            UpdateGameExe(MakeRelPath(ZipFolder, PrgSetup->BaseDir()));
            InfoData.CloseRequest(this);
            break;
        case 1:
            // Extract to games dir subfolder
            S = ZipFolder;
            if ((S != "") && (S[Length(S) - 1] == PathDelimChar)) S.truncate(Length(S) - 1);
            while (Pos(PathDelim, S) != 0) S = Copy(S, Pos(PathDelim, S) + 1, MaxInt);
            do {
                if (!InputQuery(LanguageSetup->ProfileMountingZipMakeNormalMountWorkCaption(), LanguageSetup->ProfileMountingZipMakeNormalMountWorkLabel(), S, this)) return;
                S = MakeFileSysOKFolderName(S);
                if (DirectoryExists(GamesDir + S)) { MessageDlg(Format(LanguageSetup->MessageDirectoryAlreadyExists(), {GamesDir + S}), mtError, mbOK, 0); return; }
            } while (DirectoryExists(GamesDir + S));
            if (MessageDlg(Format(LanguageSetup->ProfileMountingZipMakeNormalMountConfirmation(), {ZipFile, GamesDir + S}), mtConfirmation, mbYes | mbNo, 0) != mrYes) return;
            ExtractZipDrive(this, ZipFile, ZipFolder, GamesDir + S);
            // RealFolder;DRIVE;Letter;False;;FreeSpace
            SpecialSettings = MakeRelPath(GamesDir, PrgSetup->BaseDir()) + ";DRIVE;" + ui.ZipFolderDriveLetterComboBox->currentText() + ";False;;" + IntToStr(ui.ZipFolderFreeSpaceTrackbar->value());
            UpdateGameExe(MakeRelPath(GamesDir, PrgSetup->BaseDir()));
            InfoData.CloseRequest(this);
            break;
    }
}

void TProfileMountEditorPhysFSFrame::RepackTypeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetComboHint(ui.RepackTypeComboBox);
}

void TProfileMountEditorPhysFSFrame::RepackTypeComboBoxDropDown(QObject* Sender)
{
    Q_UNUSED(Sender);
    SetComboDropDownDropDownWidth(ui.RepackTypeComboBox);
}

bool TProfileMountEditorPhysFSFrame::ChangeExtraExeFile(const QString& NewFolder, QString& ExeName)
{
    QString S;

    if (Copy(Trim(ExtUpperCase(ExeName)), 1, 7) != "DOSBOX:") return false;
    S = Copy(Trim(ExeName), 8, MaxInt);
    if ((Length(S) >= 3) && (ExtUpperCase(QString(S[0])) == ui.ZipFolderDriveLetterComboBox->currentText()) && (Copy(S, 2, 2) == ":\\")) {
        S = Copy(S, 4, MaxInt);
        // Note: the original code uses the file name of the setup program here
        ExeName = IncludeTrailingPathDelimiter(NewFolder) + ToNativePath(S) + ExtractFileName(InfoData.BaseGameFrame->ui.SetupExeEdit->text());
        return true;
    }
    return false;
}

void TProfileMountEditorPhysFSFrame::UpdateGameExe(const QString& NewFolder)
{
    QString S, T;
    int I, J;
    TModernProfileEditorBaseFrame *F = InfoData.BaseGameFrame;

    if (F == nullptr) return;

    if (F->ui.GameRelPathCheckBox->isChecked()) {
        S = Trim(IncludeTrailingPathDelimiter(ExtractFilePath(F->ui.GameExeEdit->text())));
        if ((Length(S) >= 3) && (ExtUpperCase(QString(S[0])) == ui.ZipFolderDriveLetterComboBox->currentText()) && (Copy(S, 2, 2) == ":\\")) {
            S = Copy(S, 4, MaxInt);
            F->ui.GameExeEdit->setText(IncludeTrailingPathDelimiter(NewFolder) + ToNativePath(S) + ExtractFileName(F->ui.GameExeEdit->text()));
            F->ui.GameRelPathCheckBox->setChecked(false);
            F->GameExeEditChange(F->ui.GameExeEdit);
        }
    }

    if (F->ui.SetupRelPathCheckBox->isChecked()) {
        S = Trim(IncludeTrailingPathDelimiter(ExtractFilePath(F->ui.SetupExeEdit->text())));
        if ((Length(S) >= 3) && (ExtUpperCase(QString(S[0])) == ui.ZipFolderDriveLetterComboBox->currentText()) && (Copy(S, 2, 2) == ":\\")) {
            S = Copy(S, 4, MaxInt);
            F->ui.SetupExeEdit->setText(IncludeTrailingPathDelimiter(NewFolder) + ToNativePath(S) + ExtractFileName(F->ui.SetupExeEdit->text()));
            F->ui.SetupRelPathCheckBox->setChecked(false);
            F->SetupExeEditChange(F->ui.SetupExeEdit);
        }
    }

    for (I = 0; I <= F->ExtraExeFiles->Count() - 1; I++) {
        S = (*F->ExtraExeFiles)[I];
        J = Pos(';', S);
        if (J > 0) {
            T = Copy(S, J + 1, MaxInt);
            if (ChangeExtraExeFile(NewFolder, T)) S = Copy(S, 1, J) + T;
        }
        (*F->ExtraExeFiles)[I] = S;
    }
}
