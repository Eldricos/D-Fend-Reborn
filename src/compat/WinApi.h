// Include this instead of <windows.h>.  It removes the Win32 A/W macros that
// clash with Delphi routine and method names used in the port.
// In Windows specific code call the explicit wide API instead, e.g.
// CreateFileW, ShellExecuteW, GetShortPathNameW, MessageBoxW.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#undef DeleteFile
#undef CopyFile
#undef GetEnvironmentVariable
#undef CreateDirectory
#undef GetCurrentDirectory
#undef MessageBox
#undef FindText
#undef Beep
#undef CreateFile
#undef GetObject
#undef GetVersion
#undef GetMessage
#undef LoadIcon
#undef LoadImage
#undef GetClassName
#undef DrawText
#undef CreateFont
#undef SendMessage
#undef PostMessage
#undef GetUserName
#undef MoveFile
#undef RegisterClass
#undef GetFileAttributes
#undef GetTempPath
#undef GetModuleFileName
#undef CreateProcess
#undef ShellExecute
#undef GetCommandLine
#undef GetComputerName
#undef LoadString
#undef FindResource
#undef CreateWindow
#undef CreateMutex
#undef OpenMutex
#undef CreateEvent
#undef RemoveDirectory
#undef SetCurrentDirectory
#undef GetShortPathName
#undef GetLongPathName
#undef StartService
#undef GetTextMetrics
#undef GetFreeSpace
#endif
