// C++ port of InternetDataWaitFormUnit.pas (implementation section).
#include "InternetDataWaitFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"

#include <QPainter>
#include <QTimer>
#include <cmath>

TInternetDataWaitForm* InternetDataWaitForm = nullptr;

namespace {
// "Searching" animation (like aviFindFolder): a magnifying glass moving over a
// folder with documents.
class TSearchAnimation : public QWidget
{
public:
    explicit TSearchAnimation(QWidget* Parent) : QWidget(Parent)
    {
        auto* T = new QTimer(this);
        connect(T, &QTimer::timeout, this, [this]() { Phase += 0.08; update(); });
        T->start(40);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter P(this);
        P.setRenderHint(QPainter::Antialiasing);
        const double W = width(), H = height();
        // folder
        const QRectF Folder(W / 2 - 26, H / 2 - 12, 52, 30);
        P.setPen(QPen(QColor(150, 110, 20), 1));
        P.setBrush(QColor(235, 190, 70));
        P.drawRoundedRect(QRectF(Folder.left(), Folder.top() - 5, 20, 8), 2, 2);   // tab
        // documents in the folder
        P.setPen(QPen(QColor(120, 120, 120), 1));
        P.setBrush(Qt::white);
        P.drawRect(QRectF(Folder.left() + 8, Folder.top() - 8, 22, 20));
        P.drawRect(QRectF(Folder.left() + 20, Folder.top() - 6, 22, 20));
        P.setPen(QPen(QColor(180, 180, 180), 1));
        for (int I = 0; I < 3; I++) P.drawLine(QPointF(Folder.left() + 24, Folder.top() - 1 + I * 4), QPointF(Folder.left() + 38, Folder.top() - 1 + I * 4));
        // folder front
        P.setPen(QPen(QColor(150, 110, 20), 1));
        P.setBrush(QColor(250, 210, 90));
        P.drawRoundedRect(QRectF(Folder.left(), Folder.top() + 4, Folder.width(), Folder.height() - 4), 3, 3);
        // magnifying glass sweeping over the folder
        const QPointF C(W / 2 + std::sin(Phase) * 20, H / 2 + std::cos(Phase * 2) * 4 - 2);
        const double R = 9;
        P.setPen(QPen(QColor(70, 70, 70), 4, Qt::SolidLine, Qt::RoundCap));
        P.drawLine(C + QPointF(R * 0.7, R * 0.7), C + QPointF(R * 1.6, R * 1.6));   // handle
        P.setPen(QPen(QColor(90, 90, 90), 2));
        P.setBrush(QColor(200, 230, 255, 150));
        P.drawEllipse(C, R, R);
        P.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
        P.drawArc(QRectF(C.x() - R + 3, C.y() - R + 3, 2 * R - 6, 2 * R - 6), 100 * 16, 70 * 16);   // reflection
    }
private:
    double Phase = 0;
};
} // namespace

TInternetDataWaitForm::TInternetDataWaitForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // TAnimate (CommonAVI = aviFindFolder): the Windows animation of a magnifying
    // glass searching a folder. Port note: drawn by the program itself (the
    // Windows animation resources are not available on other systems).
    {
        auto* Anim = new TSearchAnimation(ui.Animate);
        auto* L = new QVBoxLayout(ui.Animate);
        L->setContentsMargins(0, 0, 0, 0);
        L->addWidget(Anim);
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
