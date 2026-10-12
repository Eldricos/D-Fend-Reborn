// C++ port of RunPrgManagerUnit.pas (implementation section).
#include "RunPrgManagerUnit.h"

#include "Process.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"

TRunPrgManager* RunPrgManager = nullptr;

// THandle is a process id in the port (see Process.h); 0 = INVALID_HANDLE_VALUE.
static const THandle InvalidHandle = 0;

// { TRunPrgManager }

TRunPrgManager::TRunPrgManager()
{
    Timer = new QTimer();
    QObject::connect(Timer, &QTimer::timeout, Timer, [this]() { TimerWork(Timer); });
    Timer->setInterval(1000);
    Timer->start();
}

TRunPrgManager::~TRunPrgManager()
{
    delete Timer;
}

void TRunPrgManager::RunBeforeExecutionCommand(TGame* AGame)
{
    RunCommands(AGame->CommandBeforeExecution(), AGame->CommandBeforeExecutionWait(), AGame->CommandBeforeExecutionParallel(), AGame->CommandBeforeExecutionMinimized());
}

void TRunPrgManager::AddCommand(TGame* AGame, THandle AHandle)
{
    QString S;
    THandle NewHandle;

    S = Trim(AGame->CommandAfterExecution());
    if (S.isEmpty()) return;

    TRunPrgRecord R;
    R.Commands = S;
    R.CommandMinimized = AGame->CommandAfterExecutionMinimized();
    R.CommandParallel = AGame->CommandAfterExecutionParallel();
    NewHandle = AHandle;  // DuplicateHandle -> process id can be used directly
    R.WaitHandle = NewHandle;
    List.append(R);
}

int TRunPrgManager::GetCount()
{
    return List.size();
}

TRunPrgRecord TRunPrgManager::GetData(int I)
{
    return List[I];
}

void TRunPrgManager::TimerWork(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    Timer->stop();
    try {
        I = 0;
        while (I < List.size()) {
            if ((List[I].WaitHandle != InvalidHandle) && (!IsProcessRunning(List[I].WaitHandle))) {
                TRunPrgRecord R = List[I];
                RunCommands(R.Commands, false, R.CommandParallel, R.CommandMinimized);
                List.removeAt(I);
                continue;
            }
            I++;
        }
    } catch (...) {
        Timer->start();
        throw;
    }
    Timer->start();
}

THandle TRunPrgManager::RunCommandAndGetHandle(const QString& Command, bool Minimized)
{
    THandle result = InvalidHandle;

    // CreateProcess(nil,PChar(Command),...) with STARTF_USESHOWWINDOW/SW_MINIMIZE if Minimized
    THandle H = StartProcessCommandLine(Command, GetCurrentDir(), Minimized ? swMinimized : swNormal);
    if (H == 0) {
        // If GetLastError=ERROR_BAD_EXE_FORMAT then ShellExecute(...,'open',Command,...)
        if (ShellOpen(Command)) return result;
        MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {Command}), mtError, mbOK, 0);
        return result;
    }

    result = H;
    return result;
}

void TRunPrgManager::RunCommand(const QString& Command, bool Wait, bool Minimized)
{
    THandle H;
    H = RunCommandAndGetHandle(Command, Minimized);
    if (H != InvalidHandle) {
        if (Wait) WaitForProcess(H);
    }
}

void TRunPrgManager::RunCommands(const QString& Commands, bool Wait, bool Parallel, bool Minimized)
{
    TStringList* St;
    QString S;
    int I;
    THandle H;
    bool B;

    St = StringToStringList(Commands);
    try {
        if (Parallel) {
            QList<THandle> L;
            for (I = 0; I < St->Count(); I++) {
                S = Trim((*St)[I]);
                if (S.isEmpty()) continue;
                H = RunCommandAndGetHandle(S, Minimized);
                if (H != InvalidHandle) L.append(H);
            }
            if (Wait) {
                do {
                    B = true;
                    for (I = 0; I < L.size(); I++) if (!WaitForProcess(L[I], 100)) B = false;
                } while (!B);
            }
        } else {
            I = 0; while (I < St->Count()) if (Trim((*St)[I]).isEmpty()) St->Delete(I); else I++;
            for (I = 0; I < St->Count(); I++) {
                RunCommand(Trim((*St)[I]), Wait || (I < St->Count() - 1), Minimized);
            }
        }
    } catch (...) {
        delete St;
        throw;
    }
    delete St;
}

void RunPrgManagerUnit_Initialization()
{
    RunPrgManager = new TRunPrgManager();
}

void RunPrgManagerUnit_Finalization()
{
    delete RunPrgManager;
    RunPrgManager = nullptr;
}
