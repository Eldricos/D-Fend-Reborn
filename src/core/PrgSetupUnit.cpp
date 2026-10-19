// C++ port of PrgSetupUnit.pas (implementation section).
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#include <QDir>
#include <QStandardPaths>

TOperationMode OperationMode = omPrgDir;
TPrgSetup *PrgSetup = nullptr;

/* TDOSBoxSetting */

TDOSBoxSetting::TDOSBoxSetting(TBasePrgSetup *APrgSetup, int ANr)
{
    FPrgSetup = APrgSetup;
    FNr = ANr;
    ReadSettings();
    InitDirs();
}

TDOSBoxSetting::~TDOSBoxSetting()
{
    DoneDirs();
    WriteSettings();
}

void TDOSBoxSetting::ReadSettings()
{
    QString Section;
    if (FNr == 0) Section = "ProgramSets"; else Section = "DOSBox-" + IntToStr(FNr + 1);

    TMemIniFile *Ini = FPrgSetup->MemIni();

    if (FNr == 0) FName = "Default"; else FName = Ini->ReadString(Section, "Name", "");

    FDosBoxDir = Ini->ReadString(Section, "Location", "");
    FDosBoxExecutable = Ini->ReadString(Section, "Executable", "");
    FDosBoxMapperFile = Ini->ReadString(Section, "LocationMap", "." + PathDelim + "mapper.map");
    FDosBoxLanguage = Ini->ReadString(Section, "DosBoxLanguageFile", "");
    FSDLVideodriver = Ini->ReadString(Section, "SDLVideodriver", "DirectX");
    FCommandLineParameters = Ini->ReadString(Section, "CommandLineParameters", "");
    FCustomSettings = Ini->ReadString(Section, "CustomSettings", "");

    FKeyboardLayout = Ini->ReadString(Section, "KeyboardLayout", "");
    FCodepage = Ini->ReadString(Section, "Codepage", "");

    FHideDosBoxConsole = Ini->ReadBool(Section, "Hideconsole", true);
    FCenterDOSBoxWindow = Ini->ReadBool(Section, "CenterDOSBoxWindow", false);
    FDisableScreensaver = Ini->ReadBool(Section, "DisableScreensaver", false);
    FWaitOnError = Ini->ReadBool(Section, "WaitOnError", true);
}

void TDOSBoxSetting::WriteSettings()
{
    QString Section;
    if (FNr == 0) Section = "ProgramSets"; else Section = "DOSBox-" + IntToStr(FNr + 1);

    TMemIniFile *Ini = FPrgSetup->MemIni();

    // The Delphi code writes the name via the global PrgSetup object
    if (FNr != 0) ((PrgSetup != nullptr) ? PrgSetup->MemIni() : Ini)->WriteString(Section, "Name", FName);

    Ini->WriteString(Section, "Location", FDosBoxDir);
    Ini->WriteString(Section, "Executable", FDosBoxExecutable);
    Ini->WriteString(Section, "LocationMap", FDosBoxMapperFile);
    Ini->WriteString(Section, "DosBoxLanguageFile", FDosBoxLanguage);
    Ini->WriteString(Section, "SDLVideodriver", FSDLVideodriver);
    Ini->WriteString(Section, "CommandLineParameters", FCommandLineParameters);
    Ini->WriteString(Section, "CustomSettings", FCustomSettings);

    Ini->WriteString(Section, "KeyboardLayout", FKeyboardLayout);
    Ini->WriteString(Section, "Codepage", Codepage());

    Ini->WriteBool(Section, "Hideconsole", FHideDosBoxConsole);
    Ini->WriteBool(Section, "CenterDOSBoxWindow", FCenterDOSBoxWindow);
    Ini->WriteBool(Section, "DisableScreensaver", FDisableScreensaver);
    Ini->WriteBool(Section, "WaitOnError", FWaitOnError);

    FPrgSetup->UpdateFile();
}

void TDOSBoxSetting::InitDirs()
{
    if (OperationMode == omPortable) {
        FDosBoxDir = MakeExtAbsPath(FDosBoxDir, PrgDir());
        FDosBoxLanguage = MakeExtAbsPath(FDosBoxLanguage, PrgDir());
        if ((!FileExists(FDosBoxLanguage)) && FileExists(IncludeTrailingPathDelimiter(FDosBoxDir) + ExtractFileName(FDosBoxLanguage)))
            FDosBoxLanguage = IncludeTrailingPathDelimiter(FDosBoxDir) + ExtractFileName(FDosBoxLanguage);
    }
}

void TDOSBoxSetting::DoneDirs()
{
    if (OperationMode == omPortable) {
        SetDosBoxDir(MakeExtRelPath(DosBoxDir(), PrgDir()));
        SetDosBoxLanguage(MakeExtRelPath(DosBoxLanguage(), PrgDir()));
    }
}

/* TPackerSetting */

TPackerSetting::TPackerSetting(TBasePrgSetup *APrgSetup, int ANr)
{
    FPrgSetup = APrgSetup;
    FNr = ANr;
    ReadSettings();
}

TPackerSetting::~TPackerSetting()
{
    WriteSettings();
}

void TPackerSetting::ReadSettings()
{
    QString Section = "Packer-" + IntToStr(FNr + 1);
    TMemIniFile *Ini = FPrgSetup->MemIni();

    FName = Ini->ReadString(Section, "Name", "");
    FZipFileName = Ini->ReadString(Section, "Filename", "");
    FFileExtensions = Ini->ReadString(Section, "FileExtensions", "");
    FExtractFile = Ini->ReadString(Section, "ExtractFile", "");
    FCreateFile = Ini->ReadString(Section, "CreateFile", "");
    FUpdateFile = Ini->ReadString(Section, "UpdateFile", "");
    FTrailingBackslash = Ini->ReadBool(Section, "TrailingBackslash", true);
}

void TPackerSetting::WriteSettings()
{
    QString Section = "Packer-" + IntToStr(FNr + 1);
    // The Delphi code writes via the global PrgSetup object
    TMemIniFile *Ini = (PrgSetup != nullptr) ? PrgSetup->MemIni() : FPrgSetup->MemIni();

    Ini->WriteString(Section, "Name", FName);
    Ini->WriteString(Section, "Filename", FZipFileName);
    Ini->WriteString(Section, "FileExtensions", FFileExtensions);
    Ini->WriteString(Section, "ExtractFile", FExtractFile);
    Ini->WriteString(Section, "CreateFile", FCreateFile);
    Ini->WriteString(Section, "UpdateFile", FUpdateFile);
    Ini->WriteBool(Section, "TrailingBackslash", FTrailingBackslash);

    FPrgSetup->UpdateFile();
}

/* TPrgSetup */

TPrgSetup::TPrgSetup(const QString &SetupFile)
    : TBasePrgSetup((SetupFile == "") ? PrgDataDir() + SettingsFolder + PathDelim + MainSetupFile : SetupFile)
{
    ReadSettings();
    InitDirs();
    if (ExtUpperCase(Language()) == "Deutsch.ini") SetLanguage("German.ini");

    FDOSBox = new TList();
    FPacker = new TList();
    FDOSBoxBasedUserInterpretersPrograms = new TStringList();
    FDOSBoxBasedUserInterpretersParameters = new TStringList();
    FDOSBoxBasedUserInterpretersExtensions = new TStringList();
    FWindowsBasedEmulatorsNames = new TStringList();
    FWindowsBasedEmulatorsPrograms = new TStringList();
    FWindowsBasedEmulatorsParameters = new TStringList();
    FWindowsBasedEmulatorsExtensions = new TStringList();
    LoadDOSBoxSettings();
    LoadPackerSettings();
    LoadStringListSettings();
}

TPrgSetup::~TPrgSetup()
{
    DeleteOldDOSBoxSettings();
    for (int I = 0; I < FDOSBox->size(); I++) delete static_cast<TDOSBoxSetting *>((*FDOSBox)[I]);
    delete FDOSBox; FDOSBox = nullptr;

    DeleteOldPackerSettings();
    for (int I = 0; I < FPacker->size(); I++) delete static_cast<TPackerSetting *>((*FPacker)[I]);
    delete FPacker; FPacker = nullptr;

    SaveStringListSettings();
    delete FDOSBoxBasedUserInterpretersPrograms;
    delete FDOSBoxBasedUserInterpretersParameters;
    delete FDOSBoxBasedUserInterpretersExtensions;
    delete FWindowsBasedEmulatorsNames;
    delete FWindowsBasedEmulatorsPrograms;
    delete FWindowsBasedEmulatorsParameters;
    delete FWindowsBasedEmulatorsExtensions;

    DoneDirs();
    DestroySetup();   // inherited Destroy
}

void TPrgSetup::LoadDOSBoxSettings()
{
    FDOSBox->append(new TDOSBoxSetting(this, 0));
    for (int I = 2; I < MaxInt; I++) {
        if (!MemIni()->SectionExists("DOSBox-" + IntToStr(I))) break;
        FDOSBox->append(new TDOSBoxSetting(this, I - 1));
    }
}

void TPrgSetup::DeleteOldDOSBoxSettings()
{
    int I = FDOSBox->size() + 1;
    while (MemIni()->SectionExists("DOSBox-" + IntToStr(I))) {
        MemIni()->EraseSection("DOSBox-" + IntToStr(I));
        I++;
    }
}

void TPrgSetup::LoadPackerSettings()
{
    for (int I = 1; I < MaxInt; I++) {
        if (!MemIni()->SectionExists("Packer-" + IntToStr(I))) break;
        FPacker->append(new TPackerSetting(this, I - 1));
    }
}

void TPrgSetup::DeleteOldPackerSettings()
{
    int I = FPacker->size() + 1;
    while (MemIni()->SectionExists("Packer-" + IntToStr(I))) {
        MemIni()->EraseSection("Packer-" + IntToStr(I));
        I++;
    }
}

void TPrgSetup::LoadStringListSettings()
{
    QString S, T, U, V;

    /* DOSBox based user interpreters */
    for (int I = 0; I <= 99; I++) {
        S = Trim(GetString(500 + I));
        T = Trim(GetString(600 + I));
        U = Trim(GetString(700 + I));
        if ((S != "") || (T != "") || (U != "")) {
            if ((OperationMode == omPortable) && (S != "")) S = MakeExtAbsPath(S, PrgDir());
            FDOSBoxBasedUserInterpretersPrograms->Add(S);
            FDOSBoxBasedUserInterpretersParameters->Add(T);
            FDOSBoxBasedUserInterpretersExtensions->Add(U);
        }
    }

    /* Windows based emulators */
    for (int I = 0; I <= 99; I++) {
        S = Trim(GetString(800 + I));
        T = Trim(GetString(900 + I));
        U = Trim(GetString(1000 + I));
        V = Trim(GetString(1100 + I));
        if ((S != "") || (T != "") || (U != "") || (V != "")) {
            FWindowsBasedEmulatorsNames->Add(S);
            if ((OperationMode == omPortable) && (T != "")) T = MakeExtAbsPath(T, PrgDir());
            FWindowsBasedEmulatorsPrograms->Add(T);
            FWindowsBasedEmulatorsParameters->Add(U);
            FWindowsBasedEmulatorsExtensions->Add(V);
        }
    }
}

void TPrgSetup::SaveStringListSettings()
{
    int M;
    QString S, T, U, V;

    /* DOSBox based user interpreters */
    M = Max(Max(FDOSBoxBasedUserInterpretersPrograms->Count(), FDOSBoxBasedUserInterpretersParameters->Count()), FDOSBoxBasedUserInterpretersExtensions->Count());
    for (int I = 0; I < M; I++) {
        if (I < FDOSBoxBasedUserInterpretersPrograms->Count()) S = Trim((*FDOSBoxBasedUserInterpretersPrograms)[I]); else S = "";
        if (I < FDOSBoxBasedUserInterpretersParameters->Count()) T = Trim((*FDOSBoxBasedUserInterpretersParameters)[I]); else T = "";
        if (I < FDOSBoxBasedUserInterpretersExtensions->Count()) U = Trim((*FDOSBoxBasedUserInterpretersExtensions)[I]); else U = "";
        if ((OperationMode == omPortable) && (S != "")) S = MakeExtRelPath(S, PrgDir());
        SetString(500 + I, S);
        SetString(600 + I, T);
        SetString(700 + I, U);
    }
    for (int I = M; I <= 99; I++) {
        SetString(500 + I, "");
        SetString(600 + I, "");
        SetString(700 + I, "");
    }

    /* Windows based emulators */
    M = Max(Max(Max(FWindowsBasedEmulatorsNames->Count(), FWindowsBasedEmulatorsPrograms->Count()), FWindowsBasedEmulatorsParameters->Count()), FWindowsBasedEmulatorsExtensions->Count());
    for (int I = 0; I < M; I++) {
        if (I < FWindowsBasedEmulatorsNames->Count()) S = Trim((*FWindowsBasedEmulatorsNames)[I]); else S = "";
        if (I < FWindowsBasedEmulatorsPrograms->Count()) T = Trim((*FWindowsBasedEmulatorsPrograms)[I]); else T = "";
        if (I < FWindowsBasedEmulatorsParameters->Count()) U = Trim((*FWindowsBasedEmulatorsParameters)[I]); else U = "";
        if (I < FWindowsBasedEmulatorsExtensions->Count()) V = Trim((*FWindowsBasedEmulatorsExtensions)[I]); else V = "";
        if ((OperationMode == omPortable) && (T != "")) T = MakeExtRelPath(T, PrgDir());
        SetString(800 + I, S);
        SetString(900 + I, T);
        SetString(1000 + I, U);
        SetString(1100 + I, V);
    }
    for (int I = M; I <= 99; I++) {
        SetString(800 + I, "");
        SetString(900 + I, "");
        SetString(1000 + I, "");
        SetString(1100 + I, "");
    }
}

void TPrgSetup::ReadSettings()
{
    const QString D = PathDelim;

    AddStringRec(0, "ProgramSets", "DefGameLoc", PrgDataDir() + "VirtualHD" + D);
    AddStringRec(1, "ProgramSets", "DefLoc", PrgDataDir());
    AddStringRec(2, "ProgramSets", "DefDataLoc", PrgDataDir() + "GameData" + D);
    AddStringRec(3, "ProgramSets", "LanguageFile", "English.ini");
    AddStringRec(4, "ProgramSets", "ColOrder", "12345678");
    AddStringRec(5, "ProgramSets", "ColVisible", "11111100");
    AddStringRec(6, "ProgramSets", "ILVS", "List");
    AddStringRec(7, "ProgramSets", "PathToFREEDOS", "." + D + "VirtualHD" + D + "FREEDOS" + D);
    AddStringRec(8, "ProgramSets", "UpdateCheckURL", DFRebornLatestReleaseAPI);
    AddStringRec(9, "ProgramSets", "GamesListViewBackground", "");
    AddStringRec(10, "ProgramSets", "GamesListViewFontColor", "clWindowText");
    AddStringRec(11, "ProgramSets", "ScreenshotsListViewBackground", "");
    AddStringRec(12, "ProgramSets", "ScreenshotsListViewFontColor", "clWindowText");
    AddStringRec(13, "ProgramSets", "GamesTreeViewBackground", "");
    AddStringRec(14, "ProgramSets", "GamesTreeViewFontColor", "clWindowText");
    AddStringRec(15, "ProgramSets", "ToolbarBackground", "");
    AddStringRec(16, "ProgramSets", "UserGroups", "");
    AddStringRec(17, "ProgramSets", "WaveEncMp3", "");
    AddStringRec(18, "ProgramSets", "WaveEncOgg", "");
    AddStringRec(19, "ProgramSets", "WaveEncMp3Parameters", "-h -V 0 \"%1\" \"%2\"");
    AddStringRec(20, "ProgramSets", "WaveEncOggParameters", "\"%1\" --output=\"%2\" --quality=10");
    AddStringRec(21, "ProgramSets", "ValueForNotSet", "");
    AddStringRec(22, "ProgramSets", "ScummVMPath", "");
    AddStringRec(23, "ProgramSets", "ScummVMAdditionalCommandLine", "");
    AddStringRec(24, "ProgramSets", "ProgramVersion", "");
    AddStringRec(25, "WineSupport", "ShellScriptPreamble", "#!/bin/bash");
    AddStringRec(26, "ProgramSets", "QBasic", "");
    AddStringRec(27, "ProgramSets", "QBasicParams", "/run %s");
    AddStringRec(28, "ProgramSets", "LastAddedDriveType", "Drive");
    AddStringRec(29, "ProgramSets", "QuickStarterDOSBoxTemplate", "");
    AddStringRec(30, "ProgramSets", "TextEditor", "");
    AddStringRec(31, "ProgramSets", "ImageViewer", "");
    AddStringRec(32, "ProgramSets", "SoundPlayer", "");
    AddStringRec(33, "ProgramSets", "VideoPlayer", "");
    AddStringRec(34, "ProgramSets", "DeleteToRecycleBin", "1011110");
    AddStringRec(35, "ProgramSets", "FilterMain", "");
    AddStringRec(36, "ProgramSets", "FilterSub", "");
    AddStringRec(37, "ProgramSets", "ColWidths", "");
    AddStringRec(38, "ProgramSets", "ColumnWidthsNoExtraInfoMode", "");
    AddStringRec(39, "ProgramSets", "CaptureDefaultPath", "." + D + CaptureSubDir + D);
    AddStringRec(40, "ProgramSets", "IconSet", "Modern");
    AddStringRec(41, "ProgramSets", "RenameAllExpression", "%P_%N.%E");
    AddStringRec(42, "ProgramSets", "ActiveDataReaderFields", "-XXXXX");
    AddStringRec(43, "ProgramSets", "InstallerNames", DefaultInstallerNames);
    AddStringRec(44, "ProgramSets", "ExportColumns", "XXXXXXXXX");
    AddStringRec(45, "ProgramSets", "ArchiveIDFiles", DefaultArchiveIDFiles);
    AddStringRec(46, "ProgramSets", "LastSelectedProfile", "");
    AddStringRec(47, "ProgramSets", "DefaultTreeFilter", "11111111");
    AddStringRec(48, "ProgramSets", "DefaultFloppyDrive", "");
    AddStringRec(49, "ProgramSets", "WindowsProgramsRunner", "wine");   // D-Fend Reborn

    for (int I = 0; I <= 25; I++) AddStringRec(450 + I, "WineSupport", QString(QChar('A' + I)), "");

    /* DOSBox based user interpreters */
    for (int I = 0; I <= 99; I++) AddStringRec(500 + I, "Interpreters", "Program" + IntToStr(I + 1), "");
    for (int I = 0; I <= 99; I++) AddStringRec(600 + I, "Interpreters", "Parameters" + IntToStr(I + 1), "");
    for (int I = 0; I <= 99; I++) AddStringRec(700 + I, "Interpreters", "Extensions" + IntToStr(I + 1), "");

    /* Windows based emulators */
    for (int I = 0; I <= 99; I++) AddStringRec(800 + I, "WindowsBasedEmulators", "Name" + IntToStr(I + 1), "");
    for (int I = 0; I <= 99; I++) AddStringRec(900 + I, "WindowsBasedEmulators", "Program" + IntToStr(I + 1), "");
    for (int I = 0; I <= 99; I++) AddStringRec(1000 + I, "WindowsBasedEmulators", "Parameters" + IntToStr(I + 1), "");
    for (int I = 0; I <= 99; I++) AddStringRec(1100 + I, "WindowsBasedEmulators", "Extensions" + IntToStr(I + 1), "");

    AddBooleanRec(0, "ProgramSets", "AskBeforeDelete", true);
    AddBooleanRec(1, "ProgramSets", "ShowLastTab", false);
    AddBooleanRec(2, "ProgramSets", "Minimize", false);
    AddBooleanRec(3, "ProgramSets", "MainMaximized", false);
    AddBooleanRec(4, "ProgramSets", "MinimizeToTray", false);
    AddBooleanRec(5, "ProgramSets", "DeleteOnlyInBaseDir", true);
    AddBooleanRec(6, "ProgramSets", "ShowThumbNails", true);
    AddBooleanRec(7, "ProgramSets", "ShowFilterTree", true);
    AddBooleanRec(8, "ProgramSets", "ShowSearchBox", true);
    AddBooleanRec(9, "ProgramSets", "ShowExtrainfo", true);
    AddBooleanRec(10, "ProgramSets", "StartWithWindows", false);
    AddBooleanRec(11, "ProgramSets", "TooltipsInGamesList", true);
    AddBooleanRec(12, "ProgramSets", "DFendStyleProfileEditor", false);
    AddBooleanRec(13, "ProgramSets", "EasySetup", true);
    AddBooleanRec(14, "ProgramSets", "AllowMultiFloppyImagesMount", false);
    AddBooleanRec(15, "ProgramSets", "AllowPhysFSUsage", false);
    AddBooleanRec(16, "ProgramSets", "AllowTextModeLineChange", false);
    AddBooleanRec(17, "ProgramSets", "VersionSpecificUpdateCheck", true);
    AddBooleanRec(18, "ProgramSets", "UseShortFolderNames", true);
    AddBooleanRec(19, "ProgramSets", "AlwaysSetScreenshotFolderAutomatically", true);
    AddBooleanRec(20, "ProgramSets", "ShowXMLExportMenuItem", false);
    AddBooleanRec(21, "ProgramSets", "ShowXMLImportMenuItem", false);
    AddBooleanRec(22, "ProgramSets", "MinimizeOnScummVMStart", false);
    AddBooleanRec(23, "WineSupport", "Enable", false);
    AddBooleanRec(24, "WineSupport", "LinuxLinkMode", false);
    AddBooleanRec(25, "WineSupport", "RemapMounts", false);
    AddBooleanRec(26, "WineSupport", "RemapScreenshotFolder", false);
    AddBooleanRec(27, "WineSupport", "RemapMapperFile", false);
    AddBooleanRec(28, "WineSupport", "RemapDOSBoxFolder", false);
    AddBooleanRec(29, "ProgramSets", "UseCheckSumsForProfiles", true);
    AddBooleanRec(30, "ProgramSets", "AllowVGAChipsetSettings", false);
    AddBooleanRec(31, "ProgramSets", "AllowGlideSettings", false);
    AddBooleanRec(32, "ProgramSets", "AllowPrinterSettings", false);
    AddBooleanRec(33, "ProgramSets", "AllowNe2000", false);
    AddBooleanRec(34, "ProgramSets", "AllowInnova", false);
    AddBooleanRec(35, "ProgramSets", "ShowToolbar", true);
    AddBooleanRec(36, "ProgramSets", "ShowToolbarCaptions", true);
    AddBooleanRec(37, "ProgramSets", "ShowToolbarButtonClose", false);
    AddBooleanRec(38, "ProgramSets", "ShowToolbarButtonRun", true);
    AddBooleanRec(39, "ProgramSets", "ShowToolbarButtonRunSetup", false);
    AddBooleanRec(40, "ProgramSets", "ShowToolbarButtonAdd", true);
    AddBooleanRec(41, "ProgramSets", "ShowToolbarButtonEdit", true);
    AddBooleanRec(42, "ProgramSets", "ShowToolbarButtonDelete", true);
    AddBooleanRec(43, "ProgramSets", "ShowToolbarButtonHelp", true);
    AddBooleanRec(44, "ProgramSets", "RestoreWindowWhenDOSBoxCloses", false);
    AddBooleanRec(45, "ProgramSets", "NonFavorites.Bold", false);
    AddBooleanRec(46, "ProgramSets", "NonFavorites.Italic", false);
    AddBooleanRec(47, "ProgramSets", "NonFavorites.Underline", false);
    AddBooleanRec(48, "ProgramSets", "Favorites.Bold", true);
    AddBooleanRec(49, "ProgramSets", "Favorites.Italic", false);
    AddBooleanRec(50, "ProgramSets", "Favorites.Underline", false);
    AddBooleanRec(51, "ProgramSets", "ScreenshotsGamesList.UseFirstScreenshot", true);
    AddBooleanRec(52, "ProgramSets", "ShowFullscreenInfo", true);
    AddBooleanRec(53, "ProgramSets", "GridLinesInGamesList", false);
    AddBooleanRec(54, "ProgramSets", "QuickStarterMaximized", false);
    AddBooleanRec(55, "ProgramSets", "QuickStarterDOSBoxFullscreen", false);
    AddBooleanRec(56, "ProgramSets", "QuickStarterDOSBoxAutoClose", true);
    AddBooleanRec(57, "ProgramSets", "FileTypeFallbackForEditor", true);
    AddBooleanRec(58, "ProgramSets", "AutoFixLineWrap", false);
    AddBooleanRec(59, "ProgramSets", "CenterScummVMWindow", false);
    AddBooleanRec(60, "ProgramSets", "HideScummVMConsole", true);
    AddBooleanRec(61, "ProgramSets", "ShowShortNameWarnings", true);
    AddBooleanRec(62, "ProgramSets", "RestoreWhenScummVMCloses", false);
    AddBooleanRec(63, "ProgramSets", "UseWindowsExeIcons", true);
    AddBooleanRec(64, "ProgramSets", "MinimizeOnWindowsGameStart", false);
    AddBooleanRec(65, "ProgramSets", "RestoreWhenWindowsGameCloses", false);
    AddBooleanRec(66, "ProgramSets", "RestoreFilter", true);
    AddBooleanRec(67, "ProgramSets", "AllowPixelShader", false);
    AddBooleanRec(68, "ProgramSets", "StoreColWidths", false);
    AddBooleanRec(69, "ProgramSets", "RenameProfFileOnRenamingProfile", false);
    AddBooleanRec(70, "ProgramSets", "ScanFolderAllGames", false);
    AddBooleanRec(71, "ProgramSets", "ScanFolderUseFileName", true);
    AddBooleanRec(72, "ProgramSets", "ShowAddGameInfoOnEmptyGamesList", true);
    AddBooleanRec(73, "ProgramSets", "RestorePreviewerCategory", true);
    AddBooleanRec(74, "ProgramSets", "ShowMainMenu", true);
    AddBooleanRec(75, "ProgramSets", "IgnoreDirectoryCollisions", false);
    AddBooleanRec(76, "ProgramSets", "CreateConfFilesForProfiles", false);
    AddBooleanRec(77, "ProgramSets", "AddMountingDataAutomatically", true);
    AddBooleanRec(78, "ProgramSets", "BinaryCache", true);
    AddBooleanRec(79, "ProgramSets", "ImportZipWithoutDialogIfPossible", true);
    AddBooleanRec(80, "ProgramSets", "DOSBoxStartLogging", false);
    AddBooleanRec(81, "ProgramSets", "DataReaderAllPlatforms", false);
    AddBooleanRec(82, "ProgramSets", "StoreHistory", true);
    AddBooleanRec(83, "ProgramSets", "RestoreLastSelectedProfile", false);
    AddBooleanRec(84, "ProgramSets", "OldDeleteMenuItem", false);
    AddBooleanRec(85, "ProgramSets", "DefaultUninstall", true);
    AddBooleanRec(86, "ProgramSets", "SingleInstanceOnly", true);
    AddBooleanRec(87, "ProgramSets", "NonModalViewer", false);
    AddBooleanRec(88, "ProgramSets", "AllowPackingWindowsGames", false);
    AddBooleanRec(90, "ProgramSets", "OfferRunAsAdmin", false);
    /* AddBooleanRec(91,'ProgramSets','ActivateIncompleteFeatures',False); */
    AddBooleanRec(92, "ProgramSets", "DOSBoxFullSearchDone", false);   // D-Fend Reborn
    AddBooleanRec(93, "ProgramSets", "UseWindowsProgramsRunner", true);   // D-Fend Reborn

    AddIntegerRec(0, "ProgramSets", "MainLeft", -1);
    AddIntegerRec(1, "ProgramSets", "MainTop", -1);
    AddIntegerRec(2, "ProgramSets", "MainWidth", -1);
    AddIntegerRec(3, "ProgramSets", "MainHeight", -1);
    AddIntegerRec(4, "ProgramSets", "TreeWidth", -1);
    AddIntegerRec(5, "ProgramSets", "ScreenshotHeight", -1);
    AddIntegerRec(6, "ProgramSets", "ScreenshotPreviewHeight", 100);
    AddIntegerRec(7, "ProgramSets", "AddButtonFunction", 2);
    AddIntegerRec(8, "ProgramSets", "CheckForUpdates", 0);
    AddIntegerRec(9, "ProgramSets", "LastUpdateCheck", 0);
    AddIntegerRec(10, "ProgramSets", "StartWindowSize", 0);
    AddIntegerRec(11, "ProgramSets", "GamesListViewFontSize", 9);
    AddIntegerRec(12, "ProgramSets", "ScreenshotsListViewFontSize", 9);
    AddIntegerRec(13, "ProgramSets", "GamesTreeViewFontSize", 9);
    AddIntegerRec(14, "ProgramSets", "ToolbarFontSize", 9);
    AddIntegerRec(15, "ProgramSets", "CompressionLevel", 3);
    AddIntegerRec(16, "ProgramSets", "ScreenshotsGamesList.Width", 150);
    AddIntegerRec(17, "ProgramSets", "ScreenshotsGamesList.Height", 100);
    AddIntegerRec(18, "ProgramSets", "QuickStarterLeft", -1);
    AddIntegerRec(19, "ProgramSets", "QuickStarterTop", -1);
    AddIntegerRec(20, "ProgramSets", "QuickStarterWidth", -1);
    AddIntegerRec(21, "ProgramSets", "QuickStarterHeight", -1);
    AddIntegerRec(22, "ProgramSets", "ScreenshotsGamesList.UseScreenshotNr", 1);
    AddIntegerRec(23, "ProgramSets", "DOSBoxShortFileNameAlgorithm", 3);
    AddIntegerRec(24, "ProgramSets", "LastWizardMode", 1);
    AddIntegerRec(25, "ProgramSets", "IconSize", 32);
    AddIntegerRec(26, "ProgramSets", "ImageFilter", 6);
    AddIntegerRec(27, "ProgramSets", "NoteLinesInTooltips", 5);
    AddIntegerRec(28, "ProgramSets", "PreviewerCategory", 0);
    AddIntegerRec(29, "ProgramSets", "LanguageEditorViewMode", 0);
    AddIntegerRec(32, "ProgramSets", "PackageListsCheckForUpdates", 0);
    AddIntegerRec(33, "ProgramSets", "LastPackageListeUpdateCheck", 0);
    AddIntegerRec(34, "ProgramSets", "CheatsDBCheckForUpdates", 0);
    AddIntegerRec(35, "ProgramSets", "LastCheatsDBUpdateCheck", 0);
    AddIntegerRec(36, "ProgramSets", "DOSBoxStartFailedTimeout", 3);
    AddIntegerRec(37, "ProgramSets", "DataReaderMaxImages", 25);
    AddIntegerRec(38, "ProgramSets", "ThumbnailCacheSize", 500);
    AddIntegerRec(39, "ProgramSets", "DataReaderSource", 0);
    AddIntegerRec(40, "ProgramSets", "GameInfoSource", 0);   // D-Fend Reborn: 0=auto setup templates, 1=Wikidata
    AddIntegerRec(41, "ProgramSets", "ColorScheme", 0);   // D-Fend Reborn: 0=as the operating system, 1=light, 2=dark
}

void TPrgSetup::InitDirs()
{
    if (BaseDir() == "") SetBaseDir(PrgDataDir());
    if (GameDir() == "") SetGameDir(BaseDir() + "VirtualHD" + PathDelim);
    if (DataDir() == "") SetDataDir(BaseDir() + "GameData" + PathDelim);
    if (CaptureDir() == "") SetCaptureDir(BaseDir() + CaptureSubDir + PathDelim);

    if (OperationMode == omPortable) {
        SetBaseDir(MakeExtAbsPath(BaseDir(), PrgDir()));
        SetGameDir(MakeExtAbsPath(GameDir(), PrgDir()));
        SetDataDir(MakeExtAbsPath(DataDir(), PrgDir()));
        SetCaptureDir(MakeExtAbsPath(CaptureDir(), PrgDir()));
        SetScummVMPath(MakeExtAbsPath(ScummVMPath(), PrgDir()));
        SetQBasic(MakeExtAbsPath(QBasic(), PrgDir()));
        SetWaveEncOgg(MakeExtAbsPath(WaveEncOgg(), PrgDir()));
        SetWaveEncMp3(MakeExtAbsPath(WaveEncMp3(), PrgDir()));

        SetCaptureDir(MakeRelPath(CaptureDir(), BaseDir()));
    }

    if (!FileExists(WaveEncOgg())) {
        // program folder, Bin folder (and on Linux/macOS the installed oggenc)
        QString S = FindHelperProgram(OggEncPrgFile);
        if (!S.isEmpty()) SetWaveEncOgg(S);
    }
}

void TPrgSetup::DoneDirs()
{
    if (OperationMode == omPortable) {
        SetCaptureDir(MakeAbsPath(CaptureDir(), BaseDir()));

        SetBaseDir(MakeExtRelPath(BaseDir(), PrgDir()));
        SetGameDir(MakeExtRelPath(GameDir(), PrgDir()));
        SetDataDir(MakeExtRelPath(DataDir(), PrgDir()));
        SetCaptureDir(MakeExtRelPath(CaptureDir(), PrgDir()));
        SetScummVMPath(MakeExtRelPath(ScummVMPath(), PrgDir()));
        SetQBasic(MakeExtRelPath(QBasic(), PrgDir()));
        SetWaveEncOgg(MakeExtRelPath(WaveEncOgg(), PrgDir()));
        SetWaveEncMp3(MakeExtRelPath(WaveEncMp3(), PrgDir()));
    }
}

QString TPrgSetup::GetDriveLetter(QChar DriveLetter)
{
    return GetString(450 + DriveLetter.toUpper().unicode() - 'A');
}

void TPrgSetup::SetDriveLetter(QChar DriveLetter, const QString &Value)
{
    SetString(450 + DriveLetter.toUpper().unicode() - 'A', Value);
}

int TPrgSetup::GetListCount(int Index)
{
    switch (Index) {
        case 0: return FDOSBox->size();
        case 1: return FPacker->size();
        default: return 0;
    }
}

TDOSBoxSetting *TPrgSetup::GetDOSBoxSettings(int I)
{
    if ((I < 0) || (I >= FDOSBox->size())) return nullptr;
    return static_cast<TDOSBoxSetting *>((*FDOSBox)[I]);
}

TPackerSetting *TPrgSetup::GetPackerSettings(int I)
{
    if ((I < 0) || (I >= FPacker->size())) return nullptr;
    return static_cast<TPackerSetting *>((*FPacker)[I]);
}

int TPrgSetup::AddDOSBoxSettings(const QString &Name)
{
    FDOSBox->append(new TDOSBoxSetting(this, FDOSBox->size()));
    int result = FDOSBox->size() - 1;
    static_cast<TDOSBoxSetting *>((*FDOSBox)[result])->SetName(Name);
    return result;
}

bool TPrgSetup::DeleteDOSBoxSettings(int ANr)
{
    bool result = (ANr > 0) && (ANr < FDOSBox->size()); /* do not delete ANr=0 */
    if (!result) return result;

    delete static_cast<TDOSBoxSetting *>((*FDOSBox)[ANr]);
    FDOSBox->removeAt(ANr);
    for (int I = ANr; I < FDOSBox->size(); I++) static_cast<TDOSBoxSetting *>((*FDOSBox)[I])->SetNr(I);
    return result;
}

bool TPrgSetup::SwapDOSBoxSettings(int ANr1, int ANr2)
{
    bool result = (ANr1 > 0) && (ANr1 < FDOSBox->size()) && (ANr2 > 0) && (ANr2 < FDOSBox->size());
    if (!result) return result;

    FDOSBox->swapItemsAt(ANr1, ANr2);
    static_cast<TDOSBoxSetting *>((*FDOSBox)[ANr1])->SetNr(ANr1);
    static_cast<TDOSBoxSetting *>((*FDOSBox)[ANr2])->SetNr(ANr2);
    return result;
}

int TPrgSetup::AddPackerSettings(const QString &Name)
{
    FPacker->append(new TPackerSetting(this, FPacker->size()));
    int result = FPacker->size() - 1;
    static_cast<TPackerSetting *>((*FPacker)[result])->SetName(Name);
    return result;
}

bool TPrgSetup::DeletePackerSettings(int ANr)
{
    bool result = (ANr >= 0) && (ANr < FPacker->size());
    if (!result) return result;

    delete static_cast<TPackerSetting *>((*FPacker)[ANr]);
    FPacker->removeAt(ANr);
    for (int I = ANr; I < FPacker->size(); I++) static_cast<TPackerSetting *>((*FPacker)[I])->SetNr(I);
    return result;
}

bool TPrgSetup::SwapPackerSettings(int ANr1, int ANr2)
{
    bool result = (ANr1 >= 0) && (ANr1 < FPacker->size()) && (ANr2 >= 0) && (ANr2 < FPacker->size());
    if (!result) return result;

    FPacker->swapItemsAt(ANr1, ANr2);
    static_cast<TPackerSetting *>((*FPacker)[ANr1])->SetNr(ANr1);
    static_cast<TPackerSetting *>((*FPacker)[ANr2])->SetNr(ANr2);
    return result;
}

/* global */

static void ReadOperationMode()
{
    OperationMode = omPrgDir;
    if (!FileExists(PrgDir() + OperationModeConfig)) return;

    TStringList St;
    if (!St.LoadFromFile(PrgDir() + OperationModeConfig)) return;
    for (int I = 0; I < St.Count(); I++) {
        QString S = Trim(ExtUpperCase(St[I]));
        if (S == "PRGDIRMODE") { OperationMode = omPrgDir; return; }
        if (S == "USERDIRMODE") { OperationMode = omUserDir; return; }
        if (S == "PORTABLEMODE") { OperationMode = omPortable; return; }
    }
}

QString UserDataDir()
{
#ifdef Q_OS_WIN
    return IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_PROFILE)) + "D-Fend Reborn" + PathDelim;
#else
    // ~/.local/share/D-Fend Reborn/ (macOS: ~/Library/Application Support/D-Fend Reborn/)
    return IncludeTrailingPathDelimiter(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)) + "D-Fend Reborn" + PathDelim;
#endif
}

QString PrgDataDir()
{
    if (OperationMode == omUserDir) return UserDataDir();
    return PrgDir();
}

bool WineSupportEnabled()
{
    if (ParamCount() == 1) {
        QString S = Trim(ExtUpperCase(ParamStr(1)));
        if ((Pos("WINEMODE", S) > 0) || (Pos("WINESUPPORTENABLED", S) > 0)) return true;
        if ((Pos("WINDOWSMODE", S) > 0) || (Pos("NOWINESUPPORT", S) > 0) || (Pos("WINESUPPORTDISABLED", S) > 0)) return false;
    }
    return PrgSetup->EnableWineMode();
}

void DOSBoxSettingToDOSBoxData(TDOSBoxSetting *DOSBoxSetting, TDOSBoxData &DOSBoxData)
{
    DOSBoxData.Name = DOSBoxSetting->Name();
    DOSBoxData.DosBoxDir = DOSBoxSetting->DosBoxDir();
    DOSBoxData.DosBoxExecutable = DOSBoxSetting->DosBoxExecutable();
    DOSBoxData.DosBoxMapperFile = DOSBoxSetting->DosBoxMapperFile();
    DOSBoxData.DosBoxLanguage = DOSBoxSetting->DosBoxLanguage();
    DOSBoxData.SDLVideodriver = DOSBoxSetting->SDLVideodriver();
    DOSBoxData.CommandLineParameters = DOSBoxSetting->CommandLineParameters();
    DOSBoxData.CustomSettings = DOSBoxSetting->CustomSettings();
    DOSBoxData.KeyboardLayout = DOSBoxSetting->KeyboardLayout();
    DOSBoxData.Codepage = DOSBoxSetting->Codepage();
    DOSBoxData.HideDosBoxConsole = DOSBoxSetting->HideDosBoxConsole();
    DOSBoxData.CenterDOSBoxWindow = DOSBoxSetting->CenterDOSBoxWindow();
    DOSBoxData.DisableScreensaver = DOSBoxSetting->DisableScreensaver();
    DOSBoxData.WaitOnError = DOSBoxSetting->WaitOnError();
}

void DOSBoxDataToDOSBoxSetting(const TDOSBoxData &DOSBoxData, TDOSBoxSetting *DOSBoxSetting)
{
    DOSBoxSetting->SetName(DOSBoxData.Name);
    DOSBoxSetting->SetDosBoxDir(DOSBoxData.DosBoxDir);
    DOSBoxSetting->SetDosBoxExecutable(DOSBoxData.DosBoxExecutable);
    DOSBoxSetting->SetDosBoxMapperFile(DOSBoxData.DosBoxMapperFile);
    DOSBoxSetting->SetDosBoxLanguage(DOSBoxData.DosBoxLanguage);
    DOSBoxSetting->SetSDLVideodriver(DOSBoxData.SDLVideodriver);
    DOSBoxSetting->SetCommandLineParameters(DOSBoxData.CommandLineParameters);
    DOSBoxSetting->SetCustomSettings(DOSBoxData.CustomSettings);
    DOSBoxSetting->SetKeyboardLayout(DOSBoxData.KeyboardLayout);
    DOSBoxSetting->SetCodepage(DOSBoxData.Codepage);
    DOSBoxSetting->SetHideDosBoxConsole(DOSBoxData.HideDosBoxConsole);
    DOSBoxSetting->SetCenterDOSBoxWindow(DOSBoxData.CenterDOSBoxWindow);
    DOSBoxSetting->SetDisableScreensaver(DOSBoxData.DisableScreensaver);
    DOSBoxSetting->SetWaitOnError(DOSBoxData.WaitOnError);
}

void InitDOSBoxData(TDOSBoxData &DOSBoxData)
{
    DOSBoxData.Name = "";
    DOSBoxData.DosBoxDir = "";
    DOSBoxData.DosBoxExecutable = "";
    DOSBoxData.DosBoxMapperFile = "." + PathDelim + "mapper.txt";
    DOSBoxData.DosBoxLanguage = "";
    DOSBoxData.SDLVideodriver = "DirectX";
    DOSBoxData.CommandLineParameters = "";
    DOSBoxData.CustomSettings = "";
    DOSBoxData.KeyboardLayout = "";
    DOSBoxData.Codepage = "";
    DOSBoxData.HideDosBoxConsole = true;
    DOSBoxData.CenterDOSBoxWindow = false;
    DOSBoxData.DisableScreensaver = false;
    DOSBoxData.WaitOnError = true;
}

void PrgSetupUnit_Initialization()
{
    ReadOperationMode();
    UpdateSettingsFilesLocation();
    PrgSetup = new TPrgSetup();
}

void PrgSetupUnit_Finalization()
{
    delete PrgSetup;
    PrgSetup = nullptr;
}
