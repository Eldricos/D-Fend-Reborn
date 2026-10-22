// C++ port of ScummVMToolsUnit.pas (implementation section).
#include "ScummVMToolsUnit.h"

#include "IniFiles.h"
#include "PrgConsts.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"

#include <QRegularExpression>
#include <memory>

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
    if (!FileExists(S)) {
        if (WarnIfScummVMNotFound) {
            if (Trim(CustomScummVMPath).isEmpty() && Trim(PrgSetup->ScummVMPath()).isEmpty())
                MessageDlg(LanguageSetup->MessageScummVMNotConfigured(), mtError, mbOK, 0);
            else
                MessageDlg(Format(LanguageSetup->MessageFileNotFound(), {S}), mtError, mbOK, 0);
        }
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
    FName->Clear();
    FLongName->Clear();
    Mode = 0; J = 10;
    for (I = 0; I < St->Count(); I++) switch (Mode) {
        case 0:
            if (I == 0) {
                K = Pos(":\\", (*St)[I]);
                if (K > 0) FScummVMIniFile = Copy((*St)[I], K - 1, MaxInt);
#ifndef Q_OS_WIN
                // Port note: absolute paths on non Windows systems start with '/'
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

QString TScummVMGamesList::NameFromDetectedID(const QString& ID)
{
    const QString S = Trim(LowerCase(ID));
    if (S.isEmpty()) return QString();
    if (FName->IndexOf(S) >= 0) return (*FName)[FName->IndexOf(S)];
    // the list and the detection may differ in the engine prefix ("engine:game" / "game")
    auto GamePart = [](const QString& X) { return X.mid(X.indexOf(':') + 1); };
    for (int I = 0; I < FName->Count(); I++) if (GamePart(LowerCase((*FName)[I])) == GamePart(S)) return (*FName)[I];
    return QString();
}

QString TScummVMGamesList::DescriptionFromName(const QString& Name)
{
    const int I = FName->IndexOf(Name);
    return (I >= 0) ? (*FLongName)[I] : QString();
}

QList<TScummVMDetectedGame> ParseScummVMDetectOutput(const QStringList& Lines)
{
    // GameID        Description          Full Path
    // ------------- -------------------- ----------
    // sky:sky       Beneath a Steel Sky  /games/BASS
    QList<TScummVMDetectedGame> Result;
    QList<int> Start;   // column starts (from the line of dashes)
    for (const QString& Line : Lines) {
        if (Start.isEmpty()) {
            const QString T = Line.trimmed();
            if (!T.startsWith(QLatin1String("---")) || T.contains(QRegularExpression("[^- ]"))) continue;
            for (int I = 0; I < Line.length(); I++) if ((Line[I] == '-') && ((I == 0) || (Line[I - 1] == ' '))) Start << I;
            if (Start.size() < 2) Start.clear();
            continue;
        }
        if (Line.trimmed().isEmpty() || Line.startsWith(QLatin1String("WARNING"))) continue;
        auto Column = [&](int Nr) {
            const int From = Start[Nr];
            return (Nr + 1 < Start.size()) ? Line.mid(From, Start[Nr + 1] - From).trimmed() : Line.mid(From).trimmed();
        };
        TScummVMDetectedGame G;
        G.ID = Column(0);
        G.Description = Column(1);
        if (Start.size() > 2) G.Path = Column(2);
        if (!G.ID.isEmpty() && !G.ID.contains(' ')) Result << G;
    }
    return Result;
}

QList<TScummVMDetectedGame> DetectScummVMGames(const QString& Folder, const QString& ScummVMPath)
{
    const QString Prg = IncludeTrailingPathDelimiter(Trim(ScummVMPath).isEmpty() ? PrgSetup->ScummVMPath() : Trim(ScummVMPath)) + ScummPrgFile;
    const QString Dir = ExcludeTrailingPathDelimiter(MakeAbsPath(Folder, PrgSetup->BaseDir()));
    if (Trim(Folder).isEmpty() || !FileExists(Prg) || !DirectoryExists(Dir)) return {};
    std::unique_ptr<TStringList> St(RunAndGetOutput(Prg, "--detect \"--path=" + QDir::toNativeSeparators(Dir) + "\"", true));
    if (!St) return {};
    return ParseScummVMDetectOutput(St->ToQStringList());
}
