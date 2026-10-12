// C++ port of HistoryUnit.pas (implementation section).
#include "HistoryUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "LanguageSetupUnit.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>

THistory* History = nullptr;

// StrToDate / StrToTime replacements (Delphi raised EConvertError on failure).
// Besides the current locale the formats written by the original Windows
// program (e.g. "12.10.2009" / "14:33:10") are accepted.
static bool TryStrToDateLocal(const QString &S, TDateTime &Value)
{
    const QString T = S.trimmed();
    QDate D = QLocale().toDate(T, QLocale::ShortFormat);
    if (!D.isValid()) D = QLocale().toDate(T, QLocale::LongFormat);
    static const char *Formats[] = {"dd.MM.yyyy", "d.M.yyyy", "dd.MM.yy", "MM/dd/yyyy", "M/d/yyyy", "dd/MM/yyyy", "yyyy-MM-dd", "M/d/yy"};
    for (const char *F : Formats) {
        if (D.isValid()) break;
        D = QDate::fromString(T, QString::fromLatin1(F));
        if (D.isValid() && D.year() < 1950 && QString::fromLatin1(F).count('y') == 2) D = D.addYears(100);
    }
    if (!D.isValid()) return false;
    Value = EncodeDate(D.year(), D.month(), D.day());
    return true;
}

static bool TryStrToTimeLocal(const QString &S, TDateTime &Value)
{
    const QString T = S.trimmed();
    QTime Tm = QLocale().toTime(T, QLocale::LongFormat);
    if (!Tm.isValid()) Tm = QLocale().toTime(T, QLocale::ShortFormat);
    static const char *Formats[] = {"HH:mm:ss", "H:mm:ss", "HH:mm", "H:mm", "h:mm:ss AP", "h:mm:ss ap", "h:mm AP"};
    for (const char *F : Formats) {
        if (Tm.isValid()) break;
        Tm = QTime::fromString(T, QString::fromLatin1(F));
    }
    if (!Tm.isValid()) return false;
    Value = Tm.msecsSinceStartOfDay() / 86400000.0;
    return true;
}

/* THistory */

THistory::THistory()
{
    StartData.clear();
    for (int I = 0; I <= 26; I++) GamesList[I] = nullptr;
    StartNames = nullptr;
    StartDates = nullptr;
    StartTimes = nullptr;
    StartDataIndex = nullptr;

    hChangeNotification = 0; // INVALID_HANDLE_VALUE
    LastChangeNotifyCheck = 0;
    LastHistoryTime = 0;
}

THistory::~THistory()
{
    hChangeNotification = 0; // FindCloseChangeNotification
    FreeLists();
}

void THistory::FreeLists()
{
    StartData.clear();
    for (int I = 0; I <= 26; I++) if (GamesList[I] != nullptr) { delete GamesList[I]; GamesList[I] = nullptr; }
    if (StartNames != nullptr) { delete StartNames; StartNames = nullptr; }
    if (StartDates != nullptr) { delete StartDates; StartDates = nullptr; }
    if (StartTimes != nullptr) { delete StartTimes; StartTimes = nullptr; }
    if (StartDataIndex != nullptr) { delete StartDataIndex; StartDataIndex = nullptr; }
}

QString THistory::GetHistoryFileName()
{
    return PrgDataDir() + SettingsFolder + PathDelim + HistoryFileName;
}

int THistory::GetHistoryFileTime()
{
    TSearchRec Rec;
    int result = 0;
    int I = FindFirst(GetHistoryFileName(), faAnyFile, Rec);
    if (I == 0) result = Rec.Time;
    FindClose(Rec);
    return result;
}

// Port specific: more precise change detection than the DOS file time (2s
// resolution) used by the original together with the change notification.
qint64 THistory::GetHistoryFileStamp()
{
    QFileInfo Info(ToNativePath(GetHistoryFileName()));
    if (!Info.exists()) return 0;
    return Info.lastModified().toMSecsSinceEpoch() * 31 + Info.size();
}

int THistory::ListNr(const QString& GameName)
{
    int result = 0;
    for (int I = 1; I <= Length(GameName); I++) {
        QString S = ExtUpperCase(QString(GameName[I - 1]));
        if ((S != "") && (S[0] >= 'A') && (S[0] <= 'Z')) result = 1 + (S[0].unicode() - 'A');
        break;
    }
    return result;
}

void THistory::LoadList()
{
    int I, Nr1, Nr2, Nr3;
    QString S;
    TDateTime StartTime, T;

    /* Init lists */
    StartData.clear();
    for (I = 0; I <= 26; I++) GamesList[I] = new TStringList;
    StartNames = new TStringList;
    StartDates = new TStringList;
    StartTimes = new TStringList;
    StartDataIndex = new TList;

    /* Load file */
    if (!FileExists(GetHistoryFileName())) return;
    TStringList RawList;
    if (!RawList.LoadFromFile(GetHistoryFileName())) return;

    for (I = 0; I < RawList.Count(); I++) {
        /* Prepare line */
        S = Trim(RawList[I]);
        if (S == "") continue;
        TStringList *St = ValueToList(S);
        if (St->Count() < 3) { delete St; continue; }
        while (St->Count() > 3) { (*St)[0] = (*St)[0] + ";" + (*St)[1]; St->Delete(1); }

        StartTime = 0;
        if (TryStrToDateLocal((*St)[1], T)) StartTime = T;
        if (TryStrToTimeLocal((*St)[2], T)) StartTime = StartTime + T;

        /* Add to statistics */
        Nr1 = ListNr((*St)[0]);
        Nr2 = GamesList[Nr1]->IndexOf((*St)[0]);
        if (Nr2 < 0) {
            Nr3 = StartData.size();
            TInfoRec R;
            R.Count = 1; R.First = StartTime; R.Last = StartTime; R.FirstStartNamesNr = StartNames->Count();
            StartData.append(R);
            GamesList[Nr1]->AddObject((*St)[0], (intptr_t)Nr3);
        } else {
            Nr3 = GamesList[Nr1]->ObjectAsInt(Nr2);
            TInfoRec &R = StartData[Nr3];
            R.Count++;
            if (StartTime > R.Last) R.Last = StartTime;
            if (StartTime < R.First) R.First = StartTime;
        }

        /* Add to history */
        StartNames->AddObject((*St)[0], (intptr_t)Nr3);
        StartDates->Add((*St)[1]);
        StartTimes->Add((*St)[2]);
        StartDataIndex->append(reinterpret_cast<void *>((intptr_t)Nr3));
        delete St;
    }

    for (I = 0; I <= 26; I++) GamesList[I]->SetSorted(true);
}

void THistory::LoadOrUpdateList()
{
    if (GamesList[0] == nullptr) {
        LoadList();
        // FindFirstChangeNotification(ExtractFilePath(GetHistoryFileName), ...) replaced by polling
        hChangeNotification = 1;
        LastChangeNotifyCheck = GetTickCount();
        LastHistoryTime = GetHistoryFileTime();
        LastHistoryStamp = GetHistoryFileStamp();
        return;
    }

    if (LastChangeNotifyCheck < GetTickCount() + 1000) {
        LastChangeNotifyCheck = GetTickCount();
        if (hChangeNotification != 0) {
            qint64 Stamp = GetHistoryFileStamp();
            int I = GetHistoryFileTime();
            if ((Stamp != LastHistoryStamp) || (I != LastHistoryTime)) {
                LastHistoryTime = I;
                LastHistoryStamp = Stamp;
                FreeLists();
                LoadList();
            }
        }
    }
}

void THistory::ResetUpdateNotify()
{
    if (hChangeNotification == 0) return;
    LastHistoryTime = GetHistoryFileTime();
    LastHistoryStamp = GetHistoryFileStamp();
    LastChangeNotifyCheck = GetTickCount();
}

void THistory::Add(const QString& GameName)
{
    int Nr1, Nr2, Nr3;
    TDateTime StartTime;

    if ((Trim(GameName) == "") || (!PrgSetup->StoreHistory())) return;

    StartTime = Now();
    LoadOrUpdateList();

    /* Add to statistics */
    Nr1 = ListNr(GameName);
    Nr2 = GamesList[Nr1]->IndexOf(GameName);
    if (Nr2 < 0) {
        Nr3 = StartData.size();
        TInfoRec R;
        R.Count = 1; R.First = StartTime; R.Last = StartTime;
        StartData.append(R);
        GamesList[Nr1]->AddObject(GameName, (intptr_t)Nr3);
    } else {
        Nr3 = GamesList[Nr1]->ObjectAsInt(Nr2);
        StartData[Nr3].Count++; StartData[Nr3].Last = StartTime;
    }

    /* Add to history */
    StartNames->Add(GameName);
    StartDates->Add(DateToStr(StartTime));
    StartTimes->Add(TimeToStr(StartTime));
    StartDataIndex->append(reinterpret_cast<void *>((intptr_t)Nr3));

    /* Add to file */
    bool Ok = false;
    {
        ResetUpdateNotify();
        QFile F(ToNativePath(GetHistoryFileName()));
        QIODevice::OpenMode Mode = FileExists(GetHistoryFileName()) ? (QIODevice::WriteOnly | QIODevice::Append) : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (F.open(Mode)) {
            QByteArray Line = EncodeAnsi(GameName + ";" + DateToStr(StartTime) + ";" + TimeToStr(StartTime) + LineBreak);
            Ok = (F.write(Line) == Line.size());
            F.close();
        }
    }
    if (!Ok)
        MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {GetHistoryFileName()}), mtError, mbOK, 0);
}

bool THistory::Clear()
{
    bool result = true;
    if (!FileExists(GetHistoryFileName())) return result;
    result = ExtDeleteFile(GetHistoryFileName(), ftProfile);
    ResetUpdateNotify();
    if (!result) MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {GetHistoryFileName()}), mtError, mbOK, 0);

    StartData.clear();
    for (int I = 0; I <= 26; I++) if (GamesList[I] != nullptr) GamesList[I]->Clear();
    if (StartNames != nullptr) StartNames->Clear();
    if (StartDates != nullptr) StartDates->Clear();
    if (StartTimes != nullptr) StartTimes->Clear();
    if (StartDataIndex != nullptr) StartDataIndex->clear();
    return result;
}

void THistory::LoadHistory(TListView* AListView)
{
    TListColumn *C;
    int I, Nr1, Nr3;
    TListItem *L;

    LoadOrUpdateList();

    AListView->Items()->BeginUpdate();
    AListView->Columns()->BeginUpdate();
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryGame()); C->SetWidth(1);
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryDateTime()); C->SetWidth(1);
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryStarts()); C->SetWidth(1);

    for (I = 0; I < StartNames->Count(); I++) {
        L = AListView->Items()->Add();
        L->SetCaption((*StartNames)[I]);
        L->SubItems().Add((*StartDates)[I] + " " + (*StartTimes)[I]);
        L->SetData(reinterpret_cast<void *>((intptr_t)I));
        Nr1 = ListNr((*StartNames)[I]);
        Nr3 = GamesList[Nr1]->ObjectAsInt(GamesList[Nr1]->IndexOf((*StartNames)[I]));
        L->SubItems().Add(IntToStr(StartData[Nr3].Count));
    }

    AListView->Items()->EndUpdate();
    for (I = 0; I < AListView->Columns()->Count(); I++) AListView->Column(I)->SetWidth(-2);
    AListView->Columns()->EndUpdate();
}

TStringList* THistory::GetRecentlyStarted(int MaxGames, bool UpperCase)
{
    QString S;

    LoadOrUpdateList();

    TStringList *result = new TStringList;
    for (int I = StartNames->Count() - 1; I >= 0; I--) {
        S = (*StartNames)[I];
        if (UpperCase) S = ExtUpperCase(S);
        if (result->IndexOf(S) < 0) result->Add(S);
        if (result->Count() >= MaxGames) return result;
    }
    return result;
}

void THistory::LoadStatistics(TListView* AListView)
{
    TListColumn *C;
    int I, Nr3;
    TListItem *L;

    LoadOrUpdateList();

    AListView->Items()->BeginUpdate();
    AListView->Columns()->BeginUpdate();
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryGame()); C->SetWidth(1);
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryStarts()); C->SetWidth(1);
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryFirst()); C->SetWidth(1);
    C = AListView->Columns()->Add(); C->SetCaption(LanguageSetup->HistoryLast()); C->SetWidth(1);

    {
        TStringList St;
        for (I = 0; I <= 26; I++) St.AddStrings(*GamesList[I]);
        St.Sort();
        for (I = 0; I < St.Count(); I++) {
            L = AListView->Items()->Add();
            L->SetCaption(St[I]);
            Nr3 = St.ObjectAsInt(I);
            L->SetData(reinterpret_cast<void *>((intptr_t)StartData[Nr3].FirstStartNamesNr));
            L->SubItems().Add(IntToStr(StartData[Nr3].Count));
            L->SubItems().Add(DateToStr(StartData[Nr3].First) + " " + TimeToStr(StartData[Nr3].First));
            L->SubItems().Add(DateToStr(StartData[Nr3].Last) + " " + TimeToStr(StartData[Nr3].Last));
        }
    }

    AListView->Items()->EndUpdate();
    for (I = 0; I < AListView->Columns()->Count(); I++) AListView->Column(I)->SetWidth(-2);
    AListView->Columns()->EndUpdate();
}

int THistory::GameStartCount(const QString& GameName)
{
    int Nr2;
    TStringList *St;

    LoadOrUpdateList();

    int result = 0;
    St = GamesList[ListNr(GameName)];
    Nr2 = St->IndexOf(GameName); if (Nr2 < 0) return result;
    result = StartData[St->ObjectAsInt(Nr2)].Count;
    return result;
}

// Note: the original checked "I>StartNames.Count" (off by one); the port uses
// ">=" and also handles a not yet loaded list instead of crashing.
int THistory::GetStartCount(int I)
{
    int result = 0;
    if (StartNames == nullptr || (I < 0) || (I >= StartNames->Count())) return result;
    result = StartData[StartNames->ObjectAsInt(I)].Count;
    return result;
}

TDateTime THistory::GetStartDateTime(int I)
{
    TDateTime result = 0, T;
    if (StartNames == nullptr || (I < 0) || (I >= StartNames->Count())) return result;
    if (TryStrToDateLocal((*StartDates)[I], T)) result = T;
    if (TryStrToTimeLocal((*StartTimes)[I], T)) result = result + T;
    return result;
}

TDateTime THistory::GetStartDateTimeFirst(int I)
{
    TDateTime result = 0;
    if (StartNames == nullptr || (I < 0) || (I >= StartNames->Count())) return result;
    result = StartData[StartNames->ObjectAsInt(I)].First;
    return result;
}

TDateTime THistory::GetStartDateTimeLast(int I)
{
    TDateTime result = 0;
    if (StartNames == nullptr || (I < 0) || (I >= StartNames->Count())) return result;
    result = StartData[StartNames->ObjectAsInt(I)].Last;
    return result;
}

void HistoryUnit_Initialization()
{
    History = new THistory;
}

void HistoryUnit_Finalization()
{
    delete History;
    History = nullptr;
}
