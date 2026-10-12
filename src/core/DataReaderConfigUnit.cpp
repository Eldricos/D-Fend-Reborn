// C++ port of DataReaderConfigUnit.pas (implementation section).
// IXMLNode → QDomElement (Node.Attributes[X] → attribute(X),
// Node.ChildNodes → childNodes()).
#include "DataReaderConfigUnit.h"
#include "CommonTools.h"
#include "PackageDBToolsUnit.h"

/* global */

static THTMLStructureElement* HTMLStructElementFromXMLNode(const QDomElement& Node, bool AllowMultipleChildrenInNextLevel)
{
    THTMLStructureElement* result = nullptr;
    if (Node.childNodes().count() != 1) return result;
    QDomNode NN = Node.childNodes().at(0);
    QDomElement N = NN.toElement();
    if (NN.nodeName() == "Element") result = new TElement(N, AllowMultipleChildrenInNextLevel);
    if (NN.nodeName() == "PerGameBlock") result = new TPerGameBlock(N);
    if (NN.nodeName() == "Link") result = new TLink(N);
    if (NN.nodeName() == "Img") result = new TImg(N);
    if (NN.nodeName() == "Javascript") result = new TJavascript(N);
    if (result != nullptr) {
        if (!result->LoadOK()) { delete result; result = nullptr; }
    }
    return result;
}

static bool HTMLStructElementFromXMLNode(const QDomElement& Node, TList* List)
{
    bool result = false;
    QDomNodeList Children = Node.childNodes();
    for (int I = 0; I < Children.count(); I++) {
        QDomNode NN = Children.at(I);
        QDomElement N = NN.toElement();
        THTMLStructureElement* E = nullptr;
        if (NN.nodeName() == "Element") E = new TElement(N, true);
        if (NN.nodeName() == "PerGameBlock") E = new TPerGameBlock(N);
        if (NN.nodeName() == "Link") E = new TLink(N);
        if (NN.nodeName() == "SearchGameData") E = new TSearchGameData(N);
        if (E == nullptr) return result; else List->append(E);
    }
    result = true;
    return result;
}

/* THTMLStructureElement */

THTMLStructureElement::THTMLStructureElement()
{
    FLoadOK = true;
}

/* TLink */

TLink::TLink(const QDomElement& Node) : THTMLStructureElement()
{
    if (Node.hasAttribute("Key")) FAttributeKey = Node.attribute("Key"); else FAttributeKey = "";
    if (Node.hasAttribute("Value")) FAttributeValue = Node.attribute("Value"); else FAttributeValue = "";
    if (Node.hasAttribute("Nr")) { FLoadOK = TryStrToInt(Node.attribute("Nr"), FNr); if (!FLoadOK) return; } else FNr = 0;
    if (Node.hasAttribute("HrefSubstring")) FHrefSubstring = Node.attribute("HrefSubstring"); else FHrefSubstring = "";
}

/* TImg */

TImg::TImg(const QDomElement& Node) : THTMLStructureElement()
{
    if (Node.hasAttribute("Key")) FAttributeKey = Node.attribute("Key"); else FAttributeKey = "";
    if (Node.hasAttribute("Value")) FAttributeValue = Node.attribute("Value"); else FAttributeValue = "";
    if (Node.hasAttribute("Nr")) { FLoadOK = TryStrToInt(Node.attribute("Nr"), FNr); if (!FLoadOK) return; } else FNr = 0;
}

/* TElementWithSub */

TElementWithSub::TElementWithSub(const QDomElement& Node, bool AllowMultipleChildren) : THTMLStructureElement()
{
    FChildren = new TList;
    if (AllowMultipleChildren) {
        FLoadOK = HTMLStructElementFromXMLNode(Node, FChildren);
    } else {
        FChild = HTMLStructElementFromXMLNode(Node, false); FChildren->append(FChild);
        FLoadOK = (FChild != nullptr);
    }
}

TElementWithSub::~TElementWithSub()
{
    for (int I = 0; I < FChildren->count(); I++) delete static_cast<THTMLStructureElement*>((*FChildren)[I]);
    delete FChildren;
}

int TElementWithSub::GetChildCount()
{
    return FChildren->count();
}

THTMLStructureElement* TElementWithSub::GetChildren(int I)
{
    if ((I < 0) || (I >= FChildren->count())) return nullptr; else return static_cast<THTMLStructureElement*>((*FChildren)[I]);
}

/* TElement */

TElement::TElement(const QDomElement& Node, bool AllowMultipleChildren) : TElementWithSub(Node, AllowMultipleChildren)
{
    if (!LoadOK()) return;

    if (Node.hasAttribute("Type")) FElementType = Node.attribute("Type"); else FElementType = "";
    if (Node.hasAttribute("Key")) FAttributeKey = Node.attribute("Key"); else FAttributeKey = "";
    if (Node.hasAttribute("Value")) FAttributeValue = Node.attribute("Value"); else FAttributeValue = "";
    if (Node.hasAttribute("Nr")) { FLoadOK = TryStrToInt(Node.attribute("Nr"), FNr); if (!FLoadOK) return; } else FNr = 0;
    if (Node.hasAttribute("ContainsText")) FContainsText = Node.attribute("ContainsText"); else FContainsText = "";
}

/* TJavascript */

TJavascript::TJavascript(const QDomElement& Node) : THTMLStructureElement()
{
    if (Node.hasAttribute("ContainsText")) FContainsText = Node.attribute("ContainsText"); else FContainsText = "";
}

/* TPerGameBlock */

TPerGameBlock::TPerGameBlock(const QDomElement& Node) : TElementWithSub(Node, false)
{
    if (!LoadOK()) return;

    if (Node.hasAttribute("Type")) FElementType = Node.attribute("Type"); else FElementType = "";
    if (Node.hasAttribute("Key")) FAttributeKey = Node.attribute("Key"); else FAttributeKey = "";
    if (Node.hasAttribute("Value")) FAttributeValue = Node.attribute("Value"); else FAttributeValue = "";
}

/* TSearchGameData */

TSearchGameData::TSearchGameData(const QDomElement& Node) : THTMLStructureElement()
{
    if (Node.hasAttribute("Genre")) FGenre = Node.attribute("Genre"); else FGenre = "";
    if (Node.hasAttribute("Developer")) FDeveloper = Node.attribute("Developer"); else FDeveloper = "";
    if (Node.hasAttribute("Publisher")) FPublisher = Node.attribute("Publisher"); else FPublisher = "";
    if (Node.hasAttribute("Year")) FYear = Node.attribute("Year"); else FYear = "";
    if (Node.hasAttribute("Notes")) FNotes = Node.attribute("Notes"); else FNotes = "";

    if (Node.hasAttribute("IgnoreTags")) {
        FIgnoreTags = ValueToList(Node.attribute("IgnoreTags"));
    } else {
        FIgnoreTags = new TStringList;
    }

    if (Node.hasAttribute("StartTokens")) {
        FStartTokens = ValueToList(Node.attribute("StartTokens"));
    } else {
        FStartTokens = new TStringList;
    }

    if (Node.hasAttribute("StoppTokens")) {
        FStoppTokens = ValueToList(Node.attribute("StoppTokens"));
    } else {
        FStoppTokens = new TStringList;
    }
}

TSearchGameData::~TSearchGameData()
{
    delete FIgnoreTags;
    delete FStartTokens;
    delete FStoppTokens;
}

/* TDataReaderConfig */

TDataReaderConfig::TDataReaderConfig(const QString& AConfigFile)
{
    FConfigOK = false;
    FConfigFile = AConfigFile;
    FVersion = 0;
    FGamesListURL = "";
    FGamesListAllPlatformsURL = "";
    FGamesList = nullptr;
    FGameRecordBaseURL = "";
    FGameRecord = nullptr;
    FCoverRecordBaseURL = "";
    FCoverRecord = nullptr;

    QDomDocument* XMLDoc = LoadXMLDoc(FConfigFile, FConfigFile); if (XMLDoc == nullptr) return;
    std::unique_ptr<QDomDocument> XMLDocFree(XMLDoc);   // finally XMLDoc.Free

    QDomElement Root = XMLDoc->documentElement();
    if (Root.nodeName() != "DataReader") return;
    if (!Root.hasAttribute("Version")) return;
    if (!TryStrToInt(Root.attribute("Version"), FVersion)) return;

    QDomNodeList Children = Root.childNodes();
    for (int I = 0; I < Children.count(); I++) {
        QDomNode NN = Children.at(I);
        QDomElement N = NN.toElement();
        if (NN.nodeName() == "GamesList") {
            if (FGamesList != nullptr) return;
            if (N.hasAttribute("URL")) FGamesListURL = N.attribute("URL"); else return;
            FGamesList = HTMLStructElementFromXMLNode(N, true); if (FGamesList == nullptr) return;
            if (N.hasAttribute("AllPlatformsURL"))
                FGamesListAllPlatformsURL = N.attribute("AllPlatformsURL");
            else
                FGamesListAllPlatformsURL = FGamesListURL;
        }
        if (NN.nodeName() == "GamePage") {
            if (FGameRecord != nullptr) return;
            if (N.hasAttribute("URL")) FGameRecordBaseURL = N.attribute("URL"); else return;
            FGameRecord = HTMLStructElementFromXMLNode(N, true); if (FGameRecord == nullptr) return;
        }
        if (NN.nodeName() == "CoverPage") {
            if (FCoverRecord != nullptr) return;
            if (N.hasAttribute("URL")) FCoverRecordBaseURL = N.attribute("URL"); else return;
            FCoverRecord = HTMLStructElementFromXMLNode(N, false); if (FCoverRecord == nullptr) return;
        }
    }
    FConfigOK = (FGamesList != nullptr) && (FGameRecord != nullptr) && (FCoverRecord != nullptr);
    FConfigOK = true;   // as in the Delphi code (statement after the try/finally block)
}

TDataReaderConfig::~TDataReaderConfig()
{
    if (FGamesList != nullptr) delete FGamesList;
    if (FGameRecord != nullptr) delete FGameRecord;
    if (FCoverRecord != nullptr) delete FCoverRecord;
}
