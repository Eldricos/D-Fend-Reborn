// C++ port of ProgramUpdateCheckUnit.pas (implementation section).
#include "ProgramUpdateCheckUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "DownloadWaitFormUnit.h"
#include "LanguageSetupUnit.h"
#include "PrgConsts.h"
#include "MainUnit.h"

#include <cmath>

// INVALID_HANDLE_VALUE -> 0 (THandle is a process id in the port)
static THandle UpdaterProcessHandleProcess = 0;

static void RunExternalUpdateCheck(bool Quite)
{
    QString FileName, Add, Prg;

    if ((UpdaterProcessHandleProcess != 0) && (!IsProcessRunning(UpdaterProcessHandleProcess))) {
        UpdaterProcessHandleProcess = 0;
    }
    if (UpdaterProcessHandleProcess != 0) {
        MessageDlg(LanguageSetup->UpdateSingleInstanceMessage(), mtError, mbOK, 0);
        return;
    }

    FileName = TempDir() + "UpdateCheckSetup.txt";

    {
        TStringList St;
        St.Add(GetNormalFileVersionAsString());
        if (PrgSetup->VersionSpecificUpdateCheck()) Add = "?Version=" + GetNormalFileVersionAsString(); else Add = "";
        St.Add(PrgSetup->UpdateCheckURL() + Add);
        St.Add("DFendReloadedUpdate.exe");
        St.Add(PrgDir());
        if (Quite) St.Add("silent"); else St.Add("normal");
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

        St.SaveToFile(FileName);
    }

    Prg = "";
    if (FileExists(PrgDir() + "UpdateCheck.exe")) Prg = PrgDir() + "UpdateCheck.exe"; else {
        if (FileExists(PrgDir() + BinFolder + PathDelim + "UpdateCheck.exe")) Prg = PrgDir() + BinFolder + PathDelim + "UpdateCheck.exe";
    }
    if (Prg == "") {
        if (Quite) return;
        ShellOpen(LanguageSetup->MenuHelpUpdatesURL());
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {PrgDir() + BinFolder + PathDelim + "UpdateCheck.exe"}), mtError, mbOK, 0);
        return;
    }

    // CreateProcess(PChar(Prg),PChar('"'+Prg+'" '+FileName),...,PChar(PrgDir),...)
    THandle Pid = StartProcess(Prg, QStringList() << FileName, PrgDir());
    if (Pid != 0) {
        UpdaterProcessHandleProcess = Pid;
    }

    if (DFendReloadedMainForm != nullptr && DFendReloadedMainForm->DeleteOnExit != nullptr) {
        if (DFendReloadedMainForm->DeleteOnExit->IndexOf(FileName) < 0) DFendReloadedMainForm->DeleteOnExit->Add(FileName);
    }

    PrgSetup->SetLastUpdateCheck(Round(std::trunc(Date())));
}

TUpdateResult RunUpdateCheck(QWidget *AOwner, bool Quite, bool QuiteOnError)
{
    QString URL, FileName;
    bool B;
    TUpdateResult result = urUpdateInstallCanceled;

    FileName = TempDir() + "UpdateCheckSetup.txt";
    URL = PrgSetup->UpdateCheckURL(); if (PrgSetup->VersionSpecificUpdateCheck()) URL = URL + "?Version=" + GetNormalFileVersionAsString();

    if (Quite) {
        B = (DownloadFileWithOutDialog(AOwner, -1, "", URL, "", FileName) == drSuccess);
    } else {
        B = (DownloadFileWithDialog(AOwner, -1, "", URL, "", FileName) == drSuccess);
    }

    if (!B) {
        if (!QuiteOnError) MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {URL}), mtError, mbOK, 0);
        return result;
    }

    TStringList St;
    if (!St.LoadFromFile(FileName)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotOpenFile(), {FileName}), mtError, mbOK, 0); return result;
    }
    ExtDeleteFile(FileName, ftTemp);

    PrgSetup->SetLastUpdateCheck(Round(std::trunc(Date())));

    if ((St.Count() > 1) && (VersionToInt(St[0]) > VersionToInt(GetNormalFileVersionAsString()))) {
        result = urUpdateAvailable;
        RunExternalUpdateCheck(Quite);
    } else {
        result = urNoUpdatesAvailable;
        if (Quite) return result;
        MessageDlg(LanguageSetup->UpdateNoUpdates(), mtInformation, mbOK, 0);
    }
    return result;
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
    if ((UpdaterProcessHandleProcess != 0) && (!IsProcessRunning(UpdaterProcessHandleProcess))) {
        UpdaterProcessHandleProcess = 0;
    }
}

void ProgramUpdateCheckUnit_Initialization()
{
}

void ProgramUpdateCheckUnit_Finalization()
{
    // CloseHandle(UpdaterProcessHandleProcess) - nothing to release for a process id
    UpdaterProcessHandleProcess = 0;
}
