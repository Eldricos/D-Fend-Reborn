// C++ port of GameDBFilterToolsUnit.pas (implementation section).
#include "GameDBFilterToolsUnit.h"
#include "CommonTools.h"

static inline TStringList *Sub(const TStringList *St, int I) { return static_cast<TStringList *>(St->Objects(I)); }

static void AddNestedStrings(TStringList *St, TStringList *ListToAdd, int StartNr, int EndNr)
{
    for (int I = StartNr; I <= EndNr; I++) {
        if (ListToAdd->Objects(I) == nullptr) St->Add((*ListToAdd)[I]); else {
            TStringList *St2 = new TStringList;
            AddNestedStrings(St2, Sub(ListToAdd, I), 0, Sub(ListToAdd, I)->Count() - 1);
            St->AddObject((*ListToAdd)[I], St2);
        }
    }
}

static void AddNestedStrings(TStringList *St, TStringList *ListToAdd)
{
    AddNestedStrings(St, ListToAdd, 0, ListToAdd->Count() - 1);
}

static void AddNestedString(TStringList *St, TStringList *ListToAdd, int Nr)
{
    AddNestedStrings(St, ListToAdd, Nr, Nr);
}

static const char *const TwoLetterOps[8] = {"==", "<>", "!=", "=!", "<=", ">=", "=<", "=>"};
static const char OneLetterOps[3] = {'=', '<', '>'};

TStringList* SplitSearchString(const QString& S_)
{
    int I, J, Start, QuoteEnd, Count;
    bool Found, InText;
    QChar TextStart;

    QString S = Trim(ExtUpperCase(S_));
    auto C = [&S](int Index) -> QChar { return S[Index - 1]; };   // S[Index] (1-based)

    TStringList *result = new TStringList;
    if (Length(S) == 0) return result;

    Start = 1;
    I = 1;
    while (I <= Length(S)) {

        /* Quoted text */
        if ((C(I) == '"') || (C(I) == '\'')) {
            if (Start < I) { result->Add(Trim(Copy(S, Start, (I - 1) - Start + 1))); }
            QuoteEnd = Length(S);
            for (J = I + 1; J <= Length(S); J++) if (C(J) == C(I)) { QuoteEnd = J - 1; break; }
            result->Add("\"" + Trim(Copy(S, I + 1, QuoteEnd - (I + 1) + 1)) + "\"");
            I = QuoteEnd + 2; Start = I; continue;
        }

        /* Two letter symbols */
        if (I + 1 <= Length(S)) {
            Found = false;
            for (J = 0; J < 8; J++) if ((C(I) == QLatin1Char(TwoLetterOps[J][0])) && (C(I + 1) == QLatin1Char(TwoLetterOps[J][1]))) {
                if (Start < I) { result->Add(Trim(Copy(S, Start, (I - 1) - Start + 1))); }
                result->Add(QString::fromLatin1(TwoLetterOps[J])); I += 2; Start = I; Found = true; break;
            }
            if (Found) continue;
        }

        /* One letter symbols */
        Found = false;
        for (J = 0; J < 3; J++) if (C(I) == QLatin1Char(OneLetterOps[J])) {
            if (Start < I) { result->Add(Trim(Copy(S, Start, (I - 1) - Start + 1))); }
            result->Add(QString(QLatin1Char(OneLetterOps[J]))); I++; Start = I; Found = true; break;
        }
        if (Found) continue;

        /* "!" and "-" symbol; only if not in plain text string */
        if ((Start == I) && ((C(I) == '!') || (C(I) == '-'))) {
            result->Add(QString(C(I))); I++; Start = I; continue;
        }

        /* Sub elements */
        if ((C(I) == '(') || (C(I) == '[') || (C(I) == '{')) {
            if (Start < I) { result->Add(Trim(Copy(S, Start, (I - 1) - Start + 1))); }
            InText = false; TextStart = '"'; Count = 1;
            for (J = I + 1; J <= Length(S); J++) {
                if (!InText) {
                    if ((C(J) == '(') || (C(J) == '[') || (C(J) == '{')) Count++;
                    if ((C(J) == ')') || (C(J) == ']') || (C(J) == '}')) Count--;
                    if ((C(J) == '"') || (C(J) == '\'')) { InText = true; TextStart = C(J); }
                } else {
                    if (C(J) == TextStart) InText = false;
                }
                if (Count == 0) {
                    result->AddObject("()", SplitSearchString(Copy(S, I + 1, (J - 1) - (I + 1) + 1)));
                    I = J + 1; Found = true; break;
                }
            }
            if (!Found) {
                result->AddObject("()", SplitSearchString(Copy(S, I + 1, MaxInt)));
                I = Length(S) + 1;
            }
            Start = I; continue;
        }

        /* Spaces */
        if (C(I) == ' ') {
            if (Start < I) { result->Add(Trim(Copy(S, Start, (I - 1) - Start + 1))); }
            I++; Start = I; continue;
        }

        /* Plain text */
        I++;
    }

    /* Add last plain string */
    if (Start < Length(S) + 1) result->Add(Trim(Copy(S, Start, MaxInt)));
    return result;
}

TStringList* ProcessSearchElements(TStringList* St, const TLanguageLocalOperatorNames& OperatorNames)
{
    int I, J, Position;
    bool Found;

    TStringList *result = new TStringList;
    TStringList Temp;

    auto AddTemp = [&]() {
        if (Temp.Count() == 0) return;
        TStringList *St2 = new TStringList;
        St2->AddStrings(Temp);
        result->AddObject("", St2);
        Temp.Clear();
    };

    if (St->Count() == 1) {
        if (St->Objects(0) == nullptr) { Temp.Add((*St)[0]); AddTemp(); } else { delete result; result = ProcessSearchElements(Sub(St, 0), OperatorNames); }
        return result;
    }

    Position = 0;
    for (I = 0; I < St->Count(); I++) {
        const QString &SI = (*St)[I];
        /* Logical ops */
        if ((SI == "AND") || (SI == "OR")) { AddTemp(); result->Add(SI); Position = 0; continue; }
        if ((OperatorNames.NameAnd != "") && (SI == ExtUpperCase(OperatorNames.NameAnd))) { AddTemp(); result->Add("AND"); Position = 0; continue; }
        if ((OperatorNames.NameOr != "") && (SI == ExtUpperCase(OperatorNames.NameOr))) { AddTemp(); result->Add("OR"); Position = 0; continue; }
        if ((OperatorNames.NameNot != "") && (SI == ExtUpperCase(OperatorNames.NameNot))) { AddTemp(); result->Add("!"); Position = 0; continue; }
        if ((SI == "!") || (SI == "-") || (SI == "NOT")) { AddTemp(); result->Add("!"); Position = 0; continue; }
        if ((SI == "&") || (SI == "&&")) { AddTemp(); result->Add("AND"); Position = 0; continue; }
        if ((SI == "|") || (SI == "||")) { AddTemp(); result->Add("OR"); Position = 0; continue; }

        /* Subs */
        if (SI == "()") {
            AddTemp();
            result->AddObject("()", ProcessSearchElements(Sub(St, I), OperatorNames));
            continue;
        }

        /* [0] String [1] CompOp [2] String */
        if (Position == 0) {
            Temp.Add(SI); Position = 1; continue;
        }

        if (Position == 1) {
            Found = false;
            for (J = 0; J < 8; J++) if (SI == QLatin1String(TwoLetterOps[J])) { Found = true; break; }
            if (!Found) for (J = 0; J < 3; J++) if (SI == QString(QLatin1Char(OneLetterOps[J]))) { Found = true; break; }
            if (Found) { Temp.Add(SI); Position = 2; } else { AddTemp(); Temp.Add(SI); Position = 1; }
            continue;
        }

        if (Position == 2) {
            Temp.Add(SI); AddTemp(); Position = 0; continue;
        }
    }

    AddTemp();
    return result;
}

TStringList* ProcessNotOperators(TStringList* St)
{
    int I;
    QString S;
    TStringList *St2;

    TStringList *result = new TStringList;

    I = 0;
    while (I < St->Count()) {
        if ((*St)[I] == "!") {
            if ((I == St->Count() - 1) || (St->Objects(I + 1) == nullptr)) { I++; continue; }
            if ((*St)[I + 1] == "()") {
                St2 = ProcessNotOperators(Sub(St, I + 1)); result->AddObject("()", St2);
            } else {
                AddNestedString(result, St, I + 1);
            }
            I += 2;
            if ((*St)[I - 1] == "()") S = "!()"; else S = "!";
            (*result)[result->Count() - 1] = S;
        } else {
            if ((St->Objects(I) != nullptr) && ((*St)[I] == "()")) {
                St2 = ProcessNotOperators(Sub(St, I)); result->AddObject("()", St2);
            } else {
                AddNestedString(result, St, I);
            }
            I++;
        }
    }
    return result;
}

TStringList* ProcessSearchOperators1(TStringList* St)
{
    int I, J;
    bool LastWasOp;
    TStringList *St2, *St3;

    TStringList *result = new TStringList;

    LastWasOp = true;
    I = 0;
    while ((I < St->Count()) && ((*St)[I] != "") && ((*St)[I] != "!")) I++;

    for (J = I; J < St->Count(); J++) {
        if (LastWasOp) {
            if (St->Objects(J) == nullptr) continue;
            LastWasOp = false;
        } else {
            if (St->Objects(J) == nullptr) { result->Add((*St)[J]); LastWasOp = true; continue; }
            result->Add("AND");
        }
        St2 = Sub(St, J);
        if ((St2->Count() == 0) || ((*St)[J] == "") || ((*St)[J] == "!")) {
            St3 = new TStringList; St3->AddStrings(*St2);
        } else {
            St3 = ProcessSearchOperators1(St2);
        }
        result->AddObject((*St)[J], St3);
    }
    return result;
}

TStringList* ProcessSearchOperators2(TStringList* St)
{
    QString Op;
    int I;
    bool NextIsOp;
    TStringList *St2;

    TStringList *result = new TStringList;

    auto AddToResult = [&](int Nr) {
        if (((*St)[Nr] == "()") || ((*St)[Nr] == "!()")) result->AddObject((*St)[Nr], ProcessSearchOperators2(Sub(St, Nr)));
        if (((*St)[Nr] == "") || ((*St)[Nr] == "!")) AddNestedString(result, St, Nr);
    };

    if (St->Count() < 3) { result->Add("AND"); if (St->Count() > 0) AddToResult(0); return result; }

    Op = (*St)[1]; result->Add(Op); AddToResult(0); AddToResult(2); NextIsOp = true;
    for (I = 3; I < St->Count(); I++) {
        if (NextIsOp) {
            if (St->Objects(I) != nullptr) { AddToResult(I); continue; }
            if ((*St)[I] == Op) NextIsOp = false; else {
                /* Wrong operator needing sub list */
                FreeNestedStringList(Sub(result, result->Count() - 1));
                result->Delete(result->Count() - 1);
                St2 = new TStringList;
                AddNestedStrings(St2, St, I - 1, St->Count() - 1);
                result->AddObject("()", ProcessSearchOperators2(St2));
                FreeNestedStringList(St2);
                break;
            }
        } else {
            if (St->Objects(I) != nullptr) AddToResult(I);
            NextIsOp = true;
        }
    }
    return result;
}

TStringList* ProcessQuotes(TStringList* St)
{
    TStringList *result = new TStringList;

    for (int I = 0; I < St->Count(); I++) {
        if (((*St)[I] != "") && (((*St)[I][0] == '"') || ((*St)[I][0] == '\''))) {
            result->Add(Copy((*St)[I], 2, Length((*St)[I]) - 2));
        } else {
            if (St->Objects(I) != nullptr) result->AddObject((*St)[I], ProcessQuotes(Sub(St, I))); else result->Add((*St)[I]);
        }
    }
    return result;
}

void FreeNestedStringList(TStringList* St)
{
    if (St == nullptr) return;
    for (int I = 0; I < St->Count(); I++) if (St->Objects(I) != nullptr) FreeNestedStringList(Sub(St, I));
    delete St;
}

static TStringList *FlatNestedStringList(TStringList *St, const QString &Indent)
{
    TStringList *result = new TStringList;
    for (int I = 0; I < St->Count(); I++) {
        if (St->Objects(I) != nullptr) {
            result->Add(Indent + "  List element [" + (*St)[I] + "]:");
            TStringList *St2 = FlatNestedStringList(Sub(St, I), Indent + "    ");
            result->AddStrings(*St2);
            delete St2;
        } else {
            result->Add(Indent + (*St)[I]);
        }
    }
    return result;
}

QString NestedStringListToString(TStringList* St)
{
    TStringList *St2 = FlatNestedStringList(St, "");
    QString result = St2->Text();
    delete St2;
    return result;
}
