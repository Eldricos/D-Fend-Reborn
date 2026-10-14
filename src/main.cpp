// D-Fend Reborn - C++/Qt port of D-Fend Reloaded.
//
// Replaces the Delphi project file (DFend.dpr): creates the application,
// runs the unit initialization sections in the order Delphi would run them,
// creates the main form and runs the event loop.
#include <QApplication>
#include <QNetworkProxyFactory>
#include <QSslConfiguration>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>
#include <QElapsedTimer>

#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "LanguageSetupUnit.h"
#include "LoggingUnit.h"
#include "DOSBoxCountUnit.h"
#include "GameDBUnit.h"
#include "HistoryUnit.h"
#include "IconLoaderUnit.h"
#include "ImageCacheUnit.h"
#include "ScreenshotsCacheUnit.h"
#include "ListViewImageUnit.h"
#include "RunPrgManagerUnit.h"
#include "ScreensaverControlUnit.h"
#include "ZipManagerUnit.h"
#include "CheatDBUnit.h"
#include "ProgramUpdateCheckUnit.h"
#include "PackageManagerToolsUnit.h"
#include "TextEditPopupUnit.h"
#include "HelpTools.h"
#include "MainUnit.h"

int main(int argc, char *argv[])
{
    QApplication App(argc, argv);
    App.setApplicationName("D-Fend Reborn");
    App.setApplicationDisplayName("D-Fend Reborn");
    App.setOrganizationName("D-Fend Reborn");
    App.setApplicationVersion(DFR_VERSION);
    App.setWindowIcon(QIcon(":/icons/MainUnit_DFendRebornMainForm.ico"));

    // VCL default font: Tahoma 8pt (Font.Height=-11 at 96 dpi)
    QFont F;
    F.setFamilies({"Tahoma", "Segoe UI", "DejaVu Sans", "Liberation Sans"});
    F.setPixelSize(11);
    App.setFont(F);
#ifdef Q_OS_WIN
    if (QStyleFactory::keys().contains("windowsvista", Qt::CaseInsensitive)) App.setStyle("windowsvista");
#else
    App.setStyle("Fusion");
#endif

    // D-Fend Reborn: all downloads (package lists and packages, update check,
    // data reader, cheats database) use the proxy settings of the system
    // (Windows: Internet options; Linux: http_proxy / https_proxy / no_proxy
    // and, if Qt supports it, the desktop settings)
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    // Certificates named by SSL_CERT_FILE (the OpenSSL convention, e.g. the
    // certificate of a company proxy) are trusted in addition to the system
    // certificates (Qt itself only reads the system certificate store)
    {
        const QString CertFile = qEnvironmentVariable("SSL_CERT_FILE");
        if (!CertFile.isEmpty() && QFileInfo::exists(CertFile)) {
            QSslConfiguration SSL = QSslConfiguration::defaultConfiguration();
            SSL.addCaCertificates(CertFile);
            QSslConfiguration::setDefaultConfiguration(SSL);
        }
    }

    // Delphi unit initialization sections (dependency order)
    PrgConsts_Initialization();
    LoggingUnit_Initialization();
    PrgSetupUnit_Initialization();
    LanguageSetupUnit_Initialization();
    DOSBoxCountUnit_Initialization();
    GameDBUnit_Initialization();
    HistoryUnit_Initialization();
    IconLoaderUnit_Initialization();
    ImageCacheUnit_Initialization();
    ScreenshotsCacheUnit_Initialization();
    ListViewImageUnit_Initialization();
    RunPrgManagerUnit_Initialization();
    ScreensaverControlUnit_Initialization();
    ZipManagerUnit_Initialization();
    CheatDBUnit_Initialization();
    ProgramUpdateCheckUnit_Initialization();
    PackageManagerToolsUnit_Initialization();
    TextEditPopupUnit_Initialization();
    HelpTools_Initialization();

    int Result;
    {
        auto *MainForm = new TDFendRebornMainForm();
        if (MainForm->FormCreateAborted()) {
            Result = 0;
        } else {
            if (TraceEnabled()) {
                // Reports when the event loop was blocked ("Not responding")
                auto *Watchdog = new QTimer(MainForm);
                auto *Clock = new QElapsedTimer();
                Clock->start();
                QObject::connect(Watchdog, &QTimer::timeout, [Clock]() {
                    qint64 D = Clock->restart();
                    if (D > 400) Trace(QString("EVENT LOOP BLOCKED for about %1 ms").arg(D - 100));
                });
                Watchdog->start(100);
            }
            MainForm->show();
            Result = App.exec();
        }
        delete MainForm;
    }

    // finalization sections (reverse order)
    HelpTools_Finalization();
    TextEditPopupUnit_Finalization();
    PackageManagerToolsUnit_Finalization();
    ProgramUpdateCheckUnit_Finalization();
    ZipManagerUnit_Finalization();
    ScreensaverControlUnit_Finalization();
    RunPrgManagerUnit_Finalization();
    ListViewImageUnit_Finalization();
    ScreenshotsCacheUnit_Finalization();
    ImageCacheUnit_Finalization();
    IconLoaderUnit_Finalization();
    HistoryUnit_Finalization();
    GameDBUnit_Finalization();
    DOSBoxCountUnit_Finalization();
    LanguageSetupUnit_Finalization();
    PrgSetupUnit_Finalization();
    return Result;
}
