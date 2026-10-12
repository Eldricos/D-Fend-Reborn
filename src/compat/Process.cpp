#include "Process.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
#include <QUrl>

#ifdef Q_OS_WIN
#include "WinApi.h"
#else
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>
#endif

QStringList SplitCommandLine(const QString &CommandLine)
{
    QStringList R;
    QString Cur;
    bool InQuote = false, Has = false;
    for (int i = 0; i < CommandLine.length(); i++) {
        QChar C = CommandLine[i];
        if (C == '"') { InQuote = !InQuote; Has = true; continue; }
        if (C.isSpace() && !InQuote) {
            if (Has) { R << Cur; Cur.clear(); Has = false; }
            continue;
        }
        Cur += C;
        Has = true;
    }
    if (Has) R << Cur;
    return R;
}

THandle StartProcess(const QString &Program, const QStringList &Arguments, const QString &WorkingDir, TShowWindowMode Show)
{
    QProcess P;
    P.setProgram(ToNativePath(Program));
    P.setArguments(Arguments);
    QString Dir = WorkingDir.isEmpty() ? ExtractFilePath(Program) : WorkingDir;
    if (!Dir.isEmpty()) P.setWorkingDirectory(ToNativePath(Dir));
#ifdef Q_OS_WIN
    if (Show != swNormal) {
        P.setCreateProcessArgumentsModifier([Show](QProcess::CreateProcessArguments *A) {
            A->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            A->startupInfo->wShowWindow = Show == swHide ? SW_HIDE : Show == swMinimized ? SW_SHOWMINNOACTIVE : SW_SHOWMAXIMIZED;
            if (Show == swHide) A->flags |= CREATE_NO_WINDOW;
        });
    }
#else
    Q_UNUSED(Show);
#endif
    qint64 Pid = 0;
    if (!P.startDetached(&Pid)) return 0;
    return Pid;
}

THandle StartProcessCommandLine(const QString &CommandLine, const QString &WorkingDir, TShowWindowMode Show)
{
    QStringList L = SplitCommandLine(CommandLine);
    if (L.isEmpty()) return 0;
    QString Prg = L.takeFirst();
    return StartProcess(Prg, L, WorkingDir, Show);
}

int RunProcessAndWait(const QString &Program, const QStringList &Arguments, const QString &WorkingDir, bool HideWindow,
                      bool ProcessEventsWhileWaiting)
{
    QProcess P;
    QString Dir = WorkingDir.isEmpty() ? ExtractFilePath(Program) : WorkingDir;
    if (!Dir.isEmpty()) P.setWorkingDirectory(ToNativePath(Dir));
#ifdef Q_OS_WIN
    if (HideWindow) {
        P.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *A) {
            A->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            A->startupInfo->wShowWindow = SW_HIDE;
            A->flags |= CREATE_NO_WINDOW;
        });
    }
#else
    Q_UNUSED(HideWindow);
#endif
    P.setProcessChannelMode(QProcess::ForwardedChannels);
    P.start(ToNativePath(Program), Arguments);
    if (!P.waitForStarted()) return -1;
    while (!P.waitForFinished(ProcessEventsWhileWaiting ? 50 : -1)) {
        if (P.state() == QProcess::NotRunning) break;
        if (ProcessEventsWhileWaiting) QCoreApplication::processEvents();
    }
    return P.exitCode();
}

QString RunProcessAndGetOutput(const QString &Program, const QStringList &Arguments, const QString &WorkingDir, int TimeoutMs)
{
    QProcess P;
    if (!WorkingDir.isEmpty()) P.setWorkingDirectory(ToNativePath(WorkingDir));
    P.setProcessChannelMode(QProcess::MergedChannels);
    P.start(ToNativePath(Program), Arguments);
    if (!P.waitForStarted()) return QString();
    P.waitForFinished(TimeoutMs);
    return DecodeAnsi(P.readAll());
}

bool IsProcessRunning(THandle Pid)
{
    if (Pid <= 0) return false;
#ifdef Q_OS_WIN
    HANDLE H = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(Pid));
    if (!H) return false;
    DWORD R = WaitForSingleObject(H, 0);
    CloseHandle(H);
    return R == WAIT_TIMEOUT;
#else
    // reap the child if it is ours and already finished
    int Status;
    pid_t W = waitpid(static_cast<pid_t>(Pid), &Status, WNOHANG);
    if (W == static_cast<pid_t>(Pid)) return false;
    return kill(static_cast<pid_t>(Pid), 0) == 0 || errno == EPERM;
#endif
}

bool WaitForProcess(THandle Pid, int TimeoutMs, bool ProcessEventsWhileWaiting)
{
    QElapsedTimer T;
    T.start();
    while (IsProcessRunning(Pid)) {
        if (TimeoutMs >= 0 && T.elapsed() > TimeoutMs) return false;
        if (ProcessEventsWhileWaiting) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(50);
    }
    return true;
}

void TerminateProcessById(THandle Pid)
{
    if (Pid <= 0) return;
#ifdef Q_OS_WIN
    HANDLE H = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(Pid));
    if (H) { TerminateProcess(H, 1); CloseHandle(H); }
#else
    kill(static_cast<pid_t>(Pid), SIGTERM);
#endif
}

bool ShellOpen(const QString &FileOrUrl, const QString &Parameters, const QString &WorkingDir)
{
    if (FileOrUrl.isEmpty()) return false;
    QString F = FileOrUrl;
    bool IsUrl = F.contains("://") || F.startsWith("mailto:", Qt::CaseInsensitive);
    if (!Parameters.isEmpty() && !IsUrl) {
        QFileInfo FI(ToNativePath(F));
        if (FI.isExecutable() || FI.suffix().compare("exe", Qt::CaseInsensitive) == 0 ||
            FI.suffix().compare("bat", Qt::CaseInsensitive) == 0)
            return StartProcess(F, SplitCommandLine(Parameters), WorkingDir) != 0;
    }
    if (IsUrl) return QDesktopServices::openUrl(QUrl(F));
    return QDesktopServices::openUrl(QUrl::fromLocalFile(ToNativePath(F)));
}

bool OpenFolder(const QString &Folder)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(ToNativePath(Folder)));
}
