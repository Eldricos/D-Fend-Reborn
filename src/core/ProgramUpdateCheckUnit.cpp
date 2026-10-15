// C++ port of ProgramUpdateCheckUnit.pas (implementation section).
//
// D-Fend Reborn: the update check asks GitHub for the latest release of the
// project (DFRebornLatestReleaseAPI). If there is a newer version:
// - Windows: like the original, the external Bin\UpdateCheck.exe is started
//   (tools/UpdateCheck). It reads UpdateInfo.txt of the latest release
//   (version and URL of the update installer), asks the user, downloads the
//   update installer and runs it for the program folder.
// - other systems (or without UpdateCheck.exe): the user is asked whether
//   to open the release page in the browser.
#include "ProgramUpdateCheckUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "Process.h"
#include "MainUnit.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <cmath>

// URL of the update information; settings files of older versions may still
// contain the address of the original D-Fend Reloaded update file.
static QString UpdateCheckURL()
{
    QString URL = Trim(PrgSetup->UpdateCheckURL());
    if (URL.isEmpty() || URL.contains("UpdateInfo.txt", Qt::CaseInsensitive)) URL = DFRebornLatestReleaseAPI;
    return URL;
}

#ifdef Q_OS_WIN
// URL of the update information of the latest release (asset of each
// release, created by the release workflow)
static const QString UpdateInfoURL = DFRebornHomepage + "/releases/latest/download/UpdateInfo.txt";

// Starts UpdateCheck.exe (RunExternalUpdateCheck of the original). Returns
// false if UpdateCheck.exe is missing.
static bool RunExternalUpdateCheck(bool Quite)
{
    QString Prg;
    if (FileExists(PrgDir() + "UpdateCheck.exe")) Prg = PrgDir() + "UpdateCheck.exe";
    else if (FileExists(PrgDir() + BinFolder + PathDelim + "UpdateCheck.exe")) Prg = PrgDir() + BinFolder + PathDelim + "UpdateCheck.exe";
    if (Prg.isEmpty()) return false;

    const QString FileName = TempDir() + "UpdateCheckSetup.txt";
    TStringList St;
    St.Add(GetReleaseVersionAsString());
    St.Add(UpdateInfoURL);
    St.Add("DFendRebornUpdate.exe");
    St.Add(PrgDir());
    St.Add(Quite ? "silent" : "normal");
    St.Add(LanguageSetup->UpdateCannotFindFile());
    St.Add(LanguageSetup->UpdateDownloadFailed());
    St.Add(LanguageSetup->UpdateDownloadFailedBeforeURL());
    St.Add(LanguageSetup->UpdateDownloadFailedAfterURL());
    St.Add(LanguageSetup->UpdateURL());
    St.Add(LanguageSetup->UpdateDownloading());
    St.Add(LanguageSetup->UpdateConnecting());
    St.Add(LanguageSetup->UpdateFileName());
    St.Add(LanguageSetup->UpdateTransfered());
    St.Add(LanguageSetup->UpdateFileSize());
    St.Add(LanguageSetup->UpdateRamainingTime());
    St.Add(LanguageSetup->UpdateTotalTime());
    St.Add(LanguageSetup->UpdateCannotReadFile());
    St.Add(LanguageSetup->UpdateNoUpdates());
    St.Add(LanguageSetup->UpdateNewVersionPart1());
    St.Add(LanguageSetup->UpdateNewVersionPart2());
    St.Add(LanguageSetup->OK());
    St.Add(LanguageSetup->Yes());
    St.Add(LanguageSetup->No());
    // UpdateCheck.exe is an ANSI program: system code page
    if (!St.SaveToFile(FileName)) return false;

    if (StartProcess(Prg, {FileName}, PrgDir()) == 0) return false;
    if (DFendRebornMainForm != nullptr && DFendRebornMainForm->DeleteOnExit != nullptr && DFendRebornMainForm->DeleteOnExit->IndexOf(FileName) < 0)
        DFendRebornMainForm->DeleteOnExit->Add(FileName);
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
    Q_UNUSED(AOwner);
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
#ifdef Q_OS_WIN
        // UpdateCheck.exe asks the user and installs the update
        if (RunExternalUpdateCheck(Quite)) return urUpdateAvailable;
#endif
        // "New version" X "on server. Do you want to update now ?"
        if (MessageDlg(LanguageSetup->UpdateNewVersionPart1() + " " + Version + " " + LanguageSetup->UpdateNewVersionPart2(), mtConfirmation, mbYesNo, 0) == mrYes)
            ShellOpen(Page);
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
    // port does not keep it)
}

void ProgramUpdateCheckUnit_Initialization()
{
}

void ProgramUpdateCheckUnit_Finalization()
{
}
