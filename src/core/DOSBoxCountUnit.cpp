// C++ port of DOSBoxCountUnit.pas (implementation section).
#include "DOSBoxCountUnit.h"

#include "Process.h"

TPrgCounter* DOSBoxCounter = nullptr;
TPrgCounter* ScummVMCounter = nullptr;
TPrgCounter* WindowsGameCounter = nullptr;

quint32 LastDOSBoxStartTime = 0;
TGame* LastDOSBoxProfile = nullptr;

// In the port THandle is the process id of the started program (see
// Process.h), so the list stores process ids instead of duplicated process
// handles.  Nothing has to be closed.

// { TPrgCounter }

TPrgCounter::TPrgCounter()
{
    FList = new TList();
}

TPrgCounter::~TPrgCounter()
{
    // For I:=0 to FList.Count-1 do CloseHandle(THandle(FList[I])); -> process ids need no closing
    delete FList;
}

void TPrgCounter::Add(THandle DOSBoxHandle)
{
    // DuplicateHandle(...) -> the process id can be stored directly
    THandle NewHandle = DOSBoxHandle;
    if (NewHandle > 0) FList->append(reinterpret_cast<void*>(static_cast<intptr_t>(NewHandle)));
}

int TPrgCounter::GetCount()
{
    int I = 0;
    while (I < FList->count()) {
        if (!IsProcessRunning(static_cast<THandle>(reinterpret_cast<intptr_t>((*FList)[I])))) {
            FList->removeAt(I);
            continue;
        }
        I++;
    }

    return FList->count();
}

void DOSBoxCountUnit_Initialization()
{
    DOSBoxCounter = new TPrgCounter();
    ScummVMCounter = new TPrgCounter();
    WindowsGameCounter = new TPrgCounter();
}

void DOSBoxCountUnit_Finalization()
{
    delete DOSBoxCounter; DOSBoxCounter = nullptr;
    delete ScummVMCounter; ScummVMCounter = nullptr;
    delete WindowsGameCounter; WindowsGameCounter = nullptr;
}
