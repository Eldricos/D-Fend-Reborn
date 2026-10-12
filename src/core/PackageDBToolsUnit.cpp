// C++ port of PackageDBToolsUnit.pas (implementation section).
#include "PackageDBToolsUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "PackageDBUnit.h"
#include "DownloadWaitFormUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

QString LoadXMLDoc(const QString& FileName, QDomDocument*& XMLDoc, const QString& OrigFileName)
{
    QString result;
    QString ErrorFileName;
    QByteArray MSt;

    XMLDoc = nullptr;

    if (!FileExists(FileName)) return result;

    if (Trim(OrigFileName) == "") ErrorFileName = FileName; else ErrorFileName = OrigFileName;

    XMLDoc = new QDomDocument;

    {
        QFile F(FileName);
        if (!F.open(QIODevice::ReadOnly)) {
            result = Format(LanguageSetup->MessageCouldNotOpenFile(), {ErrorFileName});
            delete XMLDoc; XMLDoc = nullptr;
            return result;
        }
        MSt = F.readAll();
    }
    // Delphi 2007 TXMLDocument accepted files without encoding declaration
    // containing ANSI characters; QDom expects UTF-8 in that case.
    if (!XMLDoc->setContent(MSt)) {
        result = Format(LanguageSetup->MessageCouldNotActivateXML(), {ErrorFileName});
        delete XMLDoc; XMLDoc = nullptr;
        return result;
    }

    if (XMLDoc->documentElement().isNull()) {
        result = Format(LanguageSetup->MessageCouldNotActivateXML(), {ErrorFileName});
        delete XMLDoc; XMLDoc = nullptr;
        return result;
    }

    return result;
}

QDomDocument* LoadXMLDoc(const QString& FileName, const QString& OrigFileName)
{
    QDomDocument* result = nullptr;
    QString S = LoadXMLDoc(FileName, result, OrigFileName);
    if (S != "") MessageDlg(S, mtError, mbOK, 0);
    return result;
}

TStringList* SaveXMLDoc(const QDomDocument& Doc, const QList<QString>& Lines, bool Standalone)
{
    QString S;
    TStringList* result;

    // Doc.Version:='1.0'; Doc.StandAlone; Doc.Encoding:='UTF-8'; NodeIndentStr:='  ';
    S = Doc.toString(2);
    result = new TStringList;

    result->SetText(S);
    // The first line is the xml declaration (it is replaced below).  QDom
    // documents created in code do not have one - insert a placeholder.
    if (result->Count() == 0 || !Trim((*result)[0]).startsWith("<?xml")) result->Insert(0, "");
    if (Standalone)
        (*result)[0] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    else
        (*result)[0] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>";
    for (int X = 0; X < Lines.count(); X++) result->Insert(1 + X, Lines[X]);
    return result;
}

void SaveXMLDoc(const QDomDocument& Doc, const QList<QString>& Lines, const QString& Datei, bool Standalone)
{
    TStringList* St = SaveXMLDoc(Doc, Lines, Standalone);
    // The file is declared as UTF-8, so store it as UTF-8 (without BOM).
    St->SaveToFileUtf8(Datei, false);
    delete St;
}

TDateTime DecodeUpdateDate(const QString& S_)
{
    QString S = S_;
    int I;
    int D, M, Y;
    TDateTime result = 0;

    I = Pos("/", S); if (I == 0) return result;
    if (!TryStrToInt(Copy(S, 1, I - 1), M)) return result;
    S = Copy(S, I + 1, MaxInt);

    I = Pos("/", S); if (I == 0) return result;
    if (!TryStrToInt(Copy(S, 1, I - 1), D)) return result;
    S = Copy(S, I + 1, MaxInt);

    if (!TryStrToInt(S, Y)) return result;

    if (!QDate::isValid(Y, M, D)) return result;
    result = EncodeDate(Y, M, D);
    return result;
}

QString EncodeUpdateDate(TDateTime Date)
{
    int Y, M, D;
    QString S1, S2, S3;
    DecodeDate(Date, Y, M, D);
    S1 = IntToStr(M); if (Length(S1) < 2) S1 = "0" + S1;
    S2 = IntToStr(D); if (Length(S2) < 2) S2 = "0" + S2;
    S3 = IntToStr(Y); if (Length(S3) < 2) S3 = "0" + S3;
    return S1 + "/" + S2 + "/" + S3;
}

// Indy TIdHTTP.Get replacement: synchronous download via QNetworkAccessManager
static QBuffer* DownloadFileFromInternet(const QString& URL, bool Quite)
{
    QBuffer* result = nullptr;

    QNetworkAccessManager HTTP;
    QNetworkRequest Request{QUrl(URL)};
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* Reply = HTTP.get(Request);
    QEventLoop Loop;
    QObject::connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
    if (!Reply->isFinished()) Loop.exec(QEventLoop::ExcludeUserInputEvents);

    const int Status = Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (Reply->error() != QNetworkReply::NoError || (Status != 0 && (Status < 200 || Status >= 300))) {
        if (!Quite) MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {URL}), mtError, mbOK, 0);
    } else {
        result = new QBuffer;
        result->setData(Reply->readAll());
        result->open(QIODevice::ReadOnly);
    }
    delete Reply;
    return result;
}

static QBuffer* LoadFileToBuffer(const QString& FileName)
{
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return nullptr;
    QBuffer* result = new QBuffer;
    result->setData(F.readAll());
    result->open(QIODevice::ReadOnly);
    result->seek(0);
    return result;
}

static QBuffer* GetFileFromAnyDrive(const QString& Path)
{
    QBuffer* result = nullptr;
#ifdef Q_OS_WIN
    if ((Length(Path) < 2) || (Path[1] != ':')) return result;

    for (char C = 'A'; C <= 'Z'; C++) {
        const QString Root = QString(QChar(C)) + ":\\";
        UINT I = GetDriveTypeW(reinterpret_cast<LPCWSTR>(Root.utf16()));
        if ((I != DRIVE_REMOVABLE) && (I != DRIVE_FIXED) && (I != DRIVE_CDROM) && (I != DRIVE_RAMDISK)) continue;
        UINT SaveErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
        const QString F = QString(QChar(C)) + Copy(Path, 2, MaxInt);
        if (FileExists(F)) {
            result = LoadFileToBuffer(F);
            if (result != nullptr) { SetErrorMode(SaveErrorMode); return result; }
        }
        SetErrorMode(SaveErrorMode);
    }
#else
    // TODO(port): there are no drive letters - just try the path itself
    // (and with native path delimiters).
    if (FileExists(Path)) return LoadFileToBuffer(Path);
    if (FileExists(ToNativePath(Path))) return LoadFileToBuffer(ToNativePath(Path));
#endif
    return result;
}

static QBuffer* GetFileFromLocalPath(const QString& Path)
{
    QBuffer* result;
    do {
        result = GetFileFromAnyDrive(Path);
        if (result != nullptr) return result;
        if (MessageDlg(Format(LanguageSetup->PackageManagerMenuUpdateListsLocal(), {ExtractFileName(Path)}), mtInformation, mbOK | mbCancel, 0) != mrOk) return result;
    } while (true);
}

QBuffer* DownloadFile(const QString& URL, bool Quite)
{
    // Port: https URLs are downloaded from the internet, too (Indy only
    // supported plain http here).
    if (ExtUpperCase(Copy(URL, 1, 7)) == "HTTP:/" "/" || ExtUpperCase(Copy(URL, 1, 8)) == "HTTPS:/" "/") {
        return DownloadFileFromInternet(URL, Quite);
    } else {
        return GetFileFromLocalPath(URL);
    }
}

bool DownloadFile(const QString& URL, const QString& FileName, bool Quite)
{
    bool result = false;
    QBuffer* MSt = DownloadFile(URL, Quite);
    if (MSt == nullptr) return result;
    QFile F(FileName);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate) || F.write(MSt->data()) != MSt->data().size()) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {FileName}), mtError, mbOK, 0);
        delete MSt;
        return result;
    }
    F.close();
    result = true;
    delete MSt;
    return result;
}

QString GetNiceFileSize(qint64 Size)
{
    if (Size < 1024) return IntToStr(Size) + " " + LanguageSetup->Bytes();
    if (Size < 1024 * 1024) return IntToStr(Size / 1024) + " " + LanguageSetup->KBytes();
    return IntToStr(Size / 1024 / 1024) + " " + LanguageSetup->MBytes();
}

QString ExtractFileNameFromURL(const QString& URL, const QString& DefaultExtension, bool RemoveDirs)
{
    static const char* Exts[] = {".EXE", ".ZIP", ".7Z", ".ICO", ".INI", ".PROF", ".XML"};
    int I;
    QString S;
    QString result = URL;

    if ((ExtUpperCase(Copy(URL, 1, 7)) == "HTTP:/" "/") || (Pos("/", URL) > 0)) {
        if (ExtUpperCase(Copy(URL, 1, 7)) == "HTTP:/" "/") result = Copy(result, 8, MaxInt);
        if (RemoveDirs) {
            I = Pos("/", result); while (I > 0) { result = Copy(result, I + 1, MaxInt); I = Pos("/", result); }
        } else {
            result = Replace(result, "/", "-");
        }
        result = Replace(result, "%20", " ");
    } else {
        result = Replace(result, ":", "-");
        result = Replace(result, "\\", "-");
        result = Replace(result, "%20", " ");
        if (ExtUpperCase(Copy(result, Length(result) - 3, 4)) == ".XML") result = Trim(Copy(result, 1, Length(result) - 4));
    }

    result = MakeFileSysOKFolderName(result);

    S = ExtUpperCase(ExtractFileExt(ExtractFileName(result)));
    for (const char* E : Exts) if (S == QLatin1String(E)) return result;
    if (S == ExtUpperCase(DefaultExtension)) return result;
    result = result + DefaultExtension;
    return result;
}

QString URLFileNameFromFileName(const QString& FileName)
{
    return Replace(FileName, " ", "%20");
}

QString ReplaceBRs(const QString& S)
{
    return Replace(S, "\\\\", "\r");
}

void ClearPackageCache()
{
    int I;
    TSearchRec Rec;
    QString UserUpper;
    bool B;
    const QString Dir = IncludeTrailingPathDelimiter(ToNativePath(PrgDataDir() + PackageDBSubFolder));

    I = FindFirst(Dir + "*.xml", faAnyFile, Rec);
    UserUpper = ExtUpperCase(PackageDBUserFile);
    while (I == 0) {
        B = true;
        if (((Rec.Attr & faDirectory) == 0) && (ExtUpperCase(Rec.Name) != UserUpper)) ExtDeleteFile(Dir + Rec.Name, ftTemp, true, B);
        if (!B) { FindClose(Rec); return; }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    ExtDeleteFolder(ToNativePath(PrgDataDir() + PackageDBCacheSubFolder), ftTemp);
}

namespace {
class TPackageDownloadStatusClass
{
private:
    QWidget* FOwner;
public:
    TPackageDownloadStatusClass(QWidget* AOwner) : FOwner(AOwner) {}
    void PackageDBDownload(QObject* Sender, qint64 Progress, qint64 Size, TDownloadStatus Status, bool& ContinueDownload)
    {
        Q_UNUSED(Sender);
        switch (Status) {
            case dsStart: InitDownloadWaitForm(FOwner, static_cast<int>(Size / 1024)); break;
            case dsProgress:
                if (DownloadWaitForm != nullptr && DownloadWaitForm->ui.ProgressBar->maximum() == 1) DownloadWaitForm->ui.ProgressBar->setMaximum(static_cast<int>(qMax<qint64>(1, Size / 1024)));
                ContinueDownload = StepDownloadWaitForm(static_cast<int>(Progress / 1024));
                break;
            case dsDone: DoneDownloadWaitForm(); break;
        }
    }
};
}

bool UpdatePackageDB(QWidget* Owner, bool UpdateAllListe, bool Quite)
{
    Q_UNUSED(UpdateAllListe);  // not used in the original either
    bool result;
    TPackageDownloadStatusClass PackageDownloadStatusClass(Owner);
    TPackageDB* PackageDB = new TPackageDB;
    PackageDB->LoadDB(false, false, Quite);
    PackageDB->SetOnDownload([&PackageDownloadStatusClass](QObject* Sender, qint64 Progress, qint64 Size, TDownloadStatus Status, bool& ContinueDownload) {
        PackageDownloadStatusClass.PackageDBDownload(Sender, Progress, Size, Status, ContinueDownload);
    });
    // GetKeyState(VK_LSHIFT) / GetKeyState(VK_RSHIFT)
    result = PackageDB->LoadDB(true, (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) != 0, Quite);
    delete PackageDB;
    return result;
}
