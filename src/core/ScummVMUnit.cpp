// C++ port of ScummVMUnit.pas (implementation section).
#include "ScummVMUnit.h"

#include <QProcess>
#include <QStandardPaths>

#include "Process.h"
#include "IniFiles.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "GameDBToolsUnit.h"
#include "LanguageSetupUnit.h"
#include "ScummVMToolsUnit.h"
#include "ZipManagerUnit.h"
#include "DOSBoxCountUnit.h"
#include "RunPrgManagerUnit.h"
#include "HistoryUnit.h"
#include "MainUnit.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif

// {DEFINE AddDFRPrefixInRunMode}

bool MinimizedAtScummVMStart = false;

// THandle is a process id in the port (see Process.h); 0 = INVALID_HANDLE_VALUE.
static const THandle InvalidHandle = 0;

// --- Application (Forms.TApplication) helpers -------------------------------

static void ApplicationRestore()
{
    if (DFendRebornMainForm == nullptr) return;
    if (DFendRebornMainForm->isMinimized()) DFendRebornMainForm->showNormal();
    DFendRebornMainForm->activateWindow();
}

static void ApplicationMinimize()
{
    if (DFendRebornMainForm != nullptr) DFendRebornMainForm->showMinimized();
}

static bool MainFormIsMinimized()
{
    return (DFendRebornMainForm != nullptr) && DFendRebornMainForm->isMinimized();
}

QString FindScummVMIni(bool UseSecondOne)
{
    QString S;

    if (!UseSecondOne) {
        S = ScummVMGamesList->ScummVMIniFile();
        if (FileExists(S)) { return S; }

        S = IncludeTrailingPathDelimiter(GetSpecialFolder(WId(0), CSIDL_APPDATA)) + "ScummVM" + PathDelim;
        if (FileExists(S + ScummVMConfFileName)) { return S + ScummVMConfFileName; }

#ifdef Q_OS_WIN
        wchar_t Buf[520];
        UINT Len = GetWindowsDirectoryW(Buf, 512);
        S = IncludeTrailingPathDelimiter(QString::fromWCharArray(Buf, static_cast<int>(Len)));
        if (FileExists(S + ScummVMConfFileName)) { return S + ScummVMConfFileName; }
#else
        // Port note: default location of the ScummVM configuration on non Windows systems
        S = IncludeTrailingPathDelimiter(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)) + "scummvm" + PathDelim;
        if (FileExists(S + ScummVMConfFileName)) { return S + ScummVMConfFileName; }
#endif
    }

    S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath());
    if (FileExists(S + ScummVMConfFileName)) { return S + ScummVMConfFileName; }
    return "";
}

static void AddKeysFromDefaultScummVMIni(TStringList* St1, TStringList* St2, TIniFile* Ini, const QString& GameName)
{
    Q_UNUSED(GameName);
    TStringList *KnownKeys, *Section;
    int I, J;
    QString S;

    KnownKeys = new TStringList();
    Section = new TStringList();

    for (I = 0; I < St1->Count(); I++) { S = Trim(ExtUpperCase((*St1)[I])); J = Pos("=", S); if ((!S.isEmpty()) && (S[0] != '[') && (J > 0)) KnownKeys->Add(Trim(Copy(S, 1, J - 1))); }
    for (I = 0; I < St2->Count(); I++) { S = Trim(ExtUpperCase((*St2)[I])); J = Pos("=", S); if ((!S.isEmpty()) && (S[0] != '[') && (J > 0)) KnownKeys->Add(Trim(Copy(S, 1, J - 1))); }

    /* not copy [game] section
    Section.Clear;
    Ini.ReadSection(GameName,Section);
    For I:=0 to Section.Count-1 do begin
      If KnownKeys.IndexOf(ExtUpperCase(Section[I]))>=0 then continue;
      St2.Add(Section[I]+'='+Ini.ReadString(GameName,Section[I],''));
      KnownKeys.Add(ExtUpperCase(Section[I]));
    end; */

    Section->Clear();
    Ini->ReadSection("scummvm", *Section);
    for (I = 0; I < Section->Count(); I++) {
        if (KnownKeys->IndexOf(ExtUpperCase((*Section)[I])) >= 0) continue;
        St1->Add((*Section)[I] + "=" + Ini->ReadString("scummvm", (*Section)[I], ""));
        KnownKeys->Add(ExtUpperCase((*Section)[I]));
    }

    delete KnownKeys;
    delete Section;
}

static QString FindTheme(const QString& ScummVMPath)
{
    TSearchRec Rec;
    int I;
    QString result = "";
    I = FindFirst(ScummVMPath + "*.zip", faAnyFile, Rec);
    while (I == 0) {
        if (FileExists(ScummVMPath + ChangeFileExt(Rec.Name, ".ini"))) {
            result = ChangeFileExt(Rec.Name, ""); break;
        }
        I = FindNext(Rec);
    }
    FindClose(Rec);
    return result;
}

TStringList* BuildScummVMIniFile(TGame* Game, bool RunMode)
{
    Q_UNUSED(RunMode);
    QString S;
    TIniFile *Ini, *Ini2;
    TStringList *St1, *St2, *St3;

    TStringList* result = new TStringList();

    St1 = new TStringList();
    St2 = new TStringList();

    St1->Add("[scummvm]");
    St1->Add("gfx_mode=" + Game->ScummVMFilter());
    if (Game->StartFullscreen()) St1->Add("fullscreen=true"); else St1->Add("fullscreen=false");
    if (Game->ScummVMConfirmExit()) St1->Add("confirm_exit=true"); else St1->Add("confirm_exit=false");
    if (PrgSetup->HideScummVMConsole()) St1->Add("console=false");

    // {$IFDEF AddDFRPrefixInRunMode} If RunMode then S:='DFR' else {$ENDIF}
    S = "";
    St2->Add("[" + S + Game->ScummVMGame() + "]");
    St2->Add("gameid=" + Game->ScummVMGame());

    if (Trim(Game->ScummVMPath()).isEmpty()) S = ""; else S = IncludeTrailingPathDelimiter(MakeAbsPath(Game->ScummVMPath(), PrgSetup->BaseDir()));
    St2->Add("path=" + S);

    if (!Trim(Game->ScummVMSavePath()).isEmpty()) S = IncludeTrailingPathDelimiter(MakeAbsPath(Game->ScummVMSavePath(), PrgSetup->BaseDir())); else {
        if (Trim(Game->ScummVMPath()).isEmpty()) S = ""; else S = IncludeTrailingPathDelimiter(MakeAbsPath(Game->ScummVMPath(), PrgSetup->BaseDir()));
    }
    St2->Add("savepath=" + S);

    St2->Add("autosave_period=" + IntToStr(Game->ScummVMAutosave()));
    if (!Trim(Game->ScummVMLanguage()).isEmpty()) St2->Add("language=" + Game->ScummVMLanguage());
    St2->Add("music_volume=" + IntToStr(Game->ScummVMMusicVolume()));
    St2->Add("speech_volume=" + IntToStr(Game->ScummVMSpeechVolume()));
    St2->Add("sfx_volume=" + IntToStr(Game->ScummVMSFXVolume()));
    St2->Add("midi_gain=" + IntToStr(Game->ScummVMMIDIGain()));
    St2->Add("output_rate=" + IntToStr(Game->ScummVMSampleRate()));
    if (Game->AspectCorrection()) St2->Add("aspect_ratio=true"); else St2->Add("aspect_ratio=false");
    S = Game->ScummVMMusicDriver(); if ((!S.isEmpty()) && (S[Length(S) - 1] == ')')) { S.chop(1); Game->SetScummVMMusicDriver(S); }
    St2->Add("music_driver=" + Game->ScummVMMusicDriver());
    if (Game->ScummVMNativeMT32()) St2->Add("native_mt32=true"); else St2->Add("native_mt32=false");
    if (Game->ScummVMEnableGS()) St2->Add("enable_gs=true"); else St2->Add("enable_gs=false");
    if (Game->ScummVMMultiMIDI()) St2->Add("multi_midi=true"); else St2->Add("multi_midi=false");
    St2->Add("talkspeed=" + IntToStr(Game->ScummVMTalkSpeed()));
    if (Game->ScummVMSpeechMute()) St2->Add("speech_mute=true"); else St2->Add("speech_mute=false");
    if (Game->ScummVMSubtitles()) St2->Add("subtitles=true"); else St2->Add("subtitles=false");
    St2->Add("cdrom=" + IntToStr(Game->ScummVMCDROM()));
    St2->Add("joystick_num=" + IntToStr(Game->ScummVMJoystickNum()));
    if (!Trim(Game->ScummVMExtraPath()).isEmpty()) St2->Add("extrapath=" + MakeAbsPath(Game->ScummVMExtraPath(), PrgSetup->BaseDir()));

    S = Trim(LowerCase(Game->ScummVMGame()));

    if ((S == "sky") || (S == "queen")) {
        if (Game->ScummVMAltIntro()) St2->Add("alt_intro=true"); else St2->Add("alt_intro=false");
    }

    if (S == "sword2") {
        St2->Add("gfx_details=" + IntToStr(Game->ScummVMGFXDetails()));
        if (Game->ScummVMMusicMute()) St2->Add("music_mute=true"); else St2->Add("music_mute=false");
        if (Game->ScummVMObjectLabels()) St2->Add("object_labels=true"); else St2->Add("object_labels=false");
        if (Game->ScummVMReverseStereo()) St2->Add("reverse_stereo=true"); else St2->Add("reverse_stereo=false");
        if (Game->ScummVMSFXMute()) St2->Add("sfx_mute=true"); else St2->Add("sfx_mute=false");
    }

    if ((S == "queen") || (S == "simon1") || (S == "simon2")) {
        if (Game->ScummVMMusicMute()) St2->Add("music_mute=true"); else St2->Add("music_mute=false");
        if (Game->ScummVMSFXMute()) St2->Add("sfx_mute=true"); else St2->Add("sfx_mute=false");
    }

    if (S == "kyra1") {
        St2->Add("walkspeed=" + IntToStr(Game->ScummVMWalkspeed()));
    }

    S = FindScummVMIni();
    if (!S.isEmpty()) {
        Ini = new TIniFile(S);
        S = FindScummVMIni(true); if (!S.isEmpty()) Ini2 = new TIniFile(S); else Ini2 = nullptr;

        S = Ini->ReadString("scummvm", "themepath", "");
        if ((S.isEmpty()) && (Ini2 != nullptr)) S = Ini2->ReadString("scummvm", "themepath", "");
        if (S.isEmpty()) S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath());
        St1->Add("themepath=" + S);

        S = Ini->ReadString("scummvm", "gui_theme", "");
        if ((S.isEmpty()) && (Ini2 != nullptr)) S = Ini2->ReadString("scummvm", "gui_theme", "");
        if (S.isEmpty()) S = FindTheme(IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()));
        St1->Add("gui_theme=" + S);

        S = Ini->ReadString("scummvm", "extrapath", "");
        if ((S.isEmpty()) && (Ini2 != nullptr)) S = Ini2->ReadString("scummvm", "extrapath", "");
        if (S.isEmpty()) S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath());
        St1->Add("extrapath=" + S);

        delete Ini2;
        AddKeysFromDefaultScummVMIni(St1, St2, Ini, Game->ScummVMGame());
        delete Ini;
    } else {
        St2->Add("themepath=\"" + IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "\"");
    }

    result->AddStrings(*St1);
    result->Add("");
    result->AddStrings(*St2);

    result->Add("");
    if (!Trim(Game->CustomSettings()).isEmpty()) {
        St3 = StringToStringList(Game->CustomSettings());
        result->AddStrings(*St3);
        delete St3;
    }

    delete St1;
    delete St2;
    return result;
}

QString GetScummVMCommandLine(const QString& INIFile, const QString& GameName, const QString& AdditionalCommandLine, const QString& RenderMode, const QString& DataPlatform)
{
    QString RenderModeParam, PlatformParam;
    QString result;

    RenderModeParam = "";
    if ((!Trim(RenderMode).isEmpty()) && (Trim(ExtUpperCase(RenderMode)) != "DEFAULT"))
        RenderModeParam = "--render-mode=" + RenderMode + " ";

    PlatformParam = "";
    if ((!Trim(DataPlatform).isEmpty()) && (Trim(ExtUpperCase(DataPlatform)) != "AUTO"))
        PlatformParam = "--platform=" + DataPlatform + " ";

    result = "";
    if (!Trim(AdditionalCommandLine).isEmpty()) result = result + AdditionalCommandLine + " ";
    if (PrgSetup->HideScummVMConsole()) result = result + "--no-console ";

    result = result + "--config=\"" + INIFile + "\" " + RenderModeParam + PlatformParam + GameName;
    return result;
}

static THandle RunScummVM(const QString& INIFile, const QString& GameName, const QString& AdditionalCommandLine, bool FullScreen, const QString& GameDir, const QString& ScreenshotDir, const QString& RenderMode, const QString& DataPlatform, bool asAdmin)
{
    Q_UNUSED(GameDir);
    QString PrgFile, Params;
    bool Waited;
    THandle result = InvalidHandle;

    PrgFile = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + ScummPrgFile;
#ifndef Q_OS_WIN
    // Linux/macOS: ScummVM installed in the search path (no folder configured)
    if (!FileExists(PrgFile)) {
        QString T = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "scummvm";
        if (Trim(PrgSetup->ScummVMPath()).isEmpty()) T = QStandardPaths::findExecutable("scummvm");
        if (!T.isEmpty() && FileExists(T)) PrgFile = T;
    }
#endif
    if (!FileExists(PrgFile)) {
        ApplicationRestore();
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindScummVM(), {PrgFile}), mtError, mbOK, 0);
        return result;
    }

    Params = GetScummVMCommandLine(INIFile, GameName, AdditionalCommandLine, RenderMode, DataPlatform);

    if (asAdmin) {
#ifdef Q_OS_WIN
        ShellOpen(PrgDir() + BinFolder + PathDelim + "AdminLauncher.exe", "/dir=" + ScreenshotDir + " /run=\"" + PrgFile + "\" " + Params, ScreenshotDir);
        return result;
#else
        // Port note: AdminLauncher.exe only exists on Windows - start ScummVM normally
#endif
    }

    if (!ScreenshotDir.isEmpty()) {
        result = StartProcessNative(PrgFile, Params, ScreenshotDir);
    } else {
        result = StartProcessNative(PrgFile, Params, ExtractFilePath(PrgFile));
        if (result == 0) {
            ApplicationRestore();
            MessageDlg(Format(LanguageSetup->MessageCouldNotStartProgram(), {PrgFile}), mtError, mbOK, 0);
            return InvalidHandle;
        }
    }

    if (PrgSetup->CenterScummVMWindow() && (!FullScreen)) {
        Sleep(1000); Waited = true;
        CenterWindowFromProcessID(result);
    } else {
        Waited = false;
    }

    if (PrgSetup->MinimizeOnScummVMStart()) {
        if (!Waited) { Sleep(1000); Waited = true; }
        // {MinimizedAtScummVMStart:=True; -> RunScummVMGame}
    }

    if (PrgSetup->HideScummVMConsole()) {
        if (!Waited) Sleep(1000);
        HideWindowFromProcessIDAndTitle(result, "\\scummvm.exe");
    }

    ScummVMCounter->Add(result);
    return result;
}

void RunScummVMGame(TGame* Game)
{
    TStringList* St;
    QString S, Params, Dir;
    int ZipRecNr;
    bool Error;
    THandle ScummVMHandle;
    bool AlreadyMinimized;
    bool RunAsAdmin;

    AlreadyMinimized = false;

    ZipRecNr = ZipManager->AddGame(Game, Error);
    if (Error) return;

    St = BuildScummVMIniFile(Game, true);
    do {
        if (!St->SaveToFile(TempDir() + ScummVMConfFileName)) {
            ApplicationRestore();
            MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {TempDir() + ScummVMConfFileName}), mtError, mbOK, 0);
            break;
        }
        History->Add(Game->Name());

        S = "";
        if (!Trim(Game->CaptureFolder()).isEmpty()) {
            S = MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir());
            if (!ForceDirectories(S)) S = "";
        }

        RunPrgManager->RunBeforeExecutionCommand(Game);

        if (PrgSetup->MinimizeOnScummVMStart()) {
            AlreadyMinimized = MainFormIsMinimized();
            ApplicationMinimize();
        }

        Params = Trim(PrgSetup->ScummVMAdditionalCommandLine());
        if (!Trim(Game->ScummVMParameters()).isEmpty()) {
            if (!Params.isEmpty()) Params = Params + " ";
            Params = Params + Trim(Game->ScummVMParameters());
        }

        RunAsAdmin = false;
        if (Game->RunAsAdmin() && PrgSetup->OfferRunAsAdmin()) {
            if (ZipRecNr < 0) RunAsAdmin = true; else MessageDlg(LanguageSetup->ProfileMountingZipAdminError(), mtError, mbOK, 0);
        }

        if (Trim(Game->ScummVMPath()).isEmpty()) Dir = ""; else Dir = IncludeTrailingPathDelimiter(MakeAbsPath(Game->ScummVMPath(), PrgSetup->BaseDir()));
        ScummVMHandle = RunScummVM(TempDir() + ScummVMConfFileName, /* {$IFDEF AddDFRPrefixInRunMode}'DFR'+{$ENDIF} */ Game->ScummVMGame(), Params, Game->StartFullscreen(), Dir, S, Game->ScummVMRenderMode(), Game->ScummVMPlatform(), RunAsAdmin);
        if (ZipRecNr >= 0) ZipManager->ActivateRepackCheck(ZipRecNr, ScummVMHandle);
        RunPrgManager->AddCommand(Game, ScummVMHandle);
        // CloseHandle(ScummVMHandle) -> nothing to do for process ids
    } while (false);
    delete St;

    if (PrgSetup->MinimizeOnScummVMStart() && (!AlreadyMinimized)) MinimizedAtScummVMStart = true;
}
