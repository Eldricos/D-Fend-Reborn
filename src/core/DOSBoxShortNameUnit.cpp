// C++ port of DOSBoxShortNameUnit.pas (implementation section).
#include "DOSBoxShortNameUnit.h"

#include <cmath>

#include "FileNameConvertor.h"
#include "DosBoxUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"

// { old algorithm }

static QString OldDOSBoxShortName(const QString& RootPath, const QString& LongName)
{
    QString Path, ReferenceShortName, S;
    TSearchRec Rec;
    int I;
    TStringList* St;
    QString S1, S2;
    QString result;

    Path = IncludeTrailingPathDelimiter(RootPath);

    // {Check if LongName is already ok}
    S1 = ExtUpperCase(ShortName(Path + LongName));
    S = ExtUpperCase(IncludeTrailingPathDelimiter(ShortName(Path)));
    S1 = Copy(S1, Length(S) + 1, MaxInt);

    S2 = ExtUpperCase(Path + LongName);
    S = ExtUpperCase(IncludeTrailingPathDelimiter(Path));
    S2 = Copy(S2, Length(S) + 1, MaxInt);

    if (S1 == S2) { return ExtUpperCase(LongName); }

    // {Create short name for compareing}
    ReferenceShortName = ExtractFileName(ExtUpperCase(ShortName(Path + LongName)));
    if (Length(ReferenceShortName) > 2) ReferenceShortName.truncate(Length(ReferenceShortName) - 2);
    result = ReferenceShortName + "~1";

    St = new TStringList();
    // {Find all files with the same shortname (without ~x}
    I = FindFirst(Path + "*.*", faAnyFile, Rec);
    while (I == 0) {
        S = ExtUpperCase(ShortName(Path + Rec.Name));
        while (Pos("\\", S) > 0) S = Copy(S, Pos("\\", S) + 1, MaxInt);
        while (Pos("/", S) > 0) S = Copy(S, Pos("/", S) + 1, MaxInt);  // native path delimiter on non Windows systems
        if (Length(S) > 2) S.truncate(Length(S) - 2);
        if (S == ReferenceShortName) St->Add(Rec.Name);
        I = FindNext(Rec);
    }
    FindClose(Rec);

    // {Find new number}
    St->Sort();
    for (I = 0; I < St->Count(); I++) {
        if (ExtUpperCase((*St)[I]) == ExtUpperCase(LongName)) {
            result = ReferenceShortName + "~" + IntToStr(I + 1);
            break;
        }
    }
    delete St;
    return result;
}

// { new algorithm }

static bool PathNameOK(QString SubDir)
{
    static const QString AllowedCharsDefault = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ") + QChar(0xC4) + QChar(0xD6) + QChar(0xDC) +
        QStringLiteral("abcdefghijklmnopqrstuvwxyz") + QChar(0xE4) + QChar(0xF6) + QChar(0xFC) + QChar(0xDF) +
        QStringLiteral("01234567890-_=.,;!()$#@{}&'`~") + QChar(246) + QChar(255) + QChar(0xa0) + QChar(0xe5);
    int I;
    QString Name, Ext;

    // {Only check subdir}
    do {
        I = Pos("\\", SubDir);
        if (I > 0) SubDir = Copy(SubDir, I + 1, MaxInt);
    } while (!(I <= 0));

    I = Pos(".", SubDir);
    if (I > 0) { Name = Copy(SubDir, 1, I - 1); Ext = Copy(SubDir, I + 1, MaxInt); } else { Name = SubDir; Ext = ""; }

    if ((Length(Name) > 8) || (Length(Ext) > 3)) return false;

    // (Pos(...)<0 is never true - kept from the original code)
    for (I = 1; I <= Length(Name); I++) if (Pos(Name[I - 1], AllowedCharsDefault) < 0) return false;
    for (I = 1; I <= Length(Ext); I++) if (Pos(Ext[I - 1], AllowedCharsDefault) < 0) return false;

    return true;
}

static void GetShortParts(const QString& PathName, QString& Name, QString& Ext)
{
    int I;
    I = Pos(".", PathName);
    if (I > 0) { Name = Copy(PathName, 1, I - 1); Ext = Copy(PathName, I + 1, MaxInt); } else { Name = PathName; Ext = ""; }
    Name = RemoveIllegalFileNameChars(Name);
    if (!Name.isEmpty()) Name = ExtUpperCase(Name);
    Ext = RemoveIllegalFileNameChars(Ext);
    if (!Ext.isEmpty()) Ext = ExtUpperCase(Ext);
    if (Length(Name) > 8) Name = Copy(Name, 1, 8);
    if (Length(Ext) > 3) Ext = Copy(Ext, 1, 3);
}

static TStringList* FilesWithSameShortName(QString ParentDir, QString Name, const QString& Ext, int Digits)
{
    TSearchRec Rec;
    int I;
    QString Name2, Ext2;
    TStringList* result = new TStringList();

    ParentDir = IncludeTrailingPathDelimiter(ParentDir);
    if (Length(Name) > 8 - 1 - Digits) Name.truncate(qMax(0, 8 - 1 - Digits));

    I = FindFirst(ParentDir + "*.*", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Name != ".") && (Rec.Name != "..")) {
            if (!PathNameOK(Rec.Name)) {
                GetShortParts(Rec.Name, Name2, Ext2);
                if (Length(Name2) > 8 - 1 - Digits) Name2.truncate(qMax(0, 8 - 1 - Digits));
                if ((Name == Name2) && (Ext == Ext2)) result->Add(Rec.Name);
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

static QString NewDOSBoxShortName(const QString& RootPath, const QString& LongName)
{
    QString PDir, SDir, Name, Ext, S;
    int I;
    TStringList* St;
    QString result = "";

    PDir = IncludeTrailingPathDelimiter(MakeAbsPath(RootPath, PrgSetup->BaseDir()));
    SDir = LongName;

    if ((!DirectoryExists(PDir + SDir)) && (!FileExists(PDir + SDir))) return result;

    if (PathNameOK(SDir)) { return ExtUpperCase(SDir); }

    GetShortParts(SDir, Name, Ext);

    I = 0; St = nullptr;

    do {
        I++;
        if (St != nullptr) { delete St; St = nullptr; }
        St = FilesWithSameShortName(PDir, Name, Ext, I);
    } while (!(St->Count() < Round(std::pow(10.0, I))));

    St->Sort();

    S = ExtUpperCase(SDir);
    for (I = 0; I < St->Count(); I++) if (ExtUpperCase((*St)[I]) == S) {
        S = IntToStr(I + 1);
        S = Copy(Name, 1, 8 - 1 - Length(S)) + "~" + S;
        result = S;
        break;
    }

    delete St;
    return result;
}

// { the real DOSBox way of making short names}

static void RealDOSBoxShortNameErrorMessage(const QString& RootPath, const QString& LongName, const QString& ShortName, TConvertResult ConvertResult)
{
    Q_UNUSED(ShortName);
    QString ErrorMessage;
    ErrorMessage = "";
    switch (ConvertResult) {
        case tcrErrParentNotFound: ErrorMessage = Format(LanguageSetup->MessageShortNameErrorParentNotFound(), {RootPath}); break;
        case tcrErrDelimiterInFileName: ErrorMessage = Format(LanguageSetup->MessageShortNameErrorDelimiterInFileName(), {LongName}); break;
        case tcrErrFileNotFound: ErrorMessage = Format(LanguageSetup->MessageShortNameErrorFileNotFound(), {LongName, RootPath}); break;
        case tcrWarnInvalidChars: ErrorMessage = Format(LanguageSetup->MessageShortNameWarningInvalidChars(), {LongName}); break;
        case tcrWarnSystemShort: ErrorMessage = Format(LanguageSetup->MessageShortNameWarningSystemShort(), {LongName}); break;
        case tcrWarnSystemShortAlreadyUsed: ErrorMessage = Format(LanguageSetup->MessageShortNameWarningSystemShortAlreadyUsed(), {LongName}); break;
        default: ErrorMessage = ""; break;
    }
    if (!ErrorMessage.isEmpty()) {
        if (ConvertResult == tcrWarnInvalidChars || ConvertResult == tcrWarnSystemShort || ConvertResult == tcrWarnSystemShortAlreadyUsed)
            MessageDlg(ErrorMessage, mtWarning, mbOK, 0);
        else
            MessageDlg(ErrorMessage, mtError, mbOK, 0);
    }
}

static QString RealDOSBoxShortName(const QString& RootPath, const QString& LongName, bool UseSystemShortName)
{
    TConvertResult ConvertResult;
    bool isDir;
    QString ShortName;

    ConvertResult = ConvertLongFileNameToShort(
        RootPath,
        LongName,
        WineSupportEnabled(),
        UseSystemShortName,
        true,
        false,
        ShortName, isDir
    );

    if (ConvertResult == tcrErrParentNotFound || ConvertResult == tcrErrDelimiterInFileName || ConvertResult == tcrErrFileNotFound) ShortName = LongName;
    QString result = ShortName;

    if (!PrgSetup->ShowShortNameWarnings()) return result;
    RealDOSBoxShortNameErrorMessage(RootPath, LongName, ShortName, ConvertResult); // {if not in separate function compiler says "return value may not defined"}
    return result;
}

// { select algorithm }

QString DOSBoxShortName(const QString& RootPath, const QString& LongName)
{
    QString result;
    switch (PrgSetup->DOSBoxShortFileNameAlgorithm()) {
        case 1: result = OldDOSBoxShortName(RootPath, LongName); break;
        case 2: result = NewDOSBoxShortName(RootPath, LongName); if (result.isEmpty()) result = OldDOSBoxShortName(RootPath, LongName); break;
        case 3: result = RealDOSBoxShortName(RootPath, LongName, false); break;
        default: result = RealDOSBoxShortName(RootPath, LongName, false); break;
    }
    return result;
}
