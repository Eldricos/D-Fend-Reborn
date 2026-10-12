// C++ port of ZipManagerUnit.pas (implementation section).
//
// The archives themselves are extracted / repacked by ZipInfoFormUnit (which
// uses the external packer programs configured in the program settings).
// TTimer -> QTimer, WaitForSingleObject(DOSBoxHandle,0) -> IsProcessRunning(pid).
#include "ZipManagerUnit.h"

#include "Process.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "DosBoxUnit.h"
#include "MainUnit.h"
#include "ZipInfoFormUnit.h"

TZipManager* ZipManager = nullptr;

// THandle is a process id in the port (see Process.h).  The Delphi code uses
// INVALID_HANDLE_VALUE for "DOSBox not started yet" and gets INVALID_HANDLE_VALUE
// from RunDosBox if DOSBox could not be started.  A failed start returns 0 in
// the port, so "not started yet" needs a value of its own.
static const THandle NotActivatedHandle = -1;   // INVALID_HANDLE_VALUE (record not activated yet)
static const THandle InvalidHandle = 0;         // INVALID_HANDLE_VALUE (returned by a failed start)

static QWidget* ApplicationMainForm()
{
    return DFendReloadedMainForm;
}

// { TZipManager }

TZipManager::TZipManager()
{
    NrCount = -1;
    Timer = new QTimer();
    QObject::connect(Timer, &QTimer::timeout, Timer, [this]() { TimerWork(Timer); });
    Timer->setInterval(1000);
    Timer->start();
}

TZipManager::~TZipManager()
{
    delete Timer;
}

int TZipManager::GetCount()
{
    return List.size();
}

TZipRecord TZipManager::GetData(int I)
{
    return List[I];
}

bool TZipManager::CheckMountCommand(const QString& Command, QString& Folder, QString& ZipFile, int& DeleteType, QString& DriveLetter, bool& IsPhysFS)
{
    TStringList *St, *St2;
    QString S, T;
    int I;
    bool result = false;
    IsPhysFS = false;

    St = ValueToList(Command);
    St2 = new TStringList();
    try {
        do {
            S = Trim((*St)[0]);
            I = Pos("$", S);
            while (I > 0) { St2->Add(Trim(Copy(S, 1, I - 1))); S = Trim(Copy(S, I + 1, MaxInt)); I = Pos("$", S); }
            St2->Add(S);
            for (I = 0; I < St2->Count(); I++) (*St2)[I] = MakeAbsPath((*St2)[I], PrgSetup->BaseDir());
            S = (*St2)[0];

            // {general: RealFolder;Type;Letter;IO;Label;FreeSpace}
            if (St->Count() < 3) break;
            if (St->Count() == 3) St->Add("false");
            if (St->Count() == 4) St->Add("");
            if (St->Count() == 5) St->Add("");

            T = Trim(ExtUpperCase((*St)[1]));

            if (St2->Count() != 2) break;

            if (T == "ZIP") {
                // {RealFolder$ZipFile;ZIP;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)}
                result = true;
                DriveLetter = (*St)[2];
                Folder = ShortName((*St2)[0]);
                ZipFile = ShortName((*St2)[1]);
                DeleteType = 0;
                if (St->Count() >= 7) {
                    S = Trim(ExtUpperCase((*St)[6]));
                    if (S == "FILES") DeleteType = 1;
                    if (S == "FOLDER") DeleteType = 2;
                    if (S == "NO-NOREPACK") DeleteType = 3;
                    if (S == "FILES-NOREPACK") DeleteType = 4;
                    if (S == "FOLDER-NOREPACK") DeleteType = 5;
                }
            }

            if (T == "PHYSFS") {
                // {RealFolder$ZipFile;PHYSFS;Letter;False;;FreeSpace;DeleteMode(no;files;folder;no-norepack;files-norepack;folder-norepack)}

                DeleteType = 0;
                if (St->Count() >= 7) {
                    S = Trim(ExtUpperCase((*St)[6]));
                    if (S == "FILES") DeleteType = 1;
                    if (S == "FOLDER") DeleteType = 2;
                    if (S == "NO-NOREPACK") DeleteType = 3;
                    if (S == "FILES-NOREPACK") DeleteType = 4;
                    if (S == "FOLDER-NOREPACK") DeleteType = 5;
                }
                if (DeleteType == 0) break; // {no extracting needed, only add a record if repack is needed}

                IsPhysFS = true;
                result = true;
                DriveLetter = (*St)[2];
                Folder = ShortName((*St2)[0]);
                ZipFile = ShortName((*St2)[1]);
            }
        } while (false);
    } catch (...) {
        delete St;
        delete St2;
        throw;
    }
    delete St;
    delete St2;
    return result;
}

int TZipManager::AddGame(TGame* Game, bool& Error)
{
    if (ScummVMMode(Game))
        return AddGameScummVM(Game, Error);
    else
        return AddGameDOSBox(Game, Error);
}

int TZipManager::AddGameDOSBox(TGame* Game, bool& Error)
{
    int I, J, K, Nr, DeleteType = 0;
    QString S, Folder, ZipFile, DriveLetter;
    bool IsPhysFS;

    Error = false;
    int result = -1;

    Nr = -1;
    for (I = 0; I <= 9; I++) {
        S = Game->Mount(I);
        if (S.isEmpty()) continue;
        if (CheckMountCommand(S, Folder, ZipFile, DeleteType, DriveLetter, IsPhysFS)) {
            if (Nr < 0) {
                NrCount++;
                Nr = List.size();
                List.append(TZipRecord());
                List[Nr].Nr = NrCount;
                List[Nr].DOSBoxHandle = NotActivatedHandle;
                List[Nr].IsScummVM = false;
                List[Nr].InavlidDOSBox = false;
                List[Nr].ZipDrives = 0;
            }

            for (J = 0; J <= List.size() - 2; J++) for (K = 0; K < List[J].ZipDrives; K++) if (Trim(ExtUpperCase(MakeRelPath(List[J].ZipFile[K], PrgSetup->BaseDir()))) == Trim(ExtUpperCase(MakeRelPath(ZipFile, PrgSetup->BaseDir())))) {
                MessageDlg(Format(LanguageSetup->ZipFormZipFileInUseWarning(), {ZipFile}), mtError, mbOK, 0);
                Error = true; NrCount--; List.removeLast(); return result;
            }

            int D = List[Nr].ZipDrives;
            List[Nr].Folder[D] = Folder;
            List[Nr].ZipFile[D] = ZipFile;
            List[Nr].DeleteType[D] = DeleteType;
            List[Nr].DriveLetter[D] = DriveLetter;
            List[Nr].OnlyRepack[D] = IsPhysFS;
            List[Nr].ZipDrives++;
        }
    }

    if (Nr >= 0) {
        result = List[Nr].Nr;
        for (I = 0; I < List[Nr].ZipDrives; I++) if (!List[Nr].OnlyRepack[I]) {
            if (!UnpackData(List[Nr].Folder[I], List[Nr].ZipFile[I])) {
                Error = true; NrCount--; List.removeLast(); return result;
            }
        }
    }
    return result;
}

int TZipManager::AddGameScummVM(TGame* Game, bool& Error)
{
    QString Zip, Folder;
    int Nr, J, K;

    Error = false;
    int result = -1;

    if (Trim(Game->ScummVMZip()).isEmpty()) return result;
    Zip = MakeAbsPath(Game->ScummVMZip(), PrgSetup->BaseDir());
    Folder = MakeAbsPath(Game->ScummVMPath(), PrgSetup->BaseDir());

    Error = !UnpackData(Folder, Zip); if (Error) return result;

    NrCount++;
    Nr = List.size();
    List.append(TZipRecord());
    List[Nr].Nr = NrCount;
    List[Nr].DOSBoxHandle = NotActivatedHandle;
    List[Nr].IsScummVM = true;
    List[Nr].InavlidDOSBox = false;
    List[Nr].ZipDrives = 1;

    for (J = 0; J <= List.size() - 2; J++) for (K = 0; K < List[J].ZipDrives; K++) if (Trim(ExtUpperCase(MakeRelPath(List[J].ZipFile[K], PrgSetup->BaseDir()))) == Trim(ExtUpperCase(Zip))) {
        MessageDlg(Format(LanguageSetup->ZipFormZipFileInUseWarning(), {Zip}), mtError, mbOK, 0);
        Error = true; NrCount--; List.removeLast(); return result;
    }

    List[Nr].Folder[0] = Folder;
    List[Nr].ZipFile[0] = Zip;
    List[Nr].DeleteType[0] = 2; // {2=Delete whole folder}
    List[Nr].DriveLetter[0] = "";
    List[Nr].OnlyRepack[List[Nr].ZipDrives] = false;

    result = List[Nr].Nr;
    return result;
}

bool TZipManager::UnpackData(const QString& Folder, const QString& ZipFile)
{
    QString AZipFile, ADestFolder;
    AZipFile = MakeAbsPath(ZipFile, PrgSetup->BaseDir());
    ADestFolder = MakeAbsPath(Folder, PrgSetup->BaseDir());
    return ExtractZipFile(ApplicationMainForm(), AZipFile, ADestFolder);
}

void TZipManager::ActivateRepackCheck(int Nr, THandle DOSBoxHandle)
{
    int I;
    THandle NewHandle;

    for (I = 0; I < List.size(); I++) if (List[I].Nr == Nr) {
        if (DOSBoxHandle == InvalidHandle || DOSBoxHandle == NotActivatedHandle) {
            List[I].DOSBoxHandle = 0;
            List[I].InavlidDOSBox = true;
        } else {
            NewHandle = DOSBoxHandle;  // DuplicateHandle -> the process id can be used directly
            List[I].DOSBoxHandle = NewHandle;
        }
        break;
    }

    if (FOnInfoBarNotify) FOnInfoBarNotify(nullptr);
}

void TZipManager::RepackData(const QString& Folder, const QString& ZipFile, int DeleteType, bool AddToZipFile)
{
    QString AZipFile, ADestFolder;
    TDeleteMode ADeleteMode;

    AZipFile = MakeAbsPath(ZipFile, PrgSetup->BaseDir());
    ADestFolder = MakeAbsPath(Folder, PrgSetup->BaseDir());
    switch (DeleteType) {
        case 0: case 3: ADeleteMode = dmNoNoWarning; break;
        case 1: case 4: ADeleteMode = dmFilesNoWarning; break;
        case 2: case 5: ADeleteMode = dmFolderNoWarning; break;
        default: ADeleteMode = dmNoNoWarning; break;
    }

    if (DeleteType >= 3) {
        // {Delete only}
        DeleteUnpackedFiles(ApplicationMainForm(), ADestFolder, ADeleteMode);
    } else {
        // {Repack and delete}
        if (AddToZipFile)
            ::AddToZipFile(ApplicationMainForm(), AZipFile, ADestFolder, ADeleteMode, GetCompressStrengthFromPrgSetup());
        else
            CreateZipFile(ApplicationMainForm(), AZipFile, ADestFolder, ADeleteMode, GetCompressStrengthFromPrgSetup());
    }
}

void TZipManager::TimerWork(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, J;
    Timer->stop();
    try {
        I = 0;
        while (I < List.size()) {
            if ((List[I].DOSBoxHandle != NotActivatedHandle) && (List[I].InavlidDOSBox || (!IsProcessRunning(List[I].DOSBoxHandle)))) {
                TZipRecord R = List[I];
                // The Delphi code reads List[J].ZipFile[J], List[J].DeleteType[J] and
                // List[J].OnlyRepack[J] here (index error, only correct for the first
                // record); the port uses the record that is processed.
                for (J = 0; J < R.ZipDrives; J++) RepackData(R.Folder[J], R.ZipFile[J], R.DeleteType[J], R.OnlyRepack[J]);
                List.removeAt(I);
                continue;
            }
            I++;
        }

        if ((List.size() == 0) && FOnInfoBarNotify) FOnInfoBarNotify(nullptr);
    } catch (...) {
        Timer->start();
        throw;
    }
    Timer->start();
}

void ZipManagerUnit_Initialization()
{
    ZipManager = new TZipManager();
}

void ZipManagerUnit_Finalization()
{
    delete ZipManager;
    ZipManager = nullptr;
}
