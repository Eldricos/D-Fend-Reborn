// C++ port of ProfileMountEditorCDImageFrameUnit.pas (implementation section).
#include "ProfileMountEditorCDImageFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "CreateISOImageFormUnit.h"
#include "IconLoaderUnit.h"
#include "ImageTools.h"

// TStringGrid helpers (FixedRows = FixedCols = 0): Cells[0,R]
static QString GridCell(QTableWidget *G, int R)
{
    QTableWidgetItem *I = G->item(R, 0);
    return I ? I->text() : QString();
}

static void SetGridCell(QTableWidget *G, int R, const QString &S)
{
    if (!G->item(R, 0)) G->setItem(R, 0, new QTableWidgetItem());
    G->item(R, 0)->setText(S);
}

TProfileMountEditorCDImageFrame::TProfileMountEditorCDImageFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // OpenDialog: TOpenDialog (DefaultExt = 'iso')
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "iso";

    // TStringGrid always has a current cell
    ui.CDROMImageTab->setCurrentCell(0, 0);

    // DFM event wiring
    connect(ui.CDROMImageButton, &QToolButton::clicked, this, [this]() { CDROMImageButtonClick(ui.CDROMImageButton); });
    connect(ui.CDROMImageAddButton, &QToolButton::clicked, this, [this]() { CDROMImageAddButtonClick(ui.CDROMImageAddButton); });
    connect(ui.CDROMImageDelButton, &QToolButton::clicked, this, [this]() { CDROMImageDelButtonClick(ui.CDROMImageDelButton); });
    connect(ui.ISOImageCreateButton, &QToolButton::clicked, this, [this]() { ISOImageCreateButtonClick(ui.ISOImageCreateButton); });
    connect(ui.CDROMImageDriveLetterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { CDROMImageDriveLetterComboBoxChange(ui.CDROMImageDriveLetterComboBox); });
}

TProfileMountEditorCDImageFrame::~TProfileMountEditorCDImageFrame()
{
    delete OpenDialog;
}

void TProfileMountEditorCDImageFrame::resizeEvent(QResizeEvent *E)
{
    QWidget::resizeEvent(E);
    FrameResize(this);   // OnResize
}

// TProfileMountEditorCDImageFrame

void TProfileMountEditorCDImageFrame::FrameResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CDROMImageTab->setColumnWidth(0, ui.CDROMImageTab->width() - 2 * ui.CDROMImageTab->frameWidth() - 25);
}

bool TProfileMountEditorCDImageFrame::Init(const TInfoData& AInfoData)
{
    TStringList *St, *St2;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;

    ui.CDROMImageLabel->setText(LanguageSetup->ProfileMountingFile());
    ui.CDROMImageAddButton->setToolTip(LanguageSetup->ProfileMountingAddImage());
    ui.CDROMImageDelButton->setToolTip(LanguageSetup->ProfileMountingDelImage());
    ui.CDROMImageSwitchLabel->setText(LanguageSetup->ProfileMountingSwitchImage());
    ui.CDROMImageDriveLetterLabel->setText(LanguageSetup->ProfileMountingLetter());
    ui.CDImageDriveLetterWarningLabel->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.CDROMImageButton->setToolTip(LanguageSetup->ChooseFile());
    ui.ISOImageCreateButton->setToolTip(LanguageSetup->ProfileMountingCreateImage());

    for (char C = 'A'; C <= 'Y'; C++) ui.CDROMImageDriveLetterComboBox->addItem(QString(QChar(C)));
    ui.CDROMImageDriveLetterComboBox->setCurrentIndex(3);
    ui.ImageTypeInfoLabel->setText(LanguageSetup->ProfileMountingImageTypeInfo());

    UserIconLoader->DialogImage(DI_SelectFile, ui.CDROMImageButton);
    UserIconLoader->DialogImage(DI_ImageCD, ui.ISOImageCreateButton);
    UserIconLoader->DialogImage(DI_Add, ui.CDROMImageAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.CDROMImageDelButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "CDROMIMAGE") {
        // ImageFile;CDROMIMAGE;Letter;;;
        result = true;
        St2 = new TStringList();
        S = Trim((*St)[0]);
        I = Pos('$', S);
        while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos('$', S); }
        St2->Add(S);
        ui.CDROMImageTab->setRowCount(St2->Count());
        for (I = 0; I <= St2->Count() - 1; I++) SetGridCell(ui.CDROMImageTab, I, (*St2)[I]);
        delete St2;
        if (St->Count() >= 3) {
            I = ui.CDROMImageDriveLetterComboBox->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.CDROMImageDriveLetterComboBox->setCurrentIndex(I);
        }
    } else {
        result = false;
        for (I = 2; I <= ui.CDROMImageDriveLetterComboBox->count() - 1; I++) {
            if (Pos(ui.CDROMImageDriveLetterComboBox->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.CDROMImageDriveLetterComboBox->setCurrentIndex(I); break; }
        }
    }
    delete St;
    if (ui.CDROMImageTab->currentRow() < 0) ui.CDROMImageTab->setCurrentCell(0, 0);

    CDROMImageDriveLetterComboBoxChange(this);
    return result;
}

QString TProfileMountEditorCDImageFrame::Done()
{
    int I;
    QString S;

    // ImageFile;CDROMIMAGE;Letter;;;
    S = MakeRelPath(GridCell(ui.CDROMImageTab, 0), PrgSetup->BaseDir()); for (I = 1; I <= ui.CDROMImageTab->rowCount() - 1; I++) S = S + "$" + MakeRelPath(GridCell(ui.CDROMImageTab, I), PrgSetup->BaseDir());
    return StringReplace(S, ";", "<semicolon>", rfReplaceAll) + ";CDROMImage;" + ui.CDROMImageDriveLetterComboBox->currentText() + ";;;";
}

bool TProfileMountEditorCDImageFrame::CheckBeforeDone()
{
    int I;
    QString S;

    for (I = 0; I <= ui.CDROMImageTab->rowCount() - 1; I++) {
        S = MakeAbsPath(GridCell(ui.CDROMImageTab, I), PrgSetup->BaseDir());
        if (!CheckCDImage(S)) {
            if (MessageDlg(Format(LanguageSetup->ProfileMountingImageTypeWarning(), {S}), mtWarning, mbYes | mbNo, 0) != mrYes) return false;
        }
    }
    return true;
}

QString TProfileMountEditorCDImageFrame::GetName()
{
    return LanguageSetup->ProfileMountingCDROMImageSheet();
}

void TProfileMountEditorCDImageFrame::CDROMImageButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (ui.CDROMImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    if (Trim(GridCell(ui.CDROMImageTab, ui.CDROMImageTab->currentRow())) == "") S = PrgSetup->BaseDir(); else S = GridCell(ui.CDROMImageTab, ui.CDROMImageTab->currentRow());
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    OpenDialog->DefaultExt = "iso";
    OpenDialog->InitialDir = ExtractFilePath(S);
    OpenDialog->Title = LanguageSetup->ProfileMountingFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
    if (!OpenDialog->Execute()) return;
    SetGridCell(ui.CDROMImageTab, ui.CDROMImageTab->currentRow(), MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
}

void TProfileMountEditorCDImageFrame::ISOImageCreateButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (ui.CDROMImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    if (!ShowCreateISOImageDialog(this, S, false)) return;
    if (S == "") return;
    SetGridCell(ui.CDROMImageTab, ui.CDROMImageTab->currentRow(), MakeRelPath(S, PrgSetup->BaseDir()));
}

void TProfileMountEditorCDImageFrame::ShowFrame()
{
}

void TProfileMountEditorCDImageFrame::CDROMImageAddButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CDROMImageTab->setRowCount(ui.CDROMImageTab->rowCount() + 1);
    ui.CDROMImageTab->setCurrentCell(ui.CDROMImageTab->rowCount() - 1, 0);
}

void TProfileMountEditorCDImageFrame::CDROMImageDelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (ui.CDROMImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    if (ui.CDROMImageTab->rowCount() == 1) {
        SetGridCell(ui.CDROMImageTab, 0, "");
    } else {
        for (I = ui.CDROMImageTab->currentRow() + 1; I <= ui.CDROMImageTab->rowCount() - 1; I++) SetGridCell(ui.CDROMImageTab, I - 1, GridCell(ui.CDROMImageTab, I));
        ui.CDROMImageTab->setRowCount(ui.CDROMImageTab->rowCount() - 1);
    }
}

void TProfileMountEditorCDImageFrame::CDROMImageDriveLetterComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.CDImageDriveLetterWarningLabel->setVisible((ui.CDROMImageDriveLetterComboBox->currentIndex() >= 0) && (Pos(ui.CDROMImageDriveLetterComboBox->itemText(ui.CDROMImageDriveLetterComboBox->currentIndex()), InfoData.UsedDriveLetters) > 0));
}
