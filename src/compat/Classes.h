// Delphi Classes unit compatibility: TStringList (with Objects, Values, Sorted,
// case insensitive IndexOf ...) and a few helpers.
#pragma once

#include "SysUtils.h"
#include <QList>
#include <QStringList>
#include <functional>
#include <QtGlobal>
#include <Qt>

class QObject;

// Common Delphi types used in declarations
using THandle = qint64;                              // process id (see Process.h)
using TNotifyEvent = std::function<void(QObject *)>;
using TShiftState = Qt::KeyboardModifiers;
using TList = QList<void *>;                         // untyped TList
using PString = QString *;
using TStringDynArray = QStringList;

enum TDuplicates { dupIgnore, dupAccept, dupError };
enum TCloseAction { caNone, caHide, caFree, caMinimize };

class TStringList
{
public:
    TStringList() = default;
    TStringList(const QStringList &L);
    TStringList(const TStringList &) = default;
    TStringList &operator=(const TStringList &) = default;
    virtual ~TStringList() = default;

    // --- access
    int Count() const { return FItems.size(); }
    bool IsEmpty() const { return FItems.isEmpty(); }
    QString &operator[](int I) { return FItems[I].S; }
    const QString &operator[](int I) const { return FItems[I].S; }
    QString Strings(int I) const { return FItems[I].S; }
    void SetStrings(int I, const QString &S) { FItems[I].S = S; }
    void *Objects(int I) const { return FItems[I].O; }
    void SetObjects(int I, void *O) { FItems[I].O = O; }
    template <typename T> T *ObjectAs(int I) const { return static_cast<T *>(FItems[I].O); }
    int ObjectAsInt(int I) const { return static_cast<int>(reinterpret_cast<intptr_t>(FItems[I].O)); }

    // --- modification
    int Add(const QString &S);
    int AddObject(const QString &S, void *O);
    int AddObject(const QString &S, intptr_t IntObject) { return AddObject(S, reinterpret_cast<void *>(IntObject)); }
    void AddStrings(const TStringList &L);
    void AddStrings(const QStringList &L);
    void Insert(int Index, const QString &S);
    void InsertObject(int Index, const QString &S, void *O);
    void Delete(int Index);
    void Clear();
    void Exchange(int I1, int I2);
    void Move(int CurIndex, int NewIndex);
    void Assign(const TStringList &Source);
    void Assign(const QStringList &Source);

    // --- searching
    int IndexOf(const QString &S) const;
    int IndexOfName(const QString &Name) const;
    int IndexOfObject(const void *O) const;
    bool Find(const QString &S, int &Index) const;   // only for sorted lists

    // --- sorting
    void Sort();
    void CustomSort(const std::function<int(const TStringList &, int, int)> &Compare);
    bool Sorted() const { return FSorted; }
    void SetSorted(bool Value);
    TDuplicates Duplicates() const { return FDuplicates; }
    void SetDuplicates(TDuplicates D) { FDuplicates = D; }
    bool CaseSensitive() const { return FCaseSensitive; }
    void SetCaseSensitive(bool B) { FCaseSensitive = B; if (FSorted) Sort(); }

    // --- name=value pairs
    QChar NameValueSeparator() const { return FNameValueSeparator; }
    void SetNameValueSeparator(QChar C) { FNameValueSeparator = C; }
    QString Names(int I) const;
    QString ValueFromIndex(int I) const;
    void SetValueFromIndex(int I, const QString &V);
    QString Values(const QString &Name) const;
    void SetValue(const QString &Name, const QString &Value);   // Values[Name]:=Value (empty deletes)

    // --- text conversion
    QString Text() const;               // lines joined with LineBreak (incl. trailing)
    void SetText(const QString &S);
    QString CommaText() const;
    void SetCommaText(const QString &S);
    QChar Delimiter() const { return FDelimiter; }
    void SetDelimiter(QChar C) { FDelimiter = C; }
    QChar QuoteChar() const { return FQuoteChar; }
    void SetQuoteChar(QChar C) { FQuoteChar = C; }
    bool StrictDelimiter() const { return FStrictDelimiter; }
    void SetStrictDelimiter(bool B) { FStrictDelimiter = B; }
    QString DelimitedText() const;
    void SetDelimitedText(const QString &S);

    // --- files
    bool LoadFromFile(const QString &FileName);
    bool SaveToFile(const QString &FileName) const;
    bool SaveToFileUtf8(const QString &FileName, bool Bom = false) const;
    bool LoadedAsUtf8() const { return FUtf8; }

    // --- conversion
    QStringList ToQStringList() const;
    operator QStringList() const { return ToQStringList(); }

    // --- iteration (range based for over the strings)
    class const_iterator {
    public:
        const_iterator(const TStringList *L, int I) : FL(L), FI(I) {}
        const QString &operator*() const { return (*FL)[FI]; }
        const_iterator &operator++() { ++FI; return *this; }
        bool operator!=(const const_iterator &O) const { return FI != O.FI; }
    private:
        const TStringList *FL;
        int FI;
    };
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, Count()); }

private:
    struct Item { QString S; void *O = nullptr; };
    QList<Item> FItems;
    bool FSorted = false;
    bool FCaseSensitive = false;
    TDuplicates FDuplicates = dupIgnore;
    QChar FNameValueSeparator = '=';
    QChar FDelimiter = ',';
    QChar FQuoteChar = '"';
    bool FStrictDelimiter = false;
    bool FUtf8 = false;

    int CompareStrings(const QString &A, const QString &B) const;
    int InsertSorted(const QString &S, void *O);
};

using TStrings = TStringList;
