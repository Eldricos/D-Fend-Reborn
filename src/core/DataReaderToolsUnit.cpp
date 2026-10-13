// C++ port of DataReaderToolsUnit.pas (implementation section).
#include "DataReaderToolsUnit.h"
#include "CommonTools.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QStringDecoder>
#include <memory>

// Indy TIdHTTP → QNetworkAccessManager.  Every call creates its own manager in
// the calling thread (the data reader threads call this from QThread::run()),
// exactly like the Delphi code created a TIdHTTP object for every download.
// Returns nullptr on error (like the nil TMemoryStream).
static std::unique_ptr<QByteArray> DownloadFile(const QString& URL, const QString& Referer)
{
    if ((LowerCase(Copy(URL, 1, 7)) != "http://") && (LowerCase(Copy(URL, 1, 8)) != "https://") && (LowerCase(Copy(URL, 1, 6)) != "ftp://")) return nullptr;

    QNetworkAccessManager HTTP;
    QNetworkRequest Request{QUrl(URL)};
    Request.setRawHeader("User-Agent", "User-Agent=Mozilla/5.0 (Windows; U; Windows NT 6.0)");
    Request.setRawHeader("Accept-Language", "en");
    if (!Referer.isEmpty()) Request.setRawHeader("Referer", Referer.toUtf8());
    // Port note: Indy did not follow redirects by default; following them is
    // required for today's http → https redirects.
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    Request.setTransferTimeout(60000);

    QNetworkReply* Reply = HTTP.get(Request);
    QEventLoop Loop;
    QObject::connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
    if (!Reply->isFinished()) Loop.exec();

    std::unique_ptr<QByteArray> result;
    if (Reply->error() == QNetworkReply::NoError) result = std::make_unique<QByteArray>(Reply->readAll());
    delete Reply;
    return result;
}

static QString RemoveLineBreaks(const QString& S)
{
    QString result = S;
    for (int I = 1; I <= Length(S); I++) if ((S[I - 1] == QChar(10)) || (S[I - 1] == QChar(13))) result[I - 1] = ' ';
    return result;
}

bool DownloadTextFile(const QString& URL, QString& Lines, const QString& Referer)
{
    std::unique_ptr<QByteArray> MSt = DownloadFile(URL, Referer);
    bool result = (MSt != nullptr); if (!result) return result;
    // Port note: Delphi copied the raw bytes into an AnsiString.  Valid UTF-8
    // (today's web pages) is decoded as UTF-8, everything else byte-wise with
    // the 8 bit codepage like before.
    QStringDecoder Decoder(QStringDecoder::Utf8);
    QString S = Decoder.decode(*MSt);
    if (Decoder.hasError()) S = DecodeAnsi(*MSt);
    Lines = S;
    Lines = RemoveLineBreaks(Lines);
    return result;
}

bool DownloadFileToDisk(const QString& URL, const QString& FileName, const QString& Referer)
{
    bool result = false;
    std::unique_ptr<QByteArray> MSt = DownloadFile(URL, Referer); if (MSt == nullptr) return result;
    result = true;
    QFile F(FileName);
    if (!F.open(QIODevice::WriteOnly | QIODevice::Truncate)) return result;
    F.write(*MSt);
    F.close();
    return result;
}

static int FindNext(const QString& Lines, const QString& Search, int From = 1)
{
    QString S, T;
    int result = 0;
    if (Search == " ") S = " "; else S = ExtUpperCase(Search);
    if (S.isEmpty()) return result;
    for (int I = From; I <= Length(Lines) - Length(S) + 1; I++) {
        T = ExtUpperCase(QString(Lines[I - 1])); if (T.isEmpty()) T = " ";
        if (T[0] == S[0]) {
            if (ExtUpperCase(Copy(Lines, I + 1, Length(S) - 1)) == Copy(S, 2, MaxInt)) {
                result = I; return result;
            }
        }
    }
    return result;
}

QString EncodeName(const QString& Name)
{
    QString result;
    for (int I = 1; I <= Length(Name); I++) {
        const QChar C = Name[I - 1];
        if (C == ' ') result += '+';
        else if (C == '/') result += "%2F";
        else if (C == '#') result += "%23";
        else if (C == '+') result += "%2B";
        else if (C == '?') result += "%3F";
        else if (C == '&') result += "%26";
        else result += C;
    }
    return result;
}

static bool TryHexStrToInt(const QString& S, int& Value)
{
    bool result = true;
    Value = 0;
    QString T = ExtUpperCase(S);
    for (int I = 1; I <= Length(T); I++) {
        const QChar C = T[I - 1];
        if (C >= '0' && C <= '9') Value = Value * 16 + (C.unicode() - '0');
        else if (C >= 'A' && C <= 'F') Value = Value * 16 + (C.unicode() - 'A' + 10);
        else { result = false; return result; }
    }
    return result;
}

QString DecodeName(const QString& Name)
{
    const QString ReplaceFrom[2] = {QStringLiteral("&nbsp;"), QString(QChar(0xC3)) + QChar(0xB8)};
    const QString ReplaceTo[2] = {QStringLiteral(" "), QString(QChar(0xF8))};
    QString S, T, result;
    int I, J, K;

    S = Name; result = "";
    while (!S.isEmpty()) {
        I = FindNext(S, "&#x"); if (I == 0) { result = result + S; break; }
        J = FindNext(S, ";", I + 3); if (J == 0) { result = result + S; break; }
        T = Copy(S, I + 3, J - (I + 3));
        if (!TryHexStrToInt(T, K)) { result = result + Copy(S, 1, J); S = Copy(S, J + 1, MaxInt); continue; }
        char32_t UCS4 = static_cast<char32_t>(K);
        result = result + Copy(S, 1, I - 1) + QString::fromUcs4(&UCS4, 1);
        S = Copy(S, J + 1, MaxInt);
    }

    for (J = 0; J <= 1; J++) {
        S = result; result = "";
        while (!S.isEmpty()) {
            I = FindNext(S, ReplaceFrom[J]);
            if (I == 0) { result = result + S; break; }
            result = result + Copy(S, 1, I - 1) + ReplaceTo[J];
            S = Copy(S, I + Length(ReplaceFrom[J]), MaxInt);
        }
    }
    return result;
}

TStringList* GetTagContents(const QString& Lines, int StartPosition, const QString& Tag, const QString& TagAttributeName, const QString& TagAttributeValue, const QString& RequestAttributeName, TStringList* RequestAttributeValue, const QString& RequestTextContent)
{
    int Position, I, J, TagContentStart, Count, TagContentEnd;
    QChar C;
    bool Ok;
    QString RequestValue, S;

    TStringList* result = new TStringList;
    Position = StartPosition;

    /* Find "<Tag" */
    I = FindNext(Lines, "<" + Tag, Position);
    while (I > 0) {
        Position = I + Length("<" + Tag);
        if (Position > Length(Lines)) return result;

        /* Check if "<Tag" has a DataKey="DataValue" or DataKey='DataValue' attribute */
        if ((Trim(TagAttributeName).isEmpty()) && (Trim(RequestAttributeName).isEmpty())) {
            while (Position < Length(Lines)) {
                if (Lines[Position - 1] == '>') { Position++; break; }
                Position++;
            }
            Ok = true;
        } else {
            RequestValue = "";
            Ok = (Trim(TagAttributeName).isEmpty());
            while (Position < Length(Lines)) {
                if (Lines[Position - 1] != ' ') {
                    if ((!Trim(TagAttributeName).isEmpty()) && (ExtUpperCase(Copy(Lines, Position, Length(TagAttributeName))) == ExtUpperCase(TagAttributeName))) {
                        Position += Length(TagAttributeName);
                        if ((Position > Length(Lines)) || (Lines[Position - 1] != '=')) continue;
                        Position++;
                        if ((Position > Length(Lines)) || ((Lines[Position - 1] != '"') && (Lines[Position - 1] != '\''))) continue;
                        C = Lines[Position - 1];
                        Position++;
                        if (ExtUpperCase(Copy(Lines, Position, Length(TagAttributeValue + C))) == ExtUpperCase(TagAttributeValue + C)) { Position += Length(TagAttributeValue + C); Ok = true; }
                        continue;
                    }
                    if ((!Trim(RequestAttributeName).isEmpty()) && (ExtUpperCase(Copy(Lines, Position, Length(RequestAttributeName))) == ExtUpperCase(RequestAttributeName))) {
                        Position += Length(RequestAttributeName);
                        if ((Position > Length(Lines)) || (Lines[Position - 1] != '=')) continue;
                        Position++;
                        if ((Position > Length(Lines)) || ((Lines[Position - 1] != '"') && (Lines[Position - 1] != '\''))) continue;
                        C = Lines[Position - 1];
                        Position++;
                        I = FindNext(Lines, QString(C), Position);
                        if (I == 0) RequestValue = Copy(Lines, Position, MaxInt); else {
                            RequestValue = Copy(Lines, Position, (I - 1) - Position + 1);
                            Position = I + 1;
                        }
                        continue;
                    }
                }
                if (Lines[Position - 1] == '>') { Position++; break; }
                Position++;
            }
        }

        /* Find "</Tag", count open and closing tag to get the right one */
        if (Ok) {
            TagContentStart = Position;
            TagContentEnd = Length(Lines);
            Count = 1;
            while ((Count > 0) && (Position < Length(Lines))) {
                I = FindNext(Lines, "<" + Tag, Position);
                J = FindNext(Lines, "</" + Tag, Position);
                if ((I == 0) && (J == 0)) break;
                if ((I == 0) || (J == 0)) {
                    if (I > 0) Count++; else Count--;
                    TagContentEnd = Max(I, J) - 1;
                    Position = Max(I, J) + Length(Tag);
                } else {
                    if (I < J) Count++; else Count--;
                    TagContentEnd = Min(I, J) - 1;
                    Position = Min(I, J) + Length(Tag);
                }
                while ((Position <= Length(Lines)) && (Lines[Position - 1] != '>')) Position++;
            }

            S = Copy(Lines, TagContentStart, TagContentEnd - TagContentStart + 1);
            if (!RequestTextContent.isEmpty()) {
                if (Pos(ExtUpperCase(RequestTextContent), ExtUpperCase(S)) == 0) Ok = false;
            }
            if (Ok) {
                result->Add(S);
                if (RequestAttributeValue != nullptr) RequestAttributeValue->Add(RequestValue);
            }
        }

        I = FindNext(Lines, "<" + Tag, Position);
    }
    return result;
}

QString GetTagContents(const QString& Lines, int StartPosition, const QString& Tag, const QString& TagAttributeName, const QString& TagAttributeValue, int TagNr)
{
    QString result = "";
    TStringList* St = GetTagContents(Lines, StartPosition, Tag, TagAttributeName, TagAttributeValue);
    if ((TagNr > 0) && (St->Count() >= TagNr)) result = (*St)[TagNr - 1];
    if ((TagNr < 0) && (St->Count() >= -TagNr)) result = (*St)[St->Count() - (-TagNr)];   // Delphi: St[St.Count-TagNr] (out of range for TagNr<0, fixed to count from last)
    delete St;
    return result;
}

TStringList* GetJavascriptURLs(const QString& Lines, const QString& RequestTextContent)
{
    QString S;
    int I, J;
    TStringList* result = new TStringList;
    TStringList* St = GetTagContents(Lines, 1, "script", "type", "text/javascript", "", nullptr, RequestTextContent);
    if (St->Count() == 0) { delete St; return result; }
    S = (*St)[0];
    while (!S.isEmpty()) {
        I = FindNext(S, QStringLiteral("http:/") + "/"); if (I == 0) break;
        S = Copy(S, I, MaxInt);
        I = FindNext(S, "\""); J = FindNext(S, "'"); if ((I == 0) && (J == 0)) break;
        if (I == 0) I = J; else {
            if (J != 0) I = Min(I, J);
        }
        result->Add(Copy(S, 1, I - 1));
        S = Copy(S, I + 1, MaxInt);
    }
    delete St;
    return result;
}

TStringList* GetTag(const QString& Lines, int StartPosition, const QString& Tag, const QString& TagAttributeName, const QString& TagAttributeValue, const QString& RequestAttributeName, TStringList* RequestAttributeValue)
{
    int Position, I, J, TagStart, Count, TagEnd;
    QChar C;
    bool Ok;
    QString RequestValue;

    TStringList* result = new TStringList;
    Position = StartPosition;

    /* Find "<Tag" */
    I = FindNext(Lines, "<" + Tag, Position);
    while (I > 0) {
        TagStart = I;
        Position = I + Length("<" + Tag);
        if (Position > Length(Lines)) return result;

        /* Check if "<Tag" has a DataKey="DataValue" or DataKey='DataValue' attribute */
        if ((Trim(TagAttributeName).isEmpty()) && (Trim(RequestAttributeName).isEmpty())) {
            while (Position < Length(Lines)) {
                if (Lines[Position - 1] == '>') { Position++; break; }
                Position++;
            }
            Ok = true;
        } else {
            RequestValue = "";
            Ok = (Trim(TagAttributeName).isEmpty());
            while (Position < Length(Lines)) {
                if (Lines[Position - 1] != ' ') {
                    if ((!Trim(TagAttributeName).isEmpty()) && (ExtUpperCase(Copy(Lines, Position, Length(TagAttributeName))) == ExtUpperCase(TagAttributeName))) {
                        Position += Length(TagAttributeName);
                        if ((Position > Length(Lines)) || (Lines[Position - 1] != '=')) continue;
                        Position++;
                        if ((Position > Length(Lines)) || ((Lines[Position - 1] != '"') && (Lines[Position - 1] != '\''))) continue;
                        C = Lines[Position - 1];
                        Position++;
                        if (ExtUpperCase(Copy(Lines, Position, Length(TagAttributeValue + C))) == ExtUpperCase(TagAttributeValue + C)) { Position += Length(TagAttributeValue + C); Ok = true; }
                        continue;
                    }
                    if ((!Trim(RequestAttributeName).isEmpty()) && (ExtUpperCase(Copy(Lines, Position, Length(RequestAttributeName))) == ExtUpperCase(RequestAttributeName))) {
                        Position += Length(RequestAttributeName);
                        if ((Position > Length(Lines)) || (Lines[Position - 1] != '=')) continue;
                        Position++;
                        if ((Position > Length(Lines)) || ((Lines[Position - 1] != '"') && (Lines[Position - 1] != '\''))) continue;
                        C = Lines[Position - 1];
                        Position++;
                        I = FindNext(Lines, QString(C), Position);
                        if (I == 0) RequestValue = Copy(Lines, Position, MaxInt); else {
                            RequestValue = Copy(Lines, Position, (I - 1) - Position + 1);
                            Position = I + 1;
                        }
                        continue;
                    }
                }
                if (Lines[Position - 1] == '>') { Position++; break; }
                Position++;
            }
        }

        /* Find "</Tag", count open and closing tag to get the right one */
        if (Ok) {
            TagEnd = Length(Lines);
            Count = 1;
            while ((Count > 0) && (Position < Length(Lines))) {
                I = FindNext(Lines, "<" + Tag, Position);
                J = FindNext(Lines, "</" + Tag, Position);
                if ((I == 0) && (J == 0)) break;
                if ((I == 0) || (J == 0)) {
                    if (I > 0) Count++; else Count--;
                    Position = Max(I, J) + Length(Tag);
                } else {
                    if (I < J) Count++; else Count--;
                    Position = Min(I, J) + Length(Tag);
                }
                while ((Position <= Length(Lines)) && (Lines[Position - 1] != '>')) Position++;
                TagEnd = Position;
            }
            result->Add(Copy(Lines, TagStart, TagEnd - TagStart + 1));
            if (RequestAttributeValue != nullptr) RequestAttributeValue->Add(RequestValue);
        }

        I = FindNext(Lines, "<" + Tag, Position);
    }
    return result;
}

QString GetTag(const QString& Lines, int StartPosition, const QString& Tag, const QString& TagAttributeName, const QString& TagAttributeValue, int TagNr)
{
    QString result = "";
    TStringList* St = GetTag(Lines, StartPosition, Tag, TagAttributeName, TagAttributeValue);
    if ((TagNr > 0) && (St->Count() >= TagNr)) result = (*St)[TagNr - 1];
    if ((TagNr < 0) && (St->Count() >= -TagNr)) result = (*St)[St->Count() - (-TagNr)];   // Delphi: St[St.Count-TagNr] (out of range for TagNr<0, fixed to count from last)
    delete St;
    return result;
}

static void GetTokens(const QString& Lines, TStringList* Data, QList<int>* IsTag)
{
    int P;
    bool InTag;
    int I, J;

    P = 1;
    InTag = false;

    while (P < Length(Lines)) {

        if (InTag) {
            I = FindNext(Lines, ">", P);
            if (I == 0) return;
            J = FindNext(Lines, " ", P);
            if ((J == 0) || (J > I)) J = I;
            Data->Add(ExtUpperCase(Copy(Lines, P, J - P)));
            IsTag->append(1);
        } else {
            I = FindNext(Lines, "<", P);
            if (I == 0) { Data->Add(Copy(Lines, I, MaxInt)); IsTag->append(0); return; }
            if (I > P) { Data->Add(Copy(Lines, P, I - P)); IsTag->append(0); }
        }

        P = I + 1;
        InTag = !InTag;
    }
}

TStringList* GetPlainText(const QString& Lines, TStringList* IgnoreTags, TStringList* StartTokens, TStringList* StoppTokens)
{
    TStringList Data;
    QList<int> IsTag;
    QString InIgnoreMode;
    bool Started;
    int I;
    QString Token, S;

    TStringList* result = new TStringList;
    result->Add("");

    if (IgnoreTags != nullptr) for (I = 0; I < IgnoreTags->Count(); I++) (*IgnoreTags)[I] = ExtUpperCase((*IgnoreTags)[I]);
    if (StartTokens != nullptr) for (I = 0; I < StartTokens->Count(); I++) (*StartTokens)[I] = ExtUpperCase((*StartTokens)[I]);
    if (StoppTokens != nullptr) for (I = 0; I < StoppTokens->Count(); I++) (*StoppTokens)[I] = ExtUpperCase((*StoppTokens)[I]);

    Started = (StartTokens == nullptr);

    GetTokens(Lines, &Data, &IsTag);

    InIgnoreMode = "";
    for (I = 0; I < Data.Count(); I++) {
        Token = Trim(Data[I]);
        if (Token.isEmpty()) continue;
        if (IsTag[I] == 1) {
            /* Tag */
            if (InIgnoreMode.isEmpty()) {
                if ((IgnoreTags != nullptr) && (IgnoreTags->IndexOf(Token) >= 0)) InIgnoreMode = Token;
                if ((InIgnoreMode.isEmpty()) && Started) {
                    if ((Token == "BR") || (Token == "BR/") || (Token == "P") || (Token == "DIV") || (Token == "LI")) result->Add("");
                }
            } else {
                if (Token == "/" + InIgnoreMode) InIgnoreMode = "";
            }
        } else {
            /* Text */
            if ((StartTokens != nullptr) && (StartTokens->IndexOf(ExtUpperCase(Token)) >= 0)) {
                Started = true;
                continue;
            }
            if ((StoppTokens != nullptr) && (StoppTokens->IndexOf(ExtUpperCase(Token)) >= 0)) {
                break;
            }
            if ((InIgnoreMode.isEmpty()) && Started) {
                S = (*result)[result->Count() - 1];
                if ((!S.isEmpty()) && (S[Length(S) - 1] != ' ') && (Pos(Token[0], ".,;:") == 0)) S = S + " ";
                S = S + Token;
                (*result)[result->Count() - 1] = S;
            }
        }
    }

    return result;
}

QString GetPlainDecodedText(const QString& ALines)
{
    QString Lines = ALines;
    QString S;
    int I, J, Count;
    QString result;

    S = "";
    Count = 0;

    while (!Lines.isEmpty()) {
        I = FindNext(Lines, "<div", 1);
        J = FindNext(Lines, "</div", 1);

        if ((I == 0) && (J == 0)) break;

        if ((J == 0) || ((I > 0) && (I < J))) {
            if (Count == 0) S = Trim(S + " " + Copy(Lines, 1, I - 1));
            Count++;
            Lines = Copy(Lines, I + 4, MaxInt);
            continue;
        }

        if ((I == 0) || ((J > 0) && (J < I))) {
            J = FindNext(Lines, ">", J + 4);
            if (J == 0) Lines = ""; else {
                if (Count > 0) Count--;
                Lines = Copy(Lines, J + 1, MaxInt);
            }
            continue;
        }
    }

    if (Count == 0) S = Trim(S + " " + Lines);

    TStringList* St = GetPlainText(S);
    result = Trim(St->Text());
    delete St;

    const QString VH = QStringLiteral("View History");
    if ((Length(result) > Length(VH)) && (Copy(result, Length(result) - Length(VH) + 1, MaxInt) == VH)) result = Trim(Copy(result, 1, Length(result) - Length(VH)));
    const QString Ed = QStringLiteral("Edit");
    if ((Length(result) > Length(Ed)) && (Copy(result, Length(result) - Length(Ed) + 1, MaxInt) == Ed)) result = Trim(Copy(result, 1, Length(result) - Length(Ed)));
    return result;
}
