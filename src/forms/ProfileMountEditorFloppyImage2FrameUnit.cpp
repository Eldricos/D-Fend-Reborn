// C++ port of ProfileMountEditorFloppyImage2FrameUnit.pas (implementation section).
#include "ProfileMountEditorFloppyImage2FrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "CreateISOImageFormUnit.h"
#include "CreateImageUnit.h"
#include "IconLoaderUnit.h"

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

TProfileMountEditorFloppyImage2Frame::TProfileMountEditorFloppyImage2Frame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // OpenDialog: TOpenDialog (DefaultExt = 'iso')
    OpenDialog = new TOpenDialog(this);
    OpenDialog->DefaultExt = "iso";

    // TStringGrid always has a current cell
    ui.FloppyImageTab->setCurrentCell(0, 0);

    // DFM event wiring
    connect(ui.FloppyImageButton2, &QToolButton::clicked, this, [this]() { FloppyImageButton2Click(ui.FloppyImageButton2); });
    connect(ui.FloppyImageCreateButton2, &QToolButton::clicked, this, [this]() { FloppyImageCreateButton2Click(ui.FloppyImageCreateButton2); });
    connect(ui.FloppyImageDelButton, &QToolButton::clicked, this, [this]() { FloppyImageDelButtonClick(ui.FloppyImageDelButton); });
    connect(ui.FloppyImageAddButton, &QToolButton::clicked, this, [this]() { FloppyImageAddButtonClick(ui.FloppyImageAddButton); });
    connect(ui.FloppyImageDriveLetterComboBox2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { FloppyImageDriveLetterComboBox2Change(ui.FloppyImageDriveLetterComboBox2); });
    connect(ui.FloppyPopupCreateImage, &QAction::triggered, this, [this]() { FloppyPopupWork(ui.FloppyPopupCreateImage); });
    connect(ui.FloppyPopupReadImage, &QAction::triggered, this, [this]() { FloppyPopupWork(ui.FloppyPopupReadImage); });
}

TProfileMountEditorFloppyImage2Frame::~TProfileMountEditorFloppyImage2Frame()
{
    delete OpenDialog;
}

void TProfileMountEditorFloppyImage2Frame::resizeEvent(QResizeEvent *E)
{
    QWidget::resizeEvent(E);
    FrameResize(this);   // OnResize
}

// TProfileMountEditorFloppyImage2Frame

void TProfileMountEditorFloppyImage2Frame::FrameResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FloppyImageTab->setColumnWidth(0, ui.FloppyImageTab->width() - 2 * ui.FloppyImageTab->frameWidth() - 25);
}

bool TProfileMountEditorFloppyImage2Frame::Init(const TInfoData& AInfoData)
{
    TStringList *St, *St2;
    QString S;
    int I;
    bool result;

    InfoData = AInfoData;

    ui.FloppyImageLabel->setText(LanguageSetup->ProfileMountingFile());
    ui.FloppyImageAddButton->setToolTip(LanguageSetup->ProfileMountingAddImage());
    ui.FloppyImageDelButton->setToolTip(LanguageSetup->ProfileMountingDelImage());
    ui.FloppyImageSwitchLabel->setText(LanguageSetup->ProfileMountingSwitchImage());
    ui.FloppyImageDriveLetterLabel2->setText(LanguageSetup->ProfileMountingLetter());
    ui.FloppyImageDriveLetterWarningLabel2->setText(LanguageSetup->ProfileMountingDriveLetterAlreadyInUse());
    ui.DriveLetterInfoLabel3->setText(LanguageSetup->ProfileMountingLetterInfo());
    ui.FloppyImageButton2->setToolTip(LanguageSetup->ChooseFile());
    ui.FloppyImageCreateButton2->setToolTip(LanguageSetup->ProfileMountingCreateImage());

    for (char C = 'A'; C <= 'Y'; C++) ui.FloppyImageDriveLetterComboBox2->addItem(QString(QChar(C)));
    ui.FloppyImageDriveLetterComboBox2->addItem("0");
    ui.FloppyImageDriveLetterComboBox2->addItem("1");
    ui.FloppyImageDriveLetterComboBox2->setCurrentIndex(0);

    ui.FloppyPopupCreateImage->setText(LanguageSetup->ProfileMountingFloppyImageCreate());
    ui.FloppyPopupReadImage->setText(LanguageSetup->ProfileMountingFloppyImageRead());

    UserIconLoader->DialogImage(DI_SelectFile, ui.FloppyImageButton2);
    UserIconLoader->DialogImage(DI_ImageFloppy, ui.FloppyImageCreateButton2);
    UserIconLoader->DialogImage(DI_Add, ui.FloppyImageAddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.FloppyImageDelButton);

    St = ValueToList(InfoData.Data);
    S = Trim(ExtUpperCase((*St)[1]));
    if (S == "FLOPPYIMAGE") {
        // ImageFile1$ImageFile2;FLOPPYIMAGE;Letter;;;
        result = true;
        St2 = new TStringList();
        S = Trim((*St)[0]);
        I = Pos('$', S);
        while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos('$', S); }
        St2->Add(S);
        ui.FloppyImageTab->setRowCount(St2->Count());
        for (I = 0; I <= St2->Count() - 1; I++) SetGridCell(ui.FloppyImageTab, I, (*St2)[I]);
        delete St2;
        if (St->Count() >= 3) {
            I = ui.FloppyImageDriveLetterComboBox2->findText(Trim(UpperCase((*St)[2])), Qt::MatchFixedString); if (I < 0) I = 2;
            ui.FloppyImageDriveLetterComboBox2->setCurrentIndex(I);
        }
    } else {
        result = false;
        for (I = 0; I <= ui.FloppyImageDriveLetterComboBox2->count() - 1; I++) {
            if (Pos(ui.FloppyImageDriveLetterComboBox2->itemText(I), InfoData.UsedDriveLetters) == 0) { ui.FloppyImageDriveLetterComboBox2->setCurrentIndex(I); break; }
        }
    }
    delete St;
    if (ui.FloppyImageTab->currentRow() < 0) ui.FloppyImageTab->setCurrentCell(0, 0);

    FloppyImageDriveLetterComboBox2Change(this);
    return result;
}

void TProfileMountEditorFloppyImage2Frame::ShowFrame()
{
}

bool TProfileMountEditorFloppyImage2Frame::CheckBeforeDone()
{
    return true;
}

QString TProfileMountEditorFloppyImage2Frame::Done()
{
    QString S;
    int I;

    // ImageFile;FLOPPYIMAGE;Letter;;;
    S = MakeRelPath(GridCell(ui.FloppyImageTab, 0), PrgSetup->BaseDir()); for (I = 1; I <= ui.FloppyImageTab->rowCount() - 1; I++) S = S + "$" + MakeRelPath(GridCell(ui.FloppyImageTab, I), PrgSetup->BaseDir());

    return StringReplace(S, ";", "<semicolon>", rfReplaceAll) + ";FloppyImage;" + ui.FloppyImageDriveLetterComboBox2->currentText() + ";;;";
}

QString TProfileMountEditorFloppyImage2Frame::GetName()
{
    return LanguageSetup->ProfileMountingFloppyImageSheet();
}

void TProfileMountEditorFloppyImage2Frame::FloppyImageButton2Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (ui.FloppyImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    if (Trim(GridCell(ui.FloppyImageTab, ui.FloppyImageTab->currentRow())) == "") S = PrgSetup->BaseDir(); else S = GridCell(ui.FloppyImageTab, ui.FloppyImageTab->currentRow());
    S = MakeAbsPath(S, PrgSetup->BaseDir());
    OpenDialog->DefaultExt = "iso";
    OpenDialog->InitialDir = ExtractFilePath(S);
    OpenDialog->Title = LanguageSetup->ProfileMountingFile();
    OpenDialog->Filter = LanguageSetup->ProfileMountingFileFilter();
    if (!OpenDialog->Execute()) return;
    SetGridCell(ui.FloppyImageTab, ui.FloppyImageTab->currentRow(), MakeRelPath(OpenDialog->FileName, PrgSetup->BaseDir()));
}

void TProfileMountEditorFloppyImage2Frame::FloppyImageCreateButton2Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    QPoint P;

    if (ui.FloppyImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    P = ui.FloppyImageCreateButton2->mapToGlobal(QPoint(0, 0));  // ClientToScreen(Point(Left,Top))
    ui.FloppyPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TProfileMountEditorFloppyImage2Frame::FloppyImageAddButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FloppyImageTab->setRowCount(ui.FloppyImageTab->rowCount() + 1);
    ui.FloppyImageTab->setCurrentCell(ui.FloppyImageTab->rowCount() - 1, 0);
}

void TProfileMountEditorFloppyImage2Frame::FloppyImageDelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;

    if (ui.FloppyImageTab->currentRow() < 0) {
        MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
        return;
    }
    if (ui.FloppyImageTab->rowCount() == 1) {
        SetGridCell(ui.FloppyImageTab, 0, "");
    } else {
        for (I = ui.FloppyImageTab->currentRow() + 1; I <= ui.FloppyImageTab->rowCount() - 1; I++) SetGridCell(ui.FloppyImageTab, I - 1, GridCell(ui.FloppyImageTab, I));
        ui.FloppyImageTab->setRowCount(ui.FloppyImageTab->rowCount() - 1);
    }
}

void TProfileMountEditorFloppyImage2Frame::FloppyImageDriveLetterComboBox2Change(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.FloppyImageDriveLetterWarningLabel2->setVisible((ui.FloppyImageDriveLetterComboBox2->currentIndex() >= 0) && (Pos(ui.FloppyImageDriveLetterComboBox2->itemText(ui.FloppyImageDriveLetterComboBox2->currentIndex()), InfoData.UsedDriveLetters) > 0));
}

void TProfileMountEditorFloppyImage2Frame::FloppyPopupWork(QObject* Sender)
{
    QString S;

    switch (Sender->property("tag").toInt()) {
        case 0:
            if (ui.FloppyImageTab->currentRow() < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            S = ShowCreateImageFileDialog(this, true, false, InfoData.GameDB);
            if (S == "") return;
            SetGridCell(ui.FloppyImageTab, ui.FloppyImageTab->currentRow(), MakeRelPath(S, PrgSetup->BaseDir()));
            break;
        case 1:
            if (ui.FloppyImageTab->currentRow() < 0) {
                MessageDlg(LanguageSetup->MessageNoImageSelected(), mtError, mbOK, 0);
                return;
            }
            if (!ShowCreateISOImageDialog(this, S, true)) return;
            if (S == "") return;
            SetGridCell(ui.FloppyImageTab, ui.FloppyImageTab->currentRow(), MakeRelPath(S, PrgSetup->BaseDir()));
            break;
    }
}
