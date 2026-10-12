// C++ port of PackageDBUnit.pas (implementation section).
#include "PackageDBUnit.h"

#include "PackageDBToolsUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"

// Iterates over the element children of N (IXMLNode.ChildNodes)
static QList<QDomElement> ChildElements(const QDomElement& N)
{
    QList<QDomElement> result;
    for (QDomNode C = N.firstChild(); !C.isNull(); C = C.nextSibling()) if (C.isElement()) result.append(C.toElement());
    return result;
}

// Adds a child element (IXMLNode.AddChild)
static QDomElement AddChild(QDomDocument& Doc, QDomElement Parent, const QString& Name)
{
    QDomElement E = Doc.createElement(Name);
    Parent.appendChild(E);
    return E;
}

// IXMLNode.NodeValue := Value
static void SetNodeValue(QDomElement N, const QString& Value)
{
    while (!N.firstChild().isNull()) N.removeChild(N.firstChild());
    N.appendChild(N.ownerDocument().createTextNode(Value));
}

// { TDownloadData }

TDownloadData::TDownloadData(TPackageList* APackageList)
{
    FName = "";
    FURL = "";
    FPackageChecksum = "";
    FSize = 0;
    FPackageList = APackageList;
    FPackageFileURL = FPackageList->URL();

    FChecksumXMLName = "PackageChecksum";
}

bool TDownloadData::CheckAttributes(const QDomElement& N, const QList<QString>& Attr)
{
    for (const QString& A : Attr) if (!N.hasAttribute(A)) return false;
    return true;
}

bool TDownloadData::LoadFromXMLNode(const QDomElement& N)
{
    if (!CheckAttributes(N, {"Name", FChecksumXMLName, "Size"})) return false;
    FName = DecodeHTMLSymbols(N.attribute("Name"));
    FPackageChecksum = N.attribute(FChecksumXMLName);
    bool OK;
    FSize = N.attribute("Size").trimmed().toLongLong(&OK); if (!OK) FSize = 0;
    FURL = N.text(); if (FURL == "") return false;
    return true;
}

void TDownloadData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    N.setAttribute("Name", EncodeHTMLSymbols(FName));
    N.setAttribute(FChecksumXMLName, FPackageChecksum);
    N.setAttribute("Size", IntToStr(FSize));
    SetNodeValue(N, FURL);
}

// { TDownloadIconData }

TDownloadIconData::TDownloadIconData(TPackageList* APackageList) : TDownloadData(APackageList)
{
    FChecksumXMLName = "FileChecksum";
}

// { TDownloadAutoSetupData }

TDownloadAutoSetupData::TDownloadAutoSetupData(TPackageList* APackageList) : TDownloadData(APackageList)
{
    FGenre = "";
    FDeveloper = "";
    FPublisher = "";
    FYear = "";
    FLanguage = "";
    FMaxVersion = "";
}

bool TDownloadAutoSetupData::LoadFromXMLNode(const QDomElement& N)
{
    if (!TDownloadData::LoadFromXMLNode(N)) return false;
    if (!CheckAttributes(N, {"Genre", "Developer", "Publisher", "Year", "Language"})) return false;
    FGenre = DecodeHTMLSymbols(N.attribute("Genre"));
    FDeveloper = DecodeHTMLSymbols(N.attribute("Developer"));
    FPublisher = DecodeHTMLSymbols(N.attribute("Publisher"));
    FYear = DecodeHTMLSymbols(N.attribute("Year"));
    FLanguage = DecodeHTMLSymbols(N.attribute("Language"));
    if (N.hasAttribute("GameExeChecksum")) FGameExeChecksum = N.attribute("GameExeChecksum"); else FGameExeChecksum = "";
    if (N.hasAttribute("MaxVersion")) FMaxVersion = N.attribute("MaxVersion"); else FMaxVersion = "";
    return true;
}

void TDownloadAutoSetupData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    TDownloadData::SaveToXMLNode(N);
    N.setAttribute("Genre", EncodeHTMLSymbols(FGenre));
    N.setAttribute("Developer", EncodeHTMLSymbols(FDeveloper));
    N.setAttribute("Publisher", EncodeHTMLSymbols(FPublisher));
    N.setAttribute("Year", EncodeHTMLSymbols(FYear));
    N.setAttribute("Language", EncodeHTMLSymbols(FLanguage));
    if (FGameExeChecksum != "") N.setAttribute("GameExeChecksum", FGameExeChecksum);
    if (FMaxVersion != "") N.setAttribute("MaxVersion", FMaxVersion);
}

// { TDownloadZipData }

TDownloadZipData::TDownloadZipData(TPackageList* APackageList) : TDownloadAutoSetupData(APackageList)
{
    FLicense = "";
    FMetaLink = false;
    FAutoSetupForZipURL = ""; FAutoSetupForZipURLMaxVersion = "";
}

bool TDownloadZipData::LoadFromXMLNode(const QDomElement& N)
{
    QString S;
    if (!TDownloadAutoSetupData::LoadFromXMLNode(N)) return false;
    if (!CheckAttributes(N, {"License"})) return false;
    FLicense = DecodeHTMLSymbols(N.attribute("License"));

    if (N.hasAttribute("MetaLink")) {
        S = ExtUpperCase(N.attribute("MetaLink"));
        FMetaLink = (S == "YES") || (S == "ON") || (S == "TRUE");
    }

    if (N.hasAttribute("AutoSetupURL")) {
        FAutoSetupForZipURL = N.attribute("AutoSetupURL");
        if (N.hasAttribute("AutoSetupURLMaxVersion")) FAutoSetupForZipURLMaxVersion = N.attribute("AutoSetupURLMaxVersion");
    }

    return true;
}

void TDownloadZipData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    TDownloadAutoSetupData::SaveToXMLNode(N);

    N.setAttribute("License", EncodeHTMLSymbols(FLicense));
    if (FMetaLink) N.setAttribute("MetaLink", "Yes");
    if (FAutoSetupForZipURL != "") {
        N.setAttribute("AutoSetupURL", FAutoSetupForZipURL);
        if (FAutoSetupForZipURLMaxVersion != "") N.setAttribute("AutoSetupURLMaxVersion", FAutoSetupForZipURLMaxVersion);
    }
}

// { TDownloadLanguage }

TDownloadLanguageData::TDownloadLanguageData(TPackageList* APackageList) : TDownloadData(APackageList)
{
    FMinVersion = ""; FMaxVersion = ""; FAuthor = ""; FDescription = "";
}

bool TDownloadLanguageData::LoadFromXMLNode(const QDomElement& N)
{
    if (!TDownloadData::LoadFromXMLNode(N)) return false;
    if (!CheckAttributes(N, {"MinVersion", "MaxVersion", "Author"})) return false;
    if (N.hasAttribute("Description")) FDescription = N.attribute("Description");
    FMinVersion = N.attribute("MinVersion");
    FMaxVersion = N.attribute("MaxVersion");
    FAuthor = DecodeHTMLSymbols(N.attribute("Author"));
    return true;
}

void TDownloadLanguageData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    TDownloadData::SaveToXMLNode(N);
    if (FDescription != "") N.setAttribute("Description", FDescription);
    N.setAttribute("MinVersion", FMinVersion);
    N.setAttribute("MaxVersion", FMaxVersion);
    N.setAttribute("Author", EncodeHTMLSymbols(FAuthor));
}

// { TDownloadExeData }

TDownloadExeData::TDownloadExeData(TPackageList* APackageList) : TDownloadData(APackageList)
{
    FDescription = "";
}

bool TDownloadExeData::LoadFromXMLNode(const QDomElement& N)
{
    if (!TDownloadData::LoadFromXMLNode(N)) return false;
    if (!CheckAttributes(N, {"Description"})) return false;
    FDescription = DecodeHTMLSymbols(N.attribute("Description"));
    return true;
}

void TDownloadExeData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    TDownloadData::SaveToXMLNode(N);
    N.setAttribute("Description", EncodeHTMLSymbols(FDescription));
}

// { TDownloadIconSetData }

TDownloadIconSetData::TDownloadIconSetData(TPackageList* APackageList) : TDownloadData(APackageList)
{
    FChecksumXMLName = "FileChecksum";
    FMinVersion = ""; FMaxVersion = ""; FAuthor = "";
}

bool TDownloadIconSetData::LoadFromXMLNode(const QDomElement& N)
{
    if (!TDownloadData::LoadFromXMLNode(N)) return false;
    if (!CheckAttributes(N, {"MinVersion", "MaxVersion", "Author"})) return false;
    FMinVersion = N.attribute("MinVersion");
    FMaxVersion = N.attribute("MaxVersion");
    FAuthor = DecodeHTMLSymbols(N.attribute("Author"));
    return true;
}

void TDownloadIconSetData::SaveToXMLNode(const QDomElement& N_)
{
    QDomElement N = N_;
    TDownloadData::SaveToXMLNode(N);
    N.setAttribute("MinVersion", FMinVersion);
    N.setAttribute("MaxVersion", FMaxVersion);
    N.setAttribute("Author", EncodeHTMLSymbols(FAuthor));
}

// { TPackageList }

TPackageList::TPackageList(const QString& AURL)
{
    FUpdateDate = 0;
    FFileName = "";
    FURL = AURL;
    FGame = new TList;
    FAutoSetup = new TList;
    FLanguage = new TList;
    FExePackage = new TList;
    FIcon = new TList;
    FIconSet = new TList;
    FProviderName = "";
    FProviderText = "";
    FProviderURL = "";
    FProviderActions.clear();
    FReferer = "automatic";
}

TPackageList::~TPackageList()
{
    Clear();
    delete FGame;
    delete FAutoSetup;
    delete FLanguage;
    delete FExePackage;
    delete FIcon;
    delete FIconSet;
}

void TPackageList::Clear()
{
    for (int I = 0; I < FGame->count(); I++) delete static_cast<TDownloadZipData*>((*FGame)[I]);
    FGame->clear();
    for (int I = 0; I < FAutoSetup->count(); I++) delete static_cast<TDownloadAutoSetupData*>((*FAutoSetup)[I]);
    FAutoSetup->clear();
    for (int I = 0; I < FLanguage->count(); I++) delete static_cast<TDownloadLanguageData*>((*FLanguage)[I]);
    FLanguage->clear();
    for (int I = 0; I < FExePackage->count(); I++) delete static_cast<TDownloadExeData*>((*FExePackage)[I]);
    FExePackage->clear();
    for (int I = 0; I < FIcon->count(); I++) delete static_cast<TDownloadIconData*>((*FIcon)[I]);
    FIcon->clear();
    for (int I = 0; I < FIconSet->count(); I++) delete static_cast<TDownloadIconSetData*>((*FIconSet)[I]);
    FIconSet->clear();
    FProviderName = "";
    FProviderText = "";
    FProviderURL = "";
    FProviderActions.clear();
    HasProviderInfo = false;
}

int TPackageList::GetCount(int Index)
{
    switch (Index) {
        case 0: return FGame->count();
        case 1: return FAutoSetup->count();
        case 2: return FLanguage->count();
        case 3: return FExePackage->count();
        case 4: return FIcon->count();
        case 5: return FIconSet->count();
        default: return 0;
    }
}

TDownloadZipData* TPackageList::GetGame(int I)
{
    if ((I < 0) || (I >= FGame->count())) return nullptr;
    return static_cast<TDownloadZipData*>((*FGame)[I]);
}

TDownloadAutoSetupData* TPackageList::GetAutoSetup(int I)
{
    if ((I < 0) || (I >= FAutoSetup->count())) return nullptr;
    return static_cast<TDownloadAutoSetupData*>((*FAutoSetup)[I]);
}

TDownloadLanguageData* TPackageList::GetLanguage(int I)
{
    if ((I < 0) || (I >= FLanguage->count())) return nullptr;
    return static_cast<TDownloadLanguageData*>((*FLanguage)[I]);
}

TDownloadExeData* TPackageList::GetExePackage(int I)
{
    if ((I < 0) || (I >= FExePackage->count())) return nullptr;
    return static_cast<TDownloadExeData*>((*FExePackage)[I]);
}

TDownloadIconData* TPackageList::GetIcon(int I)
{
    if ((I < 0) || (I >= FIcon->count())) return nullptr;
    return static_cast<TDownloadIconData*>((*FIcon)[I]);
}

TDownloadIconSetData* TPackageList::GetIconSet(int I)
{
    if ((I < 0) || (I >= FIconSet->count())) return nullptr;
    return static_cast<TDownloadIconSetData*>((*FIconSet)[I]);
}

bool TPackageList::AddGame(const QDomElement& N)
{
    TDownloadZipData* DownloadZipData = new TDownloadZipData(this);
    bool result = DownloadZipData->LoadFromXMLNode(N);
    if (result) FGame->append(DownloadZipData); else delete DownloadZipData;
    return result;
}

bool TPackageList::AddAutoSetup(const QDomElement& N)
{
    TDownloadAutoSetupData* DownloadAutoSetupData = new TDownloadAutoSetupData(this);
    bool result = DownloadAutoSetupData->LoadFromXMLNode(N);
    if (result) FAutoSetup->append(DownloadAutoSetupData); else delete DownloadAutoSetupData;
    return result;
}

bool TPackageList::AddLanguage(const QDomElement& N)
{
    TDownloadLanguageData* DownloadLanguageData = new TDownloadLanguageData(this);
    bool result = DownloadLanguageData->LoadFromXMLNode(N);
    if (result) FLanguage->append(DownloadLanguageData); else delete DownloadLanguageData;
    return result;
}

bool TPackageList::AddExePackage(const QDomElement& N)
{
    TDownloadExeData* DownloadExeData = new TDownloadExeData(this);
    bool result = DownloadExeData->LoadFromXMLNode(N);
    if (result) FExePackage->append(DownloadExeData); else delete DownloadExeData;
    return result;
}

bool TPackageList::AddIcon(const QDomElement& N)
{
    TDownloadIconData* DownloadIconData = new TDownloadIconData(this);
    bool result = DownloadIconData->LoadFromXMLNode(N);
    if (result) FIcon->append(DownloadIconData); else delete DownloadIconData;
    return result;
}

bool TPackageList::AddIconSet(const QDomElement& N)
{
    TDownloadIconSetData* DownloadIconSetData = new TDownloadIconSetData(this);
    bool result = DownloadIconSetData->LoadFromXMLNode(N);
    if (result) FIconSet->append(DownloadIconSetData); else delete DownloadIconSetData;
    return result;
}

void TPackageList::SetProviderInfo(const QDomElement& N)
{
    HasProviderInfo = true;
    if (N.hasAttribute("WriteToProfile") && (Trim(ExtUpperCase(N.attribute("WriteToProfile"))) == "YES")) FProviderActions.insert(paWriteInfoToProfile);
    if (N.hasAttribute("Dialog") && (Trim(ExtUpperCase(N.attribute("Dialog"))) == "YES")) FProviderActions.insert(paInfoDialog);
    if (N.hasAttribute("OpenURL") && (Trim(ExtUpperCase(N.attribute("OpenURL"))) == "YES")) FProviderActions.insert(paOpenURL);
    if (N.hasAttribute("Name")) FProviderName = DecodeHTMLSymbols(N.attribute("Name"));
    if (N.hasAttribute("Text")) FProviderText = DecodeHTMLSymbols(N.attribute("Text"));
    if (N.hasAttribute("URL")) FProviderURL = N.attribute("URL");
    if (N.hasAttribute("Referer")) FReferer = N.attribute("Referer");
}

bool TPackageList::LoadFromFile(const QString& AFileName, const QString& AOrigFileName)
{
    QDomDocument* XMLDoc;

    FFileName = AFileName;
    Clear();

    XMLDoc = LoadXMLDoc(AFileName, AOrigFileName); if (XMLDoc == nullptr) return false;
    QDomElement Root = XMLDoc->documentElement();
    if (Root.nodeName() != "DFRPackagesFile") { delete XMLDoc; return false; }
    if (!Root.hasAttribute("Name")) { delete XMLDoc; return false; }
    FName = DecodeHTMLSymbols(Root.attribute("Name"));
    if (!Root.hasAttribute("LastUpdateDate")) FUpdateDate = 0; else {
        FUpdateDate = DecodeUpdateDate(Root.attribute("LastUpdateDate"));
    }
    for (const QDomElement& N : ChildElements(Root)) {
        if (N.nodeName() == "Game") AddGame(N);
        if (N.nodeName() == "AutoSetup") AddAutoSetup(N);
        if (N.nodeName() == "Language") AddLanguage(N);
        if (N.nodeName() == "ExePackage") AddExePackage(N);
        if (N.nodeName() == "Icon") AddIcon(N);
        if (N.nodeName() == "IconSet") AddIconSet(N);
        if (N.nodeName() == "Provider") SetProviderInfo(N);
    }
    delete XMLDoc;

    return true;
}

bool TPackageList::SaveToFile(QWidget* ParentForm)
{
    Q_UNUSED(ParentForm);
    if (FFileName == "") return false;

    QDomDocument Doc;
    QDomElement Root = Doc.createElement("DFRPackagesFile");
    Doc.appendChild(Root);
    Root.setAttribute("Name", FName);
    Root.setAttribute("LastUpdateDate", EncodeUpdateDate(FUpdateDate));

    for (int I = 0; I < FExePackage->count(); I++) static_cast<TDownloadExeData*>((*FExePackage)[I])->SaveToXMLNode(AddChild(Doc, Root, "ExePackage"));
    for (int I = 0; I < FGame->count(); I++) static_cast<TDownloadZipData*>((*FGame)[I])->SaveToXMLNode(AddChild(Doc, Root, "Game"));
    for (int I = 0; I < FAutoSetup->count(); I++) static_cast<TDownloadAutoSetupData*>((*FAutoSetup)[I])->SaveToXMLNode(AddChild(Doc, Root, "AutoSetup"));
    for (int I = 0; I < FIcon->count(); I++) static_cast<TDownloadIconData*>((*FIcon)[I])->SaveToXMLNode(AddChild(Doc, Root, "Icon"));
    for (int I = 0; I < FIconSet->count(); I++) static_cast<TDownloadIconSetData*>((*FIconSet)[I])->SaveToXMLNode(AddChild(Doc, Root, "IconSet"));
    for (int I = 0; I < FLanguage->count(); I++) static_cast<TDownloadLanguageData*>((*FLanguage)[I])->SaveToXMLNode(AddChild(Doc, Root, "Language"));

    if (HasProviderInfo) {
        QDomElement Node = AddChild(Doc, Root, "Provider");
        if (FProviderActions.contains(paWriteInfoToProfile)) Node.setAttribute("WriteToProfile", "Yes");
        if (FProviderActions.contains(paInfoDialog)) Node.setAttribute("Dialog", "Yes");
        if (FProviderActions.contains(paOpenURL)) Node.setAttribute("OpenURL", "Yes");
        if (FProviderName != "") Node.setAttribute("Name", EncodeHTMLSymbols(FProviderName));
        if (FProviderText != "") Node.setAttribute("Text", EncodeHTMLSymbols(FProviderText));
        if (FProviderURL != "") Node.setAttribute("URL", FProviderURL);
        if (FReferer != "") Node.setAttribute("Referer", FReferer);
    }

    SaveXMLDoc(Doc, {"<!DOCTYPE DFRPackagesFile SYSTEM \"" + DFRHomepage + "Packages/DFRPackagesFile.dtd\">"}, FFileName, false);
    return true;
}

bool TPackageList::FindAndRemoveGame(TDownloadZipData* AGame, QWidget* ParentForm)
{
    int Index = FGame->indexOf(AGame);
    if (Index < 0) return false;

    delete static_cast<TDownloadZipData*>((*FGame)[Index]);
    FGame->removeAt(Index);

    SaveToFile(ParentForm);
    return true;
}

bool TPackageList::FindAndRemoveAutoSetup(TDownloadAutoSetupData* AAutoSetup, QWidget* ParentForm)
{
    int Index = FAutoSetup->indexOf(AAutoSetup);
    if (Index < 0) return false;

    delete static_cast<TDownloadAutoSetupData*>((*FAutoSetup)[Index]);
    FAutoSetup->removeAt(Index);

    SaveToFile(ParentForm);
    return true;
}

bool TPackageList::FindAndRemoveIcon(TDownloadIconData* AIcon, QWidget* ParentForm)
{
    int Index = FIcon->indexOf(AIcon);
    if (Index < 0) return false;

    delete static_cast<TDownloadIconData*>((*FIcon)[Index]);
    FIcon->removeAt(Index);

    SaveToFile(ParentForm);
    return true;
}

bool TPackageList::FindAndRemoveIconSet(TDownloadIconSetData* AIconSet, QWidget* ParentForm)
{
    int Index = FIconSet->indexOf(AIconSet);
    if (Index < 0) return false;

    delete static_cast<TDownloadIconSetData*>((*FIconSet)[Index]);
    FIconSet->removeAt(Index);

    SaveToFile(ParentForm);
    return true;
}

bool TPackageList::FindAndRemoveLanguage(TDownloadLanguageData* ALanguage, QWidget* ParentForm)
{
    int Index = FLanguage->indexOf(ALanguage);
    if (Index < 0) return false;

    // Port: the original freed FIconSet[Index] here (copy & paste bug)
    delete static_cast<TDownloadLanguageData*>((*FLanguage)[Index]);
    FLanguage->removeAt(Index);

    SaveToFile(ParentForm);
    return true;
}

// { TMainPackageFile }

TPackageListFile::TPackageListFile(bool AIsMasterList)
{
    FIsMasterList = AIsMasterList;
    FUpdateDate = 0;
    FURL = new TStringList;
    FActive = new TList;
    FFileName = "";
}

TPackageListFile::~TPackageListFile()
{
    delete FURL;
    delete FActive;
}

void TPackageListFile::Clear()
{
    FURL->Clear();
    FActive->clear();
}

int TPackageListFile::GetCount()
{
    return FURL->Count();
}

QString TPackageListFile::GetURL(int I)
{
    if ((I < 0) || (I >= FURL->Count())) return QString();
    return (*FURL)[I];
}

void TPackageListFile::SetURL(int I, const QString& Value)
{
    if ((I < 0) || (I >= FURL->Count())) return;
    (*FURL)[I] = Value;
}

bool TPackageListFile::GetActive(int I)
{
    if ((I < 0) || (I >= FURL->Count())) return false;
    return (*FActive)[I] != nullptr;
}

void TPackageListFile::SetActive(int I, bool Value)
{
    if ((I < 0) || (I >= FURL->Count())) return;
    (*FActive)[I] = Value ? reinterpret_cast<void*>(1) : nullptr;
}

void TPackageListFile::Add(const QString& AURL, bool AActive)
{
    FURL->Add(AURL);
    FActive->append(AActive ? reinterpret_cast<void*>(1) : nullptr);
}

void TPackageListFile::Delete(int Nr)
{
    if ((Nr < 0) || (Nr >= FURL->Count())) return;
    FURL->Delete(Nr);
    FActive->removeAt(Nr);
}

bool TPackageListFile::LoadFromFile(const QString& AFileName, const QString& AOrigFileName)
{
    QDomDocument* XMLDoc;

    Clear();
    FFileName = AFileName;

    XMLDoc = LoadXMLDoc(AFileName, AOrigFileName);
    if ((XMLDoc == nullptr) || (XMLDoc->documentElement().nodeName() != "DFRPackagesList")) { delete XMLDoc; return false; }
    QDomElement Root = XMLDoc->documentElement();
    if (!Root.hasAttribute("LastUpdateDate")) FUpdateDate = 0; else {
        FUpdateDate = DecodeUpdateDate(Root.attribute("LastUpdateDate"));
    }
    for (const QDomElement& N : ChildElements(Root)) if (N.nodeName() == "DFRPackagesFile") {
        if (N.hasAttribute("URL")) FURL->Add(N.attribute("URL"));
        if (N.hasAttribute("Active")) {
            if (Trim(ExtUpperCase(N.attribute("Active"))) == "YES") FActive->append(reinterpret_cast<void*>(1)); else FActive->append(nullptr);
        } else {
            FActive->append(reinterpret_cast<void*>(1));
        }
    }
    delete XMLDoc;

    if (FIsMasterList) SpecialLoadActiveState();

    return true;
}

void TPackageListFile::SaveToFile(QWidget* ParentForm)
{
    Q_UNUSED(ParentForm);
    QDomDocument Doc;
    QDomElement Root = Doc.createElement("DFRPackagesList");
    Doc.appendChild(Root);
    Root.setAttribute("LastUpdateDate", EncodeUpdateDate(FUpdateDate));
    for (int I = 0; I < FURL->Count(); I++) {
        QDomElement N = AddChild(Doc, Root, "DFRPackagesFile");
        N.setAttribute("URL", (*FURL)[I]);
        if ((*FActive)[I] != nullptr) N.setAttribute("Active", "Yes"); else N.setAttribute("Active", "No");
    }
    SaveXMLDoc(Doc, {"<!DOCTYPE DFRPackagesList SYSTEM \"" + DFRHomepage + "Packages/DFRPackagesList.dtd\">"}, FFileName, false);

    if (FIsMasterList) SpecialSaveActiveState();
}

bool TPackageListFile::UpdateFromServer(const QString& URL, const QString& TempFile, bool UpdateAnyway, bool UpdateErrorQuite)
{
    bool result = DownloadFile(URL, TempFile, UpdateErrorQuite);
    if (!result) return result;

    TPackageListFile* TempMainFile = new TPackageListFile(FIsMasterList);
    if (TempMainFile->LoadFromFile(TempFile)) {
        if (!((TempMainFile->UpdateDate() <= UpdateDate()) && (!UpdateAnyway))) {
            Clear();
            ExtDeleteFile(FileName(), ftTemp);
            RenameFile(TempFile, FileName());
            LoadFromFile(FileName());
        }
    }
    delete TempMainFile;
    ExtDeleteFile(TempFile, ftTemp);
    return result;
}

void TPackageListFile::SpecialLoadActiveState()
{
    QString ActiveStateFileName;
    TStringList St;
    int J;
    QString S;

    ActiveStateFileName = ChangeFileExt(FFileName, ".txt");

    if (!FileExists(ActiveStateFileName)) return;

    if (!St.LoadFromFile(ActiveStateFileName)) return;
    for (int I = 0; I < St.Count(); I++) {
        S = St[I];
        if ((S != "") && (S[0] == ';')) continue;
        J = FURL->IndexOf(S);
        if (J >= 0) (*FActive)[J] = nullptr;
    }
}

void TPackageListFile::SpecialSaveActiveState()
{
    QString ActiveStateFileName;
    TStringList St;

    ActiveStateFileName = ChangeFileExt(FFileName, ".txt");

    for (int I = 0; I < FActive->count(); I++) if ((*FActive)[I] == nullptr) St.Add((*FURL)[I]);
    if (St.Count() == 0) {
        DeleteFile(ActiveStateFileName);
    } else {
        St.Insert(0, "; Deactivated main package lists");
        St.SaveToFile(ActiveStateFileName);
    }
}

// { TPackageDB }

TPackageDB::TPackageDB()
{
    FOnDownload = nullptr;
    FPackageList = new TList;
}

TPackageDB::~TPackageDB()
{
    Clear();
    delete FPackageList;
}

void TPackageDB::Clear()
{
    for (int I = 0; I < FPackageList->count(); I++) delete static_cast<TPackageList*>((*FPackageList)[I]);
    FPackageList->clear();
}

int TPackageDB::GetCount()
{
    return FPackageList->count();
}

TPackageList* TPackageDB::GetPackageList(int I)
{
    if ((I < 0) || (I >= FPackageList->count())) return nullptr;
    return static_cast<TPackageList*>((*FPackageList)[I]);
}

bool TPackageDB::InitDBDir()
{
    FDBDir = IncludeTrailingPathDelimiter(ToNativePath(PrgDataDir() + PackageDBSubFolder));

    if (!DirectoryExists(FDBDir)) {
        if (!ForceDirectories(FDBDir)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {FDBDir}), mtError, mbOK, 0);
            return false;
        }
    }

    return true;
}

static bool IsInternetURL(const QString& URL)
{
    // Port: https is treated like http (see PackageDBToolsUnit.DownloadFile)
    return (ExtUpperCase(Copy(URL, 1, 7)) == "HTTP:/" "/") || (ExtUpperCase(Copy(URL, 1, 8)) == "HTTPS:/" "/");
}

TPackageList* TPackageDB::InitPackageFile(const QString& URL, bool Update, bool UpdateAll, bool UpdateRemoteOnly, bool UpdateErrorQuite)
{
    TPackageList* TempList;
    TPackageList* result = new TPackageList(URL);
    result->LoadFromFile(DBDir() + ExtractFileNameFromURL(URL, ".xml", false));

    if (!Update) return result;
    if (UpdateRemoteOnly && !IsInternetURL(URL)) return result;

    if (!DownloadFile(URL, DBDir() + PackageDBTempFile, UpdateErrorQuite)) return result;

    TempList = new TPackageList(URL);
    if (TempList->LoadFromFile(DBDir() + PackageDBTempFile, DBDir() + ExtractFileNameFromURL(URL, ".xml", false))) {
        if (!((TempList->UpdateDate() <= result->UpdateDate()) && (!UpdateAll))) {
            delete result;
            delete TempList; TempList = nullptr;
            ExtDeleteFile(DBDir() + ExtractFileNameFromURL(URL, ".xml", false), ftTemp);
            RenameFile(DBDir() + PackageDBTempFile, DBDir() + ExtractFileNameFromURL(URL, ".xml", false));
            result = new TPackageList(URL);
            result->LoadFromFile(DBDir() + ExtractFileNameFromURL(URL, ".xml", false));
        }
    }
    delete TempList;
    ExtDeleteFile(DBDir() + PackageDBTempFile, ftTemp);
    return result;
}

bool TPackageDB::FileInUse(const QString& FileName_)
{
    QString FileName = ExtUpperCase(ExtractFileName(FileName_));
    if (FileName == ExtUpperCase(PackageDBMainFile)) return true;
    if (FileName == ExtUpperCase(PackageDBUserFile)) return true;
    for (int I = 0; I < FPackageList->count(); I++) {
        if (FileName == ExtUpperCase(ExtractFileName(static_cast<TPackageList*>((*FPackageList)[I])->FileName()))) return true;
    }
    return false;
}

bool TPackageDB::InitPackageFiles(TPackageListFile* MainFile, TPackageListFile* UserFile, bool Update, bool UpdateAll, bool UpdateRemoteOnly, bool UpdateErrorQuite)
{
    int I, C1, C2;
    TPackageList* P;
    TSearchRec Rec;
    bool ContinueDownload = true;
    bool result;

    if (FOnDownload) FOnDownload(nullptr, 0, MainFile->Count() + UserFile->Count(), dsStart, ContinueDownload);
    ContinueDownload = true;

    // Load and update files

    C1 = 0; for (I = 0; I < MainFile->Count(); I++) if (MainFile->Active(I)) C1++;
    C2 = 0; for (I = 0; I < UserFile->Count(); I++) if (UserFile->Active(I)) C2++;

    for (I = 0; I < MainFile->Count(); I++) if (MainFile->Active(I)) {
        if (FOnDownload) FOnDownload(nullptr, I, C1 + C2, dsProgress, ContinueDownload);
        P = InitPackageFile(MainFile->URL(I), Update, UpdateAll, UpdateRemoteOnly, UpdateErrorQuite);
        if (P != nullptr) FPackageList->append(P);
        if (!ContinueDownload) break;
    }
    if (ContinueDownload) for (I = 0; I < UserFile->Count(); I++) if (UserFile->Active(I)) {
        ContinueDownload = true;
        if (FOnDownload) FOnDownload(nullptr, C1 + I, C1 + C2, dsProgress, ContinueDownload);
        P = InitPackageFile(UserFile->URL(I), Update, UpdateAll, UpdateRemoteOnly, UpdateErrorQuite);
        if (P != nullptr) FPackageList->append(P);
        if (!ContinueDownload) break;
    }
    if (FOnDownload) FOnDownload(nullptr, MainFile->Count() + UserFile->Count(), MainFile->Count() + UserFile->Count(), dsDone, ContinueDownload);
    result = ContinueDownload;

    // Delete unused files
    I = FindFirst(DBDir() + "*.xml", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            if (!FileInUse(Rec.Name)) ExtDeleteFile(DBDir() + Rec.Name, ftTemp);
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);

    return result;
}

bool TPackageDB::LoadDB(bool Update, bool UpdateAll, bool UpdateErrorQuite)
{
    TPackageListFile* MainFile;
    TPackageListFile* UserFile;
    bool B;
    bool result;

    Clear();
    result = true;
    if (!InitDBDir()) return result;

    MainFile = new TPackageListFile(true);
    MainFile->LoadFromFile(DBDir() + PackageDBMainFile);
    if (Update)
        result = MainFile->UpdateFromServer(Format(PackageDBMainFileURL, {GetNormalFileVersionAsString()}), DBDir() + PackageDBTempFile, UpdateAll, UpdateErrorQuite);
    else
        result = true;
    UserFile = new TPackageListFile(false);
    UserFile->LoadFromFile(DBDir() + PackageDBUserFile);
    B = InitPackageFiles(MainFile, UserFile, result && Update, result && UpdateAll, false, UpdateErrorQuite);
    result = result && B;
    delete UserFile;
    delete MainFile;
    return result;
}

void TPackageDB::UpdateRemoteFiles(bool UpdateErrorQuite)
{
    TPackageListFile* MainFile;
    TPackageListFile* UserFile;

    Clear();
    if (!InitDBDir()) return;

    MainFile = new TPackageListFile(true);
    MainFile->LoadFromFile(DBDir() + PackageDBMainFile);
    MainFile->UpdateFromServer(Format(PackageDBMainFileURL, {GetNormalFileVersionAsString()}), DBDir() + PackageDBTempFile, false, UpdateErrorQuite);
    UserFile = new TPackageListFile(false);
    UserFile->LoadFromFile(DBDir() + PackageDBUserFile);
    InitPackageFiles(MainFile, UserFile, true, false, true, UpdateErrorQuite);
    delete UserFile;
    delete MainFile;
}

bool TPackageDB::FindAndRemoveGame(TDownloadZipData* AGame, QWidget* ParentForm)
{
    for (int I = 0; I < FPackageList->count(); I++) if (static_cast<TPackageList*>((*FPackageList)[I])->FindAndRemoveGame(AGame, ParentForm)) return true;
    return false;
}

bool TPackageDB::FindAndRemoveAutoSetup(TDownloadAutoSetupData* AAutoSetup, QWidget* ParentForm)
{
    for (int I = 0; I < FPackageList->count(); I++) if (static_cast<TPackageList*>((*FPackageList)[I])->FindAndRemoveAutoSetup(AAutoSetup, ParentForm)) return true;
    return false;
}

bool TPackageDB::FindAndRemoveIcon(TDownloadIconData* AIcon, QWidget* ParentForm)
{
    for (int I = 0; I < FPackageList->count(); I++) if (static_cast<TPackageList*>((*FPackageList)[I])->FindAndRemoveIcon(AIcon, ParentForm)) return true;
    return false;
}

bool TPackageDB::FindAndRemoveIconSet(TDownloadIconSetData* AIconSet, QWidget* ParentForm)
{
    for (int I = 0; I < FPackageList->count(); I++) if (static_cast<TPackageList*>((*FPackageList)[I])->FindAndRemoveIconSet(AIconSet, ParentForm)) return true;
    return false;
}

bool TPackageDB::FindAndRemoveLanguage(TDownloadLanguageData* ALanguage, QWidget* ParentForm)
{
    for (int I = 0; I < FPackageList->count(); I++) if (static_cast<TPackageList*>((*FPackageList)[I])->FindAndRemoveLanguage(ALanguage, ParentForm)) return true;
    return false;
}
