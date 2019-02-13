/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ShellApplication.h"

#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLockFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QtNetwork/QLocalSocket>

#include "autotype/AutoType.h"
#include "core/Global.h"

#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
#include "core/OSEventFilter.h"
#endif

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "cli/TextStream.h"
#include "cli/Utils.h"

namespace
{
    constexpr int WaitTimeoutMSec = 150;
    const char BlockSizeProperty[] = "blockSize";
} // namespace

ShellApplication::ShellApplication(int& argc, char** argv)
    : QCoreApplication(argc, argv)
#ifdef Q_OS_UNIX
    , m_unixSignalNotifier(nullptr)
#endif
    , m_alreadyRunning(false)
    , m_lockFile(nullptr)
#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    , m_osEventFilter(new OSEventFilter())
{
    installNativeEventFilter(m_osEventFilter.data());
#else
{
#endif
#if defined(Q_OS_UNIX)
    registerUnixSignals();
#endif

    QString identifier = this->getIdentifier();
    QString lockName = identifier + ".lock";
    m_socketName = identifier + ".socket";
    qDebug("Using identifier %s.", qPrintable(identifier));

    // According to documentation we should use RuntimeLocation on *nixes, but even Qt doesn't respect
    // this and creates sockets in TempLocation, so let's be consistent.
    m_lockFile = new QLockFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + lockName);
    m_lockFile->setStaleLockTime(0);
    m_lockFile->tryLock();

    m_lockServer.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_lockServer, SIGNAL(newConnection()), this, SIGNAL(anotherInstanceStarted()));
    connect(&m_lockServer, SIGNAL(newConnection()), this, SLOT(processIncomingConnection()));

    switch (m_lockFile->error()) {
    case QLockFile::NoError:
        // No existing lock was found, start listener
        m_alreadyRunning = true;
        m_lockServer.listen(m_socketName);
        break;
    case QLockFile::LockFailedError: {
        // Attempt to connect to the existing instance
        QLocalSocket client;
        for (int i = 0; i < 3; ++i) {
            client.connectToServer(m_socketName);
            if (client.waitForConnected(WaitTimeoutMSec)) {
                // Connection succeeded, this will raise the existing window if minimized
                client.abort();
                break;
            }
        }

        if (!m_alreadyRunning) {
            // If we get here then the original instance is likely dead
            qWarning() << QCoreApplication::translate(
                              "Main", "Existing single-instance lock file is invalid. Launching new instance.")
                              .toUtf8()
                              .constData();

            // forceably reset the lock file
            m_lockFile->removeStaleLockFile();
            m_lockFile->tryLock();
            // start the listen server
            m_lockServer.listen(m_socketName);
        }
        break;
    }
    default:
        qWarning() << QCoreApplication::translate("Main",
                                                  "The lock file could not be created. Single-instance mode disabled.")
                          .toUtf8()
                          .constData();
    }
}

ShellApplication::~ShellApplication()
{
    m_lockServer.close();
    if (m_lockFile) {
        m_lockFile->unlock();
        delete m_lockFile;
    }
}

void ShellApplication::preserveLock()
{
    m_lockFile = nullptr;
}

#if defined(Q_OS_UNIX)
int ShellApplication::unixSignalSocket[2];

void ShellApplication::registerUnixSignals()
{
    int result = ::socketpair(AF_UNIX, SOCK_STREAM, 0, unixSignalSocket);
    Q_ASSERT(0 == result);
    if (0 != result) {
        // do not register handles when socket creation failed, otherwise
        // application will be unresponsive to signals such as SIGINT or SIGTERM
        return;
    }

    QVector<int> const handledSignals = {SIGQUIT, SIGINT, SIGTERM, SIGHUP};
    for (auto s : handledSignals) {
        struct sigaction sigAction;

        sigAction.sa_handler = handleUnixSignal;
        sigemptyset(&sigAction.sa_mask);
        sigAction.sa_flags = 0 | SA_RESTART;
        sigaction(s, &sigAction, nullptr);
    }

    m_unixSignalNotifier = new QSocketNotifier(unixSignalSocket[1], QSocketNotifier::Read, this);
    connect(m_unixSignalNotifier, SIGNAL(activated(int)), this, SLOT(quitBySignal()));
}

void ShellApplication::handleUnixSignal(int sig)
{
    switch (sig) {
    case SIGQUIT:
    case SIGINT:
    case SIGTERM: {
        char buf = 0;
        Q_UNUSED(::write(unixSignalSocket[0], &buf, sizeof(buf)));
        return;
    }
    case SIGHUP:
        return;
    }
}

void ShellApplication::quitBySignal()
{
    m_unixSignalNotifier->setEnabled(false);
    char buf;
    Q_UNUSED(::read(unixSignalSocket[1], &buf, sizeof(buf)));
    emit quitSignalReceived();
}
#endif

void ShellApplication::processIncomingConnection()
{
    if (m_lockServer.hasPendingConnections()) {
        QLocalSocket* socket = m_lockServer.nextPendingConnection();
        socket->setProperty(BlockSizeProperty, 0);
        connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
    }
}

void ShellApplication::socketReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) {
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_0);

    int blockSize = socket->property(BlockSizeProperty).toInt();
    if (blockSize == 0) {
        // Relies on the fact that QDataStream format streams a quint32 into sizeof(quint32) bytes
        if (socket->bytesAvailable() < qint64(sizeof(quint32))) {
            return;
        }
        in >> blockSize;
    }

    if (socket->bytesAvailable() < blockSize || in.atEnd()) {
        socket->setProperty(BlockSizeProperty, blockSize);
        return;
    }

    QStringList arguments;
    in >> arguments;
    int exitCode = executeCommand(arguments);
    if (exitCode) {
      qDebug("Command exited with an error");
    } else {
      qDebug("Command exited with success");
    }
    socket->deleteLater();
}

int ShellApplication::executeCommand(const QStringList& arguments) const
{
    TextStream errorTextStream(Utils::STDERR, QIODevice::WriteOnly);
    if (arguments.size() == 0) {
        return EXIT_FAILURE;
    }

    if (arguments.at(0) == QString("lock")) {
        exit();
    }
    errorTextStream << arguments.at(0) << endl;
    return 0;

}

QString ShellApplication::getIdentifier()
{
    QString identifier = "keepassxc-shell";

    QString userName = qgetenv("USER");
    if (userName.isEmpty()) {
        userName = qgetenv("USERNAME");
    }
    if (!userName.isEmpty()) {
        identifier += "-" + userName;
    }
#ifdef QT_DEBUG
    // In DEBUG mode don't interfere with Release instances
    identifier += "-DEBUG";
#endif
    return identifier;
}

bool ShellApplication::isAlreadyRunning() const
{
    return m_alreadyRunning;
}

void ShellApplication::setDatabase(QSharedPointer<Database> database)
{
    m_database = database;
}

QSharedPointer<Database> ShellApplication::getDatabase()
{
    return m_database;
}

bool ShellApplication::sendArgumentsToRunningInstance(const QStringList& arguments)
{
    QLocalSocket client;
    client.connectToServer(m_socketName);
    const bool connected = client.waitForConnected(WaitTimeoutMSec);
    if (!connected) {
        return false;
    }

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_0);
    out << quint32(0) << arguments;
    out.device()->seek(0);
    out << quint32(data.size() - sizeof(quint32));

    const bool writeOk = client.write(data) != -1 && client.waitForBytesWritten(WaitTimeoutMSec);
    client.disconnectFromServer();
    const bool disconnected = client.waitForDisconnected(WaitTimeoutMSec);
    return writeOk && disconnected;
}
