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

#ifndef KEEPASSX_APPLICATION_H
#define KEEPASSX_APPLICATION_H

#include <QCoreApplication>
#include <QtNetwork/QLocalServer>

#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
#include <QScopedPointer>

class OSEventFilter;
#endif
class QLockFile;
class QSocketNotifier;

#include "core/Database.h"


class ShellApplication : public QCoreApplication
{
    Q_OBJECT

public:
    ShellApplication(int& argc, char** argv);
    ~ShellApplication() override;

    bool isAlreadyRunning() const;
    QString getIdentifier();

    bool sendArgumentsToRunningInstance(const QStringList& arguments);
    int executeCommand(const QStringList& arguments) const;
    void preserveLock();
    void setDatabase(QSharedPointer<Database> database);
    QSharedPointer<Database> getDatabase();

signals:
    void anotherInstanceStarted();
    void applicationActivated();
    void quitSignalReceived();

private slots:
#if defined(Q_OS_UNIX)
    void quitBySignal();
#endif
    void processIncomingConnection();
    void socketReadyRead();

private:
    QSharedPointer<Database> m_database;
#if defined(Q_OS_UNIX)
    /**
     * Register Unix signals such as SIGINT and SIGTERM for clean shutdown.
     */
    void registerUnixSignals();
    QSocketNotifier* m_unixSignalNotifier;
    static void handleUnixSignal(int sig);
    static int unixSignalSocket[2];
#endif
    bool m_alreadyRunning;
    QLockFile* m_lockFile;
    QLocalServer m_lockServer;
    QString m_socketName;
#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    QScopedPointer<OSEventFilter> m_osEventFilter;
#endif
};

#endif // KEEPASSX_APPLICATION_H
