// C++ port of DataReaderMobyUnit.pas (implementation section).
#include "DataReaderMobyUnit.h"
#include "CommonTools.h"
#include "DataReaderToolsUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "InternetDataWaitFormUnit.h"
#include <cmath>

// "Nr" selection used in many places: Nr>0 = from start, Nr<0 = from last.
// The Delphi code used St[St.Count-I] for I<0, which is out of range (raises
// EStringListError); the port counts from the end (St[St.Count+I]).
static int NrIndex(int Count, int I) { return Count + I; }

/* TMobyDataReader */

TMobyDataReader::TMobyDataReader() : TDataReader()
{
    FConfig = nullptr;
}

TMobyDataReader::~TMobyDataReader()
{
    if (FConfig != nullptr) delete FConfig;
}

bool TMobyDataReader::LoadConfig(const QString& AConfigFile, bool UpdateCheck)
{
    TDataReaderConfig* TempConfig;
    int I;
    bool result = false;

    if ((!FileExists(AConfigFile)) && (!DownloadConfigFile(AConfigFile))) return result;

    if (FConfig != nullptr) { delete FConfig; FConfig = nullptr; }

    do {
        FConfig = new TDataReaderConfig(AConfigFile);
        result = FConfig->ConfigOK();
        if (!result) {
            delete FConfig; FConfig = nullptr;
            if (!DownloadConfigFile(AConfigFile)) return result;
        }
    } while (!result);

    if (UpdateCheck) {
        const QString TempFile = TempDir() + ExtractFileName(AConfigFile);
        FLastUpdateCheckOK = DownloadConfigFile(TempFile);
        if (FLastUpdateCheckOK) {
            TempConfig = new TDataReaderConfig(TempFile);
            if (TempConfig->ConfigOK()) I = TempConfig->Version(); else I = -1;
            delete TempConfig;
            if (I > FConfig->Version()) {
                delete FConfig;
                CopyFile(TempFile, AConfigFile, false);
                FConfig = new TDataReaderConfig(AConfigFile);
                result = FConfig->ConfigOK();
                if (!result) { ExtDeleteFile(TempFile, ftTemp); return result; }
            }
            ExtDeleteFile(TempFile, ftTemp);
        }
    }

    FBrowserURLDOS = FConfig->GamesListURL();
    FBrowserURLAll = FConfig->GamesListAllPlatformsURL();
    FDataDomain = DomainOnly(FConfig->GameRecordBaseURL());
    return result;
}

bool TMobyDataReader::DownloadConfigFile(const QString& DestFile)
{
    return DownloadFileToDisk(DataReaderUpdateURL, DestFile);
}

bool TMobyDataReader::ReadList(const QString& ASearchString)
{
    QString Lines, URL, S;
    int I, Count;

    FGameNames->Clear();
    FGameURLs->Clear();
    FLastListRequest = "";
    bool result = false;
    if (FConfig == nullptr) return result;
    URL = GetListURL(PrgSetup->DataReaderAllPlatforms());

    result = false;
    URL = Format(URL, {EncodeName(ASearchString)});
    for (I = 0; I <= 10; I++) {
        S = URL;
        if (I > 0) {
            if (S.isEmpty() || S[Length(S) - 1] != '/') S = S + "/";
            S = S + "offset," + IntToStr(I * 25) + "/";
        }
        if (!DownloadTextFile(S, Lines)) return result;
        Count = FGameURLs->Count();
        if (ReadListGlobalPart(Lines, FConfig->GamesList())) result = true;
        if (FGameURLs->Count() == Count) break;
    }

    if (result) FLastListRequest = Format(URL, {EncodeName(ASearchString)});
    result = true; /* if no games found but no error occured, report true */
    return result;
}

QString TMobyDataReader::GetListURL(bool all)
{
    if (all) return FConfig->GamesListAllPlatformsURL(); else return FConfig->GamesListURL();
}

bool TMobyDataReader::ReadListGlobalPart(const QString& Lines, THTMLStructureElement* NextElement)
{
    TStringList* St;
    int I;
    QString S, T;
    bool result = false;

    if (auto* E = dynamic_cast<TElement*>(NextElement)) {
        St = GetTagContents(Lines, 1, E->ElementType(), E->AttributeKey(), E->AttributeValue());
        if (St->Count() == 0) { delete St; return result; }
        S = (*St)[0];
        if (E->Nr() != 0) {
            I = E->Nr();
            if ((I > 0) && (St->Count() >= I)) S = (*St)[I - 1];
            if ((I < 0) && (St->Count() >= -I)) S = (*St)[NrIndex(St->Count(), I)];
        }
        result = false;
        for (I = 0; I < E->ChildCount(); I++) {
            if (ReadListGlobalPart(S, E->Children(I))) result = true;
        }
        delete St;
        return result;
    }

    if (auto* E = dynamic_cast<TPerGameBlock*>(NextElement)) {
        St = GetTagContents(Lines, 1, E->ElementType(), E->AttributeKey(), E->AttributeValue());
        if (St->Count() == 0) { delete St; return result; }
        result = true;
        for (I = 0; I < St->Count(); I++) {
            ReadListPerGamePart((*St)[I], E->Child(), S, T);
            if ((!S.isEmpty()) && (!T.isEmpty()) && (FGameURLs->IndexOf(T) < 0)) { FGameNames->Add(DecodeName(S)); FGameURLs->Add(T); }
        }
        delete St;
        return result;
    }
    return result;
}

void TMobyDataReader::ReadListPerGamePart(const QString& Lines, THTMLStructureElement* NextElement, QString& GameName, QString& GameURL)
{
    TStringList *St, *St2;
    int I;
    QString S;

    GameName = ""; GameURL = "";

    if (auto* E = dynamic_cast<TElement*>(NextElement)) {
        St = GetTagContents(Lines, 1, E->ElementType(), E->AttributeKey(), E->AttributeValue());
        if (St->Count() == 0) { delete St; return; }
        S = (*St)[0];
        if (E->Nr() != 0) {
            I = E->Nr();
            if ((I > 0) && (St->Count() >= I)) S = (*St)[I - 1];
            if ((I < 0) && (St->Count() >= -I)) S = (*St)[NrIndex(St->Count(), I)];
        }
        ReadListPerGamePart(S, E->Child(), GameName, GameURL);
        delete St;
        return;
    }

    if (auto* L = dynamic_cast<TLink*>(NextElement)) {
        St2 = new TStringList;
        St = GetTagContents(Lines, 1, "a", L->AttributeKey(), L->AttributeValue(), "href", St2);
        if (St->Count() != 0) {
            GameName = (*St)[0]; GameURL = (*St2)[0];
            if (L->Nr() != 0) {
                I = L->Nr();
                if ((I > 0) && (St->Count() >= I)) { GameName = (*St)[I - 1]; GameURL = (*St2)[I - 1]; }
                if ((I < 0) && (St->Count() >= -I)) { GameName = (*St)[NrIndex(St->Count(), I)]; GameURL = (*St2)[NrIndex(St->Count(), I)]; }
            }
            S = Trim(L->HrefSubstring());
            if ((!S.isEmpty()) && (Pos(ExtUpperCase(S), ExtUpperCase(GameURL)) == 0)) { GameName = ""; GameURL = ""; }
        }
        delete St;
        delete St2;
        return;
    }
}

bool TMobyDataReader::GetGameData(int Nr, QString& Genre, QString& Developer, QString& Publisher, QString& Year, QString& Notes, QString& ImagePageURL)
{
    QString S, T, Lines;
    bool result = false;
    Genre = ""; Developer = ""; Publisher = ""; Year = ""; Notes = ""; ImagePageURL = "";
    if ((Nr < 0) || (Nr >= FGameNames->Count())) return result;

    S = FConfig->GameRecordBaseURL();
    if ((!S.isEmpty()) && (S[Length(S) - 1] != '/')) S = S + "/";
    T = (*FGameURLs)[Nr];
    if ((!T.isEmpty()) && (T[0] == '/')) T = Copy(T, 2, MaxInt);

    if (!DownloadTextFile(S + T, Lines, FLastListRequest)) return result;
    result = true;
    GetGameDataFromLines(Lines, FConfig->GameRecord(), Genre, Developer, Publisher, Year, Notes, ImagePageURL);
    return result;
}

static QString ExtDecodeName(const QString& S)
{
    QString result = "";
    TStringList* St = GetPlainText(S);
    for (int I = 0; I < St->Count(); I++) if (Trim((*St)[I]) == ",") result = result + ";"; else result = result + DecodeName((*St)[I]);
    delete St;
    return result;
}

void TMobyDataReader::GetGameDataFromLines(const QString& Lines, THTMLStructureElement* NextElement, QString& Genre, QString& Developer, QString& Publisher, QString& Year, QString& Notes, QString& ImagePageURL)
{
    TStringList *St, *St2;
    QString S, T;
    int I, J;
    QString S1, S2, S3, S4, S5;

    if (auto* L = dynamic_cast<TLink*>(NextElement)) {
        St2 = new TStringList;
        St = GetTagContents(Lines, 1, "a", L->AttributeKey(), L->AttributeValue(), "href", St2);
        if ((St->Count() != 0) && (ExtUpperCase(Copy((*St)[0], 1, 4)) == "<IMG")) {
            T = Trim(ExtUpperCase(L->HrefSubstring()));
            if (!T.isEmpty()) {
                S = "";
                for (I = 0; I < St2->Count(); I++) if (Pos(T, ExtUpperCase((*St2)[I])) != 0) { S = (*St2)[I]; break; }
            } else {
                S = (*St2)[0];
                if (L->Nr() != 0) {
                    I = L->Nr();
                    if ((I > 0) && (St->Count() >= I)) S = (*St2)[I - 1];
                    if ((I < 0) && (St->Count() >= -I)) S = (*St2)[NrIndex(St->Count(), I)];
                }
            }
            if (!S.isEmpty()) ImagePageURL = S;
        }
        delete St;
        delete St2;
        return;
    }

    if (auto* E = dynamic_cast<TElement*>(NextElement)) {
        St = GetTagContents(Lines, 1, E->ElementType(), E->AttributeKey(), E->AttributeValue(), E->ContainsText());
        // Note: the Delphi code passes ContainsText as RequestAttributeName (4th
        // optional parameter) – kept as in the original.
        if (St->Count() == 0) { delete St; return; }
        S = (*St)[0];
        if (E->Nr() != 0) {
            I = E->Nr();
            if ((I > 0) && (St->Count() >= I)) S = (*St)[I - 1];
            if ((I < 0) && (St->Count() >= -I)) S = (*St)[NrIndex(St->Count(), I)];
        }
        for (I = 0; I < E->ChildCount(); I++)
            GetGameDataFromLines(S, E->Children(I), Genre, Developer, Publisher, Year, Notes, ImagePageURL);
        delete St;
        return;
    }

    if (auto* D = dynamic_cast<TSearchGameData*>(NextElement)) {
        S1 = Trim(ExtUpperCase(D->Genre()));
        S2 = Trim(ExtUpperCase(D->Developer()));
        S3 = Trim(ExtUpperCase(D->Publisher()));
        S4 = Trim(ExtUpperCase(D->Year()));
        S5 = Trim(ExtUpperCase(D->Notes()));

        if ((!S5.isEmpty()) && (S5 != "SELF")) {
            St = GetPlainText(Lines, D->IgnoreTags(), D->StartTokens(), D->StoppTokens());
            Notes = Trim(St->Text());
            if ((!Notes.isEmpty()) && (Notes[Length(Notes) - 1] == '[')) Notes = Trim(Copy(Notes, 1, Length(Notes) - 1));
            delete St;
        }

        /* Simple mode (fallback) */

        St = GetPlainText(Lines);
        for (I = 0; I <= St->Count() - 2; I++) {
            S = ExtUpperCase((*St)[I]);
            if ((!S1.isEmpty()) && (Pos(S1, S) != 0)) Genre = DecodeName((*St)[I + 1]);
            if ((!S2.isEmpty()) && (Pos(S2, S) != 0)) Developer = DecodeName((*St)[I + 1]);
            if ((!S3.isEmpty()) && (Pos(S3, S) != 0)) Publisher = DecodeName((*St)[I + 1]);
            if ((!S4.isEmpty()) && (Pos(S4, S) != 0)) Year = DecodeName((*St)[I + 1]);
        }
        delete St;

        /* Semi simple mode */

        if (S1 == "SELF") Genre = GetPlainDecodedText(Lines);
        if (S2 == "SELF") Developer = GetPlainDecodedText(Lines);
        if (S3 == "SELF") Publisher = GetPlainDecodedText(Lines);
        if (S4 == "SELF") Year = GetPlainDecodedText(Lines);
        if (S5 == "SELF") Notes = GetPlainDecodedText(Lines);

        /* Complex mode for detecting multiple values */

        St = GetTagContents(Lines, 1, "div", "", "");
        if ((St->Count() == 1) && (Copy((*St)[0], 1, 1) == "<")) {
            St2 = GetTagContents((*St)[0], 1, "div", "", ""); delete St; St = St2;
        }
        for (I = 0; I <= St->Count() - 2; I++) {
            St2 = GetPlainText((*St)[I]);
            if (St2->Count() > 0) S = (*St2)[0];
            for (J = 1; J < St2->Count(); J++) S = S + " " + (*St2)[J];
            delete St2;
            S = ExtUpperCase(S);
            if ((!S1.isEmpty()) && (Pos(S1, S) != 0)) { T = ExtDecodeName((*St)[I + 1]); if (!T.isEmpty()) Genre = T; }
            if ((!S2.isEmpty()) && (Pos(S2, S) != 0)) { T = ExtDecodeName((*St)[I + 1]); if (!T.isEmpty()) Developer = T; }
            if ((!S3.isEmpty()) && (Pos(S3, S) != 0)) { T = ExtDecodeName((*St)[I + 1]); if (!T.isEmpty()) Publisher = T; }
            if ((!S4.isEmpty()) && (Pos(S4, S) != 0)) { T = ExtDecodeName((*St)[I + 1]); if (!T.isEmpty()) Year = T; }
            if ((!S5.isEmpty()) && (Pos(S5, S) != 0)) { T = ExtDecodeName((*St)[I + 1]); if (!T.isEmpty()) Notes = T; }
        }
        delete St;
    }
}

bool TMobyDataReader::GetImageURLAndNextURLsFromURL(const QString& CoverPageURL, TStringList*& ImageURLs, TStringList* PossibleNextURLs)
{
    QString Lines, S, T, LinesUpper, preURL;
    int I, J, CharsBefore;
    TStringList* St;
    bool JSMode;

    bool result = false;
    if (!DownloadTextFile(CoverPageURL, Lines)) return result;

    /* Scan for image URL in lines */
    JSMode = false;
    St = GetGameCoverFromLines(Lines, FConfig->CoverRecord(), JSMode);
    result = (St->Count() != 0);
    for (I = 0; I < St->Count(); I++) {
        T = Trim(ExtUpperCase((*St)[I]));
        if ((Copy(T, 1, 7) != QStringLiteral("HTTP:/") + "/") && (Copy(T, 1, 8) != QStringLiteral("HTTPS:/") + "/")) {
            T = (*St)[I];
            if ((!T.isEmpty()) && (T[0] == '/')) T = Copy(T, 2, MaxInt);
            ImageURLs->Add(S + T);   // S is still empty here (as in the Delphi code)
        } else {
            ImageURLs->Add((*St)[I]);
        }
    }
    delete St;

    /* Scan for other cover page URLs */
    if ((PossibleNextURLs != nullptr) && !JSMode) {
        S = CoverPageURL;
        if (!S.isEmpty()) S.chop(1);
        while ((!S.isEmpty()) && (S[Length(S) - 1] != '/')) S.chop(1);
        preURL = "";
        J = 0; for (I = 1; I <= Length(S); I++) if (S[I - 1] == '/') {
            J++; if (J == 3) { preURL = Copy(S, 1, I); S = Copy(S, I + 1, MaxInt); break; }
        }
        S = ExtUpperCase(S);
        if (S.isEmpty()) return result;

        LinesUpper = ExtUpperCase(Lines);
        CharsBefore = 0;

        I = Pos(S, LinesUpper); while (I > 0) {
            T = Copy(LinesUpper, I, MaxInt);
            J = Pos("\"", T); if (J == 0) break;
            PossibleNextURLs->Add(preURL + Copy(Lines, CharsBefore + I, J - 1));

            LinesUpper = Copy(LinesUpper, (I - 1) + J + 1, MaxInt);
            CharsBefore = CharsBefore + (I - 1) + J;

            I = Pos(S, LinesUpper);
        }
    }
    return result;
}

void TMobyDataReader::GetGameCoverFromSinglePages(const QString& InitialURL, TStringList* ImageURLs, int MaxImages)
{
    QString S;
    TStringList *WaitingURLs, *ScannedURLsUpper, *PossibleNextURLs, *St, *NewImageURLs;
    int I, J;
    bool B;

    WaitingURLs = new TStringList;
    /* Get initial cover page URL */
    WaitingURLs->Add(InitialURL);

    ScannedURLsUpper = new TStringList;
    PossibleNextURLs = new TStringList;
    /* While there are URLs in the queue */
    while (WaitingURLs->Count() > 0) {
        PossibleNextURLs->Clear();

        /* Get next image URL and scan for more cover page URLs */
        if (MaxImages == 1) St = nullptr; else St = PossibleNextURLs;
        NewImageURLs = new TStringList;
        if (GetImageURLAndNextURLsFromURL((*WaitingURLs)[0], NewImageURLs, St)) {
            /* Save new image URL */
            for (I = 0; I < NewImageURLs->Count(); I++) {
                B = true; for (J = 0; J < ImageURLs->Count(); J++) if (ExtUpperCase((*ImageURLs)[J]) == ExtUpperCase((*NewImageURLs)[I])) { B = false; break; }
                if (B) ImageURLs->Add((*NewImageURLs)[I]);
            }

            /* Remove cover page URL from queue and mark it as scanned */
            ScannedURLsUpper->Add(ExtUpperCase((*WaitingURLs)[0]));
            WaitingURLs->Delete(0);

            /* Check possible new cover page URLs for real new URLs */
            for (I = 0; I < PossibleNextURLs->Count(); I++) {
                S = ExtUpperCase((*PossibleNextURLs)[I]);
                if (ScannedURLsUpper->IndexOf(S) < 0) WaitingURLs->Add((*PossibleNextURLs)[I]);
            }
        } else {
            WaitingURLs->Delete(0);
        }
        delete NewImageURLs;

        /* Stop after max. MaxImages images */
        if (ImageURLs->Count() >= MaxImages) break;
    }
    delete ScannedURLsUpper;
    delete PossibleNextURLs;
    delete WaitingURLs;
}

class TDownloadDataThread : public QThread
{
public:
    TDownloadDataThread(const QString& AImageBaseURL, TMobyDataReader* ADataReader, TStringList* AURLList);
    ~TDownloadDataThread() override;
    // properties
    TStringList* ImageURLs() const { return FImageURLs; }
protected:
    void run() override { Execute(); }
    void Execute();
private:
    QString FImageBaseURL;
    TMobyDataReader* FDataReader = nullptr;
    TStringList* FURList = nullptr;
    TStringList* FImageURLs = nullptr;
};

TDownloadDataThread::TDownloadDataThread(const QString& AImageBaseURL, TMobyDataReader* ADataReader, TStringList* AURLList)
{
    FImageBaseURL = AImageBaseURL;
    FDataReader = ADataReader;
    FURList = AURLList;
    FImageURLs = new TStringList;
    start();   // Resume
}

TDownloadDataThread::~TDownloadDataThread()
{
    wait();
    delete FImageURLs;
}

void TDownloadDataThread::Execute()
{
    TStringList* NewImageURLs;
    int I, J;

    for (I = 0; I < FURList->Count(); I++) {
        NewImageURLs = new TStringList;
        /* Get next image URL */
        if (!FDataReader->GetImageURLAndNextURLsFromURL((*FURList)[I], NewImageURLs, nullptr)) { delete NewImageURLs; continue; }

        /* Check if image URL is relative to ImageBaseURL and make is absolute if needed */
        for (J = 0; J < NewImageURLs->Count(); J++) {
            if (ExtUpperCase(Trim(Copy((*NewImageURLs)[J], 1, 4))) != "HTTP") {
                if (FImageBaseURL.isEmpty() || FImageBaseURL[Length(FImageBaseURL) - 1] != '\\') (*NewImageURLs)[J] = FImageBaseURL + "/" + (*NewImageURLs)[J]; else (*NewImageURLs)[J] = FImageBaseURL + (*NewImageURLs)[J];
            }
        }

        FImageURLs->AddStrings(*NewImageURLs);
        delete NewImageURLs;
    }
}

void TMobyDataReader::ProcessLinks(const QString& BaseURL, TStringList* Links, const QString& ImageBaseURL, TStringList* ImageURLs, int MaxImages)
{
    const int ThreadCount = 8;
    int I, J, K;
    TStringList* ImagePages;
    QString S, NewImageURL;
    bool B;
    TStringList* ThreadURLList[ThreadCount];
    TDownloadDataThread* ThreadList[ThreadCount];

    ImagePages = new TStringList;
    for (I = 0; I < Links->Count(); I++) {
        if (ImageURLs->Count() + ImagePages->Count() >= MaxImages) break;
        S = (*Links)[I];

        /* Is this an URL to a real image? */
        if ((ExtUpperCase(Copy(S, 1, Length(BaseURL))) != ExtUpperCase(BaseURL)) || (Length(S) == Length(BaseURL))) continue;

        ImagePages->Add(S);
    }

    for (I = 0; I < ThreadCount; I++) ThreadURLList[I] = new TStringList;
    for (I = 0; I < ImagePages->Count(); I++) ThreadURLList[I % ThreadCount]->Add((*ImagePages)[I]);

    for (I = 0; I < ThreadCount; I++) if (ThreadURLList[I]->Count() == 0) ThreadList[I] = nullptr; else ThreadList[I] = new TDownloadDataThread(ImageBaseURL, this, ThreadURLList[I]);

    do {
        QThread::msleep(50);   // Sleep(50)
        B = true;
        for (I = 0; I < ThreadCount; I++) if ((ThreadList[I] != nullptr) && !ThreadList[I]->isFinished()) { B = false; break; }
    } while (!B);

    /* Save new image URLs */
    for (I = 0; I < ThreadCount; I++) if (ThreadList[I] != nullptr) for (J = 0; J < ThreadList[I]->ImageURLs()->Count(); J++) {
        NewImageURL = (*ThreadList[I]->ImageURLs())[J];
        B = true; for (K = 0; K < ImageURLs->Count(); K++) if (ExtUpperCase((*ImageURLs)[K]) == ExtUpperCase(NewImageURL)) { B = false; break; }
        if (B) ImageURLs->Add(NewImageURL);
    }

    for (I = 0; I < ThreadCount; I++) if (ThreadList[I] != nullptr) delete ThreadList[I];
    for (I = 0; I < ThreadCount; I++) delete ThreadURLList[I];
    delete ImagePages;
}

void TMobyDataReader::GetGameCoverFromMediaList(const QString& ListPageURL, TStringList* ImageURLs, int MaxImages)
{
    QString Lines, ScreensahotsURL;
    int I;
    TStringList *Tags, *href;

    if (!DownloadTextFile(ListPageURL, Lines)) return;

    href = new TStringList;
    /* Get all links, find link to screenshots page */
    ScreensahotsURL = "";
    Tags = GetTagContents(Lines, 1, "a", "", "", "href", href);
    for (I = 0; I < href->Count(); I++) {
        (*href)[I] = Config()->CoverRecordBaseURL() + (*href)[I];
        if (Trim(ExtUpperCase((*Tags)[I])) == "SCREENSHOTS") ScreensahotsURL = (*href)[I];
    }
    delete Tags;

    /* Process image file links */
    ProcessLinks(ListPageURL, href, Config()->CoverRecordBaseURL(), ImageURLs, MaxImages);
    delete href;

    if (!ScreensahotsURL.isEmpty()) {
        if (ScreensahotsURL[Length(ScreensahotsURL) - 1] != '/') ScreensahotsURL = ScreensahotsURL + "/";
        if (!DownloadTextFile(ScreensahotsURL, Lines)) return;
        href = new TStringList;
        Tags = GetTagContents(Lines, 1, "a", "", "", "href", href);
        delete Tags;
        for (I = 0; I < href->Count(); I++) (*href)[I] = Config()->CoverRecordBaseURL() + (*href)[I];
        ProcessLinks(ScreensahotsURL, href, Config()->CoverRecordBaseURL(), ImageURLs, MaxImages);
        delete href;
    }
}

bool TMobyDataReader::GetGameCover(const QString& CoverPageURL, QString& ImageURL, int MaxImages)
{
    QString URL, S, T;
    TStringList* ImageURLs;
    int I;
    bool result;

    ImageURL = "";
    if (Trim(CoverPageURL).isEmpty()) { result = true; return result; }

    ImageURLs = new TStringList;
    /* Get initial cover page URL */
    URL = FConfig->CoverRecordBaseURL();
    if ((!URL.isEmpty()) && (URL[Length(URL) - 1] != '/')) URL = URL + "/";
    S = CoverPageURL;
    if ((!S.isEmpty()) && (S[0] == '/')) S = Copy(S, 2, MaxInt);
    URL = URL + S;

    /* Get initial cover und folder links to more images */
    GetGameCoverFromSinglePages(URL, ImageURLs, MaxImages);

    if ((MaxImages > 1) && PrgSetup->DataReaderDownloadCompleteMediaLibrary()) {
        /* Get media list URL */
        if ((!URL.isEmpty()) && (URL[Length(URL) - 1] == '/')) URL = Copy(URL, 1, Length(URL) - 1);
        while ((!URL.isEmpty()) && (URL[Length(URL) - 1] != '/')) URL = Copy(URL, 1, Length(URL) - 1);

        /* Get complete media list */
        GetGameCoverFromMediaList(URL, ImageURLs, MaxImages);
    }

    /* Fix URLs with missing absolute part */
    for (I = 0; I < ImageURLs->Count(); I++) {
        S = (*ImageURLs)[I];
        if (LowerCase(Copy(S, 1, 4)) != "http") {
            if (Copy(S, 1, 1) == "/") S = Copy(S, 2, MaxInt);
            T = FConfig->CoverRecordBaseURL();
            if (T.isEmpty() || T[Length(T) - 1] != '/') T = T + "/";
            S = T + S;
            (*ImageURLs)[I] = S;
        }
    }

    /* Combine image URLs */
    if (ImageURLs->Count() > 0) ImageURL = (*ImageURLs)[0];
    for (I = 1; I < ImageURLs->Count(); I++) ImageURL = ImageURL + "$" + (*ImageURLs)[I];
    result = (ImageURLs->Count() > 0);
    delete ImageURLs;
    return result;
}

TStringList* TMobyDataReader::GetGameCoverFromLines(const QString& Lines, THTMLStructureElement* NextElement, bool& JSMode)
{
    TStringList *St, *St2;
    int I;
    QString S;

    TStringList* result = new TStringList;

    if (auto* E = dynamic_cast<TElement*>(NextElement)) {
        St = GetTagContents(Lines, 1, E->ElementType(), E->AttributeKey(), E->AttributeValue());
        if (St->Count() == 0) { delete St; return result; }
        S = (*St)[0];
        if (E->Nr() != 0) {
            I = E->Nr();
            if ((I > 0) && (St->Count() >= I)) S = (*St)[I - 1];
            if ((I < 0) && (St->Count() >= -I)) S = (*St)[NrIndex(St->Count(), I)];
        }
        St2 = GetGameCoverFromLines(S, E->Child(), JSMode);
        result->AddStrings(*St2);
        delete St2;
        delete St;
        return result;
    }

    if (auto* J = dynamic_cast<TJavascript*>(NextElement)) {
        JSMode = true;
        St = GetJavascriptURLs(Lines, J->ContainsText());
        result->AddStrings(*St);
        delete St;
        return result;
    }

    if (auto* Img = dynamic_cast<TImg*>(NextElement)) {
        // Delphi casts to TLink here (same field layout); the TImg accessors are used.
        St2 = new TStringList;
        St = GetTagContents(Lines, 1, "img", Img->AttributeKey(), Img->AttributeValue(), "src", St2);
        if (St->Count() != 0) {
            S = (*St2)[0];
            if (Img->Nr() != 0) {
                I = Img->Nr();
                if ((I > 0) && (St->Count() >= I)) S = (*St2)[I - 1];
                if ((I < 0) && (St->Count() >= -I)) S = (*St2)[NrIndex(St->Count(), I)];
            }
            if (!S.isEmpty()) result->Add(S);
        }
        delete St;
        delete St2;
        return result;
    }
    return result;
}

/* TDataReaderThread */

TDataReaderThread::TDataReaderThread(TDataReader* ADataReader)
{
    FDataReader = ADataReader;
    FSuccess = false;
}

TDataReaderThread::~TDataReaderThread()
{
    wait();   // TThread.Destroy waits for the thread
}

/* TDataReaderLoadConfigThread */

TDataReaderLoadConfigThread::TDataReaderLoadConfigThread(TDataReader* ADataReader, bool AForceUpdate) : TDataReaderThread(ADataReader)
{
    FForceUpdate = AForceUpdate;
    start();   // Resume
}

void TDataReaderLoadConfigThread::Execute()
{
    bool DoUpdateCheck;
    // CoInitialize(nil) is not needed (no COM/MSXML)
    if (FForceUpdate) {
        DoUpdateCheck = true;
    } else {
        DoUpdateCheck = false;
        switch (PrgSetup->DataReaderCheckForUpdates()) {
            case 0: DoUpdateCheck = false; break;
            case 1: DoUpdateCheck = (Round(std::trunc(Date())) >= PrgSetup->LastDataReaderUpdateCheck() + 7); break;
            case 2: DoUpdateCheck = (Round(std::trunc(Date())) >= PrgSetup->LastDataReaderUpdateCheck() + 1); break;
            case 3: DoUpdateCheck = true; break;
        }
    }
    if (FDataReader->LoadConfig(PrgDataDir() + SettingsFolder + PathDelim + DataReaderConfigFile, DoUpdateCheck)) FSuccess = FDataReader->LastUpdateCheckOK(); else FSuccess = false;
    if (!FSuccess) FSuccess = FDataReader->LoadConfig(PrgDataDir() + SettingsFolder + PathDelim + DataReaderConfigFile, false);
    if (DoUpdateCheck && FSuccess) PrgSetup->SetLastDataReaderUpdateCheck(Round(std::trunc(Date())));
}

/* TDataReaderGameListThread */

TDataReaderGameListThread::TDataReaderGameListThread(TDataReader* ADataReader, const QString& AName) : TDataReaderThread(ADataReader)
{
    FName = AName;
    start();   // Resume
}

void TDataReaderGameListThread::Execute()
{
    FSuccess = FDataReader->ReadList(FName);
}

/* TDataReaderGameDataThread */

TDataReaderGameDataThread::TDataReaderGameDataThread(TDataReader* ADataReader, int ANr, bool AFullImages) : TDataReaderThread(ADataReader)
{
    FNr = ANr;
    FFullImages = AFullImages;
    FGenre = ""; FDeveloper = ""; FPublisher = ""; FYear = ""; FImageURL = ""; FNotes = "";
    start();   // Resume
}

void TDataReaderGameDataThread::Execute()
{
    QString S;
    int MaxImageCount;
    FSuccess = FDataReader->GetGameData(FNr, FGenre, FDeveloper, FPublisher, FYear, FNotes, S);
    if (FFullImages) MaxImageCount = PrgSetup->DataReaderMaxImages(); else MaxImageCount = 1;
    if (FSuccess) FSuccess = FDataReader->GetGameCover(S, FImageURL, MaxImageCount);
}

/* TDataReaderGameCoverThread */

TDataReaderGameCoverThread::TDataReaderGameCoverThread(TDataReader* ADataReader, const QString& ADownloadURL, const QString& ADestFolder) : TDataReaderThread(ADataReader)
{
    FDownloadURLs = new TStringList;
    FDownloadURLs->Add(ADownloadURL);
    FDestFolder = ADestFolder;
    start();   // Resume
}

TDataReaderGameCoverThread::TDataReaderGameCoverThread(TDataReader* ADataReader, TStringList* ADownloadURLs, const QString& ADestFolder) : TDataReaderThread(ADataReader)
{
    FDownloadURLs = new TStringList;
    FDownloadURLs->AddStrings(*ADownloadURLs);
    FDestFolder = ADestFolder;
    start();   // Resume
}

TDataReaderGameCoverThread::~TDataReaderGameCoverThread()
{
    wait();
    delete FDownloadURLs;
}

void TDataReaderGameCoverThread::Execute()
{
    QString FileName, Path, S;
    int I, J;

    FSuccess = true;

    for (J = 0; J < FDownloadURLs->Count(); J++) {
        FileName = (*FDownloadURLs)[J];

        I = Pos("/", FileName); while (I > 0) { FileName = Copy(FileName, I + 1, MaxInt); I = Pos("/", FileName); }
        Path = IncludeTrailingPathDelimiter(FDestFolder);
        if (FileExists(Path + FileName)) continue;

        I = Pos("/covers/small/", (*FDownloadURLs)[J]);
        if (I != 0) {
            S = Copy((*FDownloadURLs)[J], 1, I - 1) + "/covers/large/" + Copy((*FDownloadURLs)[J], I + Length("/covers/small/"), MaxInt);
            if (DownloadFileToDisk(S, Path + FileName)) continue;
        }

        if (!DownloadFileToDisk((*FDownloadURLs)[J], Path + FileName)) FSuccess = false;
    }
}
