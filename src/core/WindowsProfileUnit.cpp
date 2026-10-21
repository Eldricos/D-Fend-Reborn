// C++ port of WindowsProfileUnit.pas (implementation section).
#include "WindowsProfileUnit.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

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
    if (DFendRebornMainForm == nullptr) return;
    if (DFendRebornMainForm->isMinimized()) DFendRebornMainForm->showNormal();
    DFendRebornMainForm->activateWindow();
}

static void ApplicationMinimize()
{
    if (DFendRebornMainForm != nullptr) DFendRebornMainForm->showMinimized();
}

static bool MainFormIsMinimized()
{
    return (DFendRebornMainForm != nullptr) && DFendRebornMainForm->isMinimized();
}

[[maybe_unused]] static THandle RunFile(const QString& FileName, const QString& Parameters)
{
    THandle result;
    // {ShellExecute(Application.MainForm.Handle,'open',PChar(FileName),PChar(Parameters),PChar(IncludeTrailingPathDelimiter((ExtractFilePath(FileName)))),SW_SHOW);}

    result = StartProcessNative(FileName, Parameters, ExtractFilePath(FileName));
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

#ifndef Q_OS_WIN
// D-Fend Reborn: Windows program files are started with the program set in
// the options (default: Wine) if it is installed. Returns the program and its
// arguments up to the file name; empty: start the file directly.
static QStringList WindowsProgramsRunnerCommand(const QString& FileName)
{
    if (!PrgSetup->UseWindowsProgramsRunner()) return {};
    const QString Ext = ExtUpperCase(ExtractFileExt(FileName));
    const bool IsProgram = (Ext == ".EXE" || Ext == ".COM");
    const bool IsOther = (Ext == ".BAT" || Ext == ".CMD" || Ext == ".MSI");
    if (!IsProgram && !IsOther) return {};

    QStringList Cmd = SplitCommandLine(Trim(PrgSetup->WindowsProgramsRunner()));
    if (Cmd.isEmpty()) return {};
    QString Program = Cmd[0];
    if (QFileInfo(Program).isAbsolute()) {
        if (!QFileInfo(Program).isExecutable()) return {};
    } else {
        Program = QStandardPaths::findExecutable(Program);
        if (Program.isEmpty()) return {};
    }
    Cmd[0] = Program;
    // batch files and installer packages are opened by Wine's "start"
    // ("/wait": the process ends with the program, like for .exe files)
    if (IsOther) Cmd << "start" << "/wait" << "/unix";
    Cmd << FileName;
    return Cmd;
}
#endif

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
        // Port note: AdminLauncher.exe only exists on Windows - start the program normally
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
    ShellExecInfo.hwnd = DFendRebornMainForm ? reinterpret_cast<HWND>(DFendRebornMainForm->winId()) : nullptr;
    ShellExecInfo.lpVerb = L"open";
    ShellExecInfo.lpFile = WFile.c_str();
    ShellExecInfo.lpParameters = WParams.c_str();
    ShellExecInfo.lpDirectory = WDir.c_str();
    ShellExecInfo.nShow = SW_SHOW;

    if (ExternalStartHook) { ExternalStartHook(FileName); return InvalidHandle; }
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
    const QString WorkDir = IncludeTrailingPathDelimiter(ExtractFilePath(FileName));
    QStringList Runner = WindowsProgramsRunnerCommand(FileName);
    if (!Runner.isEmpty()) {
        const QString Program = Runner.takeFirst();
        result = StartProcess(Program, Runner + SplitCommandLine(Parameters), WorkDir);
    } else {
        result = StartProcess(FileName, SplitCommandLine(Parameters), WorkDir);
    }
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
