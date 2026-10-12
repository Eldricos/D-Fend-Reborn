// C++ port of CommonComponents.pas (implementation section).
#include "CommonComponents.h"
#include "CommonTools.h"

#include <QDataStream>

// {DEFINE DoubleNumberCheck}

/* TBasePrgSetup */

// File modification time packed like a DOS date/time (FatDate*65536+FatTime)
static quint32 GetSimpleFileTime(const QString &FileName)
{
    int Age = FileAge(FileName);
    if (Age == -1) return 0;
    return static_cast<quint32>(Age);
}

TBasePrgSetup::TBasePrgSetup(const QString &ASetupFile)
{
    BooleanListUsed = -1;
    IntegerListUsed = -1;
    StringListUsed = -1;
    FSetupFile = ASetupFile;
    FLastTimeStamp = 0;
    FFirstRun = !FileExists(ASetupFile);
    FOwnINI = true;
    if (ASetupFile != "") {
        Ini = nullptr;
        CheckAndUpdateTimeStamp();
    } else {
        Ini = nullptr; FOwnINI = false;
    }
    FStoreConfigOnExit = true;
    FChanged = false;
}

TBasePrgSetup::TBasePrgSetup(const QString &ASetupFile, bool ATimeStampCheck)
{
    BooleanListUsed = -1;
    IntegerListUsed = -1;
    StringListUsed = -1;
    FSetupFile = ASetupFile;
    FLastTimeStamp = 0;
    FFirstRun = !FileExists(ASetupFile);
    FOwnINI = true;
    if (ASetupFile != "") {
        Ini = nullptr;
        if (ATimeStampCheck) CheckAndUpdateTimeStamp();
    } else {
        Ini = nullptr; FOwnINI = false;
    }
    FStoreConfigOnExit = true;
    FChanged = false;
}

TBasePrgSetup *TBasePrgSetup::CreateNoTimeStampCheck(const QString &ASetupFile)
{
    return new TBasePrgSetup(ASetupFile, false);
}

TBasePrgSetup::TBasePrgSetup(TBasePrgSetup *ABasePrgSetup)
{
    BooleanListUsed = -1;
    IntegerListUsed = -1;
    StringListUsed = -1;
    FOwnINI = false;
    FFirstRun = false;
    Ini = ABasePrgSetup->MemIni();
    ClearLists();
    FStoreConfigOnExit = true;
    FChanged = false;
}

TBasePrgSetup::~TBasePrgSetup()
{
    DestroySetup();

    if (FOwnINI && (Ini != nullptr)) delete Ini;
    Ini = nullptr;
}

void TBasePrgSetup::DestroySetup()
{
    if (FDestroyed) return;
    FDestroyed = true;

    if (FChanged && FStoreConfigOnExit) {
        LoadIniNow();
        if (OwnINI() && Ini != nullptr) {
            TStringList St;
            TStringList St2;
            Ini->ReadSections(St);
            for (int I = 0; I < St.Count(); I++) {
                St2.Clear();
                Ini->ReadSection(St[I], St2);
                if (St2.Count() == 0) Ini->EraseSection(St[I]);
            }
        }
        try {
            if (Ini != nullptr) {
                ForceDirectories(ExtractFilePath(Ini->FileName()));
                UpdateFile();
            }
        } catch (...) {}
    }
}

void TBasePrgSetup::LoadIniNow()
{
    if ((Ini == nullptr) && FOwnINI)
        Ini = new TMemIniFile(FSetupFile);
}

TMemIniFile *TBasePrgSetup::GetIni()
{
    LoadIniNow();
    return Ini;
}

TFileChangeStatus TBasePrgSetup::CheckAndUpdateTimeStamp()
{
    TFileChangeStatus result;
    if (FSetupFile == "") return fcsNoChange;

    if (!FileExists(FSetupFile)) return fcsDeleted;

    quint32 NewFileTime = GetSimpleFileTime(FSetupFile);
    if (NewFileTime == FLastTimeStamp) result = fcsNoChange; else result = fcsChanged;
    FLastTimeStamp = NewFileTime;
    return result;
}

void TBasePrgSetup::ReloadINI()
{
    if (Ini != nullptr) delete Ini;
    Ini = new TMemIniFile(FSetupFile);
    for (int I = 0; I < BooleanList.size(); I++) BooleanList[I].Cached = false;
    for (int I = 0; I < IntegerList.size(); I++) IntegerList[I].Cached = false;
    for (int I = 0; I < StringList.size(); I++) StringList[I].Cached = false;
}

void TBasePrgSetup::RenameINI(const QString &NewFile)
{
    LoadIniNow();
    if (Ini != nullptr) {
        Ini->UpdateFile();
        delete Ini;
        Ini = nullptr;
    }
    if (FSetupFile != NewFile) {
        if (RenameFile(FSetupFile, NewFile)) FSetupFile = NewFile;
    }
    Ini = new TMemIniFile(FSetupFile);
}

void TBasePrgSetup::ClearLists()
{
    BooleanList.clear();
    IntegerList.clear();
    StringList.clear();
}

void TBasePrgSetup::AssignFrom(TBasePrgSetup *ABasePrgSetup)
{
    AssignFromPartially(ABasePrgSetup, QList<int>());
}

void TBasePrgSetup::AssignFromPartially(TBasePrgSetup *ABasePrgSetup, const QList<int> &SettingsToKeep)
{
    bool B;
    for (int I = 0; I < BooleanList.size(); I++) {
        B = true;
        for (int J = 0; J < SettingsToKeep.size(); J++) if (SettingsToKeep[J] == BooleanList[I].Nr) { B = false; break; }
        if (!B) continue;
        BooleanList[I].Cached = true;
        BooleanList[I].CacheValueBool = ABasePrgSetup->GetBoolean(BooleanList[I].Nr);
    }
    for (int I = 0; I < IntegerList.size(); I++) {
        B = true;
        for (int J = 0; J < SettingsToKeep.size(); J++) if (SettingsToKeep[J] == IntegerList[I].Nr) { B = false; break; }
        if (!B) continue;
        IntegerList[I].Cached = true;
        IntegerList[I].CacheValueInteger = ABasePrgSetup->GetInteger(IntegerList[I].Nr);
    }
    for (int I = 0; I < StringList.size(); I++) {
        B = true;
        for (int J = 0; J < SettingsToKeep.size(); J++) if (SettingsToKeep[J] == StringList[I].Nr) { B = false; break; }
        if (!B) continue;
        StringList[I].Cached = true;
        StringList[I].CacheValueString = ABasePrgSetup->GetString(StringList[I].Nr);
    }

    StoreAllValues();
}

int TBasePrgSetup::IndexOf(int Nr, const TConfigRecArray &List, TConfigIndexArray &Index)
{
    if (Index.size() == 0) {
        if (List.isEmpty()) return -1;
        /* List is sorted J:=0; For I:=0 to length(List)-1 do J:=Max(J,List[I].Nr); SetLength(Index,J+1); */
        Index.resize(List[List.size() - 1].Nr + 1);
        for (int I = 0; I < Index.size(); I++) Index[I] = -1;
        for (int I = 0; I < List.size(); I++) Index[List[I].Nr] = I;
    }
    if (Nr < 0 || Nr >= Index.size()) return -1;
    return Index[Nr];
}

void TBasePrgSetup::StoreAllValues()
{
    LoadIniNow();
    if (Ini == nullptr) return;

    for (int I = 0; I < BooleanList.size(); I++)
        Ini->WriteBool(BooleanList[I].Section, BooleanList[I].Key, GetBoolean(BooleanList[I].Nr));

    for (int I = 0; I < IntegerList.size(); I++)
        Ini->WriteInteger(IntegerList[I].Section, IntegerList[I].Key, GetInteger(IntegerList[I].Nr));

    for (int I = 0; I < StringList.size(); I++)
        Ini->WriteString(StringList[I].Section, StringList[I].Key, GetString(StringList[I].Nr));

    if (FChanged) UpdateFile();
    FChanged = false;

    CheckAndUpdateTimeStamp();
}

void TBasePrgSetup::UpdateFile()
{
    LoadIniNow();
    try {
        if (Ini != nullptr) { ForceDirectories(ExtractFilePath(Ini->FileName())); Ini->UpdateFile(); }
    } catch (...) {} /* Language Files may be read only in program foldes */
}

void TBasePrgSetup::ResetToDefault()
{
    for (int I = 0; I < BooleanList.size(); I++)
        SetBoolean(BooleanList[I].Nr, BooleanList[I].DefaultBool);

    for (int I = 0; I < IntegerList.size(); I++)
        SetInteger(IntegerList[I].Nr, IntegerList[I].DefaultInteger);

    for (int I = 0; I < StringList.size(); I++)
        SetString(StringList[I].Nr, StringList[I].DefaultString);

    UpdateFile();
    FChanged = false;

    CheckAndUpdateTimeStamp();
}

void TBasePrgSetup::FastAddRecDone()
{
    if (BooleanListUsed >= 0) BooleanList.resize(BooleanListUsed);
    if (IntegerListUsed >= 0) IntegerList.resize(IntegerListUsed);
    if (StringListUsed >= 0) StringList.resize(StringListUsed);
}

void TBasePrgSetup::FastAddRecStart()
{
    BooleanListUsed = 0;
    IntegerListUsed = 0;
    StringListUsed = 0;
}

int TBasePrgSetup::AddRec(int ANr, const QString &ASection, const QString &AKey, TConfigRecArray &List)
{
    int result = List.size();
    List.resize(result + 1);

    int J = result;
#ifdef DoubleNumberCheck
    for (int I = 0; I < result; I++) {
        if ((J == List.size() - 1) && (ANr < List[I].Nr)) J = I;
        if (ANr == List[I].Nr) ShowMessage("Double use of nr " + IntToStr(ANr));
    }
    for (int I = result - 1; I >= J; I--) List[I + 1] = List[I];
    result = J;
#else
    if (result > 0 && ANr < List[result - 1].Nr) {
        for (int I = 0; I < result; I++) if (ANr < List[I].Nr) { J = I; break; }
        for (int I = result - 1; I >= J; I--) List[I + 1] = List[I];
        result = J;
    }
#endif

    TConfigRec &R = List[J];
    R.Nr = ANr;
    R.Section = ASection;
    R.Key = AKey;
    R.Cached = false;
    return result;
}

int TBasePrgSetup::AddRecFast(int ANr, const QString &ASection, const QString &AKey, TConfigRecArray &List, int &UsedCounter)
{
    if (UsedCounter == List.size()) List.resize(UsedCounter + 50);
    int result = UsedCounter;
    UsedCounter++;

    int J = result;
#ifdef DoubleNumberCheck
    for (int I = 0; I < result; I++) {
        if ((J == result) && (ANr < List[I].Nr)) J = I;
        if (ANr == List[I].Nr) ShowMessage("Double use of nr " + IntToStr(ANr));
    }
    for (int I = result - 1; I >= J; I--) List[I + 1] = List[I];
    result = J;
#else
    if (result > 0 && ANr < List[result - 1].Nr) {
        for (int I = 0; I < result; I++) if (ANr < List[I].Nr) { J = I; break; }
        for (int I = result - 1; I >= J; I--) List[I + 1] = List[I];
        result = J;
    }
#endif

    TConfigRec &R = List[J];
    R.Nr = ANr;
    R.Section = ASection;
    R.Key = AKey;
    R.Cached = false;
    return result;
}

void TBasePrgSetup::AddBooleanRec(int Nr, const QString &Section, const QString &Key, bool Default)
{
    if (BooleanListUsed >= 0)
        BooleanList[AddRecFast(Nr, Section, Key, BooleanList, BooleanListUsed)].DefaultBool = Default;
    else
        BooleanList[AddRec(Nr, Section, Key, BooleanList)].DefaultBool = Default;
}

void TBasePrgSetup::AddIntegerRec(int Nr, const QString &Section, const QString &Key, int Default)
{
    if (IntegerListUsed >= 0)
        IntegerList[AddRecFast(Nr, Section, Key, IntegerList, IntegerListUsed)].DefaultInteger = Default;
    else
        IntegerList[AddRec(Nr, Section, Key, IntegerList)].DefaultInteger = Default;
}

void TBasePrgSetup::AddStringRec(int Nr, const QString &Section, const QString &Key, const QString &Default)
{
    if (StringListUsed >= 0)
        StringList[AddRecFast(Nr, Section, Key, StringList, StringListUsed)].DefaultString = Default;
    else
        StringList[AddRec(Nr, Section, Key, StringList)].DefaultString = Default;
}

bool TBasePrgSetup::GetBoolean(int Index)
{
    QString S;
    bool B, result;
    int I = IndexOf(Index, BooleanList, BooleanIndex);
    if (I < 0) return false;
    TConfigRec &R = BooleanList[I];
    if (R.Cached) return R.CacheValueBool;

    if (R.DefaultBool) S = "true"; else S = "false";
    LoadIniNow();
    if (Ini != nullptr) S = Trim(ExtUpperCase(Ini->ReadString(R.Section, R.Key, S)));
    B = false; result = false;
    if ((S == "0") || (S == "FALSE")) { result = false; B = true; }
    if ((S == "1") || (S == "TRUE")) { result = true; B = true; }
    if (!B) result = R.DefaultBool;

    R.Cached = true;
    R.CacheValueBool = result;
    return result;
}

int TBasePrgSetup::GetInteger(int Index)
{
    int result;
    int I = IndexOf(Index, IntegerList, IntegerIndex);
    Q_ASSERT_X(I >= 0, "TBasePrgSetup::GetInteger",
               qPrintable("Es gibt keinen Eintrag in der Integer-Liste mit der Nummer " + IntToStr(Index)));
    if (I < 0) return 0;
    TConfigRec &R = IntegerList[I];
    if (R.Cached) return R.CacheValueInteger;
    LoadIniNow();
    if (Ini != nullptr) result = Ini->ReadInteger(R.Section, R.Key, R.DefaultInteger); else result = R.DefaultInteger;
    R.Cached = true;
    R.CacheValueInteger = result;
    return result;
}

void TBasePrgSetup::ReadStringFromINI(int I)
{
    LoadIniNow();
    TConfigRec &R = StringList[I];
    R.Cached = true;
    if (Ini != nullptr) R.CacheValueString = Ini->ReadString(R.Section, R.Key, R.DefaultString); else R.CacheValueString = R.DefaultString;
}

void TBasePrgSetup::SetChanged()
{
    FChanged = true;
}

QString TBasePrgSetup::GetString(int Index)
{
    int I = IndexOf(Index, StringList, StringIndex);
    if (I < 0) return QString();
    if (!StringList[I].Cached) ReadStringFromINI(I);
    return StringList[I].CacheValueString;
}

void TBasePrgSetup::SetBoolean(int Index, bool Value)
{
    int I = IndexOf(Index, BooleanList, BooleanIndex);
    if (I < 0) return;
    TConfigRec &R = BooleanList[I];
    LoadIniNow();
    if (Ini != nullptr) {
        if (R.DefaultBool == Value) Ini->DeleteKey(R.Section, R.Key); else Ini->WriteBool(R.Section, R.Key, Value);
    }
    R.Cached = true;
    R.CacheValueBool = Value;
    FChanged = true;
}

void TBasePrgSetup::SetInteger(int Index, int Value)
{
    int I = IndexOf(Index, IntegerList, IntegerIndex);
    if (I < 0) return;
    TConfigRec &R = IntegerList[I];
    LoadIniNow();
    if (Ini != nullptr) {
        if (R.DefaultInteger == Value) Ini->DeleteKey(R.Section, R.Key); else Ini->WriteInteger(R.Section, R.Key, Value);
    }
    R.Cached = true;
    R.CacheValueInteger = Value;
    FChanged = true;
}

void TBasePrgSetup::SetString(int Index, const QString &Value)
{
    int I = IndexOf(Index, StringList, StringIndex);
    if (I < 0) return;
    TConfigRec &R = StringList[I];
    LoadIniNow();
    if (Ini != nullptr) {
        if (R.DefaultString == Value) Ini->DeleteKey(R.Section, R.Key); else Ini->WriteString(R.Section, R.Key, Value);
    }
    R.Cached = true;
    R.CacheValueString = Value;
    FChanged = true;
}

void TBasePrgSetup::CacheAllStrings()
{
    for (int I = 0; I < StringList.size(); I++) if (!StringList[I].Cached) ReadStringFromINI(I);
}

// The binary format does not match the Delphi version (QDataStream is used).
void TBasePrgSetup::LoadBinConfig(QIODevice *St, PConfigRecArray PConfig, TConfigType ConfigType)
{
    QDataStream DS(St);
    DS.setVersion(QDataStream::Qt_6_0);
    switch (ConfigType) {
        case ctBoolean:
            for (int I = 0; I < PConfig->size(); I++) { bool B = false; DS >> B; (*PConfig)[I].CacheValueBool = B; (*PConfig)[I].Cached = true; }
            break;
        case ctInteger:
            for (int I = 0; I < PConfig->size(); I++) { qint32 J = 0; DS >> J; (*PConfig)[I].CacheValueInteger = J; (*PConfig)[I].Cached = true; }
            break;
        case ctString:
            for (int I = 0; I < PConfig->size(); I++) {
                QString S;
                DS >> S;
                (*PConfig)[I].CacheValueString = S;
                (*PConfig)[I].Cached = true;
            }
            break;
    }
}

void TBasePrgSetup::StoreBinConfig(QIODevice *St, PConfigRecArray PConfig, TConfigType ConfigType)
{
    QDataStream DS(St);
    DS.setVersion(QDataStream::Qt_6_0);
    for (int I = 0; I < PConfig->size(); I++) switch (ConfigType) {
        case ctBoolean:
            if (!(*PConfig)[I].Cached) GetBoolean((*PConfig)[I].Nr);
            DS << (*PConfig)[I].CacheValueBool;
            break;
        case ctInteger:
            if (!(*PConfig)[I].Cached) GetInteger((*PConfig)[I].Nr);
            DS << static_cast<qint32>((*PConfig)[I].CacheValueInteger);
            break;
        case ctString:
            if (!(*PConfig)[I].Cached) GetString((*PConfig)[I].Nr);
            DS << (*PConfig)[I].CacheValueString;
            break;
    }
}

void TBasePrgSetup::LoadFromStream(QIODevice *St)
{
    LoadBinConfig(St, &BooleanList, ctBoolean);
    LoadBinConfig(St, &IntegerList, ctInteger);
    LoadBinConfig(St, &StringList, ctString);
    QDataStream DS(St);
    DS.setVersion(QDataStream::Qt_6_0);
    DS >> FLastTimeStamp;
    FChanged = false;
}

void TBasePrgSetup::SaveToStream(QIODevice *St)
{
    StoreBinConfig(St, &BooleanList, ctBoolean);
    StoreBinConfig(St, &IntegerList, ctInteger);
    StoreBinConfig(St, &StringList, ctString);
    QDataStream DS(St);
    DS.setVersion(QDataStream::Qt_6_0);
    DS << FLastTimeStamp;
}

int TBasePrgSetup::GetBinaryVersionID()
{
    return 1000 * 1000 * BooleanList.size() + 1000 * IntegerList.size() + StringList.size();
}
