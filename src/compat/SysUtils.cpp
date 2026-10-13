#include "SysUtils.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QThread>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QLocale>
#include <QApplication>
#include <cmath>

const QString PathDelim = QString(QDir::separator());
const QChar PathDelimChar = QDir::separator();
#ifdef Q_OS_WIN
const QString LineBreak = QStringLiteral("\r\n");
#else
const QString LineBreak = QStringLiteral("\n");
#endif

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

int Length(const QString &S) { return S.length(); }

QString Copy(const QString &S, int Index, int Count)
{
    if (Index < 1) Index = 1;
    if (Count <= 0 || Index > S.length()) return QString();
    return S.mid(Index - 1, Count);
}

int Pos(const QString &SubStr, const QString &S)
{
    if (SubStr.isEmpty()) return 0;
    return S.indexOf(SubStr) + 1;
}

int Pos(QChar C, const QString &S) { return S.indexOf(C) + 1; }

int PosEx(const QString &SubStr, const QString &S, int Offset)
{
    if (SubStr.isEmpty() || Offset < 1 || Offset > S.length()) return 0;
    return S.indexOf(SubStr, Offset - 1) + 1;
}

void Delete(QString &S, int Index, int Count)
{
    if (Index < 1 || Index > S.length() || Count <= 0) return;
    S.remove(Index - 1, Count);
}

void Insert(const QString &Source, QString &S, int Index)
{
    if (Index < 1) Index = 1;
    if (Index > S.length() + 1) Index = S.length() + 1;
    S.insert(Index - 1, Source);
}

QString StringOfChar(QChar C, int Count) { return Count > 0 ? QString(Count, C) : QString(); }

// Delphi Trim removes all characters <= ' '
QString Trim(const QString &S)
{
    int a = 0, b = S.length() - 1;
    while (a <= b && S[a].unicode() <= ' ') a++;
    while (b >= a && S[b].unicode() <= ' ') b--;
    return S.mid(a, b - a + 1);
}

QString TrimLeft(const QString &S)
{
    int a = 0;
    while (a < S.length() && S[a].unicode() <= ' ') a++;
    return S.mid(a);
}

QString TrimRight(const QString &S)
{
    int b = S.length() - 1;
    while (b >= 0 && S[b].unicode() <= ' ') b--;
    return S.left(b + 1);
}

QString UpperCase(const QString &S) { return S.toUpper(); }
QString LowerCase(const QString &S) { return S.toLower(); }

int CompareText(const QString &S1, const QString &S2) { return QString::compare(S1, S2, Qt::CaseInsensitive); }
int CompareStr(const QString &S1, const QString &S2) { return QString::compare(S1, S2, Qt::CaseSensitive); }
bool SameText(const QString &S1, const QString &S2) { return CompareText(S1, S2) == 0; }
bool AnsiSameText(const QString &S1, const QString &S2) { return CompareText(S1, S2) == 0; }
bool StartsText(const QString &SubText, const QString &Text) { return Text.startsWith(SubText, Qt::CaseInsensitive); }
bool EndsText(const QString &SubText, const QString &Text) { return Text.endsWith(SubText, Qt::CaseInsensitive); }
bool ContainsText(const QString &Text, const QString &SubText) { return Text.contains(SubText, Qt::CaseInsensitive); }

QString QuotedStr(const QString &S)
{
    QString R = S;
    R.replace("'", "''");
    return "'" + R + "'";
}

QString AnsiDequotedStr(const QString &S, QChar Quote)
{
    if (S.length() >= 2 && S.startsWith(Quote) && S.endsWith(Quote)) {
        QString R = S.mid(1, S.length() - 2);
        R.replace(QString(2, Quote), QString(Quote));
        return R;
    }
    return S;
}

QString StringReplace(const QString &S, const QString &OldPattern, const QString &NewPattern, int Flags)
{
    if (OldPattern.isEmpty()) return S;
    Qt::CaseSensitivity cs = (Flags & rfIgnoreCase) ? Qt::CaseInsensitive : Qt::CaseSensitive;
    if (Flags & rfReplaceAll) {
        QString R = S;
        R.replace(OldPattern, NewPattern, cs);
        return R;
    }
    int i = S.indexOf(OldPattern, 0, cs);
    if (i < 0) return S;
    QString R = S;
    R.replace(i, OldPattern.length(), NewPattern);
    return R;
}

QString IntToStr(qint64 Value) { return QString::number(Value); }

QString IntToHex(qint64 Value, int Digits)
{
    return QString::number(static_cast<quint64>(Value), 16).toUpper().rightJustified(Digits, '0');
}

static bool parseDelphiInt(const QString &S, qint64 &V)
{
    QString T = S.trimmed();
    bool ok = false;
    if (T.startsWith('$')) V = T.mid(1).toLongLong(&ok, 16);
    else if (T.startsWith("-$")) { V = -T.mid(2).toLongLong(&ok, 16); }
    else if (T.startsWith("0x", Qt::CaseInsensitive)) V = T.mid(2).toLongLong(&ok, 16);
    else V = T.toLongLong(&ok, 10);
    return ok;
}

int StrToInt(const QString &S)
{
    qint64 V;
    if (!parseDelphiInt(S, V)) throw EConvertError(QString("'%1' is not a valid integer value").arg(S));
    return static_cast<int>(V);
}

int StrToIntDef(const QString &S, int Default)
{
    qint64 V;
    if (!parseDelphiInt(S, V) || V > INT32_MAX || V < INT32_MIN) return Default;
    return static_cast<int>(V);
}

qint64 StrToInt64Def(const QString &S, qint64 Default)
{
    qint64 V;
    return parseDelphiInt(S, V) ? V : Default;
}

bool TryStrToInt(const QString &S, int &Value)
{
    qint64 V;
    if (!parseDelphiInt(S, V) || V > INT32_MAX || V < INT32_MIN) return false;
    Value = static_cast<int>(V);
    return true;
}

static bool parseFloat(const QString &S, double &V)
{
    QString T = S.trimmed();
    bool ok = false;
    V = T.toDouble(&ok);
    if (!ok) {
        // accept the local decimal separator, too
        V = QLocale().toDouble(T, &ok);
        if (!ok) { T.replace(',', '.'); V = T.toDouble(&ok); }
    }
    return ok;
}

double StrToFloat(const QString &S)
{
    double V;
    if (!parseFloat(S, V)) throw EConvertError(QString("'%1' is not a valid floating point value").arg(S));
    return V;
}

double StrToFloatDef(const QString &S, double Default)
{
    double V;
    return parseFloat(S, V) ? V : Default;
}

bool TryStrToFloat(const QString &S, double &Value) { return parseFloat(S, Value); }

QString FloatToStr(double Value)
{
    return QString::number(Value, 'g', 15);
}

QString BoolToStr(bool B, bool UseBoolStrs)
{
    if (UseBoolStrs) return B ? "True" : "False";
    return B ? "-1" : "0";
}

bool StrToBoolDef(const QString &S, bool Default)
{
    QString T = S.trimmed().toUpper();
    if (T == "TRUE" || T == "1" || T == "-1" || T == "YES") return true;
    if (T == "FALSE" || T == "0" || T == "NO") return false;
    return Default;
}

QString Format(const QString &Fmt, std::initializer_list<QVariant> Args)
{
    return Format(Fmt, QList<QVariant>(Args));
}

QString Format(const QString &Fmt, const QList<QVariant> &Args)
{
    QString R;
    int argIndex = 0;
    int i = 0;
    const int n = Fmt.length();
    while (i < n) {
        QChar c = Fmt[i];
        if (c != '%') { R += c; i++; continue; }
        i++;
        if (i >= n) break;
        if (Fmt[i] == '%') { R += '%'; i++; continue; }
        // [index:] [-] [width] [.prec] type
        int start = i;
        QString num;
        while (i < n && Fmt[i].isDigit()) num += Fmt[i++];
        if (i < n && Fmt[i] == ':') { argIndex = num.toInt(); num.clear(); i++; }
        else if (!num.isEmpty()) { i = start; num.clear(); }
        bool left = false;
        if (i < n && Fmt[i] == '-') { left = true; i++; }
        int width = -1;
        if (i < n && Fmt[i] == '*') { width = argIndex < Args.size() ? Args[argIndex++].toInt() : 0; i++; }
        else { QString w; while (i < n && Fmt[i].isDigit()) w += Fmt[i++]; if (!w.isEmpty()) width = w.toInt(); }
        int prec = -1;
        if (i < n && Fmt[i] == '.') {
            i++;
            if (i < n && Fmt[i] == '*') { prec = argIndex < Args.size() ? Args[argIndex++].toInt() : 0; i++; }
            else { QString p; while (i < n && Fmt[i].isDigit()) p += Fmt[i++]; prec = p.isEmpty() ? 0 : p.toInt(); }
        }
        if (i >= n) break;
        QChar t = Fmt[i++].toLower();
        QVariant a = argIndex < Args.size() ? Args[argIndex] : QVariant();
        argIndex++;
        QString v;
        switch (t.unicode()) {
        case 'd': case 'u': {
            qint64 x = a.toLongLong();
            v = QString::number(qAbs(x));
            if (prec > 0) v = v.rightJustified(prec, '0');
            if (x < 0) v = "-" + v;
            break;
        }
        case 'x': {
            v = QString::number(a.toLongLong(), 16).toUpper();
            if (prec > 0) v = v.rightJustified(prec, '0');
            break;
        }
        case 'f': v = QString::number(a.toDouble(), 'f', prec < 0 ? 2 : prec); break;
        case 'e': v = QString::number(a.toDouble(), 'e', prec < 0 ? 15 : prec); break;
        case 'g': v = QString::number(a.toDouble(), 'g', prec < 0 ? 15 : prec); break;
        case 'n': case 'm': v = QLocale().toString(a.toDouble(), 'f', prec < 0 ? 2 : prec); break;
        case 'p': v = QString::number(a.toULongLong(), 16).toUpper().rightJustified(8, '0'); break;
        case 's': default:
            v = a.toString();
            if (prec >= 0) v = v.left(prec);
            break;
        }
        if (width > v.length()) v = left ? v.leftJustified(width, ' ') : v.rightJustified(width, ' ');
        R += v;
    }
    return R;
}

bool CharInSet(QChar C, const QString &Chars) { return Chars.contains(C); }

// ---------------------------------------------------------------------------
// File names
// ---------------------------------------------------------------------------

static inline bool isDelim(QChar c) { return c == '\\' || c == '/'; }

QString ToNativePath(const QString &S)
{
    QString R = S;
#ifdef Q_OS_WIN
    R.replace('/', '\\');
#else
    R.replace('\\', '/');
#endif
    return R;
}

QString IncludeTrailingPathDelimiter(const QString &S)
{
    if (!S.isEmpty() && isDelim(S[S.length() - 1])) return S;
    return S + PathDelim;
}

QString ExcludeTrailingPathDelimiter(const QString &S)
{
    QString R = S;
    while (R.length() > 1 && isDelim(R[R.length() - 1])) {
#ifdef Q_OS_WIN
        if (R.length() == 3 && R[1] == ':') break;  // keep "C:\"
#endif
        R.chop(1);
    }
    return R;
}

static int lastDelimiter(const QString &S, bool includeColon)
{
    for (int i = S.length() - 1; i >= 0; i--) {
        if (isDelim(S[i])) return i;
        if (includeColon && S[i] == ':') return i;
    }
    return -1;
}

QString ExtractFilePath(const QString &FileName)
{
    int i = lastDelimiter(FileName, true);
    return FileName.left(i + 1);
}

QString ExtractFileDir(const QString &FileName)
{
    int i = lastDelimiter(FileName, true);
    if (i > 0 && isDelim(FileName[i]) && !(i == 2 && FileName[1] == ':')) return FileName.left(i);
    return FileName.left(i + 1);
}

QString ExtractFileName(const QString &FileName)
{
    int i = lastDelimiter(FileName, true);
    return FileName.mid(i + 1);
}

QString ExtractFileExt(const QString &FileName)
{
    for (int i = FileName.length() - 1; i >= 0; i--) {
        QChar c = FileName[i];
        if (c == '.') return FileName.mid(i);
        if (isDelim(c) || c == ':') break;
    }
    return QString();
}

QString ExtractFileDrive(const QString &FileName)
{
    if (FileName.length() >= 2 && FileName[1] == ':') return FileName.left(2);
    if (FileName.length() >= 2 && isDelim(FileName[0]) && isDelim(FileName[1])) {
        // UNC: \\server\share
        int j = 0, i = 2;
        while (i < FileName.length() && j < 2) {
            if (isDelim(FileName[i])) j++;
            if (j < 2) i++;
        }
        return FileName.left(i);
    }
    return QString();
}

QString ChangeFileExt(const QString &FileName, const QString &Extension)
{
    QString Ext = ExtractFileExt(FileName);
    return FileName.left(FileName.length() - Ext.length()) + Extension;
}

QString ExpandFileName(const QString &FileName)
{
    if (FileName.isEmpty()) return FileName;
    QString R = QDir::cleanPath(QDir(QDir::currentPath()).absoluteFilePath(ToNativePath(FileName)));
    R = QDir::toNativeSeparators(R);
    if (isDelim(FileName[FileName.length() - 1])) R = IncludeTrailingPathDelimiter(R);
    return R;
}

QString ExtractRelativePath(const QString &BaseName, const QString &DestName)
{
    QString baseDir = ExtractFilePath(BaseName);
    if (baseDir.isEmpty()) return DestName;
    QString R = QDir(ToNativePath(baseDir)).relativeFilePath(ToNativePath(DestName));
    return QDir::toNativeSeparators(R);
}

bool IsPathDelimiter(const QString &S, int Index)
{
    return Index >= 1 && Index <= S.length() && isDelim(S[Index - 1]);
}

bool IsAbsolutePath(const QString &S)
{
    if (S.length() >= 2 && S[1] == ':') return true;
    if (!S.isEmpty() && isDelim(S[0])) return true;
    return false;
}

// ---------------------------------------------------------------------------
// File system
// ---------------------------------------------------------------------------

bool FileExists(const QString &FileName)
{
    if (FileName.isEmpty()) return false;
    QFileInfo fi(ToNativePath(FileName));
    return fi.exists() && !fi.isDir();
}

bool DirectoryExists(const QString &Directory)
{
    if (Directory.isEmpty()) return false;
    QFileInfo fi(ToNativePath(Directory));
    return fi.exists() && fi.isDir();
}

bool ForceDirectories(const QString &Dir)
{
    if (Dir.isEmpty()) return false;
    return QDir().mkpath(ToNativePath(ExcludeTrailingPathDelimiter(Dir)));
}

bool CreateDir(const QString &Dir) { return QDir().mkdir(ToNativePath(Dir)); }
bool RemoveDir(const QString &Dir) { return QDir().rmdir(ToNativePath(Dir)); }

bool DeleteFile(const QString &FileName)
{
    QFile f(ToNativePath(FileName));
    if (!f.exists()) return false;
    if (!f.remove()) {
        // read only files can not be deleted with Delphi DeleteFile either,
        // but try once more after removing the read only flag
        f.setPermissions(f.permissions() | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        return f.remove();
    }
    return true;
}

bool RenameFile(const QString &OldName, const QString &NewName)
{
    QString o = ToNativePath(OldName), n = ToNativePath(NewName);
    if (QFileInfo(o).isDir()) return QDir().rename(o, n);
    return QFile::rename(o, n);
}

bool CopyFile(const QString &ExistingFileName, const QString &NewFileName, bool FailIfExists)
{
    QString s = ToNativePath(ExistingFileName), d = ToNativePath(NewFileName);
    if (QFile::exists(d)) {
        if (FailIfExists) return false;
        QFile::remove(d);
    }
    return QFile::copy(s, d);
}

qint64 GetFileSizeOf(const QString &FileName)
{
    QFileInfo fi(ToNativePath(FileName));
    return fi.exists() ? fi.size() : -1;
}

QString GetCurrentDir() { return QDir::toNativeSeparators(QDir::currentPath()); }
bool SetCurrentDir(const QString &Dir) { return QDir::setCurrent(ToNativePath(Dir)); }

TDateTime FileDateTime(const QString &FileName)
{
    QFileInfo fi(ToNativePath(FileName));
    if (!fi.exists()) return 0;
    return QDateTimeToDateTime(fi.lastModified());
}

int FileAge(const QString &FileName)
{
    QFileInfo fi(ToNativePath(FileName));
    if (!fi.exists()) return -1;
    return DateTimeToFileDate(QDateTimeToDateTime(fi.lastModified()));
}

bool FileSetDateTime(const QString &FileName, TDateTime Value)
{
    QFile f(ToNativePath(FileName));
    if (!f.open(QIODevice::ReadWrite)) return false;
    return f.setFileTime(DateTimeToQDateTime(Value), QFileDevice::FileModificationTime);
}

static void fillSearchRec(TSearchRec &F)
{
    const QString &name = F.FEntries[F.FIndex];
    F.Name = name;
    F.Attr = 0;
    F.Size = 0;
    if (name == "." || name == "..") {
        F.Attr = faDirectory;
        F.Time = 0;
        F.TimeStamp = 0;
        return;
    }
    QFileInfo fi(F.FDir + "/" + name);
    if (fi.isDir()) F.Attr |= faDirectory; else F.Size = fi.size();
    if (fi.isHidden()) F.Attr |= faHidden;
    if (!fi.isWritable()) F.Attr |= faReadOnly;
    if (!fi.isDir()) F.Attr |= faArchive;
    F.TimeStamp = QDateTimeToDateTime(fi.lastModified());
    F.Time = DateTimeToFileDate(F.TimeStamp);
}

int FindFirst(const QString &Path, int Attr, TSearchRec &F)
{
    QString p = ToNativePath(Path);
    QString dir = ExtractFilePath(p);
    QString mask = ExtractFileName(p);
    if (dir.isEmpty()) dir = ".";
    if (mask.isEmpty()) mask = "*";
    if (mask == "*.*") mask = "*";
    F.FDir = ExcludeTrailingPathDelimiter(dir);
    F.FAttr = Attr;
    F.FEntries.clear();
    F.FIndex = 0;
    QDir d(F.FDir);
    if (!d.exists()) return 2;
    QDir::Filters filters = QDir::Files | QDir::System;
    if (Attr & faDirectory) filters |= QDir::Dirs;
    if (Attr & faHidden) filters |= QDir::Hidden;
    // Delphi always reports normal files; directories only if requested.
    QStringList entries = d.entryList(QStringList() << mask, filters, QDir::Name | QDir::IgnoreCase | QDir::DirsFirst);
    if (Attr & faDirectory) {
        QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(mask), QRegularExpression::CaseInsensitiveOption);
        if (rx.match(".").hasMatch()) entries.prepend("..");
        if (rx.match(".").hasMatch()) entries.prepend(".");
    }
    F.FEntries = entries;
    if (F.FEntries.isEmpty()) return 18;  // ERROR_NO_MORE_FILES
    fillSearchRec(F);
    return 0;
}

int FindNext(TSearchRec &F)
{
    F.FIndex++;
    if (F.FIndex >= F.FEntries.size()) return 18;
    fillSearchRec(F);
    return 0;
}

void FindClose(TSearchRec &F)
{
    F.FEntries.clear();
    F.FIndex = 0;
}

QByteArray EncodeAnsi(const QString &S)
{
#ifdef Q_OS_WIN
    QStringEncoder enc(QStringConverter::System);
    return enc.encode(S);
#else
    // On non Windows systems the files written by D-Fend Reloaded were usually
    // created on Windows with codepage 1252.
    QStringEncoder enc("windows-1252");
    if (enc.isValid()) return enc.encode(S);
    return S.toLatin1();
#endif
}

QString DecodeAnsi(const QByteArray &B)
{
#ifdef Q_OS_WIN
    QStringDecoder dec(QStringConverter::System);
    return dec.decode(B);
#else
    QStringDecoder dec("windows-1252");
    if (dec.isValid()) return dec.decode(B);
    return QString::fromLatin1(B);
#endif
}

QString ReadTextFile(const QString &FileName, bool *Ok, bool *IsUtf8)
{
    QFile f(ToNativePath(FileName));
    if (!f.open(QIODevice::ReadOnly)) {
        if (Ok) *Ok = false;
        if (IsUtf8) *IsUtf8 = false;
        return QString();
    }
    QByteArray data = f.readAll();
    f.close();
    if (Ok) *Ok = true;
    if (data.startsWith("\xEF\xBB\xBF")) {
        if (IsUtf8) *IsUtf8 = true;
        return QString::fromUtf8(data.mid(3));
    }
    if (data.startsWith("\xFF\xFE")) {
        if (IsUtf8) *IsUtf8 = false;
        QStringDecoder dec(QStringConverter::Utf16LE);
        return dec.decode(data.mid(2));
    }
    // Detect UTF-8 without BOM: only if it contains non ASCII and decodes cleanly
    bool hasHigh = false;
    for (char c : data) if (static_cast<unsigned char>(c) >= 0x80) { hasHigh = true; break; }
    if (hasHigh) {
        QStringDecoder dec(QStringConverter::Utf8);
        QString s = dec.decode(data);
        if (!dec.hasError()) {
            if (IsUtf8) *IsUtf8 = true;
            return s;
        }
    }
    if (IsUtf8) *IsUtf8 = false;
    return DecodeAnsi(data);
}

bool WriteTextFile(const QString &FileName, const QString &Text, bool Utf8, bool Bom)
{
    QFile f(ToNativePath(FileName));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QByteArray data;
    if (Utf8) {
        if (Bom) data = "\xEF\xBB\xBF";
        data += Text.toUtf8();
    } else {
        data = EncodeAnsi(Text);
    }
    bool ok = f.write(data) == data.size();
    f.close();
    return ok;
}

// ---------------------------------------------------------------------------
// Date & time
// ---------------------------------------------------------------------------

static const QDate DelphiEpoch(1899, 12, 30);

TDateTime QDateTimeToDateTime(const QDateTime &DT)
{
    if (!DT.isValid()) return 0;
    QDateTime local = DT.toLocalTime();
    double days = DelphiEpoch.daysTo(local.date());
    double frac = local.time().msecsSinceStartOfDay() / 86400000.0;
    return days >= 0 ? days + frac : days - frac;
}

QDateTime DateTimeToQDateTime(TDateTime DT)
{
    double days = std::floor(DT);
    double frac = std::fabs(DT - days);
    if (DT < 0) { days = std::ceil(DT); frac = std::fabs(DT - days); }
    QDate d = DelphiEpoch.addDays(static_cast<qint64>(days));
    QTime t = QTime(0, 0).addMSecs(static_cast<int>(std::llround(frac * 86400000.0)) % 86400000);
    return QDateTime(d, t);
}

TDateTime Now() { return QDateTimeToDateTime(QDateTime::currentDateTime()); }
TDateTime Date() { return std::floor(Now()); }
TDateTime Time() { double n = Now(); return n - std::floor(n); }

TDateTime EncodeDate(int Year, int Month, int Day)
{
    QDate d(Year, Month, Day);
    if (!d.isValid()) throw EConvertError("Invalid argument to date encode");
    return DelphiEpoch.daysTo(d);
}

TDateTime EncodeTime(int Hour, int Min, int Sec, int MSec)
{
    return (((Hour * 60.0 + Min) * 60.0 + Sec) * 1000.0 + MSec) / 86400000.0;
}

void DecodeDate(TDateTime DT, int &Year, int &Month, int &Day)
{
    QDate d = DateTimeToQDateTime(DT).date();
    Year = d.year();
    Month = d.month();
    Day = d.day();
}

void DecodeTime(TDateTime DT, int &Hour, int &Min, int &Sec, int &MSec)
{
    QTime t = DateTimeToQDateTime(DT).time();
    Hour = t.hour();
    Min = t.minute();
    Sec = t.second();
    MSec = t.msec();
}

QString DateToStr(TDateTime DT) { return QLocale().toString(DateTimeToQDateTime(DT).date(), QLocale::ShortFormat); }
QString TimeToStr(TDateTime DT) { return QLocale().toString(DateTimeToQDateTime(DT).time(), QLocale::LongFormat); }
QString DateTimeToStr(TDateTime DT)
{
    QDateTime d = DateTimeToQDateTime(DT);
    if (d.time() == QTime(0, 0)) return DateToStr(DT);
    return DateToStr(DT) + " " + TimeToStr(DT);
}

TDateTime StrToDateDef(const QString &S, TDateTime Default)
{
    QDate d = QLocale().toDate(S.trimmed(), QLocale::ShortFormat);
    if (!d.isValid()) d = QDate::fromString(S.trimmed(), Qt::ISODate);
    if (!d.isValid()) return Default;
    return DelphiEpoch.daysTo(d);
}

TDateTime StrToDateTimeDef(const QString &S, TDateTime Default)
{
    QDateTime d = QLocale().toDateTime(S.trimmed(), QLocale::ShortFormat);
    if (!d.isValid()) d = QDateTime::fromString(S.trimmed(), Qt::ISODate);
    if (!d.isValid()) return StrToDateDef(S, Default);
    return QDateTimeToDateTime(d);
}

// Converts a Delphi FormatDateTime pattern into a Qt pattern.
QString FormatDateTime(const QString &Format, TDateTime DT)
{
    QDateTime d = DateTimeToQDateTime(DT);
    QString R;
    int i = 0;
    const int n = Format.length();
    auto count = [&](QChar c) { int j = i; while (j < n && Format[j].toLower() == c.toLower()) j++; return j - i; };
    while (i < n) {
        QChar c = Format[i];
        QChar lc = c.toLower();
        if (c == '"' || c == '\'') {
            int j = Format.indexOf(c, i + 1);
            if (j < 0) j = n;
            R += Format.mid(i + 1, j - i - 1);
            i = j + 1;
            continue;
        }
        if (lc == 'y') { int k = count(c); R += k <= 2 ? d.toString("yy") : d.toString("yyyy"); i += k; continue; }
        if (lc == 'm') {
            int k = count(c);
            // 'm' directly after h or before s means minutes
            bool minute = false;
            QString prev = R.right(3);
            int back = i - 1;
            while (back >= 0 && (Format[back] == ':' || Format[back] == ' ')) back--;
            if (back >= 0 && Format[back].toLower() == 'h') minute = true;
            int fwd = i + k;
            while (fwd < n && (Format[fwd] == ':' || Format[fwd] == ' ')) fwd++;
            if (fwd < n && Format[fwd].toLower() == 's') minute = true;
            if (minute) R += k >= 2 ? d.toString("mm") : d.toString("m");
            else R += k == 1 ? d.toString("M") : k == 2 ? d.toString("MM") : k == 3 ? QLocale().toString(d, "MMM") : QLocale().toString(d, "MMMM");
            i += k;
            continue;
        }
        if (lc == 'd') {
            int k = count(c);
            if (k == 1) R += d.toString("d");
            else if (k == 2) R += d.toString("dd");
            else if (k == 3) R += QLocale().toString(d, "ddd");
            else if (k == 4) R += QLocale().toString(d, "dddd");
            else if (k == 5) R += QLocale().toString(d.date(), QLocale::ShortFormat);
            else R += QLocale().toString(d.date(), QLocale::LongFormat);
            i += k;
            continue;
        }
        if (lc == 'h') { int k = count(c); R += k >= 2 ? d.toString("HH") : d.toString("H"); i += k; continue; }
        if (lc == 'n') { int k = count(c); R += k >= 2 ? d.toString("mm") : d.toString("m"); i += k; continue; }
        if (lc == 's') { int k = count(c); R += k >= 2 ? d.toString("ss") : d.toString("s"); i += k; continue; }
        if (lc == 'z') { int k = count(c); R += k >= 3 ? d.toString("zzz") : d.toString("z"); i += k; continue; }
        if (lc == 'c') { R += DateTimeToStr(DT); i++; continue; }
        if (c == '/') { R += QLocale().dateFormat(QLocale::ShortFormat).contains('.') ? "." : "/"; i++; continue; }
        R += c;
        i++;
    }
    return R;
}

TDateTime FileDateToDateTime(int FileDate)
{
    int date = (FileDate >> 16) & 0xFFFF;
    int time = FileDate & 0xFFFF;
    int year = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;
    int hour = (time >> 11) & 0x1F;
    int min = (time >> 5) & 0x3F;
    int sec = (time & 0x1F) * 2;
    QDate d(year, month, day);
    if (!d.isValid()) return 0;
    return DelphiEpoch.daysTo(d) + EncodeTime(hour, min, sec, 0);
}

int DateTimeToFileDate(TDateTime DT)
{
    QDateTime d = DateTimeToQDateTime(DT);
    int year = d.date().year();
    if (year < 1980 || year > 2107) return 0;
    int date = ((year - 1980) << 9) | (d.date().month() << 5) | d.date().day();
    int time = (d.time().hour() << 11) | (d.time().minute() << 5) | (d.time().second() / 2);
    return (date << 16) | time;
}

int DaysBetween(TDateTime A, TDateTime B) { return static_cast<int>(std::fabs(A - B) + 1e-9); }

#ifndef _WIN32
quint32 GetTickCount()
{
    static QElapsedTimer t;
    if (!t.isValid()) t.start();
    return static_cast<quint32>(t.elapsed());
}
#endif

// ---------------------------------------------------------------------------
// Process / environment
// ---------------------------------------------------------------------------

int ParamCount() { return QCoreApplication::arguments().size() - 1; }

QString ParamStr(int Index)
{
    QStringList a = QCoreApplication::arguments();
    if (Index == 0) return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return Index < a.size() ? a[Index] : QString();
}

QString GetEnvironmentVariable(const QString &Name) { return qEnvironmentVariable(Name.toUtf8().constData()); }

void Sleep(int Milliseconds) { QThread::msleep(static_cast<unsigned long>(Milliseconds)); }

void Beep() { QApplication::beep(); }

void ProcessMessages() { QCoreApplication::processEvents(); }

int Round(double X)
{
    // Delphi rounds to even ("banker's rounding")
    double r = std::nearbyint(X);
    return static_cast<int>(r);
}

int Trunc(double X) { return static_cast<int>(X); }
