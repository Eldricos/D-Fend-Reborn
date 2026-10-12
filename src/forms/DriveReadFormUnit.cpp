// C++ port of DriveReadFormUnit.pas (implementation section).
#include "DriveReadFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TDriveReadForm* DriveReadForm = nullptr;

// GetDriveType(PChar(Drive+':\'))=DRIVE_CDROM
static bool IsCDROMDrive(QChar Drive)
{
#ifdef Q_OS_WIN
    const QString Root = QString(Drive) + ":\\";
    return GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16())) == DRIVE_CDROM;
#else
    // TODO(port): drive letters only exist on Windows
    Q_UNUSED(Drive);
    return false;
#endif
}

TDriveReadForm::TDriveReadForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.AbortButton, &QPushButton::clicked, this, [this]() { AbortButtonClick(ui.AbortButton); });
    FormCreate(this);   // OnCreate
}

TDriveReadForm::~TDriveReadForm() = default;

void TDriveReadForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TDriveReadForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=... - not needed in Qt

    WriteMode = false;
    Count = 0;
    Aborted = false;
    // ProgressBar.DoubleBuffered:=True; StatusBar.DoubleBuffered:=True; - not needed in Qt

    ui.AbortButton->setText(LanguageSetup->Abort());

    UserIconLoader->DialogImage(DI_Abort, ui.AbortButton);
}

void TDriveReadForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (WriteMode) {
        setWindowTitle(LanguageSetup->WriteImageCaptionIMG());
    } else {
        if (IsCDROMDrive(Drive))
            setWindowTitle(LanguageSetup->ReadImageCaptionISO());
        else
            setWindowTitle(LanguageSetup->ReadImageCaptionIMG());
    }

    // PostMessage(Handle,WM_USER+1,0,0);
    QTimer::singleShot(0, this, [this]() { QEvent* Msg = nullptr; AfterOnShow(Msg); });
}

void TDriveReadForm::AfterOnShow(QEvent*& Msg)
{
    Q_UNUSED(Msg);
    const TReadProgressEvent Progress = [this](quint32 BytesRead, quint32 TotalBytesToRead, bool& ContinueReading) {
        ReadProgress(BytesRead, TotalBytesToRead, ContinueReading);
    };
    if (WriteMode) {
        ReadResult = WriteFloppyData(Drive, FileName, Progress);
    } else {
        if (IsCDROMDrive(Drive))
            ReadResult = ReadCDData(Drive, FileName, Progress);
        else
            ReadResult = ReadFloppyData(Drive, FileName, Progress);
    }
    setModalResult(mrOk);
    // PostMessage(Handle,WM_Close,0,0); - setModalResult closes the form after this handler
}

void TDriveReadForm::AbortButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    Aborted = true;
}

void TDriveReadForm::ReadProgress(quint32 BytesRead, quint32 TotalBytesToRead, bool& ContinueReading)
{
    QString S;
    Count++;
    if (Count % 10 == 0) {
        if (WriteMode) {
            VclLayout::setSimpleText(ui.StatusBar, Format(LanguageSetup->WriteImageInfo(), {IntToStr(BytesRead / 1024), IntToStr(TotalBytesToRead / 1024)}));
        } else {
            if (TotalBytesToRead > 0) S = Format(" (" + LanguageSetup->ReadImageInfo2() + ")", {IntToStr(TotalBytesToRead / 1024)}); else S = "";
            VclLayout::setSimpleText(ui.StatusBar, Format(LanguageSetup->ReadImageInfo1(), {IntToStr(BytesRead / 1024)}) + S);
        }
    }
    if (TotalBytesToRead > 0)
        ui.ProgressBar->setValue(Round(double(BytesRead) / double(TotalBytesToRead) * 1000));
    else
        ui.ProgressBar->setValue(Round(double(BytesRead) / (1440.0 * 1024) * 1000));
    ui.ProgressBar->repaint();
    ProcessMessages();
    ContinueReading = !Aborted;
}

// { global }

TReadDataResult ShowDriveReadDialog(QWidget* AOwner, QChar ADrive, const QString& AFileName)
{
    DriveReadForm = new TDriveReadForm(AOwner);
    DriveReadForm->Drive = ADrive;
    DriveReadForm->FileName = AFileName;
    DriveReadForm->ShowModal();
    const TReadDataResult result = DriveReadForm->ReadResult;
    delete DriveReadForm;
    DriveReadForm = nullptr;
    return result;
}

TReadDataResult ShowDriveWriteDialog(QWidget* AOwner, QChar ADrive, const QString& AFileName)
{
    DriveReadForm = new TDriveReadForm(AOwner);
    DriveReadForm->WriteMode = true;
    DriveReadForm->Drive = ADrive;
    DriveReadForm->FileName = AFileName;
    DriveReadForm->ShowModal();
    const TReadDataResult result = DriveReadForm->ReadResult;
    delete DriveReadForm;
    DriveReadForm = nullptr;
    return result;
}
