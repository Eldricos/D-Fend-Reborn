// C++ port of GameDBFilterUnit.pas (implementation section).
#include "GameDBFilterUnit.h"
#include "CommonTools.h"
#include "GameDBFilterToolsUnit.h"
#include "LanguageSetupUnit.h"

/* TFilterBase */

TFilterBase::TFilterBase(TStringList* ASt, bool ANotPrefix)
{
    Q_UNUSED(ASt);
    FNotPrefix = ANotPrefix;
    // ProcessStringList(ASt) is called by the derived constructors
}

/* TFilterElement */

TFilterElement::TFilterElement(TStringList* ASt, bool ANotPrefix) : TFilterBase(ASt, ANotPrefix)
{
    ProcessStringList(ASt);
}

void TFilterElement::ProcessStringList(TStringList* ASt)
{
    if (ASt == nullptr) throw Exception("Access violation");   // Delphi: AV, caught by SetFilterString

    if (ASt->Count() == 3) {
        const QString &S0 = (*ASt)[0];
        FFilterType = ftUserData;
        if ((S0 == ExtUpperCase(LanguageSetup->GameName())) || (S0 == "NAME")) FFilterType = ftName;
        if ((S0 == ExtUpperCase(LanguageSetup->GameGenre())) || (S0 == "GENRE")) FFilterType = ftGenre;
        if ((S0 == ExtUpperCase(LanguageSetup->GameDeveloper())) || (S0 == "DEVELOPER")) FFilterType = ftDeveloper;
        if ((S0 == ExtUpperCase(LanguageSetup->GamePublisher())) || (S0 == "PUBLISHER")) FFilterType = ftPublisher;
        if ((S0 == ExtUpperCase(LanguageSetup->GameYear())) || (S0 == "YEAR")) FFilterType = ftYear;
        if ((S0 == ExtUpperCase(LanguageSetup->GameNotes())) || (S0 == "NOTES")) FFilterType = ftNotes;
        if ((S0 == ExtUpperCase(LanguageSetup->GameWWW())) || (S0 == "WWW")) FFilterType = ftWWW;
        if ((S0 == ExtUpperCase(LanguageSetup->GameLanguage())) || (S0 == "LANGUAGE")) FFilterType = ftLanguage;
        if ((S0 == ExtUpperCase(LanguageSetup->GameLicense())) || (S0 == "LICENSE")) FFilterType = ftLicense;
        if (FFilterType == ftUserData) FUserKeyUpper = S0;

        const QString &S1 = (*ASt)[1];
        FCompareType = ftEqual;
        if ((S1 == "<>") || (S1 == "!=") || (S1 == "=!")) FCompareType = ftNotEqual;
        if (S1 == ">") FCompareType = ftBigger;
        if (S1 == "<") FCompareType = ftSmaller;
        if ((S1 == ">=") || (S1 == "=>")) FCompareType = ftBiggerOrEqual;
        if ((S1 == "<=") || (S1 == "=<")) FCompareType = ftSmallerOrEqual;
    } else {
        FFilterType = ftAll;
        FCompareType = ftEqual;
    }
    if (ASt->Count() > 0) FFilterValueUpper = (*ASt)[ASt->Count() - 1]; else FFilterValueUpper = "";
    FFilterValueNumberOk = TryStrToInt(FFilterValueUpper, FFilterValueNumber);
}

bool TFilterElement::Compare(const QString& GameValue, TSpecialProcessingType ProcessingType)
{
    auto Contains = [this](const QString &S) -> bool { return (Pos(FFilterValueUpper, S) > 0); };
    int GameValueNumber = 0;
    bool B;
    QString S;
    bool result;

    if (FFilterValueUpper == "") return true;

    if ((!FFilterValueNumberOk) && (ProcessingType == sptNumber)) ProcessingType = sptNone;
    if (ProcessingType == sptNumber) {
        B = TryStrToInt(GameValue, GameValueNumber);
        if (!B) ProcessingType = sptNone;
    }

    if (ProcessingType == sptNumber) {
        switch (FCompareType) {
            case ftEqual: result = (FFilterValueNumber == GameValueNumber); break;
            case ftNotEqual: result = (FFilterValueNumber != GameValueNumber); break;
            case ftBigger: result = (FFilterValueNumber < GameValueNumber); break;
            case ftSmaller: result = (FFilterValueNumber > GameValueNumber); break;
            case ftBiggerOrEqual: result = (FFilterValueNumber <= GameValueNumber); break;
            case ftSmallerOrEqual: result = (FFilterValueNumber >= GameValueNumber); break;
            default: result = (FFilterValueNumber == GameValueNumber); break;
        }
        return result;
    }

    if (ProcessingType != sptNone) {
        switch (ProcessingType) {
            case sptGenre: S = GetCustomGenreName(GameValue); break;
            case sptLanguage: S = GetCustomLanguageName(GameValue); break;
            case sptLicense: S = GetCustomLicenseName(GameValue); break;
            default: S = GetCustomGenreName(GameValue); break;
        }
        switch (FCompareType) {
            case ftEqual: case ftBiggerOrEqual: case ftSmallerOrEqual: result = Contains(ExtUpperCase(GameValue)) || Contains(ExtUpperCase(S)); break;
            case ftNotEqual: case ftBigger: case ftSmaller: result = !(Contains(ExtUpperCase(GameValue)) || Contains(ExtUpperCase(S))); break;
            default: result = Contains(ExtUpperCase(GameValue)) || Contains(ExtUpperCase(S)); break;
        }
        return result;
    }

    /* if ProcessingType=sptNone: GameValue already upper cased by caller */
    switch (FCompareType) {
        case ftEqual: case ftBiggerOrEqual: case ftSmallerOrEqual: result = Contains(GameValue); break;
        case ftNotEqual: case ftBigger: case ftSmaller: result = !Contains(GameValue); break;
        default: result = Contains(GameValue); break;
    }
    return result;
}

QString TFilterElement::GetUserData(TGame* AGame)
{
    QString result = "";
    TStringList *St = StringToStringList(AGame->UserInfo());
    for (int I = 0; I < St->Count(); I++) if (ExtUpperCase(Copy(Trim((*St)[I]), 1, 8)) == FUserKeyUpper + "=") { result = Copy(Trim((*St)[I]), Length(FUserKeyUpper) + 2, MaxInt); break; }
    delete St;
    return result;
}

bool TFilterElement::FilterOk(TGame* AGame)
{
    bool result;
    switch (FFilterType) {
        case ftAll:
            result = !NotPrefix();
            if (Compare(AGame->CacheNameUpper)) return result;
            if (Compare(AGame->CacheGenre, sptGenre)) return result;
            if (Compare(AGame->CacheDeveloperUpper)) return result;
            if (Compare(AGame->CachePublisherUpper)) return result;
            if (Compare(AGame->CacheYearUpper, sptNumber)) return result;
            if (Compare(AGame->CacheLanguage, sptLanguage)) return result;
            if (Compare(AGame->License(), sptLicense)) return result;
            result = NotPrefix();
            break;
        case ftName: result = NotPrefix() ^ Compare(AGame->CacheNameUpper); break;
        case ftGenre: result = NotPrefix() ^ Compare(AGame->CacheGenre, sptGenre); break;
        case ftDeveloper: result = NotPrefix() ^ Compare(AGame->CacheDeveloperUpper); break;
        case ftPublisher: result = NotPrefix() ^ Compare(AGame->CachePublisherUpper); break;
        case ftYear: result = NotPrefix() ^ Compare(AGame->CacheYearUpper, sptNumber); break;
        case ftNotes: {
            TStringList *St = StringToStringList(AGame->Notes());
            result = NotPrefix() ^ Compare(St->Text());
            delete St;
            break;
        }
        case ftWWW: {
            bool B = false;
            for (int I = 1; I <= 9; I++) if (Compare(ExtUpperCase(AGame->WWW(I)))) { B = true; break; }
            result = NotPrefix() ^ B;
            break;
        }
        case ftLanguage: result = NotPrefix() ^ Compare(AGame->CacheLanguage, sptLanguage); break;
        case ftLicense: result = NotPrefix() ^ Compare(AGame->License(), sptLicense); break;
        case ftUserData: result = NotPrefix() ^ Compare(ExtUpperCase(GetUserData(AGame))); break;
        default: result = NotPrefix() ^ Compare(AGame->CacheNameUpper); break;
    }
    return result;
}

TStringList* TFilterElement::Print(const QString& Indent)
{
    QString S = "\"" + IntToStr(int(FFilterType)) + "\", \"" + FFilterValueUpper + "\", \"" + IntToStr(int(FCompareType)) + "\"";
    if (NotPrefix()) S = "\"not\", " + S;
    TStringList *result = new TStringList;
    result->Add(Indent + "Element (" + S + ")");
    return result;
}

/* TFilterList */

TFilterList::TFilterList(TStringList* ASt, bool ANotPrefix) : TFilterBase(ASt, ANotPrefix)
{
    try {
        ProcessStringList(ASt);
    } catch (...) {
        // Delphi calls the destructor if the constructor raises an exception
        if (FList != nullptr) {
            for (int I = 0; I < FList->size(); I++) delete static_cast<TFilterBase *>((*FList)[I]);
            delete FList; FList = nullptr;
        }
        throw;
    }
}

void TFilterList::ProcessStringList(TStringList* ASt)
{
    TFilterList *FilterList;
    TFilterElement *FilterElement;

    FList = new TList;

    if (ASt == nullptr) throw Exception("Access violation");   // Delphi: AV, caught by SetFilterString

    if (ASt->Count() < 2) return;
    if (((*ASt)[0] != "AND") && ((*ASt)[0] != "OR")) return;
    if ((*ASt)[0] == "AND") FFilterListType = fltAnd; else FFilterListType = fltOr;

    for (int I = 1; I < ASt->Count(); I++) {
        if (((*ASt)[I] == "()") || ((*ASt)[I] == "!()")) {
            FilterList = new TFilterList(static_cast<TStringList *>(ASt->Objects(I)), (*ASt)[I] == "!()");
            FList->append(FilterList);
        } else {
            FilterElement = new TFilterElement(static_cast<TStringList *>(ASt->Objects(I)), (*ASt)[I] == "!");
            FList->append(FilterElement);
        }
    }
}

TFilterList::~TFilterList()
{
    if (FList != nullptr) {
        for (int I = 0; I < FList->size(); I++) delete static_cast<TFilterBase *>((*FList)[I]);
        delete FList;
    }
}

bool TFilterList::FilterOk(TGame* AGame)
{
    bool result;
    if (FFilterListType == fltOr) {
        result = !NotPrefix();
        for (int I = 0; I < FList->size(); I++) if (static_cast<TFilterBase *>((*FList)[I])->FilterOk(AGame)) return result;
        result = NotPrefix();
    } else {
        result = NotPrefix();
        for (int I = 0; I < FList->size(); I++) if (!static_cast<TFilterBase *>((*FList)[I])->FilterOk(AGame)) return result;
        result = !NotPrefix();
    }
    return result;
}

TStringList* TFilterList::Print(const QString& Indent)
{
    QString S;
    TStringList *result = new TStringList;
    if (FFilterListType == fltOr) S = "\"or\""; else S = "\"and\"";
    if (NotPrefix()) S = "\"not\", " + S;
    result->Add(Indent + "List (" + S + ")");
    for (int I = 0; I < FList->size(); I++) {
        TStringList *St = static_cast<TFilterBase *>((*FList)[I])->Print(Indent + "  ");
        result->AddStrings(*St);
        delete St;
    }
    return result;
}

/* TGamesListFilter */

TGamesListFilter::TGamesListFilter()
{
    FFilter = nullptr;
}

TGamesListFilter::~TGamesListFilter()
{
    if (FFilter != nullptr) delete FFilter;
}

bool TGamesListFilter::FilterOk(TGame* AGame)
{
    if (FFilter != nullptr) return FFilter->FilterOk(AGame); else return true;
}

void TGamesListFilter::SetFilterString(const QString& S)
{
    TStringList *St1, *St2;
    TLanguageLocalOperatorNames OperatorNames;

    if (FFilter != nullptr) { delete FFilter; FFilter = nullptr; }

    OperatorNames.NameAnd = LanguageSetup->SearchAnd();
    OperatorNames.NameOr = LanguageSetup->SearchOr();
    OperatorNames.NameNot = LanguageSetup->SearchNot();

    St1 = nullptr; St2 = nullptr;
    try {
        St1 = SplitSearchString(S);
        St2 = ProcessSearchElements(St1, OperatorNames); FreeNestedStringList(St1); St1 = nullptr;
        St1 = ProcessNotOperators(St2); FreeNestedStringList(St2); St2 = nullptr;
        St2 = ProcessSearchOperators1(St1); FreeNestedStringList(St1); St1 = nullptr;
        St1 = ProcessSearchOperators2(St2); FreeNestedStringList(St2); St2 = nullptr;
        St2 = ProcessQuotes(St1); FreeNestedStringList(St1); St1 = nullptr;
        FFilter = new TFilterList(St2, false);
        if (St1 != nullptr) FreeNestedStringList(St1);
        if (St2 != nullptr) FreeNestedStringList(St2);
    } catch (...) {
        if (St1 != nullptr) FreeNestedStringList(St1);
        if (St2 != nullptr) FreeNestedStringList(St2);
        if (FFilter != nullptr) { delete FFilter; FFilter = nullptr; }
        TStringList Sub1;
        Sub1.Add(S);
        TStringList Top;
        Top.AddObject("", &Sub1);
        FFilter = new TFilterList(&Top, false);
    }
}
