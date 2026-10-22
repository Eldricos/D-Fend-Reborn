// Helpers of the main window code (MainUnit.cpp, MainUnitMenuWork.cpp)
#pragma once

#include <QAction>
#include "GameDBUnit.h"
#include "ComCtrls.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

// (Sender as TComponent).Tag
inline int Tag(QObject *O)
{
    return O ? O->property("tag").toInt() : 0;
}

// (Sender as TMenuItem).Hint
inline QString ActionHint(QObject *Sender)
{
    if (!Sender) return QString();
    QVariant V = Sender->property("hint");
    if (V.isValid()) return V.toString();
    auto *A = qobject_cast<QAction *>(Sender);
    if (!A) return QString();
    // QAction::toolTip() returns the text when no tool tip was set
    QString T = A->text();
    T.remove('&');
    if (A->toolTip() == T) return QString();
    return A->toolTip();
}

// SetProcessWorkingSetSize(GetCurrentProcess,$ffffffff,$ffffffff)
inline void TrimWorkingSet()
{
#ifdef Q_OS_WIN
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
}

inline TGame *GameOf(TListItem *Item)
{
    return Item ? static_cast<TGame *>(Item->Data()) : nullptr;
}
