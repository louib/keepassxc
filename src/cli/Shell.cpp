/*
 *  Copyright (C) 2017 KeePassXC Team
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
#include "Shell.h"
#include "Command.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QProcess>
#include <QTextStream>

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "cli/Utils.h"
#include "config-keepassx.h"

#ifdef WITH_XC_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

Shell::Shell()
{
    this->name = QString("shell");
    this->description = QString("Launch the interactive shell.");
}

Shell::~Shell()
{
}

Database* database;


#ifdef WITH_XC_READLINE
char* commandArgumentsCompletion(const char*, int state)
{

    static int currentIndex;
    static QMap<QString, QString> firstArguments;
    static QStringList fieldNames;

    if (firstArguments.isEmpty()) {
        firstArguments.insert("rm", "entry");
        firstArguments.insert("mv", "entry");
        firstArguments.insert("show", "entry");
        firstArguments.insert("clip", "entry");
        firstArguments.insert("edit", "entry");
        firstArguments.insert("rmdir", "group");
        firstArguments.insert("mkdir", "group");
        firstArguments.insert("gen", "group");
        firstArguments.insert("ls", "group");
        firstArguments.insert("add", "group");
    }

    if (fieldNames.isEmpty()) {
        fieldNames << "url";
        fieldNames << "password";
        fieldNames << "username";
    }

    if (state == 0) {
        currentIndex = 0;
    }

    QStringList arguments = QString::fromUtf8(rl_line_buffer).split(QRegExp(" "), QString::KeepEmptyParts);
    QString commandName = arguments.at(0);

    QString currentText = arguments.last();

    if (arguments.size() == 2) {

        if (!firstArguments.contains(commandName)) {
            return nullptr;
        }

        QStringList suggestions;
        if (firstArguments[commandName] == "entry") {
            suggestions = database->rootGroup()->getSuggestions(arguments.at(1), true);
        } else {
            suggestions = database->rootGroup()->getSuggestions(arguments.at(1), false);
        }

        while (currentIndex < suggestions.size()) {

            const char* suggestion = qPrintable(suggestions.at(currentIndex++));

            // Each string the generator function returns as a match
            // must be allocated with malloc();
            char* response = static_cast<char*>(malloc(sizeof(char) * strlen(suggestion)));
            strcpy(response, suggestion);
            return response;

        }

    } else if (arguments.size() == 3) {

        QStringList suggestions;
        if (commandName == "edit") {
            suggestions = QStringList(fieldNames);
        } else if (commandName == "mv") {
            suggestions = database->rootGroup()->getSuggestions(arguments.at(1), false);
        }

        while (currentIndex < suggestions.size()) {

            QString currentSuggestion = suggestions.at(currentIndex++);

            if (currentSuggestion.startsWith(currentText)) {
                const char* suggestion = qPrintable(currentSuggestion);

                // Each string the generator function returns as a match
                // must be allocated with malloc();
                char* response = static_cast<char*>(malloc(sizeof(char) * strlen(suggestion)));
                strcpy(response, suggestion);
                return response;

            }

        }

    }

    return nullptr;

}

char* commandNameCompletion(const char* text, int state)
{
    static int currentIndex;
    static QStringList commandNames;

    if (commandNames.isEmpty()) {
        for (Command* command : Command::getShellCommands()) {
            commandNames << command->name;
        }
        commandNames << QString("quit");
        commandNames << QString("help");
    }

    if (state == 0) {
        currentIndex = 0;
    }

    while (currentIndex < commandNames.size()) {

        const char* commandName = qPrintable(commandNames.at(currentIndex++));

        // Each string the generator function returns as a match
        // must be allocated with malloc();
        if (strncmp(commandName, text, strlen(text)) == 0) {
            char* response = static_cast<char*>(malloc(sizeof(char) * strlen(commandName)));
            strcpy(response, commandName);
            return response;
        }

    }

    return nullptr;
}

char** keepassxc_completion(const char* text, int start, int)
{

    // First word of the line, so it's a command to complete.
    if (start == 0) {
        rl_completion_suppress_append = 0;
        return rl_completion_matches(text, commandNameCompletion);
    } else {
        rl_completion_suppress_append = 1;
        return rl_completion_matches(text, commandArgumentsCompletion);
    }

    return nullptr;
}
#endif

int clipText(QString text)
{

    QString programName = "";
    QStringList arguments;

#ifdef Q_OS_MACOS
    programName = "pbcopy";
#endif

#ifdef Q_OS_UNIX
    programName = "xclip";
    arguments << "-i" << "-selection" << "clipboard";
#endif

    // TODO add this for windows.
    if (programName.isEmpty()) {
        qCritical("No program defined for clipboard manipulation");
        return EXIT_FAILURE;
    }

    QProcess* clipProcess = new QProcess(nullptr);
    clipProcess->start(programName, arguments);
    clipProcess->waitForStarted();

    const char* data = qPrintable(text);
    clipProcess->write(data, strlen(data));
    clipProcess->waitForBytesWritten();
    clipProcess->closeWriteChannel();
    clipProcess->waitForFinished();

    return clipProcess->exitCode();
}


bool clipEntry(QString entryPath)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        qCritical("Entry %s does not exist!", qPrintable(entryPath));
        return false;
    }

    clipText(entry->password());

    outputTextStream << "Successfully copied password to clipboard!\n";
    return true;

}

bool move(QString groupEntryPath, QString destinationPath)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    Group* destinationGroup = database->rootGroup()->findGroupByPath(destinationPath);
    if (!destinationGroup) {
        qCritical("Group %s not found.", qPrintable(destinationPath));
        return false;
    }

    Entry* entry = database->rootGroup()->findEntryByPath(groupEntryPath);
    if (!entry) {
        Group* group = database->rootGroup()->findGroupByPath(groupEntryPath);
        if (group == nullptr) {
            qCritical("No group or entry found with path %s.", qPrintable(groupEntryPath));
            return false;
        }
        group->setParent(destinationGroup);
        outputTextStream << QString(group->name() + " moved to " + destinationGroup->name() + "\n");
    } else {
        entry->setGroup(destinationGroup);
        outputTextStream << QString(entry->title() + " moved to " + destinationGroup->name() + "\n");
    }
    return true;

}

void printHelp()
{
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    for (Command* command : Command::getShellCommands()) {
        outputTextStream << command->getDescriptionLine();
    }
    outputTextStream.flush();
}

QString getLine(QString prompt)
{
#ifdef WITH_XC_READLINE
    char* chars = readline(qPrintable(prompt));
    QString line(chars);

    if (!line.isEmpty()) {
      add_history(chars);
    }
#else
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    outputTextStream << prompt;
    outputTextStream.flush();

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QString line = inputTextStream.readLine();
#endif
    return line;
}

int Shell::execute(int argc, char** argv)
{
    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << QString(argv[i]);
    }
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Start KeePassXC's shell"));
    parser.addPositionalArgument("database",
                                 QCoreApplication::translate("main", "Path of the database."));
    QCommandLineOption guiPrompt(
        QStringList() << "g"
                      << "gui-prompt",
        QCoreApplication::translate("main", "Use a GUI prompt unlocking the database."));
    parser.addOption(guiPrompt);
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        QCoreApplication app(argc, argv);
        parser.showHelp(EXIT_FAILURE);
    }

    QCoreApplication app(argc, argv);
    database = Database::unlockFromStdin(args.at(0));

    if (!database) {
        return EXIT_FAILURE;
    }

    out << "KeePassXC " << KEEPASSX_VERSION << " interactive shell\n";
    if (!database->metadata()->name().isNull()) {
        out << "Using database " << database->metadata()->name() << '\n';
    } else {
        out << "Using database " << args.at(0) << '\n';
    }
    out << "Use 'help' to list the available commands.\n";
    out.flush();

#ifdef WITH_XC_READLINE
    rl_readline_name = const_cast<char*>("kpxcli");
    rl_attempted_completion_function = keepassxc_completion;
#endif

    while (true) {

      out.flush();
      QString line = getLine("KeePassXC> ");
      if (line.isEmpty()) {
          continue;
      }

      QStringList arguments = line.trimmed().split(QRegExp(" "));
      QString commandName = arguments.takeFirst();

      Command* command = Command::getCommand(commandName);
      if (command && command->isShellCommand()) {
          command->executeFromShell(database, args.at(0), arguments);
      } else if (commandName == QString("help")) {
          printHelp();
      } else if (commandName == QString("clip")) {
          if (arguments.length() != 2) {
              out << "Usage: clip entry\n";
              continue;
          }
          clipEntry(arguments.at(1));
      } else if (commandName == QString("mv")) {
          if (arguments.length() != 3) {
              out << "Usage: mv entry|group group\n";
              continue;
          }
          move(arguments.at(1), arguments.at(2));
      } else if (commandName == QString("quit")) {
          break;
      } else {
          out << QString("Invalid command '" + commandName + "'\n");
      }

    }

    delete database;
    return EXIT_SUCCESS;
}
