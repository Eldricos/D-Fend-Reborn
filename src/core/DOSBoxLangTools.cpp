// C++ port of DOSBoxLangTools.pas (implementation section).
#include "DOSBoxLangTools.h"

#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "DOSBoxTempUnit.h"
#include "DosBoxUnit.h"
#include "MainUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

// The Delphi strings are 8 bit (ANSI) strings.  In the port a string holds the
// ANSI decoded characters, so an OEM encoded string is represented by the ANSI
// decoding of its OEM bytes (exactly what TStringList.LoadFromFile/SaveToFile
// read and write).

static QChar AnsiChar(int B)
{
    QString S = DecodeAnsi(QByteArray(1, static_cast<char>(B)));
    return S.isEmpty() ? QChar(B) : S[0];
}

static const int SpecialCharsBytes[7] = {27, 0xC9, 0xCD, 0xBA, 0xBB, 0xC8, 0xBC};
static const QString SpecialSymbols[7] = {"<ESC>", "<UL>", "<->", "<|>", "<UR>", "<LL>", "<LR>"};
static QString SpecialChars(int I) { return QString(AnsiChar(SpecialCharsBytes[I])); }

static const QString NoConvert[3] = {"CONFIG_", "AUTOEXEC_CONFIGFILE_HELP", "CONFIGFILE_INTRO"};

#ifndef Q_OS_WIN
// Code page 437 (upper half) - used as OEM code page outside Windows
static const ushort CP437High[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};
#endif

// CharToOEM
static QString CharToOEM(const QString& S)
{
    QByteArray B;
#ifdef Q_OS_WIN
    std::wstring W = S.toStdWString();
    B.resize(static_cast<int>(W.size()));
    if (!W.empty()) CharToOemBuffW(W.c_str(), B.data(), static_cast<DWORD>(W.size()));
#else
    for (QChar C : S) {
        ushort U = C.unicode();
        if (U < 0x80) { B.append(static_cast<char>(U)); continue; }
        int Found = -1;
        for (int I = 0; I < 128; I++) if (CP437High[I] == U) { Found = I; break; }
        B.append(Found >= 0 ? static_cast<char>(0x80 + Found) : '?');
    }
#endif
    return DecodeAnsi(B);
}

// OEMToChar
static QString OEMToChar(const QString& S)
{
    QByteArray B = EncodeAnsi(S);
#ifdef Q_OS_WIN
    if (B.isEmpty()) return QString();
    int Len = MultiByteToWideChar(CP_OEMCP, 0, B.constData(), B.size(), nullptr, 0);
    std::wstring W(static_cast<size_t>(Len), L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, B.constData(), B.size(), &W[0], Len);
    return QString::fromStdWString(W);
#else
    QString R;
    for (char Ch : B) {
        uchar U = static_cast<uchar>(Ch);
        R += (U < 0x80) ? QChar(U) : QChar(CP437High[U - 0x80]);
    }
    return R;
#endif
}

static QString SingleLineStringToDOSBoxLangString(const QString& S)
{
    QString T;
    int I;
    QString result = S;
    if (Trim(result).isEmpty()) return result;

    T = CharToOEM(result);
    int Z = T.indexOf(QChar(0)); if (Z >= 0) T.truncate(Z);
    result = T;

    for (I = 0; I <= 6; I++) result = Replace(result, SpecialSymbols[I], SpecialChars(I));
    return result;
}

static QString StringToDOSBoxLangString(const QString& S)
{
    TStringList St;
    int I;
    St.SetText(S);
    for (I = 0; I < St.Count(); I++) St[I] = SingleLineStringToDOSBoxLangString(St[I]);
    return St.Text();
}

static QString SingleLineDOSBoxLangStringToString(const QString& S)
{
    QString T;
    int I;
    QString result = S;
    if (Trim(result).isEmpty()) return result;

    for (I = 0; I <= 6; I++) result = Replace(result, SpecialChars(I), SpecialSymbols[I]);

    T = OEMToChar(result);
    int Z = T.indexOf(QChar(0)); if (Z >= 0) T.truncate(Z);
    result = T;
    return result;
}

static QString DOSBoxLangStringToString(const QString& S)
{
    TStringList St;
    int I;
    St.SetText(S);
    for (I = 0; I < St.Count(); I++) St[I] = SingleLineDOSBoxLangStringToString(St[I]);
    return St.Text();
}

bool LoadDOSBoxLangFile(const QString& FileName, TStringList* LangIDStrings, TStringList* LangValueStrings)
{
    TStringList* St;
    int I, J;
    QString TempID, TempValue, S;
    bool B;

    if (!FileExists(FileName)) return false;
    St = new TStringList();
    if (!St->LoadFromFile(FileName)) { delete St; return false; }
    TempID = ""; TempValue = "";
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]);
        if ((!S.isEmpty()) && (S[0] == ':')) {
            if (!Trim(TempID).isEmpty()) {
                LangIDStrings->Add(TempID);
                LangValueStrings->Add(TempValue);
            }
            TempID = Copy(S, 2, MaxInt); TempValue = "";
        } else {
            if (!TempValue.isEmpty()) {
                if (TempValue == " ") TempValue = "\r"; else TempValue = TempValue + "\r";
            }
            if (((*St)[I].isEmpty()) && (TempValue.isEmpty())) TempValue = " "; else TempValue = TempValue + (*St)[I];
        }
    }
    if (!TempID.isEmpty()) {
        LangIDStrings->Add(TempID);
        LangValueStrings->Add(TempValue);
    }
    delete St;

    for (I = 0; I < LangIDStrings->Count(); I++) {
        S = Trim(ExtUpperCase((*LangIDStrings)[I]));
        B = true; for (J = 0; J <= 2; J++) if (Copy(S, 1, Length(NoConvert[J])) == NoConvert[J]) { B = false; break; }
        if (B) (*LangValueStrings)[I] = DOSBoxLangStringToString((*LangValueStrings)[I]);
    }
    return true;
}

bool SaveDOSBoxLangFile(const QString& FileName, TStringList* LangIDStrings, TStringList* LangValueStrings)
{
    TStringList *St, *St2, *LangValueStrings2;
    int I, J;
    QString S;
    bool B;
    bool result;

    LangValueStrings2 = new TStringList();
    LangValueStrings2->AddStrings(*LangValueStrings);

    for (I = 0; I < LangIDStrings->Count(); I++) {
        S = Trim(ExtUpperCase((*LangIDStrings)[I]));
        B = true; for (J = 0; J <= 2; J++) if (Copy(S, 1, Length(NoConvert[J])) == NoConvert[J]) { B = false; break; }
        if (B) (*LangValueStrings2)[I] = StringToDOSBoxLangString((*LangValueStrings2)[I]);
    }

    result = false;
    St = new TStringList();
    for (I = 0; I < Min(LangIDStrings->Count(), LangValueStrings2->Count()); I++) {
        St->Add(":" + (*LangIDStrings)[I]);
        St2 = new TStringList();
        St2->SetText((*LangValueStrings2)[I]);
        while ((St2->Count() > 0) && (Trim((*St2)[St2->Count() - 1]).isEmpty())) St2->Delete(St2->Count() - 1);
        St->AddStrings(*St2);
        delete St2;
    }
    if (St->SaveToFile(FileName)) result = true;
    delete St;
    delete LangValueStrings2;
    return result;
}

static void FindAndAddLngFiles(const QString& Dir, TStrings* St, TStrings* St2)
{
    TSearchRec Rec;
    int I, J;
    bool B;
    QString S;

    I = FindFirst(Dir + "*.lng", faAnyFile, Rec);
    while (I == 0) {
        B = true;
        S = Trim(ExtUpperCase(ChangeFileExt(Rec.Name, "")));
        // (the original code compares St[I] (I = FindFirst/FindNext result = 0) instead of St[J])
        for (J = 0; J < St->Count(); J++) if (Trim(ExtUpperCase((*St)[I])) == S) { B = false; break; }
        if (B) { St->Add(ChangeFileExt(Rec.Name, "")); St2->Add(Dir + Rec.Name); }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void GetDOSBoxLangNamesAndFiles(const QString& DOSBoxPath, TStringList* Names, TStringList* Files, bool WithDefaultEnglish)
{
    int I;
    TStringList *St1, *St2;

    St1 = new TStringList();
    St2 = new TStringList();

    if (WithDefaultEnglish) {
        St1->Add("English");
        St2->Add("");
    }

    FindAndAddLngFiles(PrgDataDir() + LanguageSubDir + PathDelim, St1, St2);
    FindAndAddLngFiles(IncludeTrailingPathDelimiter(DOSBoxPath), St1, St2);

    for (I = 0; I < St1->Count(); I++) St1->SetObjects(I, reinterpret_cast<void*>(static_cast<intptr_t>(I)));
    St1->Sort();

    for (I = 0; I < St1->Count(); I++) {
        Names->Add((*St1)[I]);
        Files->Add((*St2)[St1->ObjectAsInt(I)]);
    }

    delete St1;
    delete St2;
}

TStringList* GetDOSBoxLangNames(const QString& DOSBoxPath, bool WithDefaultEnglish)
{
    TStringList* Files;
    TStringList* result = new TStringList();
    Files = new TStringList();
    GetDOSBoxLangNamesAndFiles(DOSBoxPath, result, Files, WithDefaultEnglish);
    delete Files;
    return result;
}

QString GetTempDOSBoxEnglishLangFile()
{
    TTempGame* TempGame;
    QString result;
    ForceDirectories(TempDir() + TempSubFolder + PathDelim);
    result = TempDir() + TempSubFolder + PathDelim + "DOSBox-English.lng";
    TempGame = new TTempGame();
    TempGame->SetSimpleDefaults();
    TempGame->Game()->SetCloseDosBoxAfterGameExit(true);
    TempGame->Game()->SetCustomDOSBoxLanguage("ENGLISH");
    TempGame->Game()->StoreAllValues();
    RunCommandAndWait(TempGame->Game(), DFendReloadedMainForm ? DFendReloadedMainForm->DeleteOnExit : nullptr, "CONFIG -writelang \"" + result + "\"" + "\r" + "exit", true);
    delete TempGame;
    return result;
}
