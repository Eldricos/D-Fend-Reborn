#include "IniFiles.h"

TMemIniFile::TMemIniFile(const QString &FileName) : FFileName(FileName)
{
    Load();
}

void TMemIniFile::Load()
{
    FSections.clear();
    FPreamble.clear();
    FSectionIndexValid = false;
    if (FFileName.isEmpty() || !FileExists(FFileName)) return;
    bool Ok = false;
    QString Text = ReadTextFile(FFileName, &Ok, &FUtf8);
    if (!Ok) return;
    TStringList L;
    L.SetText(Text);
    SetStrings(L);
    FModified = false;
}

void TMemIniFile::SetStrings(const TStringList &List)
{
    FSections.clear();
    FSectionIndexValid = false;
    FPreamble.clear();
    int Cur = -1;
    for (int i = 0; i < List.Count(); i++) {
        QString S = Trim(List[i]);
        if (S.isEmpty() || S.startsWith(';')) {
            Line L;
            L.IsKey = false;
            L.Value = S;
            if (Cur >= 0) FSections[Cur].Lines.append(L); else FPreamble.append(L);
            continue;
        }
        if (S.startsWith('[') && S.endsWith(']')) {
            QString Name = S.mid(1, S.length() - 2);
            int Idx = FindSection(Name);
            if (Idx < 0) {
                FSections.append(Section{Name, {}});
                Idx = FSections.size() - 1;
                FSectionIndexValid = false;
            }
            Cur = Idx;
            continue;
        }
        Line L;
        int P = S.indexOf('=');
        if (P >= 0) {
            L.Name = Trim(S.left(P));
            L.Value = S.mid(P + 1);
        } else {
            L.Name = S;
            L.Value = QString();
        }
        if (Cur >= 0) FSections[Cur].Lines.append(L); else { L.IsKey = false; L.Value = S; FPreamble.append(L); }
    }
}

void TMemIniFile::GetStrings(TStringList &List) const
{
    List.Clear();
    for (const Line &L : FPreamble) List.Add(L.Value);
    for (const Section &S : FSections) {
        List.Add("[" + S.Name + "]");
        for (const Line &L : S.Lines) {
            if (L.IsKey) List.Add(L.Name + "=" + L.Value);
            else List.Add(L.Value);
        }
        List.Add(QString());
    }
}

// Section and key lookups use hash indexes (case insensitive like Delphi);
// files with many keys (language files: >2000 entries) are read and written
// often, linear searches made that noticeably slow.
int TMemIniFile::FindSection(const QString &Name) const
{
    if (!FSectionIndexValid) {
        FSectionIndex.clear();
        FSectionIndex.reserve(FSections.size());
        for (int i = 0; i < FSections.size(); i++) {
            QString K = FSections[i].Name.toCaseFolded();
            if (!FSectionIndex.contains(K)) FSectionIndex.insert(K, i);
        }
        FSectionIndexValid = true;
    }
    return FSectionIndex.value(Name.toCaseFolded(), -1);
}

int TMemIniFile::FindKey(const Section &S, const QString &Name) const
{
    if (!S.KeyIndexValid) {
        S.KeyIndex.clear();
        S.KeyIndex.reserve(S.Lines.size());
        for (int i = 0; i < S.Lines.size(); i++) {
            if (!S.Lines[i].IsKey) continue;
            QString K = S.Lines[i].Name.toCaseFolded();
            if (!S.KeyIndex.contains(K)) S.KeyIndex.insert(K, i);
        }
        S.KeyIndexValid = true;
    }
    return S.KeyIndex.value(Name.toCaseFolded(), -1);
}

void TMemIniFile::Rename(const QString &FileName, bool Reload)
{
    FFileName = FileName;
    if (Reload) Load();
}

QString TMemIniFile::ReadString(const QString &Section, const QString &Ident, const QString &Default) const
{
    int S = FindSection(Section);
    if (S < 0) return Default;
    int K = FindKey(FSections[S], Ident);
    if (K < 0) return Default;
    return FSections[S].Lines[K].Value;
}

void TMemIniFile::WriteString(const QString &SectionName, const QString &Ident, const QString &Value)
{
    int S = FindSection(SectionName);
    if (S < 0) {
        FSections.append(Section{SectionName, {}});
        S = FSections.size() - 1;
        if (FSectionIndexValid) FSectionIndex.insert(SectionName.toCaseFolded(), S);
    }
    Section &Sec = FSections[S];
    int K = FindKey(Sec, Ident);
    if (K >= 0) {
        Sec.Lines[K].Value = Value;
    } else {
        // insert after the last key (before trailing empty lines)
        int Pos = Sec.Lines.size();
        while (Pos > 0 && !Sec.Lines[Pos - 1].IsKey && Sec.Lines[Pos - 1].Value.isEmpty()) Pos--;
        Line L;
        L.Name = Ident;
        L.Value = Value;
        Sec.Lines.insert(Pos, L);
        // all keys are before Pos (only trailing empty lines follow), so the
        // index stays valid and only needs the new key
        if (Sec.KeyIndexValid) Sec.KeyIndex.insert(Ident.toCaseFolded(), Pos);
    }
    FModified = true;
}

int TMemIniFile::ReadInteger(const QString &Section, const QString &Ident, int Default) const
{
    QString S = ReadString(Section, Ident, QString());
    if (S.isEmpty()) return Default;
    return StrToIntDef(S, Default);
}

void TMemIniFile::WriteInteger(const QString &Section, const QString &Ident, int Value)
{
    WriteString(Section, Ident, IntToStr(Value));
}

bool TMemIniFile::ReadBool(const QString &Section, const QString &Ident, bool Default) const
{
    return ReadInteger(Section, Ident, Default ? 1 : 0) != 0;
}

void TMemIniFile::WriteBool(const QString &Section, const QString &Ident, bool Value)
{
    WriteString(Section, Ident, Value ? "1" : "0");
}

double TMemIniFile::ReadFloat(const QString &Section, const QString &Ident, double Default) const
{
    QString S = ReadString(Section, Ident, QString());
    if (S.isEmpty()) return Default;
    return StrToFloatDef(S, Default);
}

void TMemIniFile::WriteFloat(const QString &Section, const QString &Ident, double Value)
{
    WriteString(Section, Ident, FloatToStr(Value));
}

TDateTime TMemIniFile::ReadDateTime(const QString &Section, const QString &Ident, TDateTime Default) const
{
    QString S = ReadString(Section, Ident, QString());
    if (S.isEmpty()) return Default;
    return StrToDateTimeDef(S, Default);
}

void TMemIniFile::WriteDateTime(const QString &Section, const QString &Ident, TDateTime Value)
{
    WriteString(Section, Ident, DateTimeToStr(Value));
}

bool TMemIniFile::SectionExists(const QString &Section) const
{
    return FindSection(Section) >= 0;
}

bool TMemIniFile::ValueExists(const QString &Section, const QString &Ident) const
{
    int S = FindSection(Section);
    return S >= 0 && FindKey(FSections[S], Ident) >= 0;
}

void TMemIniFile::ReadSection(const QString &Section, TStringList &Strings) const
{
    Strings.Clear();
    int S = FindSection(Section);
    if (S < 0) return;
    for (const Line &L : FSections[S].Lines)
        if (L.IsKey) Strings.Add(L.Name);
}

void TMemIniFile::ReadSections(TStringList &Strings) const
{
    Strings.Clear();
    for (const Section &S : FSections) Strings.Add(S.Name);
}

void TMemIniFile::ReadSectionValues(const QString &Section, TStringList &Strings) const
{
    Strings.Clear();
    int S = FindSection(Section);
    if (S < 0) return;
    for (const Line &L : FSections[S].Lines)
        if (L.IsKey) Strings.Add(L.Name + "=" + L.Value);
}

void TMemIniFile::EraseSection(const QString &Section)
{
    int S = FindSection(Section);
    if (S >= 0) { FSections.removeAt(S); FSectionIndexValid = false; FModified = true; }
}

void TMemIniFile::DeleteKey(const QString &Section, const QString &Ident)
{
    int S = FindSection(Section);
    if (S < 0) return;
    int K = FindKey(FSections[S], Ident);
    if (K >= 0) { FSections[S].Lines.removeAt(K); FSections[S].KeyIndexValid = false; FModified = true; }
}

void TMemIniFile::Clear()
{
    FSections.clear();
    FSectionIndexValid = false;
    FPreamble.clear();
    FModified = true;
}

void TMemIniFile::UpdateFile()
{
    if (FFileName.isEmpty()) return;
    TStringList L;
    GetStrings(L);
    WriteTextFile(FFileName, L.Text(), FUtf8, false);
    FModified = false;
}

TIniFile::~TIniFile() = default;

void TIniFile::WriteString(const QString &Section, const QString &Ident, const QString &Value)
{
    TMemIniFile::WriteString(Section, Ident, Value);
    UpdateFile();
}

void TIniFile::EraseSection(const QString &Section)
{
    TMemIniFile::EraseSection(Section);
    UpdateFile();
}

void TIniFile::DeleteKey(const QString &Section, const QString &Ident)
{
    TMemIniFile::DeleteKey(Section, Ident);
    UpdateFile();
}
