// C++ port of ScummVMToolsUnit.pas (implementation section).
#include "ScummVMToolsUnit.h"

#include "IniFiles.h"
#include "PrgConsts.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"

TScummVMGamesList* ScummVMGamesList = nullptr;

// { TScummVMGamesList }

TScummVMGamesList::TScummVMGamesList()
{
    FName = new TStringList();
    FLongName = new TStringList();
    LoadConfig();
    FScummVMIniFile = "";
}

TScummVMGamesList::~TScummVMGamesList()
{
    SaveConfig();
    delete FName;
    delete FLongName;
}

void TScummVMGamesList::LoadConfig()
{
    TIniFile* Ini;
    int I;

    Ini = new TIniFile(PrgDataDir() + SettingsFolder + PathDelim + ScummVMConfOptFile);
    Ini->ReadSections(*FName);
    for (I = 0; I < FName->Count(); I++) FLongName->Add(Ini->ReadString((*FName)[I], "Description", ""));
    delete Ini;
}

void TScummVMGamesList::SaveConfig()
{
    TIniFile* Ini;
    int I;

    if (FName->Count() == 0) {
        ExtDeleteFile(PrgDataDir() + SettingsFolder + PathDelim + ScummVMConfOptFile, ftProfile);
        return;
    }

    Ini = new TIniFile(PrgDataDir() + SettingsFolder + PathDelim + ScummVMConfOptFile);
    for (I = 0; I < FName->Count(); I++) Ini->WriteString((*FName)[I], "Description", (*FLongName)[I]);
    delete Ini;
}

int TScummVMGamesList::GetCount()
{
    return FName->Count();
}

QString TScummVMGamesList::GetScummVMIniFile()
{
    QString result = FScummVMIniFile;
    if (!result.isEmpty()) return result;
    LoadListFromScummVM(false);
    result = FScummVMIniFile;
    return result;
}

bool TScummVMGamesList::LoadListFromScummVM(bool WarnIfScummVMNotFound, const QString& CustomScummVMPath)
{
    QString S;

    FName->Clear();
    FLongName->Clear();

    if (!Trim(CustomScummVMPath).isEmpty()) {
        S = IncludeTrailingPathDelimiter(Trim(CustomScummVMPath)) + ScummPrgFile;
    } else {
        S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile;
    }
#ifndef Q_OS_WIN
    // TODO(port): on non Windows systems the ScummVM binary has no ".exe" extension
    if (!FileExists(S)) {
        QString T = ExtractFilePath(S) + "scummvm";
        if (FileExists(T)) S = T;
    }
#endif
    if (!FileExists(S)) {
        if (WarnIfScummVMNotFound) MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {S}), mtError, mbOK, 0);
        return false;
    }

    return LoadListFromScummVMFile(S);
}

bool TScummVMGamesList::LoadListFromScummVMFile(const QString& ScummVMPrgFile)
{
    TStringList* St;
    bool result = false;

    St = RunAndGetOutput(ScummVMPrgFile, "-z", true);
    if (St == nullptr) return result;
    result = LoadListFromScummVMStringList(St);
    delete St;
    return result;
}

bool TScummVMGamesList::LoadListFromScummVMStringList(TStringList* St)
{
    int I, Mode, J, K;
    Mode = 0; J = 10;
    for (I = 0; I < St->Count(); I++) switch (Mode) {
        case 0:
            if (I == 0) {
                K = Pos(":\\", (*St)[I]);
                if (K > 0) FScummVMIniFile = Copy((*St)[I], K - 1, MaxInt);
#ifndef Q_OS_WIN
                // TODO(port): absolute paths on non Windows systems start with '/'
                if (K == 0) {
                    K = Pos(": /", (*St)[I]);
                    if (K > 0) FScummVMIniFile = Trim(Copy((*St)[I], K + 2, MaxInt));
                }
#endif
            }
            if (Copy((*St)[I], 1, 3) == "---") { J = Pos(" ", (*St)[I]); Mode = 1; }
            break;
        case 1:
            FName->Add(Trim(Copy((*St)[I], 1, J - 1))); FLongName->Add(Trim(Copy((*St)[I], J + 1, MaxInt)));
            break;
    }
    return (Mode == 1);
}

QString TScummVMGamesList::NameFromDescription(const QString& Description)
{
    QString result = "";
    int I = FLongName->IndexOf(Description);
    if (I >= 0) result = (*FName)[I];
    return result;
}
