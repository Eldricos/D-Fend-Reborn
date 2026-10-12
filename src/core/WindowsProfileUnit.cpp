// C++ port of WindowsProfileUnit.pas (implementation section).
#include "WindowsProfileUnit.h"

#include <QProcess>

#include "Process.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "DosBoxUnit.h"
#include "RunPrgManagerUnit.h"
#include "DOSBoxCountUnit.h"
#include "HistoryUnit.h"
#include "PrgConsts.h"
#include "MainUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

bool MinimizedAtWindowsGameStart = false;
TDateTime MinimizedAtWindowsGameStartTime = 0;

// THandle is a process id in the port (see Process.h); 0 = INVALID_HANDLE_VALUE.
static const THandle InvalidHandle = 0;

// --- Application (Forms.TApplication) helpers -------------------------------

static void ApplicationRestore()
{
    if (DFendReloadedMainForm == nullptr) return;
    if (DFendReloadedMainForm->isMinimized()) DFendReloadedMainForm->showNormal();
    DFendReloadedMainForm->activateWindow();
}

static void ApplicationMinimize()
{
    if (DFendReloadedMainForm != nullptr) DFendReloadedMainForm->showMinimized();
}

static bool MainFormIsMinimized()
{
    return (DFendReloadedMainForm != nullptr) && DFendReloadedMainForm->isMinimized();
}

// CreateProcess(PrgFile,'"'+PrgFile+'" '+Params,...,WorkDir,...) replacement.
// Returns the process id or 0.
static THandle CreateProcessCommandLine(const QString& PrgFile, const QString& Params, const QString& WorkDir)
{
    QProcess P;
    P.setProgram(ToNativePath(PrgFile));
#ifdef Q_OS_WIN
    P.setNativeArguments(Params);
#else
    P.setArguments(SplitCommandLine(Params));
#endif
    if (!WorkDir.isEmpty()) P.setWorkingDirectory(ToNativePath(WorkDir));
    qint64 Pid = 0;
    if (!P.startDetached(&Pid)) return 0;
    return Pid;
}

[[maybe_unused]] static THandle RunFile(const QString& FileName, const QString& Parameters)
{
    THandle result;
    // {ShellExecute(Application.MainForm.Handle,'open',PChar(FileName),PChar(Parameters),PChar(IncludeTrailingPathDelimiter((ExtractFilePath(FileName)))),SW_SHOW);}

    result = CreateProcessCommandLine(FileName, Parameters, ExtractFilePath(FileName));
    if (result == 0) {
        // if (GetLastError=740) {ERROR_ELEVATION_REQUIRED} then ShellExecute(...)
        if (!ShellOpen(FileName, Parameters, IncludeTrailingPathDelimiter(ExtractFilePath(FileName)))) {
            ApplicationRestore();
            MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {FileName}), mtError, mbOK, 0);
        }
        return InvalidHandle;
    }

    if (PrgSetup->MinimizeOnWindowsGameStart()) {
        Sleep(1000);
        // {MinimizedAtWindowsGameStart:=True; -> RunWindowsGame/RunWindowssExtraFile}
    }

    WindowsGameCounter->Add(result);
    return result;
}

static THandle RunFile2(const QString& FileName, const QString& Parameters, bool asAdmin)
{
    THandle result;
    QString S;

    if (asAdmin) {
        result = InvalidHandle;
        S = IncludeTrailingPathDelimiter(ExtractFilePath(FileName));
#ifdef Q_OS_WIN
        ShellOpen(PrgDir() + BinFolder + PathDelim + "AdminLauncher.exe", "/dir=" + S + " /run=\"" + FileName + "\" " + Parameters, S);
        return result;
#else
        // TODO(port): AdminLauncher.exe only exists on Windows - start the program normally
#endif
    }

#ifdef Q_OS_WIN
    SHELLEXECUTEINFOW ShellExecInfo;
    ZeroMemory(&ShellExecInfo, sizeof(ShellExecInfo));
    std::wstring WFile = ToNativePath(FileName).toStdWString();
    std::wstring WParams = Parameters.toStdWString();
    std::wstring WDir = ToNativePath(IncludeTrailingPathDelimiter(ExtractFilePath(FileName))).toStdWString();
    ShellExecInfo.cbSize = sizeof(ShellExecInfo);
    ShellExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShellExecInfo.hwnd = DFendReloadedMainForm ? reinterpret_cast<HWND>(DFendReloadedMainForm->winId()) : nullptr;
    ShellExecInfo.lpVerb = L"open";
    ShellExecInfo.lpFile = WFile.c_str();
    ShellExecInfo.lpParameters = WParams.c_str();
    ShellExecInfo.lpDirectory = WDir.c_str();
    ShellExecInfo.nShow = SW_SHOW;

    if (ShellExecuteExW(&ShellExecInfo)) {
        result = 0;
        if (ShellExecInfo.hProcess != nullptr) {
            result = static_cast<THandle>(GetProcessId(ShellExecInfo.hProcess));
            CloseHandle(ShellExecInfo.hProcess);
        }
    } else {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {FileName}), mtError, mbOK, 0);
        return InvalidHandle;
    }
#else
    result = StartProcess(FileName, SplitCommandLine(Parameters), IncludeTrailingPathDelimiter(ExtractFilePath(FileName)));
    if (result == 0) {
        // ShellExecuteEx can also open documents
        if (!ShellOpen(FileName, Parameters, IncludeTrailingPathDelimiter(ExtractFilePath(FileName)))) {
            ApplicationRestore();
            MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {FileName}), mtError, mbOK, 0);
            return InvalidHandle;
        }
    }
#endif

    if (PrgSetup->MinimizeOnWindowsGameStart()) {
        Sleep(1000);
        // {MinimizedAtWindowsGameStart:=True; -> RunWindowsGame/RunWindowssExtraFile}
    }

    WindowsGameCounter->Add(result);
    return result;
}

static bool WindowsRunCheck(QString& FileName)
{
    if (FileName.isEmpty()) {
        ApplicationRestore();
        MessageDlg(LanguageSetup->MessageNoFileName(), mtError, mbOK, 0);
        return false;
    }
    FileName = MakeAbsPath(FileName, PrgSetup->BaseDir());

    if (!FileExists(FileName)) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {FileName}), mtError, mbOK, 0);
        return false;
    }

    if (IsDOSExe(FileName)) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageDOSExeExecuteWarning(), {FileName}), mtError, mbOK, 0);
    }

    return true;
}

void RunWindowsGame(TGame* Game, bool RunSetup)
{
    QString S, T;
    THandle Handle;
    bool AlreadyMinimized;

    AlreadyMinimized = false;

    if (RunSetup) {
        S = Trim(Game->SetupExe());
        T = Game->SetupParameters();
    } else {
        S = Trim(Game->GameExe());
        T = Game->GameParameters();
    }

    if (!WindowsRunCheck(S)) return;
    if (!RunCheck(Game, RunSetup)) return;
    History->Add(Game->Name());

    RunPrgManager->RunBeforeExecutionCommand(Game);

    if (PrgSetup->MinimizeOnWindowsGameStart()) {
        AlreadyMinimized = MainFormIsMinimized();
        ApplicationMinimize();
    }

    Handle = RunFile2(S, T, Game->RunAsAdmin() && PrgSetup->OfferRunAsAdmin());
    RunPrgManager->AddCommand(Game, Handle);
    // CloseHandle(Handle) -> nothing to do for process ids

    if (PrgSetup->MinimizeOnWindowsGameStart() && (!AlreadyMinimized)) {
        MinimizedAtWindowsGameStart = true;
        MinimizedAtWindowsGameStartTime = Now();
    }
}

void RunWindowssExtraFile(TGame* Game, int Nr)
{
    QString S, T;
    int I;
    THandle Handle;
    bool AlreadyMinimized;

    AlreadyMinimized = false;

    S = Trim(Game->ExtraPrgFile(Nr)); I = Pos(";", S);
    if ((S.isEmpty()) || (I == 0)) {
        S = Trim(Game->GameExe()); T = Game->GameParameters();
    } else {
        S = Copy(S, I + 1, MaxInt);
        T = Game->ExtraPrgFileParameter(Nr);
    }

    if (!WindowsRunCheck(S)) return;
    if (!RunCheck(Game, false, Nr)) return;
    History->Add(Game->Name());

    RunPrgManager->RunBeforeExecutionCommand(Game);

    if (PrgSetup->MinimizeOnWindowsGameStart()) {
        AlreadyMinimized = MainFormIsMinimized();
        ApplicationMinimize();
    }

    Handle = RunFile2(S, T, Game->RunAsAdmin() && PrgSetup->OfferRunAsAdmin());
    RunPrgManager->AddCommand(Game, Handle);

    if (PrgSetup->MinimizeOnWindowsGameStart() && (!AlreadyMinimized)) {
        MinimizedAtWindowsGameStart = true;
        MinimizedAtWindowsGameStartTime = Now();
    }
}
