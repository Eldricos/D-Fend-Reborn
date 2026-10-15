// C++ port of ProgramUpdateCheckUnit.pas (implementation section).
//
// D-Fend Reborn: the update check asks GitHub for the latest release of the
// project (DFRebornLatestReleaseAPI). If there is a newer version:
// - Windows: if the user wants to update, the update installer of the
//   release (D-Fend-Reborn-x.y.z-UpdateSetup.exe) is downloaded and started
//   in automatic mode (/A) for the program folder, and D-Fend Reborn closes;
//   the update installer starts it again when it is finished. (The original
//   used the external UpdateCheck.exe for this, an NSIS downloader that
//   current virus scanners block.) Installed and portable copies are
//   updated; without DFend.dat (e.g. a build folder) the release page is
//   opened instead.
// - other systems: the user is asked whether to open the release page.
#include "ProgramUpdateCheckUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "MainUnit.h"
#include "DownloadWaitFormUnit.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <cmath>

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

// URL of the update information; settings files of older versions may still
// contain the address of the original D-Fend Reloaded update file.
static QString UpdateCheckURL()
{
    QString URL = Trim(PrgSetup->UpdateCheckURL());
    if (URL.isEmpty() || URL.contains("UpdateInfo.txt", Qt::CaseInsensitive)) URL = DFRebornLatestReleaseAPI;
    return URL;
}

#ifdef Q_OS_WIN
// Downloads the update installer of the release and starts it for the
// program folder. Returns false if the program cannot be updated this way
// (then the release page is offered).
static bool InstallUpdate(QWidget *AOwner, const QJsonObject& Release)
{
    // DFend.dat is written by the setup and is part of the portable zip; the
    // update installer needs it to know the installation type
    if (!FileExists(PrgDir() + OperationModeConfig)) return false;

    QString URL;
    qint64 Size = -1;
    for (const QJsonValue V : Release.value("assets").toArray()) {
        const QJsonObject Asset = V.toObject();
        if (Asset.value("name").toString().endsWith("-UpdateSetup.exe", Qt::CaseInsensitive)) {
            URL = Asset.value("browser_download_url").toString();
            Size = Asset.value("size").toInteger(-1);
            break;
        }
    }
    if (URL.isEmpty()) return false;

    const QString FileName = TempDir() + "DFendRebornUpdate.exe";
    if (FileExists(FileName)) QFile::remove(FileName);
    switch (DownloadFileWithDialog(AOwner, Size, "", URL, "", FileName)) {
        case drSuccess: break;
        case drCancel: return true;
        default:
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {URL}), mtError, mbOK, 0);
            return true;
    }

    // ShellExecute: the update installer needs admin rights (UAC prompt);
    // /D= must be the last parameter, without quotes and trailing backslash
    const QString Params = "/A /D=" + QDir::toNativeSeparators(ExcludeTrailingPathDelimiter(PrgDir()));
    const HINSTANCE R = ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(FileName).utf16()),
                                      reinterpret_cast<LPCWSTR>(Params.utf16()), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(R) <= 32) {
        // e.g. the UAC prompt was cancelled
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {FileName}), mtError, mbOK, 0);
        return true;
    }

    // the update installer replaces the program files: close D-Fend Reborn
    if (DFendRebornMainForm != nullptr) QTimer::singleShot(0, DFendRebornMainForm, []() { DFendRebornMainForm->close(); });
    return true;
}
#endif

// Synchronous HTTP GET (the dialog/event loop stays responsive).
// Returns the HTTP status code (0 = no connection / timeout).
static int HttpGet(const QString& URL, QByteArray& Data)
{
    QNetworkAccessManager Manager;
    QNetworkRequest Request{QUrl(URL)};
    Request.setRawHeader("User-Agent", "D-Fend Reborn");
    Request.setRawHeader("Accept", "application/vnd.github+json");
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *Reply = Manager.get(Request);
    QEventLoop Loop;
    QTimer Timeout;
    Timeout.setSingleShot(true);
    QObject::connect(&Timeout, &QTimer::timeout, Reply, &QNetworkReply::abort);
    QObject::connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
    Timeout.start(20000);
    Loop.exec();
    const int Status = Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    Data = Reply->readAll();
    Reply->deleteLater();
    return Status;
}

TUpdateResult RunUpdateCheck(QWidget *AOwner, bool Quite, bool QuiteOnError)
{
    const QString URL = UpdateCheckURL();
    QByteArray Data;
    int Status;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    Status = HttpGet(URL, Data);
    QApplication::restoreOverrideCursor();

    // 404: the project has no published release yet
    if (Status == 404) {
        PrgSetup->SetLastUpdateCheck(Round(std::trunc(Date())));
        if (!Quite) MessageDlg(LanguageSetup->UpdateNoUpdates(), mtInformation, mbOK, 0);
        return urNoUpdatesAvailable;
    }
    const QJsonObject Release = QJsonDocument::fromJson(Data).object();
    if (Status != 200 || Release.isEmpty()) {
        if (!QuiteOnError) MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {URL}), mtError, mbOK, 0);
        return urUpdateInstallCanceled;
    }

    PrgSetup->SetLastUpdateCheck(Round(std::trunc(Date())));

    // tag names like "v1.5.0" or "1.5.0"
    QString Version = Trim(Release.value("tag_name").toString());
    if (Version.startsWith('v', Qt::CaseInsensitive)) Version = Version.mid(1);
    const QString Page = Release.value("html_url").toString(DFRebornHomepage + "/releases");

    if (!Version.isEmpty() && VersionToInt(Version) > VersionToInt(GetReleaseVersionAsString())) {
        // "New version" X "on server. Do you want to update now ?"
        if (MessageDlg(LanguageSetup->UpdateNewVersionPart1() + " " + Version + " " + LanguageSetup->UpdateNewVersionPart2(), mtConfirmation, mbYesNo, 0) == mrYes) {
#ifdef Q_OS_WIN
            if (InstallUpdate(AOwner, Release)) return urUpdateAvailable;
#else
            Q_UNUSED(AOwner);
#endif
            ShellOpen(Page);
        }
        return urUpdateAvailable;
    }

    if (!Quite) MessageDlg(LanguageSetup->UpdateNoUpdates(), mtInformation, mbOK, 0);
    return urNoUpdatesAvailable;
}

void RunUpdateCheckIfSetup(QWidget *AOwner)
{
    switch (PrgSetup->CheckForUpdates()) {
        case 0: /* Do not check automatically */ break;
        case 1: if (Round(std::trunc(Date())) >= PrgSetup->LastUpdateCheck() + 7) RunUpdateCheck(AOwner, true, true); break;
        case 2: if (Round(std::trunc(Date())) >= PrgSetup->LastUpdateCheck() + 1) RunUpdateCheck(AOwner, true, true); break;
        case 3: RunUpdateCheck(AOwner, true, true); break;
    }
}

void RunUpdateCheckIdleCloseHandle()
{
    // (the original closed the process handle of UpdateCheck.exe here; the
    // port installs updates itself)
}

void ProgramUpdateCheckUnit_Initialization()
{
}

void ProgramUpdateCheckUnit_Finalization()
{
}
