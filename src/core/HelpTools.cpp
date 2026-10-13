// C++ port of HelpTools.pas (implementation section).
//
// THTMLhelpRouter routes the application help requests to the HTML help
// (CHM) viewer.  On Windows HtmlHelpW from HHCTRL.OCX is loaded dynamically
// exactly like the Delphi code did (HtmlHelpA).  On other systems a portable
// emulation of HtmlHelp is installed which opens the .chm file with the
// associated viewer (topics / context ids cannot be addressed there).
#include "HelpTools.h"

#include <QApplication>
#include <QLibrary>
#include <QMainWindow>
#include <QToolTip>
#include <QWidget>

THtmlHelpA HtmlHelpA;
THandle HHCTRL = 0;
THTMLhelpRouter *GLOBAL_HELPROUTER = nullptr;

THelpEvent ApplicationOnHelp;
QString ApplicationHelpFile;

static QLibrary *HHCTRLLibrary = nullptr;

/* {$HINTS off} {$WARNINGS off} */

#ifdef Q_OS_WIN
// Native structures (htmlhelp.h, unicode versions)
struct NATIVE_HH_POPUP {
    int cbStruct;
    HINSTANCE hinst;
    UINT idString;
    LPCWSTR pszText;
    POINT pt;
    COLORREF clrForeground;
    COLORREF clrBackground;
    RECT rcMargins;
    LPCWSTR pszFont;
};
struct NATIVE_HH_AKLINK {
    int cbStruct;
    BOOL fReserved;
    LPCWSTR pszKeywords;
    LPCWSTR pszUrl;
    LPCWSTR pszMsgText;
    LPCWSTR pszMsgTitle;
    LPCWSTR pszWindow;
    BOOL fIndexOnFail;
};
typedef HWND(WINAPI *THtmlHelpWProc)(HWND hwndCaller, LPCWSTR pszFile, UINT uCommand, DWORD_PTR dwData);
typedef BOOL(WINAPI *TWinHelpWProc)(HWND hWndMain, LPCWSTR lpszHelp, UINT uCommand, ULONG_PTR dwData);

static THtmlHelpWProc HtmlHelpWProc = nullptr;

static bool WinHelp(THandle HelpHandle, const QString &HelpFile, int Command, qintptr Data)
{
    // WinHelp is resolved at run time (not available on newer Windows versions anyway)
    HMODULE User32 = GetModuleHandleW(L"user32.dll");
    if (User32 == nullptr) return false;
    TWinHelpWProc Proc = reinterpret_cast<TWinHelpWProc>(reinterpret_cast<void *>(GetProcAddress(User32, "WinHelpW")));
    if (Proc == nullptr) return false;
    return Proc(reinterpret_cast<HWND>(HelpHandle), reinterpret_cast<LPCWSTR>(HelpFile.utf16()), static_cast<UINT>(Command), static_cast<ULONG_PTR>(Data)) != FALSE;
}
#else
static bool WinHelp(THandle HelpHandle, const QString &HelpFile, int Command, qintptr Data)
{
    // Port note: WinHelp (.hlp files) does not exist on this platform
    Q_UNUSED(HelpHandle); Q_UNUSED(HelpFile); Q_UNUSED(Command); Q_UNUSED(Data);
    return false;
}

// Portable HtmlHelp emulation
static THandle PortableHtmlHelp(THandle hwndCaller, const QString &pszFile, quint32 uCommand, qintptr dwData)
{
    Q_UNUSED(hwndCaller);
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
            // The help file has already been opened by the preceding HH_DISPLAY_TOPIC call
            return 1;
        default: {
            QString F = pszFile;
            int P = F.indexOf("::");
            if (P >= 0) F = F.left(P);
            if (F == "" || !FileExists(F)) return 0;
            // TODO(port): a generic CHM viewer cannot be told to show a topic / context id
            return ShellOpen(F) ? 1 : 0;
        }
    }
}
#endif

bool LoadHHCTRL()
{
    if (HHCTRL == 0) {
        HtmlHelpA = nullptr;
#ifdef Q_OS_WIN
        if (HHCTRLLibrary == nullptr) HHCTRLLibrary = new QLibrary("HHCTRL.OCX");
        if (HHCTRLLibrary->load()) {
            HHCTRL = 1;
            HtmlHelpWProc = reinterpret_cast<THtmlHelpWProc>(HHCTRLLibrary->resolve("HtmlHelpW"));
            if (HtmlHelpWProc != nullptr) {
                HtmlHelpA = [](THandle hwndCaller, const QString &pszFile, quint32 uCommand, qintptr dwData) -> THandle {
                    LPCWSTR F = pszFile.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(pszFile.utf16());
                    HWND H = HtmlHelpWProc(reinterpret_cast<HWND>(hwndCaller), F, static_cast<UINT>(uCommand), static_cast<DWORD_PTR>(dwData));
                    return static_cast<THandle>(reinterpret_cast<qintptr>(H));
                };
            }
        }
#else
        HHCTRL = 1;
        HtmlHelpA = PortableHtmlHelp;
#endif
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
        HFile = ChangeFileExt(HFile, ".chm");
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
        HFileLocal = ChangeFileExt(HF, ".chm");
        HID = topicid;
        if (Copy(LowerCase(ExtractFileExt(HID)), 1, 4) != ".htm") HID = HID + ".htm";
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
#ifdef Q_OS_WIN
        const QString Font = "MS Sans Serif, 10";
        NATIVE_HH_POPUP HP;
        HP.cbStruct = sizeof(HP);
        HP.hinst = nullptr;
        HP.idString = 0;
        HP.pszText = reinterpret_cast<LPCWSTR>(text.utf16());
        HP.pt.x = pt.x();
        HP.pt.y = pt.y();
        HP.clrForeground = static_cast<COLORREF>(-1);
        HP.clrBackground = static_cast<COLORREF>(-1);
        HP.rcMargins.left = -1;
        HP.rcMargins.right = -1;
        HP.rcMargins.top = -1;
        HP.rcMargins.bottom = -1;
        HP.pszFont = reinterpret_cast<LPCWSTR>(Font.utf16());
        return HtmlHelpA(WidgetHandle(CForm), QString(), HH_DISPLAY_TEXT_POPUP, reinterpret_cast<qintptr>(&HP)) != 0;
#else
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
#endif
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
        Hfile = ChangeFileExt(HF, ".chm");

#ifdef Q_OS_WIN
        const QString Empty = "";
        NATIVE_HH_AKLINK HA;
        HA.cbStruct = sizeof(HA);
        HA.fReserved = FALSE;
        HA.pszKeywords = reinterpret_cast<LPCWSTR>(keyword.utf16());
        HA.pszUrl = reinterpret_cast<LPCWSTR>(Empty.utf16());
        HA.pszMsgText = reinterpret_cast<LPCWSTR>(Empty.utf16());
        HA.pszMsgTitle = reinterpret_cast<LPCWSTR>(Empty.utf16());
        HA.pszWindow = reinterpret_cast<LPCWSTR>(Empty.utf16());
        HA.fIndexOnFail = TRUE;
#else
        THH_AKLINK HA;
        HA.cbStruct = sizeof(HA);
        HA.fReserved = false;
        HA.pszKeywords = keyword;
        HA.pszUrl = "";
        HA.pszMsgText = "";
        HA.pszMsgTitle = "";
        HA.pszMsgWindow = "";
        HA.fIndexOnFail = true;
#endif
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
#ifdef Q_OS_WIN
        HtmlHelpWProc = nullptr;
        if (HHCTRLLibrary != nullptr) HHCTRLLibrary->unload();
#endif
        HHCTRL = 0;
    }
    delete HHCTRLLibrary;
    HHCTRLLibrary = nullptr;
}
