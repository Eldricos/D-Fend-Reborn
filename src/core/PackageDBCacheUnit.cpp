// C++ port of PackageDBCacheUnit.pas (implementation section).
#include "PackageDBCacheUnit.h"

#include "PrgConsts.h"
#include "CommonTools.h"
#include "HashCalc.h"
#include "PackageDBToolsUnit.h"
#include "PrgSetupUnit.h"

// { TCachedPackage }

TCachedPackage::TCachedPackage(const QString& AName, const QString& ADescription, const QString& AFilename)
{
    TSearchRec Rec;
    int I;

    FName = AName;
    FDescription = ADescription;
    FFilename = AFilename;
    FPackageChecksum = GetMD5Sum(FFilename, false);

    if (!FileExists(FFilename)) return;
    I = FindFirst(FFilename, faAnyFile, Rec);
    if (I == 0) FSize = static_cast<int>(Rec.Size);
    FindClose(Rec);
}

// { TPackageDBCache }

TPackageDBCache::TPackageDBCache()
{
    InitDBDir();
    FList = new TList;
    LoadCache();
}

TPackageDBCache::~TPackageDBCache()
{
    SaveCache();
    for (int I = 0; I < FList->count(); I++) delete static_cast<TCachedPackage*>((*FList)[I]);
    delete FList;
}

void TPackageDBCache::InitDBDir()
{
    FDBDir = IncludeTrailingPathDelimiter(ToNativePath(PrgDataDir() + PackageDBCacheSubFolder));
    ForceDirectories(FDBDir);
}

int TPackageDBCache::GetCount()
{
    return FList->count();
}

TCachedPackage* TPackageDBCache::GetPackage(int I)
{
    if ((I < 0) || (I >= FList->count())) return nullptr;
    return static_cast<TCachedPackage*>((*FList)[I]);
}

TCachedPackage* TPackageDBCache::FindPackage(const QString& AName)
{
    for (int I = 0; I < FList->count(); I++) if (static_cast<TCachedPackage*>((*FList)[I])->Name() == AName) return static_cast<TCachedPackage*>((*FList)[I]);
    return nullptr;
}

void TPackageDBCache::AddCache(const QString& AName, const QString& ADescription, const QString& AFilename)
{
    FList->append(new TCachedPackage(AName, ADescription, AFilename));
    SaveCache();
}

void TPackageDBCache::DelCache(const QString& AName)
{
    DelCache(FindPackage(AName));
}

void TPackageDBCache::DelCache(TCachedPackage* ACachedPackage)
{
    if (ACachedPackage == nullptr) return;
    ExtDeleteFile(ACachedPackage->Filename(), ftUninstall);
    FList->removeAt(FList->indexOf(ACachedPackage));
    delete ACachedPackage;
    SaveCache();
}

QString TPackageDBCache::FindChecksum(const QString& AName)
{
    for (int I = 0; I < FList->count(); I++) if (static_cast<TCachedPackage*>((*FList)[I])->Name() == AName) return static_cast<TCachedPackage*>((*FList)[I])->PackageChecksum();
    return QString();
}

void TPackageDBCache::LoadCache()
{
    QDomDocument* XMLDoc;
    TSearchRec Rec;
    int I;
    QString S;
    bool OK;

    // Step 1: Read xml file
    XMLDoc = LoadXMLDoc(FDBDir + PackageDBCacheFile);
    if (XMLDoc != nullptr) {
        QDomElement Root = XMLDoc->documentElement();
        if (Root.nodeName() == "DFRPackagesCache") {
            for (QDomNode Node = Root.firstChild(); !Node.isNull(); Node = Node.nextSibling()) {
                QDomElement N = Node.toElement();
                if (N.isNull()) continue;
                if ((N.nodeName() == "ExePackage") && N.hasAttribute("Name") && N.hasAttribute("Description") && (N.text() != ""))
                    FList->append(new TCachedPackage(N.attribute("Name"), N.attribute("Description"), FDBDir + N.text()));
            }
        }
        delete XMLDoc;
    }

    // Step 2: Read directory and add missing files
    I = FindFirst(FDBDir + "*.exe", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) {
            S = ExtUpperCase(Rec.Name);
            OK = false;
            for (int J = 0; J < FList->count(); J++) if (ExtUpperCase(ExtractFileName(static_cast<TCachedPackage*>((*FList)[J])->Filename())) == S) { OK = true; break; }
            if (!OK) FList->append(new TCachedPackage(ChangeFileExt(Rec.Name, ""), ChangeFileExt(Rec.Name, ""), FDBDir + Rec.Name));
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

void TPackageDBCache::SaveCache()
{
    if (FList->count() == 0) {
        ExtDeleteFile(FDBDir + PackageDBCacheFile, ftTemp);
        return;
    }

    QDomDocument XMLDoc;
    XMLDoc.appendChild(XMLDoc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""));
    QDomElement Root = XMLDoc.createElement("DFRPackagesCache");
    XMLDoc.appendChild(Root);
    for (int I = 0; I < FList->count(); I++) {
        TCachedPackage* P = static_cast<TCachedPackage*>((*FList)[I]);
        QDomElement N = XMLDoc.createElement("ExePackage");
        Root.appendChild(N);
        N.setAttribute("Name", P->Name());
        N.setAttribute("Description", P->Description());
        N.appendChild(XMLDoc.createTextNode(ExtractFileName(P->Filename())));
    }
    QFile F(FDBDir + PackageDBCacheFile);
    if (F.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        F.write(XMLDoc.toByteArray(2));
        F.close();
    }
}

void TPackageDBCache::ClearCache()
{
    for (int I = 0; I < FList->count(); I++) {
        ExtDeleteFile(static_cast<TCachedPackage*>((*FList)[I])->Filename(), ftUninstall);
        delete static_cast<TCachedPackage*>((*FList)[I]);
    }
    FList->clear();
    SaveCache();
}
