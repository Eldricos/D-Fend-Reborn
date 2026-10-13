// C++ port of LanguageSetupUnit.pas (implementation section).
// The bodies of TLanguageSetup::SetupLanguageStrings1..5 (the default English
// strings) are generated into LanguageSetupStrings.cpp by tools/langstrings.py.
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"

#include <QFile>
#include <QStringDecoder>

TLanguageSetup* LanguageSetup = nullptr;
QString LanguageSpecialLowerCase;
QString LanguageSpecialUpperCase;
QString LanguageSetupFastYes;
QString LanguageSetupFastNo;
TStringList* LanguageSetupEnglishGenres = nullptr;
TStringList* LanguageSetupEnglishGenresUpper = nullptr;
TStringList* LanguageSetupCustomGenres = nullptr;
TStringList* LanguageSetupCustomGenresUpper = nullptr;
TStringList* LanguageSetupEnglishLanguages = nullptr;
TStringList* LanguageSetupEnglishLanguagesUpper = nullptr;
TStringList* LanguageSetupCustomLanguages = nullptr;
TStringList* LanguageSetupCustomLanguagesUpper = nullptr;
TStringList* LanguageSetupEnglishLicenses = nullptr;
TStringList* LanguageSetupEnglishLicensesUpper = nullptr;
TStringList* LanguageSetupCustomLicenses = nullptr;
TStringList* LanguageSetupCustomLicensesUpper = nullptr;

static const QString DefaultLanguageFile = QStringLiteral("English.ini");

// ---------------------------------------------------------------------------
// Character set handling (port specific)
//
// The original program read the language files with the Windows ANSI API, the
// [CharsetSettings] Name=XXX_CHARSET value was used to select the font
// character set.  The compat TMemIniFile decodes non UTF-8 files with the
// system ANSI codepage (Windows) or windows-1252 (other systems).  If the
// language file declares another character set, the file is decoded again with
// the matching codepage here.  Because the compat layer can only write UTF-8 or
// the default ANSI codepage, such files are written back as UTF-8 (which the
// port reads transparently).
// ---------------------------------------------------------------------------

static QString CodecFromCharsetName(const QString &CharsetName)
{
    const QString S = Trim(UpperCase(CharsetName));
    if (S == "EASTEUROPE_CHARSET" || S == "238") return QStringLiteral("windows-1250");
    if (S == "RUSSIAN_CHARSET" || S == "204") return QStringLiteral("windows-1251");
    if (S == "GREEK_CHARSET" || S == "161") return QStringLiteral("windows-1253");
    if (S == "TURKISH_CHARSET" || S == "162") return QStringLiteral("windows-1254");
    if (S == "HEBREW_CHARSET" || S == "177") return QStringLiteral("windows-1255");
    if (S == "ARABIC_CHARSET" || S == "178") return QStringLiteral("windows-1256");
    if (S == "BALTIC_CHARSET" || S == "186") return QStringLiteral("windows-1257");
    if (S == "VIETNAMESE_CHARSET" || S == "163") return QStringLiteral("windows-1258");
    if (S == "THAI_CHARSET" || S == "222") return QStringLiteral("windows-874");
    if (S == "SHIFTJIS_CHARSET" || S == "128") return QStringLiteral("Shift_JIS");
    if (S == "HANGEUL_CHARSET" || S == "HANGUL_CHARSET" || S == "129") return QStringLiteral("windows-949");
    if (S == "GB2312_CHARSET" || S == "134") return QStringLiteral("GBK");
    if (S == "CHINESEBIG5_CHARSET" || S == "136") return QStringLiteral("Big5");
    if (S == "ANSI_CHARSET" || S == "0") return QStringLiteral("windows-1252");
    return QString();   // DEFAULT_CHARSET or unknown: keep the default decoding
}

static void ApplyLanguageFileCharset(TLanguageSetup *L)
{
    TMemIniFile *Ini = L->MemIni();
    if (Ini == nullptr || Ini->Utf8()) return;
    const QString FileName = Ini->FileName();
    if (FileName.isEmpty() || !FileExists(FileName)) return;

    QString Codec = CodecFromCharsetName(Ini->ReadString("CharsetSettings", "Name", ""));
    if (Codec.isEmpty()) {
        const QString HTML = Trim(LowerCase(Ini->ReadString("CharsetSettings", "HTMLCharset", "")));
        if (!HTML.isEmpty() && HTML != "iso-8859-1" && HTML != "utf-8" && HTML != "utf8") Codec = HTML;
    }
    if (Codec.isEmpty()) return;

    QFile F(ToNativePath(FileName));
    if (!F.open(QIODevice::ReadOnly)) return;
    const QByteArray Data = F.readAll();
    F.close();

    QStringDecoder Dec(Codec.toLatin1().constData());
    if (!Dec.isValid() && Codec == "windows-874") Dec = QStringDecoder("TIS-620");
    if (!Dec.isValid() && Codec == "windows-949") Dec = QStringDecoder("EUC-KR");
    if (!Dec.isValid()) return;
    const QString Text = Dec.decode(Data);
    if (Dec.hasError()) return;
    if (Text == DecodeAnsi(Data)) return;   // same result as the default decoding

    TStringList St;
    St.SetText(Text);
    Ini->SetStrings(St);
    Ini->SetUtf8(true);
}

// ---------------------------------------------------------------------------
// TLanguageSetup
// ---------------------------------------------------------------------------

static QString LanguageSetupIniFileName(const QString &FileName)
{
    if (FileName.isEmpty() || !FileExists(FileName)) {
        ForceDirectories(PrgDataDir() + LanguageSubDir);
        return PrgDataDir() + LanguageSubDir + PathDelim + DefaultLanguageFile;
    }
    return FileName;
}

TLanguageSetup::TLanguageSetup(const QString& FileName_, bool MinimalInit)
    : TBasePrgSetup(LanguageSetupIniFileName(FileName_))
{
    QString FileName = FileName_;
    if (!FileExists(FileName)) FileName = "";

    ApplyLanguageFileCharset(this);

    if (MinimalInit) {
        SetStoreConfigOnExit(false);
    } else {
        SetChanged();
    }

    FastAddRecStart();
    try {

        if ((Trim(ExtUpperCase(FileName)) == ExtUpperCase(DefaultLanguageFile)) || (Trim(FileName) == "")) {
            AddStringRec(NR_Author, "Author", "Name", "Alexander Herzog");
            AddStringRec(NR_MaxVersion, "LanguageFileInfo", "MaxVersion", GetNormalFileVersionAsString());
            AddStringRec(NR_DownloadChecksum, "LanguageFileInfo", "DownloadChecksum", "");
            AddStringRec(NR_NSISLanguageID, "LanguageFileInfo", "NSISLanguageID", "1033");
            AddStringRec(NR_LocalLanguageName, "LanguageFileInfo", "LocalLanguageName", "English");
            AddStringRec(NR_HelpFile, "LanguageFileInfo", "HelpFile", "English");
        } else {
            AddStringRec(NR_Author, "Author", "Name", "No Author defined");
            AddStringRec(NR_MaxVersion, "LanguageFileInfo", "MaxVersion", "0");
            AddStringRec(NR_DownloadChecksum, "LanguageFileInfo", "DownloadChecksum", "");
            AddStringRec(NR_NSISLanguageID, "LanguageFileInfo", "NSISLanguageID", "");
            AddStringRec(NR_LocalLanguageName, "LanguageFileInfo", "LocalLanguageName", "");
            AddStringRec(NR_HelpFile, "LanguageFileInfo", "HelpFile", "English");
        }

        if (!MinimalInit) {
            SetupLanguageStrings1();
            SetupLanguageStrings2();
            SetupLanguageStrings3();
            SetupLanguageStrings4();
            SetupLanguageStrings5();
            /* Needed to split this; compiler said "Too many local constants. Use shorter procedures." */
        }
    } catch (...) {
        FastAddRecDone();
        throw;
    }
    FastAddRecDone();
}

TLanguageSetup::~TLanguageSetup()
{
    StoreAllValues();
    DestroySetup();   // inherited Destroy
}

/* Function TLanguageSetup.GetString2 / Procedure TLanguageSetup.SetString2:
   commented out in the original source. */

// ---------------------------------------------------------------------------
// Global routines
// ---------------------------------------------------------------------------

QString MaskUnderlineAmpersand(const QString& S)
{
    int J = Pos("&", S);
    if (J == 0) return S;
    QString result = Copy(S, 1, J - 1);
    for (; J <= Length(S); J++)
        if (S[J - 1] == '&') result = result + "&&";
        else result = result + S[J - 1];
    return result;
}

QString RemoveUnderline(const QString& S)
{
    int J = Pos("&", S);
    if (J == 0) return S;
    QString result = Copy(S, 1, J - 1);
    int I = J;
    while (I <= Length(S)) {
        if (S[I - 1] != '&') result = result + S[I - 1];
        else if (I == Length(S)) result = result + " ";
        else if (S[I] == '&') { result = result + "&"; I++; }
        I++;
    }
    return result;
}

static void CheckDefaultLanguageFile()
{
    if (FileExists(PrgDir() + LanguageSubDir + PathDelim + DefaultLanguageFile)) return;
    if (FileExists(PrgDataDir() + LanguageSubDir + PathDelim + DefaultLanguageFile)) return;

    TLanguageSetup *TempLanguageSetup = new TLanguageSetup(PrgDataDir() + LanguageSubDir + PathDelim + DefaultLanguageFile);
    delete TempLanguageSetup;
}

// Replacement for ResHookUnit (the original program patched the VCL resource
// strings used by MessageDlg; MainUnit assigned the translated captions).
static void UpdateMsgDlgCaptions()
{
    MsgDlgCaptions.Warning = LanguageSetup->MsgDlgWarning();
    MsgDlgCaptions.Error = LanguageSetup->MsgDlgError();
    MsgDlgCaptions.Information = LanguageSetup->MsgDlgInformation();
    MsgDlgCaptions.Confirmation = LanguageSetup->MsgDlgConfirm();
    MsgDlgCaptions.Yes = LanguageSetup->MsgDlgYes();
    MsgDlgCaptions.No = LanguageSetup->MsgDlgNo();
    MsgDlgCaptions.OK = LanguageSetup->MsgDlgOK();
    MsgDlgCaptions.Cancel = LanguageSetup->MsgDlgCancel();
    MsgDlgCaptions.Abort = LanguageSetup->MsgDlgAbort();
    MsgDlgCaptions.Retry = LanguageSetup->MsgDlgRetry();
    MsgDlgCaptions.Ignore = LanguageSetup->MsgDlgIgnore();
    MsgDlgCaptions.All = LanguageSetup->MsgDlgAll();
    MsgDlgCaptions.NoToAll = LanguageSetup->MsgDlgNoToAll();
    MsgDlgCaptions.YesToAll = LanguageSetup->MsgDlgYesToAll();
    MsgDlgCaptions.Help = LanguageSetup->Help();
    MsgDlgCaptions.Close = LanguageSetup->Close();
}

void LoadLanguage(const QString& LanguageFile)
{
    TLanguageSetup *Temp;

    delete LanguageSetup; LanguageSetup = nullptr;

    if (FileExists(PrgDataDir() + LanguageSubDir + PathDelim + LanguageFile))
        LanguageSetup = new TLanguageSetup(PrgDataDir() + LanguageSubDir + PathDelim + LanguageFile);

    if ((PrgDir() != PrgDataDir()) && FileExists(PrgDir() + LanguageSubDir + PathDelim + LanguageFile)) {
        if (LanguageSetup == nullptr) {
            LanguageSetup = new TLanguageSetup(PrgDir() + LanguageSubDir + PathDelim + LanguageFile);
        } else {
            Temp = new TLanguageSetup(PrgDir() + LanguageSubDir + PathDelim + LanguageFile);
            if (VersionToInt(Temp->MaxVersion()) > VersionToInt(LanguageSetup->MaxVersion())) {
                delete LanguageSetup; LanguageSetup = Temp;
            } else {
                delete Temp;
            }
        }
    }

    if (LanguageSetup == nullptr)
        LanguageSetup = new TLanguageSetup();

    LanguageSpecialLowerCase = LanguageSetup->CharsetLower();
    LanguageSpecialUpperCase = LanguageSetup->CharsetUpper();
    if (Length(LanguageSpecialUpperCase) < Length(LanguageSpecialLowerCase))
        LanguageSpecialUpperCase = LanguageSpecialUpperCase + Copy(LanguageSpecialLowerCase, Length(LanguageSpecialUpperCase) + 1, MaxInt);

    LanguageSetupFastYes = RemoveUnderline(LanguageSetup->Yes());
    LanguageSetupFastNo = RemoveUnderline(LanguageSetup->No());

    UpdateMsgDlgCaptions();

    LoadGameListTranslations();
}

void GetDefaultGameListTranslations(TStringList*& DefaultGenreEnglish, TStringList*& DefaultGenreCustom, TStringList*& DefaultLanguageEnglish, TStringList*& DefaultLanguageCustom, TStringList*& DefaultLicenseEnglish, TStringList*& DefaultLicenseCustom)
{
    DefaultGenreEnglish = ValueToList(LanguageSetup->GameGenreEnglish());
    DefaultGenreCustom = ValueToList(LanguageSetup->GameGenreCustom());
    DefaultLanguageEnglish = ValueToList(LanguageSetup->GameLanguageEnglish());
    DefaultLanguageCustom = ValueToList(LanguageSetup->GameLanguageCustom());
    DefaultLicenseEnglish = ValueToList(LanguageSetup->GameLicenseEnglish());
    DefaultLicenseCustom = ValueToList(LanguageSetup->GameLicenseCustom());
}

static void AddListAndUpper(const QString &Value, TStringList *List, TStringList *ListUpper)
{
    TStringList *St = ValueToList(Value);
    List->AddStrings(*St);
    delete St;
    for (int I = 0; I < List->Count(); I++) ListUpper->Add(ExtUpperCase((*List)[I]));
}

void LoadGameListTranslations()
{
    QString S, FileName, GenreEnglish, GenreCustom, LanguageEnglish, LanguageCustom, LicenseEnglish, LicenseCustom;

    /* Clear old data */
    LanguageSetupEnglishGenres->Clear();
    LanguageSetupEnglishGenresUpper->Clear();
    LanguageSetupCustomGenres->Clear();
    LanguageSetupCustomGenresUpper->Clear();
    LanguageSetupEnglishLanguages->Clear();
    LanguageSetupEnglishLanguagesUpper->Clear();
    LanguageSetupCustomLanguages->Clear();
    LanguageSetupCustomLanguagesUpper->Clear();
    LanguageSetupEnglishLicenses->Clear();
    LanguageSetupEnglishLicensesUpper->Clear();
    LanguageSetupCustomLicenses->Clear();
    LanguageSetupCustomLicensesUpper->Clear();

    /* Load default values in temp data */
    GenreEnglish = LanguageSetup->GameGenreEnglish();
    GenreCustom = LanguageSetup->GameGenreCustom();
    LanguageEnglish = LanguageSetup->GameLanguageEnglish();
    LanguageCustom = LanguageSetup->GameLanguageCustom();
    LicenseEnglish = LanguageSetup->GameLicenseEnglish();
    LicenseCustom = LanguageSetup->GameLicenseCustom();

    FileName = PrgDataDir() + LanguageSubDir + PathDelim + ExtractFileName(ChangeFileExt(LanguageSetup->SetupFile(), ".custom.ini"));
    if (FileExists(FileName)) {
        TMemIniFile Mem(FileName);
        S = Mem.ReadString("GenreNames", "English", ""); if (Trim(S) != "") GenreEnglish = S;
        S = Mem.ReadString("GenreNames", "Custom", ""); if (Trim(S) != "") GenreCustom = S;
        S = Mem.ReadString("LanguageNames", "English", ""); if (Trim(S) != "") LanguageEnglish = S;
        S = Mem.ReadString("LanguageNames", "Custom", ""); if (Trim(S) != "") LanguageCustom = S;
        S = Mem.ReadString("LicenseNames", "English", ""); if (Trim(S) != "") LicenseEnglish = S;
        S = Mem.ReadString("LicenseNames", "Custom", ""); if (Trim(S) != "") LicenseCustom = S;
    }

    /* Make final data from temp data */
    AddListAndUpper(GenreEnglish, LanguageSetupEnglishGenres, LanguageSetupEnglishGenresUpper);
    AddListAndUpper(GenreCustom, LanguageSetupCustomGenres, LanguageSetupCustomGenresUpper);
    AddListAndUpper(LanguageEnglish, LanguageSetupEnglishLanguages, LanguageSetupEnglishLanguagesUpper);
    AddListAndUpper(LanguageCustom, LanguageSetupCustomLanguages, LanguageSetupCustomLanguagesUpper);
    AddListAndUpper(LicenseEnglish, LanguageSetupEnglishLicenses, LanguageSetupEnglishLicensesUpper);
    AddListAndUpper(LicenseCustom, LanguageSetupCustomLicenses, LanguageSetupCustomLicensesUpper);
}

static void RebuildUpper(TStringList *List, TStringList *ListUpper)
{
    ListUpper->Clear();
    for (int I = 0; I < List->Count(); I++) ListUpper->Add(ExtUpperCase((*List)[I]));
}

void SaveGameListTranslations()
{
    QString FileName, GenreEnglish, GenreCustom, LanguageEnglish, LanguageCustom, LicenseEnglish, LicenseCustom;

    /* Make temp data from normal data */
    GenreEnglish = ListToValue(LanguageSetupEnglishGenres);
    if (Trim(GenreEnglish) == Trim(LanguageSetup->GameGenreEnglish())) GenreEnglish = "";
    GenreCustom = ListToValue(LanguageSetupCustomGenres);
    if (Trim(GenreCustom) == Trim(LanguageSetup->GameGenreCustom())) GenreCustom = "";
    LanguageEnglish = ListToValue(LanguageSetupEnglishLanguages);
    if (Trim(LanguageEnglish) == Trim(LanguageSetup->GameLanguageEnglish())) LanguageEnglish = "";
    LanguageCustom = ListToValue(LanguageSetupCustomLanguages);
    if (Trim(LanguageCustom) == Trim(LanguageSetup->GameLanguageCustom())) LanguageCustom = "";
    LicenseEnglish = ListToValue(LanguageSetupEnglishLicenses);
    if (Trim(LicenseEnglish) == Trim(LanguageSetup->GameLicenseEnglish())) LicenseEnglish = "";
    LicenseCustom = ListToValue(LanguageSetupCustomLicenses);
    if (Trim(LicenseCustom) == Trim(LanguageSetup->GameLicenseCustom())) LicenseCustom = "";

    FileName = PrgDataDir() + LanguageSubDir + PathDelim + ExtractFileName(ChangeFileExt(LanguageSetup->SetupFile(), ".custom.ini"));
    if ((GenreEnglish == "") && (GenreCustom == "") && (LanguageEnglish == "") && (LanguageCustom == "") && (LicenseEnglish == "") && (LicenseCustom == "")) {
        /* No custom file needed */
        if (FileExists(FileName)) ExtDeleteFile(FileName, ftProfile);
    } else {
        /* Write data to file */
        ForceDirectories(ExtractFilePath(FileName));
        TMemIniFile Mem(FileName);
        if (Trim(GenreEnglish) == "") Mem.DeleteKey("GenreNames", "English"); else Mem.WriteString("GenreNames", "English", GenreEnglish);
        if (Trim(GenreCustom) == "") Mem.DeleteKey("GenreNames", "Custom"); else Mem.WriteString("GenreNames", "Custom", GenreCustom);
        if (Trim(LanguageEnglish) == "") Mem.DeleteKey("LanguageNames", "English"); else Mem.WriteString("LanguageNames", "English", LanguageEnglish);
        if (Trim(LanguageCustom) == "") Mem.DeleteKey("LanguageNames", "Custom"); else Mem.WriteString("LanguageNames", "Custom", LanguageCustom);
        if (Trim(LicenseEnglish) == "") Mem.DeleteKey("LicenseNames", "English"); else Mem.WriteString("LicenseNames", "English", LicenseEnglish);
        if (Trim(LicenseCustom) == "") Mem.DeleteKey("LicenseNames", "Custom"); else Mem.WriteString("LicenseNames", "Custom", LicenseCustom);
        Mem.UpdateFile();
    }

    /* Update upper data */
    RebuildUpper(LanguageSetupEnglishGenres, LanguageSetupEnglishGenresUpper);
    RebuildUpper(LanguageSetupCustomGenres, LanguageSetupCustomGenresUpper);
    RebuildUpper(LanguageSetupCustomLanguages, LanguageSetupCustomLanguagesUpper);
    RebuildUpper(LanguageSetupEnglishLanguages, LanguageSetupEnglishLanguagesUpper);
    RebuildUpper(LanguageSetupCustomLicenses, LanguageSetupCustomLicensesUpper);
    RebuildUpper(LanguageSetupEnglishLicenses, LanguageSetupEnglishLicensesUpper);
}

static QString TranslateCustomString(const QString &S, TStringList *SourceListUpper, TStringList *DestList)
{
    int I, J;
    QString T, result;

    J = Pos(";", S);
    if (J > 0) {
        T = S; result = "";
        while (T != "") {
            if (J > 0) {
                I = SourceListUpper->IndexOf(ExtUpperCase(Trim(Copy(T, 1, J - 1))));
                if (result != "") result = result + ";";
                if (I < 0) result = result + Trim(Copy(T, 1, J - 1)); else result = result + (*DestList)[I];
                T = Trim(Copy(T, J + 1, MaxInt));
            } else {
                I = SourceListUpper->IndexOf(ExtUpperCase(T));
                if (result != "") result = result + ";";
                if (I < 0) result = result + T; else result = result + (*DestList)[I];
                break;
            }
            J = Pos(";", T);
        }
    } else {
        I = SourceListUpper->IndexOf(ExtUpperCase(S));
        if (I < 0) result = S; else result = (*DestList)[I];
    }
    return result;
}

QString GetCustomGenreName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupEnglishGenresUpper, LanguageSetupCustomGenres);
}

TStringList* GetCustomGenreName(TStringList* St)
{
    TStringList *result = new TStringList;
    for (int I = 0; I < St->Count(); I++) {
        QString S = GetCustomGenreName((*St)[I]);
        if (result->IndexOf(S) < 0) result->Add(S);
    }
    delete St;
    result->Sort();
    return result;
}

QString GetCustomLanguageName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupEnglishLanguagesUpper, LanguageSetupCustomLanguages);
}

TStringList* GetCustomLanguageName(TStringList* St)
{
    TStringList *result = new TStringList;
    for (int I = 0; I < St->Count(); I++) {
        QString S = GetCustomLanguageName((*St)[I]);
        if (result->IndexOf(S) < 0) result->Add(S);
    }
    delete St;
    result->Sort();
    return result;
}

QString GetCustomLicenseName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupEnglishLicensesUpper, LanguageSetupCustomLicenses);
}

TStringList* GetCustomLicenseName(TStringList* St) /* original list will be freed ! */
{
    TStringList *result = new TStringList;
    for (int I = 0; I < St->Count(); I++) {
        QString S = GetCustomLicenseName((*St)[I]);
        if (result->IndexOf(S) < 0) result->Add(S);
    }
    delete St;
    result->Sort();
    return result;
}

QString GetEnglishGenreName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupCustomGenresUpper, LanguageSetupEnglishGenres);
}

QString GetEnglishLanguageName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupCustomLanguagesUpper, LanguageSetupEnglishLanguages);
}

QString GetEnglishLicenseName(const QString& S)
{
    return TranslateCustomString(S, LanguageSetupCustomLicensesUpper, LanguageSetupEnglishLicenses);
}

static TStringList *ExtList(TStringList *St, TStringList *CustomList)
{
    TStringList *result = new TStringList; result->AddStrings(*St); delete St;

    for (int I = 0; I < CustomList->Count(); I++) {
        QString S = ExtUpperCase((*CustomList)[I]);
        bool B = true;
        for (int J = 0; J < result->Count(); J++) if (ExtUpperCase((*result)[J]) == S) { B = false; break; }
        if (B) result->Add((*CustomList)[I]);
    }

    result->Sort();
    return result;
}

TStringList* ExtGenreList(TStringList* St)
{
    return ExtList(St, LanguageSetupCustomGenres);
}

TStringList* ExtLanguageList(TStringList* St)
{
    return ExtList(St, LanguageSetupCustomLanguages);
}

TStringList* ExtLicenseList(TStringList* St)
{
    return ExtList(St, LanguageSetupCustomLicenses);
}

static void GetLanguageInfo(const QString &LangFile, QString &LangName, int &NSIS, int &MaxVersion)
{
    QString GlobalName, LocalName, S;

    GlobalName = ChangeFileExt(ExtractFileName(LangFile), "");
    TLanguageSetup *TempLang = new TLanguageSetup(LangFile, true);
    TempLang->SetStoreConfigOnExit(false);
    LocalName = TempLang->LocalLanguageName();
    if ((Trim(ExtUpperCase(LocalName)) == Trim(ExtUpperCase(GlobalName))) || (Trim(LocalName) == ""))
        LangName = GlobalName;
    else
        LangName = LocalName + " (" + GlobalName + ")";
    S = Trim(TempLang->NSISLanguageID());
    if (S != "") { if (!TryStrToInt(S, NSIS)) NSIS = -1; } else NSIS = -1;
    MaxVersion = VersionToInt(TempLang->MaxVersion());
    delete TempLang;
}

static void AddLanguages(TStringList *St, TStringList *FileNames, TList *MaxVersionList, const QString &Dir)
{
    int I, NSIS, MaxVersion, Nr;
    TSearchRec Rec;
    QString S;

    I = FindFirst(Dir + LanguageSubDir + PathDelim + "*.ini", faAnyFile, Rec);
    while (I == 0) {
        if (ExtLowerCase(Copy(Rec.Name, Length(Rec.Name) - 10)) != ".custom.ini") {
            GetLanguageInfo(Dir + LanguageSubDir + PathDelim + Rec.Name, S, NSIS, MaxVersion);
            Nr = St->IndexOf(S);
            if (Nr < 0) {
                St->AddObject(S, (intptr_t)NSIS);
                if (FileNames != nullptr) FileNames->Add(Dir + LanguageSubDir + PathDelim + Rec.Name);
                MaxVersionList->append(reinterpret_cast<void *>((intptr_t)MaxVersion));
            } else {
                if (MaxVersion > (int)reinterpret_cast<intptr_t>((*MaxVersionList)[Nr])) {
                    (*St)[Nr] = S; St->SetObjects(Nr, reinterpret_cast<void *>((intptr_t)NSIS));
                    if (FileNames != nullptr) (*FileNames)[Nr] = Dir + LanguageSubDir + PathDelim + Rec.Name;
                    (*MaxVersionList)[Nr] = reinterpret_cast<void *>((intptr_t)MaxVersion);
                }
            }
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

TStringList* GetLanguageList()
{
    TStringList *result = new TStringList;
    TList MaxVersion;
    AddLanguages(result, nullptr, &MaxVersion, PrgDataDir());
    if (PrgDataDir() != PrgDir()) AddLanguages(result, nullptr, &MaxVersion, PrgDir());
    result->Sort();
    return result;
}

QString LanguageFileNameFromName(const QString& Name)
{
    QString result = "";

    TStringList Names, FileNames;
    TList MaxVersion;
    AddLanguages(&Names, &FileNames, &MaxVersion, PrgDataDir());
    if (PrgDataDir() != PrgDir()) AddLanguages(&Names, &FileNames, &MaxVersion, PrgDir());

    int I = Names.IndexOf(Name);
    if (I >= 0) result = FileNames[I];
    return result;
}

int FindLanguageInList(const QString& Lang, TStringList* List)
{
    int result = -1;
    QString S = ExtUpperCase(Lang);
    for (int I = 0; I < List->Count(); I++) if (S == ExtUpperCase(ShortLanguageName((*List)[I]))) {
        result = I; break;
    }
    return result;
}

int FindEnglishInList(TStringList* List)
{
    return FindLanguageInList("English", List);
}

QString ShortLanguageName(const QString& LongLanguageName)
{
    QString result = LongLanguageName;

    int J = -1;
    for (int I = Length(result); I >= 1; I--) if (result[I - 1] == '(') { J = I; break; }

    if (J > 0) {
        result = Trim(Copy(result, J + 1, MaxInt));
        if (Pos(")", result) != 0) result = Trim(Copy(result, 1, Pos(")", result) - 1));
    }
    return result;
}

// ---------------------------------------------------------------------------
// initialization / finalization
// ---------------------------------------------------------------------------

void LanguageSetupUnit_Initialization()
{
    LanguageSetup = nullptr;
    LanguageSetupEnglishGenres = new TStringList;
    LanguageSetupEnglishGenresUpper = new TStringList;
    LanguageSetupCustomGenres = new TStringList;
    LanguageSetupCustomGenresUpper = new TStringList;
    LanguageSetupEnglishLanguages = new TStringList;
    LanguageSetupEnglishLanguagesUpper = new TStringList;
    LanguageSetupCustomLanguages = new TStringList;
    LanguageSetupCustomLanguagesUpper = new TStringList;
    LanguageSetupEnglishLicenses = new TStringList;
    LanguageSetupEnglishLicensesUpper = new TStringList;
    LanguageSetupCustomLicenses = new TStringList;
    LanguageSetupCustomLicensesUpper = new TStringList;
    CheckDefaultLanguageFile();
    LoadLanguage(PrgSetup->Language());
}

void LanguageSetupUnit_Finalization()
{
    delete LanguageSetup; LanguageSetup = nullptr;
    delete LanguageSetupEnglishGenres; LanguageSetupEnglishGenres = nullptr;
    delete LanguageSetupEnglishGenresUpper; LanguageSetupEnglishGenresUpper = nullptr;
    delete LanguageSetupCustomGenres; LanguageSetupCustomGenres = nullptr;
    delete LanguageSetupCustomGenresUpper; LanguageSetupCustomGenresUpper = nullptr;
    delete LanguageSetupEnglishLanguages; LanguageSetupEnglishLanguages = nullptr;
    delete LanguageSetupEnglishLanguagesUpper; LanguageSetupEnglishLanguagesUpper = nullptr;
    delete LanguageSetupCustomLanguages; LanguageSetupCustomLanguages = nullptr;
    delete LanguageSetupCustomLanguagesUpper; LanguageSetupCustomLanguagesUpper = nullptr;
    delete LanguageSetupEnglishLicenses; LanguageSetupEnglishLicenses = nullptr;
    delete LanguageSetupEnglishLicensesUpper; LanguageSetupEnglishLicensesUpper = nullptr;
    delete LanguageSetupCustomLicenses; LanguageSetupCustomLicenses = nullptr;
    delete LanguageSetupCustomLicensesUpper; LanguageSetupCustomLicensesUpper = nullptr;
}
