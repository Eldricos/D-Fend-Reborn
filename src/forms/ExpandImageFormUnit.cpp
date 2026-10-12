// C++ port of ExpandImageFormUnit.pas (implementation section).
#include "ExpandImageFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "GameDBUnit.h"
#include "DosBoxUnit.h"
#include "ImageTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "DOSBoxTempUnit.h"
#include "PrgConsts.h"
#include "MainUnit.h"
#include "Process.h"

TExpandImageForm* ExpandImageForm = nullptr;

TExpandImageForm::TExpandImageForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "img";

    connect(ui.FileNameButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FileNameButton); });
    connect(ui.FolderButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.FolderButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TExpandImageForm::~TExpandImageForm()
{
    delete OpenDialog;
}

void TExpandImageForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt

    setWindowTitle(LanguageSetup->ExtractImageCaption());
    ui.FileNameEdit_EditLabel->setText(LanguageSetup->ExtractImageImageFile());
    ui.FileNameButton->setToolTip(LanguageSetup->ChooseFile());
    ui.FolderEdit_EditLabel->setText(LanguageSetup->ExtractImageDestinationFolder());
    ui.FolderButton->setToolTip(LanguageSetup->ChooseFolder());
    ui.OpenFolderCheckBox->setText(LanguageSetup->ExtractImageOpenDestinationFolder());

    ui.ImageTypeRadioGroup->setTitle(LanguageSetup->ExtractImageImageType());
    ui.ImageTypeRadioGroup->setItemText(0, LanguageSetup->ExtractImageImageTypeFloppy());
    ui.ImageTypeRadioGroup->setItemText(1, LanguageSetup->ExtractImageImageTypeHD());
    ui.ImageTypeRadioGroup->setItemText(2, LanguageSetup->ExtractImageImageTypeISO());
    ui.ImageTypeRadioGroup->setItemIndex(0);
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    OpenDialog->Title = LanguageSetup->ExtractImageOpenFileTitle();
    OpenDialog->Filter = LanguageSetup->ExtractImageOpenFileFilter();

    UserIconLoader->DialogImage(DI_SelectFile, ui.FileNameButton);
    UserIconLoader->DialogImage(DI_SelectFolder, ui.FolderButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TExpandImageForm::ButtonWork(QObject* Sender)
{
    QString S;
    switch (Sender->property("tag").toInt()) {
        case 0:
            S = Trim(ui.FileNameEdit->text());
            if (S == "") S = PrgSetup->BaseDir(); else S = ExtractFilePath(MakeAbsPath(S, PrgSetup->BaseDir()));
            OpenDialog->InitialDir = S;
            if (OpenDialog->Execute()) {
                ui.FileNameEdit->setText(OpenDialog->FileName);
                S = Trim(ExtUpperCase(ExtractFileExt(ui.FileNameEdit->text())));
                if ((S == ".ISO") || (S == ".CUE") || (S == ".BIN")) ui.ImageTypeRadioGroup->setItemIndex(2); else {
                    if ((GetFileSize(MakeAbsPath(ui.FileNameEdit->text(), PrgSetup->BaseDir()))) >= 5 * 1024 * 1024)
                        ui.ImageTypeRadioGroup->setItemIndex(1);
                    else
                        ui.ImageTypeRadioGroup->setItemIndex(0);
                }
            }
            break;
        case 1:
            S = Trim(ui.FolderEdit->text());
            if (S == "") S = PrgSetup->BaseDir(); else S = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(S), PrgSetup->BaseDir()));
            if (SelectDirectory(LanguageSetup->ExtractImageFolderTitle(), QString(), S, this)) ui.FolderEdit->setText(S);
            break;
    }
}

void TExpandImageForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString FileName, Dir;

    FileName = Trim(ui.FileNameEdit->text());
    if (FileName == "") {
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    FileName = MakeAbsPath(FileName, PrgSetup->BaseDir());
    if (!FileExists(FileName)) {
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {FileName}), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    if ((ui.ImageTypeRadioGroup->itemIndex() == 2) && (!CheckCDImage(FileName))) {
        if (MessageDlg(Format(LanguageSetup->ProfileMountingImageTypeWarning(), {FileName}), mtWarning, mbYes | mbNo, 0) != mrYes) {
            setModalResult(mrNone); return;
        }
    }

    Dir = Trim(ui.FolderEdit->text());
    if (Dir == "") {
        MessageDlg(LanguageSetup->MessageNoFolderName(), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }
    Dir = IncludeTrailingPathDelimiter(MakeAbsPath(IncludeTrailingPathDelimiter(Dir), PrgSetup->BaseDir()));
    if (!ForceDirectories(Dir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {FileName}), mtError, mbOK, 0);
        setModalResult(mrNone); return;
    }

    if (!ExpandImage(FileName, Dir, ui.ImageTypeRadioGroup->itemIndex())) {
        setModalResult(mrNone); return;
    }

    if (ui.OpenFolderCheckBox->isChecked()) OpenFolder(Dir);
}

bool TExpandImageForm::ExpandImage(const QString& FileName, const QString& Dir, int ImageType)
{
    TTempGame* TempGame;
    QString FreeDOS;
    bool result = false;

    FreeDOS = Trim(PrgSetup->PathToFREEDOS());
    if (FreeDOS != "") FreeDOS = IncludeTrailingPathDelimiter(MakeAbsPath(FreeDOS, PrgSetup->BaseDir()));
    if ((FreeDOS == "") || (!DirectoryExists(FreeDOS))) {
        MessageDlg(LanguageSetup->ProfileEditorNeedFreeDOS(), mtError, mbOK, 0);
        return result;
    }

    TempGame = new TTempGame();
    TempGame->Game()->SetNrOfMounts(3);
    TempGame->Game()->SetMount0(FreeDOS + ";DRIVE;C;False;;" + IntToStr(DefaultFreeHDSize));
    switch (ImageType) {
        case 0: TempGame->Game()->SetMount1(FileName + ";FLOPPYIMAGE;A;;;"); break;
        case 1: TempGame->Game()->SetMount1(FileName + ";IMAGE;A;;;512,63,16," + GetGeometryFromFile(FileName)); break;
        case 2: TempGame->Game()->SetMount1(FileName + ";CDROMIMAGE;A;;;"); break;
    }
    TempGame->Game()->SetMount2(Dir + ";DRIVE;D;False;;1000");

    {
        TStringList St;
        St.Add("C:\\4DOS.COM /C COPY A:\\*.* D:\\ /S");
        St.Add("exit");
        TempGame->Game()->SetAutoexec(StringListToString(&St));
    }

    TempGame->Game()->StoreAllValues();
    RunCommand(TempGame->Game(), DFendReloadedMainForm->DeleteOnExit, "", true);
    // The Delphi code only freed TempGame.Game (not TempGame itself); deleting
    // the TTempGame frees the game and removes the temporary profile file.
    delete TempGame;

    result = true;
    return result;
}

void TExpandImageForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ExtrasImagesExtractToFolder);
}

void TExpandImageForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowExpandImageDialog(QWidget* AOwner)
{
    bool result;
    ExpandImageForm = new TExpandImageForm(AOwner);
    result = (ExpandImageForm->ShowModal() == mrOk);
    delete ExpandImageForm;
    ExpandImageForm = nullptr;
    return result;
}
