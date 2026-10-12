// C++ port of DownloadWaitFormUnit.pas (implementation section).
//
// Indy (TIdHTTP / TIdFTP) is replaced by QNetworkAccessManager.  The original
// code ran the download in a TThread and polled it from the GUI thread with
// Application.ProcessMessages; here the download runs asynchronously in the
// GUI thread inside a local QEventLoop, which gives the same behaviour (the
// dialog stays responsive, Abort works).  Qt 6 has no FTP support any more,
// so FTP downloads (direct ftp:// URLs or HTTP redirects to ftp://) use the
// external "curl" program.
#include "DownloadWaitFormUnit.h"

#include <QBuffer>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QStorageInfo>

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PackageDBToolsUnit.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

TDownloadWaitForm *DownloadWaitForm = nullptr;

// TWorkMode (IdComponent)
enum TWorkMode { wmRead, wmWrite };

// TDownloadWaitForm

TDownloadWaitForm::TDownloadWaitForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.AbortButton, &QPushButton::clicked, this, [this]() { AbortButtonClick(ui.AbortButton); });
    FormCreate(this);   // OnCreate
}

TDownloadWaitForm::~TDownloadWaitForm() = default;

void TDownloadWaitForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    Canceled = false;
    HTTP = nullptr;
    FTP = nullptr;
    FWorkEvent1WorkSize = 0;

    ui.AbortButton->setText(LanguageSetup->MsgDlgAbort());
}

void TDownloadWaitForm::AbortButtonClick(QObject * /*Sender*/)
{
    Canceled = true;
    ui.AbortButton->setEnabled(false);
}

void TDownloadWaitForm::WorkEvent(QObject * /*ASender*/, int /*AWorkMode*/, qint64 AWorkCount)
{
    ui.ProgressBar->setValue(int(qMin<qint64>(AWorkCount, MaxInt)));
    if (DownloadWaitForm->ui.ProgressBar->maximum() > 1) {
        if (FWorkEvent1WorkSize > 100000000) {
            ui.ProgressBar->setValue(int(AWorkCount / 100000));
            setWindowTitle(LanguageSetup->PackageManagerDownloading() + " [" + GetNiceFileSize(AWorkCount) + ", " + IntToStr(AWorkCount / 1000 / qint64(ui.ProgressBar->maximum())) + "%]");
        } else {
            ui.ProgressBar->setValue(int(qMin<qint64>(AWorkCount, MaxInt)));
            setWindowTitle(LanguageSetup->PackageManagerDownloading() + " [" + GetNiceFileSize(AWorkCount) + ", " + IntToStr(100 * AWorkCount / qint64(ui.ProgressBar->maximum())) + "%]");
        }
    } else {
        setWindowTitle(LanguageSetup->PackageManagerDownloading() + " [" + GetNiceFileSize(AWorkCount) + "]");
    }
    update();
    repaint();
    ProcessMessages();

    // If Canceled then FTP.Abort / HTTP.Disconnect: the running transfer polls
    // DownloadWaitForm->Canceled and aborts itself (see THTTPThread).
}

// global

void InitDownloadWaitForm(QWidget *AOwner, int MaxPos)
{
    if (DownloadWaitForm == nullptr) DownloadWaitForm = new TDownloadWaitForm(AOwner);
    DownloadWaitForm->ui.ProgressBar->setMinimum(0);
    DownloadWaitForm->ui.ProgressBar->setValue(0);
    DownloadWaitForm->ui.ProgressBar->setMaximum(Max(1, MaxPos));
    DownloadWaitForm->setWindowTitle(LanguageSetup->PackageManagerDownloading());
    DownloadWaitForm->show();
    DownloadWaitForm->update();
    DownloadWaitForm->repaint();
    ProcessMessages();
}

bool StepDownloadWaitForm(int Pos)
{
    bool result = true;
    if (DownloadWaitForm == nullptr) return result;
    if (DownloadWaitForm->ui.ProgressBar->maximum() > 1) {
        DownloadWaitForm->ui.ProgressBar->setValue(Pos);
        DownloadWaitForm->setWindowTitle(LanguageSetup->PackageManagerDownloading() + " [" + IntToStr(100 * qint64(Pos) / qint64(DownloadWaitForm->ui.ProgressBar->maximum())) + "%]");
    }
    DownloadWaitForm->update();
    DownloadWaitForm->repaint();
    ProcessMessages();
    result = !DownloadWaitForm->Canceled;
    return result;
}

void DoneDownloadWaitForm()
{
    if (DownloadWaitForm != nullptr) {
        delete DownloadWaitForm;
        DownloadWaitForm = nullptr;
    }
}

// St is a memory stream (QBuffer) or a file stream (QFile)
static bool SimpleFileCheck(QIODevice *St, const QString &DestFile)
{
    bool result = true;
    QString Ext = Trim(ExtUpperCase(ExtractFileExt(DestFile)));
    if ((Ext != ".ZIP") && (Ext != ".EXE")) return result;

    QByteArray C;
    if (auto *B = qobject_cast<QBuffer *>(St)) {
        C = B->data().left(2);
    } else {
        qint64 SavePos = St->pos();
        St->seek(0);
        C = St->read(2);
        St->seek(SavePos);
    }
    if (C.size() < 2) return false;
    if (Ext == ".ZIP") result = (C[0] == 'P') && (C[1] == 'K');
    if (Ext == ".EXE") result = (C[0] == 'M') && (C[1] == 'Z');
    return result;
}

static QString CalcReferer(const QString &URL, const QString &Referer)
{
    QString result, S, T;
    if (ExtUpperCase(Referer) == "AUTOMATIC") {
        if (Pos("SOURCEFORGE", ExtUpperCase(URL)) == 0) {
            int I = Pos(QString("/") + "/", URL);
            if (I == 0) { S = ""; T = URL; } else { S = Copy(URL, 1, I + 1); T = Copy(URL, I + 2, MaxInt); }
            I = Pos('/', T); if (I > 0) T = Copy(T, 1, I);
            result = S + T;
        } else {
            result = "";
        }
    } else {
        result = Referer;
    }
    return result;
}

// ---------------------------------------------------------------------------
// THTTPThread
// ---------------------------------------------------------------------------

namespace {

class EDownloadError {};   // replaces the Indy exceptions

class THTTPThread
{
public:
    THTTPThread(QWidget *AOwner, const QString &AURL, const QString &AReferer, const QString &ADestFile, qint64 ASize, bool ShowWaitDialog);
    ~THTTPThread();
    void Execute();
    TDownloadResult Success() const { return FSuccess; }

private:
    QString FURL, FReferer, FDestFile;
    TDownloadResult FSuccess;
    qint64 FWorkEvent1WorkCount = 0, FWorkEvent1WorkSize = 0;
    bool FTPDownloadHandled;
    QNetworkAccessManager *HTTP = nullptr;
    QNetworkAccessManager *FTP = nullptr;   // marker while an FTP transfer runs
    QNetworkReply *FReply = nullptr;        // running HTTP request (HTTP.Response)
    qint64 FSize;

    void WorkEvent1(QObject *ASender, int AWorkMode, qint64 AWorkCount);
    void WorkEvent2(QObject *ASender, int AWorkMode);
    void RedirectEvent(QObject *Sender, QString &dest, int &NumRedirect, bool &Handled, QString &VMethod);
    void FixSourceForgeDownload(QBuffer *MSt);
    // HTTP.Get(URL, Stream): throws EDownloadError on failure
    void HTTPGet(const QString &URL, QIODevice *Stream);
    // FTP.Get via curl: returns false on failure
    bool FTPGet(const QString &Host, int Port, const QString &User, const QString &Password, const QString &Path, const QString &FileName);
    qint64 FTPSize(const QString &Host, int Port, const QString &User, const QString &Password, const QString &Path);
    bool Canceled() const { return (DownloadWaitForm != nullptr) && DownloadWaitForm->Canceled; }
};

THTTPThread::THTTPThread(QWidget *AOwner, const QString &AURL, const QString &AReferer, const QString &ADestFile, qint64 ASize, bool ShowWaitDialog)
{
    FURL = AURL;
    FReferer = AReferer;
    FDestFile = ADestFile;
    FSuccess = drFail;
    FTPDownloadHandled = false;
    FSize = ASize;
    if (ShowWaitDialog) InitDownloadWaitForm(AOwner, int(qMin<qint64>(ASize, MaxInt)));
    HTTP = nullptr; FTP = nullptr;
}

THTTPThread::~THTTPThread()
{
    DoneDownloadWaitForm();
}

void THTTPThread::HTTPGet(const QString &AURL, QIODevice *Stream)
{
    QString URL = AURL;
    int NumRedirect = 0;
    while (true) {
        QNetworkRequest Request{QUrl(URL)};
        Request.setRawHeader("User-Agent", "User-Agent=Mozilla/5.0 (Windows; U; Windows NT 6.0)");
        Request.setRawHeader("Accept-Language", "en");
        const QString Referer = CalcReferer(FURL, FReferer);
        if (!Referer.isEmpty()) Request.setRawHeader("Referer", Referer.toUtf8());
        Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        Request.setTransferTimeout(10 * 1000);   // ConnectTimeout / ReadTimeout

        QNetworkReply *Reply = HTTP->get(Request);
        FReply = Reply;
        QEventLoop Loop;
        bool Aborted = false;
        bool IsRedirect = false;
        QObject::connect(Reply, &QNetworkReply::metaDataChanged, &Loop, [&]() {
            const int Status = Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            IsRedirect = (Status >= 300 && Status < 400) && Reply->hasRawHeader("Location");
        });
        QObject::connect(Reply, &QIODevice::readyRead, &Loop, [&]() {
            const QByteArray Data = Reply->readAll();
            if (!IsRedirect) Stream->write(Data);
        });
        QObject::connect(Reply, &QNetworkReply::downloadProgress, &Loop, [&](qint64 Received, qint64 /*Total*/) {
            if (IsRedirect) return;
            WorkEvent1(HTTP, wmRead, Received);
            if (Canceled() && !Aborted) { Aborted = true; Reply->abort(); }   // HTTP.Disconnect
        });
        QObject::connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
        if (!Reply->isFinished()) Loop.exec();
        if (!IsRedirect) {
            const QByteArray Rest = Reply->readAll();
            if (!Rest.isEmpty()) Stream->write(Rest);
        }

        const int Status = Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError Error = Reply->error();
        QString Location;
        if (Status >= 300 && Status < 400 && Reply->hasRawHeader("Location")) {
            Location = Reply->url().resolved(QUrl(QString::fromUtf8(Reply->rawHeader("Location")))).toString();
        }
        FReply = nullptr;
        Reply->deleteLater();

        if (Aborted) throw EDownloadError();
        if (!Location.isEmpty()) {
            // HandleRedirects=True + OnRedirect
            NumRedirect++;
            if (NumRedirect > 15) throw EDownloadError();
            bool Handled = true;
            QString VMethod = "GET";
            RedirectEvent(HTTP, Location, NumRedirect, Handled, VMethod);
            if (!Handled) throw EDownloadError();   // Indy raises when a redirect is not handled
            URL = Location;
            continue;
        }
        if (Error != QNetworkReply::NoError || Status >= 400) throw EDownloadError();
        return;
    }
}

void THTTPThread::Execute()
{
    bool DeletePartialFile;

    if (ExtUpperCase(Copy(FURL, 1, 6)) == QString("FTP:/") + "/") {
        int I = 0; bool B = true; QString V = "GET";
        RedirectEvent(nullptr, FURL, I, B, V);
        if (FTPDownloadHandled) FSuccess = drSuccess; else {
            if (Canceled()) FSuccess = drCancel;
        }
        return;
    }

    HTTP = new QNetworkAccessManager();
    if (DownloadWaitForm != nullptr) DownloadWaitForm->HTTP = HTTP;

    if (FSize < 1024 * 1024) {
        // Small file - download to memory stream
        QBuffer MSt;
        MSt.open(QIODevice::ReadWrite);
        bool Exit = false;
        try {
            HTTPGet(FURL, &MSt);
            if ((MSt.size() < 30000) && ((FSize <= 0) || (FSize != MSt.size()))) FixSourceForgeDownload(&MSt);
        } catch (const EDownloadError &) {
            if (!FTPDownloadHandled) Exit = true;
        }
        if (!Exit) {
            if (!FTPDownloadHandled) {
                if (!SimpleFileCheck(&MSt, FDestFile)) Exit = true;
                else if (Canceled()) { FSuccess = drCancel; Exit = true; }
                else {
                    QFile F(FDestFile);
                    if (F.open(QIODevice::WriteOnly | QIODevice::Truncate)) { F.write(MSt.data()); F.close(); } else Exit = true;   // MSt.SaveToFile raises on error
                }
            }
            if (!Exit) FSuccess = drSuccess;
        }
    } else {
        // Large file - download to file
        auto *FSt = new QFile(FDestFile);
        DeletePartialFile = false;
        bool Exit = false;
        if (!FSt->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            Exit = true;   // TFileStream.Create raises
        } else {
            try {
                bool GetFailed = false;
                try {
                    HTTPGet(FURL, FSt);
                } catch (const EDownloadError &) {
                    DeletePartialFile = true; FSuccess = drFail; GetFailed = true;
                }
                if (GetFailed) {
                    Exit = true;
                } else if ((FSt->size() < 30000) && ((FSize <= 0) || (FSize != FSt->size()))) {
                    QBuffer MSt;
                    MSt.open(QIODevice::ReadWrite);
                    FSt->seek(0);
                    MSt.write(FSt->readAll());
                    delete FSt; FSt = nullptr; DeleteFile(FDestFile);
                    FixSourceForgeDownload(&MSt);
                    FSt = new QFile(FDestFile);
                    FSt->open(QIODevice::ReadWrite | QIODevice::Truncate);
                    FSt->write(MSt.data());
                }
            } catch (const EDownloadError &) {
                if (!FTPDownloadHandled) { DeletePartialFile = true; Exit = true; }
            }
            if (!Exit) {
                if (!FTPDownloadHandled) {
                    if (FSt == nullptr || !SimpleFileCheck(FSt, FDestFile)) { DeletePartialFile = true; Exit = true; }
                    else if (Canceled()) { FSuccess = drCancel; DeletePartialFile = true; Exit = true; }
                }
                if (!Exit) FSuccess = drSuccess;
            }
        }
        delete FSt;
        if (DeletePartialFile) DeleteFile(FDestFile);
    }

    if (DownloadWaitForm != nullptr) DownloadWaitForm->HTTP = nullptr;
    delete HTTP;
    HTTP = nullptr;
}

void THTTPThread::FixSourceForgeDownload(QBuffer *MSt)
{
    const QString SFURL = QString("http:/") + "/downloads.sourceforge.net/";
    if (ExtLowerCase(Copy(FURL, 1, Length(SFURL))) != SFURL) return;

    TStringList St;
    St.SetText(QString::fromLocal8Bit(MSt->data()));
    QString S = St.Text();
    int I = Pos("class=\"direct-download\"", S);
    if (I == 0) return;
    int J = 0, K = 0;
    while (I > 0) {
        if (S[I - 1] == '"') {
            if (J == 0) J = I - 1; else { K = I + 1; break; }
        }
        I--;
    }
    if (K == 0) return;
    S = Copy(S, K, J - K + 1);

    I = Pos("&amp;", S);
    while (I > 0) {
        S = Copy(S, 1, I - 1) + "&" + Copy(S, I + 5, MaxInt);
        I = Pos("&amp;", S);
    }

    MSt->close();
    MSt->setData(QByteArray());
    MSt->open(QIODevice::ReadWrite);
    HTTPGet(S, MSt);
}

void THTTPThread::WorkEvent1(QObject *ASender, int AWorkMode, qint64 AWorkCount)
{
    FWorkEvent1WorkCount = AWorkCount;
    if ((HTTP != nullptr) && (FTP == nullptr)) {
        qint64 ContentLength = -1;
        // HTTP.Response.ContentLength: taken from the running reply
        if (FReply != nullptr) {
            const QVariant V = FReply->header(QNetworkRequest::ContentLengthHeader);
            if (V.isValid()) ContentLength = V.toLongLong();
        }
        FWorkEvent1WorkSize = Max<qint64>(1, ContentLength);
    }
    WorkEvent2(ASender, AWorkMode);   // Synchronize(WorkEvent2)
}

void THTTPThread::WorkEvent2(QObject *ASender, int AWorkMode)
{
    if (DownloadWaitForm == nullptr) return;
    if (DownloadWaitForm->ui.ProgressBar->maximum() == 1) {
        if (FWorkEvent1WorkSize > 100000000)
            DownloadWaitForm->ui.ProgressBar->setMaximum(int(FWorkEvent1WorkSize / 100000));
        else
            DownloadWaitForm->ui.ProgressBar->setMaximum(int(FWorkEvent1WorkSize));
        DownloadWaitForm->SetWorkEvent1WorkSize(FWorkEvent1WorkSize);
    }
    DownloadWaitForm->DoWorkEvent(ASender, AWorkMode, FWorkEvent1WorkCount);
}

// TODO(port): Qt 6 has no FTP support (QNetworkAccessManager / TIdFTP); the
// external "curl" program is used for FTP transfers.
static QString CurlProgram()
{
#ifdef Q_OS_WIN
    QString P = QStandardPaths::findExecutable("curl.exe");
#else
    QString P = QStandardPaths::findExecutable("curl");
#endif
    return P.isEmpty() ? QString("curl") : P;
}

static QString FTPURL(const QString &Host, int Port, const QString &Path)
{
    return "ftp://" + Host + ":" + IntToStr(Port) + Path;
}

qint64 THTTPThread::FTPSize(const QString &Host, int Port, const QString &User, const QString &Password, const QString &Path)
{
    QProcess P;
    P.start(CurlProgram(), {"--silent", "--head", "--ftp-pasv", "--connect-timeout", "10", "--user", User + ":" + Password, FTPURL(Host, Port, Path)});
    if (!P.waitForFinished(30000)) { P.kill(); return -1; }
    const QStringList Lines = QString::fromLocal8Bit(P.readAllStandardOutput()).split('\n');
    for (const QString &L : Lines) {
        const QString T = L.trimmed();
        if (T.startsWith("Content-Length:", Qt::CaseInsensitive)) return T.mid(15).trimmed().toLongLong();
    }
    return -1;
}

bool THTTPThread::FTPGet(const QString &Host, int Port, const QString &User, const QString &Password, const QString &Path, const QString &FileName)
{
    QProcess P;
    P.start(CurlProgram(), {"--silent", "--fail", "--ftp-pasv", "--connect-timeout", "10", "--user", User + ":" + Password, "--output", FileName, FTPURL(Host, Port, Path)});
    if (!P.waitForStarted(10000)) return false;
    while (!P.waitForFinished(100)) {
        ProcessMessages();
        WorkEvent1(FTP, wmRead, GetFileSizeOf(FileName) < 0 ? 0 : GetFileSizeOf(FileName));
        if (Canceled()) {   // FTP.Abort
            P.kill();
            P.waitForFinished(5000);
            return false;
        }
        if (P.state() == QProcess::NotRunning) break;
    }
    WorkEvent1(FTP, wmRead, GetFileSizeOf(FileName) < 0 ? 0 : GetFileSizeOf(FileName));
    return (P.exitStatus() == QProcess::NormalExit) && (P.exitCode() == 0);
}

void THTTPThread::RedirectEvent(QObject * /*Sender*/, QString &dest, int & /*NumRedirect*/, bool &Handled, QString & /*VMethod*/)
// by Alexander Katz (skatz@svitonline.com)
{
    QString S, S1, Username, Password, Host;
    int I, Port;

    if (ExtUpperCase(Copy(dest, 1, 6)) != QString("FTP:/") + "/") return;

    auto *FTPMarker = new QNetworkAccessManager();
    FTP = FTPMarker;
    if (DownloadWaitForm != nullptr) DownloadWaitForm->FTP = FTP;
    S = Copy(dest, 7, MaxInt); I = Pos('/', S); if (I > 0) S = Copy(S, 1, I - 1);
    I = Pos('@', S);
    if (I > 0) {
        S1 = Copy(S, 1, I - 1);
        Delete(S, 1, I);
        I = Pos(':', S1);
        if (I > 0) {
            Username = Copy(S1, 1, I - 1);
            Password = Copy(S1, I + 1, MaxInt);
        } else {
            Username = S1;
            Password = "";
        }
    } else {
        Username = "anonymous";
        Password = "sorry@nomail.com";
    }
    I = Pos(':', S);
    if (I > 0) {
        Port = StrToIntDef(Copy(S, I + 1, MaxInt), 21);
        Delete(S, I, MaxInt);   // remove ":port" from the host name (Delphi: Delete(S,I+1,MaxInt) kept the ':')
    } else Port = 21;
    Host = S;
    S = Copy(dest, 7, MaxInt); I = Pos('/', S); if (I > 0) S = Copy(S, I, MaxInt);
    FWorkEvent1WorkSize = FTPSize(Host, Port, Username, Password, S);

    if (FSize < 1024 * 1024) {
        // Small file - download to memory stream (here: to the destination file, removed again on failure)
        if (!FTPGet(Host, Port, Username, Password, S, FDestFile)) {
            DeleteFile(FDestFile);
        } else {
            QFile MSt(FDestFile);
            bool OK = MSt.open(QIODevice::ReadOnly) && SimpleFileCheck(&MSt, FDestFile);
            MSt.close();
            if (!OK) {
                DeleteFile(FDestFile);
            } else if (Canceled()) {
                FSuccess = drCancel;
                DeleteFile(FDestFile);
            } else {
                Handled = false;
                FTPDownloadHandled = true;
            }
        }
    } else {
        // Large file - download to file
        if (FTPGet(Host, Port, Username, Password, S, FDestFile)) {
            QFile FSt(FDestFile);
            bool OK = FSt.open(QIODevice::ReadOnly) && SimpleFileCheck(&FSt, FDestFile);
            FSt.close();
            if (OK) {
                if (Canceled()) {
                    FSuccess = drCancel;
                } else {
                    Handled = false;
                    FTPDownloadHandled = true;
                }
            }
        }
    }

    if (DownloadWaitForm != nullptr) DownloadWaitForm->FTP = nullptr;
    FTP = nullptr;
    delete FTPMarker;
}

}  // namespace

static TDownloadResult DownloadSingleFileFromInternet(QWidget *AOwner, qint64 Size, const QString &URL, const QString &Referer, const QString &DestFile, bool ShowWaitDialog)
{
    auto *HTTPThread = new THTTPThread(AOwner, DecodeHTMLSymbols(URL), DecodeHTMLSymbols(Referer), DestFile, Size, ShowWaitDialog);
    HTTPThread->Execute();
    TDownloadResult result = HTTPThread->Success();
    delete HTTPThread;
    return result;
}

static void MergeFiles(const TStringList &SourceFiles, const QString &DestFile)
{
    QFile Output(DestFile);
    if (!Output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    for (int I = 0; I < SourceFiles.Count(); I++) {
        QFile Input(SourceFiles[I]);
        if (!Input.open(QIODevice::ReadOnly)) continue;
        while (!Input.atEnd()) Output.write(Input.read(1024 * 1024));
    }
}

static TDownloadResult DownloadFileFromInternet(QWidget *AOwner, qint64 Size, const QString &URL, const QString &Referer, const QString &DestFile, bool ShowWaitDialog)
{
    TDownloadResult result;
    if (Size <= PackageMaxFileSize) {
        result = DownloadSingleFileFromInternet(AOwner, Size, URL, Referer, DestFile, ShowWaitDialog);
        return result;
    }

    result = drFail;
    int Nr = 0;
    ForceDirectories(TempDir() + TempSubFolder);

    TStringList Files;
    qint64 LoadSize = Size;

    while (LoadSize > 0) {
        Nr++;
        QString S = TempDir() + TempSubFolder + PathDelim + ExtractFileName(DestFile) + ".part" + IntToStr(Nr);
        Files.Add(S);
        result = DownloadFileFromInternet(AOwner, Min<qint64>(PackageMaxFileSize, LoadSize), URL + ".part" + IntToStr(Nr), Referer, S, ShowWaitDialog);
        if (result != drSuccess) break;
        LoadSize = LoadSize - Min<qint64>(PackageMaxFileSize, LoadSize);
    }

    if (result == drSuccess) MergeFiles(Files, DestFile);

    for (int I = 0; I < Files.Count(); I++) DeleteFile(Files[I]);
    return result;
}

static TDownloadResult CopyFileWithDialog(QWidget *AOwner, const QString &SourceFile, const QString &DestinationFile)
{
    const qint64 BufSize = 1024 * 1224;
    TDownloadResult result = drFail;

    QFile FSt1(SourceFile);
    if (!FSt1.open(QIODevice::ReadOnly)) return result;
    QFile FSt2(DestinationFile);
    if (!FSt2.open(QIODevice::WriteOnly | QIODevice::Truncate)) return result;

    qint64 C = FSt1.size(), D = 0;
    InitDownloadWaitForm(AOwner, int(qMin<qint64>(C, MaxInt)));
    bool Failed = false;
    while (C > 0) {
        qint64 I = Min(C, BufSize);
        const QByteArray Buffer = FSt1.read(I);
        if (Buffer.size() != I || FSt2.write(Buffer) != I) { Failed = true; break; }
        C -= I; D += I;
        StepDownloadWaitForm(int(qMin<qint64>(D, MaxInt)));
        if ((DownloadWaitForm != nullptr) && DownloadWaitForm->Canceled) { result = drCancel; DoneDownloadWaitForm(); return result; }
    }
    DoneDownloadWaitForm();
    if (Failed) return result;
    result = drSuccess;
    return result;
}

static TDownloadResult GetSingleLocalFile(QWidget *AOwner, const QString &URL, const QString &DestFile)
{
    TDownloadResult result = drCancel;
    QString AlternateURL = Replace(URL, "%20", " ");

#ifdef Q_OS_WIN
    if ((URL.length() < 2) || (URL[1] != ':')) return result;

    while (true) {
        for (char C = 'A'; C <= 'Z'; C++) {
            const QString Root = QString(QChar(C)) + ":\\";
            UINT I = GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16()));
            if ((I != DRIVE_REMOVABLE) && (I != DRIVE_FIXED) && (I != DRIVE_CDROM) && (I != DRIVE_RAMDISK)) continue;
            UINT SaveErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
            if (FileExists(QString(QChar(C)) + Copy(URL, 2, MaxInt))) {
                result = CopyFileWithDialog(AOwner, QString(QChar(C)) + Copy(URL, 2, MaxInt), DestFile);
                if (result == drSuccess) { SetErrorMode(SaveErrorMode); return result; }
            }
            if (FileExists(QString(QChar(C)) + Copy(AlternateURL, 2, MaxInt))) {
                result = CopyFileWithDialog(AOwner, QString(QChar(C)) + Copy(AlternateURL, 2, MaxInt), DestFile);
                if (result == drSuccess) { SetErrorMode(SaveErrorMode); return result; }
            }
            SetErrorMode(SaveErrorMode);
        }
        if (MessageDlg(Format(LanguageSetup->PackageManagerMenuUpdateListsLocal(), {ExtractFileName(URL)}), mtInformation, mbOK | mbCancel, 0) != mrOk) return result;
    }
#else
    // TODO(port): no drive letters outside of Windows - the file is searched at the given
    // path and (for "X:\..." paths) on all mounted volumes.
    while (true) {
        QStringList Candidates;
        if (!((URL.length() >= 2) && (URL[1] == ':'))) {
            Candidates << URL << AlternateURL;
        } else {
            const QString Rel = ToNativePath(Copy(URL, 3, MaxInt)), AltRel = ToNativePath(Copy(AlternateURL, 3, MaxInt));
            for (const QStorageInfo &Volume : QStorageInfo::mountedVolumes()) {
                if (!Volume.isValid() || !Volume.isReady()) continue;
                Candidates << IncludeTrailingPathDelimiter(Volume.rootPath()) + Rel << IncludeTrailingPathDelimiter(Volume.rootPath()) + AltRel;
            }
        }
        for (const QString &F : Candidates) if (FileExists(F)) {
            result = CopyFileWithDialog(AOwner, F, DestFile);
            if (result == drSuccess) return result;
        }
        if (MessageDlg(Format(LanguageSetup->PackageManagerMenuUpdateListsLocal(), {ExtractFileName(URL)}), mtInformation, mbOK | mbCancel, 0) != mrOk) return result;
    }
#endif
}

static TDownloadResult GetLocalFile(QWidget *AOwner, qint64 Size, const QString &URL, const QString &DestFile)
{
    TDownloadResult result;
    if (Size <= PackageMaxFileSize) {
        result = GetSingleLocalFile(AOwner, URL, DestFile);
        return result;
    }

    result = drFail;
    int Nr = 0;
    ForceDirectories(TempDir() + TempSubFolder);

    TStringList Files;
    qint64 LoadSize = Size;

    while (LoadSize > 0) {
        Nr++;
        QString S = TempDir() + TempSubFolder + PathDelim + ExtractFileName(DestFile) + ".part" + IntToStr(Nr);
        Files.Add(S);
        result = GetSingleLocalFile(AOwner, URL + ".part" + IntToStr(Nr), S);
        if (result != drSuccess) break;
        LoadSize = LoadSize - Min<qint64>(PackageMaxFileSize, LoadSize);
    }
    if (result == drSuccess) MergeFiles(Files, DestFile);

    for (int I = 0; I < Files.Count(); I++) DeleteFile(Files[I]);
    return result;
}

static bool IsInternetURL(const QString &URL)
{
    QString S = ExtUpperCase(URL);
    return (Copy(S, 1, 7) == QString("HTTP:/") + "/") || (Copy(S, 1, 8) == QString("HTTPS:/") + "/") || (Copy(S, 1, 6) == QString("FTP:/") + "/");
}

static QString BuildAbsURL(const QString &AbsBase, const QString &URL)
{
    QString result;
    if (!IsInternetURL(URL)) {
        result = AbsBase; while (!result.isEmpty() && result[result.length() - 1] != '/') result.chop(1);
        if (!URL.isEmpty() && URL[0] != '/') result = result + URL; else result = result + Copy(URL, 2, MaxInt);
    } else {
        result = URL;
    }
    return result;
}

TDownloadResult DownloadFileWithDialog(QWidget *AOwner, qint64 Size, const QString &AbsBase, const QString &URL, const QString &Referer, const QString &DestFile)
{
    TDownloadResult result;
    QString S;
    if (IsInternetURL(URL) || IsInternetURL(AbsBase)) {
        S = BuildAbsURL(AbsBase, URL);
        result = DownloadFileFromInternet(AOwner, Size, S, Referer, DestFile, true);
    } else {
        S = MakeAbsPath(URL, IncludeTrailingPathDelimiter(ExtractFilePath(AbsBase)));
        result = GetLocalFile(AOwner, Size, S, DestFile);
    }
    return result;
}

TDownloadResult DownloadFileWithOutDialog(QWidget *AOwner, qint64 Size, const QString &AbsBase, const QString &URL, const QString &Referer, const QString &DestFile)
{
    TDownloadResult result;
    QString S;
    if (IsInternetURL(URL) || IsInternetURL(AbsBase)) {
        S = BuildAbsURL(AbsBase, URL);
        result = DownloadFileFromInternet(AOwner, Size, S, Referer, DestFile, false);
    } else {
        S = MakeAbsPath(URL, IncludeTrailingPathDelimiter(ExtractFilePath(AbsBase)));
        result = GetLocalFile(AOwner, Size, S, DestFile);
    }
    return result;
}

static TStringList *GetDownloadLinksFromMetaLink(const QString &XMLFileName)
{
    auto *result = new TStringList();
    QDomDocument *XML = LoadXMLDoc(XMLFileName); if (XML == nullptr) return result;

    auto ChildByName = [](const QDomElement &P, const QString &Name) -> QDomElement {
        for (QDomNode N = P.firstChild(); !N.isNull(); N = N.nextSibling()) if (N.isElement() && N.nodeName() == Name) return N.toElement();
        return QDomElement();
    };

    QDomElement N1 = XML->documentElement();
    if (N1.nodeName() == "metalink") {
        QDomElement N2 = ChildByName(N1, "files");
        if (!N2.isNull()) {
            N1 = ChildByName(N2, "file");
            if (!N1.isNull()) {
                N2 = ChildByName(N1, "resources");
                if (!N2.isNull()) {
                    for (QDomNode N = N2.firstChild(); !N.isNull(); N = N.nextSibling())
                        if (N.isElement() && N.nodeName() == "url") result->Add(N.toElement().text());
                }
            }
        }
    }
    delete XML;
    return result;
}

static TDownloadResult MetaLinkProcessor(QWidget *AOwner, int ASize, const QString &XMLFileName, const QString &DestFile)
{
    TDownloadResult result = drFail;
    TStringList *St = GetDownloadLinksFromMetaLink(XMLFileName);
    while (St->Count() > 0) {
        int I = int(QRandomGenerator::global()->bounded(St->Count()));
        result = DownloadFileWithDialog(AOwner, ASize, "", (*St)[I], "", DestFile);
        if (result != drFail) break;
        St->Delete(I);
    }
    delete St;
    return result;
}

TDownloadResult MetaLinkDownload(QWidget *AOwner, int ASize, const QString &AbsBase, const QString &MetaLinkURL, const QString &Referer, const QString &DestFile)
{
    QString TempXMLFile = TempDir() + PackageDBTempFile;
    TDownloadResult result = DownloadFileWithDialog(AOwner, 0, AbsBase, MetaLinkURL, Referer, TempXMLFile);
    if (result != drSuccess) return result;
    result = MetaLinkProcessor(AOwner, ASize, TempXMLFile, DestFile);
    ExtDeleteFile(TempXMLFile, ftTemp);
    return result;
}
