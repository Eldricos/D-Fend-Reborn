// Delphi RTL (SysUtils / StrUtils / DateUtils / FileCtrl) compatibility layer.
//
// The C++ port of D-Fend Reloaded keeps the structure of the original Delphi
// code.  To make the translation as direct (and therefore as correct) as
// possible the commonly used RTL routines are provided here with their Delphi
// names and Delphi semantics.  In particular string positions are 1-based just
// like in Pascal (Copy, Pos, Delete, Insert).
#pragma once

#ifdef _WIN32
// Include <windows.h> before anything else, so that WinApi.h can remove the
// Win32 macros that clash with Delphi names once and for all (a later
// #include <windows.h> from Qt or system headers is then a no-op).
#include "WinApi.h"
#endif

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QDateTime>
#include <QDir>
#include <initializer_list>
#include <cstdint>

// windows.h defines macros with the same names as several Delphi routines.
// Always include windows.h through "WinApi.h" which removes them again.
#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef GetEnvironmentVariable
#undef GetEnvironmentVariable
#endif
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif
#ifdef MessageBox
#undef MessageBox
#endif
#ifdef FindText
#undef FindText
#endif
#ifdef Beep
#undef Beep
#endif
#ifdef CreateFile
#undef CreateFile
#endif
#ifdef GetObject
#undef GetObject
#endif
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef PostMessage
#undef PostMessage
#endif
#ifdef GetUserName
#undef GetUserName
#endif
#ifdef LoadIcon
#undef LoadIcon
#endif
#ifdef LoadImage
#undef LoadImage
#endif
#ifdef DrawText
#undef DrawText
#endif
#ifdef CreateFont
#undef CreateFont
#endif
#ifdef GetClassName
#undef GetClassName
#endif
#ifdef GetMessage
#undef GetMessage
#endif
#ifdef GetVersion
#undef GetVersion
#endif
#ifdef MoveFile
#undef MoveFile
#endif
#ifdef RegisterClass
#undef RegisterClass
#endif

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------

using TDateTime = double;   // days since 1899-12-30 (Delphi TDateTime)
using DWord = quint32;
using Cardinal = quint32;
using Int64 = qint64;
constexpr int MaxInt = 2147483647;
constexpr qint64 MaxInt64 = Q_INT64_C(9223372036854775807);

// Native path delimiter ('\' on Windows, '/' elsewhere).  Use this wherever the
// Delphi code used a literal '\' inside a file system path.
extern const QString PathDelim;
extern const QChar PathDelimChar;
extern const QString LineBreak;  // #13#10 on Windows, #10 elsewhere

// ---------------------------------------------------------------------------
// String routines (1-based positions, Delphi semantics)
// ---------------------------------------------------------------------------

int Length(const QString &S);
QString Copy(const QString &S, int Index, int Count = 0x7FFFFFFF);
int Pos(const QString &SubStr, const QString &S);
int Pos(QChar C, const QString &S);
int PosEx(const QString &SubStr, const QString &S, int Offset = 1);
void Delete(QString &S, int Index, int Count);
void Insert(const QString &Source, QString &S, int Index);
QString StringOfChar(QChar C, int Count);

QString Trim(const QString &S);
QString TrimLeft(const QString &S);
QString TrimRight(const QString &S);
QString UpperCase(const QString &S);
QString LowerCase(const QString &S);
inline QString AnsiUpperCase(const QString &S) { return UpperCase(S); }
inline QString AnsiLowerCase(const QString &S) { return LowerCase(S); }
int CompareText(const QString &S1, const QString &S2);
int CompareStr(const QString &S1, const QString &S2);
inline int AnsiCompareText(const QString &S1, const QString &S2) { return CompareText(S1, S2); }
inline int AnsiCompareStr(const QString &S1, const QString &S2) { return CompareStr(S1, S2); }
bool SameText(const QString &S1, const QString &S2);
bool AnsiSameText(const QString &S1, const QString &S2);
bool StartsText(const QString &SubText, const QString &Text);
bool EndsText(const QString &SubText, const QString &Text);
bool ContainsText(const QString &Text, const QString &SubText);
QString QuotedStr(const QString &S);
QString AnsiDequotedStr(const QString &S, QChar Quote);

enum TReplaceFlag { rfReplaceAll = 1, rfIgnoreCase = 2 };
QString StringReplace(const QString &S, const QString &OldPattern, const QString &NewPattern, int Flags);

QString IntToStr(qint64 Value);
QString IntToHex(qint64 Value, int Digits);
int StrToInt(const QString &S);                    // throws EConvertError on failure
int StrToIntDef(const QString &S, int Default);
qint64 StrToInt64Def(const QString &S, qint64 Default);
bool TryStrToInt(const QString &S, int &Value);
double StrToFloat(const QString &S);               // throws EConvertError on failure
double StrToFloatDef(const QString &S, double Default);
bool TryStrToFloat(const QString &S, double &Value);
QString FloatToStr(double Value);
QString BoolToStr(bool B, bool UseBoolStrs = false);
bool StrToBoolDef(const QString &S, bool Default);

// Delphi Format(): supports %s %d %u %x %f %e %g %n %m %p %% with index,
// width and precision ("%.2d", "%-10s", "%0:s" ...).
QString Format(const QString &Fmt, std::initializer_list<QVariant> Args);
QString Format(const QString &Fmt, const QList<QVariant> &Args);

// Character sets used in the Delphi code ("C in ['a'..'z']")
bool CharInSet(QChar C, const QString &Chars);

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------

class Exception {
public:
    explicit Exception(const QString &AMessage) : Message(AMessage) {}
    virtual ~Exception() = default;
    QString Message;
};
class EConvertError : public Exception { using Exception::Exception; };
class EAbort : public Exception { using Exception::Exception; };

// ---------------------------------------------------------------------------
// File names
// ---------------------------------------------------------------------------

QString IncludeTrailingPathDelimiter(const QString &S);
QString ExcludeTrailingPathDelimiter(const QString &S);
inline QString IncludeTrailingBackslash(const QString &S) { return IncludeTrailingPathDelimiter(S); }
inline QString ExcludeTrailingBackslash(const QString &S) { return ExcludeTrailingPathDelimiter(S); }
QString ExtractFilePath(const QString &FileName);   // including trailing delimiter
QString ExtractFileDir(const QString &FileName);    // without trailing delimiter
QString ExtractFileName(const QString &FileName);
QString ExtractFileExt(const QString &FileName);    // including '.'
QString ExtractFileDrive(const QString &FileName);
QString ChangeFileExt(const QString &FileName, const QString &Extension);
QString ExpandFileName(const QString &FileName);
QString ExtractRelativePath(const QString &BaseName, const QString &DestName);
QString ToNativePath(const QString &S);              // normalise '\' and '/' to PathDelim
bool IsPathDelimiter(const QString &S, int Index);    // 1-based
bool IsAbsolutePath(const QString &S);

// ---------------------------------------------------------------------------
// File system
// ---------------------------------------------------------------------------

bool FileExists(const QString &FileName);
bool DirectoryExists(const QString &Directory);
bool ForceDirectories(const QString &Dir);
bool CreateDir(const QString &Dir);
bool RemoveDir(const QString &Dir);
bool DeleteFile(const QString &FileName);
bool RenameFile(const QString &OldName, const QString &NewName);
bool CopyFile(const QString &ExistingFileName, const QString &NewFileName, bool FailIfExists);
qint64 GetFileSizeOf(const QString &FileName);
QString GetCurrentDir();
bool SetCurrentDir(const QString &Dir);
TDateTime FileDateTime(const QString &FileName);   // last modification time
int FileAge(const QString &FileName);              // DOS packed date/time, -1 if missing
bool FileSetDateTime(const QString &FileName, TDateTime Value);

// File attributes for FindFirst/FindNext
enum : int {
    faReadOnly = 0x01, faHidden = 0x02, faSysFile = 0x04, faVolumeID = 0x08,
    faDirectory = 0x10, faArchive = 0x20, faAnyFile = 0x3F
};

struct TSearchRec {
    QString Name;
    qint64 Size = 0;
    int Attr = 0;
    int Time = 0;              // DOS packed date/time
    TDateTime TimeStamp = 0;   // modification date as TDateTime
    // internal
    QStringList FEntries;
    QString FDir;
    int FIndex = 0;
    int FAttr = 0;
};
int FindFirst(const QString &Path, int Attr, TSearchRec &F);   // 0 = found
int FindNext(TSearchRec &F);                                   // 0 = found
void FindClose(TSearchRec &F);

// Reads a whole text file.  UTF-8 (with or without BOM) is detected, everything
// else is decoded with the system 8 bit codepage (as Delphi 2007 did).
QString ReadTextFile(const QString &FileName, bool *Ok = nullptr, bool *IsUtf8 = nullptr);
bool WriteTextFile(const QString &FileName, const QString &Text, bool Utf8 = false, bool Bom = false);
QByteArray EncodeAnsi(const QString &S);
QString DecodeAnsi(const QByteArray &B);

// ---------------------------------------------------------------------------
// Date & time
// ---------------------------------------------------------------------------

TDateTime Now();
TDateTime Date();
TDateTime Time();
TDateTime QDateTimeToDateTime(const QDateTime &DT);
QDateTime DateTimeToQDateTime(TDateTime DT);
TDateTime EncodeDate(int Year, int Month, int Day);
TDateTime EncodeTime(int Hour, int Min, int Sec, int MSec);
void DecodeDate(TDateTime DT, int &Year, int &Month, int &Day);
void DecodeTime(TDateTime DT, int &Hour, int &Min, int &Sec, int &MSec);
QString DateToStr(TDateTime DT);
QString TimeToStr(TDateTime DT);
QString DateTimeToStr(TDateTime DT);
TDateTime StrToDateDef(const QString &S, TDateTime Default);
TDateTime StrToDateTimeDef(const QString &S, TDateTime Default);
QString FormatDateTime(const QString &Format, TDateTime DT);
TDateTime FileDateToDateTime(int FileDate);
int DateTimeToFileDate(TDateTime DT);
int DaysBetween(TDateTime A, TDateTime B);
#ifdef _WIN32
// Use the Win32 function (DWORD GetTickCount(void)) - declaring our own
// version would clash with <windows.h>.
extern "C" __declspec(dllimport) unsigned long __stdcall GetTickCount(void);
#else
quint32 GetTickCount();
#endif

// ---------------------------------------------------------------------------
// Process / environment
// ---------------------------------------------------------------------------

int ParamCount();
QString ParamStr(int Index);      // 0 = program file name
QString GetEnvironmentVariable(const QString &Name);
void Sleep(int Milliseconds);
void Beep();
void ProcessMessages();           // Application.ProcessMessages

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

template <typename T> inline T Min(T a, T b) { return a < b ? a : b; }
template <typename T> inline T Max(T a, T b) { return a > b ? a : b; }
int Round(double X);   // banker's rounding like Delphi
int Trunc(double X);
