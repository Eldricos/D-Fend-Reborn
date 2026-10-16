// D-Fend Reborn: game information (see GameInfoUnit.h)
#include "GameInfoUnit.h"

#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "GameDBUnit.h"

#include <QApplication>
#include <QEventLoop>
#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <algorithm>
#include <memory>

// --- standard values (Bin/GameInfo.ini) ---------------------------------------

namespace {

struct TGameInfoValues {
    QHash<QString, QString> Aliases[3];   // Genre, Language, License: lower case name -> standard value
    QHash<QString, QString> WikidataGenres;
    bool Loaded = false;
};

TGameInfoValues& Values()
{
    static TGameInfoValues V;
    if (V.Loaded) return V;
    V.Loaded = true;

    QString FileName = PrgDir() + BinFolder + PathDelim + "GameInfo.ini";
    if (!FileExists(FileName)) FileName = PrgDir() + "GameInfo.ini";
    bool Ok = false;
    const QString Text = ReadTextFile(FileName, &Ok);
    if (!Ok) return V;

    static const QStringList Kinds = {"Genre", "Language", "License"};
    QString Section;
    for (QString Line : Text.split(QRegularExpression("\r\n|\n|\r"))) {
        Line = Line.trimmed();
        if (Line.isEmpty() || Line.startsWith(';')) continue;
        if (Line.startsWith('[') && Line.endsWith(']')) { Section = Line.mid(1, Line.length() - 2); continue; }
        const int I = Line.indexOf('=');
        if (I <= 0) continue;
        const QString Key = Line.left(I).trimmed(), Value = Line.mid(I + 1).trimmed();
        for (int K = 0; K < 3; K++) {
            if (Section == Kinds[K] + "s") V.Aliases[K].insert(Key.toLower(), Key);
            if (Section == Kinds[K] + "Aliases") V.Aliases[K].insert(Key.toLower(), Value);
        }
        if (Section == "WikidataGenres") V.WikidataGenres.insert(Key.toLower(), Value);
    }
    return V;
}

QString StandardList(const QHash<QString, QString>& Aliases, const QString& Value)
{
    QStringList Result;
    for (const QString& Part : Value.split(QRegularExpression("[;,]"))) {
        const QString P = Part.trimmed();
        if (P.isEmpty()) continue;
        const QString Std = Aliases.value(P.toLower(), P);
        for (const QString& S : Std.split(';')) if ((!S.isEmpty()) && (!Result.contains(S))) Result << S;
    }
    return Result.join(';');
}

} // namespace

QString StandardGameInfoValue(const QString& Kind, const QString& Value)
{
    const int K = (Kind == "Genre") ? 0 : ((Kind == "Language") ? 1 : 2);
    return StandardList(Values().Aliases[K], Value);
}

// --- exported auto setup templates ---------------------------------------------

void NormaliseAutoSetupTemplateFile(const QString& FileName)
{
    bool Ok = false;
    const QString Text = ReadTextFile(FileName, &Ok);
    if (!Ok) return;
    const QString NL = Text.contains("\r\n") ? "\r\n" : "\n";
    QStringList Lines = Text.split(QRegularExpression("\r\n|\n"));

    static const QRegularExpression YearRe("(19[7-9]\\d|20[0-4]\\d)");
    QString Section;
    for (QString& Line : Lines) {
        if (Line.startsWith('[') && Line.endsWith(']')) { Section = Line.mid(1, Line.length() - 2); continue; }
        if (Section != "ExtraInfo") continue;
        const int I = Line.indexOf('=');
        if (I <= 0) continue;
        const QString Key = Line.left(I), Value = Line.mid(I + 1);
        QString NewValue = Value;
        if ((Key == "Genre") || (Key == "Language")) NewValue = StandardGameInfoValue(Key, Value);
        if (Key == "Year") {
            const QRegularExpressionMatch M = YearRe.match(Value);
            NewValue = M.hasMatch() ? M.captured(1) : QString();
        }
        if (Key == "UserInfo") {
            // License=...[13][10]Version=...[13][10]
            QStringList Items = Value.split("[13][10]");
            for (QString& Item : Items) if (Item.startsWith("License=")) Item = "License=" + StandardGameInfoValue("License", Item.mid(8));
            NewValue = Items.join("[13][10]");
        }
        if (NewValue != Value) Line = Key + "=" + NewValue;
    }
    WriteTextFile(FileName, Lines.join(NL), true, false);
}

// --- auto setup templates (offline) -------------------------------------------

static QString SearchKey(const QString& S)
{
    // lower case, only letters and digits, single spaces ("Keen 4: Oracle" -> "keen 4 oracle")
    QString R;
    for (const QChar C : S.toLower()) R += C.isLetterOrNumber() ? C : QChar(' ');
    return R.simplified();
}

QList<TGameInfo> SearchTemplateGameInfo(const QString& Name)
{
    static std::unique_ptr<TGameDB> AutoSetupDB;
    if (!AutoSetupDB) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        AutoSetupDB = std::make_unique<TGameDB>(PrgDataDir() + AutoSetupSubDir, false);
        QApplication::restoreOverrideCursor();
    }

    const QString Key = SearchKey(Name);
    const QStringList Words = Key.split(' ', Qt::SkipEmptyParts);
    struct THit { int Score; TGameInfo Info; };
    QList<THit> Hits;
    QSet<QString> Seen;
    for (int I = 0; I < AutoSetupDB->Count(); I++) {
        TGame* G = AutoSetupDB->Game(I);
        const QString N = Trim(G->Name());
        if (N.isEmpty()) continue;
        const QString K = SearchKey(N);
        int Score;
        if (K == Key) Score = 0;
        else if (K.startsWith(Key)) Score = 1;
        else {
            bool All = !Words.isEmpty();
            for (const QString& W : Words) if (!K.contains(W)) { All = false; break; }
            if (!All) continue;
            Score = 2;
        }
        TGameInfo Info;
        Info.Name = N;
        Info.Genre = Trim(G->Genre());
        Info.Developer = Trim(G->Developer());
        Info.Publisher = Trim(G->Publisher());
        Info.Year = Trim(G->Year());
        Info.Internet = Trim(G->WWW(1));
        Info.Notes = Trim(G->Notes()).replace("[13][10]", "\n");
        Info.Id = ExtractFileName(G->SetupFile());
        Info.DetailsLoaded = true;
        // (several templates for versions of a game: show the game once)
        const QString Dup = (Info.Name + "|" + Info.Developer + "|" + Info.Year).toLower();
        if (Seen.contains(Dup)) continue;
        Seen.insert(Dup);
        Hits.append({Score, Info});
    }
    std::stable_sort(Hits.begin(), Hits.end(), [](const THit& A, const THit& B) {
        if (A.Score != B.Score) return A.Score < B.Score;
        return A.Info.Name.compare(B.Info.Name, Qt::CaseInsensitive) < 0;
    });
    QList<TGameInfo> Result;
    for (const THit& H : Hits) { Result.append(H.Info); if (Result.count() >= 200) break; }
    return Result;
}

// --- Wikidata (online) -----------------------------------------------------------
// The same requests as tools/autosetup.py (lookup): items found by the
// Wikidata search are filtered with a SPARQL query (video games and MS-DOS
// programs); labels in English or as multilingual ("mul") labels.

static const QString WikidataVideoGame = QStringLiteral("Q7889");
static const QString WikidataMSDOS = QStringLiteral("Q31845");

static bool HTTPGetJson(const QUrl& URL, QJsonObject& Json, QString& Error)
{
    QNetworkAccessManager HTTP;
    QNetworkRequest Request(URL);
    // (Wikimedia asks for a User-Agent naming the program)
    Request.setRawHeader("User-Agent", ("D-Fend-Reborn/" + GetReleaseVersionAsString() + " (https://github.com/Eldricos/D-Fend-Reborn)").toUtf8());
    Request.setRawHeader("Accept", "application/sparql-results+json, application/json");
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    Request.setTransferTimeout(30000);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QNetworkReply* Reply = HTTP.get(Request);
    QEventLoop Loop;
    QObject::connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
    if (!Reply->isFinished()) Loop.exec(QEventLoop::ExcludeUserInputEvents);
    QApplication::restoreOverrideCursor();

    const bool OK = (Reply->error() == QNetworkReply::NoError);
    if (!OK) Error = URL.host() + ": " + Reply->errorString();
    const QByteArray Data = Reply->readAll();
    delete Reply;
    if (!OK) return false;
    QJsonParseError ParseError;
    const QJsonDocument Doc = QJsonDocument::fromJson(Data, &ParseError);
    if (!Doc.isObject()) { Error = URL.host() + ": " + ParseError.errorString(); return false; }
    Json = Doc.object();
    return true;
}

static bool Sparql(const QString& Query, QJsonArray& Rows, QString& Error)
{
    const QUrl URL = QUrl::fromEncoded("https://query.wikidata.org/sparql?format=json&query=" + QUrl::toPercentEncoding(Query), QUrl::StrictMode);
    QJsonObject Json;
    if (!HTTPGetJson(URL, Json, Error)) return false;
    Rows = Json.value("results").toObject().value("bindings").toArray();
    return true;
}

static QString Binding(const QJsonValue& Row, const QString& Name)
{
    return Row.toObject().value(Name).toObject().value("value").toString();
}

static QString ItemId(const QString& URI)
{
    return URI.mid(URI.lastIndexOf('/') + 1);
}

bool SearchWikidataGameInfo(const QString& Name, bool DOSOnly, QList<TGameInfo>& Result, QString& Error)
{
    Result.clear();
    if (Trim(Name).isEmpty()) return true;

    // 1. Wikidata search (labels and aliases)
    const QUrl URL = QUrl::fromEncoded("https://www.wikidata.org/w/api.php?action=wbsearchentities&language=en&uselang=en&type=item&limit=50&format=json&search="
                                       + QUrl::toPercentEncoding(Trim(Name)), QUrl::StrictMode);
    QJsonObject Json;
    if (!HTTPGetJson(URL, Json, Error)) return false;
    QStringList Ids;
    for (const QJsonValue& V : Json.value("search").toArray()) {
        const QString Id = V.toObject().value("id").toString();
        if (QRegularExpression("^Q\\d+$").match(Id).hasMatch()) Ids << Id;
    }
    if (Ids.isEmpty()) return true;

    // 2. games and DOS programs among them; DOS: a platform whose name
    // contains "DOS" (MS-DOS, DOS, PC DOS, ...)
    QStringList Values;
    for (const QString& Id : Ids) Values << "wd:" + Id;
    const QString Query = QString(
        "SELECT ?item ?itemLabel (MIN(YEAR(?date)) AS ?year) (GROUP_CONCAT(DISTINCT ?pfName; separator=\"|\") AS ?platforms) WHERE {\n"
        "  VALUES ?item { %1 }\n"
        "  { ?item wdt:P31/wdt:P279* wd:%2 . } UNION { ?item wdt:P400 wd:%3 . }\n"
        "  OPTIONAL { ?item wdt:P577 ?date . }\n"
        "  OPTIONAL { ?item wdt:P400 ?pf . ?pf rdfs:label ?pfName . FILTER(LANG(?pfName) IN (\"en\", \"mul\")) }\n"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en,mul\" . }\n"
        "} GROUP BY ?item ?itemLabel").arg(Values.join(' '), WikidataVideoGame, WikidataMSDOS);
    QJsonArray Rows;
    if (!Sparql(Query, Rows, Error)) return false;

    QHash<QString, TGameInfo> Found;
    for (const QJsonValue& Row : Rows) {
        if (DOSOnly && (!Binding(Row, "platforms").contains("DOS"))) continue;
        TGameInfo Info;
        Info.Id = ItemId(Binding(Row, "item"));
        Info.Name = Binding(Row, "itemLabel");
        Info.Year = Binding(Row, "year");
        Found.insert(Info.Id, Info);
    }
    for (const QString& Id : Ids) if (Found.contains(Id)) Result.append(Found.value(Id));   // (search order)
    return true;
}

bool LoadWikidataGameInfo(TGameInfo& Info, QString& Error)
{
    if (Info.DetailsLoaded) return true;
    if (!QRegularExpression("^Q\\d+$").match(Info.Id).hasMatch()) return false;

    const QString Query = QString(
        "SELECT ?itemLabel ?itemDescription ?devLabel ?pubLabel ?genreName ?date ?website ?article WHERE {\n"
        "  VALUES ?item { wd:%1 }\n"
        "  OPTIONAL { ?item wdt:P178 ?dev . }\n"
        "  OPTIONAL { ?item wdt:P123 ?pub . }\n"
        "  OPTIONAL { ?item wdt:P136 ?genre . ?genre rdfs:label ?genreName . FILTER(LANG(?genreName) = \"en\") }\n"
        "  OPTIONAL { ?item wdt:P577 ?date . }\n"
        "  OPTIONAL { ?item wdt:P856 ?website . }\n"
        "  OPTIONAL { ?article schema:about ?item ; schema:isPartOf <https://en.wikipedia.org/> . }\n"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en,mul\" . }\n"
        "}").arg(Info.Id);
    QJsonArray Rows;
    if (!Sparql(Query, Rows, Error)) return false;

    auto Collect = [&Rows](const QString& Key) {
        QStringList L;
        for (const QJsonValue& Row : Rows) {
            const QString V = Binding(Row, Key);
            if ((!V.isEmpty()) && (!L.contains(V))) L << V;
        }
        return L;
    };

    QStringList Genres;
    const TGameInfoValues& V = Values();
    for (const QString& G : Collect("genreName")) {
        const QString Std = V.WikidataGenres.value(G.toLower());
        for (const QString& S : Std.split(';')) if ((!S.isEmpty()) && (!Genres.contains(S))) Genres << S;
    }
    QStringList Years;
    for (const QString& D : Collect("date")) Years << D.left(4);
    std::sort(Years.begin(), Years.end());

    if (!Collect("itemLabel").isEmpty()) Info.Name = Collect("itemLabel").first();
    Info.Genre = Genres.join(';');
    Info.Developer = Collect("devLabel").join(", ");
    Info.Publisher = Collect("pubLabel").join(", ");
    if (!Years.isEmpty()) Info.Year = Years.first();
    QStringList WWW = Collect("website");
    if (WWW.isEmpty()) WWW = Collect("article");
    Info.Internet = WWW.isEmpty() ? ("https://www.wikidata.org/wiki/" + Info.Id) : WWW.first();
    Info.Notes = Collect("itemDescription").isEmpty() ? QString() : Collect("itemDescription").first();
    Info.DetailsLoaded = true;
    return true;
}
