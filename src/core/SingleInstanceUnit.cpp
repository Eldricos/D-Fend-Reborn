// C++ port of SingleInstanceUnit.pas (implementation section).
//
// The Delphi version uses a named Win32 event ("DFR-running"): a second instance
// opens it, signals it (the first instance polls it and brings its window to
// the front) and terminates.  The port emulates the named event with a
// QLocalServer / QLocalSocket pair (works on all platforms).
#include "SingleInstanceUnit.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

static QLocalServer *EventServer = nullptr;
static int EventSignalCount = 0;

static QString EventServerName()
{
    const QString EventName = "D-Fend-Reborn-running";
    // Win32 events live in the session namespace, local sockets are global on
    // Unix systems: make the name user specific.
    QString User = qEnvironmentVariable("USER");
    if (User.isEmpty()) User = qEnvironmentVariable("USERNAME");
    if (User.isEmpty()) return EventName;
    return EventName + "-" + User;
}

bool InstanceRunning(THandle &hEvent)
{
    bool result = false;
    try {
        // hEvent:=OpenEvent(EVENT_ALL_ACCESS,False,EventName);
        QLocalSocket Socket;
        Socket.connectToServer(EventServerName());
        if (Socket.waitForConnected(1000)) {
            // SetEvent(hEvent)
            Socket.write("1");
            Socket.waitForBytesWritten(1000);
            Socket.disconnectFromServer();
            if (Socket.state() != QLocalSocket::UnconnectedState) Socket.waitForDisconnected(1000);
            hEvent = 0;
            result = true;
            QCoreApplication::quit();   // Application.Terminate
            return result;
        }
        // hEvent:=CreateEvent(nil,False,False,EventName);
        if (EventServer == nullptr) {
            QLocalServer::removeServer(EventServerName());   // stale socket file after a crash (Unix)
            EventServer = new QLocalServer();
            EventServer->setSocketOptions(QLocalServer::UserAccessOption);
            QObject::connect(EventServer, &QLocalServer::newConnection, []() {
                while (EventServer != nullptr && EventServer->hasPendingConnections()) {
                    QLocalSocket *S = EventServer->nextPendingConnection();
                    EventSignalCount++;
                    QObject::connect(S, &QLocalSocket::disconnected, S, &QObject::deleteLater);
                    S->disconnectFromServer();
                }
            });
            if (!EventServer->listen(EventServerName())) {
                delete EventServer;
                EventServer = nullptr;
                hEvent = 0;
                return result;
            }
        }
        EventSignalCount = 0;
        hEvent = 1;
    } catch (...) {}
    return result;
}

bool InstanceEventSignaled(THandle hEvent)
{
    if (hEvent == 0 || EventServer == nullptr) return false;
    if (EventSignalCount <= 0) return false;
    EventSignalCount = 0;   // auto reset event
    return true;
}

void CloseInstanceEvent(THandle &hEvent)
{
    if (EventServer != nullptr) {
        EventServer->close();
        delete EventServer;
        EventServer = nullptr;
    }
    EventSignalCount = 0;
    hEvent = 0;
}
