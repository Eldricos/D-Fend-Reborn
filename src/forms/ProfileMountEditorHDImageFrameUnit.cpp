// C++ port of ProfileMountEditorHDImageFrameUnit.pas (implementation section).
#include "ProfileMountEditorHDImageFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "ImageTools.h"
#include "CreateImageUnit.h"
#include "IconLoaderUnit.h"

TProfileMountEditorHDImageFrame::TProfileMountEditorHDImageFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // OpenDialog: TOpenDialog (DefaultExt = 'iso')
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "iso";

    // DFM event wiring
    connect(ui.ImageButton, &QToolButton::clicked, this, [this]() { ImageButtonClick(ui.ImageButton); });
    connect(ui.ImageCreateButton, &QToolButton::clicked, this, [this]() { ImageCreateButtonClick(ui.ImageCreateButton); });
    connect(ui.ImageDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ImageDriveLetterComboBoxChange(ui.ImageDriveLetterComboBox); });
}

TProfileMountEditorHDImageFrame::~TProfileMountEditorHDImageFrame()
{
    delete OpenDialog;
}

// TProfileMountEditorHDImageFrame

bool TProfileMountEditorHDImageFrame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;

    ui.ImageEdit_EditLabel->setText(LanguageSetup->ProfileMountingFile());
    ui.ImageGeometryEdit_EditLabel->setText(LanguageSetup->ProfileMountingGeometry());
    ui.ImageDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.HDImageDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.DriveLetterInfoLabel2->setText(LanguageSetup->ProfileMountingLetterInfo());
    ui.ImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ImageCreateButton->setToolTip(LanguageSetup->ProfileMountingCreateImage());

    for (char C = 'A'; C <= 'Y'; C++) ui.ImageDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.ImageDriveLetterComboBox->addItem("2");
    ui.ImageDriveLetterComboBox->addItem("3");
    ui.ImageDriveLetterComboBox->setCurrentIndex(3);

    UserIconLoader->DialogImage(DI_SelectFile, ui.ImageButton);
    UserIconLoader->DialogImage(DI_ImageHD, ui.ImageCreateButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "IMAGE") {
        // ImageFile;IMAGE;LetterOR23;;;geometry
        result = true;
        ui.ImageEdit->setText((*St)[0]);
        if (St->Count() >= 3) {
            I = ui.ImageDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.ImageDriveLetterComboBox->setCurrentIndex(I);
        }
        if (St->Count() >= 6) {
            ui.ImageGeometryEdit->setText((*St)[5]);
        }
    } else {
        result = false;
        for (I = 2; I <= ui.ImageDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.ImageDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.ImageDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    ImageDriveLetterComboBoxChange(this);
    return result;
}

void TProfileMountEditorHDImageFrame::ShowFrame()
{
}

bool TProfileMountEditorHDImageFrame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorHDImageFrame::Done()
{
    // ImageFile;IMAGE;LetterOR23;;;geometry
    return StringReplace(MakeRelPath(ui.ImageEdit->text(), PrgSetup->BaseDir()), ";", "<semicolon>", rfReplaceAll) + ";Image;" + ui.ImageDriveLetterComboBox->currentText() + ";;;" + ui.ImageGeometryEdit->text();
}

QString TProfileMountEditorHDImageFrame::GetName()
{
    return LanguageSetup->ProfileMountingImageSheet();
}

void TProfileMountEditorHDImageFrame::ImageButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.ImageEdit->text()) == "") S = PrgSetup->BaseDir(); else S = ui.ImageEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    OpenDialog->DefaultExt = "img";
    OpenDialog->InitialDir = ExtractFilePath(S);
    OpenDialog->Title = LanguageSetup->ProfileMountingFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
    if (!OpenDialog->Execute()) return;
    ui.ImageEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
    ui.ImageGeometryEdit->setText(GetGeometryFromFile(OpenDialog->FileName));
}

void TProfileMountEditorHDImageFrame::ImageCreateButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    S = ShowCreateImageFileDialog(this, false, true, InfoData.GameDB);
    if (S == "") return;
    ui.ImageEdit->setText(MakeRelPath(S, PrgSetup->BaseDir()));
    ui.ImageGeometryEdit->setText(GetGeometryFromFile(S));
}

void TProfileMountEditorHDImageFrame::ImageDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.HDImageDriveLetterWarningLabel->setVisible((ui.ImageDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.ImageDriveLetterComboBox->itemText(ui.ImageDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}
