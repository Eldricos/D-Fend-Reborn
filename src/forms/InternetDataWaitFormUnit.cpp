// C++ port of InternetDataWaitFormUnit.pas (implementation section).
#include "InternetDataWaitFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"

TInternetDataWaitForm* InternetDataWaitForm = nullptr;

TInternetDataWaitForm::TInternetDataWaitForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // TODO(port): TAnimate (CommonAVI = aviFindFolder) has no Qt equivalent; an
    // indeterminate progress bar inside the placeholder shows the activity.
    {
        auto* Busy = new QProgressBar(ui.Animate);
        Busy->setRange(0, 0);
        Busy->setTextVisible(false);
        auto* L = new QVBoxLayout(ui.Animate);
        L->setContentsMargins(0, 0, 0, 0);
        L->addWidget(Busy, 0, Qt::AlignVCenter);
        ui.Animate->setText(QString());
    }

    // DFM: Timer (Interval = 50, OnTimer = TimerTimer)
    Timer = new QTimer(this);
    Timer->setInterval(50);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });
    Timer->start();

    Thread = nullptr;
    ThreadList = nullptr;
}

TInternetDataWaitForm::~TInternetDataWaitForm()
{
}

void TInternetDataWaitForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TInternetDataWaitForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed with Qt
}

void TInternetDataWaitForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    bool B;
    int I;
    if ((Thread != nullptr) && Thread->isFinished()) setModalResult(mrOk);
    if (ThreadList != nullptr) {
        B = true;
        for (I = 0; I < ThreadList->count(); I++) if (!static_cast<QThread*>(static_cast<TDataReaderThread*>((*ThreadList)[I]))->isFinished()) { B = false; break; }
        if (B) setModalResult(mrOk);
    }
}

/* global */

static void ShowInternetWaitDialog(QWidget* AOwner, QThread* AThread, const QString& ACaption, const QString& AInfo)
{
    InternetDataWaitForm = new TInternetDataWaitForm(AOwner);
    InternetDataWaitForm->setWindowTitle(ACaption);
    InternetDataWaitForm->ui.InfoLabel->setText(AInfo);
    InternetDataWaitForm->Thread = AThread;
    InternetDataWaitForm->ShowModal();
    delete InternetDataWaitForm;
    InternetDataWaitForm = nullptr;
}

static void ShowInternetWaitDialog(QWidget* AOwner, TList* AThread, const QString& ACaption, const QString& AInfo)
{
    InternetDataWaitForm = new TInternetDataWaitForm(AOwner);
    InternetDataWaitForm->setWindowTitle(ACaption);
    InternetDataWaitForm->ui.InfoLabel->setText(AInfo);
    InternetDataWaitForm->ThreadList = AThread;
    InternetDataWaitForm->ShowModal();
    delete InternetDataWaitForm;
    InternetDataWaitForm = nullptr;
}

static bool ShowDataReaderInternetWaitDialog(QWidget* AOwner, TDataReaderThread* AThread, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    ShowInternetWaitDialog(AOwner, static_cast<QThread*>(AThread), ACaption, AInfo);
    bool result = AThread->Success();
    if (!result) MessageDlg(AError, mtError, mbOK, 0, AOwner);
    return result;
}

static bool ShowDataReaderInternetWaitDialog(QWidget* AOwner, TList* AThread, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    ShowInternetWaitDialog(AOwner, AThread, ACaption, AInfo);
    bool result = true;
    for (int I = 0; I < AThread->count(); I++) if (!static_cast<TDataReaderThread*>((*AThread)[I])->Success()) result = false;
    if (!result) MessageDlg(AError, mtError, mbOK, 0, AOwner);
    return result;
}

bool ShowDataReaderInternetConfigWaitDialog(QWidget* AOwner, TDataReader* ADataReader, bool AForceUpdate, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    TDataReaderLoadConfigThread* DataReaderLoadConfigThread = new TDataReaderLoadConfigThread(ADataReader, AForceUpdate);
    bool result = ShowDataReaderInternetWaitDialog(AOwner, DataReaderLoadConfigThread, ACaption, Format(AInfo, {DomainOnly(DataReaderUpdateURL)}), Format(AError, {DomainOnly(DataReaderUpdateURL)}));
    delete DataReaderLoadConfigThread;
    return result;
}

bool ShowDataReaderInternetListWaitDialog(QWidget* AOwner, TDataReader* ADataReader, const QString& AName, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    TDataReaderGameListThread* DataReaderGameListThread = new TDataReaderGameListThread(ADataReader, AName);
    QString URL = ADataReader->GetListURL(PrgSetup->DataReaderAllPlatforms());
    bool result = ShowDataReaderInternetWaitDialog(AOwner, DataReaderGameListThread, ACaption, Format(AInfo, {DomainOnly(URL)}), Format(AError, {DomainOnly(URL)}));
    delete DataReaderGameListThread;
    return result;
}

TDataReaderGameDataThread* ShowDataReaderInternetDataWaitDialog(QWidget* AOwner, TDataReader* ADataReader, int ANr, bool AFullImages, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    TDataReaderGameDataThread* result = new TDataReaderGameDataThread(ADataReader, ANr, AFullImages);
    if (!ShowDataReaderInternetWaitDialog(AOwner, result, ACaption, Format(AInfo, {ADataReader->DataDomain()}), Format(AError, {ADataReader->DataDomain()}))) { delete result; result = nullptr; }
    return result;
}

void ShowDataReaderInternetCoverWaitDialog(QWidget* AOwner, TDataReader* ADataReader, const QString& ADownloadURL, const QString& ADestFolder, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    TDataReaderGameCoverThread* DataReaderGameCoverThread = new TDataReaderGameCoverThread(ADataReader, ADownloadURL, ADestFolder);
    ShowDataReaderInternetWaitDialog(AOwner, DataReaderGameCoverThread, ACaption, Format(AInfo, {ADataReader->DataDomain()}), Format(AError, {ADataReader->DataDomain()}));
    delete DataReaderGameCoverThread;
}

void ShowDataReaderInternetCoverWaitDialog(QWidget* AOwner, TDataReader* ADataReader, TStringList* ADownloadURLs, const QString& ADestFolder, const QString& ACaption, const QString& AInfo, const QString& AError)
{
    const int ThreadCount = 8;
    TDataReaderGameCoverThread* DataReaderGameCoverThread[ThreadCount];
    TStringList* URLList[ThreadCount];
    int I;

    for (I = 0; I < ThreadCount; I++) { URLList[I] = new TStringList; DataReaderGameCoverThread[I] = nullptr; }
    for (I = 0; I < ADownloadURLs->Count(); I++) URLList[I % ThreadCount]->Add((*ADownloadURLs)[I]);
    for (I = 0; I < ThreadCount; I++) if (URLList[I]->Count() > 0) DataReaderGameCoverThread[I] = new TDataReaderGameCoverThread(ADataReader, URLList[I], ADestFolder);
    {
        TList ThreadList;
        for (I = 0; I < ThreadCount; I++) if (DataReaderGameCoverThread[I] != nullptr) ThreadList.append(DataReaderGameCoverThread[I]);
        ShowDataReaderInternetWaitDialog(AOwner, &ThreadList, ACaption, Format(AInfo, {ADataReader->DataDomain()}), Format(AError, {ADataReader->DataDomain()}));
    }
    for (I = 0; I < ThreadCount; I++) if (DataReaderGameCoverThread[I] != nullptr) delete DataReaderGameCoverThread[I];
    for (I = 0; I < ThreadCount; I++) delete URLList[I];
}

QString DomainOnly(const QString& S)
{
    int I;
    QString result = S;
    I = Pos(QStringLiteral(":/") + "/", result); if (I > 0) result = Copy(result, I + 3, MaxInt);
    I = Pos("/", result); if (I > 0) result = Copy(result, 1, I - 1);
    return result;
}
