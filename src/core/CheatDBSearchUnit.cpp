// C++ port of CheatDBSearchUnit.pas (implementation section).
#include "CheatDBSearchUnit.h"

#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "LanguageSetupUnit.h"

#include <QtEndian>

static const QByteArray DFRSearch = QByteArray("DFRAddressSearchFile");

// --- stream helpers (TFileStream / TMemoryStream ReadBuffer, little endian) --

class EReadError : public Exception { using Exception::Exception; };

static void ReadBuf(QIODevice& St, void* Buf, qint64 Count)
{
    if (Count <= 0) return;
    if (St.read(static_cast<char*>(Buf), Count) != Count) throw EReadError("Stream read error");
}

static int ReadInt(QIODevice& St)
{
    char B[4];
    ReadBuf(St, B, 4);
    return qFromLittleEndian<qint32>(B);
}

static QString ReadAnsiString(QIODevice& St, int Len)
{
    if (Len < 0) throw EReadError("Stream read error");
    QByteArray B(Len, '\0');
    ReadBuf(St, B.data(), Len);
    return DecodeAnsi(B);
}

static void WriteBuf(QIODevice& St, const void* Buf, qint64 Count)
{
    if (Count <= 0) return;
    if (St.write(static_cast<const char*>(Buf), Count) != Count) throw Exception("Stream write error");
}

static void WriteInt(QIODevice& St, int I)
{
    char B[4];
    qToLittleEndian<qint32>(I, B);
    WriteBuf(St, B, 4);
}

// memory stream reads at a given position
static int MemInt(const QByteArray& M, int Pos) { return qFromLittleEndian<qint32>(M.constData() + Pos); }
static quint16 MemWord(const QByteArray& M, int Pos) { return qFromLittleEndian<quint16>(M.constData() + Pos); }
static quint8 MemByte(const QByteArray& M, int Pos) { return static_cast<quint8>(M[Pos]); }

static bool LoadMemoryStream(QByteArray& M, const QString& FileName)
{
    QFile F(FileName);
    if (!F.open(QIODevice::ReadOnly)) return false;
    M = F.readAll();
    return true;
}

// --- implementation ----------------------------------------------------------

static bool SearchFileOK(const QString& AFileName, QString& AName)
{
    QByteArray S;
    int I;
    bool result = false;
    try {
        QFile FSt(AFileName);
        if (!FSt.open(QIODevice::ReadOnly)) throw EReadError("Cannot open file");
        if (FSt.size() < DFRSearch.length() + 4) return result;
        S.resize(DFRSearch.length());
        ReadBuf(FSt, S.data(), DFRSearch.length());
        result = (S == DFRSearch);
        if (result) {
            I = ReadInt(FSt);
            AName = ReadAnsiString(FSt, I);
        }
    } catch (...) {
        result = false; return result;
    }
    return result;
}

void GetAddressSearchFiles(TStringList* AFiles, TStringList* ANames)
{
    int I;
    TSearchRec Rec;
    QString S;
    I = FindFirst(PrgDataDir() + CheatDBSearchSubFolder + PathDelim + "*.dat", faAnyFile, Rec);
    while (I == 0) {
        if (SearchFileOK(PrgDataDir() + CheatDBSearchSubFolder + PathDelim + Rec.Name, S)) {
            AFiles->Add(PrgDataDir() + CheatDBSearchSubFolder + PathDelim + Rec.Name);
            ANames->Add(S);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

QString GetNewAddressSearchFile()
{
    int I = 0;
    while (FileExists(PrgDataDir() + CheatDBSearchSubFolder + PathDelim + "Search" + IntToStr(I) + ".dat")) I++;
    return PrgDataDir() + CheatDBSearchSubFolder + PathDelim + "Search" + IntToStr(I) + ".dat";
}

/* TAddressSearcher */

TAddressSearcher::TAddressSearcher()
{
    FName = "";
    FLastSavedGameFileName = "";
    FCompleteFile = nullptr;
    FAddressList = nullptr;
    FAddressSizeList = nullptr;
    FAddressLastValue = nullptr;
    FSearchDataFileName = "";
}

TAddressSearcher::~TAddressSearcher()
{
    Clear(false);
}

void TAddressSearcher::Clear(bool AKeepName)
{
    if (FCompleteFile != nullptr) { delete FCompleteFile; FCompleteFile = nullptr; }
    if (FAddressList != nullptr) { delete FAddressList; FAddressList = nullptr; }
    if (FAddressSizeList != nullptr) { delete FAddressSizeList; FAddressSizeList = nullptr; }
    if (FAddressLastValue != nullptr) { delete FAddressLastValue; FAddressLastValue = nullptr; }
    if (!AKeepName) { FName = ""; FLastSavedGameFileName = ""; }
}

bool TAddressSearcher::LoadFromFile(const QString& AFileName)
{
    QByteArray S;
    int I;
    quint8 Mode;
    bool result = false;
    Clear(false);
    FSearchDataFileName = AFileName;

    try {
        QFile FSt(AFileName);
        if (!FSt.open(QIODevice::ReadOnly)) throw EReadError("Cannot open file");
        if (FSt.size() < DFRSearch.length() + 5) return result;
        S.resize(DFRSearch.length());
        ReadBuf(FSt, S.data(), DFRSearch.length());
        if (S != DFRSearch) return result;
        I = ReadInt(FSt); FName = ReadAnsiString(FSt, I);
        I = ReadInt(FSt); FLastSavedGameFileName = ReadAnsiString(FSt, I);
        ReadBuf(FSt, &Mode, 1);
        if (Mode == 0) {
            /* Whole saved game stored */
            FCompleteFile = new QByteArray(FSt.readAll());
        } else {
            /* Load address list */
            FAddressList = new QList<int>;
            FAddressSizeList = new QList<int>;
            FAddressLastValue = new QList<int>;
            I = ReadInt(FSt);
            if (I > 0) {
                FAddressList->reserve(I);
                FAddressSizeList->reserve(I);
                FAddressLastValue->reserve(I);
            }
            for (int J = 0; J < I; J++) FAddressList->append(ReadInt(FSt));
            for (int J = 0; J < I; J++) FAddressSizeList->append(ReadInt(FSt));
            for (int J = 0; J < I; J++) FAddressLastValue->append(ReadInt(FSt));
        }
    } catch (...) {
        result = false; return result;
    }
    // The Delphi function never set result to True (the return value is not
    // used by the callers); report success here.
    result = true;
    return result;
}

void TAddressSearcher::SaveToFile(const QString& AFileName)
{
    QByteArray S;
    int I, J;
    quint8 Mode;
    try {
        QString FileName;
        if (Trim(AFileName) == "") {
            if (Trim(FSearchDataFileName) == "") FSearchDataFileName = GetNewAddressSearchFile();
            ForceDirectories(ExtractFilePath(FSearchDataFileName));
            FileName = FSearchDataFileName;
        } else {
            ForceDirectories(ExtractFilePath(AFileName));
            FileName = AFileName;
        }
        QFile FSt(FileName);
        if (!FSt.open(QIODevice::WriteOnly | QIODevice::Truncate)) throw Exception("Cannot create file");
        S = DFRSearch; WriteBuf(FSt, S.constData(), DFRSearch.length());
        S = EncodeAnsi(FName); I = S.length(); WriteInt(FSt, I); WriteBuf(FSt, S.constData(), S.length());
        S = EncodeAnsi(FLastSavedGameFileName); I = S.length(); WriteInt(FSt, I); WriteBuf(FSt, S.constData(), S.length());
        if (FCompleteFile != nullptr) {
            /* Store whole saved game */
            Mode = 0; WriteBuf(FSt, &Mode, 1);
            WriteBuf(FSt, FCompleteFile->constData(), FCompleteFile->size());
        } else {
            /* Store address list */
            Mode = 1; WriteBuf(FSt, &Mode, 1);
            if (FAddressList == nullptr) return; // Delphi: access violation, caught by the except block
            I = FAddressList->size(); WriteInt(FSt, I);
            for (I = 0; I < FAddressList->size(); I++) { J = (*FAddressList)[I]; WriteInt(FSt, J); }
            for (I = 0; I < FAddressSizeList->size(); I++) { J = (*FAddressSizeList)[I]; WriteInt(FSt, J); }
            for (I = 0; I < FAddressLastValue->size(); I++) { J = (*FAddressLastValue)[I]; WriteInt(FSt, J); }
        }
    } catch (...) {
        return;
    }
}

bool TAddressSearcher::LoadSavedGameFile(const QString& ASavedGameFile)
{
    bool result = false;
    Clear(true);

    if (!FileExists(ASavedGameFile)) return result;
    FCompleteFile = new QByteArray;
    if (!LoadMemoryStream(*FCompleteFile, ASavedGameFile)) {
        delete FCompleteFile; FCompleteFile = nullptr; result = false; return result;
    }
    FLastSavedGameFileName = ASavedGameFile;
    result = true;
    return result;
}

void TAddressSearcher::SearchAddressWholeFile(const QByteArray& AMSt, int AValue)
{
    int Address;
    auto AddAddress = [this, &Address, AValue](int ASize) { FAddressList->append(Address); FAddressSizeList->append(ASize); FAddressLastValue->append(AValue); };
    int I;
    quint16 W;
    quint8 B;
    for (Address = 0; Address <= AMSt.size() - 4; Address++) {
        I = MemInt(AMSt, Address);
        if (I == AValue) AddAddress(4);
    }
    if ((AValue / 0x10000) == 0) {
        /* Check word sized values */
        for (Address = 0; Address <= AMSt.size() - 2; Address++) {
            W = MemWord(AMSt, Address);
            if (W == AValue) AddAddress(2);
        }
    }
    if ((AValue / 0x100) == 0) {
        /* Check byte sized values */
        for (Address = 0; Address <= AMSt.size() - 1; Address++) {
            B = MemByte(AMSt, Address);
            if (B == AValue) AddAddress(1);
        }
    }
}

void TAddressSearcher::SearchAddressInList(const QByteArray& AMSt, int AValue)
{
    int I, J, S;
    quint16 W;
    quint8 B;
    auto DeleteI = [this, &I]() { FAddressList->removeAt(I); FAddressSizeList->removeAt(I); FAddressLastValue->removeAt(I); };
    I = 0;
    while (I < FAddressList->size()) {
        S = (*FAddressSizeList)[I];
        if ((*FAddressList)[I] + S > AMSt.size()) {
            DeleteI();
            continue;
        }
        const int Position = (*FAddressList)[I];
        if (S == 4) {
            J = MemInt(AMSt, Position);
            if (J == AValue) { (*FAddressLastValue)[I] = AValue; I++; } else { DeleteI(); }
            continue;
        }
        if (S == 2) {
            W = MemWord(AMSt, Position);
            if (W == AValue) { (*FAddressLastValue)[I] = AValue; I++; } else { DeleteI(); }
            continue;
        }
        if (S == 1) {
            B = MemByte(AMSt, Position);
            if (B == AValue) { (*FAddressLastValue)[I] = AValue; I++; } else { DeleteI(); }
            continue;
        }
        // Delphi: endless loop for invalid sizes; drop the entry instead
        DeleteI();
    }
}

void TAddressSearcher::SearchAddress(const QString& ASavedGameFile, int AValue)
{
    QByteArray MSt;
    if (!LoadMemoryStream(MSt, ASavedGameFile)) return;
    if (FCompleteFile != nullptr) { delete FCompleteFile; FCompleteFile = nullptr; } /* Complete old file is useless on exact value search */
    if (FAddressList != nullptr) {
        /* Only search on already found addresses */
        SearchAddressInList(MSt, AValue);
    } else {
        /* Search whole file */
        FAddressList = new QList<int>;
        FAddressSizeList = new QList<int>;
        FAddressLastValue = new QList<int>;
        SearchAddressWholeFile(MSt, AValue);
    }
    FLastSavedGameFileName = ASavedGameFile;
}

void TAddressSearcher::SearchAddressIndirectAgainstWholeFile(const QByteArray& AMSt, TValueChangeType AValueChangeType)
{
    int Address;
    auto AddAddress = [this, &Address](int ASize, int AValue) { FAddressList->append(Address); FAddressSizeList->append(ASize); FAddressLastValue->append(AValue); };
    int I1, I2;
    quint16 W1, W2;
    quint8 B1, B2;
    const int MinSize = Min<int>(FCompleteFile->size(), AMSt.size());
    for (Address = 0; Address <= MinSize - 4; Address++) {
        I1 = MemInt(*FCompleteFile, Address); I2 = MemInt(AMSt, Address);
        if (((AValueChangeType == vctUp) && (I2 > I1)) || ((AValueChangeType == vctDown) && (I2 < I1))) AddAddress(4, I2);
    }
    for (Address = 0; Address <= MinSize - 2; Address++) {
        W1 = MemWord(*FCompleteFile, Address); W2 = MemWord(AMSt, Address);
        if (((AValueChangeType == vctUp) && (W2 > W1)) || ((AValueChangeType == vctDown) && (W2 < W1))) AddAddress(2, W2);
    }
    for (Address = 0; Address <= MinSize - 1; Address++) {
        B1 = MemByte(*FCompleteFile, Address); B2 = MemByte(AMSt, Address);
        if (((AValueChangeType == vctUp) && (B2 > B1)) || ((AValueChangeType == vctDown) && (B2 < B1))) AddAddress(1, B2);
    }
}

void TAddressSearcher::SearchAddressIndirectAgainstAddressList(const QByteArray& AMSt, TValueChangeType AValueChangeType)
{
    int I, J, S, OldValue;
    quint16 W;
    quint8 B;
    auto DeleteI = [this, &I]() { FAddressList->removeAt(I); FAddressSizeList->removeAt(I); FAddressLastValue->removeAt(I); };
    I = 0;
    while (I < FAddressList->size()) {
        S = (*FAddressSizeList)[I];
        OldValue = (*FAddressLastValue)[I];
        if ((*FAddressList)[I] + S > AMSt.size()) {
            DeleteI();
            continue;
        }
        const int Position = (*FAddressList)[I];
        if (S == 4) {
            J = MemInt(AMSt, Position);
            if (((AValueChangeType == vctUp) && (J > OldValue)) || ((AValueChangeType == vctDown) && (J < OldValue))) { (*FAddressLastValue)[I] = J; I++; } else { DeleteI(); }
            continue;
        }
        if (S == 2) {
            W = MemWord(AMSt, Position);
            if (((AValueChangeType == vctUp) && (W > OldValue)) || ((AValueChangeType == vctDown) && (W < OldValue))) { J = W; (*FAddressLastValue)[I] = J; I++; } else { DeleteI(); }
            continue;
        }
        if (S == 1) {
            B = MemByte(AMSt, Position);
            if (((AValueChangeType == vctUp) && (B > OldValue)) || ((AValueChangeType == vctDown) && (B < OldValue))) { J = B; (*FAddressLastValue)[I] = J; I++; } else { DeleteI(); }
            continue;
        }
        // Delphi: endless loop for invalid sizes; drop the entry instead
        DeleteI();
    }
}

void TAddressSearcher::SearchAddressIndirect(const QString& ASavedGameFile, TValueChangeType AValueChangeType)
{
    QByteArray MSt;
    if (!LoadMemoryStream(MSt, ASavedGameFile)) return;
    if (FCompleteFile != nullptr) {
        /* Match changes against whole file */
        FAddressList = new QList<int>;
        FAddressSizeList = new QList<int>;
        FAddressLastValue = new QList<int>;
        SearchAddressIndirectAgainstWholeFile(MSt, AValueChangeType);
        delete FCompleteFile; FCompleteFile = nullptr;
    } else {
        /* Match changes against adress list */
        if (FAddressList == nullptr) return; // Delphi: access violation
        SearchAddressIndirectAgainstAddressList(MSt, AValueChangeType);
    }
    FLastSavedGameFileName = ASavedGameFile;
}

bool TAddressSearcher::NoResults()
{
    return (FAddressList == nullptr) || (FAddressList->size() == 0);
}

TResultStatus TAddressSearcher::GetResult(int& AAddress, int& ASize)
{
    int MaxBytes, MaxNr;
    TResultStatus result;
    if (FAddressList == nullptr) { result = rsNoResultsYet; return result; }
    if (FAddressList->size() == 0) { result = rsNoAddress; return result; }
    result = rsMultipleAddresses;

    MaxNr = 0;
    MaxBytes = (*FAddressSizeList)[0];
    for (int I = 1; I < FAddressList->size(); I++) {
        if ((*FAddressList)[0] != (*FAddressList)[I]) return result;
        if ((*FAddressSizeList)[I] > MaxBytes) {
            MaxNr = I;
            MaxBytes = (*FAddressSizeList)[I];
        }
    }

    result = rsFound;
    AAddress = (*FAddressList)[MaxNr];
    ASize = MaxBytes;
    return result;
}

bool TAddressSearcher::GetResultNr(int ANr, int& AAddress, int& ASize)
{
    bool result = false;
    if ((FAddressList == nullptr) || (FAddressList->size() == 0) || (ANr < 0) || (ANr >= FAddressList->size())) return result;
    ASize = (*FAddressSizeList)[ANr];
    AAddress = (*FAddressList)[ANr];
    result = true;
    return result;
}

TStringList* TAddressSearcher::GetList()
{
    QList<int> Addresses;
    TStringList* result = new TStringList;
    if (FAddressList == nullptr) return result;
    for (int I = 0; I < FAddressList->size(); I++) {
        if (Addresses.indexOf((*FAddressList)[I]) >= 0) continue;
        Addresses.append((*FAddressList)[I]);
        result->AddObject(
            IntToHex((*FAddressList)[I], 1) + "h=" + IntToStr((*FAddressList)[I]) + "d " +
            "(" + IntToStr((*FAddressSizeList)[I]) + " " + LanguageSetup->Bytes() + ", " +
            LanguageSetup->SearchAddressResultMessageMultipleAddressesCurrentValue() + ": " + IntToStr((*FAddressLastValue)[I]) + ")", static_cast<intptr_t>(I));
    }
    return result;
}
