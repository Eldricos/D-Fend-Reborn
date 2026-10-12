// C++ port of ScreensaverControlUnit.pas (implementation section).
//
// The Delphi code duplicated the DOSBox process handle and polled it with
// WaitForSingleObject.  In the port THandle is the process id, so the list
// stores process ids and IsProcessRunning() is used for polling.  The screen
// saver itself is controlled by Get/SetScreensaverAllowStatus (CommonTools).
#include "ScreensaverControlUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"

#include <QTimer>

TScreensaverControl *ScreensaverControl = nullptr;

/* TScreensaverControl */

TScreensaverControl::TScreensaverControl()
{
    HandleList = new TList();
    Timer = new QTimer();
    QObject::connect(Timer, &QTimer::timeout, [this]() { TimerWork(Timer); });
    Timer->setInterval(1000);
    Timer->start();
}

TScreensaverControl::~TScreensaverControl()
{
    delete Timer;
    delete HandleList;
}

void TScreensaverControl::DOSBoxStarted(THandle DOSBoxHandle, bool DisableScreensaver)
{
    if (HandleList->size() == 0) OrigScreensaverStatus = GetScreensaverAllowStatus();
    // DuplicateHandle(GetCurrentProcess,DOSBoxHandle,...) - process ids need no duplication
    THandle NewHandle = DOSBoxHandle;
    HandleList->append(reinterpret_cast<void *>(static_cast<intptr_t>(NewHandle)));
    if ((HandleList->size() == 1) && DisableScreensaver) SetScreensaverAllowStatus(false);
}

void TScreensaverControl::TimerWork(QObject *Sender)
{
    Q_UNUSED(Sender);
    if (HandleList->size() == 0) return;

    int I = 0;
    while (I < HandleList->size()) {
        THandle H = static_cast<THandle>(reinterpret_cast<intptr_t>((*HandleList)[I]));
        if ((H != 0) && (!IsProcessRunning(H))) {
            HandleList->removeAt(I);
            continue;
        }
        I++;
    }

    if (HandleList->size() == 0) SetScreensaverAllowStatus(OrigScreensaverStatus);
}

void ScreensaverControlUnit_Initialization()
{
    ScreensaverControl = new TScreensaverControl();
}

void ScreensaverControlUnit_Finalization()
{
    delete ScreensaverControl;
    ScreensaverControl = nullptr;
}
