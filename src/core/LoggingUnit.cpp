// C++ port of LoggingUnit.pas (implementation section).
#include "LoggingUnit.h"
#include "CommonTools.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QElapsedTimer>

static QString StartupLogFile = "";

static QElapsedTimer &TraceClock()
{
    static QElapsedTimer T;
    if (!T.isValid()) T.start();
    return T;
}

bool TraceEnabled()
{
    static const bool Enabled = qEnvironmentVariableIsSet("DFEND_TRACE");
    return Enabled;
}

void Trace(const QString &Info)
{
    if (!TraceEnabled()) return;
    qWarning("[%8lld ms] %s", static_cast<long long>(TraceClock().elapsed()), qPrintable(Info));
}

TTraceScope::TTraceScope(const QString &What, int MinMs) : FWhat(What), FMinMs(MinMs), FStart(TraceEnabled() ? TraceClock().elapsed() : 0)
{
}

TTraceScope::~TTraceScope()
{
    if (!TraceEnabled()) return;
    qint64 D = TraceClock().elapsed() - FStart;
    if (D >= FMinMs) Trace(QString("%1 took %2 ms").arg(FWhat).arg(D));
}

void LogInfo(const QString &Info, bool ContinueLogging)
{
    Trace(Info);
    if (StartupLogFile == "") return;
    if (!FileExists(StartupLogFile)) { StartupLogFile = ""; return; }

    QFile F(StartupLogFile);
    if (F.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream T(&F);
        T << TimeToStr(Now()) + " " + Info << "\n";
        F.close();
    }
    if (!ContinueLogging) StartupLogFile = "";
}

void LoggingUnit_Initialization()
{
#ifdef Q_OS_WIN
    StartupLogFile = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_DESKTOPDIRECTORY)) + "D-Fend-Reborn-Log.txt";
#else
    StartupLogFile = IncludeTrailingPathDelimiter(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)) + "D-Fend-Reborn-Log.txt";
#endif
    if (!FileExists(StartupLogFile)) StartupLogFile = "";
}
