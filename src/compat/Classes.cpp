#include "Classes.h"

#include <algorithm>

TStringList::TStringList(const QStringList &L)
{
    Assign(L);
}

int TStringList::CompareStrings(const QString &A, const QString &B) const
{
    return QString::compare(A, B, FCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

int TStringList::InsertSorted(const QString &S, void *O)
{
    int Index;
    if (Find(S, Index)) {
        if (FDuplicates == dupIgnore) return Index;
        if (FDuplicates == dupError) throw Exception("String list does not allow duplicates");
    }
    FItems.insert(Index, Item{S, O});
    return Index;
}

int TStringList::Add(const QString &S) { return AddObject(S, nullptr); }

int TStringList::AddObject(const QString &S, void *O)
{
    if (FSorted) return InsertSorted(S, O);
    FItems.append(Item{S, O});
    return FItems.size() - 1;
}

void TStringList::AddStrings(const TStringList &L)
{
    for (int i = 0; i < L.Count(); i++) AddObject(L[i], L.Objects(i));
}

void TStringList::AddStrings(const QStringList &L)
{
    for (const QString &S : L) Add(S);
}

void TStringList::Insert(int Index, const QString &S) { InsertObject(Index, S, nullptr); }

void TStringList::InsertObject(int Index, const QString &S, void *O)
{
    if (FSorted) { InsertSorted(S, O); return; }
    if (Index < 0) Index = 0;
    if (Index > FItems.size()) Index = FItems.size();
    FItems.insert(Index, Item{S, O});
}

void TStringList::Delete(int Index)
{
    if (Index >= 0 && Index < FItems.size()) FItems.removeAt(Index);
}

void TStringList::Clear() { FItems.clear(); }

void TStringList::Exchange(int I1, int I2)
{
    if (I1 == I2) return;
    FItems.swapItemsAt(I1, I2);
}

void TStringList::Move(int CurIndex, int NewIndex)
{
    if (CurIndex == NewIndex) return;
    FItems.move(CurIndex, NewIndex);
}

void TStringList::Assign(const TStringList &Source)
{
    if (&Source == this) return;
    FItems = Source.FItems;
    if (FSorted) Sort();
}

void TStringList::Assign(const QStringList &Source)
{
    FItems.clear();
    FItems.reserve(Source.size());
    for (const QString &S : Source) FItems.append(Item{S, nullptr});
    if (FSorted) Sort();
}

int TStringList::IndexOf(const QString &S) const
{
    if (FSorted) {
        int I;
        return Find(S, I) ? I : -1;
    }
    for (int i = 0; i < FItems.size(); i++)
        if (CompareStrings(FItems[i].S, S) == 0) return i;
    return -1;
}

int TStringList::IndexOfName(const QString &Name) const
{
    for (int i = 0; i < FItems.size(); i++) {
        const QString &S = FItems[i].S;
        int P = S.indexOf(FNameValueSeparator);
        if (P >= 0 && CompareStrings(S.left(P), Name) == 0) return i;
    }
    return -1;
}

int TStringList::IndexOfObject(const void *O) const
{
    for (int i = 0; i < FItems.size(); i++)
        if (FItems[i].O == O) return i;
    return -1;
}

bool TStringList::Find(const QString &S, int &Index) const
{
    int L = 0, H = FItems.size() - 1;
    bool Result = false;
    while (L <= H) {
        int I = (L + H) >> 1;
        int C = CompareStrings(FItems[I].S, S);
        if (C < 0) L = I + 1;
        else {
            H = I - 1;
            if (C == 0) {
                Result = true;
                if (FDuplicates != dupAccept) L = I;
            }
        }
    }
    Index = L;
    return Result;
}

void TStringList::Sort()
{
    std::stable_sort(FItems.begin(), FItems.end(), [this](const Item &A, const Item &B) {
        return CompareStrings(A.S, B.S) < 0;
    });
}

void TStringList::CustomSort(const std::function<int(const TStringList &, int, int)> &Compare)
{
    // Sort an index permutation so that the compare function can access the
    // list by index like in Delphi.
    QList<int> idx(FItems.size());
    for (int i = 0; i < idx.size(); i++) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](int A, int B) { return Compare(*this, A, B) < 0; });
    QList<Item> N;
    N.reserve(FItems.size());
    for (int i : idx) N.append(FItems[i]);
    FItems = N;
}

void TStringList::SetSorted(bool Value)
{
    if (FSorted == Value) return;
    FSorted = Value;
    if (FSorted) Sort();
}

QString TStringList::Names(int I) const
{
    const QString &S = FItems[I].S;
    int P = S.indexOf(FNameValueSeparator);
    return P >= 0 ? S.left(P) : QString();
}

QString TStringList::ValueFromIndex(int I) const
{
    const QString &S = FItems[I].S;
    int P = S.indexOf(FNameValueSeparator);
    return P >= 0 ? S.mid(P + 1) : QString();
}

void TStringList::SetValueFromIndex(int I, const QString &V)
{
    if (V.isEmpty()) { Delete(I); return; }
    QString N = Names(I);
    FItems[I].S = N + FNameValueSeparator + V;
}

QString TStringList::Values(const QString &Name) const
{
    int I = IndexOfName(Name);
    return I >= 0 ? ValueFromIndex(I) : QString();
}

void TStringList::SetValue(const QString &Name, const QString &Value)
{
    int I = IndexOfName(Name);
    if (Value.isEmpty()) {
        if (I >= 0) Delete(I);
        return;
    }
    if (I < 0) Add(Name + FNameValueSeparator + Value);
    else FItems[I].S = Name + FNameValueSeparator + Value;
}

QString TStringList::Text() const
{
    QString R;
    for (const Item &It : FItems) { R += It.S; R += LineBreak; }
    return R;
}

void TStringList::SetText(const QString &S)
{
    FItems.clear();
    if (S.isEmpty()) return;
    QString T = S;
    T.replace("\r\n", "\n");
    T.replace('\r', '\n');
    QStringList L = T.split('\n');
    if (!L.isEmpty() && L.last().isEmpty()) L.removeLast();
    for (const QString &X : L) AddObject(X, nullptr);
}

static QString quoteIfNeeded(const QString &S, QChar Delimiter, QChar Quote, bool Strict)
{
    bool need = S.isEmpty() && false;
    for (QChar c : S) {
        if (c == Quote || c == Delimiter || (!Strict && c.unicode() <= ' ')) { need = true; break; }
    }
    if (!need) return S;
    QString R = S;
    R.replace(Quote, QString(2, Quote));
    return Quote + R + Quote;
}

QString TStringList::CommaText() const
{
    QString R;
    for (int i = 0; i < FItems.size(); i++) {
        if (i) R += ',';
        R += quoteIfNeeded(FItems[i].S, ',', '"', false);
    }
    return R;
}

void TStringList::SetCommaText(const QString &S)
{
    QChar d = FDelimiter, q = FQuoteChar;
    bool st = FStrictDelimiter;
    FDelimiter = ',';
    FQuoteChar = '"';
    FStrictDelimiter = false;
    SetDelimitedText(S);
    FDelimiter = d;
    FQuoteChar = q;
    FStrictDelimiter = st;
}

QString TStringList::DelimitedText() const
{
    QString R;
    for (int i = 0; i < FItems.size(); i++) {
        if (i) R += FDelimiter;
        R += quoteIfNeeded(FItems[i].S, FDelimiter, FQuoteChar, FStrictDelimiter);
    }
    return R;
}

void TStringList::SetDelimitedText(const QString &S)
{
    FItems.clear();
    int i = 0;
    const int n = S.length();
    auto isSpace = [&](QChar c) { return !FStrictDelimiter && c.unicode() <= ' ' && c.unicode() > 0; };
    while (i < n && isSpace(S[i])) i++;
    while (i < n) {
        QString Item;
        if (S[i] == FQuoteChar) {
            i++;
            while (i < n) {
                if (S[i] == FQuoteChar) {
                    if (i + 1 < n && S[i + 1] == FQuoteChar) { Item += FQuoteChar; i += 2; continue; }
                    i++;
                    break;
                }
                Item += S[i++];
            }
        } else {
            while (i < n && S[i] != FDelimiter && !isSpace(S[i])) Item += S[i++];
        }
        Add(Item);
        while (i < n && isSpace(S[i])) i++;
        if (i < n && S[i] == FDelimiter) {
            i++;
            if (i >= n) Add(QString());
            while (i < n && isSpace(S[i])) i++;
        }
    }
}

bool TStringList::LoadFromFile(const QString &FileName)
{
    bool Ok = false;
    QString T = ReadTextFile(FileName, &Ok, &FUtf8);
    if (!Ok) return false;
    SetText(T);
    return true;
}

bool TStringList::SaveToFile(const QString &FileName) const
{
    return WriteTextFile(FileName, Text(), FUtf8, false);
}

bool TStringList::SaveToFileUtf8(const QString &FileName, bool Bom) const
{
    return WriteTextFile(FileName, Text(), true, Bom);
}

QStringList TStringList::ToQStringList() const
{
    QStringList R;
    R.reserve(FItems.size());
    for (const Item &It : FItems) R.append(It.S);
    return R;
}
