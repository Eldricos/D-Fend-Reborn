// Starting external programs (CreateProcess / ShellExecute / WinExec
// replacements).  Processes are started detached so that DOSBox, ScummVM etc.
// keep running when D-Fend Reborn is closed - exactly like the original.
#pragma once

#include <functional>
#include "Classes.h"

class QUrl;

// D-Fend Reborn addition: the tests set this hook so that no program is
// started and no document, URL or folder is opened: the hook gets the
// program / document and the functions below report a failure.
using TExternalStartHook = std::function<void(const QString &What)>;
extern TExternalStartHook ExternalStartHook;

enum TShowWindowMode { swNormal, swHide, swMinimized, swMaximized };

// Starts Program with the given (already split) arguments.  Returns the process
// id (THandle) or 0 on failure.
THandle StartProcess(const QString &Program, const QStringList &Arguments, const QString &WorkingDir = QString(),
                     TShowWindowMode Show = swNormal);
// Starts a command line ("prog.exe" -a -b) - the Delphi code builds those.
THandle StartProcessCommandLine(const QString &CommandLine, const QString &WorkingDir = QString(),
                                TShowWindowMode Show = swNormal);
// CreateProcess(PrgFile,'"'+PrgFile+'" '+Params,...,WorkDir,...) replacement:
// Params is a command line parameter string as built by the Delphi code
// (passed unchanged on Windows, split on other systems). EnvAdd: additional
// "NAME=VALUE" environment entries. Returns the process id or 0.
THandle StartProcessNative(const QString &PrgFile, const QString &Params, const QString &WorkDir = QString(),
                           const QStringList &EnvAdd = QStringList());
// Runs a program and waits until it terminates (RunAndWait).  Returns the exit code.
int RunProcessAndWait(const QString &Program, const QStringList &Arguments, const QString &WorkingDir = QString(),
                      bool HideWindow = false, bool ProcessEventsWhileWaiting = true);
// Runs a program and returns its standard output (RunAndGetOutput)
QString RunProcessAndGetOutput(const QString &Program, const QStringList &Arguments, const QString &WorkingDir = QString(),
                               int TimeoutMs = 30000);

bool IsProcessRunning(THandle Pid);
bool WaitForProcess(THandle Pid, int TimeoutMs = -1, bool ProcessEventsWhileWaiting = true);
void TerminateProcessById(THandle Pid);

// ShellExecute(..., 'open', FileName, ...) - opens a document / URL / folder
// with the associated application.
bool ShellOpen(const QString &FileOrUrl, const QString &Parameters = QString(), const QString &WorkingDir = QString());
// Opens the folder in the file manager (explorer.exe Folder)
bool OpenFolder(const QString &Folder);
// QDesktopServices::openUrl (with ExternalStartHook)
bool OpenDesktopUrl(const QUrl &Url);

// Splits a Windows style command line into program + arguments
QStringList SplitCommandLine(const QString &CommandLine);
