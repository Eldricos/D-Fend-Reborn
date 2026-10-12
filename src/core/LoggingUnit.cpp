// C++ port of LoggingUnit.pas (implementation section).
#include "LoggingUnit.h"
#include "CommonTools.h"

#ifdef Q_OS_WIN
#include "WinApi.h"
#endif

#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

static QString StartupLogFile = "";

void LogInfo(const QString &Info, bool ContinueLogging)
{
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
    StartupLogFile = IncludeTrailingPathDelimiter(GetSpecialFolder(0, CSIDL_DESKTOPDIRECTORY)) + "D-Fend-Reloaded-Log.txt";
#else
    StartupLogFile = IncludeTrailingPathDelimiter(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)) + "D-Fend-Reloaded-Log.txt";
#endif
    if (!FileExists(StartupLogFile)) StartupLogFile = "";
}
