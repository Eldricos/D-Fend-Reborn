// C++ port of HelpTools.pas (implementation section).
//
// THTMLhelpRouter routes the application help requests to the help viewer.
// Port note: the help is a folder of HTML pages (Help/<Language>/) on all
// systems, opened in the default web browser, instead of the compiled .chm
// file shown by HHCTRL.OCX. Context ids (Application.HelpCommand(HELP_CONTEXT,
// ID)) and topic jumps open the matching page.
#include "HelpTools.h"
#include "HelpConsts.h"

#include <QApplication>
#include "Process.h"
#include <QDir>
#include <QHash>
#include <QUrl>
#include <QMainWindow>
#include <QToolTip>
#include <QWidget>

THtmlHelpA HtmlHelpA;
THandle HHCTRL = 0;
THTMLhelpRouter *GLOBAL_HELPROUTER = nullptr;

THelpEvent ApplicationOnHelp;
QString ApplicationHelpFile;

/* {$HINTS off} {$WARNINGS off} */

static bool WinHelp(THandle HelpHandle, const QString &HelpFile, int Command, qintptr Data)
{
    // Port note: WinHelp (.hlp files) is not supported (it does not exist on
    // newer Windows versions and on other systems anyway)
    Q_UNUSED(HelpHandle); Q_UNUSED(HelpFile); Q_UNUSED(Command); Q_UNUSED(Data);
    return false;
}

// Help context ids (HelpConsts.h) -> help pages
// (the [MAP] section of the original English.hhp / German.hhp)
static QString HelpContextPage(qintptr ID)
{
    static const QHash<qintptr, QString> Pages = {
        {ID_Index, "Index"},
        {ID_Introduction, "Introduction"},
        {ID_FAQs, "FAQ"},
        {ID_FirstRunWizard, "FirstRunWizard"},
        {ID_Introduction2, "Introduction2"},
        {ID_Menu, "Menu"},
        {ID_FileExportConf, "FileExportConf"},
        {ID_FileExportProf, "FileExportProf"},
        {ID_FileExportList, "FileExportList"},
        {ID_FileImportZip, "FileImportZip"},
        {ID_FileExportBuildInstallers, "FileExportBuildInstallers"},
        {ID_FileExportBuildZipPackages, "FileExportBuildZipPackages"},
        {ID_FileImportDownload, "FileImportPackage"},
        {ID_FileExportPackageListCreator, "FileExportPackageList"},
        {ID_FileOptions, "FileOptions"},
        {ID_FileOptionsGeneral, "FileOptionsGeneral"},
        {ID_FileOptionsDirectories, "FileOptionsDirectories"},
        {ID_FileOptionsSecurity, "FileOptionsSecurity"},
        {ID_FileOptionsCompressionSettings, "FileOptionsCompressionSettings"},
        {ID_FileOptionsWineSupport, "FileOptionsWineSupport"},
        {ID_FileOptionsUserInterface, "FileOptionsUserInterface"},
        {ID_FileOptionsLanguage, "FileOptionsLanguage"},
        {ID_FileOptionsToolbar, "FileOptionsToolbar"},
        {ID_FileOptionsColumnsInTheGamesList, "FileOptionsColumnsInTheGamesList"},
        {ID_FileOptionsAppearanceList, "FileOptionsAppearanceList"},
        {ID_FileOptionsListInScreenshotMode, "FileOptionsListInScreenshotMode"},
        {ID_FileOptionsAppearanceTree, "FileOptionsAppearanceTree"},
        {ID_FileOptionsAppearanceScreenshots, "FileOptionsAppearanceScreenshots"},
        {ID_FileOptionsProfileEditor, "FileOptionsProfileEditor"},
        {ID_FileOptionsDefaultValues, "FileOptionsDefaultValues"},
        {ID_FileOptionsDOSBox, "FileOptionsDOSBox"},
        {ID_FileOptionsDOSBoxCVSFeatures, "FileOptionsDOSBoxCVSFeatures"},
        {ID_FileOptionsFreeDOS, "FileOptionsFreeDOS"},
        {ID_FileOptionsScummVM, "FileOptionsScummVM"},
        {ID_FileOptionsQBasic, "FileOptionsQBasic"},
        {ID_FileOptionsWaveEncoder, "FileOptionsWaveEncoder"},
        {ID_FileOptionsTextEditor, "FileOptionsTextEditor"},
        {ID_FileOptionsMediaViewers, "FileOptionsMediaViewers"},
        {ID_FileOptionsService, "FileOptionsService"},
        {ID_FileOptionsSearchForUpdates, "FileOptionsSearchForUpdates"},
        {ID_FileOptionsWindows, "FileOptionsWindowsGames"},
        {ID_FileOptionsZipPackers, "FileOptionsZipPackers"},
        {ID_FileOptionsGamesListTranslations, "FileOptionsGamesListTranslations"},
        {ID_FileOptionsGameListInIconMode, "FileOptionsListInIconsMode"},
        {ID_FileOptionsMenubar, "FileOptionsMenubar"},
        {ID_FileOptionsUserInterpreters, "FileOptionsUserInterpreters"},
        {ID_FileOptionsImageScaling, "FileOptionsImageScaling"},
        {ID_FileOptionsMoreEmulator, "FileOptionsOtherEmulators"},
        {ID_FileOptionsAutomaticConfiguration, "FileOptionsAutomaticConfiguration"},
        {ID_FileOptionsDOSBoxGlobal, "FileOptionsDOSBoxGeneral"},
        {ID_FileOptionsDataPrivacy, "FileOptionsDataPrivacy"},
        {ID_FileOptionsResetProfiles, "FileOptionsResetProfiles"},
        {ID_ViewQuickStarter, "ViewQuickStarter"},
        {ID_RunDOSBoxOutputTest, "RunDOSBoxOutputTest"},
        {ID_ProfileWithWizard, "ProfileWithWizard"},
        {ID_ProfileEditClassic, "ProfileEditClassic"},
        {ID_ProfileCopy, "ProfileCopy"},
        {ID_ProfileUninstall, "ProfileUninstall"},
        {ID_ProfileMakeInstallerPackage, "ProfileMakeInstallerPackage"},
        {ID_ProfileMakeZipPackage, "ProfileMakeZipPackage"},
        {ID_ProfileViewDOSBoxConfFile, "ProfileViewDOSBoxConfFile"},
        {ID_ProfileViewScummVMIniFile, "ProfileViewScummVMIniFile"},
        {ID_ProfileOpenProgramFolder, "ProfileOpenProgramFolder"},
        {ID_ProfileOpenDataFolder, "ProfileOpenDataFolder"},
        {ID_ProfileOpenFileInDataFolder, "ProfileOpenFileInDataFolder"},
        {ID_ProfileInternetAddress, "ProfileInternetAddress"},
        {ID_ProfileMarkAsFavorite, "ProfileMarkAsFavorite"},
        {ID_ProfileCreateShortCut, "ProfileCreateShortCut"},
        {ID_ProfileSearchGame, "ProfileSearchGame"},
        {ID_ProfileInstallationSupport, "ProfileAddFromMedia"},
        {ID_ProfileCheating, "ProfileCheating"},
        {ID_FileEdit, "ProfileEdit"},
        {ID_ProfileEditProfile, "ProfileEditProfile"},
        {ID_ProfileEditProgramInformation, "ProfileEditProgramInformation"},
        {ID_ProfileEditProgramDirectories, "ProfileEditProgramDirectories"},
        {ID_ProfileEditDOSBox, "ProfileEditDOSBox"},
        {ID_ProfileEditScummVM, "ProfileEditScummVM"},
        {ID_ProfileEditScummVMGame, "ProfileEditScummVMGame"},
        {ID_ProfileEditCPU, "ProfileEditCPU"},
        {ID_ProfileEditMemory, "ProfileEditMemory"},
        {ID_ProfileEditGraphics, "ProfileEditGraphics"},
        {ID_ProfileEditKeyboard, "ProfileEditKeyboard"},
        {ID_ProfileEditMouse, "ProfileEditMouse"},
        {ID_ProfileEditSound, "ProfileEditSound"},
        {ID_ProfileEditSoundVolume, "ProfileEditSoundVolume"},
        {ID_ProfileEditSoundSoundBlaster, "ProfileEditSoundSoundBlaster"},
        {ID_ProfileEditSoundGUS, "ProfileEditSoundGUS"},
        {ID_ProfileEditSoundMIDI, "ProfileEditSoundMIDI"},
        {ID_ProfileEditJoystick, "ProfileEditJoystick"},
        {ID_ProfileEditDrives, "ProfileEditDrives"},
        {ID_ProfileEditSerialPorts, "ProfileEditSerialPorts"},
        {ID_ProfileEditNetwork, "ProfileEditNetwork"},
        {ID_ProfileEditPrinter, "ProfileEditPrinter"},
        {ID_ProfileEditDOSEnvironment, "ProfileEditDOSEnvironment"},
        {ID_ProfileEditStarting, "ProfileEditStarting"},
        {ID_ProfileEditHelperPrograms, "ProfileEditHelperPrograms"},
        {ID_ProfileEditHardware, "ProfileEditHardware"},
        {ID_ProfileEditAdditionalChecksums, "ProfileEditAdditionalChecksums"},
        {ID_ProfileEditSoundInnova, "ProfileEditSoundInnova"},
        {ID_ProfileEditStaging, "ProfileEditStaging"},
        {ID_ExtrasManageIcons, "ExtrasManageIcons"},
        {ID_ExtrasViewHistory, "ExtrasViewHistory"},
        {ID_ExtrasOpenGamesFolder, "ExtrasOpenGamesFolder"},
        {ID_ExtrasTemplates, "ExtrasTemplates"},
        {ID_ExtrasImagesCreateImageFromFloppy, "ExtrasImagesCreateImageFromFloppy"},
        {ID_ExtrasImagesWriteImageToFloppy, "ExtrasImagesWriteImageToFloppy"},
        {ID_ExtrasImagesCreateImage, "ExtrasImagesCreateImage"},
        {ID_ExtrasImagesCreateImageFromCD, "ExtrasImagesCreateImageFromCD"},
        {ID_ExtrasImagesExtractToFolder, "ExtrasImagesExtractToFolder"},
        {ID_ExtrasImagesCreateFromFolder, "ExtrasImagesCreateFromFolder"},
        {ID_ExtrasScanGamesFolder, "ExtrasScanGamesFolder"},
        {ID_ExtrasEditMultipleProfiles, "ExtrasEditMultipleProfiles"},
        {ID_ExtrasUninstallMultipleGames, "ExtrasUninstallMultipleGames"},
        {ID_ExtrasTransferProfiles, "ExtrasTransferProfiles"},
        {ID_ExtrasCheckProfiles, "ExtrasCheckProfiles"},
        {ID_ExtrasCreateShortcuts, "ExtrasCreateShortcuts"},
        {ID_ExtrasLanguageEditor, "ExtrasLanguageEditor"},
        {ID_ExtrasImageFromProfile, "ExtrasImagesCreateFromProfile"},
        {ID_ExtrasEditCheatDB, "ExtrasEditCheatDB"},
        {ID_ExtrasSearchAddress, "ExtrasSearchAddress"},
        {ID_ExtrasDOSBoxLanguageEditor, "ExtrasLanguageEditorDOSBox"},
        {ID_ExtrasMissingGames, "ExtrasMissingGames"},
        {ID_HelpForum, "HelpForum"},
        {ID_HelpStatistics, "HelpStatistics"},
        {ID_HelpOperationMode, "HelpOperationMode"},
        {ID_HelpSearchForUpdates, "HelpSearchForUpdates"},
        {ID_HelpOldGames, "HelpOldGames"},
        {ID_HelpGetGames, "HelpGetGames"},
    };
    return Pages.value(ID);
}

// Opens a page of the help folder in the web browser.
// Page: "FAQ", "FAQ.htm", "FAQ.html" or "FAQ.html#Anchor" (empty: contents page)
static THandle OpenHelpPage(const QString &HelpFolder, const QString &Page)
{
    if (HelpFolder.isEmpty() || !DirectoryExists(HelpFolder)) return 0;
    const QString Folder = IncludeTrailingPathDelimiter(HelpFolder);
    QString Name = Page;
    while (Name.startsWith('/') || Name.startsWith('\\')) Name = Name.mid(1);
    QString Anchor;
    const int P = Name.indexOf('#');
    if (P >= 0) { Anchor = Name.mid(P + 1); Name = Name.left(P); }
    if (!Name.isEmpty() && ExtractFileExt(Name).isEmpty()) Name += ".html";
    if (!Name.isEmpty() && !FileExists(Folder + Name) && FileExists(Folder + ChangeFileExt(Name, ".html"))) Name = ChangeFileExt(Name, ".html");
    if (Name.isEmpty() || !FileExists(Folder + Name)) Name = "Index.html";
    if (!FileExists(Folder + Name)) return 0;
    QUrl URL = QUrl::fromLocalFile(QDir::fromNativeSeparators(Folder + Name));
    if (!Anchor.isEmpty()) URL.setFragment(Anchor);
    return OpenDesktopUrl(URL) ? 1 : 0;
}

// HtmlHelp emulation for the HTML help folders (Help/<Language>/*.html),
// used on all systems.
// Port note: the original program used compiled HTML help files (.chm) and
// HtmlHelpA from HHCTRL.OCX (Windows only). D-Fend Reborn ships the help as
// plain HTML pages which are opened in the default web browser; pszFile is
// the help folder, "<folder>::/<page>" addresses a page.
static THandle HtmlFolderHelp(THandle hwndCaller, const QString &pszFile, quint32 uCommand, qintptr dwData)
{
    Q_UNUSED(hwndCaller);
    QString Folder = pszFile, Page;
    const int P = Folder.indexOf("::");
    if (P >= 0) { Page = Folder.mid(P + 2); Folder = Folder.left(P); }
    switch (uCommand) {
        case HH_CLOSE_ALL:
            return 1;
        case HH_DISPLAY_TEXT_POPUP: {
            THH_POPUP *HP = reinterpret_cast<THH_POPUP *>(dwData);
            if (HP == nullptr) return 0;
            QToolTip::showText(HP->pt, HP->pszText);
            return 1;
        }
        case HH_KEYWORD_LOOKUP:
        case HH_ALINK_LOOKUP:
            // there is no keyword index; the preceding HH_DISPLAY_TOPIC call
            // has already opened the contents page
            return 1;
        case HH_HELP_CONTEXT:
            return OpenHelpPage(Folder, HelpContextPage(dwData));
        case HH_DISPLAY_TOPIC:
            return OpenHelpPage(Folder, Page);
        default:
            // HH_DISPLAY_TOC, HH_DISPLAY_INDEX, HH_DISPLAY_SEARCH: contents page
            return OpenHelpPage(Folder, QString());
    }
}

bool LoadHHCTRL()
{
    if (HHCTRL == 0) {
        HHCTRL = 1;
        HtmlHelpA = HtmlFolderHelp;
    }
    return HHCTRL != 0;
}

static bool CheckRouterInstance()
{
    if (GLOBAL_HELPROUTER != nullptr) throw Exception("Multiple instances of THTMLhelpRouter are not allowed");
    return true;
}

static THandle WidgetHandle(QWidget *W)
{
    if (W == nullptr) return 0;
    return static_cast<THandle>(W->winId());
}

/* --- THTMLhelpRouter --- */

THTMLhelpRouter::THTMLhelpRouter(QWidget *AOwner) : QObject(AOwner)
{
    fHelpType = htAuto;
    if (CheckRouterInstance()) {
        fAppOnHelp = ApplicationOnHelp;
        ApplicationOnHelp = [this](quint16 Command, qintptr Data, bool &CallHelp) { return OnRouteHelp(Command, Data, CallHelp); };
        GLOBAL_HELPROUTER = this;
    }
    fShowType = stDefault;
}

THTMLhelpRouter::~THTMLhelpRouter()
{
    if (fAppOnHelp) ApplicationOnHelp = fAppOnHelp; else ApplicationOnHelp = nullptr;
    GLOBAL_HELPROUTER = nullptr;
}

QWidget *THTMLhelpRouter::CurrentForm()
{
    if (QApplication::activeWindow() != nullptr)
        return QApplication::activeWindow();
    else
        return qobject_cast<QWidget *>(parent());
}

void THTMLhelpRouter::SetHelpType(THelpType value)
{
    if (value != fHelpType) {
        fHelpType = value;
        if (fHelpType == htAuto || fHelpType == htHTMLhelp) LoadHHCTRL();
    }
}

bool THTMLhelpRouter::HTMLhelpInstalled()
{
    if (HHCTRL == 0) LoadHHCTRL();
    return static_cast<bool>(HtmlHelpA);
}

bool THTMLhelpRouter::FindHandle(WId &Helphandle, QString &Hfile)
{
    bool result = false;
    switch (HelpType()) {
        case htWinhelp: result = false; break;
        case htHTMLhelp: result = true; break;
        case htAuto: result = HTMLhelpInstalled(); break;
    }
    QWidget *AppWindow = qobject_cast<QWidget *>(parent());   // Application.Handle
    Hfile = ApplicationHelpFile;
    Helphandle = static_cast<WId>(WidgetHandle(AppWindow));
    /* {IFDEF VER100} */
    QWidget *CForm = CurrentForm();
    if ((CForm != nullptr) && (CForm->property("HelpFile").toString() != "")) {
        Helphandle = CForm->winId();
        Hfile = CForm->property("HelpFile").toString();
    }
    /* {ENDIF} */
    if (fHelpfile != "") {
        Hfile = fHelpfile;
        Helphandle = static_cast<WId>(WidgetHandle(AppWindow));
    }
    return result;
}

bool THTMLhelpRouter::OnRouteHelp(quint16 Command, qintptr Data, bool &CallHelp)
{
    bool showHTML;
    THandle rHandle;
    WId HelpHandle = 0;
    QString HFile;

    bool result = false;

    if (fOnHelp) result = fOnHelp(Command, Data, CallHelp);
    if (!CallHelp) return result;
    if (fAppOnHelp) result = fAppOnHelp(Command, Data, CallHelp);
    if (!CallHelp) return result;

    showHTML = FindHandle(HelpHandle, HFile);
    if (showHTML && !HtmlHelpA) LoadHHCTRL();

    result = false;
    if (showHTML && HtmlHelpA) {
        THandle H = static_cast<THandle>(HelpHandle);
        rHandle = 0;
        switch (Command) {
            case HELP_FINDER: case HELP_CONTENTS: rHandle = HtmlHelpA(H, HFile, HH_DISPLAY_TOC, 0); break;  /* show table of contents */
            case HELP_KEY: rHandle = HtmlHelpA(H, HFile, HH_DISPLAY_INDEX, Data); break;  /* display keywordshow table of contens */
            case HELP_QUIT: rHandle = HtmlHelpA(H, QString(), HH_CLOSE_ALL, 0); break;
            case HELP_CONTEXT: rHandle = HtmlHelpA(H, HFile, HH_HELP_CONTEXT, Data); break;  /* display help context */
            case HELP_CONTEXTPOPUP: rHandle = HtmlHelpA(H, HFile, HH_HELP_CONTEXT, Data); break;
            default: break;
        }
        result = rHandle != 0;
    }
    if ((!result) && (fHelpType != htHTMLhelp)) {
        switch (Command) {
            case HELP_SETPOPUP_POS: if (fShowType == stMain) Command = 0; break;
            case HELP_CONTEXTPOPUP: if (fShowType == stMain) Command = HELP_CONTEXT; break; /* no popup */
            default: break;
        }
        if (Command != 0) result = WinHelp(static_cast<THandle>(HelpHandle), ChangeFileExt(HFile, ".hlp"), Command, Data);
        else result = true;
    }
    CallHelp = false;
    return result;
}

bool THTMLhelpRouter::HelpContent()
{
    return ApplicationHelpCommand(HELP_FINDER, 0);
}

bool THTMLhelpRouter::HelpJump(const QString &hfile, const QString &topicid)
{
    bool showHTML;
    THandle rHandle;
    WId HelpHandle = 0;
    QString HF, HID, HFileLocal;

    bool result = false;
    showHTML = FindHandle(HelpHandle, HF);

    if (hfile != "") {
        HF = hfile;
        HelpHandle = 0;
    }
    if (showHTML && !HtmlHelpA) LoadHHCTRL();
    if (showHTML && HtmlHelpA) {
        /* rHandle := 0; */
        HFileLocal = HF;   // (the help folder)
        HID = topicid;
        if (Copy(LowerCase(ExtractFileExt(HID)), 1, 4) != ".htm") HID = HID + ".html";
        rHandle = HtmlHelpA(static_cast<THandle>(HelpHandle), HFileLocal + "::/" + HID, HH_DISPLAY_TOPIC, 0);  /* show table of contents */
        result = rHandle != 0;
    }
    if ((!result) && (fHelpType != htHTMLhelp)) {
        HFileLocal = ChangeFileExt(HF, ".hlp");
        QString Command = Copy(Format("JumpID(\"\",\"%s\")", {topicid}), 1, 255);
        result = WinHelp(static_cast<THandle>(HelpHandle), HFileLocal, HELP_CONTENTS, 0);
        if (result) result = WinHelp(static_cast<THandle>(HelpHandle), HFileLocal, HELP_COMMAND, reinterpret_cast<qintptr>(Command.utf16()));
    }
    return result;
}

bool THTMLhelpRouter::HelpPopup(int X, int Y, const QString &text)
{
    if ((fHelpType != htWinhelp) && HTMLhelpInstalled()) {
        QWidget *CForm = CurrentForm();
        QPoint pt(X, Y);
        if (CForm != nullptr) pt = CForm->mapToGlobal(pt);   // ClientToScreen(CForm.handle, pt)
        THH_POPUP HP;
        HP.cbStruct = sizeof(HP);
        HP.hinst = 0;
        HP.idString = 0;
        HP.pszText = text;
        HP.pt = pt;
        HP.clrForeground = -1;
        HP.clrBackground = -1;
        HP.rcMargins = QRect(QPoint(-1, -1), QPoint(-1, -1));
        HP.pszFont = "MS Sans Serif, 10";
        return HtmlHelpA(WidgetHandle(CForm), QString(), HH_DISPLAY_TEXT_POPUP, reinterpret_cast<qintptr>(&HP)) != 0;
    } else {
        return false;
    }
}

bool THTMLhelpRouter::HelpKeyword(const QString &keyword)
{
    QString Command = Copy(keyword, 1, 255);   // StrLcopy(Command, pchar(keyword), SizeOf(Command) - 1)
    return ApplicationHelpCommand(HELP_KEY, reinterpret_cast<qintptr>(Command.utf16()));
}

// Shared implementation of HelpKLink / HelpALink
static bool HelpAKLink(THTMLhelpRouter *Router, bool showHTML, WId HelpHandle, const QString &HF, const QString &keyword,
                       quint32 LookupCommand, THelpType fHelpType, const QString &WinHelpMacro)
{
    THandle rHandle;
    QString Hfile;
    bool result = false;
    Q_UNUSED(Router);

    if (showHTML && !HtmlHelpA) LoadHHCTRL();
    if (showHTML && HtmlHelpA) {
        /* rHandle := 0; */
        Hfile = HF;   // (the help folder)

        THH_AKLINK HA;
        HA.cbStruct = sizeof(HA);
        HA.fReserved = false;
        HA.pszKeywords = keyword;
        HA.pszUrl = "";
        HA.pszMsgText = "";
        HA.pszMsgTitle = "";
        HA.pszMsgWindow = "";
        HA.fIndexOnFail = true;
        rHandle = HtmlHelpA(static_cast<THandle>(HelpHandle), Hfile, HH_DISPLAY_TOPIC, 0);  /* create window */
        if (rHandle != 0) rHandle = HtmlHelpA(static_cast<THandle>(HelpHandle), Hfile, LookupCommand, reinterpret_cast<qintptr>(&HA));
        result = rHandle != 0;
    }
    if ((!result) && (fHelpType != htHTMLhelp)) {
        // HelpALink uses the .hlp file; HelpKLink in the Delphi code assigns the .hlp name to
        // the Helpfile property instead and uses the (.chm) Hfile variable - kept as in the original.
        if (LookupCommand == HH_ALINK_LOOKUP) Hfile = ChangeFileExt(HF, ".hlp");
        QString Command = Copy(Format(WinHelpMacro, {keyword}), 1, 255);
        result = WinHelp(static_cast<THandle>(HelpHandle), Hfile, HELP_CONTENTS, 0);
        if (result) result = WinHelp(static_cast<THandle>(HelpHandle), Hfile, HELP_COMMAND, reinterpret_cast<qintptr>(Command.utf16()));
    }
    return result;
}

bool THTMLhelpRouter::HelpKLink(const QString &keyword)
{
    WId HelpHandle = 0;
    QString HF;
    bool showHTML = FindHandle(HelpHandle, HF);
    bool result = HelpAKLink(this, showHTML, HelpHandle, HF, keyword, HH_KEYWORD_LOOKUP, fHelpType, "KL(\"%s\",1)");
    if ((!result) && (fHelpType != htHTMLhelp)) fHelpfile = ChangeFileExt(HF, ".hlp");   // helpfile := changefileext(HF,'.hlp')
    return result;
}

bool THTMLhelpRouter::HelpALink(const QString &akeyword)
{
    WId HelpHandle = 0;
    QString HF;
    bool showHTML = FindHandle(HelpHandle, HF);
    return HelpAKLink(this, showHTML, HelpHandle, HF, akeyword, HH_ALINK_LOOKUP, fHelpType, "AL(\"%s\",1)");
}

QString THTMLhelpRouter::GetVersion()
{
    return cVersion;
}

void THTMLhelpRouter::SetVersion(const QString &dummy)
{
    Q_UNUSED(dummy);
    /* do nothing */
}

void Register()
{
    // RegisterComponents('EC', [THTMLhelpRouter]) - design time only
}

/* {$HINTS on} {$WARNINGS on} */

THTMLhelpRouter *InitHTMLHelp(const QString &HelpFileName)
{
    // Owner: Application.MainForm
    QWidget *MainForm = nullptr;
    for (QWidget *W : QApplication::topLevelWidgets()) if (qobject_cast<QMainWindow *>(W) != nullptr) { MainForm = W; break; }
    if (MainForm == nullptr) MainForm = QApplication::activeWindow();
    THTMLhelpRouter *result = new THTMLhelpRouter(MainForm);
    result->SetHelpfile(HelpFileName);
    ApplicationHelpFile = "";
    return result;
}

// TApplication.HelpCommand
bool ApplicationHelpCommand(int Command, qintptr Data)
{
    bool CallHelp = true;
    bool result = false;
    if (ApplicationOnHelp) result = ApplicationOnHelp(static_cast<quint16>(Command), Data, CallHelp);
    if (CallHelp) {
        // VCL default: WinHelp with Application.HelpFile
        QWidget *W = QApplication::activeWindow();
        if (ApplicationHelpFile != "") result = WinHelp(WidgetHandle(W), ApplicationHelpFile, Command, Data); else result = false;
    }
    return result;
}

void HelpTools_Initialization()
{
    HHCTRL = 0;
}

void HelpTools_Finalization()
{
    if (HHCTRL != 0) {
        HtmlHelpA = nullptr;
        HHCTRL = 0;
    }
}
