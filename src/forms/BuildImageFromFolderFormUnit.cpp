// C++ port of BuildImageFromFolderFormUnit.pas (implementation section).
#include "BuildImageFromFolderFormUnit.h"
#include "PhysicalDrivesUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "CreateISOImageFormUnit.h"
#include "GameDBUnit.h"
#include "DosBoxUnit.h"
#include "PrgConsts.h"
#include "CreateImageToolsUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "WinApi.h"

TBuildImageFromFolderForm* BuildImageFromFolderForm = nullptr;

// GetDriveType(PChar(C+':\'))=DRIVE_REMOVABLE
static bool IsRemovableDrive(QChar C)
{
#ifdef Q_OS_WIN
    const QString S = QString(C) + ":\\";
    return GetDriveTypeW(reinterpret_cast<LPCWSTR>(S.utf16())) == DRIVE_REMOVABLE;
#else
    // Linux/BSD: letters assigned to the floppy drives (A, B) - raw access via
    // the device file needs write permission (e.g. "floppy" group)
    TPhysicalDrive D;
    return PhysicalDriveByLetter(C, D) && (D.Type == pdtFloppy);
#endif
}

static void SetLabelFontColor(QLabel* L, TColor C)
{
    QPalette P = L->palette();
    P.setColor(QPalette::WindowText, (C == clGrayText) ? L->palette().color(QPalette::Disabled, QPalette::WindowText) : TColorToQColor(C));
    L->setPalette(P);
}

TBuildImageFromFolderForm::TBuildImageFromFolderForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "img";

    connect(ui.FileNameButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FileNameButton); });
    connect(ui.FolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FolderButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.MakeBootableCheckBox, &QCheckBox::toggled, this, [this]() { MakeBootableCheckBoxClick(ui.MakeBootableCheckBox); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.ImageTypeGroupBox, &TRadioGroupWidget::clicked, this, [this]() { ImageTypeGroupBoxClick(ui.ImageTypeGroupBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TBuildImageFromFolderForm::~TBuildImageFromFolderForm()
{
    delete SaveDialog;
}

void TBuildImageFromFolderForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TBuildImageFromFolderForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    setWindowTitle(LanguageSetup->ImageFromFolderCaption());
    ui.FolderEdit_EditLabel->setText(LanguageSetup->ImageFromFolderSourceFolder());
    ui.FolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.FileNameEdit_EditLabel->setText(LanguageSetup->ImageFromFolderImageFile());
    ui.FileNameButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ImageTypeGroupBox->setTitle(LanguageSetup->ImageFromFolderImageType());
    ui.ImageTypeGroupBox->setItemText(0, LanguageSetup->ImageFromFolderImageTypeFloppy());
    ui.ImageTypeGroupBox->setItemText(1, LanguageSetup->ImageFromFolderImageTypeHarddisk());
    ui.FreeSpaceLabel->setText(LanguageSetup->ImageFromFolderAdditionalFreeSpace());
    ui.MakeBootableCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootable());
    ui.NeedFreeDOSLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    ui.AddKeyboardDriverCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithKeyboardDriver());
    ui.AddMouseDriverCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithMouseDriver());
    ui.MemoryManagerCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithMemoryManager());
    ui.AddFormatCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithFormat());
    ui.AddEditCheckbox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithEdit());
    ui.WriteToFloppyCheckBox->setText(LanguageSetup->ImageFromFolderWriteToFloppy());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    SaveDialog->Title = LanguageSetup->ImageFromFolderSaveDialogTitle();
    SaveDialog->Filter = LanguageSetup->ImageFromFolderSaveDialogFilter();

    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetLabelFontColor(ui.NeedFreeDOSLabel, clGrayText);
    } else {
        SetLabelFontColor(ui.NeedFreeDOSLabel, clRed);
    }

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.FolderButton);
    UserIconLoader->DialogImage(DI_SelectFile, ui.FileNameButton);
}

void TBuildImageFromFolderForm::FormShow(QObject* Sender)
{
    FloppyDriveAvailable = false;
    for (char C = 'A'; C <= 'Z'; C++) if (IsRemovableDrive(QChar(C))) { FloppyDriveAvailable = true; break; }
    ImageTypeGroupBoxClick(Sender);
}

void TBuildImageFromFolderForm::ButtonWork(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 0:
            S = Trim(ui.FolderEdit->text());
            if (S == "") S = PrgSetup->BaseDir(); else S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
            if (SelectDirectory(LanguageSetup->ExtractImageFolderTitle(), QString(), S, this)) ui.FolderEdit->setText(S);
            break;
        case 1:
            S = Trim(ui.FileNameEdit->text());
            if (S == "") S = PrgSetup->BaseDir(); else S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
            SaveDialog->InitialDir = S;
            if (SaveDialog->Execute()) ui.FileNameEdit->setText(SaveDialog->FileName);
            break;
    }
}

void TBuildImageFromFolderForm::ImageTypeGroupBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.WriteToFloppyCheckBox->setEnabled((ui.ImageTypeGroupBox->itemIndex() == 0) && FloppyDriveAvailable);
    ui.FreeSpaceLabel->setVisible(ui.ImageTypeGroupBox->itemIndex() == 1);
    ui.FreeSpaceEdit->setVisible(ui.ImageTypeGroupBox->itemIndex() == 1);
}

void TBuildImageFromFolderForm::MakeBootableCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.AddKeyboardDriverCheckBox->setEnabled(ui.MakeBootableCheckBox->isChecked());
    ui.AddMouseDriverCheckBox->setEnabled(ui.MakeBootableCheckBox->isChecked());
    ui.MemoryManagerCheckBox->setEnabled(ui.MakeBootableCheckBox->isChecked());
}

void TBuildImageFromFolderForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString FileName, Dir, FreeDOS;
    TDiskImageCreatorUpgrades Upgrades;
    int Size;

    FreeDOS = Trim(PrgSetup->PathToFREEDOS());
    if (FreeDOS != "") FreeDOS = IncludeTrailingPathDelimiter(MakeAbsPath(FreeDOS, PrgSetup->BaseDir()));
    if ((FreeDOS == "") || (!DirectoryExists(FreeDOS))) {
        MessageDlg(LanguageSetup->ProfileEditorNeedFreeDOS(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }

    Dir = Trim(ui.FolderEdit->text());
    if (Dir == "") {
        MessageDlg(LanguageSetup->MessageNoFolderName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(Dir), PrgSetup->BaseDir()));
    if (!DirectoryExists(Dir)) {
        MessageDlg(Format(LanguageSetup->MessageDirectoryNotFound(), {Dir}), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }

    FileName = Trim(ui.FileNameEdit->text());
    if (FileName == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    if (ExtractFileExt(FileName) == "") FileName = FileName + ".img";
    FileName = MakeAbsPath(FileName, PrgSetup->BaseDir());

    Upgrades = {dicuCopyFolder};
    if (ui.MakeBootableCheckBox->isEnabled() && ui.MakeBootableCheckBox->isChecked()) Upgrades.insert(dicuMakeBootable);
    if (ui.AddKeyboardDriverCheckBox->isEnabled() && ui.AddKeyboardDriverCheckBox->isChecked()) Upgrades.insert(dicuKeyboardDriver);
    if (ui.AddMouseDriverCheckBox->isEnabled() && ui.AddMouseDriverCheckBox->isChecked()) Upgrades.insert(dicuMouseDriver);
    if (ui.MemoryManagerCheckBox->isEnabled() && ui.MemoryManagerCheckBox->isChecked()) Upgrades.insert(dicuMemoryManager);
    if (ui.AddFormatCheckBox->isEnabled() && ui.AddFormatCheckBox->isChecked()) Upgrades.insert(dicuDiskTools);
    if (ui.AddEditCheckbox->isEnabled() && ui.AddEditCheckbox->isChecked()) Upgrades.insert(dicuEditor);

    TStringList St;
    St.Add(Dir);
    Size = DiskImageCreatorUpgradeSize(ui.ImageTypeGroupBox->itemIndex() == 1, Upgrades, &St);
    if (ui.ImageTypeGroupBox->itemIndex() == 0) {
        if (Size > 1457664) {
            MessageDlg(Format(LanguageSetup->ImageFromFolderNotEnoughSpace(), {Size / 1024, 1457664 / 1024}), mtError, mbOK, 0);
            setModalResult(mrNone); return;
        }
        if (!DiskImageCreator(FileName)) { setModalResult(mrNone); return; }
    } else {
        Size = (Size / 1024 / 1024) + 1 + ui.FreeSpaceEdit->value();
        if (!DiskImageCreator(Size, FileName, false, true)) { setModalResult(mrNone); return; }
    }

    if (!DiskImageCreatorUpgrade(FileName, ui.ImageTypeGroupBox->itemIndex() == 1, Upgrades, &St)) {
        setModalResult(mrNone);
        return;
    }
}

void TBuildImageFromFolderForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasImagesCreateFromFolder);
}

void TBuildImageFromFolderForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowBuildImageFromFolderDialog(QWidget* AOwner)
{
    bool result;
    BuildImageFromFolderForm = new TBuildImageFromFolderForm(AOwner);
    result = (BuildImageFromFolderForm->ShowModal() == mrOk);
    if (result && BuildImageFromFolderForm->ui.WriteToFloppyCheckBox->isChecked() && BuildImageFromFolderForm->ui.WriteToFloppyCheckBox->isEnabled()) {
        result = ShowWriteIMGImageDialog(AOwner, BuildImageFromFolderForm->ui.FileNameEdit->text());
    }
    delete BuildImageFromFolderForm;
    BuildImageFromFolderForm = nullptr;
    return result;
}
