// C++ port of CheatDBToolsUnit.pas (implementation section).
#include "CheatDBToolsUnit.h"

#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "DownloadWaitFormUnit.h"
#include "LanguageSetupUnit.h"
#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

static bool CheatsDBAvailable()
{
    return
        FileExists(PrgDataDir() + SettingsFolder + PathDelim + CheatDBFile) ||
        FileExists(PrgDir() + BinFolder + PathDelim + CheatDBFile) ||
        FileExists(PrgDir() + CheatDBFile);
}

TCheatDB* SmartLoadCheatsDB()
{
    bool B;
    TCheatDB* result = new TCheatDB;

    if (FileExists(PrgDataDir() + SettingsFolder + PathDelim + CheatDBFile)) {
        B = result->LoadFromXML(PrgDataDir() + SettingsFolder + PathDelim + CheatDBFile);
    } else {
        B = false;
    }

    if (!B) {
        if (FileExists(PrgDir() + BinFolder + PathDelim + CheatDBFile)) {
            B = result->LoadFromXML(PrgDir() + BinFolder + PathDelim + CheatDBFile);
        } else {
            B = false;
        }
    }

    if (!B) {
        if (FileExists(PrgDir() + CheatDBFile)) {
            B = result->LoadFromXML(PrgDir() + CheatDBFile);
        } else {
            B = false;
        }
    }

    if (!B) { delete result; result = nullptr; }
    return result;
}

void SmartSaveCheatsDB(TCheatDB* ACheatDB, QWidget* AForm)
{
    if (!ACheatDB->Changed()) return;
    ForceDirectories(PrgDataDir() + SettingsFolder);
    ACheatDB->SaveToXML(PrgDataDir() + SettingsFolder + PathDelim + CheatDBFile, AForm);
}

bool ExtTryStrToInt(const QString& S, int& I)
{
    QString T;
    quint32 Temp;
    bool Minus;
    bool result;
    T = Trim(S);

    Minus = (T != "") && (T[0] == '-');
    if (Minus) T = Trim(Copy(T, 2, MaxInt));

    if ((T != "") && (T[0] == '$')) {
        result = false; T = UpperCase(Trim(Copy(T, 2, MaxInt))); Temp = 0;
        for (int J = 1; J <= Length(T); J++) {
            Temp = Temp * 16;
            const QChar C = T[J - 1];
            if (C >= '0' && C <= '9') Temp = Temp + quint32(C.unicode() - '0');
            else if (C >= 'A' && C <= 'F') Temp = Temp + quint32(C.unicode() - 'A' + 10);
            else return result;
            I = static_cast<int>(Temp);
            result = true;
        }
    } else {
        result = TryStrToInt(S, I);
    }

    if (Minus) I = -I;
    return result;
}

bool LoadAddresses(const QString& S, TIntegerArray& List)
{
    QString T;
    TIntegerArray L;
    int I, J;
    auto Add = [&L](const QString& S) -> bool {
        int J;
        bool result;
        if (Trim(S) == "") { result = true; return result; }
        result = ExtTryStrToInt(Trim(S), J);
        if (!result) return result;
        L.append(J);
        return result;
    };
    bool result = false;
    T = Trim(S); L.clear();
    I = Pos(";", T); J = Pos(";", T); if (I * J > 0) I = Min(I, J); else I = Max(I, J);
    while (I != 0) {
        if (I == 1) {
            T = Trim(Copy(T, 2, MaxInt));
        } else {
            if (!Add(Copy(T, 1, I - 1))) return result;
            if (I == Length(T)) T = ""; else T = Trim(Copy(T, I + 1, MaxInt));
        }
        I = Pos(";", T); J = Pos(";", T); if (I * J > 0) I = Min(I, J); else I = Max(I, J);
    }
    if (T != "") {
        if (!Add(T)) return result;
    }
    List = L;
    result = true;
    return result;
}

bool ExtSeek(QFile* St, int Address)
{
    bool result = false;
    if (Address >= 0) {
        if (St->size() <= Address) return result;
        St->seek(Address);
    } else {
        if (St->size() - qAbs(Address) < 0) return result;
        St->seek(St->size() - qAbs(Address));
    }
    result = true;
    return result;
}

bool ExtSeek(QFile* St, int Address, int BytesToWrite)
{
    bool result = false;
    if (!ExtSeek(St, Address)) return result;
    result = (St->pos() + BytesToWrite <= St->size());
    return result;
}

void BackupFileBeforeApplyingCheat(const QString& FileName)
{
    auto BackupFile = [&FileName](int Nr) -> QString { return FileName + ".backup" + IntToStr(Nr); };
    if (FileExists(BackupFile(9))) ExtDeleteFile(BackupFile(9), ftUninstall);
    for (int I = 8; I >= 1; I--) {
        if (FileExists(BackupFile(I))) {
#ifdef Q_OS_WIN
            const bool Moved = MoveFileExW(reinterpret_cast<LPCWSTR>(BackupFile(I).utf16()), reinterpret_cast<LPCWSTR>(BackupFile(I + 1).utf16()), MOVEFILE_REPLACE_EXISTING);
#else
            if (FileExists(BackupFile(I + 1))) QFile::remove(BackupFile(I + 1));
            const bool Moved = QFile::rename(BackupFile(I), BackupFile(I + 1));
#endif
            if (!Moved) ExtDeleteFile(BackupFile(I), ftUninstall);
        }
    }
    CopyFile(FileName, BackupFile(1), false);
}

TStringList* GetGameFolders(const QString& BaseGameFolder)
{
    int I;
    TSearchRec Rec;
    TStringList* St;
    TStringList* result = new TStringList;
    result->Add(IncludeTrailingPathDelimiter(BaseGameFolder));
    I = FindFirst(IncludeTrailingPathDelimiter(BaseGameFolder) + "*.*", faDirectory, Rec);
    while (I == 0) {
        if (((Rec.Attr & faDirectory) == faDirectory) && (Rec.Name != ".") && (Rec.Name != "..")) {
            St = GetGameFolders(IncludeTrailingPathDelimiter(BaseGameFolder) + Rec.Name);
            result->AddStrings(*St);
            delete St;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

TStringList* FindMatchingGameRecords(TCheatDB* CheatDB, TStringList* Folders)
{
    int L;
    TSearchRec Rec;
    bool B;
    TStringList* result = new TStringList;
    for (int I = 0; I < CheatDB->Count(); I++) {
        B = false;
        for (int J = 0; J < CheatDB->Game(I)->Count(); J++) {
            for (int K = 0; K < Folders->Count(); K++) {
                L = FindFirst((*Folders)[K] + CheatDB->Game(I)->Action(J)->FileMask(), faAnyFile, Rec); FindClose(Rec);
                if (L == 0) { result->AddObject(CheatDB->Game(I)->Name(), CheatDB->Game(I)); B = true; break; }
            }
            if (B) break;
        }
    }
    result->Sort();
    return result;
}

static bool FileMatchingMaskPart(const QString& Mask, const QString& Name)
{
    int I, J, L, P;
    bool result = false;
    I = 1; J = 1;
    while (I <= Length(Mask)) {
        if (Mask[I - 1] == '*') {
            if (I == Length(Mask)) { result = true; return result; } else {
                L = I + 1;
                while ((L < Length(Mask)) && (Mask[L + 1 - 1] != '*')) L++;
                P = Pos(Copy(Mask, I + 1, L - I), Name);
                if (P > 0) J = P - 1; else return result;
            }
        } else {
            // Delphi reads Name[J] even if J>length(Name) (returns #0 there)
            const QChar NameJ = (J >= 1 && J <= Length(Name)) ? Name[J - 1] : QChar(0);
            if ((Mask[I - 1] != '?') && ((Length(Name) < I) || (Mask[I - 1] != NameJ))) return result;
        }
        I++; J++;
    }
    result = (J > Length(Name));
    return result;
}

bool FileMatchingMask(const QString& FileName, const QString& FileMask)
{
    QString NameMask, Name, ExtMask, Ext;
    int I;
    I = Pos(".", FileMask);
    if (I == 0) {
        NameMask = FileMask; ExtMask = "*";
    } else {
        NameMask = Copy(FileMask, 1, I - 1);
        ExtMask = Copy(FileMask, I + 1, MaxInt);
    }

    I = Pos(".", FileName);
    if (I == 0) {
        Name = FileName; Ext = "";
    } else {
        Name = Copy(FileName, 1, I - 1);
        Ext = Copy(FileName, I + 1, MaxInt);
    }

    return FileMatchingMaskPart(ExtUpperCase(NameMask), ExtUpperCase(Name)) && FileMatchingMaskPart(ExtUpperCase(ExtMask), ExtUpperCase(Ext));
}

void CheatsDBUpdateCheckIfSetup(QWidget* AForm)
{
    bool DoUpdateCheck;
    if (!CheatsDBAvailable()) DoUpdateCheck = true; else {
        DoUpdateCheck = false;
        switch (PrgSetup->CheatsDBCheckForUpdates()) {
            case 0: DoUpdateCheck = false; break;
            case 1: DoUpdateCheck = (Trunc(Date()) >= PrgSetup->LastCheatsDBUpdateCheck() + 7); break;
            case 2: DoUpdateCheck = (Trunc(Date()) >= PrgSetup->LastCheatsDBUpdateCheck() + 1); break;
            case 3: DoUpdateCheck = true; break;
        }
    }
    if (DoUpdateCheck) CheatsDBUpdateCheck(AForm, true);
}

static void UpdateCheatsDB(TCheatDB* MainDB, TCheatDB* TempDB)
{
    int Nr1, Nr2;
    TCheatGameRecord* G;
    TCheatAction* A;
    MainDB->SetVersion(TempDB->Version());
    for (int I = 0; I < TempDB->Count(); I++) {
        Nr1 = MainDB->IndexOf(TempDB->Game(I)->Name());
        if (Nr1 < 0) {
            /* Copy TempDB[I] to MainDB */
            G = new TCheatGameRecord(nullptr);
            G->CopyFrom(TempDB->Game(I));
            MainDB->Add(G);
        } else {
            for (int J = 0; J < TempDB->Game(I)->Count(); J++) {
                Nr2 = MainDB->Game(Nr1)->IndexOf(TempDB->Game(I)->Action(J)->Name());
                if (Nr2 >= 0) continue;
                /* Copy TempDB[I][J] to MainDB[Nr1] */
                A = new TCheatAction(nullptr);
                A->CopyFrom(TempDB->Game(I)->Action(J));
                MainDB->Game(Nr1)->Add(A);
            }
        }
    }
}

TUpdateResult CheatsDBUpdateCheck(QWidget* AForm, bool Quite)
{
    TCheatDB *CheatDB, *TempDB;
    QString DefaultDBFile, TempDBFile;
    TDownloadResult DR;
    TUpdateResult result = urUpdateInstallCanceled;

    DefaultDBFile = PrgDataDir() + SettingsFolder + PathDelim + CheatDBFile;
    TempDBFile = TempDir() + CheatDBFile;

    CheatDB = SmartLoadCheatsDB();
    {
        if (CheatDB == nullptr) TempDBFile = DefaultDBFile;
        if (Quite) DR = DownloadFileWithOutDialog(AForm, -1, "", CheatDBUpdateURL, "", TempDBFile);
              else DR = DownloadFileWithDialog(AForm, -1, "", CheatDBUpdateURL, "", TempDBFile);
        if (DR == drFail) {
            MessageDlg(Format(LanguageSetup->PackageManagerDownloadFailed(), {CheatDBUpdateURL}), mtError, mbOK, 0);
            delete CheatDB;
            return result;
        }
        if (CheatDB == nullptr) {
            if (DR == drSuccess) result = urUpdateInstalled; else result = urUpdateInstallCanceled;
            return result;
        }

        if (DR != drSuccess) {
            result = urUpdateInstallCanceled;
            delete CheatDB;
            return result;
        }

        result = urNoUpdatesAvailable;

        PrgSetup->SetLastCheatsDBUpdateCheck(Trunc(Date()));

        TempDB = new TCheatDB;
        if (TempDB->LoadFromXML(TempDBFile)) {
            // Literal translation of "if not TempDB.Version>CheatDB.Version":
            // in Delphi "not" binds stronger than ">", so this is a bitwise not
            // of the version number.
            if ((~TempDB->Version()) > CheatDB->Version()) {
                UpdateCheatsDB(CheatDB, TempDB);
                SmartSaveCheatsDB(CheatDB, AForm);
                result = urUpdateInstalled;
            }
        }
        delete TempDB;
        ExtDeleteFile(TempDBFile, ftTemp);
    }
    delete CheatDB;
    return result;
}
