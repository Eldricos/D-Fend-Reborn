// C++ port of DataReaderTheGamesDBUnit.pas (implementation section).
#include "DataReaderTheGamesDBUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "InternetDataWaitFormUnit.h"
#include "DataReaderToolsUnit.h"
#include "PackageDBToolsUnit.h"
#include <memory>

// IXMLNode.NodeValue of an element with text content → QDomElement::text()
static QString NodeValue(const QDomNode& N) { return N.toElement().text(); }

/* TTheGamesDBDataReader */

TTheGamesDBDataReader::TTheGamesDBDataReader() : TDataReader()
{
    FDataDomain = "thegamesdb.net";
    FBrowserURLAll = "http://thegamesdb.net/";
    FBrowserURLDOS = "http://thegamesdb.net/browse/1/";
}

TTheGamesDBDataReader::~TTheGamesDBDataReader()
{
}

QString TTheGamesDBDataReader::GetListURL(bool all)
{
    if (all) return "http://thegamesdb.net/api/GetGamesList.php?name=%s"; else return "http://thegamesdb.net/api/GetGamesList.php?name=%s&platform=PC";
}

bool TTheGamesDBDataReader::ReadList(const QString& ASearchString)
{
    QString URL;
    QString TempFile;
    int I;

    FGameNames->Clear();
    FGameURLs->Clear();
    FLastListRequest = "";
    bool result = false;
    URL = Format(GetListURL(PrgSetup->DataReaderAllPlatforms()), {EncodeName(ASearchString)});
    TempFile = TempDir() + PackageDBTempFile;
    if (!DownloadFileToDisk(URL, TempFile)) return result;
    {
        // CoInitialize(nil) is not needed (no COM/MSXML)
        std::unique_ptr<QDomDocument> XMLDoc(LoadXMLDoc(TempFile));
        if (XMLDoc == nullptr) { ExtDeleteFile(TempFile, ftTemp); return result; }
        QDomElement Root = XMLDoc->documentElement();
        if (Root.nodeName() != "Data") { ExtDeleteFile(TempFile, ftTemp); return result; }
        QDomNodeList Children = Root.childNodes();
        for (I = 0; I < Children.count(); I++) {
            QDomNode N = Children.at(I);
            if (N.nodeName() == "Game") {
                QDomElement N2 = N.firstChildElement("id");
                QDomElement N3 = N.firstChildElement("GameTitle");
                if ((!N2.isNull()) && (!N3.isNull())) {
                    FGameNames->Add(NodeValue(N3));
                    FGameURLs->Add(NodeValue(N2));
                }
            }
        }
    }
    ExtDeleteFile(TempFile, ftTemp);
    result = true;
    return result;
}

bool TTheGamesDBDataReader::GetGameData(int Nr, QString& Genre, QString& Developer, QString& Publisher, QString& Year, QString& Notes, QString& ImagePageURL)
{
    QString Id, URL, TempFile;
    int I, J, K;
    QString baseImgUrl;
    QString S;
    TStringList* St;

    bool result = false;
    Genre = ""; Developer = ""; Publisher = ""; Year = ""; Notes = ""; ImagePageURL = "";
    if ((Nr < 0) || (Nr >= FGameURLs->Count())) return result;
    Id = (*FGameURLs)[Nr];

    URL = Format("http://thegamesdb.net/api/GetGame.php?id=%s", {Id});
    TempFile = TempDir() + PackageDBTempFile;

    TempFile = TempDir() + PackageDBTempFile;
    if (!DownloadFileToDisk(URL, TempFile)) return result;
    {
        // CoInitialize(nil) is not needed (no COM/MSXML)
        std::unique_ptr<QDomDocument> XMLDoc(LoadXMLDoc(TempFile));
        if (XMLDoc == nullptr) { ExtDeleteFile(TempFile, ftTemp); return result; }
        QDomElement Root = XMLDoc->documentElement();
        if (Root.nodeName() != "Data") { ExtDeleteFile(TempFile, ftTemp); return result; }
        baseImgUrl = "";
        QDomNodeList Children = Root.childNodes();
        for (I = 0; I < Children.count(); I++) {
            QDomNode N = Children.at(I);
            if (N.nodeName() == "baseImgUrl") {
                baseImgUrl = NodeValue(N);
                continue;
            }
            if (N.nodeName() == "Game") {
                QDomNodeList GameChildren = N.childNodes();
                for (J = 0; J < GameChildren.count(); J++) {
                    QDomNode N2 = GameChildren.at(J);
                    S = N2.nodeName();
                    if (S == "ReleaseDate") { Year = NodeValue(N2); continue; }
                    if (S == "Publisher") { Publisher = NodeValue(N2); continue; }
                    if (S == "Developer") { Developer = NodeValue(N2); continue; }
                    if (S == "Overview") { Notes = NodeValue(N2); continue; }
                    if (S == "Genres") {
                        QDomNodeList GenreChildren = N2.childNodes();
                        for (K = 0; K < GenreChildren.count(); K++) {
                            QDomNode N3 = GenreChildren.at(K);
                            if (N3.nodeName() == "genre") {
                                if (!Genre.isEmpty()) Genre = Genre + "; ";
                                Genre = Genre + NodeValue(N3);
                                continue;
                            }
                        }
                        continue;
                    }
                    if (S == "Images") {
                        St = ProcessImages(baseImgUrl, N2.toElement());
                        if (St != nullptr) {
                            ImagePageURL = StringListToString(St);
                            delete St;
                        }
                        continue;
                    }
                }
                continue;
            }
        }
    }
    ExtDeleteFile(TempFile, ftTemp);
    result = true;
    return result;
}

TStringList* TTheGamesDBDataReader::ProcessImages(const QString& base, const QDomElement& Node)
{
    int I;
    TStringList* St[5];   // Delphi: Array[1..5]
    QString S;

    TStringList* result = new TStringList;

    for (I = 0; I < 5; I++) St[I] = new TStringList;
    QDomNodeList Children = Node.childNodes();
    for (I = 0; I < Children.count(); I++) {
        QDomElement N = Children.at(I).toElement();
        S = Children.at(I).nodeName();
        if (S == "boxart") {
            if (N.attribute("side") == "front") St[0]->Add(base + NodeValue(N)); else St[1]->Add(base + NodeValue(N));
            continue;
        }
        if (S == "fanart") {
            QDomElement N2 = N.firstChildElement("original");
            if (!N2.isNull()) St[2]->Add(base + NodeValue(N2));
            continue;
        }
        if (S == "screenshot") {
            QDomElement N2 = N.firstChildElement("original");
            if (!N2.isNull()) St[3]->Add(base + NodeValue(N2));
            continue;
        }
        if (S == "banner") { St[4]->Add(base + NodeValue(N)); continue; }
    }
    for (I = 0; I < 5; I++) result->AddStrings(*St[I]);
    for (I = 0; I < 5; I++) delete St[I];
    return result;
}

bool TTheGamesDBDataReader::GetGameCover(const QString& CoverPageURL, QString& ImageURL, int MaxImages)
{
    TStringList *St, *St2;
    int I;

    St = StringToStringList(CoverPageURL);
    St2 = new TStringList;
    for (I = 0; I <= Min(St->Count() - 1, MaxImages); I++) St2->Add((*St)[I]);
    ImageURL = ListToValue(St2, '$');
    delete St2;
    delete St;
    return true;
}
