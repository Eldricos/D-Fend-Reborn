// Starting external programs (CreateProcess / ShellExecute / WinExec
// replacements).  Processes are started detached so that DOSBox, ScummVM etc.
// keep running when D-Fend Reborn is closed - exactly like the original.
#pragma once

#include "Classes.h"

enum TShowWindowMode { swNormal, swHide, swMinimized, swMaximized };

// Starts Program with the given (already split) arguments.  Returns the process
// id (THandle) or 0 on failure.
THandle StartProcess(const QString &Program, const QStringList &Arguments, const QString &WorkingDir = QString(),
                     TShowWindowMode Show = swNormal);
// Starts a command line ("prog.exe" -a -b) - the Delphi code builds those.
THandle StartProcessCommandLine(const QString &CommandLine, const QString &WorkingDir = QString(),
                                TShowWindowMode Show = swNormal);
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

// Splits a Windows style command line into program + arguments
QStringList SplitCommandLine(const QString &CommandLine);
