// C++ port of PackageBuilderUnit.pas (implementation section).
#include "PackageBuilderUnit.h"

#include "CommonTools.h"
#include "HashCalc.h"
#include "PackageDBToolsUnit.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "PrgConsts.h"
#include "ZipInfoFormUnit.h"
#include "ZipPackageUnit.h"

// IXMLNode.AddChild on the document element
static QDomElement AddChild(QDomDocument* Doc, const QString& Name)
{
    QDomElement E = Doc->createElement(Name);
    Doc->documentElement().appendChild(E);
    return E;
}

// IXMLNode.NodeValue := Value
static void SetNodeValue(QDomElement N, const QString& Value)
{
    while (!N.firstChild().isNull()) N.removeChild(N.firstChild());
    N.appendChild(N.ownerDocument().createTextNode(Value));
}

static bool FileOK(const QString& DataFileName)
{
    bool result = FileExists(DataFileName);
    if (!result) result = FileExists(DataFileName + ".part1");
    if (!result) MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {DataFileName}), mtError, mbOK, 0);
    return result;
}

static qint64 MultiFileSize(const QString& FileName)
{
    TSearchRec Rec;
    qint64 result = 0;
    int I = FindFirst(FileName + ".part*", faAnyFile, Rec);
    while (I == 0) {
        result = result + Rec.Size;
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

bool AddPackageDataZip(QDomDocument* Doc, TGame* Game, const QString& DataFileName)
{
    QString S;
    QDomElement N;
    bool LargeFile;

    bool result = FileOK(DataFileName); if (!result) return result;
    LargeFile = (!FileExists(DataFileName)) && FileExists(DataFileName + ".part1");

    N = AddChild(Doc, "Game");
    if (Game == nullptr) S = ""; else S = Game->CacheName;
    N.setAttribute("Name", S);
    if (Game == nullptr) S = ""; else S = Game->CacheGenre;
    N.setAttribute("Genre", S);
    if (Game == nullptr) S = ""; else S = Game->CacheDeveloper;
    N.setAttribute("Developer", S);
    if (Game == nullptr) S = ""; else S = Game->CachePublisher;
    N.setAttribute("Publisher", S);
    if (Game == nullptr) S = ""; else S = Game->CacheYear;
    N.setAttribute("Year", S);
    if (Game == nullptr) S = "English"; else S = Game->CacheLanguage;
    N.setAttribute("Language", S);
    if (Game == nullptr) S = ""; else S = Game->License();
    N.setAttribute("License", S);
    // Port: the original discarded the checksum of non split files here
    // ("else GetMD5Sum(DataFileName,False)") and wrote the license instead.
    if (LargeFile) S = GetMD5Sum(DataFileName + ".part1", false); else S = GetMD5Sum(DataFileName, false);
    N.setAttribute("PackageChecksum", S);
    if (Game != nullptr) {
        CreateGameCheckSum(Game, false);
        N.setAttribute("GameExeChecksum", Game->GameExeMD5());
    } else {
        N.setAttribute("GameExeChecksum", "");
    }
    if (LargeFile) S = IntToStr(MultiFileSize(DataFileName)); else S = IntToStr(GetFileSize(DataFileName));
    N.setAttribute("Size", S);
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

bool AddPackageDataAutoSetup(QDomDocument* Doc, const QString& DataFileName, const QString& MaxVersion)
{
    bool result = FileOK(DataFileName); if (!result) return result;

    TIniFile Ini(DataFileName);
    QDomElement N = AddChild(Doc, "AutoSetup");
    N.setAttribute("Name", Ini.ReadString("ExtraInfo", "Name", ""));
    N.setAttribute("Genre", Ini.ReadString("ExtraInfo", "Genre", ""));
    N.setAttribute("Developer", Ini.ReadString("ExtraInfo", "Developer", ""));
    N.setAttribute("Publisher", Ini.ReadString("ExtraInfo", "Publisher", ""));
    N.setAttribute("Year", Ini.ReadString("ExtraInfo", "Year", ""));
    N.setAttribute("Language", Ini.ReadString("ExtraInfo", "Language", ""));
    N.setAttribute("PackageChecksum", GetMD5Sum(DataFileName, false));
    N.setAttribute("GameExeChecksum", Ini.ReadString("Extra", "ExeMD5", ""));
    N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
    if (Trim(MaxVersion) != "") N.setAttribute("MaxVersion", MaxVersion);
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

bool AddPackageDataIcon(QDomDocument* Doc, const QString& DataFileName)
{
    bool result = FileOK(DataFileName); if (!result) return result;

    QDomElement N = AddChild(Doc, "Icon");
    N.setAttribute("Name", ChangeFileExt(ExtractFileName(DataFileName), ""));
    N.setAttribute("FileChecksum", GetMD5Sum(DataFileName, false));
    N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

bool AddPackageDataIconSet(QDomDocument* Doc, const QString& IconSetIniFile, const QString& DataFileName)
{
    int I;
    QString S;

    bool result = FileOK(DataFileName); if (!result) return result;

    TIniFile Ini(IconSetIniFile);
    QDomElement N = AddChild(Doc, "IconSet");
    N.setAttribute("Name", Ini.ReadString("Information", "Name", ChangeFileExt(ExtractFileName(DataFileName), "")));
    N.setAttribute("FileChecksum", GetMD5Sum(DataFileName, false));
    I = VersionToInt(GetNormalFileVersionAsString()); S = IntToStr(I / 10000) + "." + IntToStr((I / 100) % 100) + ".";
    N.setAttribute("MinVersion", S + "0");
    N.setAttribute("MaxVersion", S + "99");
    N.setAttribute("Author", Ini.ReadString("Information", "Author", ""));
    N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

bool AddPackageDataLanguage(QDomDocument* Doc, const QString& DataFileName)
{
    int I;
    QString S;

    bool result = FileOK(DataFileName); if (!result) return result;

    TIniFile Ini(DataFileName);
    QDomElement N = AddChild(Doc, "Language");
    N.setAttribute("Name", ChangeFileExt(ExtractFileName(DataFileName), ""));
    I = VersionToInt(Ini.ReadString("LanguageFileInfo", "MaxVersion", "")); S = IntToStr(I / 10000) + "." + IntToStr((I / 100) % 100) + ".";
    N.setAttribute("MinVersion", S + "0");
    N.setAttribute("MaxVersion", S + "99");
    N.setAttribute("Author", Ini.ReadString("Author", "Name", ""));
    N.setAttribute("PackageChecksum", GetMD5Sum(DataFileName, false));
    N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

bool AddPackageDataExe(QDomDocument* Doc, const QString& DataFileName)
{
    bool result = FileOK(DataFileName); if (!result) return result;

    QDomElement N = AddChild(Doc, "ExePackage");
    N.setAttribute("Name", ChangeFileExt(ExtractFileName(DataFileName), ""));
    N.setAttribute("Description", "");
    N.setAttribute("PackageChecksum", GetMD5Sum(DataFileName, false));
    N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
    SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
    return result;
}

static TGame* GetAutoSetupTemplateForFolder(TGameDB* AutoSetupDB, const QString& Dir)
{
    TGame* result = nullptr;
    QString S = OnlySubFolderCheck(Dir);

    TStringList* St = GetTemplateFromFolderExt(false, S, AutoSetupDB, nullptr);
    if (St->Count() == 1) {
        int I = St->ObjectAsInt(0);
        if (I >= 0) result = AutoSetupDB->Game(I);
    }
    delete St;
    return result;
}

// Delphi SysUtils.GetFileVersion (major shl 16 + minor)
static int GetFileVersionOfPrg()
{
    // TODO(port): no version resource - derive it from the program version string
    const QStringList L = GetNormalFileVersionAsString().split('.');
    const int A = L.value(0).toInt(), B = L.value(1).toInt();
    return A * 65536 + B;
}

[[maybe_unused]] static QString GetNormalLastFileVersionAsString()
{
    int I, J;
    int A, B, C;
    I = GetFileVersionOfPrg();
    J = static_cast<int>(GetFileVersionEx(ExpandFileName(QCoreApplication::applicationFilePath())));
    A = I / 65536;
    B = I % 65536;
    C = J / 65536;
    if (C > 0) C--; else {
        C = 99;
        if (B > 0) B--; else {
            B = 9;
            A--;
        }
    }
    return Format("%d.%d.%d", {A, B, C});
}

bool AddPackageDataPlainZip(QDomDocument* Doc, const QString& DataFileName, TGameDB* AutoSetupDB, QWidget* Owner, bool AddAutoSetups, const QString& AutoSetupsMaxVersion)
{
    QString Temp;
    TGame* Template;
    QString S;
    QDomElement N;

    bool result = FileOK(DataFileName); if (!result) return result;

    Temp = IncludeTrailingPathDelimiter(TempDir() + TempSubFolder);
    if (!ForceDirectories(Temp)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Temp}), mtError, mbOK, 0);
        return false;
    }
    if (ExtractZipFile(Owner, DataFileName, Temp)) {
        Template = GetAutoSetupTemplateForFolder(AutoSetupDB, Temp);
        if (AddAutoSetups && (Template != nullptr)) CopyFile(Template->SetupFile(), IncludeTrailingPathDelimiter(ExtractFilePath(DataFileName)) + ExtractFileName(Template->SetupFile()), false);
        N = AddChild(Doc, "Game");
        if (Template == nullptr) {
            N.setAttribute("Name", "Warning: No matching auto setup template for " + ExtractFileName(DataFileName));
        } else {
            N.setAttribute("Name", Template->CacheName);
            N.setAttribute("Genre", Template->CacheGenre);
            N.setAttribute("Developer", Template->CacheDeveloper);
            N.setAttribute("Publisher", Template->CachePublisher);
            N.setAttribute("Year", Template->CacheYear);
            N.setAttribute("Language", Template->CacheLanguage);
            N.setAttribute("License", Template->License());
            N.setAttribute("PackageChecksum", GetMD5Sum(DataFileName, false));
            N.setAttribute("GameExeChecksum", Template->GameExeMD5());
            N.setAttribute("Size", IntToStr(GetFileSize(DataFileName)));
            if (AddAutoSetups) {
                if (Template == nullptr) S = "not found!"; else S = URLFileNameFromFileName(ExtractFileName(Template->SetupFile()));
                N.setAttribute("AutoSetupURL", S);
                if (Trim(AutoSetupsMaxVersion) != "") N.setAttribute("AutoSetupURLMaxVersion", AutoSetupsMaxVersion);
            }
            SetNodeValue(N, URLFileNameFromFileName(ExtractFileName(DataFileName)));
        }
    }
    ExtDeleteFolder(Temp, ftTemp);
    return result;
}

QDomDocument* AddPackageDataHeader(QWidget* ParentForm, const QString& Description)
{
    Q_UNUSED(ParentForm);
    QDomDocument* result = new QDomDocument;
    QDomElement Root = result->createElement("DFRPackagesFile");
    result->appendChild(Root);
    Root.setAttribute("Name", Description);
    Root.setAttribute("LastUpdateDate", EncodeUpdateDate(Date()));
    return result;
}

TStringList* AddPackageDataFooter(QDomDocument* Doc)
{
    return SaveXMLDoc(*Doc, {"<!DOCTYPE DFRPackagesFile SYSTEM \"" + DFRHomepage + "Packages/DFRPackagesFile.dtd\">"}, false);
}
