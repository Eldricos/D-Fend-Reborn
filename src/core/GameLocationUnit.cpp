#include "GameLocationUnit.h"

#include <QDir>
#include <QFileInfo>
#include "CommonTools.h"
#include "GameDBToolsUnit.h"
#include "GameDBUnit.h"
#include "PrgSetupUnit.h"

namespace {

constexpr int MaxMounts = 10;
constexpr int MaxExtraPrgFiles = 50;

QString AbsPath(const QString& Path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(MakeAbsPath(Trim(Path), PrgSetup->BaseDir())));
}

bool SamePrefix(const QString& Path, const QString& Prefix)
{
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity CS = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity CS = Qt::CaseSensitive;
#endif
    return (Path.compare(Prefix, CS) == 0) || Path.startsWith(Prefix + QLatin1Char('/'), CS);
}

// Path (as stored in the profile) moved from OldDir to NewDir
QString Moved(const QString& Path, const QString& OldDir, const QString& NewDir)
{
    if (Trim(Path).isEmpty()) return Path;
    const QString A = AbsPath(Path);
    if (!SamePrefix(A, OldDir)) return Path;
    QString R = MakeRelPath(QDir::toNativeSeparators(NewDir + A.mid(OldDir.length())), PrgSetup->BaseDir(), true);
    // (folders keep their trailing path delimiter)
    if (Path.endsWith(QLatin1Char('\\')) || Path.endsWith(QLatin1Char('/'))) R = IncludeTrailingPathDelimiter(R);
    return R;
}

// "RealFolder;Type;Letter;..." (folders / files of the mount, "$" separated)
QStringList MountPaths(const QString& Mount, bool& IsFolder)
{
    const QStringList F = Mount.split(QLatin1Char(';'));
    IsFolder = false;
    if (F.size() < 3) return {};
    const QString T = F[1].trimmed().toUpper();
    const QStringList P = F[0].split(QLatin1Char('$'));
    if ((T == QLatin1String("DRIVE")) || (T == QLatin1String("FLOPPY"))) { IsFolder = true; return {P[0]}; }
    if (T == QLatin1String("CDROM")) {
        // (not physical CD drives: "D:\", ASK, NUMBER:x, LABEL:x, ...)
        const QString S = P[0].trimmed();
        if (S.contains(QLatin1Char(':')) && (QDir::cleanPath(QDir::fromNativeSeparators(S)).length() <= 3)) return {};
        if (S.contains(QLatin1String("ASK"), Qt::CaseInsensitive) && !S.contains(QLatin1Char('/')) && !S.contains(QLatin1Char('\\'))) return {};
        if (S.startsWith(QLatin1String("NUMBER:"), Qt::CaseInsensitive) || S.startsWith(QLatin1String("LABEL:"), Qt::CaseInsensitive)) return {};
        IsFolder = true;
        return {P[0]};
    }
    if ((T == QLatin1String("CDROMIMAGE")) || (T == QLatin1String("FLOPPYIMAGE")) || (T == QLatin1String("IMAGE"))) return P;
    if (((T == QLatin1String("ZIP")) || (T == QLatin1String("PHYSFS"))) && (P.size() >= 2)) return {P[1]};
    return {};
}

}  // namespace

QStringList GamePaths(TGame* Game)
{
    QStringList R;
    auto Add = [&R](const QString& P) { if (!Trim(P).isEmpty()) R << AbsPath(P); };
    if (ScummVMMode(Game)) {
        Add(Game->ScummVMPath());
        return R;
    }
    const QString Exe = Trim(Game->GameExe());
    if (!Exe.startsWith(QLatin1String("DOSBOX:"), Qt::CaseInsensitive)) Add(Exe);
    const QString Setup = Trim(Game->SetupExe());
    if (!Setup.startsWith(QLatin1String("DOSBOX:"), Qt::CaseInsensitive)) Add(Setup);
    if (WindowsExeMode(Game)) return R;
    for (int I = 0; I < qMin(Game->NrOfMounts(), MaxMounts); I++) {
        bool IsFolder;
        for (const QString& P : MountPaths(Game->Mount(I), IsFolder)) Add(P);
    }
    return R;
}

QStringList MissingGamePaths(TGame* Game)
{
    QStringList R;
    for (const QString& P : GamePaths(Game)) if (!QFileInfo::exists(P)) R << P;
    return R;
}

QStringList MissingDrivePaths(TGame* Game)
{
    QStringList R;
    if (ScummVMMode(Game) || WindowsExeMode(Game)) return R;
    for (int I = 0; I < qMin(Game->NrOfMounts(), MaxMounts); I++) {
        bool IsFolder;
        const QStringList F = Game->Mount(I).split(QLatin1Char(';'));
        for (const QString& P : MountPaths(Game->Mount(I), IsFolder))
            if (!Trim(P).isEmpty() && !QFileInfo::exists(AbsPath(P))) R << F.value(2).trimmed() + QLatin1Char(';') + AbsPath(P);
    }
    return R;
}

bool RelocateGame(TGame* Game, const QString& Folder)
{
    const QStringList Missing = MissingGamePaths(Game);
    if (Missing.isEmpty()) return true;

    // folders the game folder may be in now: the chosen folder, its Steam
    // library folder (steamapps/common) and its subfolders
    const QString Root = QDir::cleanPath(QDir::fromNativeSeparators(Folder));
    QStringList Bases = {Root};
    for (const QString& Rel : {QStringLiteral("steamapps/common"), QStringLiteral("common")})
        if (QFileInfo(Root + QLatin1Char('/') + Rel).isDir()) Bases << Root + QLatin1Char('/') + Rel;
    for (const QFileInfo& Sub : QDir(Root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) Bases << QDir::cleanPath(Sub.absoluteFilePath());

    // the old and the new folder: the longest end of a missing path found below one of these folders
    const QStringList Parts = Missing[0].split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QString Lead = Missing[0].startsWith(QLatin1Char('/')) ? QStringLiteral("/") : QString();
    QString OldDir, NewDir;
    for (int Keep = Parts.size() - 1; (Keep >= 1) && OldDir.isEmpty(); Keep--) {
        const QString Tail = Parts.mid(Parts.size() - Keep).join(QLatin1Char('/'));
        const QString Ancestor = Lead + Parts.mid(0, Parts.size() - Keep).join(QLatin1Char('/'));
        const QString AncestorName = Parts[Parts.size() - Keep - 1];
        for (const QString& B : Bases) {
            if (QFileInfo::exists(B + QLatin1Char('/') + AncestorName + QLatin1Char('/') + Tail)) { OldDir = Ancestor; NewDir = B + QLatin1Char('/') + AncestorName; break; }
            if (QFileInfo::exists(B + QLatin1Char('/') + Tail)) { OldDir = Ancestor; NewDir = B; break; }
        }
    }
    if (OldDir.isEmpty()) return false;

    // all paths of the profile below the old folder (restored if the game is still incomplete)
    const QString GameExe = Game->GameExe(), SetupExe = Game->SetupExe(), ScummVMPath = Game->ScummVMPath(), ExtraPath = Game->ScummVMExtraPath();
    const QString Icon = Game->Icon(), DataDir = Game->DataDir(), ExtraFiles = Game->ExtraFiles(), ExtraDirs = Game->ExtraDirs();
    QStringList Mounts, ExtraPrg;
    for (int I = 0; I < MaxMounts; I++) Mounts << Game->Mount(I);
    for (int I = 0; I < MaxExtraPrgFiles; I++) ExtraPrg << Game->ExtraPrgFile(I);

    auto MovedList = [&](const QString& Value) {
        TStringList* St = ValueToList(Value);
        for (int I = 0; I < St->Count(); I++) (*St)[I] = Moved((*St)[I], OldDir, NewDir);
        const QString R = ListToValue(St);
        delete St;
        return R;
    };
    Game->SetGameExe(Moved(GameExe, OldDir, NewDir));
    Game->SetSetupExe(Moved(SetupExe, OldDir, NewDir));
    Game->SetScummVMPath(Moved(ScummVMPath, OldDir, NewDir));
    Game->SetScummVMExtraPath(Moved(ExtraPath, OldDir, NewDir));
    if (!ExtractFilePath(Icon).isEmpty()) Game->SetIcon(Moved(Icon, OldDir, NewDir));
    Game->SetDataDir(Moved(DataDir, OldDir, NewDir));
    if (!Trim(ExtraFiles).isEmpty()) Game->SetExtraFiles(MovedList(ExtraFiles));
    if (!Trim(ExtraDirs).isEmpty()) Game->SetExtraDirs(MovedList(ExtraDirs));
    for (int I = 0; I < MaxMounts; I++) {
        if (Trim(Mounts[I]).isEmpty()) continue;
        QStringList F = Mounts[I].split(QLatin1Char(';'));
        QStringList P = F[0].split(QLatin1Char('$'));
        for (QString& S : P) S = Moved(S, OldDir, NewDir);
        F[0] = P.join(QLatin1Char('$'));
        Game->SetMount(I, F.join(QLatin1Char(';')));
    }
    for (int I = 0; I < MaxExtraPrgFiles; I++) if (!Trim(ExtraPrg[I]).isEmpty()) Game->SetExtraPrgFile(I, Moved(ExtraPrg[I], OldDir, NewDir));

    if (MissingGamePaths(Game).isEmpty()) {
        Game->StoreAllValues();
        return true;
    }
    Game->SetGameExe(GameExe); Game->SetSetupExe(SetupExe); Game->SetScummVMPath(ScummVMPath); Game->SetScummVMExtraPath(ExtraPath);
    Game->SetIcon(Icon); Game->SetDataDir(DataDir); Game->SetExtraFiles(ExtraFiles); Game->SetExtraDirs(ExtraDirs);
    for (int I = 0; I < MaxMounts; I++) if (!Trim(Mounts[I]).isEmpty()) Game->SetMount(I, Mounts[I]);
    for (int I = 0; I < MaxExtraPrgFiles; I++) if (!Trim(ExtraPrg[I]).isEmpty()) Game->SetExtraPrgFile(I, ExtraPrg[I]);
    return false;
}
