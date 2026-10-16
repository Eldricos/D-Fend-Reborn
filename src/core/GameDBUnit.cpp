// C++ port of GameDBUnit.pas (implementation section).
#include "GameDBUnit.h"
#include <QSet>
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "WaitFormUnit.h"
#include "DosBoxUnit.h"
#include "LoggingUnit.h"

#include <QApplication>
#include <QMainWindow>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <cmath>

TGame* DefaultValueReaderGame = nullptr;

// Application.MainForm
static QWidget *MainFormWidget()
{
    if (!qApp) return nullptr;
    for (QWidget *W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow *>(W) != nullptr) return W;
    return nullptr;
}

// DOS packed file date of a file (as TSearchRec.Time)
static int GetDOSFileDate(const QString &FileName)
{
    return FileAge(FileName);
}

/* TConfOpt */

TConfOpt::TConfOpt() : TBasePrgSetup(PrgDataDir() + SettingsFolder + PathDelim + ConfOptFile)
{
    AddStringRec(0, "resolution", "value", DefaultValuesResolutionFullscreen);
    AddStringRec(1, "resolutionWindow", "value", DefaultValuesResolutionWindow);
    AddStringRec(2, "joysticks", "value", DefaultValuesJoysticks);
    AddStringRec(3, "scale", "value", DefaultValuesScale);
    AddStringRec(4, "render", "value", DefaultValueRender);
    AddStringRec(5, "cycles", "value", DefaultValueCycles);
    AddStringRec(6, "video", "value", DefaultValuesVideo);
    AddStringRec(7, "memory", "value", DefaultValuesMemory);
    AddStringRec(8, "frameskip", "value", DefaultValuesFrameSkip);
    AddStringRec(9, "core", "value", DefaultValuesCore);
    AddStringRec(10, "sblaster", "value", DefaultValuesSBlaster);
    AddStringRec(11, "oplmode", "value", DefaultValuesOPLModes);
    AddStringRec(12, "keyboardlayout", "value", DefaultValuesKeyboardLayout);
    AddStringRec(13, "codepage", "value", DefaultValuesCodepage);
    AddStringRec(14, "ReportedDOSVersion", "value", DefaultValuesReportedDOSVersion);
    AddStringRec(15, "MIDIDevice", "value", DefaultValuesMIDIDevice);
    AddStringRec(16, "Blocksize", "value", DefaultValuesBlocksize);
    AddStringRec(17, "CyclesDown", "value", DefaultValuesCyclesDown);
    AddStringRec(18, "CyclesUp", "value", DefaultValuesCyclesUp);
    AddStringRec(19, "DMA", "value", DefaultValuesDMA);
    AddStringRec(20, "DMA1", "value", DefaultValuesDMA1);
    AddStringRec(21, "GUSBase", "value", DefaultValuesGUSBase);
    AddStringRec(22, "GUSRate", "value", DefaultValuesGUSRate);
    AddStringRec(23, "HDMA", "value", DefaultValuesHDMA);
    AddStringRec(24, "IRQ", "value", DefaultValuesIRQ);
    AddStringRec(25, "IRQ1", "value", DefaultValuesIRQ1);
    AddStringRec(26, "MPU401", "value", DefaultValuesMPU401);
    AddStringRec(27, "OPLRate", "value", DefaultValuesOPLRate);
    AddStringRec(28, "PCRate", "value", DefaultValuesPCRate);
    AddStringRec(29, "Rate", "value", DefaultValuesRate);
    AddStringRec(30, "SBBase", "value", DefaultValuesSBBase);
    AddStringRec(31, "MouseSensitivity", "value", DefaultValuesMouseSensitivity);
    AddStringRec(32, "TandyRate", "value", DefaultValuesTandyRate);
    AddStringRec(33, "ScummVMFilter", "value", DefaultValuesScummVMFilter);
    AddStringRec(34, "ScummVMMusicDriver", "value", DefaultValuesScummVMMusicDriver);
    AddStringRec(35, "VGAChipsets", "value", DefaultValuesVGAChipsets);
    AddStringRec(36, "VGAVideoRAM", "value", DefaultValuesVGAVideoRAM);
    AddStringRec(37, "ScummVMRenderMode", "value", DefaultValuesScummVMRenderMode);
    AddStringRec(38, "ScummVMPlatform", "value", DefaultValuesScummVMPlatform);
    AddStringRec(39, "ScummVMLanguages", "value", DefaultValuesScummVMLanguages);
    AddStringRec(40, "CPUType", "value", DefaultValuesCPUType);
    AddStringRec(41, "SBOplEmu", "value", DefaultValuesOplEmu);
    AddStringRec(42, "GlideEmulation", "value", DefaultValuesGlideEmulation);
    AddStringRec(43, "GlideEmulationPort", "value", DefaultValuesGlideEmulationPort);
    AddStringRec(44, "GlideEmulationLFB", "value", DefaultValuesGlideEmulationLFB);
    AddStringRec(45, "InnovaEmulationSampleRate", "value", DefaultValuesInnovaEmulationSampleRate);
    AddStringRec(46, "InnovaEmulationBaseAddress", "value", DefaultValuesInnovaEmulationBaseAddress);
    AddStringRec(47, "InnovaEmulationQuality", "value", DefaultValuesInnovaEmulationQuality);
    AddStringRec(48, "NE2000EmulationBaseAddress", "value", DefaultValuesNE2000EmulationBaseAddress);
    AddStringRec(49, "NE2000EmulationInterrupt", "value", DefaultValuesNE2000EmulationInterrupt);
    AddStringRec(50, "MT32ReverbMode", "value", DefaultValuesMT32ReverbMode);
    AddStringRec(51, "MT32ReverbTime", "value", DefaultValuesMT32ReverbTime);
    AddStringRec(52, "MT32ReverbLevel", "value", DefaultValuesMT32ReverbLevel);
    AddStringRec(NR_ConfOptValuesVersion, "Values", "Version", "");
    for (int I = 0; I < StagingSettingCount; I++) AddStringRec(NR_ConfOptStagingFirst + I, QStringLiteral("Staging.") + QLatin1String(StagingSettings[I].Key), "value", StagingDefaultValues(I));

    CacheAllStrings();
    UpgradeValueLists();
}

// D-Fend Reborn 0.5.5: values of DOSBox Staging and DOSBox-X added to the
// lists; older ConfOpt.dat files get the values they do not have yet (the
// values the user added or removed stay as they are otherwise)
void TConfOpt::UpgradeValueLists()
{
    if (GetString(NR_ConfOptValuesVersion) == QLatin1String("2")) return;
    const struct { int Nr; QString Defaults; } Lists[] = {
        {6, DefaultValuesVideo}, {40, DefaultValuesCPUType}, {10, DefaultValuesSBlaster}, {11, DefaultValuesOPLModes},
        {41, DefaultValuesOplEmu}, {15, DefaultValuesMIDIDevice}, {4, DefaultValueRender}, {2, DefaultValuesJoysticks},
        {14, DefaultValuesReportedDOSVersion},
    };
    auto KeyOf = [](QString S) { S = S.trimmed(); const int P = S.indexOf('('); if (P > 0) S = S.left(P).trimmed(); return S.toLower(); };
    for (const auto& L : Lists) {
        QStringList Current = GetString(L.Nr).split(',');
        QSet<QString> Keys;
        for (const QString& S : Current) Keys.insert(KeyOf(S));
        bool Changed = false;
        for (const QString& S : L.Defaults.split(',')) if (!Keys.contains(KeyOf(S))) { Current.append(S.trimmed()); Keys.insert(KeyOf(S)); Changed = true; }
        if (Changed) SetString(L.Nr, Current.join(','));
    }
    SetString(NR_ConfOptValuesVersion, QStringLiteral("2"));
}

TConfOpt::~TConfOpt()
{
    StoreAllValues();
    DestroySetup();
}

/* TGame */

TGame::TGame(const QString& ASetupFile) : TBasePrgSetup(ASetupFile)
{
    FGameDB = nullptr;
    InitData();
    LoadCache();
}

TGame::TGame(const QString& ASetupFile, bool ATimeStampCheck, bool ADelayed) : TBasePrgSetup(ASetupFile, ATimeStampCheck)
{
    Q_UNUSED(ADelayed);
    FGameDB = nullptr;
}

TGame* TGame::CreateDelayed(const QString& ASetupFile, bool ATimeStampCheck)
{
    // If ATimeStampCheck then inherited Create(ASetupFile) else inherited CreateNoTimeStampCheck(ASetupFile);
    return new TGame(ASetupFile, ATimeStampCheck, true);
}

TGame::TGame(TBasePrgSetup* ABasePrgSetup) : TBasePrgSetup(ABasePrgSetup)
{
    FGameDB = nullptr;
    InitData();
    LoadCache();
}

TGame::~TGame()
{
    /* not needed: StoreAllValues; */
    DestroySetup();   // inherited Destroy (so that TGame::UpdateFile is used)
}

void TGame::InitData()
{
    QString S;
    int I;

    S = ExtractFileName(SetupFile());
    while ((S != "") && (S[Length(S) - 1] != '.')) Delete(S, Length(S), 1);
    if ((S != "") && (S[Length(S) - 1] == '.')) Delete(S, Length(S), 1);

    FastAddRecStart();


    AddStringRec(NR_Name, "ExtraInfo", "Name", S);

    AddStringRec(NR_Icon, "ExtraInfo", "Icon", "");
    AddStringRec(NR_GameExe, "Extra", "Exe", "");
    AddStringRec(NR_GameExeMD5, "Extra", "ExeMD5", "");
    AddStringRec(NR_SetupExe, "Extra", "Setup", "");
    AddStringRec(NR_SetupExeMD5, "Extra", "SetupMD5", "");
    AddStringRec(NR_GameParameters, "Extra", "GameParameters", "");
    AddStringRec(NR_SetupParameters, "Extra", "SetupParameters", "");
    AddBooleanRec(NR_LoadFix, "Extra", "Loadhigh", false);
    AddIntegerRec(NR_LoadFixMemory, "Extra", "LoadFixVal", 64);
    AddStringRec(NR_CaptureFolder, "dosbox", "captures", IncludeTrailingPathDelimiter(PrgSetup->CaptureDir()));
    AddStringRec(NR_ExtraDirs, "Extra", "ExtraDirs", "");
    AddStringRec(NR_ExtraFiles, "Extra", "ExtraFiles", "");
    AddStringRec(NR_LastModification, "Extra", "LastModification", "");
    AddBooleanRec(NR_IgnoreWindowsFileWarnings, "Extras", "IgnoreWindowsFileWarnings", false);
    AddBooleanRec(NR_NoDOSBoxFailedDialog, "Extras", "NoDOSBoxFailedDialog", false);
    AddStringRec(NR_ScreenshotListScreenshot, "Extra", "ScreenshotListScreenshot", "");

    for (I = 0; I <= 49; I++) {
    AddStringRec(NR_ExtraPrgFile+I, "Extra", "ExtraExe" + IntToStr(I), "");
    }
    for (I = 0; I <= 49; I++) {
    AddStringRec(NR_ExtraPrgFileParameter+I, "Extra", "ExtraParameter" + IntToStr(I), "");
    }

    AddStringRec(NR_Genre, "ExtraInfo", "Genre", "");
    AddStringRec(NR_Developer, "ExtraInfo", "Developer", "");
    AddStringRec(NR_Publisher, "ExtraInfo", "Publisher", "");
    AddStringRec(NR_Year, "ExtraInfo", "Year", "");
    AddStringRec(NR_DataDir, "Extra", "DataDir", "");
    AddBooleanRec(NR_Favorite, "ExtraInfo", "Favorite", false);
    AddStringRec(NR_Notes, "ExtraInfo", "Notes", "");
    AddStringRec(NR_Language, "ExtraInfo", "Language", "");
    AddStringRec(NR_UserInfo, "ExtraInfo", "UserInfo", "");
    AddStringRec(NR_WWW1Name, "ExtraInfo", "WWWName", "");
    AddStringRec(NR_WWW1, "ExtraInfo", "WWW", "");
    AddStringRec(NR_WWW2Name, "ExtraInfo", "WWW2Name", "");
    AddStringRec(NR_WWW2, "ExtraInfo", "WWW2", "");
    AddStringRec(NR_WWW3Name, "ExtraInfo", "WWW3Name", "");
    AddStringRec(NR_WWW3, "ExtraInfo", "WWW3", "");
    AddStringRec(NR_WWW4Name, "ExtraInfo", "WWW4Name", "");
    AddStringRec(NR_WWW4, "ExtraInfo", "WWW4", "");
    AddStringRec(NR_WWW5Name, "ExtraInfo", "WWW5Name", "");
    AddStringRec(NR_WWW5, "ExtraInfo", "WWW5", "");
    AddStringRec(NR_WWW6Name, "ExtraInfo", "WWW6Name", "");
    AddStringRec(NR_WWW6, "ExtraInfo", "WWW6", "");
    AddStringRec(NR_WWW7Name, "ExtraInfo", "WWW7Name", "");
    AddStringRec(NR_WWW7, "ExtraInfo", "WWW7", "");
    AddStringRec(NR_WWW8Name, "ExtraInfo", "WWW8Name", "");
    AddStringRec(NR_WWW8, "ExtraInfo", "WWW8", "");
    AddStringRec(NR_WWW9Name, "ExtraInfo", "WWW9Name", "");
    AddStringRec(NR_WWW9, "ExtraInfo", "WWW9", "");

    AddBooleanRec(NR_CloseDosBoxAfterGameExit, "Extra", "CloseOnExit", true);
    AddIntegerRec(NR_ShowConsoleWindow, "sdl", "ConsoleWindow", 1);
    AddBooleanRec(NR_StartFullscreen, "sdl", "fullscreen", true);
    AddBooleanRec(NR_AutoLockMouse, "sdl", "autolock", true);
    AddBooleanRec(NR_Force2ButtonMouseMode, "dos", "Force2ButtonMouseMode", false);
    AddBooleanRec(NR_SwapMouseButtons, "dos", "SwapMouseButtons", false);
    AddBooleanRec(NR_UseDoublebuffering, "sdl", "fulldouble", false);
    AddBooleanRec(NR_AspectCorrection, "render", "aspect", false);
    AddBooleanRec(NR_UseScanCodesOld, "sdl", "usecancodes", true);
    AddBooleanRec(NR_UseScanCodes, "sdl", "usescancodes", true);
    AddIntegerRec(NR_MouseSensitivity, "sdl", "sensitivity", 100);
    AddStringRec(NR_Render, "sdl", "output", "surface");
    AddStringRec(NR_WindowResolution, "sdl", "windowresolution", "original");
    AddStringRec(NR_FullscreenResolution, "sdl", "fullresolution", "original");
    AddStringRec(NR_Scale, "render", "scaler", "normal2x");
    AddIntegerRec(NR_TextModeLines, "render", "TextModeLines", 25);
    AddStringRec(NR_Priority, "sdl", "priority", "higher,normal");
    AddStringRec(NR_CustomDOSBoxDir, "dosbox", "DOSBoxDirectory", "default");
    AddStringRec(NR_CustomKeyMappingFile, "sdl", "mapperfile", "default");
    AddStringRec(NR_CustomDOSBoxLanguage, "sdl", "DOSBoxlanguage", "default");

    AddIntegerRec(NR_Memory, "dosbox", "memsize", 16);
    AddBooleanRec(NR_XMS, "dos", "xms", true);
    AddBooleanRec(NR_EMS, "dos", "ems", true);
    AddBooleanRec(NR_UMB, "dos", "umb", true);
    AddStringRec(NR_Core, "cpu", "core", "auto");
    AddStringRec(NR_Cycles, "cpu", "cycles", "auto");
    AddIntegerRec(NR_CyclesUp, "cpu", "cyclesup", 10);
    AddIntegerRec(NR_CyclesDown, "cpu", "cyclesdown", 20);
    AddStringRec(NR_CPUType, "cpu", "type", "auto");
    AddIntegerRec(NR_FrameSkip, "render", "frameskip", 0);
    AddStringRec(NR_VideoCard, "dosbox", "machine", "svga_s3");
    AddStringRec(NR_KeyboardLayout, "dos", "keyboardlayout", "default");
    AddStringRec(NR_Codepage, "dos", "codepage", "default");
    AddStringRec(NR_Serial1, "serial", "serial1", "dummy");
    AddStringRec(NR_Serial2, "serial", "serial2", "dummy");
    AddStringRec(NR_Serial3, "serial", "serial3", "disabled");
    AddStringRec(NR_Serial4, "serial", "serial4", "disabled");
    AddBooleanRec(NR_IPX, "ipx", "ipx", false);
    AddStringRec(NR_IPXType, "ipx", "type", "none");
    AddStringRec(NR_IPXAddress, "ipx", "address", "");
    AddStringRec(NR_IPXPort, "ipx", "port", "213");
    AddBooleanRec(NR_Use4DOS, "dos", "4DOS", false);
    AddBooleanRec(NR_UseDOS32A, "dos", "DOS32A", false);
    AddStringRec(NR_ReportedDOSVersion, "dos", "ReportedDOSVersion", "default");
    AddStringRec(NR_NumLockStatus, "dos", "NumLockStatus", "");
    AddStringRec(NR_CapsLockStatus, "dos", "CapsLockStatus", "");
    AddStringRec(NR_ScrollLockStatus, "dos", "ScrollLockStatus", "");
    AddStringRec(NR_VGAChipset, "vga", "svgachipset", "3s");
    AddIntegerRec(NR_VideoRam, "vga", "videoram", 2048);
    AddStringRec(NR_GlideEmulation, "glide", "glide", "false");
    AddStringRec(NR_GlidePort, "glide", "port", "600");
    AddStringRec(NR_GlideLFB, "glide", "LFB", "full");
    AddStringRec(NR_PixelShader, "vga", "PixelShader", "none");

    AddBooleanRec(NR_EnablePrinterEmulation, "printer", "printer", false);
    AddIntegerRec(NR_PrinterResolution, "printer", "dpi", 360);
    AddIntegerRec(NR_PaperWidth, "printer", "width", 85);
    AddIntegerRec(NR_PaperHeight, "printer", "height", 110);
    AddStringRec(NR_PrinterOutputFormat, "printer", "printoutput", "ps");
    AddBooleanRec(NR_PrinterMultiPage, "printer", "multipage", false);

    AddIntegerRec(NR_NrOfMounts, "Extra", "NrOfMounts", 0);
    AddStringRec(NR_Mount0, "Extra", "0", "");
    AddStringRec(NR_Mount1, "Extra", "1", "");
    AddStringRec(NR_Mount2, "Extra", "2", "");
    AddStringRec(NR_Mount3, "Extra", "3", "");
    AddStringRec(NR_Mount4, "Extra", "4", "");
    AddStringRec(NR_Mount5, "Extra", "5", "");
    AddStringRec(NR_Mount6, "Extra", "6", "");
    AddStringRec(NR_Mount7, "Extra", "7", "");
    AddStringRec(NR_Mount8, "Extra", "8", "");
    AddStringRec(NR_Mount9, "Extra", "9", "");
    AddBooleanRec(NR_AutoMountCDs, "Extra", "AutoMountCDs", false);
    AddBooleanRec(NR_SecureMode, "Extra", "SecureMode", true);

    AddBooleanRec(NR_MixerNosound, "mixer", "nosound", false);
    AddIntegerRec(NR_MixerRate, "mixer", "rate", 44100);
    AddIntegerRec(NR_MixerBlocksize, "mixer", "blocksize", 1024);
    AddIntegerRec(NR_MixerPrebuffer, "mixer", "prebuffer", 10);
    AddIntegerRec(NR_MixerVolumeMasterLeft, "mixer", "VolumeMasterLeft", 100);
    AddIntegerRec(NR_MixerVolumeMasterRight, "mixer", "VolumeMasterRight", 100);
    AddIntegerRec(NR_MixerVolumeDisneyLeft, "mixer", "VolumeDisneyLeft", 100);
    AddIntegerRec(NR_MixerVolumeDisneyRight, "mixer", "VolumeDisneyRight", 100);
    AddIntegerRec(NR_MixerVolumeSpeakerLeft, "mixer", "VolumeSpeakerLeft", 100);
    AddIntegerRec(NR_MixerVolumeSpeakerRight, "mixer", "VolumeSpeakerRight", 100);
    AddIntegerRec(NR_MixerVolumeGUSLeft, "mixer", "VolumeGUSLeft", 100);
    AddIntegerRec(NR_MixerVolumeGUSRight, "mixer", "VolumeGUSRight", 100);
    AddIntegerRec(NR_MixerVolumeSBLeft, "mixer", "VolumeSBLeft", 100);
    AddIntegerRec(NR_MixerVolumeSBRight, "mixer", "VolumeSBRight", 100);
    AddIntegerRec(NR_MixerVolumeFMLeft, "mixer", "VolumeFMLeft", 100);
    AddIntegerRec(NR_MixerVolumeFMRight, "mixer", "VolumeFMRight", 100);
    AddIntegerRec(NR_MixerVolumeCDLeft, "mixer", "VolumeCDLeft", 100);
    AddIntegerRec(NR_MixerVolumeCDRight, "mixer", "VolumeCDRight", 100);
    AddStringRec(NR_SBType, "sblaster", "sbtype", "sb16");
    AddStringRec(NR_SBBase, "sblaster", "sbbase", "220");
    AddIntegerRec(NR_SBIRQ, "sblaster", "irq", 7);
    AddIntegerRec(NR_SBDMA, "sblaster", "dma", 1);
    AddIntegerRec(NR_SBHDMA, "sblaster", "hdma", 5);
    AddBooleanRec(NR_SBMixer, "sblaster", "mixer", true);
    AddStringRec(NR_SBOplMode, "sblaster", "oplmode", "auto");
    AddIntegerRec(NR_SBOplRate, "sblaster", "oplrate", 44100);
    AddStringRec(NR_SBOplEmu, "sblaster", "oplemu", "default");
    AddBooleanRec(NR_GUS, "gus", "gus", false);
    AddIntegerRec(NR_GUSRate, "gus", "gusrate", 44100);
    AddStringRec(NR_GUSBase, "gus", "gusbase", "240");
    AddIntegerRec(NR_GUSIRQ, "gus", "irq1", 5);
    AddIntegerRec(NR_GUSDMA, "gus", "dma1", 1);
    AddStringRec(NR_GUSUltraDir, "gus", "ultradir", "C:\\ULTRASND");
    AddStringRec(NR_MIDIType, "midi", "mpu401", "intelligent");
    AddStringRec(NR_MIDIDevice, "midi", "device", "default");
    AddStringRec(NR_MIDIConfig, "midi", "config", "");
    AddStringRec(NR_MIDIMT32Mode, "midi", "MT32Mode", "auto");
    AddStringRec(NR_MIDIMT32Time, "midi", "MT32Time", "5");
    AddStringRec(NR_MIDIMT32Level, "midi", "MT32Level", "3");
    AddBooleanRec(NR_SpeakerPC, "speaker", "pcspeaker", true);
    AddIntegerRec(NR_SpeakerRate, "speaker", "pcrate", 44100);
    AddStringRec(NR_SpeakerTandy, "speaker", "tandy", "auto");
    AddIntegerRec(NR_SpeakerTandyRate, "speaker", "tandyrate", 44100);
    AddBooleanRec(NR_SpeakerDisney, "speaker", "disney", true);
    AddStringRec(NR_JoystickType, "joystick", "joysticktype", "none");
    AddBooleanRec(NR_JoystickTimed, "joystick", "timed", true);
    AddBooleanRec(NR_JoystickAutoFire, "joystick", "autofire", false);
    AddBooleanRec(NR_JoystickSwap34, "joystick", "swap34", false);
    AddBooleanRec(NR_JoystickButtonwrap, "joystick", "buttonwrap", false);

    AddBooleanRec(NR_NE2000, "ne2000", "ne2000", false);
    AddStringRec(NR_NE2000Base, "ne2000", "ne2000base", "300");
    AddIntegerRec(NR_NE2000IRQ, "ne2000", "ne2000irq", 3);
    AddStringRec(NR_NE2000MACAddress, "ne2000", "macaddress", "AC:DE:48:88:99:AA");
    AddStringRec(NR_NE2000RealInterface, "ne2000", "realinterface", "");

    AddBooleanRec(NR_Innova, "Innova", "Innova", false);
    AddIntegerRec(NR_InnovaRate, "Innova", "rate", 22050);
    AddStringRec(NR_InnovaBase, "Innova", "base", "280");
    AddIntegerRec(NR_InnovaQuality, "Innova", "quality", 0);

    AddStringRec(NR_Autoexec, "Extra", "autoexec", "");
    AddBooleanRec(NR_AutoexecOverridegamestart, "Extra", "Overridegamestart", false);
    AddBooleanRec(NR_AutoexecOverrideMount, "Extra", "OverrideMount", false);
    AddStringRec(NR_AutoexecBootImage, "Extra", "BootImage", "");
    AddStringRec(NR_AutoexecFinalization, "Extra", "Finalization", "");

    AddStringRec(NR_CustomSettings, "Extra", "CustomSettings", "");
    AddStringRec(NR_Environment, "Extra", "Environment", "PATH[61]Z:\\");

    AddIntegerRec(NR_LastOpenTab, "Extra", "Tab", 0);
    AddIntegerRec(NR_LastOpenTabModern, "Extra", "Tab2", -1);

    AddStringRec(NR_ProfileMode, "Extra", "ProfileMode", "DOSBox");

    AddStringRec(NR_ScummVMGame, "ScummVM", "GameName", "");
    AddStringRec(NR_ScummVMPath, "ScummVM", "GamePath", "");
    AddStringRec(NR_ScummVMZip, "ScummVM", "GameZipFile", "");
    AddStringRec(NR_ScummVMFilter, "ScummVM", "Filter", "2x");
    AddStringRec(NR_ScummVMRenderMode, "ScummVM", "RenderMode", "");
    AddIntegerRec(NR_ScummVMAutosave, "ScummVM", "AutosavePeriod", 300);
    AddStringRec(NR_ScummVMLanguage, "ScummVM", "Language", "");
    AddIntegerRec(NR_ScummVMMusicVolume, "ScummVM", "MusicVolume", 192);
    AddIntegerRec(NR_ScummVMSpeechVolume, "ScummVM", "SpeechVolume", 192);
    AddIntegerRec(NR_ScummVMSFXVolume, "ScummVM", "SFXVolume", 192);
    AddIntegerRec(NR_ScummVMMIDIGain, "ScummVM", "MIDIGain", 100);
    AddIntegerRec(NR_ScummVMSampleRate, "ScummVM", "SampleRate", 22050);
    AddStringRec(NR_ScummVMMusicDriver, "ScummVM", "MusicDriver", "auto");
    AddBooleanRec(NR_ScummVMNativeMT32, "ScummVM", "NativeMT32", false);
    AddBooleanRec(NR_ScummVMEnableGS, "ScummVM", "EnableGS", false);
    AddBooleanRec(NR_ScummVMMultiMIDI, "ScummVM", "MultiMIDI", false);
    AddIntegerRec(NR_ScummVMTalkSpeed, "ScummVM", "TalkSpeed", 60);
    AddBooleanRec(NR_ScummVMSpeechMute, "ScummVM", "SpeechMute", false);
    AddBooleanRec(NR_ScummVMSubtitles, "ScummVM", "Subtitles", true);
    AddStringRec(NR_ScummVMSavePath, "ScummVM", "Savepath", "");
    AddBooleanRec(NR_ScummVMConfirmExit, "ScummVM", "ConfirmExit", false);
    AddIntegerRec(NR_ScummVMCDROM, "ScummVM", "CDROM", 0);
    AddIntegerRec(NR_ScummVMJoystickNum, "ScummVM", "JoystickNum", 0);
    AddBooleanRec(NR_ScummVMAltIntro, "ScummVM", "AltIntro", false);
    AddIntegerRec(NR_ScummVMGFXDetails, "ScummVM", "GFXDetails", 3);
    AddBooleanRec(NR_ScummVMMusicMute, "ScummVM", "MusicMute", false);
    AddBooleanRec(NR_ScummVMObjectLabels, "ScummVM", "ObjectLabels", false);
    AddBooleanRec(NR_ScummVMReverseStereo, "ScummVM", "ReverseStereo", false);
    AddBooleanRec(NR_ScummVMSFXMute, "ScummVM", "SFXMute", false);
    AddIntegerRec(NR_ScummVMWalkspeed, "ScummVM", "Walkspeed", 2);
    AddStringRec(NR_ScummVMExtraPath, "ScummVM", "ExtraPath", "");
    AddStringRec(NR_ScummVMPlatform, "ScummVM", "Platform", "");
    AddStringRec(NR_ScummVMParameters, "ScummVM", "AdditionalParameters", "");

    AddStringRec(NR_CommandBeforeExecution, "ExtraCommands", "BeforeExecution", "");
    AddStringRec(NR_CommandAfterExecution, "ExtraCommands", "AfterExecution", "");
    AddBooleanRec(NR_CommandBeforeExecutionWait, "ExtraCommands", "BeforeExecution.Wait", true);
    AddBooleanRec(NR_CommandBeforeExecutionMinimized, "ExtraCommands", "BeforeExecution.Minimized", false);
    AddBooleanRec(NR_CommandAfterExecutionMinimized, "ExtraCommands", "AfterExecution.Minimized", false);
    AddBooleanRec(NR_CommandBeforeExecutionParallel, "ExtraCommands", "BeforeExecution.Parallel", false);
    AddBooleanRec(NR_CommandAfterExecutionParallel, "ExtraCommands", "AfterExecution.Parallel", false);
    AddBooleanRec(NR_RunAsAdmin, "Extra", "RunAsAdmin", false);

    AddStringRec(NR_AddtionalChecksumFile1, "ExtraGameIdentify", "File1.Filename", "");
    AddStringRec(NR_AddtionalChecksumFile1Checksum, "ExtraGameIdentify", "File1.Checksum", "");
    AddStringRec(NR_AddtionalChecksumFile2, "ExtraGameIdentify", "File2.Filename", "");
    AddStringRec(NR_AddtionalChecksumFile2Checksum, "ExtraGameIdentify", "File2.Checksum", "");
    AddStringRec(NR_AddtionalChecksumFile3, "ExtraGameIdentify", "File3.Filename", "");
    AddStringRec(NR_AddtionalChecksumFile3Checksum, "ExtraGameIdentify", "File3.Checksum", "");
    AddStringRec(NR_AddtionalChecksumFile4, "ExtraGameIdentify", "File4.Filename", "");
    AddStringRec(NR_AddtionalChecksumFile4Checksum, "ExtraGameIdentify", "File4.Checksum", "");
    AddStringRec(NR_AddtionalChecksumFile5, "ExtraGameIdentify", "File5.Filename", "");
    AddStringRec(NR_AddtionalChecksumFile5Checksum, "ExtraGameIdentify", "File5.Checksum", "");

    for (int I = 0; I < StagingSettingCount; I++) AddStringRec(NR_StagingSettingFirst + I, StagingSettings[I].Section, StagingSettings[I].Key, "default");

    FastAddRecDone();
}

QString TGame::GetExtraPrgFile(int I)
{
    if ((I < 0) || (I > 49)) return "";
    return GetString(NR_ExtraPrgFile + I);
}

QString TGame::GetExtraPrgFileParameter(int I)
{
    if ((I < 0) || (I > 49)) return "";
    return GetString(NR_ExtraPrgFileParameter + I);
}

QString TGame::GetLicense()
{
    QString result = "";
    TStringList *St = StringToStringList(UserInfo());
    for (int I = 0; I < St->Count(); I++) if (ExtUpperCase(Copy(Trim((*St)[I]), 1, 8)) == "LICENSE=") { result = Copy(Trim((*St)[I]), 9, MaxInt); break; }
    delete St;
    return result;
}

void TGame::SetExtraPrgFile(int I, const QString& S)
{
    if ((I < 0) || (I > 49)) return;
    SetString(NR_ExtraPrgFile + I, S);
}

void TGame::SetExtraPrgFileParameter(int I, const QString& S)
{
    if ((I < 0) || (I > 49)) return;
    SetString(NR_ExtraPrgFileParameter + I, S);
}

QString TGame::GetMount(int I)
{
    if ((I < 0) || (I > 9)) return ""; else return GetString(NR_Mount0 + I);
}

void TGame::SetMount(int I, const QString& Value)
{
    switch (I) {
        case 0: SetMount0(Value); break;
        case 1: SetMount1(Value); break;
        case 2: SetMount2(Value); break;
        case 3: SetMount3(Value); break;
        case 4: SetMount4(Value); break;
        case 5: SetMount5(Value); break;
        case 6: SetMount6(Value); break;
        case 7: SetMount7(Value); break;
        case 8: SetMount8(Value); break;
        case 9: SetMount9(Value); break;
    }
}

QString TGame::GetWWWNameString(int I)
{
    QString S, result;
    if ((I < 1) || (I > 9)) return "";
    result = GetString(NR_WWW1Name + (I - 1) * 2);
    if (Trim(result) == "") {
        result = GetString(NR_WWW1 + (I - 1) * 2);
        S = Trim(ExtUpperCase(result));
        if (Copy(S, 1, 7) == "HTTP:/" "/") result = Copy(Trim(result), 8, MaxInt);
        if (Copy(S, 1, 8) == "HTTPS:/" "/") result = Copy(Trim(result), 9, MaxInt);
    }
    return result;
}

QString TGame::GetPlainWWWNameString(int I)
{
    if ((I < 1) || (I > 9)) return "";
    return GetString(NR_WWW1Name + (I - 1) * 2);
}

void TGame::SetWWWNameString(int I, const QString& Value)
{
    if ((I < 1) || (I > 9)) return;
    SetString(NR_WWW1Name + (I - 1) * 2, Value);
}

QString TGame::GetWWWString(int I)
{
    if ((I < 1) || (I > 9)) return "";
    return GetString(NR_WWW1 + (I - 1) * 2);
}

void TGame::SetWWWString(int I, const QString& Value)
{
    if ((I < 1) || (I > 9)) return;
    SetString(NR_WWW1 + (I - 1) * 2, Value);
}

void TGame::LoadCache()
{
    CacheName = Name();
    CacheNameUpper = ExtUpperCase(CacheName);
    CacheGenre = Genre();
    CacheGenreUpper = ExtUpperCase(CacheGenre);
    CacheDeveloper = Developer();
    CacheDeveloperUpper = ExtUpperCase(CacheDeveloper);
    CachePublisher = Publisher();
    CachePublisherUpper = ExtUpperCase(CachePublisher);
    CacheYear = Year();
    CacheYearUpper = ExtUpperCase(CacheYear);
    CacheLanguage = Language();
    CacheLanguageUpper = ExtUpperCase(CacheLanguage);
    CacheUserInfo = UserInfo();
}

void TGame::ReloadINI()
{
    TBasePrgSetup::ReloadINI();
    LoadCache();
}

void TGame::CreateConfFile()
{
    if (!DOSBoxMode(this)) return;

    TStringList *St = BuildConfFile(this, false, false, -1, nullptr, false);
    if (St == nullptr) return;
    St->SaveToFile(ChangeFileExt(SetupFile(), ".conf"));
    delete St;
}

void TGame::RenameINI(const QString& NewFile)
{
    QString OldName, NewName;
    OldName = ChangeFileExt(SetupFile(), ".conf");
    TBasePrgSetup::RenameINI(NewFile);
    NewName = ChangeFileExt(SetupFile(), ".conf");
    if ((FGameDB != nullptr) && FGameDB->CreateConfFilesOnSave()) {
        if (FileExists(OldName)) RenameFile(OldName, NewName); else CreateConfFile();
    }
}

void TGame::UpdateFile()
{
    TBasePrgSetup::UpdateFile();
    const TDateTime N = Now();
    SetLastModification(IntToStr(Round(std::trunc(N))) + "-" + IntToStr(Round((N - std::trunc(N)) * 86400)));
    if ((FGameDB != nullptr) && FGameDB->CreateConfFilesOnSave()) CreateConfFile();
}

void TGame::AssignFromButKeepScummVMSettings(TGame* AGame)
{
    AssignFromPartially(AGame, ScummVMSettings);
}

/* TGameDB */

TGameDB::TGameDB(const QString& ADir, bool ATimeStampCheck)
{
    bool B;

    LogInfo("### Start of TGameDB.Create (" + ADir + ") ###");

    FCreateConfFilesOnSave = false;
    FGameList = new TList;
    for (int C = 0; C < 26; C++) { BinLoadCacheIndex[C] = nullptr; BinLoadCacheIndexLastIndex[C] = 0; }

    LogInfo("Create conf opts object");
    FConfOpt = new TConfOpt;
    if (ATimeStampCheck) FConfOpt->CacheAllStrings();
    FDir = IncludeTrailingPathDelimiter(ADir);
    FTimeStampCheck = ATimeStampCheck;
    B = false;
    QWidget *MainForm = MainFormWidget();
    if (MainForm != nullptr) {
        B = MainForm->isEnabled();
        MainForm->setEnabled(false);
    }
    try {
        LogInfo("Start loading profiles");
        LoadList();
    } catch (...) {
        if (MainForm != nullptr) MainForm->setEnabled(B);
        throw;
    }
    if (MainForm != nullptr) {
        // While PeekMessage(Msg,Application.MainForm.Handle,WM_INPUT,WM_INPUT,1) do ;
        // (input events are not delivered to disabled widgets in Qt)
        MainForm->setEnabled(B);
    }

    LogInfo("Deleting old files");
    DeleteOldFiles();

    LogInfo("### End of TGameDB.Create ###");
}

TGameDB::~TGameDB()
{
    if (PrgSetup->BinaryCache()) {
        if (FGameList->size() > 20) StoreBinCache(); else DeleteFile(FDir + CacheFile);
    }
    Clear();
    delete FGameList;
    delete FConfOpt;
}

void TGameDB::Clear()
{
    for (int I = 0; I < FGameList->size(); I++) delete static_cast<TGame *>((*FGameList)[I]);
    FGameList->clear();
}

void TGameDB::LoadGameFromFile(const QString& FileName, int DOSFileDate)
{
    TGame *Game;

    LogInfo("Loading profile " + FileName);
    if (PrgSetup->BinaryCache() && GetLoadBinCacheOffset(FileName, DOSFileDate)) {
        LogInfo("Trying to load from cache");
        Game = TGame::CreateDelayed(FileName, FTimeStampCheck);
        try {
            Game->InitData();
            Game->LoadFromStream(BinLoadCache);
            Game->LoadCache();
        } catch (...) {
            LogInfo("Cache failed, burning cache for all profiles and loading from prof file");
            delete Game;
            Game = new TGame(FileName);
            BurnLoadBinCache();
        }
        if (Game->GameExeMD5() == "") {
            LogInfo("No game exe MD5 sum. May be there is just no MD5 sum stored but may be also this cached profile is damaged. Loading this profile from prof file for safety reasons.");
            delete Game;
            Game = new TGame(FileName);
        }
    } else {
        LogInfo("Loading profile from prof file");
        Game = new TGame(FileName);
    }

    LogInfo("Adding profile to data base list");
    Game->SetOnChanged([this](QObject *Sender) { GameChanged(Sender); });
    Game->SetGameDB(this);
    FGameList->append(Game);
}

TStringList* TGameDB::GetProfilesListFromDrive()
{
    TSearchRec Rec;
    int I;
    TStringList *result = new TStringList;

    I = FindFirst(FDir + "*.prof", faAnyFile, Rec);
    while (I == 0) {
        if ((Rec.Attr & faDirectory) == 0) result->AddObject(Rec.Name, (intptr_t)Rec.Time);
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

void TGameDB::LoadList()
{
    int I;
    TStringList *List;
    quint32 C1, C2;

    Clear();
    ForceDirectories(FDir);

    if (PrgSetup->BinaryCache()) {
        LogInfo("Init binary cache");
        InitLoadBinCache();
    }

    LogInfo("Loading profiles file list from drive");
    List = GetProfilesListFromDrive();
    WaitForm = nullptr;

    try {
        FGameList->reserve(List->Count());
        C1 = GetTickCount();

        for (I = 0; I < Min(List->Count(), 100); I++) {
            LoadGameFromFile(FDir + (*List)[I], List->ObjectAsInt(I));
            if ((I % 10) == 0) ProcessMessages();
        }

        if (List->Count() > 100) {
            C2 = GetTickCount();
            if (int(C2 - C1) * List->Count() / 100 > 150) WaitForm = TWaitForm::CreateWaitForm(nullptr, LanguageSetup->MessageLoadingDataBase(), List->Count());
            for (I = 100; I < List->Count(); I++) {
                if ((WaitForm != nullptr) && ((I % 50) == 0)) WaitForm->Step(I);
                LoadGameFromFile(FDir + (*List)[I], List->ObjectAsInt(I));
                if ((I % 10) == 0) ProcessMessages();
            }
        }
    } catch (...) {
        delete List;
        if (WaitForm != nullptr) { delete WaitForm; WaitForm = nullptr; }
        throw;
    }
    delete List;
    if (WaitForm != nullptr) { delete WaitForm; WaitForm = nullptr; }

    if (PrgSetup->BinaryCache()) {
        LogInfo("Freeing binary cache");
        DoneLoadBinCache();
    }

    QWidget *MainForm = MainFormWidget();
    if (MainForm != nullptr) ForceForegroundWindow(MainForm->winId());
}

void TGameDB::DeleteOldFiles()
{
    TSearchRec Rec;
    int I;

    if (!PrgSetup->CreateConfFilesForProfiles() || (OperationMode == omPortable)) {
        I = FindFirst(FDir + "*.conf", faAnyFile, Rec);
        while (I == 0) {
            ExtDeleteFile(FDir + Rec.Name, ftProfile);
            I = FindNext(Rec);
        }
        FindClose(Rec);
    }

    I = FindFirst(FDir + "Tempprof.*", faAnyFile, Rec);
    while (I == 0) {
        ExtDeleteFile(FDir + Rec.Name, ftProfile);
        I = FindNext(Rec);
    }
    FindClose(Rec);
}

QString TGameDB::MakePROFFileName(const QString& AName, const QString& ADir, bool CheckExistingFiles)
{
    static const QString AllowedChars = QString::fromUtf8("ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜabcdefghijklmnopqrstuvwxyzäöüß01234567890-_=.,;!() ");
    int I;
    QString result = "";
    for (I = 1; I <= Length(AName); I++) if (Pos(AName[I - 1], AllowedChars) > 0) result = result + AName[I - 1];
    if (result == "") result = "Game";

    if (FileExists(ADir + result + ".prof") && CheckExistingFiles) {
        I = 0;
        do { I++; } while (FileExists(ADir + result + IntToStr(I) + ".prof"));
        result = ADir + result + IntToStr(I) + ".prof";
    } else {
        result = ADir + result + ".prof";
    }
    return result;
}

QString TGameDB::ProfFileName(const QString& AName, bool CheckExistingFiles)
{
    QString S = Trim(ExtUpperCase(AName));
    if ((Copy(S, Length(S) - 4, 5) == ".PROF") || (Copy(S, Length(S) - 4, 5) == ".CONF")) {
        S = Trim(AName); S = Trim(Copy(S, 1, Length(S) - 5));
    } else {
        S = AName;
    }
    return MakePROFFileName(Trim(S), FDir, CheckExistingFiles);
}

int TGameDB::Add(const QString& AName)
{
    TGame *Game = new TGame(MakePROFFileName(AName, FDir, true));
    Game->SetName(AName);
    Game->SetOnChanged([this](QObject *Sender) { GameChanged(Sender); });
    Game->SetGameDB(this);
    FGameList->append(Game);
    return FGameList->size() - 1;
}

int TGameDB::Add(TGame* AGame)
{
    AGame->SetOnChanged([this](QObject *Sender) { GameChanged(Sender); });
    FGameList->append(AGame);
    int result = FGameList->size() - 1;
    AGame->SetGameDB(this);
    return result;
}

bool TGameDB::Delete(int Index)
{
    QString FileName;
    bool result = (Index >= 0) && (Index < FGameList->size());
    if (!result) return result;
    FileName = static_cast<TGame *>((*FGameList)[Index])->SetupFile();
    delete static_cast<TGame *>((*FGameList)[Index]);
    FGameList->removeAt(Index);

    if (FileExists(FileName)) {
        if (!ExtDeleteFile(FileName, ftProfile)) MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {FileName}), mtError, mbOK, 0);
    }
    if (FileExists(ChangeFileExt(FileName, ".conf"))) {
        if (!ExtDeleteFile(ChangeFileExt(FileName, ".conf"), ftProfile)) MessageDlg(Format(LanguageSetup->MessageCouldNotDeleteFile(), {ChangeFileExt(FileName, ".conf")}), mtError, mbOK, 0);
    }
    return result;
}

bool TGameDB::Delete(TGame* AGame)
{
    return Delete(IndexOf(AGame));
}

int TGameDB::IndexOf(TGame* AGame)
{
    for (int I = 0; I < FGameList->size(); I++) if (static_cast<TGame *>((*FGameList)[I]) == AGame) return I;
    return -1;
}

int TGameDB::IndexOf(const QString& AGame)
{
    for (int I = 0; I < FGameList->size(); I++) if (static_cast<TGame *>((*FGameList)[I])->Name() == AGame) return I;
    return -1;
}

int TGameDB::GetCount()
{
    return FGameList->size();
}

TGame* TGameDB::GetGame(int I)
{
    if ((I < 0) || (I >= FGameList->size())) return nullptr; else return static_cast<TGame *>((*FGameList)[I]);
}

TList* TGameDB::GetSortedGamesList()
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) St.AddObject(static_cast<TGame *>((*FGameList)[I])->Name(), (*FGameList)[I]);
    St.Sort();

    TList *result = new TList;
    for (int I = 0; I < St.Count(); I++) result->append(St.Objects(I));
    return result;
}

static TStringList *GetList(TStringList *St)
{
    int I, J;
    QString S, T;

    TStringList *result = new TStringList;
    TStringList StUpper;
    for (I = 0; I < St->Count(); I++) {
        S = Trim((*St)[I]);
        if (S == "") S = LanguageSetup->NotSet();
        while (S != "") {
            J = Pos(";", S);
            if (J > 0) {
                T = Trim(Copy(S, 1, J - 1));
                S = Trim(Copy(S, J + 1, MaxInt));
            } else {
                T = S; S = "";
            }
            if (StUpper.IndexOf(ExtUpperCase(T)) < 0) {
                result->Add(T);
                StUpper.Add(ExtUpperCase(T));
            }
        }
    }
    result->Sort();
    return result;
}

TStringList* TGameDB::GetGenreList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->CacheGenre);
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetDeveloperList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->CacheDeveloper);
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetPublisherList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->CachePublisher);
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetYearList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->CacheYear);
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetLanguageList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->CacheLanguage);
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetLicenseList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            St.Add(G->License());
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetWWWList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            for (int J = 1; J <= 9; J++) {
                QString S = G->WWW(J);
                if ((J == 1) || (Trim(S) != "")) St.Add(S);
            }
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetWWWNameList(bool WithDefaultProfile, bool HideWindowsProfiles)
{
    TStringList St;
    for (int I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (WithDefaultProfile || (G->Name() != DosBoxDOSProfile)) {
            if (HideWindowsProfiles && WindowsExeMode(G)) continue;
            for (int J = 1; J <= 9; J++) {
                QString S = G->WWWNamePlain(J);
                if (Trim(S) != "") St.Add(S);
            }
        }
    }
    return GetList(&St);
}

TStringList* TGameDB::GetKeyValueList(const QString& Key, bool WithDefaultProfile)
{
    int I, J, K;
    QString S, T, KeyUpper, Val;

    TStringList *result = new TStringList;
    TStringList StUpper;
    KeyUpper = ExtUpperCase(Key);
    for (I = 0; I < FGameList->size(); I++) {
        TGame *G = static_cast<TGame *>((*FGameList)[I]);
        if (!(WithDefaultProfile || (G->Name() != DosBoxDOSProfile))) continue;

        TStringList *St2 = StringToStringList(G->CacheUserInfo);
        Val = "";
        for (K = 0; K < St2->Count(); K++) {
            S = (*St2)[K]; J = Pos("=", S); if (J == 0) T = ""; else { T = Trim(Copy(S, J + 1, MaxInt)); S = Trim(Copy(S, 1, J - 1)); }
            if (Trim(ExtUpperCase(S)) == KeyUpper) { Val = Trim(T); break; }
        }
        if (Val == "") Val = LanguageSetup->NotSet();
        S = Val;
        while (S != "") {
            J = Pos(";", S);
            if (J > 0) {
                T = Trim(Copy(S, 1, J - 1));
                S = Trim(Copy(S, J + 1, MaxInt));
            } else {
                T = S; S = "";
            }
            if (StUpper.IndexOf(ExtUpperCase(T)) < 0) {
                result->Add(T);
                StUpper.Add(ExtUpperCase(T));
            }
        }
        delete St2;
    }
    result->Sort();
    return result;
}

TStringList* TGameDB::GetUserKeys()
{
    int I, J, K;
    QString S;

    TStringList *result = new TStringList;
    TStringList StUpper;
    for (I = 0; I < FGameList->size(); I++) {
        TStringList *St = StringToStringList(static_cast<TGame *>((*FGameList)[I])->CacheUserInfo);
        for (K = 0; K < St->Count(); K++) {
            S = (*St)[K]; J = Pos("=", S); if (J == 0) continue;
            S = Trim(Copy(S, 1, J - 1));
            if (StUpper.IndexOf(ExtUpperCase(S)) >= 0) continue;
            result->Add(S);
            StUpper.Add(ExtUpperCase(S));
        }
        delete St;
    }
    result->Sort();
    return result;
}

void TGameDB::GameChanged(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (FOnChanged) FOnChanged(nullptr);   // FOnChanged(self) - TGameDB is no QObject
}

void TGameDB::StoreAllValues()
{
    for (int I = 0; I < FGameList->size(); I++) static_cast<TGame *>((*FGameList)[I])->StoreAllValues();
}

void TGameDB::LoadCache()
{
    for (int I = 0; I < FGameList->size(); I++) static_cast<TGame *>((*FGameList)[I])->LoadCache();
}

// ---------------------------------------------------------------------------
// Binary cache
//
// Same layout as the Delphi version (ID string, info text, version id, index
// 'A'..'Z' + rest, data), but integers and index strings are written with
// QDataStream and the profile data by TBasePrgSetup::SaveToStream (QDataStream),
// so the cache files of the original program are not compatible (they are
// rejected by the ID check and rebuilt).
// ---------------------------------------------------------------------------

static const QByteArray CacheIDBytes()
{
    return (CacheVersionString + QStringLiteral("-Qt")).toLatin1();
}

void TGameDB::StoreBinCache()
{
    QBuffer St;
    St.open(QIODevice::ReadWrite);
    TStringList *Index[26];
    TStringList IndexRest;
    int I, J;
    QString S;
    QChar C;

    for (int K = 0; K < 26; K++) Index[K] = new TStringList;

    /* Store game data to temporary stream */
    {
        QDataStream DS(&St);
        DS.setVersion(QDataStream::Qt_6_0);
        for (I = 0; I < FGameList->size(); I++) {
            TGame *G = static_cast<TGame *>((*FGameList)[I]);
            S = ExtractFileName(G->SetupFile());
            S = ExtUpperCase(S);
            if (S != "") C = S[0]; else C = ' ';
            J = int(St.pos());
            if ((C >= 'A') && (C <= 'Z'))
                Index[C.unicode() - 'A']->AddObject(S, (intptr_t)J); else IndexRest.AddObject(S, (intptr_t)J);
            J = GetDOSFileDate(G->SetupFile());
            DS << qint32(J);
            G->SaveToStream(&St);
        }
    }

    /* Store to output file */
    QBuffer StFinal;
    StFinal.open(QIODevice::WriteOnly);
    {
        QDataStream DS(&StFinal);
        DS.setVersion(QDataStream::Qt_6_0);

        /* Write ID */
        const QByteArray ID = CacheIDBytes();
        StFinal.write(ID);
        const QByteArray Info = CacheInfoString.toLatin1();
        StFinal.write(Info);
        if (FGameList->size() > 0) I = static_cast<TGame *>((*FGameList)[0])->GetBinaryVersionID(); else I = 0;
        DS << qint32(I);

        /* Write index */
        for (int K = 0; K < 26; K++) {
            DS << qint32(Index[K]->Count());
            for (I = 0; I < Index[K]->Count(); I++) {
                S = ChangeFileExt((*Index[K])[I], "");
                DS << S;
                DS << qint32(Index[K]->ObjectAsInt(I));
            }
        }
        DS << qint32(IndexRest.Count());
        for (I = 0; I < IndexRest.Count(); I++) {
            S = ChangeFileExt(IndexRest[I], "");
            DS << S;
            DS << qint32(IndexRest.ObjectAsInt(I));
        }

        /* Write data */
        StFinal.write(St.data());
    }
    StFinal.close();

    QFile F(ToNativePath(FDir + CacheFile));
    if (F.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        F.write(StFinal.data());
        F.close();
    }

    for (int K = 0; K < 26; K++) delete Index[K];
}

void TGameDB::InitLoadBinCache()
{
    qint32 I, K;
    QString S;

    BinLoadCache = new QBuffer;
    for (int C = 0; C < 26; C++) { BinLoadCacheIndex[C] = new TStringList; BinLoadCacheIndexLastIndex[C] = 0; }
    BinLoadCacheIndexRest = new TStringList;
    BinLoadCacheIndexRestLastIndex = 0;
    BinLoadCacheOffset = 0;
    BinLoadCache->open(QIODevice::ReadOnly);

    if (!FileExists(FDir + CacheFile)) return;
    {
        QFile F(ToNativePath(FDir + CacheFile));
        if (!F.open(QIODevice::ReadOnly)) return;
        BinLoadCache->close();
        BinLoadCache->setData(F.readAll());
        BinLoadCache->open(QIODevice::ReadOnly);
    }

    QDataStream DS(BinLoadCache);
    DS.setVersion(QDataStream::Qt_6_0);

    {
        const QByteArray ID = CacheIDBytes();
        if (BinLoadCache->read(ID.size()) != ID) return;

        const int InfoLength = CacheInfoString.toLatin1().size();
        if (BinLoadCache->size() < BinLoadCache->pos() + InfoLength) return;
        BinLoadCache->seek(BinLoadCache->pos() + InfoLength);

        DS >> I;
        if (DS.status() != QDataStream::Ok) return;
        if (!CheckGameBinaryVersionID(I)) return;
    }

    bool Failed = false;
    for (int C = 0; C < 26 && !Failed; C++) {
        DS >> I;
        if (DS.status() != QDataStream::Ok) { Failed = true; break; }
        for (int J = 0; J < I; J++) {
            DS >> S;
            DS >> K;
            if (DS.status() != QDataStream::Ok) { Failed = true; break; }
            BinLoadCacheIndex[C]->AddObject(S, (intptr_t)K);
        }
    }
    if (!Failed) {
        DS >> I;
        if (DS.status() != QDataStream::Ok) Failed = true;
        for (int J = 0; J < I && !Failed; J++) {
            DS >> S;
            DS >> K;
            if (DS.status() != QDataStream::Ok) { Failed = true; break; }
            BinLoadCacheIndexRest->AddObject(S, (intptr_t)K);
        }
    }
    if (Failed) BurnLoadBinCache();

    BinLoadCacheOffset = int(BinLoadCache->pos());
}

bool TGameDB::CheckGameBinaryVersionID(int CacheVersionID)
{
    TGame *G = new TGame(PrgSetup);
    bool result = (G->GetBinaryVersionID() == CacheVersionID);
    delete G;
    return result;
}

void TGameDB::DoneLoadBinCache()
{
    delete BinLoadCache; BinLoadCache = nullptr;
    for (int C = 0; C < 26; C++) { delete BinLoadCacheIndex[C]; BinLoadCacheIndex[C] = nullptr; }
    delete BinLoadCacheIndexRest; BinLoadCacheIndexRest = nullptr;
}

void TGameDB::BurnLoadBinCache()
{
    if (FileExists(FDir + CacheFile)) DeleteFile(FDir + CacheFile);
    for (int C = 0; C < 26; C++) BinLoadCacheIndex[C]->Clear();
    BinLoadCacheIndexRest->Clear();
}

bool TGameDB::GetLoadBinCacheOffset(const QString& FileName, int DOSFileDate)
{
    QString S;
    QChar C;
    int I, J;

    if (BinLoadCache == nullptr) return false;

    S = ExtUpperCase(ChangeFileExt(ExtractFileName(FileName), ""));
    if (S != "") C = S[0]; else C = ' ';

    if ((C >= 'A') && (C <= 'Z')) {
        const int CI = C.unicode() - 'A';
        I = -1;
        if ((BinLoadCacheIndexLastIndex[CI] > 0) && (BinLoadCacheIndexLastIndex[CI] < BinLoadCacheIndex[CI]->Count() - 1)) {
            if ((*BinLoadCacheIndex[CI])[BinLoadCacheIndexLastIndex[CI] + 1] == S) I = BinLoadCacheIndexLastIndex[CI] + 1;
        }
        if (I < 0) I = BinLoadCacheIndex[CI]->IndexOf(S);
        if (I < 0) return false;
        BinLoadCacheIndexLastIndex[CI] = I;
        J = BinLoadCacheIndex[CI]->ObjectAsInt(I);
    } else {
        I = -1;
        if ((BinLoadCacheIndexRestLastIndex > 0) && (BinLoadCacheIndexRestLastIndex < BinLoadCacheIndexRest->Count() - 1)) {
            if ((*BinLoadCacheIndexRest)[BinLoadCacheIndexRestLastIndex + 1] == S) I = BinLoadCacheIndexRestLastIndex + 1;
        }
        if (I < 0) I = BinLoadCacheIndexRest->IndexOf(S);
        if (I < 0) return false;
        BinLoadCacheIndexRestLastIndex = I;
        J = BinLoadCacheIndexRest->ObjectAsInt(I);
    }

    if (!BinLoadCache->seek(BinLoadCacheOffset + J)) return false;

    QDataStream DS(BinLoadCache);
    DS.setVersion(QDataStream::Qt_6_0);
    qint32 D = 0;
    DS >> D;
    if (DS.status() != QDataStream::Ok) return false;
    if (D != DOSFileDate) return false;

    return true;
}

/* initialization / finalization */

void GameDBUnit_Initialization()
{
    DefaultValueReaderGame = new TGame(QString());
}

void GameDBUnit_Finalization()
{
    if (DefaultValueReaderGame != nullptr) { delete DefaultValueReaderGame; DefaultValueReaderGame = nullptr; }
}
