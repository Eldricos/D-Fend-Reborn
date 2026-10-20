// Automated tests of D-Fend Reborn (program code without windows).
//
//   * conf files for DOSBox Staging and DOSBox-X (DOSBoxConfUnit)
//   * detection of the DOSBox version of an installation (DOSBoxKindOfFile)
//   * saving files via a temporary file (settings, profiles, lists)
//   * the game detection of ScummVM (output of "scummvm --detect")
//   * shaders, SoundFonts and MT-32 ROM folders found for DOSBox Staging
//   * import of GOG.com DOS games (goggame-*.info, DOSBox conf files)
//   * saving and loading profiles (TGame), including the DOSBox Staging
//     settings
//   * all auto setup templates of data/NewUserData/AutoSetup: they can be
//     loaded and their conf files for DOSBox Staging and DOSBox-X contain no
//     DOSBox 0.74 settings these versions do not know any more
//
// Run: ctest (or the DFRTests executable). The tests work in a temporary
// program folder; the data of a D-Fend Reborn installation is not touched.
#include <QtTest>
#include <memory>
#include <QTemporaryDir>

#include "CommonTools.h"
#include "DOSBoxConfUnit.h"
#include "DosBoxUnit.h"
#include "GameDBUnit.h"
#include "LanguageSetupUnit.h"
#include "LoggingUnit.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "ScummVMToolsUnit.h"
#include "StagingResourcesUnit.h"
#include "GOGImportUnit.h"

namespace {

// key=value lines of a section of a conf file ("" for the section: all
// lines outside of [autoexec])
QStringList SectionLines(const QStringList& Conf, const QString& Section)
{
    QStringList Result;
    QString Current;
    for (const QString& Line : Conf) {
        const QString T = Line.trimmed();
        if (T.startsWith('[') && T.endsWith(']')) { Current = T.mid(1, T.length() - 2).toLower(); continue; }
        if (T.isEmpty() || T.startsWith('#')) continue;
        if (Section.isEmpty() ? (Current != QLatin1String("autoexec")) : (Current == Section)) Result << T;
    }
    return Result;
}

QStringList Sections(const QStringList& Conf)
{
    QStringList Result;
    for (const QString& Line : Conf) {
        const QString T = Line.trimmed();
        if (T.startsWith('[') && T.endsWith(']')) Result << T.mid(1, T.length() - 2).toLower();
    }
    return Result;
}

QString Value(const QStringList& Conf, const QString& Section, const QString& Key)
{
    for (const QString& L : SectionLines(Conf, Section)) {
        const int Eq = L.indexOf('=');
        if (Eq > 0 && L.left(Eq).trimmed().toLower() == Key) return L.mid(Eq + 1).trimmed();
    }
    return QStringLiteral("<missing>");
}

bool HasKey(const QStringList& Conf, const QString& Section, const QString& Key)
{
    return Value(Conf, Section, Key) != QLatin1String("<missing>");
}

void WriteFile(const QString& FileName, const QByteArray& Data = QByteArray())
{
    QDir().mkpath(QFileInfo(FileName).absolutePath());
    QFile F(FileName);
    QVERIFY(F.open(QIODevice::WriteOnly));
    F.write(Data);
}

// same file (Windows: any case)
bool SamePath(const QString& A, const QString& B)
{
    const QString X = QDir::cleanPath(QDir::fromNativeSeparators(A)), Y = QDir::cleanPath(QDir::fromNativeSeparators(B));
#ifdef Q_OS_WIN
    return X.compare(Y, Qt::CaseInsensitive) == 0;
#else
    return X == Y;
#endif
}

QStringList Messages;   // message boxes since the last ClearMessages

// Message boxes of the program code: no windows in the tests
int TestMessageDlg(const QString& Caption, const QString& Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons)
{
    Q_UNUSED(DlgType);
    Q_UNUSED(Caption);
    Messages << Msg;
    if (Buttons & mbCancel) return mrCancel;
    if (Buttons & mbNo) return mrNo;
    return mrOk;
}

}  // namespace

class CoreTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir FPrgDir;

    // DOSBox installation 0 of the program settings: a fake DOSBox of the
    // given kind (only the file name is checked)
    void SetDOSBoxInstallation(const QString& ExeName)
    {
        const QString Dir = FPrgDir.path() + "/FakeDOSBox-" + ExeName + "/";
        WriteFile(Dir + ExeName);
        PrgSetup->DOSBoxSettings(0)->SetDosBoxDir(QDir::toNativeSeparators(Dir));
        PrgSetup->DOSBoxSettings(0)->SetDosBoxExecutable(ExeName);
    }

    QStringList ConfOfTemplate(const QString& FileName)
    {
        TGame Game(FileName);
        TStringList* St = BuildConfFile(&Game, false, false, -1, nullptr, false);
        if (St == nullptr) return {};
        const QStringList Result = St->ToQStringList();
        delete St;
        return Result;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(FPrgDir.isValid());
        // empty program folder: program settings and profiles are created there
        TempPrgDir = IncludeTrailingPathDelimiter(QDir::toNativeSeparators(FPrgDir.path()));
        QDir().mkpath(FPrgDir.path() + "/Confs");
        MessageDlgHook = TestMessageDlg;
        PrgConsts_Initialization();
        LoggingUnit_Initialization();
        PrgSetupUnit_Initialization();
        LanguageSetupUnit_Initialization();
        GameDBUnit_Initialization();
    }

    void cleanupTestCase()
    {
        GameDBUnit_Finalization();
        LanguageSetupUnit_Finalization();
        PrgSetupUnit_Finalization();
        TempPrgDir.clear();
    }

    // ----- conf files for DOSBox Staging -----------------------------------

    void stagingRenamedSettings()
    {
        const QStringList In = {
            "[sdl]", "fullscreen=false", "windowresolution=800x600", "autolock=true", "sensitivity=150",
            "mapperfile=mapper.map", "output=opengl", "fulldouble=false",
            "[dosbox]", "machine=vgaonly", "captures=capture", "memsize=16",
            "[render]", "aspect=true", "scaler=normal2x", "frameskip=0",
            "[cpu]", "core=auto", "cycles=fixed 5000",
            "[midi]", "mididevice=default",
            "[speaker]", "pcspeaker=true", "pcrate=44100", "disney=true",
            "[dos]", "keyboardlayout=none",
            "[autoexec]", "mount c .", "cycles=max", "c:",
        };
        const QStringList Out = ConvertConfForDOSBox(dkStaging, In);

        QCOMPARE(Value(Out, "sdl", "fullscreen"), QString("false"));
        QCOMPARE(Value(Out, "sdl", "window_size"), QString("800x600"));
        QCOMPARE(Value(Out, "sdl", "mapperfile"), QString("mapper-sdl2.map"));
        QCOMPARE(Value(Out, "mouse", "mouse_capture"), QString("onclick"));
        QCOMPARE(Value(Out, "mouse", "mouse_sensitivity"), QString("150"));
        QCOMPARE(Value(Out, "dosbox", "machine"), QString("svga_paradise"));
        QCOMPARE(Value(Out, "dosbox", "memsize"), QString("16"));
        QCOMPARE(Value(Out, "capture", "capture_dir"), QString("capture"));
        QCOMPARE(Value(Out, "render", "aspect"), QString("on"));
        QCOMPARE(Value(Out, "cpu", "core"), QString("auto"));
        QCOMPARE(Value(Out, "cpu", "cpu_cycles"), QString("5000"));
        QCOMPARE(Value(Out, "cpu", "cpu_cycles_protected"), QString("5000"));
        QCOMPARE(Value(Out, "midi", "mididevice"), QString("port"));
        QCOMPARE(Value(Out, "speaker", "pcspeaker"), QString("impulse"));
        QCOMPARE(Value(Out, "speaker", "lpt_dac"), QString("disney"));
        QCOMPARE(Value(Out, "dos", "keyboard_layout"), QString("us"));

        for (const auto& [Section, Key] : QList<QPair<QString, QString>>{
                 {"sdl", "windowresolution"}, {"sdl", "autolock"}, {"sdl", "sensitivity"}, {"sdl", "output"},
                 {"sdl", "fulldouble"}, {"dosbox", "captures"}, {"render", "scaler"}, {"render", "frameskip"},
                 {"cpu", "cycles"}, {"speaker", "pcrate"}, {"speaker", "disney"}, {"dos", "keyboardlayout"}}) {
            QVERIFY2(!HasKey(Out, Section, Key), qPrintable(Section + "/" + Key + " is still in the conf file"));
        }

        // [autoexec] is not changed (also lines that look like settings) and
        // the settings for other sections come before it
        const int A = Out.indexOf("[autoexec]");
        QVERIFY(A >= 0);
        QCOMPARE(Out.mid(A), QStringList({"[autoexec]", "mount c .", "cycles=max", "c:"}));
        QVERIFY(Out.indexOf("[mouse]") >= 0 && Out.indexOf("[mouse]") < A);
        QVERIFY(Out.indexOf("[capture]") >= 0 && Out.indexOf("[capture]") < A);
    }

    void stagingCycles_data()
    {
        QTest::addColumn<QString>("Cycles");
        QTest::addColumn<QString>("Real");
        QTest::addColumn<QString>("Protected");
        QTest::newRow("fixed") << "fixed 12000" << "12000" << "12000";
        QTest::newRow("number") << "3000" << "3000" << "3000";
        QTest::newRow("max") << "max" << "max" << "max";
        QTest::newRow("auto") << "auto" << "3000" << "max";
        QTest::newRow("upper case") << "Fixed 8000" << "8000" << "8000";
    }

    void stagingCycles()
    {
        QFETCH(QString, Cycles);
        QFETCH(QString, Real);
        QFETCH(QString, Protected);
        const QStringList Out = ConvertConfForDOSBox(dkStaging, {"[cpu]", "cycles=" + Cycles});
        QCOMPARE(Value(Out, "cpu", "cpu_cycles"), Real);
        QCOMPARE(Value(Out, "cpu", "cpu_cycles_protected"), Protected);
        QVERIFY(!HasKey(Out, "cpu", "cycles"));
    }

    void stagingUnknownCyclesAreKept()
    {
        // (DOSBox Staging still accepts the other forms of the setting)
        const QStringList In = {"[cpu]", "cycles=auto 3000 limit 50000"};
        QCOMPARE(ConvertConfForDOSBox(dkStaging, In), In);
    }

    void stagingRemovedSections()
    {
        const QStringList Out = ConvertConfForDOSBox(dkStaging, {
            "[vga]", "svgachipset=s3", "[glide]", "glide=false", "[printer]", "printer=true",
            "[innova]", "innova=true", "samplerate=22050", "sidbase=280", "quality=0",
            "[ne2000]", "ne2000=true", "nicbase=300", "realnic=list",
            "[autoexec]", "echo hello"});
        for (const QString& S : {"vga", "glide", "printer", "innova", "ne2000"})
            QVERIFY2(!Sections(Out).contains(S), qPrintable("section [" + S + "] is still in the conf file"));
        QCOMPARE(Value(Out, "innovation", "innovation"), QString("on"));
        QVERIFY(!HasKey(Out, "innovation", "samplerate"));
        QCOMPARE(Value(Out, "ethernet", "ne2000"), QString("true"));
        QCOMPARE(Value(Out, "ethernet", "nicbase"), QString("300"));
        QVERIFY(!HasKey(Out, "ethernet", "realnic"));
        QCOMPARE(Out.mid(Out.indexOf("[autoexec]")), QStringList({"[autoexec]", "echo hello"}));
    }

    void stagingNativeSettingsWin()
    {
        // settings written with the DOSBox Staging names (DOSBox Staging page
        // of the profile editor, custom settings) win over converted ones
        const QStringList Out = ConvertConfForDOSBox(dkStaging, {
            "[sdl]", "autolock=false", "[cpu]", "cycles=fixed 5000", "cpu_cycles=8000",
            "[mouse]", "mouse_capture=onclick"});
        QCOMPARE(SectionLines(Out, "cpu"), QStringList({"cpu_cycles_protected=5000", "cpu_cycles=8000"}));
        QCOMPARE(SectionLines(Out, "mouse"), QStringList({"mouse_capture=onclick"}));
    }

    void stagingMapperFile_data()
    {
        QTest::addColumn<QString>("In");
        QTest::addColumn<QString>("Out");
        QTest::newRow("extension") << "mapper.map" << "mapper-sdl2.map";
        QTest::newRow("no extension") << "mapper" << "mapper-sdl2";
        QTest::newRow("dot in folder") << "C:\\My.Games\\keys" << "C:\\My.Games\\keys-sdl2";
        QTest::newRow("already SDL 2") << "mapper-sdl2.map" << "mapper-sdl2.map";
    }

    void stagingMapperFile()
    {
        QFETCH(QString, In);
        QFETCH(QString, Out);
        QCOMPARE(Value(ConvertConfForDOSBox(dkStaging, {"[sdl]", "mapperfile=" + In}), "sdl", "mapperfile"), Out);
    }

    void stagingFalseValues()
    {
        const QStringList Out = ConvertConfForDOSBox(dkStaging, {
            "[sdl]", "autolock=false", "windowresolution=original", "[render]", "aspect=false",
            "[speaker]", "pcspeaker=false", "disney=false"});
        QCOMPARE(Value(Out, "mouse", "mouse_capture"), QString("seamless"));
        QVERIFY(!HasKey(Out, "sdl", "window_size"));   // "original": DOSBox Staging default
        QVERIFY(!HasKey(Out, "render", "aspect"));     // false: DOSBox Staging default
        QCOMPARE(Value(Out, "speaker", "pcspeaker"), QString("off"));
        QVERIFY(!HasKey(Out, "speaker", "lpt_dac"));
    }

    // ----- conf files for DOSBox-X and DOSBox ------------------------------

    void dosboxXMovedSettings()
    {
        const QStringList Out = ConvertConfForDOSBox(dkDOSBoxX, {
            "[sdl]", "windowresolution=800x600", "[dosbox]", "memsize=16", "vmemsize=8",
            "[ne2000]", "ne2000=true", "realnic=list", "[autoexec]", "mount c ."});
        QCOMPARE(Value(Out, "sdl", "windowresolution"), QString("800x600"));  // (DOSBox-X: 0.74 names)
        QCOMPARE(SectionLines(Out, "dosbox"), QStringList({"memsize=16"}));
        QCOMPARE(Value(Out, "video", "vmemsize"), QString("8"));
        QCOMPARE(SectionLines(Out, "ne2000"), QStringList({"ne2000=true"}));
        QCOMPARE(Value(Out, "ethernet, pcap", "realnic"), QString("list"));
        QVERIFY(Out.indexOf("[video]") < Out.indexOf("[autoexec]"));
    }

    void dosboxUnchanged()
    {
        const QStringList In = {"[sdl]", "output=opengl", "[cpu]", "cycles=auto", "[autoexec]", "c:"};
        QCOMPARE(ConvertConfForDOSBox(dkDOSBox, In), In);
    }

    void ignoredSettings()
    {
        QVERIFY(DOSBoxSettingIgnored(dkStaging, "SDL", "Output"));
        QVERIFY(DOSBoxSettingIgnored(dkStaging, "vga", "svgachipset"));
        QVERIFY(DOSBoxSettingIgnored(dkStaging, "render", "scaler"));
        QVERIFY(!DOSBoxSettingIgnored(dkStaging, "cpu", "cycles"));
        QVERIFY(!DOSBoxSettingIgnored(dkStaging, "sdl", "fullscreen"));
        QVERIFY(!DOSBoxSettingIgnored(dkDOSBoxX, "sdl", "output"));
        QVERIFY(!DOSBoxSettingIgnored(dkDOSBox, "vga", "svgachipset"));
    }

    // ----- DOSBox version of an installation --------------------------------

    void dosboxKindOfFile()
    {
        const QString D = FPrgDir.path() + "/Kinds/";
        WriteFile(D + "a/dosbox-staging");
        WriteFile(D + "b/dosbox-x.exe");
        WriteFile(D + "c/dosbox.exe");
        WriteFile(D + "c/resources/translations/de.lng");
        WriteFile(D + "d/dosbox");
        WriteFile(D + "d/README.txt", "DOSBox Staging\n");
        WriteFile(D + "e/dosbox");
        WriteFile(D + "e/dosbox-x.reference.conf");
        WriteFile(D + "f/dosbox.exe");
        QCOMPARE(DOSBoxKindOfFile(D + "a/dosbox-staging"), dkStaging);
        QCOMPARE(DOSBoxKindOfFile(D + "b/dosbox-x.exe"), dkDOSBoxX);
        QCOMPARE(DOSBoxKindOfFile(D + "c/dosbox.exe"), dkStaging);
        QCOMPARE(DOSBoxKindOfFile(D + "d/dosbox"), dkStaging);
        QCOMPARE(DOSBoxKindOfFile(D + "e/dosbox"), dkDOSBoxX);
        QCOMPARE(DOSBoxKindOfFile(D + "f/dosbox.exe"), dkDOSBox);
        QCOMPARE(DOSBoxKind(QString()), dkDOSBox);
    }

    // ----- saving files ----------------------------------------------------

    // Settings and profiles are written via a temporary file (WriteFileSafely)
    void safeFileSaving()
    {
        const QString File = TempPrgDir + "SafeSave.txt";
        QVERIFY(WriteTextFile(File, "first", true));
        QVERIFY(WriteTextFile(File, "second\r\nline", true));
        QCOMPARE(ReadTextFile(File), QString("second\r\nline"));
        // no temporary files left behind
        const QStringList Left = QDir(FPrgDir.path()).entryList({"SafeSave*"}, QDir::Files);
        QCOMPARE(Left, QStringList({"SafeSave.txt"}));
        // a folder that does not exist: error, nothing written
        QVERIFY(!WriteTextFile(TempPrgDir + "Missing" + PathDelim + "File.txt", "x", true));
        QVERIFY(!QFileInfo::exists(TempPrgDir + "Missing"));
    }


    // ----- ScummVM game detection (scummvm --detect) ------------------------

    void scummVMDetectOutput()
    {
        // output of ScummVM 2.8 (with a warning before the table)
        const QStringList Out = {
            "WARNING: FSNode::createReadStream: 'scummvm.ini' does not exist!",
            "GameID                         Description                                                Full Path",
            "------------------------------ ---------------------------------------------------------- ---------------------------------------------------------",
            "sky:sky                        Beneath a Steel Sky (v0.0372 CD/DOS)                       /games/BASS",
            "queen:queen                    Flight of the Amazon Queen (CD/DOS/English)                C:\\Games\\Amazon Queen",
        };
        const QList<TScummVMDetectedGame> G = ParseScummVMDetectOutput(Out);
        QCOMPARE(G.size(), 2);
        QCOMPARE(G[0].ID, QString("sky:sky"));
        QCOMPARE(G[0].Description, QString("Beneath a Steel Sky (v0.0372 CD/DOS)"));
        QCOMPARE(G[0].Path, QString("/games/BASS"));
        QCOMPARE(G[1].ID, QString("queen:queen"));
        QCOMPARE(G[1].Path, QString("C:\\Games\\Amazon Queen"));
        // no game found
        QVERIFY(ParseScummVMDetectOutput({"WARNING: ScummVM could not find any game in /games/empty",
                                          "WARNING: Consider using --recursive to search inside subdirectories"}).isEmpty());
        QVERIFY(ParseScummVMDetectOutput({}).isEmpty());
    }

    void scummVMDetectedIDs()
    {
        // list of "scummvm -z": with engine prefix (ScummVM 2.5+) or without
        for (bool Prefix : {true, false}) {
            TScummVMGamesList L;
            TStringList St;
            St.Add("Game ID                        Full Title");
            St.Add("------------------------------ -----------------------------------------------------------");
            St.Add(QString(Prefix ? "sky:sky" : "sky").leftJustified(31) + "Beneath a Steel Sky");
            St.Add(QString(Prefix ? "scumm:monkey" : "monkey").leftJustified(31) + "The Secret of Monkey Island");
            QVERIFY(L.LoadListFromScummVMStringList(&St));
            const QString Sky = Prefix ? "sky:sky" : "sky";
            QCOMPARE(L.NameFromDetectedID("sky:sky"), Sky);
            QCOMPARE(L.NameFromDetectedID("sky"), Sky);
            QCOMPARE(L.NameFromDetectedID("SKY:SKY"), Sky);
            QCOMPARE(L.DescriptionFromName(L.NameFromDetectedID("scumm:monkey")), QString("The Secret of Monkey Island"));
            QCOMPARE(L.NameFromDetectedID("queen:queen"), QString());
            QCOMPARE(L.NameFromDetectedID(""), QString());
        }
    }

#ifndef Q_OS_WIN
    void scummVMDetectRun()
    {
        // a fake ScummVM: prints the table with the folder it was given
        const QString Dir = FPrgDir.path() + "/FakeScummVM/";
        WriteFile(Dir + "scummvm",
                  "#!/bin/sh\n"
                  "P=\"\"; for A in \"$@\"; do case \"$A\" in --path=*) P=\"${A#--path=}\";; esac; done\n"
                  "echo 'GameID   Description          Full Path'\n"
                  "echo '-------- -------------------- ---------'\n"
                  "echo \"sky:sky  Beneath a Steel Sky  $P\"\n");
        QFile::setPermissions(Dir + "scummvm", QFile::permissions(Dir + "scummvm") | QFileDevice::ExeOwner);
        const QString Game = FPrgDir.path() + "/Games/Steel Sky";
        QDir().mkpath(Game);
        const QList<TScummVMDetectedGame> G = DetectScummVMGames(Game, Dir);
        QCOMPARE(G.size(), 1);
        QCOMPARE(G[0].ID, QString("sky:sky"));
        QCOMPARE(G[0].Path, Game);   // folder with a space passed as one argument
        // folder that does not exist / ScummVM not found
        QVERIFY(DetectScummVMGames(FPrgDir.path() + "/Missing", Dir).isEmpty());
        QVERIFY(DetectScummVMGames(Game, FPrgDir.path() + "/NoScummVM/").isEmpty());
    }
#endif

    // ----- DOSBox Staging shaders, SoundFonts, MT-32 ROMs ----------------------

    void stagingShaders()
    {
        const QString D = FPrgDir.path() + "/StagingShaders/";
        WriteFile(D + "resources/shaders/crt/crt-hyllian.glsl");
        WriteFile(D + "resources/shaders/scaler/xbr-lv3.glsl");
        WriteFile(D + "resources/shaders/interpolation/sharp.glsl");
        WriteFile(D + "resources/shaders/_internal/integer-upscale.glsl");
        WriteFile(D + "resources/shader-presets/crt/crt-hyllian/vga-4k.preset");
        WriteFile(D + "resources/shader-presets/crt/crt-hyllian/ega-1080p.preset");
        WriteFile(D + "resources/shader-presets/misc/fixvideo/scanlines.preset");   // (shader not there)
        WriteFile(D + "user/shaders/my-shader.glsl");
        WriteFile(D + "user/shaders/scaler/xbr-lv3.glsl");   // (twice: listed once)
        const QStringList L = StagingShaderNames({D + "resources/shaders", D + "user/shaders"}, {D + "resources/shader-presets"});
        const QStringList Expected = {"crt-auto", "crt-auto-machine", "crt-auto-arcade", "crt-auto-arcade-sharp", "sharp", "bilinear", "nearest",
                                      "crt/crt-hyllian", "crt/crt-hyllian:ega-1080p", "crt/crt-hyllian:vga-4k",
                                      "interpolation/sharp", "my-shader", "scaler/xbr-lv3"};
        QCOMPARE(L, Expected);
        // no folders: the built-in names
        QCOMPARE(StagingShaderNames({D + "missing"}, {}).size(), 7);
    }

    void stagingSoundFontsAndRoms()
    {
        const QString D = FPrgDir.path() + "/StagingSF/";
        WriteFile(D + "a/GeneralUser GS.sf2");
        WriteFile(D + "a/Sub/Arachno.SF2");
        WriteFile(D + "a/readme.txt");
        WriteFile(D + "b/FluidR3.sf3");
        WriteFile(D + "b/GeneralUser GS.sf2");   // (same name in a later folder)
        const QStringList SF = StagingSoundFonts({D + "a", D + "b", D + "missing"});
        QCOMPARE(SF.size(), 3);
        QCOMPARE(SF[0], QString("FluidR3.sf3"));
        QCOMPARE(SF[1], QString("GeneralUser GS.sf2"));
        QCOMPARE(QDir::fromNativeSeparators(MakeAbsPath(SF[2], PrgSetup->BaseDir())), D + "a/Sub/Arachno.SF2");

        const QString R = FPrgDir.path() + "/StagingROMs/";
        WriteFile(R + "roms/MT32_CONTROL.ROM", QByteArray(65536, 0));
        WriteFile(R + "roms/cm32l/CM32L_PCM.ROM", QByteArray(1048576, 0));
        WriteFile(R + "roms/other/notes.txt", "no ROM");
        WriteFile(R + "empty/readme.txt", "no ROM");
        const QStringList Roms = StagingMT32RomDirs({R + "roms", R + "empty"});
        QCOMPARE(Roms.size(), 2);
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(MakeAbsPath(Roms[0], PrgSetup->BaseDir()))), QDir::cleanPath(R + "roms"));
        QCOMPARE(QDir::cleanPath(QDir::fromNativeSeparators(MakeAbsPath(Roms[1], PrgSetup->BaseDir()))), R + "roms/cm32l");
    }

    void stagingSearchDirs()
    {
        // portable layout (dosbox-staging.conf next to the program): its folder is the configuration folder
        const QString D = QDir::cleanPath(FPrgDir.path() + "/StagingPortable");
        WriteFile(D + "/dosbox-staging.conf", "# test\n");
        const TStagingSearchDirs Dirs = StagingSearchDirs(D + "/dosbox");
        QVERIFY(Dirs.Shaders.contains(D + "/resources/shaders"));
        QVERIFY(Dirs.Shaders.contains(D + "/shaders"));
        QVERIFY(Dirs.ShaderPresets.contains(D + "/resources/shader-presets"));
        QCOMPARE(Dirs.SoundFonts.value(0), D + "/soundfonts");
        QCOMPARE(Dirs.MT32Roms.value(0), D + "/mt32-roms");
    }

    // ----- GOG games ----------------------------------------------------------

    void gogWindowsGame()
    {
        // Windows version: goggame-<ID>.info, DOSBOX folder, conf paths relative to it
        // (in other case than the files), CD image game.ins
        const QString Root = FPrgDir.path() + "/GOG Games/";
        const QString D = Root + "Duke Nukem 3D/";
        WriteFile(D + "goggame-1207658730.info", "{\n"
" \"gameId\": \"1207658730\", \"name\": \"Duke Nukem 3D: Atomic Edition\",\n"
" \"playTasks\": [\n"
"  {\"arguments\": \"-conf \\\"..\\\\dosboxDuke3D.conf\\\" -conf \\\"..\\\\dosboxDuke3D_single.conf\\\" -noconsole -c \\\"exit\\\"\",\n"
"   \"category\": \"game\", \"isPrimary\": true, \"name\": \"Duke Nukem 3D\", \"path\": \"DOSBOX\\\\dosbox.exe\", \"type\": \"FileTask\", \"workingDir\": \"DOSBOX\"},\n"
"  {\"category\": \"document\", \"name\": \"Manual\", \"path\": \"Manual.pdf\", \"type\": \"FileTask\"},\n"
"  {\"arguments\": \"-conf \\\"..\\\\dosboxDuke3D.conf\\\" -conf \\\"..\\\\dosboxDuke3D_settings.conf\\\" -noconsole -c \\\"exit\\\"\",\n"
"   \"category\": \"tool\", \"name\": \"Settings\", \"path\": \"DOSBOX\\\\dosbox.exe\", \"type\": \"FileTask\", \"workingDir\": \"DOSBOX\"}\n"
" ]\n"
"}");
        WriteFile(D + "goggame-1207658730.ico", "ico");
        WriteFile(D + "DOSBOX/dosbox.exe", "exe");
        WriteFile(D + "dosboxDuke3D.conf", "[sdl]\r\nfullscreen=true\r\noutput=opengl\r\n\r\n[cpu]\r\ncycles=fixed 20000\r\n\r\n[autoexec]\r\n# nothing\r\n");
        WriteFile(D + "dosboxDuke3D_single.conf", "[autoexec]\r\n@echo off\r\nmount C \"..\"\r\nimgmount d \"..\\game.ins\" -t iso -fs iso\r\nc:\r\ncls\r\ncd DUKE3D\r\nDUKE3D.EXE /nosound\r\nexit\r\n");
        WriteFile(D + "dosboxDuke3D_settings.conf", "[autoexec]\r\n@echo off\r\nmount C \"..\"\r\nc:\r\ncd duke3d\r\nsetup\r\nexit\r\n");
        WriteFile(D + "game.ins", "FILE \"game.gog\" BINARY\r\n");
        WriteFile(D + "Duke3D/DUKE3D.EXE", "exe");
        WriteFile(D + "Duke3D/SETUP.EXE", "exe");
        // (a GOG game that is no DOS game)
        WriteFile(Root + "Other Game/goggame-1.info", "{\"gameId\": \"1\", \"name\": \"Other Game\", \"playTasks\": [{\"category\": \"game\", \"isPrimary\": true, \"path\": \"game.exe\", \"type\": \"FileTask\"}]}");

        const QList<TGOGGame> Games = FindGOGGames(Root);
        QCOMPARE(Games.size(), 1);
        const TGOGGame& G = Games[0];
        QCOMPARE(G.Name, QString("Duke Nukem 3D: Atomic Edition"));
        QCOMPARE(G.ID, QString("1207658730"));
        QVERIFY(SamePath(G.WorkingDir, D + "DOSBOX"));
        QCOMPARE(G.GameConfs.size(), 2);
        QCOMPARE(G.SetupConfs.size(), 2);
        QVERIFY(SamePath(G.Icon, D + "goggame-1207658730.ico"));

        TGameDB DB(TempPrgDir + "GOGConfs" + PathDelim);
        TGame* P = ImportGOGGame(&DB, G);
        QVERIFY(P != nullptr);
        QCOMPARE(P->Name(), G.Name);
        QCOMPARE(P->NrOfMounts(), 2);
        QStringList M0 = P->Mount0().split(';'), M1 = P->Mount1().split(';');
        QVERIFY(SamePath(MakeAbsPath(M0[0], PrgSetup->BaseDir()), D));
        QCOMPARE(M0[1], QString("Drive"));
        QCOMPARE(M0[2], QString("C"));
        QVERIFY(SamePath(MakeAbsPath(M1[0], PrgSetup->BaseDir()), D + "game.ins"));
        QCOMPARE(M1[1], QString("CDROMIMAGE"));
        QCOMPARE(M1[2], QString("D"));
        QVERIFY(SamePath(MakeAbsPath(P->GameExe(), PrgSetup->BaseDir()), D + "Duke3D/DUKE3D.EXE"));
        QCOMPARE(P->GameParameters(), QString("/nosound"));
        QVERIFY(SamePath(MakeAbsPath(P->SetupExe(), PrgSetup->BaseDir()), D + "Duke3D/SETUP.EXE"));
        QVERIFY(!P->AutoexecOverridegamestart());
        QVERIFY(Trim(P->Autoexec()).isEmpty());
        QVERIFY(P->StartFullscreen());
        QCOMPARE(P->Cycles(), QString("20000"));
        QVERIFY(GOGGameInList(&DB, G));
        // the conf file for DOSBox: drives and start of the game
        std::unique_ptr<TStringList> Conf(BuildConfFile(P, false, false, -1, nullptr, false));
        QVERIFY(Conf != nullptr);
        const QString Text = Conf->ToQStringList().join("\n");
        QVERIFY2(Text.contains(QRegularExpression("^mount C ", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)), qPrintable(Text));
        QVERIFY2(Text.contains(QRegularExpression("^imgmount D .*game\\.ins", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)), qPrintable(Text));
        QVERIFY2(Text.contains(QRegularExpression("^DUKE3D\\.EXE /nosound$", QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)), qPrintable(Text));
    }

    void gogLinuxGameAndMenu()
    {
        // Linux version: "gameinfo", "dosbox" folder, conf files next to it, CD folder
        const QString D = FPrgDir.path() + "/GOGLinux/Ultima Underworld/";
        WriteFile(D + "gameinfo", "Ultima Underworld 1+2\n2.0.0.4\n");
        QDir().mkpath(D + "dosbox");
        WriteFile(D + "dosboxUW.conf", "[dosbox]\nmemsize=32\n[autoexec]\n");
        WriteFile(D + "dosboxUW_single.conf", "[autoexec]\nmount C \"../data\"\nmount D \"../data/cd\" -t cdrom -label UW\nc:\ncd uw\nuw.exe\nexit\n");
        WriteFile(D + "data/UW/UW.EXE", "exe");
        QDir().mkpath(D + "data/cd");
        TGOGGame G;
        QVERIFY(ReadGOGGame(D, G));
        QCOMPARE(G.Name, QString("Ultima Underworld 1+2"));
        QVERIFY(SamePath(G.WorkingDir, D + "dosbox"));
        TGameDB DB(TempPrgDir + "GOGConfs2" + PathDelim);
        TGame* P = ImportGOGGame(&DB, G);
        QVERIFY(P != nullptr);
        QCOMPARE(P->NrOfMounts(), 2);
        QCOMPARE(P->Mount1().split(';').value(1), QString("CDROM"));
        QVERIFY(SamePath(MakeAbsPath(P->GameExe(), PrgSetup->BaseDir()), D + "data/UW/UW.EXE"));
        QCOMPARE(P->Memory(), 32);

        // a menu in the autoexec section: kept, it starts the game
        QStringList Mounts;
        const QStringList Rest = GOGMountsFromAutoexec({"mount c \"..\\data\"", "c:", "cd uw", "choice /c12 Which game", "if errorlevel 2 goto two", "uw.exe", "goto end", ":two", "uw2.exe", ":end"}, D + "dosbox", D, Mounts);
        QCOMPARE(Mounts.size(), 1);
        QCOMPARE(Rest.size(), 9);
        QString Exe, Parameters;
        QStringList Before;
        QVERIFY(!GOGProgramFromAutoexec(Rest, Mounts, Exe, Parameters, Before));
        // lines before the program (e.g. "set") stay, "cd\" and ".." work
        QVERIFY(GOGProgramFromAutoexec({"set BLASTER=A220 I7 D1", "c:", "cd\\uw\\..\\uw", "UW /x"}, Mounts, Exe, Parameters, Before));
        QVERIFY(SamePath(Exe, D + "data/UW/UW.EXE"));
        QCOMPARE(Parameters, QString("/x"));
        QCOMPARE(Before, QStringList({"set BLASTER=A220 I7 D1"}));
        // no game: no GOG folder
        TGOGGame None;
        QVERIFY(!ReadGOGGame(D + "data", None));
    }

    void gogScummVMGames()
    {
        // Windows version: goggame-<ID>.info starts ScummVM with "-c ..\\beneath.ini sky"
        const QString D = FPrgDir.path() + "/GOGScummVM/Beneath a Steel Sky/";
        WriteFile(D + "goggame-1207658695.info", "{\"gameId\": \"1207658695\", \"name\": \"Beneath a Steel Sky\", \"playTasks\": [{\"arguments\": \"-c \\\"..\\\\beneath.ini\\\" sky\", \"category\": \"game\", \"isPrimary\": true, \"name\": \"Beneath a Steel Sky\", \"path\": \"ScummVM\\\\scummvm.exe\", \"type\": \"FileTask\", \"workingDir\": \"ScummVM\"}]}");
        WriteFile(D + "ScummVM/scummvm.exe", "exe");
        WriteFile(D + "beneath.ini", "[scummvm]\r\nfullscreen=true\r\n\r\n[sky]\r\ngameid=sky\r\ndescription=Beneath a Steel Sky (v0.0372 CD/DOS)\r\npath=..\\\r\nlanguage=gb\r\n");
        WriteFile(D + "SKY.DSK", "data");
        TGOGGame G;
        QVERIFY(ReadGOGGame(D, G));
        QVERIFY(G.ScummVM);
        QCOMPARE(G.Name, QString("Beneath a Steel Sky"));
        QCOMPARE(G.ScummVMGameID, QString("sky"));
        QVERIFY(SamePath(G.ScummVMDataDir, D));
        QCOMPARE(G.ScummVMLanguage, QString("gb"));
        TGameDB DB(TempPrgDir + "GOGConfs3" + PathDelim);
        TGame* P = ImportGOGGame(&DB, G);
        QVERIFY(P != nullptr);
        QCOMPARE(P->ProfileMode(), QString("ScummVM"));
        QCOMPARE(P->ScummVMGame(), QString("sky"));
        QVERIFY(SamePath(MakeAbsPath(P->ScummVMPath(), PrgSetup->BaseDir()), D));
        QCOMPARE(P->ScummVMLanguage(), QString("gb"));
        QVERIFY(GOGGameInList(&DB, G));
        // found when the folder with all games is chosen
        QCOMPARE(FindGOGGames(FPrgDir.path() + "/GOGScummVM").size(), 1);

        // Linux version: no info file, ini file with "engineid", data folder next to the scummvm folder
        const QString L = FPrgDir.path() + "/GOGScummVM/Linux Sky/";
        WriteFile(L + "gameinfo", "Beneath a Steel Sky\n1.0\n");
        QDir().mkpath(L + "scummvm");
        WriteFile(L + "data/SKY.DSK", "data");
        WriteFile(L + "bass.ini", "[sky-cd]\nengineid=sky\ngameid=sky\npath=../data\n");
        TGOGGame G2;
        QVERIFY(ReadGOGGame(L, G2));
        QVERIFY(G2.ScummVM);
        QCOMPARE(G2.Name, QString("Beneath a Steel Sky"));
        QCOMPARE(G2.ScummVMGameID, QString("sky:sky"));
        QVERIFY(SamePath(G2.ScummVMDataDir, L + "data"));
    }

    // ----- profiles ---------------------------------------------------------

    void profileSaveAndLoad()
    {
        const QString File = TempPrgDir + "Confs" + PathDelim + "Round Trip.prof";
        {
            TGame G(File);
            G.SetName("Round Trip");
            G.SetGameExe("VirtualHD\\GAME\\GAME.EXE");
            G.SetGameParameters("/fast");
            G.SetCycles("fixed 12345");
            G.SetMemory(32);
            G.SetLoadFix(true);
            G.SetLoadFixMemory(48);
            G.SetNrOfMounts(1);
            G.SetMount0("VirtualHD;DRIVE;C;false;");
            G.SetStagingSetting(0, "crt-auto");
            G.SetStagingSetting(StagingSettingCount - 1, "onstart");
            G.StoreAllValues();
            G.UpdateFile();
        }
        QVERIFY(QFileInfo::exists(File));
        TGame G(File);
        QCOMPARE(G.Name(), QString("Round Trip"));
        QCOMPARE(G.GameExe(), QString("VirtualHD\\GAME\\GAME.EXE"));
        QCOMPARE(G.GameParameters(), QString("/fast"));
        QCOMPARE(G.Cycles(), QString("fixed 12345"));
        QCOMPARE(G.Memory(), 32);
        QVERIFY(G.LoadFix());
        QCOMPARE(G.LoadFixMemory(), 48);
        QCOMPARE(G.NrOfMounts(), 1);
        QCOMPARE(G.Mount0(), QString("VirtualHD;DRIVE;C;false;"));
        QCOMPARE(G.StagingSetting(0), QString("crt-auto"));
        QCOMPARE(G.StagingSetting(StagingSettingCount - 1), QString("onstart"));
        // settings that were not set keep their default
        for (int I = 1; I < StagingSettingCount - 1; I++) QCOMPARE(G.StagingSetting(I), QString("default"));
    }

    void profileStagingSettingsInConf()
    {
        // DOSBox Staging settings of the profile are written to the conf file
        // for DOSBox Staging only, "default" settings not at all
        const QString File = TempPrgDir + "Confs" + PathDelim + "Staging Settings.prof";
        {
            TGame G(File);
            G.SetName("Staging Settings");
            G.SetStagingSetting(0, "crt-auto");
            G.StoreAllValues();
            G.UpdateFile();
        }
        const QString Section = QString::fromLatin1(StagingSettings[0].Section);
        const QString Key = QString::fromLatin1(StagingSettings[0].Key);
        const QString OtherKey = QString::fromLatin1(StagingSettings[1].Key);

        SetDOSBoxInstallation("dosbox-staging");
        QStringList Conf = ConfOfTemplate(File);
        QVERIFY(!Conf.isEmpty());
        QCOMPARE(Value(Conf, Section, Key), QString("crt-auto"));
        QVERIFY(!HasKey(Conf, QString(), OtherKey));

        SetDOSBoxInstallation("dosbox-x");
        Conf = ConfOfTemplate(File);
        QVERIFY(!Conf.isEmpty());
        QVERIFY(!HasKey(Conf, QString(), Key));
    }

    // ----- auto setup templates ---------------------------------------------

    void autoSetupTemplates_data()
    {
        QTest::addColumn<QString>("FileName");
        const QDir Dir(QStringLiteral(DFR_SOURCE_DIR "/data/NewUserData/AutoSetup"));
        const QStringList Files = Dir.entryList({"*.prof"}, QDir::Files, QDir::Name);
        QVERIFY2(Files.count() > 100, "auto setup templates not found");
        for (const QString& F : Files) QTest::newRow(qPrintable(F)) << Dir.filePath(F);
    }

    void autoSetupTemplates()
    {
        QFETCH(QString, FileName);
        {
            TGame G(FileName);
            QVERIFY2(!G.Name().trimmed().isEmpty(), "no name");
        }

        static const QList<QPair<QString, QString>> NotInStaging = {
            {"sdl", "windowresolution"}, {"sdl", "autolock"}, {"sdl", "sensitivity"}, {"sdl", "output"},
            {"sdl", "fulldouble"}, {"sdl", "fullresolution"}, {"sdl", "priority"}, {"sdl", "usescancodes"},
            {"sdl", "pixelshader"}, {"sdl", "waitonerror"}, {"dosbox", "captures"}, {"render", "scaler"},
            {"render", "frameskip"}, {"cpu", "cycles"}, {"sblaster", "oplrate"}, {"sblaster", "oplemu"},
            {"gus", "gusrate"}, {"speaker", "pcrate"}, {"speaker", "tandyrate"}, {"speaker", "disney"},
            {"dos", "keyboardlayout"}};
        SetDOSBoxInstallation("dosbox-staging");
        Messages.clear();
        const QStringList Staging = ConfOfTemplate(FileName);
        // (templates mounting a real CD drive need one; the program files of
        // the games are not needed, the "file not found" messages are ignored)
        if (Staging.isEmpty() && Messages.join(' ').contains("CD drive")) QSKIP("the template mounts a CD drive (none in this system)");
        QVERIFY2(!Staging.isEmpty(), qPrintable("no conf file for DOSBox Staging: " + Messages.join(' ')));
        QVERIFY(Sections(Staging).contains("autoexec"));
        for (const auto& [Section, Key] : NotInStaging)
            QVERIFY2(!HasKey(Staging, Section, Key), qPrintable("DOSBox Staging: " + Section + "/" + Key));
        for (const QString& S : {"vga", "glide", "printer", "innova", "ne2000"})
            QVERIFY2(!Sections(Staging).contains(S), qPrintable("DOSBox Staging: section [" + S + "]"));

        SetDOSBoxInstallation("dosbox-x");
        const QStringList X = ConfOfTemplate(FileName);
        QVERIFY2(!X.isEmpty(), "no conf file for DOSBox-X");
        QVERIFY2(!HasKey(X, "dosbox", "vmemsize"), "DOSBox-X: dosbox/vmemsize");
        QVERIFY2(!HasKey(X, "ne2000", "realnic"), "DOSBox-X: ne2000/realnic");
    }
};

QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
