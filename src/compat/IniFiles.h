// Delphi IniFiles compatibility: TMemIniFile and TIniFile.
//
// Section and key names are case insensitive, the order of sections and keys
// is preserved and comments/empty lines inside sections are kept, so files
// written by the original program round-trip unchanged.
#pragma once

#include "Classes.h"

class TMemIniFile
{
public:
    explicit TMemIniFile(const QString &FileName);
    virtual ~TMemIniFile() = default;

    QString FileName() const { return FFileName; }
    void Rename(const QString &FileName, bool Reload);

    QString ReadString(const QString &Section, const QString &Ident, const QString &Default) const;
    virtual void WriteString(const QString &Section, const QString &Ident, const QString &Value);
    int ReadInteger(const QString &Section, const QString &Ident, int Default) const;
    void WriteInteger(const QString &Section, const QString &Ident, int Value);
    bool ReadBool(const QString &Section, const QString &Ident, bool Default) const;
    void WriteBool(const QString &Section, const QString &Ident, bool Value);
    double ReadFloat(const QString &Section, const QString &Ident, double Default) const;
    void WriteFloat(const QString &Section, const QString &Ident, double Value);
    TDateTime ReadDateTime(const QString &Section, const QString &Ident, TDateTime Default) const;
    void WriteDateTime(const QString &Section, const QString &Ident, TDateTime Value);

    bool SectionExists(const QString &Section) const;
    bool ValueExists(const QString &Section, const QString &Ident) const;
    void ReadSection(const QString &Section, TStringList &Strings) const;        // keys
    void ReadSections(TStringList &Strings) const;
    void ReadSectionValues(const QString &Section, TStringList &Strings) const;  // key=value
    virtual void EraseSection(const QString &Section);
    virtual void DeleteKey(const QString &Section, const QString &Ident);

    void Clear();
    void GetStrings(TStringList &List) const;
    void SetStrings(const TStringList &List);
    virtual void UpdateFile();

    // Encoding used when writing (defaults to the encoding the file was read with)
    bool Utf8() const { return FUtf8; }
    void SetUtf8(bool B) { FUtf8 = B; }

protected:
    struct Line { QString Name; QString Value; bool IsKey = true; };   // non key lines keep raw text in Value
    struct Section { QString Name; QList<Line> Lines; };
    QString FFileName;
    QList<Section> FSections;
    QList<Line> FPreamble;   // lines before the first section
    bool FUtf8 = false;
    bool FModified = false;

    void Load();
    int FindSection(const QString &Name) const;
    int FindKey(const Section &S, const QString &Name) const;
};

// TIniFile writes every change to disk immediately (like the Win32 profile API).
class TIniFile : public TMemIniFile
{
public:
    explicit TIniFile(const QString &FileName) : TMemIniFile(FileName) {}
    ~TIniFile() override;
    void WriteString(const QString &Section, const QString &Ident, const QString &Value) override;
    void EraseSection(const QString &Section) override;
    void DeleteKey(const QString &Section, const QString &Ident) override;
};
