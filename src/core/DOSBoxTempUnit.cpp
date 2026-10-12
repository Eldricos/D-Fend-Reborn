// C++ port of DOSBoxTempUnit.pas (implementation section).
#include "DOSBoxTempUnit.h"

#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "DosBoxUnit.h"

// { TTempGame }

TTempGame::TTempGame(bool InitTempGame)
{
    TGame* DefaultGame;

    FNormalMinimizeState[0] = PrgSetup->MinimizeOnDosBoxStart();
    FNormalMinimizeState[1] = PrgSetup->MinimizeOnScummVMStart();
    FNormalMinimizeState[2] = PrgSetup->MinimizeOnWindowsGameStart();
    PrgSetup->SetMinimizeOnDosBoxStart(false);
    PrgSetup->SetMinimizeOnScummVMStart(false);
    PrgSetup->SetMinimizeOnWindowsGameStart(false);

    if (InitTempGame) {
        FTempProf = TempDir() + TempDOSBoxName + ".prof";
        FGame = new TGame(FTempProf);
        DefaultGame = new TGame(static_cast<TBasePrgSetup*>(PrgSetup));
        FGame->AssignFrom(DefaultGame);
        delete DefaultGame;
        AddFreeDOSToPath();
    } else {
        FGame = nullptr;
    }
}

TTempGame::~TTempGame()
{
    PrgSetup->SetMinimizeOnDosBoxStart(FNormalMinimizeState[0]);
    PrgSetup->SetMinimizeOnScummVMStart(FNormalMinimizeState[1]);
    PrgSetup->SetMinimizeOnWindowsGameStart(FNormalMinimizeState[2]);

    if (FGame != nullptr) {
        delete FGame;
        FGame = nullptr;
        ExtDeleteFile(FTempProf, ftTemp);
    }
}

void TTempGame::AddFreeDOSToPath()
{
    QString FreeDOSDir, MountDir, MountDriveLetter, InternFreeDOSPath;
    TStringList *St, *St2;
    int I, J;
    QString S;
    bool Add;

    if (Trim(PrgSetup->PathToFREEDOS()).isEmpty()) return;
    FreeDOSDir = IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->PathToFREEDOS(), PrgSetup->BaseDir()));
    if (!DirectoryExists(FreeDOSDir)) return;

    St = ValueToList(FGame->Mount0());
    if (St->Count() < 3) { delete St; return; }
    MountDir = IncludeTrailingPathDelimiter(MakeAbsPath((*St)[0], PrgSetup->BaseDir()));
    MountDriveLetter = (*St)[2];
    delete St;

    if (!(ExtUpperCase(Copy(FreeDOSDir, 1, Length(MountDir))) == ExtUpperCase(MountDir))) return;

    InternFreeDOSPath = MountDriveLetter + ":\\" + Copy(FreeDOSDir, Length(MountDir) + 1, MaxInt);
    InternFreeDOSPath.replace(PathDelimChar, QChar('\\'));  // path inside DOSBox

    St = StringToStringList(FGame->Environment());
    for (I = 0; I < St->Count(); I++) {
        if (Pos("=", (*St)[I]) == 0) continue;
        if (Trim(ExtUpperCase(Copy((*St)[I], 1, Pos("=", (*St)[I]) - 1))) != "PATH") continue;
        St2 = ValueToList(Trim(Copy((*St)[I], Pos("=", (*St)[I]) + 1, MaxInt)));
        Add = true;
        for (J = 0; J < St2->Count(); J++) if (ExtUpperCase((*St2)[J]) == ExtUpperCase(InternFreeDOSPath)) { Add = false; break; }
        delete St2;
        if (Add) {
            S = (*St)[I]; if ((!S.isEmpty()) && (S[Length(S) - 1] != ';')) S = S + ";";
            (*St)[I] = S + InternFreeDOSPath;
        }
    }
    FGame->SetEnvironment(StringListToString(St));
    delete St;
}

void TTempGame::SetSimpleDefaults()
{
    if (FGame == nullptr) return;
    FGame->SetProfileMode("DOSBox");
    FGame->SetAutoexec("");
    FGame->SetAutoexecOverridegamestart(true);
    FGame->SetAutoexecOverrideMount(false);
    FGame->SetAutoexecBootImage("");
    FGame->SetStartFullscreen(false);
    FGame->SetCustomDOSBoxDir("");
    FGame->SetCloseDosBoxAfterGameExit(true);
}

void RunDOSBoxCommandLineOnFolder(const QString& Folder)
{
    TTempGame* Temp;
    QString S, T;
    TStringList* St;

    Temp = new TTempGame();
    S = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    T = MakeAbsPath(Folder, PrgSetup->BaseDir());
    Temp->Game()->SetNrOfMounts(1);
    St = StringToStringList(Temp->Game()->Autoexec());
    St->Add("C:");
    if (Copy(ExtUpperCase(T), 1, Length(S)) == ExtUpperCase(S)) {
        Temp->Game()->SetMount0(MakeRelPath(PrgSetup->GameDir(), PrgSetup->BaseDir(), true) + ";Drive;C;false;");
        S = ShortName(S);
        T = ShortName(T);
        T = Copy(T, Length(S) + 1, MaxInt);
        T.replace(PathDelimChar, QChar('\\'));  // path inside DOSBox
        if ((!T.isEmpty()) && (T[Length(T) - 1] == '\\')) T = Copy(T, 1, Length(T) - 1);
        if ((!T.isEmpty()) && (T[0] == '\\')) T = Copy(T, 2, MaxInt);
        if (!T.isEmpty()) St->Add("cd " + T);
    } else {
        Temp->Game()->SetMount0(MakeRelPath(Folder, PrgSetup->BaseDir(), true) + ";Drive;C;false;");
    }
    Temp->Game()->SetAutoexec(StringListToString(St));
    delete St;
    RunWithCommandline(Temp->Game(), nullptr, "", true);
    delete Temp;
}
