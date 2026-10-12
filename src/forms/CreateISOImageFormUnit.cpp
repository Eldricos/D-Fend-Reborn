// C++ port of CreateISOImageFormUnit.pas (implementation section).
#include "CreateISOImageFormUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "DriveReadFormUnit.h"
#include "ReadDriveUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "WinApi.h"

TCreateISOImageForm* CreateISOImageForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, ID)
static void HelpCommandContext(int ID)
{
    // TODO(port): no public help context entry point in HelpTools yet; open the help contents.
    Q_UNUSED(ID);
    if (GLOBAL_HELPROUTER != nullptr) GLOBAL_HELPROUTER->HelpContent();
}

// GetDriveType(PChar(C+':\'))
enum { DRIVE_TYPE_OTHER = 0, DRIVE_TYPE_REMOVABLE = 2, DRIVE_TYPE_CDROM = 5 };
static quint32 GetDriveTypeOf(QChar C)
{
#ifdef Q_OS_WIN
    const QString S = QString(C) + ":\\";
    const UINT J = GetDriveTypeW(reinterpret_cast<LPCWSTR>(S.utf16()));
    if (J == DRIVE_REMOVABLE) return DRIVE_TYPE_REMOVABLE;
    if (J == DRIVE_CDROM) return DRIVE_TYPE_CDROM;
    return DRIVE_TYPE_OTHER;
#else
    // TODO(port): drive letters (raw floppy / CD access) only exist on Windows.
    Q_UNUSED(C);
    return DRIVE_TYPE_OTHER;
#endif
}

TCreateISOImageForm::TCreateISOImageForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    SaveDialog = new TSaveDialog(this);
    SaveDialog->DefaultExt = "iso";
    SaveDialog->Title = "ISO-Image speichern";

    connect(ui.FileNameButton, &QToolButton::clicked, this, [this]() { FileNameButtonClick(ui.FileNameButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TCreateISOImageForm::~TCreateISOImageForm()
{
    delete SaveDialog;
}

void TCreateISOImageForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);
}

void TCreateISOImageForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.FileNameEdit_EditLabel->setText(LanguageSetup->ReadImageFileName());
    ui.FileNameButton->setToolTip(LanguageSetup->ChooseFile());
    ui.InfoLabel->setText(LanguageSetup->ReadImageInfo());
    UserIconLoader->DialogImage(DI_SelectFile, ui.FileNameButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);

    FloppyMode = false;
    WriteMode = false;
}

void TCreateISOImageForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    quint32 J;

    if (FloppyMode) {
        if (WriteMode) {
            setWindowTitle(LanguageSetup->WriteImageCaptionIMG());
            SaveDialog->Title = LanguageSetup->WriteImageFileNameTitleIMG();
        } else {
            setWindowTitle(LanguageSetup->ReadImageCaptionIMG());
            SaveDialog->Title = LanguageSetup->ReadImageFileNameTitleIMG();
        }
        ui.DriveLabel->setText(LanguageSetup->ReadImageDriveLabelFloppy());
        SaveDialog->Filter = LanguageSetup->ReadImageFileNameFilterIMG();
        SaveDialog->DefaultExt = "img";

        ui.InfoLabel->setVisible(false);
        const int T = ui.InfoLabel->y() + 8;
        VclLayout::setBounds(ui.OKButton, ui.OKButton->x(), T, ui.OKButton->width(), ui.OKButton->height());
        VclLayout::setBounds(ui.CancelButton, ui.CancelButton->x(), T, ui.CancelButton->width(), ui.CancelButton->height());
        VclLayout::setBounds(ui.HelpButton, ui.HelpButton->x(), T, ui.HelpButton->width(), ui.HelpButton->height());
        VclLayout::setClientSize(this, width(), T + ui.OKButton->height() + 8);
    } else {
        setWindowTitle(LanguageSetup->ReadImageCaptionISO());
        ui.DriveLabel->setText(LanguageSetup->ReadImageDriveLabelCD());
        SaveDialog->Filter = LanguageSetup->ReadImageFileNameFilterISO();
        SaveDialog->Title = LanguageSetup->ReadImageFileNameTitleISO();
        SaveDialog->DefaultExt = "iso";
    }

    for (char C = 'A'; C <= 'Z'; C++) {
        J = GetDriveTypeOf(QChar(C));
        if (FloppyMode) {
            if (J == DRIVE_TYPE_REMOVABLE) ui.DriveComboBox->addItem(QString(QChar(C)));
        } else {
            if (J == DRIVE_TYPE_CDROM) ui.DriveComboBox->addItem(QString(QChar(C)));
        }
    }

    if (ui.DriveComboBox->count() > 0) ui.DriveComboBox->setCurrentIndex(0);
}

void TCreateISOImageForm::FileNameButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = Trim(ui.FileNameEdit->text());
    if (S == "") S = PrgSetup->BaseDir(); else S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
    SaveDialog->InitialDir = S;
    if (SaveDialog->Execute()) ui.FileNameEdit->setText(SaveDialog->FileName);
}

void TCreateISOImageForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    if (ui.DriveComboBox->currentIndex() < 0) {
        MessageDlg(LanguageSetup->MessageNoDriveSelected(), mtError, mbOK, 0);
        setModalResult(mrNone);
        return;
    }

    if (Trim(ui.FileNameEdit->text()) == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        setModalResult(mrNone);
        return;
    }

    FileName = MakeAbsPath(ui.FileNameEdit->text(), PrgSetup->BaseDir());
    S = ui.DriveComboBox->itemText(ui.DriveComboBox->currentIndex());
    Drive = S[0];
}

void TCreateISOImageForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (FloppyMode) {
        if (WriteMode)
            HelpCommandContext(ID_ExtrasImagesWriteImageToFloppy);
        else
            HelpCommandContext(ID_ExtrasImagesCreateImageFromFloppy);
    } else {
        HelpCommandContext(ID_ExtrasImagesCreateImageFromCD);
    }
}

void TCreateISOImageForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

static bool AnyDriveOfType(quint32 T)
{
    for (char C = 'A'; C <= 'Z'; C++) if (GetDriveTypeOf(QChar(C)) == T) return true;
    return false;
}

bool ShowCreateISOImageDialog(QWidget* AOwner, QString& AFileName, bool AFloppyMode)
{
    TReadDataResult ReadResult;
    QString S;
    QChar ADrive;
    bool B;
    bool result = false;

    if (AFloppyMode) {
        B = AnyDriveOfType(DRIVE_TYPE_REMOVABLE);
    } else {
        B = AnyDriveOfType(DRIVE_TYPE_CDROM);
    }
    if (!B) {
        if (AFloppyMode) S = LanguageSetup->MessageNoFloppyDrive(); else S = LanguageSetup->MessageNoCDDrive();
        MessageDlg(S, mtError, mbOK, 0);
        return result;
    }

    CreateISOImageForm = new TCreateISOImageForm(AOwner);
    CreateISOImageForm->FloppyMode = AFloppyMode;
    result = (CreateISOImageForm->ShowModal() == mrOk);
    if (result) {
        AFileName = CreateISOImageForm->FileName;
        ADrive = CreateISOImageForm->Drive;
        ReadResult = ShowDriveReadDialog(AOwner, ADrive, AFileName);
        result = (ReadResult == RD_OK);
        bool Show = true;
        switch (ReadResult) {
            case RD_CannotOpen: S = Format(LanguageSetup->MessageCouldNotDriveOpen(), {QString(ADrive)}); break;
            case RD_CannotGetDriveData: S = Format(LanguageSetup->MessageCouldNotDriveGetData(), {QString(ADrive)}); break;
            case RD_CannotSetExtendedAccess: S = Format(LanguageSetup->MessageCouldNotDriveSetExtAccess(), {QString(ADrive)}); break;
            case RD_CannotOpenOutputFile: S = Format(LanguageSetup->MessageCouldNotSaveFile(), {AFileName}); break;
            case RD_ReadError: S = Format(LanguageSetup->MessageCouldNotDriveReadError(), {QString(ADrive)}); break;
            default: Show = false; break;
        }
        if (Show) MessageDlg(S, mtError, mbOK, 0);
    }
    delete CreateISOImageForm;
    CreateISOImageForm = nullptr;
    return result;
}

bool ShowCreateISOImageDialog(QWidget* AOwner, bool AFloppyMode)
{
    QString AFileName;
    return ShowCreateISOImageDialog(AOwner, AFileName, AFloppyMode);
}

bool ShowWriteIMGImageDialog(QWidget* AOwner, const QString& ImageFileName)
{
    TReadDataResult ReadResult;
    QString S, AFileName;
    QChar ADrive;
    bool result = false;

    if (!AnyDriveOfType(DRIVE_TYPE_REMOVABLE)) {
        MessageDlg(LanguageSetup->MessageNoFloppyDrive(), mtError, mbOK, 0);
        return result;
    }

    CreateISOImageForm = new TCreateISOImageForm(AOwner);
    CreateISOImageForm->FloppyMode = true;
    CreateISOImageForm->WriteMode = true;
    CreateISOImageForm->ui.FileNameEdit->setText(ImageFileName);
    result = (CreateISOImageForm->ShowModal() == mrOk);
    if (result) {
        AFileName = CreateISOImageForm->FileName;
        ADrive = CreateISOImageForm->Drive;
        ReadResult = ShowDriveWriteDialog(AOwner, ADrive, AFileName);
        result = (ReadResult == RD_OK);
        bool Show = true;
        switch (ReadResult) {
            case RD_CannotOpen: S = Format(LanguageSetup->MessageCouldNotDriveOpen(), {QString(ADrive)}); break;
            case RD_CannotGetDriveData: S = Format(LanguageSetup->MessageCouldNotDriveGetData(), {QString(ADrive)}); break;
            case RD_CannotSetExtendedAccess: S = Format(LanguageSetup->MessageCouldNotDriveSetExtAccess(), {QString(ADrive)}); break;
            case RD_CannotOpenInputFile: S = Format(LanguageSetup->MessageCouldNotOpenFile(), {AFileName}); break;
            case RD_WriteError: S = Format(LanguageSetup->MessageCouldNotDriveWriteError(), {QString(ADrive)}); break;
            default: Show = false; break;
        }
        if (Show) MessageDlg(S, mtError, mbOK, 0);
    }
    delete CreateISOImageForm;
    CreateISOImageForm = nullptr;
    return result;
}
