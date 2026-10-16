#include "DOSBoxConfUnit.h"

#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

// Settings of DOSBox 0.74 and their DOSBox Staging names (checked with DOSBox
// Staging 0.83, which logs "Deprecated setting" / "No config section or
// setting exists" for the 0.74 names):
//
//   [sdl]      fullscreen                  unchanged
//              fulldouble, fullresolution, output, waitonerror, priority,
//              usescancodes, pixelshader   removed (Staging: OpenGL output,
//                                          fullscreen_mode=standard)
//              windowresolution=WxH        window_size=WxH ("original": default)
//              autolock=true/false         [mouse] mouse_capture=onclick/seamless
//              sensitivity                 [mouse] mouse_sensitivity
//              mapperfile=X.map            mapperfile=X-sdl2.map (the key codes
//                                          of SDL 2 differ from DOSBox 0.74)
//   [dosbox]   captures                    [capture] capture_dir
//              machine=vgaonly             machine=svga_paradise
//   [render]   frameskip, scaler           removed (Staging: shader setting)
//              aspect=true                 aspect=on (false: Staging default)
//   [cpu]      cycles=fixed N / N          cpu_cycles=N, cpu_cycles_protected=N
//              cycles=max                  cpu_cycles=max, cpu_cycles_protected=max
//              cycles=auto                 cpu_cycles=3000, cpu_cycles_protected=max
//              (other cycles values are kept, Staging still accepts them)
//   [midi]     mididevice=default/win32/alsa/oss/coreaudio/coremidi
//                                          mididevice=port
//   [sblaster] oplrate, oplemu             removed (Staging: resampled, nuked)
//   [gus]      gusrate                     removed
//   [speaker]  pcspeaker=true/false        pcspeaker=impulse/off
//              pcrate, tandyrate           removed
//              disney=true                 lpt_dac=disney
//   [innova]   innova                      [innovation] innovation
//              samplerate, sidbase, quality removed
//   [dos]      keyboardlayout              keyboard_layout ("none": us)
//   [ne2000]   ne2000, nicbase, nicirq, macaddr
//                                          [ethernet] (same names)
//              realnic                     removed (Staging uses slirp)
//   [vga], [glide], [printer]              removed (not in DOSBox Staging)
//
// DOSBox-X (checked with 2024.03.01) still reads the DOSBox 0.74 names; only
// two settings are in other sections (DOSBox-X logs "Redirected" / "Migrated"):
//
//   [dosbox]   vmemsize                    [video] vmemsize
//   [ne2000]   realnic                     [ethernet, pcap] realnic

namespace {

struct TSetting {
    QString Section; // empty: the section of the line
    QString Key;
    QString Value;
};

bool IsTrue(const QString& V) { return V == QLatin1String("true") || V == QLatin1String("1") || V == QLatin1String("on"); }

QString MapperFileForSDL2(const QString& FileName)
{
    if (FileName.isEmpty() || FileName.contains(QLatin1String("sdl2"), Qt::CaseInsensitive)) return FileName;
    int Dot = FileName.lastIndexOf('.');
    const int Delim = qMax(FileName.lastIndexOf('/'), FileName.lastIndexOf('\\'));
    if (Dot <= Delim) Dot = FileName.length();
    return FileName.left(Dot) + QStringLiteral("-sdl2") + FileName.mid(Dot);
}

// DOSBox 0.74 settings without effect in DOSBox Staging ("*": whole section)
const QMap<QString, QStringList>& StagingIgnoredSettings()
{
    static const QMap<QString, QStringList> Map = {
        {QStringLiteral("sdl"), {QStringLiteral("fulldouble"), QStringLiteral("fullresolution"), QStringLiteral("output"), QStringLiteral("waitonerror"), QStringLiteral("priority"), QStringLiteral("usescancodes"), QStringLiteral("pixelshader")}},
        {QStringLiteral("render"), {QStringLiteral("frameskip"), QStringLiteral("scaler")}},
        {QStringLiteral("vga"), {QStringLiteral("*")}},
        {QStringLiteral("glide"), {QStringLiteral("*")}},
        {QStringLiteral("printer"), {QStringLiteral("*")}},
        {QStringLiteral("sblaster"), {QStringLiteral("oplrate"), QStringLiteral("oplemu")}},
        {QStringLiteral("gus"), {QStringLiteral("gusrate")}},
        {QStringLiteral("speaker"), {QStringLiteral("pcrate"), QStringLiteral("tandyrate")}},
        {QStringLiteral("innova"), {QStringLiteral("samplerate"), QStringLiteral("sidbase"), QStringLiteral("quality")}},
        {QStringLiteral("ne2000"), {QStringLiteral("realnic")}},
    };
    return Map;
}

bool StagingIgnored(const QString& Section, const QString& Key)
{
    const auto It = StagingIgnoredSettings().constFind(Section);
    return (It != StagingIgnoredSettings().constEnd()) && (It->contains(QStringLiteral("*")) || It->contains(Key));
}

// Returns false if the setting is not known here (keep it unchanged);
// otherwise Result holds the converted settings (empty: leave it out).
bool ConvertSettingStaging(const QString& Section, const QString& Key, const QString& RawValue, QList<TSetting>& Result)
{
    const QString V = RawValue.trimmed().toLower();
    auto Add = [&Result](const QString& S, const QString& K, const QString& Val) { Result.append({S, K, Val}); };

    if (StagingIgnored(Section, Key)) return true;
    if (Section == QLatin1String("sdl")) {
        if (Key == QLatin1String("windowresolution")) {
            static const QRegularExpression SizeRx(QStringLiteral("^\\d+x\\d+$"));
            if (SizeRx.match(V).hasMatch()) Add(QString(), QStringLiteral("window_size"), V);
            return true;
        }
        if (Key == QLatin1String("autolock")) { Add(QStringLiteral("mouse"), QStringLiteral("mouse_capture"), IsTrue(V) ? QStringLiteral("onclick") : QStringLiteral("seamless")); return true; }
        if (Key == QLatin1String("sensitivity")) { Add(QStringLiteral("mouse"), QStringLiteral("mouse_sensitivity"), RawValue.trimmed()); return true; }
        if (Key == QLatin1String("mapperfile")) { Add(QString(), Key, MapperFileForSDL2(RawValue.trimmed())); return true; }
        return false;
    }
    if (Section == QLatin1String("dosbox")) {
        if (Key == QLatin1String("captures")) { Add(QStringLiteral("capture"), QStringLiteral("capture_dir"), RawValue.trimmed()); return true; }
        if (Key == QLatin1String("machine") && V == QLatin1String("vgaonly")) { Add(QString(), Key, QStringLiteral("svga_paradise")); return true; }
        return false;
    }
    if (Section == QLatin1String("render")) {
        if (Key == QLatin1String("aspect")) {
            if (IsTrue(V)) Add(QString(), Key, QStringLiteral("on"));
            else if (V != QLatin1String("false") && V != QLatin1String("0") && V != QLatin1String("off")) Add(QString(), Key, RawValue.trimmed());
            return true;
        }
        return false;
    }
    if (Section == QLatin1String("cpu")) {
        if (Key != QLatin1String("cycles")) return false;
        static const QRegularExpression FixedRx(QStringLiteral("^(fixed\\s+)?(\\d+)$"));
        const QRegularExpressionMatch M = FixedRx.match(V);
        QString Real, Protected;
        if (M.hasMatch()) { Real = M.captured(2); Protected = Real; }
        else if (V == QLatin1String("max")) { Real = V; Protected = V; }
        else if (V == QLatin1String("auto")) { Real = QStringLiteral("3000"); Protected = QStringLiteral("max"); }
        else return false;
        Add(QString(), QStringLiteral("cpu_cycles"), Real);
        Add(QString(), QStringLiteral("cpu_cycles_protected"), Protected);
        return true;
    }
    if (Section == QLatin1String("midi")) {
        if (Key != QLatin1String("mididevice")) return false;
        static const QStringList HostDevices = {QStringLiteral("default"), QStringLiteral("win32"), QStringLiteral("alsa"), QStringLiteral("oss"), QStringLiteral("coreaudio"), QStringLiteral("coremidi"), QString()};
        if (!HostDevices.contains(V)) return false;
        Add(QString(), Key, QStringLiteral("port"));
        return true;
    }
    if (Section == QLatin1String("speaker")) {
        if (Key == QLatin1String("pcspeaker")) {
            if (IsTrue(V)) Add(QString(), Key, QStringLiteral("impulse"));
            else if (V == QLatin1String("false") || V == QLatin1String("0")) Add(QString(), Key, QStringLiteral("off"));
            else return false;
            return true;
        }
        if (Key == QLatin1String("disney")) { if (IsTrue(V)) Add(QString(), QStringLiteral("lpt_dac"), QStringLiteral("disney")); return true; }
        return false;
    }
    if (Section == QLatin1String("innova")) {
        if (Key == QLatin1String("innova")) Add(QStringLiteral("innovation"), QStringLiteral("innovation"), IsTrue(V) ? QStringLiteral("on") : QStringLiteral("off"));
        else Add(QStringLiteral("innovation"), Key, RawValue.trimmed());  // (unknown setting: keep it)
        return true;
    }
    if (Section == QLatin1String("dos")) {
        if (Key != QLatin1String("keyboardlayout")) return false;
        Add(QString(), QStringLiteral("keyboard_layout"), (V == QLatin1String("none")) ? QStringLiteral("us") : RawValue.trimmed());
        return true;
    }
    if (Section == QLatin1String("ne2000")) {
        Add(QStringLiteral("ethernet"), Key, RawValue.trimmed());
        return true;
    }
    return false;
}

bool ConvertSettingX(const QString& Section, const QString& Key, const QString& RawValue, QList<TSetting>& Result)
{
    if (Section == QLatin1String("dosbox") && Key == QLatin1String("vmemsize")) { Result.append({QStringLiteral("video"), Key, RawValue.trimmed()}); return true; }
    if (Section == QLatin1String("ne2000") && Key == QLatin1String("realnic")) { Result.append({QStringLiteral("ethernet, pcap"), Key, RawValue.trimmed()}); return true; }
    return false;
}

using TConvertSetting = bool (*)(const QString&, const QString&, const QString&, QList<TSetting>&);

} // namespace

static QStringList ConvertConf(const QStringList& Lines, TConvertSetting Convert, const QStringList& SkipSections)
{
    QStringList Result;
    QMap<QString, QStringList> Moved;  // settings for other sections
    QStringList MovedOrder;
    QString Section;
    int AutoexecLine = -1;

    // settings already written with the new names (e.g. the DOSBox Staging
    // page of the profile editor or custom settings) win over converted ones
    QSet<QString> Native;
    for (const QString& Line : Lines) {
        const QString T = Line.trimmed();
        if (T.startsWith('[') && T.endsWith(']')) { Section = T.mid(1, T.length() - 2).trimmed().toLower(); continue; }
        const int Eq = T.indexOf('=');
        if (Section == QLatin1String("autoexec") || Eq <= 0 || T.startsWith('#') || T.startsWith(';')) continue;
        QList<TSetting> Dummy;
        const QString Key = T.left(Eq).trimmed().toLower();
        if (!Convert(Section, Key, T.mid(Eq + 1), Dummy)) Native.insert(Section + '/' + Key);
    }
    Section.clear();

    for (const QString& Line : Lines) {
        const QString T = Line.trimmed();
        if (T.startsWith('[') && T.endsWith(']')) {
            Section = T.mid(1, T.length() - 2).trimmed().toLower();
            if (Section == QLatin1String("autoexec") && AutoexecLine < 0) AutoexecLine = Result.count();
            // (sections whose settings are all left out or go to other sections)
            if (!SkipSections.contains(Section)) Result.append(Line);
            continue;
        }
        const int Eq = T.indexOf('=');
        if (Section == QLatin1String("autoexec") || Eq <= 0 || T.startsWith('#') || T.startsWith(';')) {
            if (!SkipSections.contains(Section)) Result.append(Line);
            continue;
        }
        const QString Key = T.left(Eq).trimmed().toLower();
        QList<TSetting> Converted;
        if (!Convert(Section, Key, T.mid(Eq + 1), Converted)) {
            if (SkipSections.contains(Section)) {  // (not possible with the tables above)
                if (!Moved.contains(Section)) MovedOrder.append(Section);
                Moved[Section].append(T);
            } else Result.append(Line);
            continue;
        }
        for (const TSetting& S : Converted) {
            if (Native.contains((S.Section.isEmpty() ? Section : S.Section) + '/' + S.Key)) continue;
            const QString NewLine = S.Key + '=' + S.Value;
            if (S.Section.isEmpty() || S.Section == Section) { Result.append(NewLine); continue; }
            if (!Moved.contains(S.Section)) MovedOrder.append(S.Section);
            Moved[S.Section].append(NewLine);
        }
    }

    if (!MovedOrder.isEmpty()) {
        QStringList Block;
        for (const QString& S : MovedOrder) Block << QStringLiteral("[%1]").arg(S) << Moved[S] << QString();
        int Pos = (AutoexecLine >= 0) ? AutoexecLine : Result.count();
        if (Pos == Result.count() && !Result.isEmpty() && !Result.last().trimmed().isEmpty()) Block.prepend(QString());
        for (const QString& L : Block) Result.insert(Pos++, L);
    }
    return Result;
}

QStringList ConvertConfForDOSBox(TDOSBoxKind Kind, const QStringList& Lines)
{
    switch (Kind) {
        case dkStaging:
            return ConvertConf(Lines, ConvertSettingStaging, {QStringLiteral("vga"), QStringLiteral("glide"), QStringLiteral("printer"), QStringLiteral("innova"), QStringLiteral("ne2000")});
        case dkDOSBoxX:
            return ConvertConf(Lines, ConvertSettingX, {});
        default:
            return Lines;
    }
}

bool DOSBoxSettingIgnored(TDOSBoxKind Kind, const QString& Section, const QString& Key)
{
    return (Kind == dkStaging) && StagingIgnored(Section.toLower(), Key.toLower());
}
