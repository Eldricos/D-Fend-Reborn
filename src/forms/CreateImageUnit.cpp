// C++ port of CreateImageUnit.pas (implementation section).
#include "CreateImageUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "ImageTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "CreateImageToolsUnit.h"
#include "CreateISOImageFormUnit.h"
#include "WinApi.h"

TCreateImageForm* CreateImageForm = nullptr;
TGame* NewProfileFromImageCreator = nullptr;

// GetDriveType(PChar(C+':\'))=DRIVE_REMOVABLE
static bool IsRemovableDrive(QChar C)
{
#ifdef Q_OS_WIN
    const QString S = QString(C) + ":\\";
    return GetDriveTypeW(reinterpret_cast<LPCWSTR>(S.utf16())) == DRIVE_REMOVABLE;
#else
    // TODO(port): drive letters (raw floppy access) only exist on Windows.
    Q_UNUSED(C);
    return false;
#endif
}

static void SetLabelFontColor(QLabel* L, TColor C)
{
    QPalette P = L->palette();
    P.setColor(QPalette::WindowText, (C == clGrayText) ? L->palette().color(QPalette::Disabled, QPalette::WindowText) : TColorToQColor(C));
    L->setPalette(P);
}

/*
Sectors=cyl*heads*spt*bps; (spt=Sectors per Track; bps=Bytes per Sector=512)

Floppy Formats:

cyl=40; heads=1; spt= 8;   160kb= 163840 (5,25" Single-Sided, Double Density (DD))
cyl=40; heads=1; spt= 9;   180kb= 184320 (5,25" Single-Sided, Double Density (DD))
cyl=40; heads=2; spt= 8;   320kb= 327680 (5,25" Double-Sided, Double Density (DD))
cyl=40; heads=2; spt= 9;   360kb= 368640 (5,25" Double-Sided, Double Density (DD))
cyl=80; heads=2; spt= 9;   720kb= 737280 (3,5" Double-Sided, Double Density (DD))
cyl=80; heads=2; spt=15;  1200kb=1228800 (5,25" Double-Sided, High-Density (HD))
cyl=80; heads=2; spt=18;  1440kb=1474560 (3,5" Double-Sided, High-Density (HD))
cyl=80; heads=2; spt=21;  1680kb=1720320 (3,5" Double-Sided, Custom Density)
cyl=82; heads=2; spt=21;  1720kb=1763328 (3,5" Double-Sided, Custom Density)
cyl=80; heads=2; spt=36;  2880kb=2949120 (3,5" Double-Sided, Extended Density (ED))

HD Formats:
cyl=63; heads=16; spt=X
*/

static int FloppyCylinders[8] = {40, 40, 40, 40, 80, 80, 80, 80};
static int FloppyHeads[8]     = { 1,  1,  2,  2,  2,  2,  2,  2};
static int FloppySPT[8]       = { 8,  9,  8,  9,  9, 15, 18, 36};

const int FloppyDefault = 6;

TCreateImageForm::TCreateImageForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = TImageList::FromResource("CreateImageUnit", "ImageList");
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "img";

    // VCL OnChange of combo boxes only fires on user changes -> activated
    connect(ui.ImageFileButton, &QToolButton::clicked, this, [this]() { ImageFileButtonClick(ui.ImageFileButton); });
    connect(ui.PageControl, &QTabWidget::currentChanged, this, [this]() { PageControlChange(ui.PageControl); });
    connect(ui.FloppyImageTypeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { FloppyImageTypeComboBoxChange(ui.FloppyImageTypeComboBox); });
    connect(ui.FloppyCylindersComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { FloppyDataChange(ui.FloppyCylindersComboBox); });
    connect(ui.FloppyHeadsComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { FloppyDataChange(ui.FloppyHeadsComboBox); });
    connect(ui.FloppySPTComboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() { FloppyDataChange(ui.FloppySPTComboBox); });
    connect(ui.HDSizeEdit, &QLineEdit::textChanged, this, [this]() { HDSizeEditChange(ui.HDSizeEdit); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.MakeBootableCheckBox, &QCheckBox::toggled, this, [this]() { MakeBootableCheckBoxClick(ui.MakeBootableCheckBox); });
    connect(ui.FormatImageCheckBox, &QCheckBox::toggled, this, [this]() { MakeBootableCheckBoxClick(ui.FormatImageCheckBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TCreateImageForm::~TCreateImageForm()
{
    delete OpenDialog;
    delete ImageList;
}

void TCreateImageForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TCreateImageForm::FormCreate(QObject* Sender)
{
    // DoubleBuffered / SetVistaFonts / Font.Charset: not needed in Qt

    {
        QPalette P = ui.FloppyImageSizeEdit->palette();
        P.setColor(QPalette::Base, palette().color(QPalette::Window));   // Color:=clBtnFace
        ui.FloppyImageSizeEdit->setPalette(P);
        P = ui.HDGeometryEdit->palette();
        P.setColor(QPalette::Base, palette().color(QPalette::Window));
        ui.HDGeometryEdit->setPalette(P);
    }

    setWindowTitle(LanguageSetup->CreateImageForm());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.FloppyImageSheet), LanguageSetup->ProfileMountingFloppyImageSheet());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.HDImageSheet), LanguageSetup->ProfileMountingImageSheet());
    ui.FloppyImageTypeLabel->setText(LanguageSetup->CreateImageFormFloppyImageType());
    ui.FloppyCylindersLabel->setText(LanguageSetup->CreateImageFormCylinders());
    ui.FloppyHeadsLabel->setText(LanguageSetup->CreateImageFormHeads());
    ui.FloppySPTLabel->setText(LanguageSetup->CreateImageFormSPT());
    ui.FloppyImageSizeEdit_EditLabel->setText(LanguageSetup->CreateImageFormFloppyImageSize());
    ui.ImageFileEdit_EditLabel->setText(LanguageSetup->CreateImageFormFilename());
    ui.ImageFileButton->setToolTip(LanguageSetup->ChooseFile());
    ui.CompressedCheckBox->setText(LanguageSetup->CreateImageFormCompression());
    ui.MakeBootableCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootable());
    ui.NeedFreeDOSLabel->setText(LanguageSetup->ProfileEditorNeedFreeDOS());
    ui.AddKeyboardDriverCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithKeyboardDriver());
    ui.AddMouseDriverCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithMouseDriver());
    ui.MemoryManagerCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithMemoryManager());
    ui.AddFormatCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithFormat());
    ui.AddEditCheckbox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithEdit());
    ui.AddDoszipCheckBox->setText(LanguageSetup->CreateImageFormMakeFloppyBootableWithDoszip());
    ui.HDSizeEdit_EditLabel->setText(LanguageSetup->CreateImageFormHDImageSize());
    ui.HDGeometryEdit_EditLabel->setText(LanguageSetup->CreateImageFormHDImageGeometry());
    ui.FormatImageCheckBox->setText(LanguageSetup->CreateImageFormFormat());
    ui.WriteToFloppyCheckBox->setText(LanguageSetup->ImageFromFolderWriteToFloppy());
    ui.CreateProfileCheckBox->setText(LanguageSetup->CreateImageFormCreateProfile());

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_SelectFile, ui.ImageFileButton);
    UserIconLoader->DialogImage(DI_ImageFloppy, ImageList, 0);
    UserIconLoader->DialogImage(DI_ImageHD, ImageList, 1);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
    // PageControl.Images = ImageList (FloppyImageSheet: ImageIndex 0, HDImageSheet: ImageIndex 1)
    ui.PageControl->setTabIcon(ui.PageControl->indexOf(ui.FloppyImageSheet), ImageList->Icon(0));
    ui.PageControl->setTabIcon(ui.PageControl->indexOf(ui.HDImageSheet), ImageList->Icon(1));

    if (DirectoryExists(IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir())))) {
        SetLabelFontColor(ui.NeedFreeDOSLabel, clGrayText);
    } else {
        SetLabelFontColor(ui.NeedFreeDOSLabel, clRed);
    }

    if (FileExists(PrgDir() + MakeDOSFilesystemFileName) || FileExists(PrgDir() + BinFolder + PathDelim + MakeDOSFilesystemFileName)) {
        ui.FloppyCylindersComboBox->addItem("40");
        ui.FloppyCylindersComboBox->addItem("80");
        ui.FloppySPTComboBox->addItem("8");
        ui.FloppySPTComboBox->addItem("9");
        ui.FloppySPTComboBox->addItem("15");
        ui.FloppySPTComboBox->addItem("18");
        ui.FloppySPTComboBox->addItem("36");
    } else {
        for (int I = 1; I <= 100; I++) ui.FloppyCylindersComboBox->addItem(IntToStr(I));
        for (int I = 1; I <= 40; I++) ui.FloppySPTComboBox->addItem(IntToStr(I));
    }
    for (int I = 1; I <= 2; I++) ui.FloppyHeadsComboBox->addItem(IntToStr(I));
    ui.FloppyImageTypeComboBox->addItem("5,25\" single-sided, double density (DD)");
    ui.FloppyImageTypeComboBox->addItem("5,25\" single-sided, double density (DD)");
    ui.FloppyImageTypeComboBox->addItem("5,25\" double-sided, double density (DD)");
    ui.FloppyImageTypeComboBox->addItem("5,25\" double-sided, double density (DD)");
    ui.FloppyImageTypeComboBox->addItem("3,5\" double-sided, double density (DD)");
    ui.FloppyImageTypeComboBox->addItem("5,25\" double-sided, high-density (HD)");
    ui.FloppyImageTypeComboBox->addItem("3,5\" double-sided, high-density (HD)");
    ui.FloppyImageTypeComboBox->addItem("3,5\" double-sided, extended density (ED)");
    ui.FloppyImageTypeComboBox->addItem("Custom format");

    JustChanging = false;
    ui.FloppyImageTypeComboBox->setCurrentIndex(FloppyDefault);
    FloppyImageTypeComboBoxChange(Sender);
    HDSizeEditChange(Sender);

    OpenDialog->Title = LanguageSetup->ProfileMountingFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilterImgOnly();
}

void TCreateImageForm::FormShow(QObject* Sender)
{
    FloppyDriveAvailable = false;
    for (char C = 'A'; C <= 'Z'; C++) if (IsRemovableDrive(QChar(C))) { FloppyDriveAvailable = true; break; }
    PageControlChange(Sender);
}

void TCreateImageForm::FloppyImageTypeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging) return;
    if (ui.FloppyImageTypeComboBox->currentIndex() == 8) return;

    JustChanging = true;
    ui.FloppyCylindersComboBox->setCurrentIndex(
        ui.FloppyCylindersComboBox->findText(IntToStr(FloppyCylinders[ui.FloppyImageTypeComboBox->currentIndex()])));
    ui.FloppyHeadsComboBox->setCurrentIndex(
        ui.FloppyHeadsComboBox->findText(IntToStr(FloppyHeads[ui.FloppyImageTypeComboBox->currentIndex()])));
    ui.FloppySPTComboBox->setCurrentIndex(
        ui.FloppySPTComboBox->findText(IntToStr(FloppySPT[ui.FloppyImageTypeComboBox->currentIndex()])));
    try {
        CalcFloppySize();
    } catch (...) {
        JustChanging = false;
        throw;
    }
    JustChanging = false;
}

void TCreateImageForm::FloppyDataChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (JustChanging) return;
    JustChanging = true;
    bool Found = false;
    try {
        for (int I = 0; I <= 7; I++) if ((StrToInt(ui.FloppyCylindersComboBox->currentText()) == FloppyCylinders[I]) && (StrToInt(ui.FloppyHeadsComboBox->currentText()) == FloppyHeads[I]) && (StrToInt(ui.FloppySPTComboBox->currentText()) == FloppySPT[I])) {
            ui.FloppyImageTypeComboBox->setCurrentIndex(I);
            Found = true;
            break;
        }
        if (!Found) ui.FloppyImageTypeComboBox->setCurrentIndex(8);
    } catch (...) {
        JustChanging = false;
        CalcFloppySize();
        throw;
    }
    JustChanging = false;
    CalcFloppySize();
}

void TCreateImageForm::CalcFloppySize()
{
    FloppyImageSize = 512 * StrToInt(ui.FloppyCylindersComboBox->currentText()) * StrToInt(ui.FloppyHeadsComboBox->currentText()) * StrToInt(ui.FloppySPTComboBox->currentText());
    ui.FloppyImageSizeEdit->setText(Format("%s Bytes = %s KB", {IntToStr(FloppyImageSize), IntToStr(FloppyImageSize / 1024)}));
}

void TCreateImageForm::ImageFileButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (ui.ImageFileEdit->text() == "")
        OpenDialog->InitialDir = PrgSetup->BaseDir();
    else
        OpenDialog->InitialDir = ExtractFilePath(ui.ImageFileEdit->text());
    if (!OpenDialog->Execute()) return;
    ui.ImageFileEdit->setText(OpenDialog->FileName);
}

void TCreateImageForm::HDSizeEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.HDGeometryEdit->setText("");
    try {
        ui.HDGeometryEdit->setText(GetGemoetryFromMB(StrToInt(ui.HDSizeEdit->text())));
    } catch (...) {
        return;
    }
}

void TCreateImageForm::MakeBootableCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.MakeBootableCheckBox->setEnabled(ui.FormatImageCheckBox->isChecked());
    ui.AddKeyboardDriverCheckBox->setEnabled(ui.MakeBootableCheckBox->isEnabled() && ui.MakeBootableCheckBox->isChecked());
    ui.AddMouseDriverCheckBox->setEnabled(ui.MakeBootableCheckBox->isEnabled() && ui.MakeBootableCheckBox->isChecked());
    ui.MemoryManagerCheckBox->setEnabled(ui.MakeBootableCheckBox->isEnabled() && ui.MakeBootableCheckBox->isChecked());
    ui.AddFormatCheckBox->setEnabled(ui.FormatImageCheckBox->isChecked());
    ui.AddEditCheckbox->setEnabled(ui.FormatImageCheckBox->isChecked());
    ui.AddDoszipCheckBox->setEnabled(ui.FormatImageCheckBox->isChecked() && DosZipAvailable());
    ui.CreateProfileCheckBox->setEnabled(ui.MakeBootableCheckBox->isEnabled() && ui.MakeBootableCheckBox->isChecked());
}

void TCreateImageForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I = 0, NeededSize, AvailableSize = 0;
    TDiskImageCreatorUpgrades Upgrades;
    QString S;
    // PageControl.ActivePageIndex=1 <=> HDImageSheet active
    const bool HDPage = (ui.PageControl->currentWidget() == ui.HDImageSheet);

    // Checks
    ImageFileName = ui.ImageFileEdit->text();
    if (Trim(ImageFileName) == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    if (ExtractFileExt(ImageFileName) == "") ImageFileName = ImageFileName + ".img";
    ImageFileName = MakeAbsPath(ImageFileName, PrgSetup->BaseDir());
    if (FileExists(ImageFileName)) {
        if (MessageDlg(Format(LanguageSetup->MessageConfirmationOverwriteFile(), {ImageFileName}), mtConfirmation, mbYes | mbNo, 0) != mrYes) {
            setModalResult(mrNone); return;
        }
        ExtDeleteFile(ImageFileName, ftProfile);
    }

    Upgrades.clear();
    if (ui.FormatImageCheckBox->isChecked()) {
        if (ui.MakeBootableCheckBox->isChecked()) Upgrades.insert(dicuMakeBootable);
        if (ui.AddKeyboardDriverCheckBox->isEnabled() && ui.AddKeyboardDriverCheckBox->isChecked()) Upgrades.insert(dicuKeyboardDriver);
        if (ui.AddMouseDriverCheckBox->isEnabled() && ui.AddMouseDriverCheckBox->isChecked()) Upgrades.insert(dicuMouseDriver);
        if (ui.MemoryManagerCheckBox->isEnabled() && ui.MemoryManagerCheckBox->isChecked()) Upgrades.insert(dicuMemoryManager);
        if (ui.AddFormatCheckBox->isEnabled() && ui.AddFormatCheckBox->isChecked()) Upgrades.insert(dicuDiskTools);
        if (ui.AddEditCheckbox->isEnabled() && ui.AddEditCheckbox->isChecked()) Upgrades.insert(dicuEditor);
        if (ui.AddDoszipCheckBox->isEnabled() && ui.AddDoszipCheckBox->isChecked()) Upgrades.insert(dicuDoszip);
        if (FD11ToolsAvailable()) Upgrades.insert(dicuUseFD11Tools);
        NeededSize = DiskImageCreatorUpgradeSize(HDPage, Upgrades);
        if (!HDPage) {
            AvailableSize = FloppyImageSize;
        } else {
            try {
                AvailableSize = StrToInt(ui.HDSizeEdit->text()) * 1024 * 1024;
            } catch (...) {
                MessageDlg(LanguageSetup->MessageWrongDiskSize(), mtError, mbOK, 0);
                setModalResult(mrNone); return;
            }
        }
        if (NeededSize > AvailableSize) {
            MessageDlg(Format(LanguageSetup->ImageFromFolderNotEnoughSpace(), {NeededSize / 1024, AvailableSize / 1024}), mtError, mbOK, 0);
            setModalResult(mrNone); return;
        }
    }

    // Create mkdosfs or plain image
    if (!HDPage) {
        if (!DiskImageCreator(
                StrToInt(ui.FloppyCylindersComboBox->currentText()),
                StrToInt(ui.FloppyHeadsComboBox->currentText()),
                StrToInt(ui.FloppySPTComboBox->currentText()),
                ImageFileName,
                ui.CompressedCheckBox->isChecked(),
                ui.FormatImageCheckBox->isChecked()
            )) { setModalResult(mrNone); return; }
    } else {
        try {
            I = StrToInt(ui.HDSizeEdit->text());
        } catch (...) {
            MessageDlg(LanguageSetup->MessageWrongDiskSize(), mtError, mbOK, 0);
            setModalResult(mrNone); return;
        }
        if (!DiskImageCreator(I, ImageFileName, ui.CompressedCheckBox->isChecked(), ui.FormatImageCheckBox->isChecked())) { setModalResult(mrNone); return; }
    }

    // Make bootable, add files etc.
    if (!Upgrades.isEmpty()) {
        DiskImageCreatorUpgrade(
            ImageFileName,
            HDPage,
            Upgrades,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            StrToInt(ui.FloppyCylindersComboBox->currentText()),
            StrToInt(ui.FloppyHeadsComboBox->currentText()),
            StrToInt(ui.FloppySPTComboBox->currentText())
        );
    }

    // Create profile for image
    if (ui.CreateProfileCheckBox->isEnabled() && ui.CreateProfileCheckBox->isChecked()) {
        S = Format(LanguageSetup->CreateImageFormCreateProfileName(), {ExtractFileName(ImageFileName)});
        if (InputQuery(LanguageSetup->CreateImageFormCreateProfileNameCaption(), LanguageSetup->CreateImageFormCreateProfileNamePrompt(), S, this)) CreateProfile(S, ImageFileName, !HDPage);
    }
}

void TCreateImageForm::CreateProfile(const QString& Name, const QString& ImageFile, bool FloppyDiskImage)
{
    int I;
    TGame* NewProfile;
    QString S;

    // Setup new profile
    I = GameDB->Add(Name);
    NewProfile = GameDB->Game(I);
    if (FloppyDiskImage) {
        NewProfile->SetNrOfMounts(0);
        NewProfile->SetAutoexecBootImage(MakeRelPath(ImageFile, PrgSetup->BaseDir()));
    } else {
        NewProfile->SetNrOfMounts(1);
        NewProfile->SetMount0(MakeRelPath(ImageFile, PrgSetup->BaseDir()) + ";IMAGE;2;;;" + GetGeometryFromFile(ImageFile));
        NewProfile->SetAutoexecBootImage("2");
    }

    S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(NewProfile->Name()) + PathDelim;
    I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(NewProfile->Name()) + IntToStr(I) + PathDelim;
    }
    NewProfile->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));

    NewProfile->StoreAllValues();
    NewProfile->LoadCache();
    NewProfileFromImageCreator = NewProfile;
}

void TCreateImageForm::PageControlChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.WriteToFloppyCheckBox->setEnabled((ui.PageControl->currentWidget() == ui.FloppyImageSheet) && FloppyDriveAvailable);
}

void TCreateImageForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasImagesCreateImage);
}

void TCreateImageForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

QString ShowCreateImageFileDialog(QWidget* AOwner, bool ShowFloppySheet, bool ShowHDSheet, TGameDB* AGameDB)
{
    QString result;
    NewProfileFromImageCreator = nullptr;
    CreateImageForm = new TCreateImageForm(AOwner);
    VclLayout::setTabVisible(CreateImageForm->ui.PageControl, CreateImageForm->ui.FloppyImageSheet, ShowFloppySheet);
    VclLayout::setTabVisible(CreateImageForm->ui.PageControl, CreateImageForm->ui.HDImageSheet, ShowHDSheet);
    CreateImageForm->GameDB = AGameDB;
    if (CreateImageForm->ShowModal() == mrOk) result = CreateImageForm->ImageFileName; else result = "";
    if ((result != "") && CreateImageForm->ui.WriteToFloppyCheckBox->isChecked() && CreateImageForm->ui.WriteToFloppyCheckBox->isEnabled()) {
        ShowWriteIMGImageDialog(AOwner, CreateImageForm->ui.ImageFileEdit->text());
    }
    delete CreateImageForm;
    CreateImageForm = nullptr;
    return result;
}
