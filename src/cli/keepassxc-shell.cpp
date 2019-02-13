/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include <cstdlib>
// move this and the fork to cli/Utils
#include <unistd.h>

#include <QCommandLineParser>
#include <QStringList>
#include <QProcess>

#include "cli/ShellApplication.h"
#include "cli/TextStream.h"
#include "cli/List.h"
#include <cli/Command.h>
#include "cli/Utils.h"

#include "config-keepassx.h"
#include "core/Bootstrap.h"
#include "crypto/Crypto.h"

#if defined(WITH_ASAN) && defined(WITH_LSAN)
#include <sanitizer/lsan_interface.h>
#endif

int main(int argc, char** argv)
{
    if (!Crypto::init()) {
        qFatal("Fatal error while testing the cryptographic functions:\n%s", qPrintable(Crypto::errorString()));
        return EXIT_FAILURE;
    }

    ShellApplication app(argc, argv);
    ShellApplication::setApplicationVersion(KEEPASSXC_VERSION);


    Bootstrap::bootstrap();

    TextStream out(stdout);
    TextStream errorTextStream(Utils::STDERR, QIODevice::WriteOnly);
    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << QString(argv[i]);
    }

    QString description("KeePassXC database shell.");
    description = description.append(QObject::tr("\n\nAvailable commands:\n"));
    for (Command* command : Command::getCommands()) {
        description = description.append(command->getDescriptionLine());
    }


    QCommandLineParser parser;
    parser.setApplicationDescription(description);
    parser.addPositionalArgument("command", QObject::tr("Name of the command to execute."));

    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(Command::QuietOption);
    parser.addOption(Command::KeyFileOption);
    // TODO : use the setOptionsAfterPositionalArgumentsMode (Qt 5.6) function
    // when available. Until then, options passed to sub-commands won't be
    // recognized by this parser.
    parser.parse(arguments);

    if (parser.positionalArguments().empty()) {
        if (parser.isSet("version")) {
            // Switch to parser.showVersion() when available (QT 5.4).
            out << KEEPASSXC_VERSION << endl;
            return EXIT_SUCCESS;
        }
        parser.showHelp();
    }


    QString commandName = parser.positionalArguments().at(0);
    if (commandName == QString("unlock")) {
        if (arguments.size() != 3) {
            errorTextStream << parser.helpText().replace("keepassxc-shell", "keepassxc-shell unlock");
            return EXIT_FAILURE;
        }
        auto db = Utils::unlockDatabase(arguments.at(2),
                                        parser.value(Command::KeyFileOption),
                                        parser.isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT,
                                        Utils::STDERR);
        if (!db) {
            return EXIT_FAILURE;
        }
        app.setDatabase(db);
        // TODO print database name instead if populated??
        out << QObject::tr("🔓 Successfully unlocked database %1 🔓").arg(arguments.at(2)) << endl;
        if (!app.isAlreadyRunning()) {

            pid_t processId = fork();
            // parent process. Nothing more to do 🔫.
            if (processId > 0) {
                errorTextStream << QObject::tr("Forked process %1 🍴🍴").arg(processId) << endl;
                app.preserveLock();
                return EXIT_SUCCESS;

            } else if (processId < 0) {
                errorTextStream << QObject::tr("Could not fork 🍴🍴") << endl;
                return EXIT_FAILURE;
            }

            qDebug("yo whats up!!");
            int exitCode = ShellApplication::exec();
            qDebug("daemon killed 🔫 %1", qPrintable(exitCode));
            return exitCode;
        } else {
            return EXIT_SUCCESS;
        }
    }

    if (!app.isAlreadyRunning()) {
        out << QObject::tr("🔑 No database unlocked 🔑") << endl;
        return EXIT_FAILURE;
    }

    app.sendArgumentsToRunningInstance(arguments);

#if defined(WITH_ASAN) && defined(WITH_LSAN)
    // do leak check here to prevent massive tail of end-of-process leak errors from third-party libraries
    __lsan_do_leak_check();
    __lsan_disable();
#endif

    return EXIT_SUCCESS;
}
