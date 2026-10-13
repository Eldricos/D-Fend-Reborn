// C++ port of DOSBoxLangTools.pas (implementation section).
#include "DOSBoxLangTools.h"
#include "DosCodePage.h"

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

// CharToOEM / OEMToChar: see DosCodePage.h
static QString CharToOEM(const QString& S) { return CharToOemAnsi(S); }
static QString OEMToChar(const QString& S) { return OemAnsiToChar(S); }

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
    RunCommandAndWait(TempGame->Game(), DFendRebornMainForm ? DFendRebornMainForm->DeleteOnExit : nullptr, "CONFIG -writelang \"" + result + "\"" + "\r" + "exit", true);
    delete TempGame;
    return result;
}
