// C++ port of FileNameConvertor.pas (implementation section).
#include "FileNameConvertor.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

// Delphi Classes.MaxListSize
static const int MaxListSize = MaxInt / 16;

// SysUtils.AnsiCompareFileName (Windows: case insensitive comparison)
static int AnsiCompareFileName(const QString& S1, const QString& S2)
{
    return CompareStr(AnsiUpperCase(S1), AnsiUpperCase(S2));
}

// Directory listing in the order FindFirst/FindNext return the entries on
// Windows (NTFS: sorted by the upper case names, "." and ".." first).  The
// short name numbering (~1, ~2, ...) depends on this order, so it must not
// differ from the original (the compat FindFirst lists directories first).
struct TDirEntry { QString Name; bool IsDir; };
static QList<TDirEntry> ReadDirEntries(const QString& ParentDir)
{
    QList<TDirEntry> L;
    QDir D(ToNativePath(ParentDir));
    if (!D.exists()) return L;
    const QFileInfoList Infos = D.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const QFileInfo& FI : Infos) L.append({FI.fileName(), FI.isDir()});
    std::stable_sort(L.begin(), L.end(), [](const TDirEntry& A, const TDirEntry& B) {
        return CompareStr(A.Name.toUpper(), B.Name.toUpper()) < 0;
    });
    if (!D.isRoot()) {
        L.prepend({"..", true});
        L.prepend({".", true});
    }
    return L;
}

TFileNameConvertor::TFileNameConvertor()
{
    ItemList = new TList();
    LongNameList = new TList();
    LinuxMode = false;
}

TFileNameConvertor::TFileNameConvertor(const QString& ParentDir, bool UseLinuxeMode) : TFileNameConvertor()
{
    LinuxMode = UseLinuxeMode;
    ReadDir(ParentDir);
}

TFileNameConvertor::TFileNameConvertor(const QString& ParentDir, const QString& ReadUpToFile, bool UseLinuxeMode) : TFileNameConvertor()
{
    LinuxMode = UseLinuxeMode;
    ReadDir(ParentDir, ReadUpToFile);
}

TFileNameConvertor::~TFileNameConvertor()
{
    Clear();
    delete ItemList;
    delete LongNameList;
}

void TFileNameConvertor::Clear()
{
    int I;
    if (ItemList != nullptr) {
        for (I = 0; I < ItemList->count(); I++)
            if ((*ItemList)[I] != nullptr)
                delete static_cast<PFilenameInfo>((*ItemList)[I]);
        ItemList->clear();
    }
    if (LongNameList != nullptr) {
        for (I = 0; I < LongNameList->count(); I++)
            if ((*LongNameList)[I] != nullptr)
                delete static_cast<PLongName>((*LongNameList)[I]);
        LongNameList->clear();
    }
}

void TFileNameConvertor::ReadDir(const QString& ParentDir)
{
    ReadDir(ParentDir, "");
}

QString WinExpandLongPathName(const QString& ParentDir, const QString& ShortName)
{
    TSearchRec Rec;
    int I;
    QString result;
    QString Dir = IncludeTrailingBackslash(ParentDir);
    I = FindFirst(Dir + ShortName, faAnyFile & ~faVolumeID, Rec);
    if (I == 0)
        result = Rec.Name;
    else
        result = ShortName;
    FindClose(Rec);
    return result;
}

QString WinExtractShortPathName(const QString& ParentDir, const QString& LongName)
{
    QString result;
    QString Dir = IncludeTrailingBackslash(ParentDir);
#ifdef Q_OS_WIN
    wchar_t Buffer[MAX_PATH];
    QString Full = ToNativePath(Dir + LongName);
    DWORD Len = GetShortPathNameW(reinterpret_cast<LPCWSTR>(Full.utf16()), Buffer, MAX_PATH);
    if (Len == 0 || Len >= MAX_PATH)
        result = LongName;
    else {
        QString S = QString::fromWCharArray(Buffer, static_cast<int>(Len));
        result = ExtractFileName(S);
    }
#else
    // TODO(port): no system short names (8.3) outside Windows
    Q_UNUSED(Dir);
    result = LongName;
#endif
    return result;
}

void TFileNameConvertor::ReadDir(const QString& AParentDir, const QString& AReadUpToFile)
{
    int I;
    QString ParentDir = AParentDir;
    QString ReadUpToFile = AReadUpToFile;

    Clear();
    ParentDir = IncludeTrailingBackslash(ParentDir);
    if (!DirectoryExists(ParentDir)) return;

    if ((!LinuxMode) && (!ReadUpToFile.isEmpty()))
        ReadUpToFile = WinExpandLongPathName(ParentDir, ReadUpToFile);

    const QList<TDirEntry> Entries = ReadDirEntries(ParentDir);
    int Index = 0;
    I = Entries.isEmpty() ? 18 : 0;
    while ((I == 0) && (I < MaxListSize)) {
        const TDirEntry& Rec = Entries[Index];
        NewItem(Rec.Name, Rec.IsDir);
        Index++;
        I = (Index < Entries.size()) ? 0 : 18;
        // Delphi compares the name of the *next* record (FindNext already called)
        QString RecName = (I == 0) ? Entries[Index].Name : Rec.Name;
        if ((!ReadUpToFile.isEmpty()) &&
            (
              ((!LinuxMode) && (AnsiCompareFileName(RecName, ReadUpToFile) == 0))
              ||
              (LinuxMode && (CompareStr(RecName, ReadUpToFile) == 0))
            ))
            I = 1;
    }
}

static int CompareItems(void* Item1, void* Item2)
{
    if (Item1 == nullptr) {
        if (Item2 == nullptr) return 0; else return -1;
    } else if (Item2 == nullptr)
        return -1;
    else return CompareStr(static_cast<PFilenameInfo>(Item1)->ShortName, static_cast<PFilenameInfo>(Item2)->ShortName);
}

static int CompareLongItems(void* Item1, void* Item2)
{
    if (Item1 == nullptr) {
        if (Item2 == nullptr) return 0; else return -1;
    } else if (Item2 == nullptr)
        return -1;
    else
        return CompareStr(static_cast<PLongName>(Item1)->LongName, static_cast<PLongName>(Item2)->LongName);
}

static int CompareLongItemsAnsi(void* Item1, void* Item2)
{
    if (Item1 == nullptr) {
        if (Item2 == nullptr) return 0; else return -1;
    } else if (Item2 == nullptr)
        return -1;
    else
        return AnsiCompareFileName(static_cast<PLongName>(Item1)->LongName, static_cast<PLongName>(Item2)->LongName);
}

void TFileNameConvertor::SortItems()
{
    std::sort(ItemList->begin(), ItemList->end(), [](void* A, void* B) { return CompareItems(A, B) < 0; });
    if (LinuxMode)
        std::sort(LongNameList->begin(), LongNameList->end(), [](void* A, void* B) { return CompareLongItems(A, B) < 0; });
    else
        std::sort(LongNameList->begin(), LongNameList->end(), [](void* A, void* B) { return CompareLongItemsAnsi(A, B) < 0; });
}

int TFileNameConvertor::AddItemSorted(TList* List, void* Item, std::function<int(void*, void*)> SCompare)
{
    int InsertIndex;
    int I;
    InsertIndex = -1;
    if ((List->count() > 0) && (SCompare(Item, List->last()) < 0))
        for (I = 0; I < List->count(); I++)
            if (SCompare(Item, (*List)[I]) < 0) {
                InsertIndex = I;
                break;
            }
    if (InsertIndex >= 0) {
        List->insert(InsertIndex, Item);
        return InsertIndex;
    } else {
        List->append(Item);
        return List->count() - 1;
    }
}

int TFileNameConvertor::GetItemsCount()
{
    return ItemList->count();
}

TFileNameInfo TFileNameConvertor::GetItem(int Index)
{
    return *static_cast<PFilenameInfo>((*ItemList)[Index]);
}

void TFileNameConvertor::RemoveTrailingDot(const QString& AName)
{
    // The Delphi routine takes the name by value, so it has no effect at all.
    QString Name = AName;
    if ((Length(Name) > 0) && (Name != ".") && (Name != "..") && (Name[Length(Name) - 1] == '.'))
        Delete(Name, Length(Name), 1);
}

QString TFileNameConvertor::FindLongName(const QString& ShortName)
{
    int I;
    I = IndexofShortName(ShortName);
    if ((I >= 0) && (I < ItemList->count()))
        return static_cast<PFilenameInfo>((*ItemList)[I])->LongName;
    else return "";
}

QString TFileNameConvertor::FindShortName(const QString& LongName)
{
    int I;
    I = IndexofLongName(LongName);
    if ((I >= 0) && (I < ItemList->count()))
        return static_cast<PFilenameInfo>((*ItemList)[I])->ShortName;
    else return "";
}

int TFileNameConvertor::IndexofShortName(const QString& ShortName)
{
    int low, high, mid, res;
    RemoveTrailingDot(ShortName);
    low = 0;
    high = ItemList->count() - 1;
    while (low <= high) {
        mid = (low + high) / 2;
        res = CompareStr(ShortName, static_cast<PFilenameInfo>((*ItemList)[mid])->ShortName);
        if (res > 0) low = mid + 1;
        else if (res < 0) high = mid - 1;
        else {
            return mid;
        }
    }
    return -1;
}

int TFileNameConvertor::IndexofLongName(const QString& LongName)
{
    int low, high, mid, res;

    low = 0;
    high = LongNameList->count() - 1;
    while (low <= high) {
        mid = (low + high) / 2;
        if (LinuxMode)
            res = CompareStr(LongName, static_cast<PLongName>((*LongNameList)[mid])->LongName);
        else
            res = AnsiCompareFileName(LongName, static_cast<PLongName>((*LongNameList)[mid])->LongName);
        if (res > 0) low = mid + 1;
        else if (res < 0) high = mid - 1;
        else {
            return IndexofShortName(static_cast<PLongName>((*LongNameList)[mid])->fnInfo->ShortName);
        }
    }
    return -1;
}

void TFileNameConvertor::NewItem(const QString& LongName, bool isDir)
{
    TFileNameInfo Item;
    PFilenameInfo PItem;
    PLongName PLongN;

    Item.LongName = LongName;
    Item.ShortName = "";
    Item.isDir = isDir;
    Item.shortNumber = 0;
    CreateShortName(Item);
    PItem = new TFileNameInfo;
    *PItem = Item;
    AddItemSorted(ItemList, PItem, CompareItems);
    PLongN = new TLongName;
    PLongN->LongName = LongName;
    PLongN->fnInfo = PItem;

    if (LinuxMode)
        AddItemSorted(LongNameList, PLongN, CompareLongItems);
    else
        AddItemSorted(LongNameList, PLongN, CompareLongItemsAnsi);
}

bool TFileNameConvertor::RemoveSpaces(QString& S)
{
    int I;
    QString S1;
    S1 = "";
    for (I = 1; I <= Length(S); I++) if (S[I - 1] != ' ') S1 = S1 + S[I - 1];
    bool result = (S1 != S);
    S = S1;
    return result;
}

int TFileNameConvertor::CompareShortName(const QString& ComparedName, const QString& ListedName)
{
    int cps, compareCount1, numberSize, compareCount2;
    cps = Pos("~", ListedName);
    if (cps > 0) {
        compareCount1 = cps - 1;
        numberSize = Pos(".", Copy(ListedName, cps, MaxInt)) - 1;
        if (numberSize < 0) numberSize = Length(ListedName) - cps + 1;
        compareCount2 = Pos(".", ComparedName) - 1;
        if (compareCount2 < 0) compareCount2 = Length(ComparedName);
        if (compareCount2 > 8) compareCount2 = 8;
        if (compareCount2 > (compareCount1 + numberSize))
            compareCount1 = compareCount2 - numberSize;
        return CompareStr(Copy(ComparedName, 1, compareCount1), Copy(ListedName, 1, compareCount1));
    } else
        return CompareStr(ComparedName, ListedName);
}

int TFileNameConvertor::CreateShortNameNumber(const QString& ShortName)
{
    int low, high, mid, res;
    int result = 0;
    low = 0;
    high = ItemList->count() - 1;
    while (low <= high) {
        mid = (low + high) / 2;
        res = CompareShortName(ShortName, static_cast<PFilenameInfo>((*ItemList)[mid])->ShortName);
        if (res > 0) low = mid + 1;
        else if (res < 0) high = mid - 1;
        else {
            while ((mid < ItemList->count()) && (CompareShortName(ShortName, static_cast<PFilenameInfo>((*ItemList)[mid])->ShortName) == 0)) {
                result = static_cast<PFilenameInfo>((*ItemList)[mid])->shortNumber;
                mid++;
            }
            break;
        }
    }
    result++;
    return result;
}

void TFileNameConvertor::CreateShortName(TFileNameInfo& Item)
{
    int len, lenExt, ps;
    bool createShort;
    QString tmpName, numStr;

    tmpName = UpperCase(Item.LongName);
    createShort = RemoveSpaces(tmpName);
    ps = Pos(".", tmpName);
    if (ps > 0) {
        lenExt = Length(tmpName) - ps;
        if (lenExt > 3) {
            while ((!tmpName.isEmpty()) && (tmpName[0] == '.'))
                Delete(tmpName, 1, 1);
            createShort = true;
        }
        ps = Pos(".", tmpName);
        if (ps > 0) len = ps - 1; else len = Length(tmpName);
    } else len = Length(tmpName);

    createShort = createShort || (len > 8);
    if (!createShort)
        createShort = (!FindLongName(tmpName).isEmpty());

    if (createShort) {
        Item.shortNumber = CreateShortNameNumber(tmpName);
        numStr = IntToStr(Item.shortNumber);
        if ((len + Length(numStr) + 1) > 8)
            Item.ShortName = Copy(tmpName, 1, 8 - 1 - Length(numStr));
        else
            Item.ShortName = Copy(tmpName, 1, len);
        Item.ShortName = Item.ShortName + "~" + numStr;
        if (ps > 0) {
            ps = tmpName.lastIndexOf('.') + 1;  // LastDelimiter('.',tmpName)
            if (ps > 0)
                Item.ShortName = Item.ShortName + Copy(tmpName, ps, 4);
        }
    } else Item.ShortName = tmpName;
    RemoveTrailingDot(Item.ShortName);
}

static bool IsOnlyDots(const QString& FileName)
{
    int I;
    bool result = false;
    if (FileName.isEmpty()) return result;
    result = true;
    for (I = 1; I <= Length(FileName); I++)
        if (FileName[I - 1] != '.') result = false;
    return result;
}

static bool HasForbiddenChars(const QString& FileName, bool UseExtendedAscii)
{
    static const QString AllowedDOSBoxFileNameChars =
        QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ") +
        QStringLiteral("abcdefghijklmnopqrstuvwxyz") +
        QStringLiteral("01234567890") +
        QStringLiteral("$#@()") +
        QStringLiteral("!%{}`~") +
        QStringLiteral("_-.&'+^") +
        QChar(246) + QChar(255) + QChar(0xa0) + QChar(0xe5);
    int I, Dot;
    Dot = Pos(".", FileName);
    if (!IsOnlyDots(FileName) &&
        (Dot > 0) && (Dot < Length(FileName)) && (Pos(".", Copy(FileName, Dot + 1, MaxInt)) > 0)) {
        return true;
    }
    for (I = 1; I <= Length(FileName); I++) {
        if ((Pos(FileName[I - 1], AllowedDOSBoxFileNameChars) <= 0) &&
            ((FileName[I - 1].unicode() < 0x80) || (!UseExtendedAscii))) {
            return true;
        }
    }
    return false;
}

TConvertResult ConvertLongFileNameToShort(const QString& AParentDir, const QString& LongName, bool UseLinuxMode, bool UseSystemShortName, bool UseSystemFallback, bool UseExtendedAscii, QString& ShortName, bool& isDir)
{
    QString RealLongName;
    QString RealShortName;
    QString TestLongName;
    TFileNameConvertor* Dir;
    TConvertResult result;
    QString ParentDir;

    ShortName = LongName;
    isDir = false;
    ParentDir = IncludeTrailingBackslash(AParentDir);
    if (!DirectoryExists(ParentDir)) {
        return tcrErrParentNotFound;
    }
    if ((Pos("\\", LongName) > 0) || (Pos("/", LongName) > 0) || (Pos(":", LongName) > 0)) {
        return tcrErrDelimiterInFileName;
    }
    if (!FileExists(ParentDir + LongName)) {
        if (!DirectoryExists(ParentDir + LongName)) {
            return tcrErrFileNotFound;
        } else isDir = true;
    }
    if (IsOnlyDots(LongName)) {
        return tcrOk;
    }

    RealLongName = WinExpandLongPathName(ParentDir, LongName);

    if (UseSystemShortName) {
        if (UseLinuxMode) ShortName = LongName;
        else ShortName = WinExtractShortPathName(ParentDir, RealLongName);
        return tcrOkExternal;
    }

    Dir = new TFileNameConvertor(ParentDir, UseLinuxMode);
    try {
        ShortName = Dir->FindShortName(RealLongName);
        if (!HasForbiddenChars(ShortName, UseExtendedAscii))
            result = tcrOk;
        else if (UseLinuxMode || !UseSystemFallback)
            result = tcrWarnInvalidChars;
        else {
            RealShortName = WinExtractShortPathName(ParentDir, LongName);
            TestLongName = Dir->FindLongName(RealShortName);
            if ((TestLongName.isEmpty()) || (TestLongName == RealLongName)) {
                ShortName = RealShortName;
                result = tcrWarnSystemShort;
            } else result = tcrWarnSystemShortAlreadyUsed;
        }
    } catch (...) {
        delete Dir;
        throw;
    }
    delete Dir;
    return result;
}
