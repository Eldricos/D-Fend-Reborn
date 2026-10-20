#include "GOGImportUnit.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "Classes.h"
#include "CommonTools.h"
#include "GameDBToolsUnit.h"
#include "GameDBUnit.h"
#include "PrgSetupUnit.h"
#include "ScummVMToolsUnit.h"

namespace {

// Path below Base (a path of a conf file: "\" as separator, any case) of an
// existing file or folder; empty if there is none
QString FindPath(const QString& Base, const QString& Rel)
{
    QString R = Rel;
    R.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString P = QDir::cleanPath(QDir(Base).absoluteFilePath(R));
    if (QFileInfo::exists(P)) return P;
#ifndef Q_OS_WIN
    // (case-insensitive like DOS and Windows: conf files of the Windows versions)
    if (!P.startsWith(QLatin1Char('/'))) return QString();
    QString Cur = QStringLiteral("/");
    for (const QString& Part : P.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        const QString Next = (Cur == QLatin1String("/")) ? Cur + Part : Cur + QLatin1Char('/') + Part;
        if (QFileInfo::exists(Next)) { Cur = Next; continue; }
        QString Found;
        for (const QString& E : QDir(Cur).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
            if (E.compare(Part, Qt::CaseInsensitive) == 0) { Found = E; break; }
        if (Found.isEmpty()) return QString();
        Cur = (Cur == QLatin1String("/")) ? Cur + Found : Cur + QLatin1Char('/') + Found;
    }
    return Cur;
#else
    return QString();
#endif
}

// Command line parameters (quotes: one parameter, quotes removed)
QStringList SplitParameters(const QString& S)
{
    QStringList R;
    QString Cur;
    bool InQuotes = false, Has = false;
    for (const QChar C : S) {
        if (C == QLatin1Char('"')) { InQuotes = !InQuotes; Has = true; continue; }
        if (C.isSpace() && !InQuotes) { if (Has) { R << Cur; Cur.clear(); Has = false; } continue; }
        Cur += C; Has = true;
    }
    if (Has) R << Cur;
    return R;
}

QStringList ConfsOfArguments(const QString& Arguments, const QString& WorkingDir)
{
    QStringList R;
    const QStringList P = SplitParameters(Arguments);
    for (int I = 0; I + 1 < P.size(); I++) {
        if (P[I].compare(QLatin1String("-conf"), Qt::CaseInsensitive) != 0) continue;
        const QString F = FindPath(WorkingDir, P[I + 1]);
        if (!F.isEmpty() && QFileInfo(F).isFile()) R << F;
    }
    return R;
}

QString FindSubFolder(const QString& Folder, const QString& Name)
{
    for (const QString& E : QDir(Folder).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (E.compare(Name, Qt::CaseInsensitive) == 0) return QDir(Folder).filePath(E);
    return QString();
}

bool IsDOSBoxTask(const QJsonObject& T)
{
    QString P = T.value(QLatin1String("path")).toString();
    P.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QFileInfo(P).fileName().startsWith(QLatin1String("dosbox"), Qt::CaseInsensitive);
}

bool IsScummVMTask(const QJsonObject& T)
{
    QString P = T.value(QLatin1String("path")).toString();
    P.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QFileInfo(P).fileName().startsWith(QLatin1String("scummvm"), Qt::CaseInsensitive);
}

// first existing path of Rel below one of the folders
QString FindPathIn(const QStringList& Bases, const QString& Rel)
{
    for (const QString& B : Bases) {
        if (B.isEmpty()) continue;
        const QString P = FindPath(B, Rel);
        if (!P.isEmpty()) return P;
    }
    return QString();
}

// Game of a ScummVM ini file (section Target or the only game section);
// relative paths: as ScummVM sees them (working folder) or below the game folder
bool ReadScummVMIni(const QString& IniFile, const QString& Target, const QString& WorkingDir, TGOGGame& Game)
{
    TStringList St;
    if (!St.LoadFromFile(IniFile)) return false;
    QMap<QString, QMap<QString, QString>> Sections;
    QStringList Order;
    QString Section;
    for (int I = 0; I < St.Count(); I++) {
        const QString S = St[I].trimmed();
        if (S.isEmpty() || S.startsWith(QLatin1Char('#')) || S.startsWith(QLatin1Char(';'))) continue;
        if (S.startsWith(QLatin1Char('[')) && S.endsWith(QLatin1Char(']'))) { Section = S.mid(1, S.length() - 2).trimmed().toLower(); if (!Order.contains(Section)) Order << Section; continue; }
        const int P = S.indexOf(QLatin1Char('='));
        if ((P > 0) && !Section.isEmpty()) Sections[Section].insert(S.left(P).trimmed().toLower(), S.mid(P + 1).trimmed());
    }
    QString Sec = Target.toLower();
    if (!Sections.contains(Sec) || !Sections[Sec].contains(QLatin1String("gameid"))) {
        Sec.clear();
        for (const QString& O : Order) {
            if ((O == QLatin1String("scummvm")) || !Sections[O].contains(QLatin1String("gameid"))) continue;
            if (!Sec.isEmpty()) return false;   // (several games: which one?)
            Sec = O;
        }
        if (Sec.isEmpty()) return false;
    }
    const QMap<QString, QString> K = Sections[Sec];
    const QString GameID = K.value(QLatin1String("gameid")), EngineID = K.value(QLatin1String("engineid"));
    if (GameID.isEmpty()) return false;
    Game.ScummVM = true;
    Game.ScummVMGameID = EngineID.isEmpty() ? GameID : EngineID + QLatin1Char(':') + GameID;
    const QStringList Bases = {WorkingDir, Game.Folder, QFileInfo(IniFile).absolutePath()};
    const QString Path = K.value(QLatin1String("path"));
    if (Game.ScummVMDataDir.isEmpty()) Game.ScummVMDataDir = Path.isEmpty() ? Game.Folder : FindPathIn(Bases, Path);
    if (!K.value(QLatin1String("extrapath")).isEmpty()) Game.ScummVMExtraPath = FindPathIn(Bases, K.value(QLatin1String("extrapath")));
    Game.ScummVMLanguage = K.value(QLatin1String("language"));
    Game.ScummVMPlatform = K.value(QLatin1String("platform"));
    if (Game.Name.isEmpty()) Game.Name = K.value(QLatin1String("description"));
    return !Game.ScummVMDataDir.isEmpty();
}

// ScummVM start entry: "-c <ini>" / "--config=<ini>", "-p <path>" / "--path=<path>", game name
bool ReadScummVMTask(const QString& Arguments, const QString& WorkingDir, TGOGGame& Game)
{
    const QStringList P = SplitParameters(Arguments);
    QString Ini, Path, Target;
    for (int I = 0; I < P.size(); I++) {
        const QString A = P[I];
        if (((A == QLatin1String("-c")) || (A == QLatin1String("-p"))) && (I + 1 < P.size())) { (A == QLatin1String("-c") ? Ini : Path) = P[++I]; continue; }
        if (A.startsWith(QLatin1String("--config="))) { Ini = A.mid(9); continue; }
        if (A.startsWith(QLatin1String("--path="))) { Path = A.mid(7); continue; }
        if (!A.startsWith(QLatin1Char('-'))) Target = A;
    }
    const QStringList Bases = {WorkingDir, Game.Folder};
    if (!Path.isEmpty()) Game.ScummVMDataDir = FindPathIn(Bases, Path);
    if (!Ini.isEmpty()) {
        const QString IniFile = FindPathIn(Bases, Ini);
        if (!IniFile.isEmpty() && ReadScummVMIni(IniFile, Target, WorkingDir, Game)) return true;
    }
    // (no ini file: the game name is the game ID)
    if (Target.isEmpty() || Game.ScummVMDataDir.isEmpty()) return false;
    Game.ScummVM = true;
    Game.ScummVMGameID = Target;
    return true;
}

// goggame-<ID>.info (Windows versions)
bool ReadInfoFile(const QString& Folder, TGOGGame& Game)
{
    const QStringList Infos = QDir(Folder).entryList({QStringLiteral("goggame-*.info")}, QDir::Files, QDir::Name);
    for (const QString& InfoName : Infos) {
        QFile F(QDir(Folder).filePath(InfoName));
        if (!F.open(QIODevice::ReadOnly)) continue;
        const QJsonObject Info = QJsonDocument::fromJson(F.readAll()).object();
        const QJsonArray Tasks = Info.value(QLatin1String("playTasks")).toArray();
        // game: the primary (or first) DOSBox task of the category "game";
        // setup: another DOSBox task (category "tool" or "setup" / "settings" in its name)
        QList<QJsonObject> DOSBoxTasks;
        for (const QJsonValue& V : Tasks) if (IsDOSBoxTask(V.toObject())) DOSBoxTasks << V.toObject();
        auto IsGame = [](const QJsonObject& T) { return T.value(QLatin1String("category")).toString() == QLatin1String("game"); };
        QJsonObject GameTask, SetupTask;
        for (const QJsonObject& T : DOSBoxTasks) if (IsGame(T) && T.value(QLatin1String("isPrimary")).toBool()) { GameTask = T; break; }
        if (GameTask.isEmpty()) for (const QJsonObject& T : DOSBoxTasks) if (IsGame(T)) { GameTask = T; break; }
        for (const QJsonObject& T : DOSBoxTasks) {
            if (T == GameTask) continue;
            const QString Name = T.value(QLatin1String("name")).toString().toLower();
            if (!IsGame(T) || Name.contains(QLatin1String("setup")) || Name.contains(QLatin1String("settings")) || Name.contains(QLatin1String("config"))) { SetupTask = T; break; }
        }
        auto SetInfo = [&]() {
            Game.Name = Info.value(QLatin1String("name")).toString().trimmed();
            Game.ID = Info.value(QLatin1String("gameId")).toString();
            if (Game.ID.isEmpty()) Game.ID = InfoName.mid(8, InfoName.length() - 13);
            const QString Icon = QDir(Folder).filePath(QStringLiteral("goggame-") + Game.ID + QStringLiteral(".ico"));
            if (QFileInfo::exists(Icon)) Game.Icon = QDir::cleanPath(Icon);
        };
        if (GameTask.isEmpty()) {
            // a game shipped with ScummVM?
            QJsonObject ScummVMTask;
            for (const QJsonValue& V : Tasks) {
                const QJsonObject T = V.toObject();
                if (!IsScummVMTask(T) || !IsGame(T)) continue;
                if (ScummVMTask.isEmpty() || T.value(QLatin1String("isPrimary")).toBool()) ScummVMTask = T;
            }
            if (ScummVMTask.isEmpty()) continue;
            const QString WD = ScummVMTask.value(QLatin1String("workingDir")).toString();
            const QString WorkingDir = WD.isEmpty() ? QDir::cleanPath(Folder) : FindPath(Folder, WD);
            Game.WorkingDir = WorkingDir.isEmpty() ? QDir::cleanPath(Folder) : WorkingDir;
            SetInfo();
            if (ReadScummVMTask(ScummVMTask.value(QLatin1String("arguments")).toString(), Game.WorkingDir, Game)) return true;
            Game = TGOGGame();
            Game.Folder = QDir::cleanPath(Folder);
            continue;
        }
        QString WD = GameTask.value(QLatin1String("workingDir")).toString();
        const QString WorkingDir = WD.isEmpty() ? QDir::cleanPath(Folder) : FindPath(Folder, WD);
        if (WorkingDir.isEmpty()) continue;
        const QStringList Confs = ConfsOfArguments(GameTask.value(QLatin1String("arguments")).toString(), WorkingDir);
        if (Confs.isEmpty()) continue;
        Game.WorkingDir = WorkingDir;
        Game.GameConfs = Confs;
        if (!SetupTask.isEmpty()) {
            WD = SetupTask.value(QLatin1String("workingDir")).toString();
            const QString SetupDir = WD.isEmpty() ? QDir::cleanPath(Folder) : FindPath(Folder, WD);
            if (!SetupDir.isEmpty()) Game.SetupConfs = ConfsOfArguments(SetupTask.value(QLatin1String("arguments")).toString(), SetupDir);
        }
        SetInfo();
        return true;
    }
    return false;
}

// conf files without goggame-<ID>.info (Linux versions, older Windows versions)
bool ReadConfFiles(const QString& Folder, TGOGGame& Game)
{
    QStringList Confs;
    for (const QString& E : QDir(Folder).entryList({QStringLiteral("*.conf")}, QDir::Files, QDir::Name))
        if (E.startsWith(QLatin1String("dosbox"), Qt::CaseInsensitive)) Confs << E;
    if (Confs.isEmpty()) return false;
    auto Find = [&Confs](const QString& Name) -> QString {
        for (const QString& C : Confs) if (C.compare(Name, Qt::CaseInsensitive) == 0) return C;
        return QString();
    };
    QString Single, Base;
    for (const QString& C : Confs) if (C.endsWith(QLatin1String("_single.conf"), Qt::CaseInsensitive)) { Single = C; break; }
    if (!Single.isEmpty()) {
        Base = Find(Single.left(Single.length() - 12) + QStringLiteral(".conf"));
    } else {
        // the conf file with the shortest name (dosbox<Game>.conf)
        for (const QString& C : Confs) {
            const QString L = C.toLower();
            if (L.endsWith(QLatin1String("_settings.conf")) || L.endsWith(QLatin1String("_setup.conf")) || L.endsWith(QLatin1String("_client.conf")) || L.endsWith(QLatin1String("_server.conf"))) continue;
            if (Base.isEmpty() || (C.length() < Base.length())) Base = C;
        }
        if (Base.isEmpty()) return false;
    }
    const QDir D(Folder);
    if (!Base.isEmpty()) Game.GameConfs << QDir::cleanPath(D.filePath(Base));
    if (!Single.isEmpty()) Game.GameConfs << QDir::cleanPath(D.filePath(Single));
    const QString Prefix = (Base.isEmpty() ? Single.left(Single.length() - 12) : Base.left(Base.length() - 5));
    for (const QString& Suffix : {QStringLiteral("_settings.conf"), QStringLiteral("_setup.conf")}) {
        const QString S = Find(Prefix + Suffix);
        if (S.isEmpty()) continue;
        if (!Base.isEmpty()) Game.SetupConfs << QDir::cleanPath(D.filePath(Base));
        Game.SetupConfs << QDir::cleanPath(D.filePath(S));
        break;
    }
    const QString DOSBoxDir = FindSubFolder(Folder, QStringLiteral("dosbox"));
    Game.WorkingDir = DOSBoxDir.isEmpty() ? QDir::cleanPath(Folder) : QDir::cleanPath(DOSBoxDir);
    // Linux versions: "gameinfo" (first line: name), support/icon.png
    QFile Info(D.filePath(QStringLiteral("gameinfo")));
    if (Info.open(QIODevice::ReadOnly | QIODevice::Text)) Game.Name = QString::fromUtf8(Info.readLine()).trimmed();
    const QString Icon = FindPath(Folder, QStringLiteral("support/icon.png"));
    if (!Icon.isEmpty()) Game.Icon = Icon;
    return true;
}

// ScummVM games without goggame-<ID>.info (Linux versions): an ini file
// with a game section in the game folder, otherwise ScummVM's own detection
// (only if there is a "scummvm" folder)
bool ReadScummVMFiles(const QString& Folder, TGOGGame& Game)
{
    const QString ScummVMDir = FindSubFolder(Folder, QStringLiteral("scummvm"));
    Game.WorkingDir = ScummVMDir.isEmpty() ? QDir::cleanPath(Folder) : QDir::cleanPath(ScummVMDir);
    QFile Info(QDir(Folder).filePath(QStringLiteral("gameinfo")));
    if (Info.open(QIODevice::ReadOnly | QIODevice::Text)) Game.Name = QString::fromUtf8(Info.readLine()).trimmed();
    const QString Icon = FindPath(Folder, QStringLiteral("support/icon.png"));
    if (!Icon.isEmpty()) Game.Icon = Icon;
    for (const QString& E : QDir(Folder).entryList({QStringLiteral("*.ini")}, QDir::Files, QDir::Name))
        if (ReadScummVMIni(QDir(Folder).filePath(E), QString(), Game.WorkingDir, Game)) return true;
    if (ScummVMDir.isEmpty()) return false;
    const QList<TScummVMDetectedGame> Detected = DetectScummVMGames(Folder);
    if (Detected.isEmpty()) return false;
    Game.ScummVM = true;
    Game.ScummVMGameID = Detected[0].ID;
    Game.ScummVMDataDir = QDir::cleanPath(QDir::fromNativeSeparators(Detected[0].Path));
    if (Game.Name.isEmpty()) Game.Name = Detected[0].Description;
    return true;
}

struct TConfData {
    QList<QPair<QString, QList<QPair<QString, QString>>>> Sections;   // (later files override earlier ones)
    QStringList Autoexec;
};

void ReadConf(const QString& FileName, TConfData& Data)
{
    TStringList St;
    if (!St.LoadFromFile(FileName)) return;
    QString Section;
    for (int I = 0; I < St.Count(); I++) {
        const QString Line = St[I];
        const QString S = Line.trimmed();
        if (S.startsWith(QLatin1Char('[')) && S.contains(QLatin1Char(']'))) {
            Section = S.mid(1, S.indexOf(QLatin1Char(']')) - 1).trimmed().toLower();
            continue;
        }
        if (Section == QLatin1String("autoexec")) {
            if (!S.startsWith(QLatin1Char('#'))) Data.Autoexec << Line;
            continue;
        }
        if (Section.isEmpty() || S.isEmpty() || S.startsWith(QLatin1Char('#'))) continue;
        const int P = S.indexOf(QLatin1Char('='));
        if (P <= 0) continue;
        const QString Key = S.left(P).trimmed().toLower(), Value = S.mid(P + 1).trimmed();
        int SI = -1;
        for (int J = 0; J < Data.Sections.size(); J++) if (Data.Sections[J].first == Section) { SI = J; break; }
        if (SI < 0) { Data.Sections.append({Section, {}}); SI = Data.Sections.size() - 1; }
        auto& Keys = Data.Sections[SI].second;
        bool Found = false;
        for (auto& K : Keys) if (K.first == Key) { K.second = Value; Found = true; break; }
        if (!Found) Keys.append({Key, Value});
    }
}

TConfData ReadConfs(const QStringList& Files)
{
    TConfData D;
    for (const QString& F : Files) ReadConf(F, D);
    return D;
}

QString StripAt(const QString& Line)
{
    QString S = Line.trimmed();
    while (S.startsWith(QLatin1Char('@'))) S = S.mid(1).trimmed();
    return S;
}

QString RelPath(const QString& Path)
{
    return MakeRelPath(QDir::toNativeSeparators(Path), PrgSetup->BaseDir(), true);
}

}  // namespace

bool ReadGOGGame(const QString& Folder, TGOGGame& Game)
{
    Game = TGOGGame();
    if (!QFileInfo(Folder).isDir()) return false;
    Game.Folder = QDir::cleanPath(QFileInfo(Folder).absoluteFilePath());
    if (!ReadInfoFile(Game.Folder, Game) && !ReadConfFiles(Game.Folder, Game)) {
        const QString F = Game.Folder;
        Game = TGOGGame();
        Game.Folder = F;
        if (!ReadScummVMFiles(Game.Folder, Game)) return false;
    }
    if (Game.ScummVM) {
        if (Game.Name.isEmpty()) Game.Name = QFileInfo(Game.Folder).fileName();
        return true;
    }
    // (a DOS game: the conf files mount something)
    const TConfData D = ReadConfs(Game.GameConfs);
    bool Mounts = false;
    for (const QString& L : D.Autoexec) {
        const QString S = StripAt(L).toLower();
        if (S.startsWith(QLatin1String("mount ")) || S.startsWith(QLatin1String("imgmount "))) { Mounts = true; break; }
    }
    if (!Mounts) return false;
    if (Game.Name.isEmpty()) Game.Name = QFileInfo(Game.Folder).fileName();
    return true;
}

QList<TGOGGame> FindGOGGames(const QString& Folder)
{
    QList<TGOGGame> R;
    TGOGGame G;
    if (ReadGOGGame(Folder, G)) { R << G; return R; }
    // subfolders (e.g. "GOG Games") and their subfolders (e.g. "GOG Galaxy" -> "Games")
    for (const QFileInfo& Sub : QDir(Folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        if (ReadGOGGame(Sub.absoluteFilePath(), G)) { R << G; continue; }
        for (const QFileInfo& Sub2 : QDir(Sub.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
            if (ReadGOGGame(Sub2.absoluteFilePath(), G)) R << G;
    }
    return R;
}

QStringList DefaultGOGFolders()
{
    QStringList L;
#ifdef Q_OS_WIN
    L << QStringLiteral("C:/GOG Games") << QStringLiteral("C:/Program Files (x86)/GOG Galaxy/Games") << QStringLiteral("C:/Program Files/GOG Galaxy/Games");
#else
    L << QDir::homePath() + QStringLiteral("/GOG Games") << QDir::homePath() + QStringLiteral("/Games");
#endif
    QStringList R;
    for (const QString& F : L) if (QFileInfo(F).isDir()) R << F;
    return R;
}

QStringList GOGMountsFromAutoexec(const QStringList& Autoexec, const QString& WorkingDir, const QString& Folder, QStringList& Mounts)
{
    QStringList Rest;
    for (const QString& Line : Autoexec) {
        const QString S = StripAt(Line);
        const QString Lower = S.toLower();
        const bool IsMount = Lower.startsWith(QLatin1String("mount "));
        const bool IsImgMount = Lower.startsWith(QLatin1String("imgmount "));
        if (!IsMount && !IsImgMount) { Rest << Line; continue; }
        const QStringList P = SplitParameters(S);
        if (P.size() < 3) { Rest << Line; continue; }
        QString Letter = P[1].left(1).toUpper();
        if ((Letter < QLatin1String("A")) || (Letter > QLatin1String("Z")) || (P[1].length() > 3)) { Rest << Line; continue; }
        // paths (up to the first option), options
        QStringList Paths;
        int I = 2;
        while ((I < P.size()) && !P[I].startsWith(QLatin1Char('-'))) {
            QString F = FindPath(WorkingDir, P[I]);
            if (F.isEmpty()) F = FindPath(Folder, P[I]);
            Paths << F;
            I++;
        }
        QString Type, FreeSize;
        for (; I < P.size(); I++) {
            const QString O = P[I].toLower();
            if ((O == QLatin1String("-t")) && (I + 1 < P.size())) Type = P[++I].toLower();
            else if ((O == QLatin1String("-freesize")) && (I + 1 < P.size())) FreeSize = P[++I];
        }
        if (Paths.isEmpty() || Paths.contains(QString())) { Rest << Line; continue; }
        if (IsMount) {
            if (Paths.size() != 1) { Rest << Line; continue; }
            if (Type == QLatin1String("cdrom")) Mounts << Paths[0] + QStringLiteral(";CDROM;") + Letter + QStringLiteral(";false;;");
            else if (Type == QLatin1String("floppy")) Mounts << Paths[0] + QStringLiteral(";FLOPPY;") + Letter + QStringLiteral(";false;;");
            else Mounts << Paths[0] + QStringLiteral(";Drive;") + Letter + QStringLiteral(";false;;") + FreeSize;
            continue;
        }
        if ((Type == QLatin1String("iso")) || (Type == QLatin1String("cdrom"))) { Mounts << Paths.join(QLatin1Char('$')) + QStringLiteral(";CDROMIMAGE;") + Letter + QStringLiteral(";;;"); continue; }
        if (Type == QLatin1String("floppy")) { Mounts << Paths.join(QLatin1Char('$')) + QStringLiteral(";FLOPPYIMAGE;") + Letter + QStringLiteral(";;;"); continue; }
        // (hard disk images: the command stays, with the full path)
        QString L = P[0] + QLatin1Char(' ') + P[1];
        for (const QString& F : Paths) L += QStringLiteral(" \"") + QDir::toNativeSeparators(F) + QLatin1Char('"');
        for (int J = 2 + Paths.size(); J < P.size(); J++) L += QLatin1Char(' ') + P[J];
        Rest << L;
    }
    return Rest;
}

bool GOGProgramFromAutoexec(const QStringList& Autoexec, const QStringList& Mounts, QString& Exe, QString& Parameters, QStringList& Rest)
{
    Exe.clear(); Parameters.clear(); Rest.clear();
    QMap<QChar, QString> Drives;   // drive letter -> folder
    for (const QString& M : Mounts) {
        const QStringList F = M.split(QLatin1Char(';'));
        if (F.size() < 3) continue;
        const QString T = F[1].toUpper();
        if ((T == QLatin1String("DRIVE")) || (T == QLatin1String("CDROM")) || (T == QLatin1String("FLOPPY"))) Drives.insert(F[2].at(0).toUpper(), F[0]);
    }
    QChar Drive = QLatin1Char('Z');
    QString Dir;   // current folder on the drive ("" = root, "\" separated)
    static const QStringList ControlFlow = {QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("choice"), QStringLiteral("call"), QStringLiteral("for"), QStringLiteral("shift"), QStringLiteral("pause")};
    for (const QString& Line : Autoexec) {
        QString S = StripAt(Line);
        QString Lower = S.toLower();
        if (S.isEmpty() || (Lower == QLatin1String("cls")) || (Lower == QLatin1String("exit")) || Lower.startsWith(QLatin1String("echo")) || Lower.startsWith(QLatin1String("rem ")) || (Lower == QLatin1String("rem")) || Lower.startsWith(QLatin1String("::"))) continue;
        if (S.startsWith(QLatin1Char(':'))) return false;
        static const QRegularExpression DriveRE(QStringLiteral("^([a-zA-Z]):\\\\?$"));
        const auto DM = DriveRE.match(S);
        if (DM.hasMatch()) { Drive = DM.captured(1).at(0).toUpper(); Dir.clear(); continue; }
        if ((Lower == QLatin1String("cd")) || Lower.startsWith(QLatin1String("cd ")) || Lower.startsWith(QLatin1String("cd\\")) || Lower.startsWith(QLatin1String("cd.."))) {
            QString A = S.mid(2).trimmed();
            if ((A.length() >= 2) && (A[1] == QLatin1Char(':'))) {
                if (A[0].toUpper() != Drive) return false;
                A = A.mid(2);
            }
            if (A.isEmpty()) continue;
            if (A.startsWith(QLatin1Char('\\'))) { Dir.clear(); A = A.mid(1); }
            for (const QString& Part : A.split(QLatin1Char('\\'), Qt::SkipEmptyParts)) {
                if (Part == QLatin1String(".")) continue;
                if (Part == QLatin1String("..")) { const int P = Dir.lastIndexOf(QLatin1Char('\\')); Dir = (P < 0) ? QString() : Dir.left(P); continue; }
                Dir = Dir.isEmpty() ? Part : Dir + QLatin1Char('\\') + Part;
            }
            continue;
        }
        if (Lower.startsWith(QLatin1String("lh ")) || Lower.startsWith(QLatin1String("loadhigh "))) { S = S.mid(S.indexOf(QLatin1Char(' ')) + 1).trimmed(); Lower = S.toLower(); }
        const int Space = S.indexOf(QLatin1Char(' '));
        const QString Command = (Space < 0) ? S : S.left(Space);
        if (ControlFlow.contains(Command.toLower())) return false;
        // a program in the current folder of the drive?
        QString Found;
        if (Drives.contains(Drive) && !Command.contains(QLatin1Char(':'))) {
            const QString Base = Dir.isEmpty() ? Command : Dir + QLatin1Char('\\') + Command;
            const QString Ext = QFileInfo(Command).suffix().toLower();
            const QStringList Exts = (Ext == QLatin1String("exe") || Ext == QLatin1String("com") || Ext == QLatin1String("bat")) ? QStringList{QString()} : QStringList{QStringLiteral(".com"), QStringLiteral(".exe"), QStringLiteral(".bat")};
            for (const QString& E : Exts) {
                const QString F = FindPath(Drives.value(Drive), Base + E);
                if (!F.isEmpty() && QFileInfo(F).isFile()) { Found = F; break; }
            }
        }
        if (Found.isEmpty()) {
            if (!Exe.isEmpty()) return false;   // (something runs after the program)
            Rest << Line;
            continue;
        }
        if (!Exe.isEmpty()) return false;       // (several programs)
        Exe = Found;
        Parameters = (Space < 0) ? QString() : S.mid(Space + 1).trimmed();
    }
    return !Exe.isEmpty();
}

bool GOGGameInList(TGameDB* GameDB, const TGOGGame& Game)
{
    const QString Folder = Game.Folder + QLatin1Char('/');
    for (int I = 0; I < GameDB->Count(); I++) {
        TGame* G = GameDB->Game(I);
        if (G->Name().compare(Game.Name, Qt::CaseInsensitive) == 0) return true;
        for (const QString& F : {G->GameExe(), G->ScummVMPath()}) {
            if (Trim(F).isEmpty()) continue;
            const QString P = QDir::cleanPath(QDir::fromNativeSeparators(MakeAbsPath(F, PrgSetup->BaseDir()))) + QLatin1Char('/');
            if (P.startsWith(Folder, Qt::CaseInsensitive)) return true;
        }
    }
    return false;
}

namespace {

void SetCaptureFolderAndIcon(TGame* G, const TGOGGame& Game)
{
    // capture folder as for new profiles, icon
    QString S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(Game.Name) + PathDelim;
    int I = 0;
    while ((!PrgSetup->IgnoreDirectoryCollisions()) && DirectoryExists(MakeAbsPath(S, PrgSetup->BaseDir()))) {
        I++;
        S = IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()) + MakeFileSysOKFolderName(Game.Name) + IntToStr(I) + PathDelim;
    }
    G->SetCaptureFolder(MakeRelPath(S, PrgSetup->BaseDir()));
    if (!Game.Icon.isEmpty()) G->SetIcon(MakeRelPath(QDir::toNativeSeparators(Game.Icon), PrgSetup->BaseDir()));
}

TGame* ImportGOGScummVMGame(TGameDB* GameDB, const TGOGGame& Game)
{
    // game name as in the list of the installed ScummVM ("engine:game" or "game")
    QString ID = Game.ScummVMGameID;
    if (ScummVMGamesList != nullptr) {
        if (ScummVMGamesList->Count() == 0) ScummVMGamesList->LoadListFromScummVM(false);
        const QString N = ScummVMGamesList->NameFromDetectedID(ID);
        if (!N.isEmpty()) ID = N;
    }
    TGame* G = GameDB->Game(GameDB->Add(Game.Name));
    G->SetProfileMode(QStringLiteral("ScummVM"));
    G->SetScummVMGame(ID);
    G->SetScummVMPath(MakeRelPath(QDir::toNativeSeparators(Game.ScummVMDataDir), PrgSetup->BaseDir()));
    G->SetScummVMLanguage(Game.ScummVMLanguage.toLower());
    if (!Game.ScummVMPlatform.isEmpty()) G->SetScummVMPlatform(Game.ScummVMPlatform.toLower());
    if (!Game.ScummVMExtraPath.isEmpty()) G->SetScummVMExtraPath(MakeRelPath(QDir::toNativeSeparators(Game.ScummVMExtraPath), PrgSetup->BaseDir()));
    SetCaptureFolderAndIcon(G, Game);
    G->StoreAllValues();
    return G;
}

}  // namespace

TGame* ImportGOGGame(TGameDB* GameDB, const TGOGGame& Game)
{
    if (Game.ScummVM) return ImportGOGScummVMGame(GameDB, Game);
    const TConfData Data = ReadConfs(Game.GameConfs);
    if (Data.Sections.isEmpty() && Data.Autoexec.isEmpty()) return nullptr;

    QStringList Mounts;
    const QStringList Autoexec = GOGMountsFromAutoexec(Data.Autoexec, Game.WorkingDir, Game.Folder, Mounts);
    QString Exe, Parameters;
    QStringList Rest;
    const bool Simple = GOGProgramFromAutoexec(Autoexec, Mounts, Exe, Parameters, Rest);

    // settings of the conf files (as "Import conf file" does)
    TStringList Conf;
    for (const auto& Sec : Data.Sections) {
        Conf.Add(QLatin1Char('[') + Sec.first + QLatin1Char(']'));
        for (const auto& K : Sec.second) {
            QString V = K.second;
            if ((Sec.first == QLatin1String("cpu")) && (K.first == QLatin1String("cycles")) && V.startsWith(QLatin1String("fixed "), Qt::CaseInsensitive)) V = V.mid(6).trimmed();
            Conf.Add(K.first + QLatin1Char('=') + V);
        }
        Conf.Add(QString());
    }
    Conf.Add(QStringLiteral("[autoexec]"));
    for (const QString& L : (Simple ? Rest : Autoexec)) Conf.Add(L);

    TGame* G = GameDB->Game(GameDB->Add(Game.Name));
    ImportConfData(GameDB, G, &Conf);

    G->SetNrOfMounts(0);
    for (const QString& M : Mounts) {
        QStringList F = M.split(QLatin1Char(';'));
        QStringList Paths = F[0].split(QLatin1Char('$'));
        for (QString& P : Paths) P = RelPath(P);
        F[0] = Paths.join(QLatin1Char('$'));
        G->SetMount(G->NrOfMounts(), F.join(QLatin1Char(';')));
        G->SetNrOfMounts(G->NrOfMounts() + 1);
    }
    if (!Exe.isEmpty()) G->SetGameExe(MakeRelPath(QDir::toNativeSeparators(Exe), PrgSetup->BaseDir()));
    if (Simple) G->SetGameParameters(Parameters);
    else G->SetAutoexecOverridegamestart(true);

    // setup program
    if (!Game.SetupConfs.isEmpty()) {
        QStringList SetupMounts;
        const QStringList SetupAutoexec = GOGMountsFromAutoexec(ReadConfs(Game.SetupConfs).Autoexec, Game.WorkingDir, Game.Folder, SetupMounts);
        QString SetupExe, SetupParameters;
        QStringList SetupRest;
        if (GOGProgramFromAutoexec(SetupAutoexec, SetupMounts, SetupExe, SetupParameters, SetupRest) && (SetupExe != Exe)) {
            G->SetSetupExe(MakeRelPath(QDir::toNativeSeparators(SetupExe), PrgSetup->BaseDir()));
            G->SetSetupParameters(SetupParameters);
        }
    }

    SetCaptureFolderAndIcon(G, Game);
    G->StoreAllValues();
    return G;
}
