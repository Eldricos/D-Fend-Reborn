// C++ port of ProfileMountEditorFloppyImage1FrameUnit.pas (implementation section).
#include "ProfileMountEditorFloppyImage1FrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "CreateISOImageFormUnit.h"
#include "CreateImageUnit.h"
#include "IconLoaderUnit.h"

TProfileMountEditorFloppyImage1Frame::TProfileMountEditorFloppyImage1Frame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // OpenDialog: TOpenDialog (DefaultExt = 'iso')
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "iso";

    // DFM event wiring
    connect(ui.FloppyImageButton, &QToolButton::clicked, this, [this]() { FloppyImageButtonClick(ui.FloppyImageButton); });
    connect(ui.FloppyImageCreateButton, &QToolButton::clicked, this, [this]() { FloppyImageCreateButtonClick(ui.FloppyImageCreateButton); });
    connect(ui.FloppyImageDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { FloppyImageDriveLetterComboBoxChange(ui.FloppyImageDriveLetterComboBox); });
    connect(ui.FloppyPopupCreateImage, &QAction::triggered, this, [this]() { FloppyPopupWork(ui.FloppyPopupCreateImage); });
    connect(ui.FloppyPopupReadImage, &QAction::triggered, this, [this]() { FloppyPopupWork(ui.FloppyPopupReadImage); });
}

TProfileMountEditorFloppyImage1Frame::~TProfileMountEditorFloppyImage1Frame()
{
    delete OpenDialog;
}

// TProfileMountEditorFloppyImage1Frame

bool TProfileMountEditorFloppyImage1Frame::Init(const TInfoData& AInfoData)
{
    TStringList* St;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;

    ui.FloppyImageEdit_EditLabel->setText(LanguageSetup->ProfileMountingFile());
    ui.FloppyImageDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.FloppyImageDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.DriveLetterInfoLabel1->setText(LanguageSetup->ProfileMountingLetterInfo());
    ui.FloppyImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.FloppyImageCreateButton->setToolTip(LanguageSetup->ProfileMountingCreateImage());

    for (char C = 'A'; C <= 'Y'; C++) ui.FloppyImageDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.FloppyImageDriveLetterComboBox->addItem("0");
    ui.FloppyImageDriveLetterComboBox->addItem("1");
    ui.FloppyImageDriveLetterComboBox->setCurrentIndex(0);

    ui.FloppyPopupCreateImage->setText(LanguageSetup->ProfileMountingFloppyImageCreate());
    ui.FloppyPopupReadImage->setText(LanguageSetup->ProfileMountingFloppyImageRead());

    UserIconLoader->DialogImage(DI_SelectFile, ui.FloppyImageButton);
    UserIconLoader->DialogImage(DI_ImageFloppy, ui.FloppyImageCreateButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "FLOPPYIMAGE") {
        // ImageFile;FLOPPYIMAGE;Letter;;;
        result = true;
        ui.FloppyImageEdit->setText((*St)[0]);
        if (St->Count() >= 3) {
            I = ui.FloppyImageDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.FloppyImageDriveLetterComboBox->setCurrentIndex(I);
        }
    } else {
        result = false;
        for (I = 0; I <= ui.FloppyImageDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.FloppyImageDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.FloppyImageDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;

    FloppyImageDriveLetterComboBoxChange(this);
    return result;
}

void TProfileMountEditorFloppyImage1Frame::ShowFrame()
{
}

bool TProfileMountEditorFloppyImage1Frame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorFloppyImage1Frame::Done()
{
    // ImageFile;FLOPPYIMAGE;Letter;;;
    return StringReplace(MakeRelPath(ui.FloppyImageEdit->text(), PrgSetup->BaseDir()), ";", "<semicolon>", rfReplaceAll) + ";FloppyImage;" + ui.FloppyImageDriveLetterComboBox->currentText() + ";;;";
}

QString TProfileMountEditorFloppyImage1Frame::GetName()
{
    return LanguageSetup->ProfileMountingFloppyImageSheet();
}

void TProfileMountEditorFloppyImage1Frame::FloppyImageButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (Trim(ui.FloppyImageEdit->text()) == "") S = PrgSetup->BaseDir(); else S = ui.FloppyImageEdit->text();
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    OpenDialog->DefaultExt = "img";
    OpenDialog->InitialDir = ExtractFilePath(S);
    OpenDialog->Title = LanguageSetup->ProfileMountingFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
    if (!OpenDialog->Execute()) return;
    ui.FloppyImageEdit->setText(MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
}

void TProfileMountEditorFloppyImage1Frame::FloppyImageCreateButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QPoint P;

    P = ui.FloppyImageCreateButton->mapToGlobal(QPoint(0, 0));  // ClientToScreen(Point(Left,Top))
    ui.FloppyPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TProfileMountEditorFloppyImage1Frame::FloppyImageDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FloppyImageDriveLetterWarningLabel->setVisible((ui.FloppyImageDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.FloppyImageDriveLetterComboBox->itemText(ui.FloppyImageDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}

void TProfileMountEditorFloppyImage1Frame::FloppyPopupWork(QObject* Sender)
{
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            S = ShowCreateImageFileDialog(this, true, false, InfoData.GameDB);
            if (S == "") return;
            ui.FloppyImageEdit->setText(MakeRelPath(S, PrgSetup->BaseDir()));
            break;
        case 1:
            if (!ShowCreateISOImageDialog(this, S, true)) return;
            if (S == "") return;
            ui.FloppyImageEdit->setText(MakeRelPath(S, PrgSetup->BaseDir()));
            break;
    }
}
