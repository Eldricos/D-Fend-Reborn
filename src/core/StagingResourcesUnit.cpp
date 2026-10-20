#include "StagingResourcesUnit.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include "CommonTools.h"
#include "GameDBUnit.h"
#include "PrgSetupUnit.h"

namespace {

QString JoinPath(const QString& Dir, const QString& Name)
{
    return QDir::cleanPath(Dir + QLatin1Char('/') + Name);
}

void AddDir(QStringList& L, const QString& Dir)
{
    if (Dir.isEmpty()) return;
    const QString D = QDir::cleanPath(Dir);
    for (const QString& S : L) if (S.compare(D, Qt::CaseInsensitive) == 0) return;
    L << D;
}

QString HomeDir() { return QDir::homePath(); }

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
QString XDGDir(const char* Var, const QString& Default)
{
    const QString S = qEnvironmentVariable(Var);
    return S.isEmpty() ? Default : S;
}

QStringList XDGDataDirs()
{
    QStringList L = qEnvironmentVariable("XDG_DATA_DIRS").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (L.isEmpty()) L = {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")};
    return L;
}
#endif

// configuration folder of DOSBox Staging
QString StagingConfigDir(const QString& ExeDir)
{
    if (QFileInfo::exists(JoinPath(ExeDir, QStringLiteral("dosbox-staging.conf")))) return ExeDir;
#if defined(Q_OS_WIN)
    QString S = qEnvironmentVariable("LOCALAPPDATA");
    if (S.isEmpty()) S = JoinPath(HomeDir(), QStringLiteral("AppData/Local"));
    return JoinPath(S, QStringLiteral("DOSBox"));
#elif defined(Q_OS_MACOS)
    return JoinPath(HomeDir(), QStringLiteral("Library/Preferences/DOSBox"));
#else
    return JoinPath(XDGDir("XDG_CONFIG_HOME", JoinPath(HomeDir(), QStringLiteral(".config"))), QStringLiteral("dosbox"));
#endif
}

bool IsRomSize(qint64 Size)
{
    // control ROMs 32/64 KB, PCM ROMs 512 KB / 1 MB, interleaved halves 32 ... 512 KB
    return (Size == 32768) || (Size == 65536) || (Size == 131072) || (Size == 262144) || (Size == 524288) || (Size == 1048576);
}

bool ContainsRoms(const QString& Dir)
{
    const QFileInfoList L = QDir(Dir).entryInfoList(QDir::Files | QDir::Readable);
    for (const QFileInfo& F : L) if (IsRomSize(F.size())) return true;
    return false;
}

QString PathForSetting(const QString& Path)
{
    return MakeRelPath(QDir::toNativeSeparators(Path), PrgSetup->BaseDir());
}

QString ExeFileOfInstallation(const QString& Installation)
{
    const QString S = Trim(ExtUpperCase(Installation));
    if (S.isEmpty() || (S == "DEFAULT")) return DOSBoxExeFileOfInstallation(0);
    for (int I = 0; I < PrgSetup->DOSBoxSettingsCount(); I++) if (Trim(ExtUpperCase(PrgSetup->DOSBoxSettings(I)->Name())) == S) return DOSBoxExeFileOfInstallation(I);
    const QString P = MakeAbsPath(Trim(Installation), PrgSetup->BaseDir());
    if (FileExists(P) && !DirectoryExists(P)) return P;
    return DOSBoxExeFile(P);
}

}  // namespace

TStagingSearchDirs StagingSearchDirs(const QString& DOSBoxExeFile)
{
    TStagingSearchDirs R;
    const QString ExeDir = QDir::cleanPath(QFileInfo(DOSBoxExeFile).absolutePath());
    const QString Config = StagingConfigDir(ExeDir);

    // resource folders: next to the program (Windows, macOS, Linux tar.gz) or <prefix>/share/dosbox-staging
    QStringList Resources;
    AddDir(Resources, JoinPath(ExeDir, QStringLiteral("resources")));
    AddDir(Resources, JoinPath(ExeDir, QStringLiteral("../share/dosbox-staging")));
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    for (const QString& D : XDGDataDirs()) AddDir(Resources, JoinPath(D, QStringLiteral("dosbox-staging")));
#endif
    AddDir(R.Shaders, JoinPath(ExeDir, QStringLiteral("shaders")));
    for (const QString& D : Resources) {
        AddDir(R.Shaders, JoinPath(D, QStringLiteral("shaders")));
        AddDir(R.ShaderPresets, JoinPath(D, QStringLiteral("shader-presets")));
    }
    AddDir(R.Shaders, JoinPath(Config, QStringLiteral("shaders")));
    AddDir(R.ShaderPresets, JoinPath(Config, QStringLiteral("shader-presets")));

    AddDir(R.SoundFonts, JoinPath(Config, QStringLiteral("soundfonts")));
    AddDir(R.MT32Roms, JoinPath(Config, QStringLiteral("mt32-roms")));
#if defined(Q_OS_WIN)
    AddDir(R.SoundFonts, QStringLiteral("C:/soundfonts"));
    AddDir(R.MT32Roms, QStringLiteral("C:/mt32-rom-data"));
#elif defined(Q_OS_MACOS)
    AddDir(R.SoundFonts, JoinPath(HomeDir(), QStringLiteral("Library/Audio/Sounds/Banks")));
    AddDir(R.MT32Roms, JoinPath(HomeDir(), QStringLiteral("Library/Audio/Sounds/MT32-Roms")));
    AddDir(R.MT32Roms, QStringLiteral("/usr/local/share/mt32-rom-data"));
    AddDir(R.MT32Roms, QStringLiteral("/usr/share/mt32-rom-data"));
#else
    const QString DataHome = XDGDir("XDG_DATA_HOME", JoinPath(HomeDir(), QStringLiteral(".local/share")));
    const QString ConfigHome = XDGDir("XDG_CONFIG_HOME", JoinPath(HomeDir(), QStringLiteral(".config")));
    AddDir(R.SoundFonts, JoinPath(DataHome, QStringLiteral("dosbox/soundfonts")));
    AddDir(R.SoundFonts, JoinPath(DataHome, QStringLiteral("soundfonts")));
    AddDir(R.SoundFonts, JoinPath(DataHome, QStringLiteral("sounds/sf2")));
    for (const QString& D : XDGDataDirs()) AddDir(R.SoundFonts, JoinPath(D, QStringLiteral("soundfonts")));
    for (const QString& D : XDGDataDirs()) AddDir(R.SoundFonts, JoinPath(D, QStringLiteral("sounds/sf2")));
    AddDir(R.SoundFonts, JoinPath(ConfigHome, QStringLiteral("dosbox/soundfonts")));
    AddDir(R.MT32Roms, JoinPath(DataHome, QStringLiteral("dosbox/mt32-roms")));
    AddDir(R.MT32Roms, JoinPath(DataHome, QStringLiteral("mt32-rom-data")));
    for (const QString& D : XDGDataDirs()) AddDir(R.MT32Roms, JoinPath(D, QStringLiteral("mt32-rom-data")));
    AddDir(R.MT32Roms, JoinPath(ConfigHome, QStringLiteral("dosbox/mt32-roms")));
#endif
    return R;
}

QStringList StagingShaderNames(const QStringList& ShaderDirs, const QStringList& PresetDirs)
{
    QStringList R = {QStringLiteral("crt-auto"), QStringLiteral("crt-auto-machine"), QStringLiteral("crt-auto-arcade"), QStringLiteral("crt-auto-arcade-sharp"),
                     QStringLiteral("sharp"), QStringLiteral("bilinear"), QStringLiteral("nearest")};

    // shaders: "path/name" (the shaders in "_internal" are used by DOSBox Staging itself)
    QMap<QString, QString> Shaders;   // lower case -> name (sorted, first folder wins)
    for (const QString& Dir : ShaderDirs) {
        QDirIterator It(Dir, {QStringLiteral("*.glsl")}, QDir::Files, QDirIterator::Subdirectories);
        while (It.hasNext()) {
            QString Name = QDir(Dir).relativeFilePath(It.next());
            Name.chop(5);
            if (Name.startsWith(QLatin1String("_internal/"))) continue;
            if (!Shaders.contains(Name.toLower())) Shaders.insert(Name.toLower(), Name);
        }
    }
    // presets: "<shader path>/<preset>.preset"
    QMap<QString, QStringList> Presets;   // shader (lower case) -> presets
    for (const QString& Dir : PresetDirs) {
        QDirIterator It(Dir, {QStringLiteral("*.preset")}, QDir::Files, QDirIterator::Subdirectories);
        while (It.hasNext()) {
            const QString Name = QDir(Dir).relativeFilePath(It.next());
            const int P = Name.lastIndexOf(QLatin1Char('/'));
            if (P <= 0) continue;
            QString Preset = Name.mid(P + 1);
            Preset.chop(7);
            QStringList& L = Presets[Name.left(P).toLower()];
            if (!L.contains(Preset, Qt::CaseInsensitive)) L << Preset;
        }
    }
    for (auto It = Shaders.cbegin(); It != Shaders.cend(); ++It) {
        if (!R.contains(It.value(), Qt::CaseInsensitive)) R << It.value();
        QStringList L = Presets.value(It.key());
        L.sort(Qt::CaseInsensitive);
        for (const QString& P : L) R << It.value() + QLatin1Char(':') + P;
    }
    return R;
}

QStringList StagingSoundFonts(const QStringList& Dirs)
{
    QStringList Names, Paths;
    for (const QString& Dir : Dirs) {
        QDirIterator It(Dir, {QStringLiteral("*.sf2"), QStringLiteral("*.sf3")}, QDir::Files, QDirIterator::Subdirectories);
        while (It.hasNext()) {
            const QString F = It.next();
            const QString Rel = QDir(Dir).relativeFilePath(F);
            if (!Rel.contains(QLatin1Char('/'))) {
                if (!Names.contains(Rel, Qt::CaseInsensitive)) Names << Rel;
            } else {
                Paths << PathForSetting(F);
            }
        }
    }
    Names.sort(Qt::CaseInsensitive);
    Paths.sort(Qt::CaseInsensitive);
    Paths.removeDuplicates();
    return Names + Paths;
}

QStringList StagingMT32RomDirs(const QStringList& Dirs)
{
    QStringList R;
    for (const QString& Dir : Dirs) {
        if (!QFileInfo(Dir).isDir()) continue;
        if (ContainsRoms(Dir)) R << PathForSetting(Dir);
        QStringList Sub;
        for (const QFileInfo& F : QDir(Dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) if (ContainsRoms(F.absoluteFilePath())) Sub << PathForSetting(F.absoluteFilePath());
        Sub.sort(Qt::CaseInsensitive);
        R += Sub;
    }
    R.removeDuplicates();
    return R;
}

QStringList StagingSettingChoices(int I, const QString& DOSBoxInstallation)
{
    if ((I < 0) || (I >= StagingSettingCount)) return {};
    const QString Key = QLatin1String(StagingSettings[I].Key);
    if ((Key != QLatin1String("shader")) && (Key != QLatin1String("soundfont")) && (Key != QLatin1String("romdir"))) return {};
    const TStagingSearchDirs Dirs = StagingSearchDirs(ExeFileOfInstallation(DOSBoxInstallation));
    if (Key == QLatin1String("shader")) return StagingShaderNames(Dirs.Shaders, Dirs.ShaderPresets);
    if (Key == QLatin1String("soundfont")) return StagingSoundFonts(Dirs.SoundFonts);
    return StagingMT32RomDirs(Dirs.MT32Roms);
}
