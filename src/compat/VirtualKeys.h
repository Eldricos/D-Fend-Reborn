// Windows / Delphi virtual key codes (VK_*) used by the OnKeyDown handlers of
// the ported forms, and the conversion of Qt key codes to them.
#pragma once

#include <QtGlobal>
#include <Qt>

#ifdef _WIN32
#include "WinApi.h"   // <windows.h> defines the VK_* constants
#else
constexpr quint16 VK_BACK = 0x08;
constexpr quint16 VK_TAB = 0x09;
constexpr quint16 VK_RETURN = 0x0D;
constexpr quint16 VK_ESCAPE = 0x1B;
constexpr quint16 VK_SPACE = 0x20;
constexpr quint16 VK_PRIOR = 0x21;
constexpr quint16 VK_NEXT = 0x22;
constexpr quint16 VK_END = 0x23;
constexpr quint16 VK_HOME = 0x24;
constexpr quint16 VK_LEFT = 0x25;
constexpr quint16 VK_UP = 0x26;
constexpr quint16 VK_RIGHT = 0x27;
constexpr quint16 VK_DOWN = 0x28;
constexpr quint16 VK_INSERT = 0x2D;
constexpr quint16 VK_DELETE = 0x2E;
constexpr quint16 VK_F1 = 0x70;
constexpr quint16 VK_F2 = 0x71;
constexpr quint16 VK_F3 = 0x72;
constexpr quint16 VK_F4 = 0x73;
constexpr quint16 VK_F5 = 0x74;
constexpr quint16 VK_F6 = 0x75;
constexpr quint16 VK_F7 = 0x76;
constexpr quint16 VK_F8 = 0x77;
constexpr quint16 VK_F9 = 0x78;
constexpr quint16 VK_F10 = 0x79;
constexpr quint16 VK_F11 = 0x7A;
constexpr quint16 VK_F12 = 0x7B;
#endif

// Qt key code -> virtual key code. Letters and digits have the same codes in
// both systems; keys without a virtual key code give 0.
inline quint16 QtKeyToVK(int Key)
{
    if ((Key >= Qt::Key_0 && Key <= Qt::Key_9) || (Key >= Qt::Key_A && Key <= Qt::Key_Z)) return quint16(Key);
    if (Key >= Qt::Key_F1 && Key <= Qt::Key_F12) return quint16(VK_F1 + (Key - Qt::Key_F1));
    switch (Key) {
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Tab: return VK_TAB;
        case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
        case Qt::Key_Escape: return VK_ESCAPE;
        case Qt::Key_Space: return VK_SPACE;
        case Qt::Key_PageUp: return VK_PRIOR;
        case Qt::Key_PageDown: return VK_NEXT;
        case Qt::Key_End: return VK_END;
        case Qt::Key_Home: return VK_HOME;
        case Qt::Key_Left: return VK_LEFT;
        case Qt::Key_Up: return VK_UP;
        case Qt::Key_Right: return VK_RIGHT;
        case Qt::Key_Down: return VK_DOWN;
        case Qt::Key_Insert: return VK_INSERT;
        case Qt::Key_Delete: return VK_DELETE;
        default: return 0;
    }
}
