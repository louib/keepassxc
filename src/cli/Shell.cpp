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
#include "core/PasswordGenerator.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "cli/Utils.h"
#include "keys/CompositeKey.h"
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
        firstArguments.insert("regen", "entry");
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
        commandNames << "show";
        commandNames << "rm";
        commandNames << "mkdir";
        commandNames << "quit";
        commandNames << "rmdir";
        commandNames << "save";
        commandNames << "clip";
        commandNames << "add";
        commandNames << "gen";
        commandNames << "regen";
        commandNames << "edit";
        commandNames << "help";
        commandNames << "ls";
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

bool saveDatabase(QString filename)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    QString errorMessage = database->saveToFile(filename);
    if (errorMessage.isEmpty()) {
        outputTextStream << QString("Successfully saved database.\n");
        outputTextStream.flush();
        return true;
    }

    outputTextStream << errorMessage;
    outputTextStream.flush();
    return false;

}

bool regenerate(QString entryPath, int passwordLength = 20)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        qCritical("Entry %s does not exist!", qPrintable(entryPath));
        return false;
    }

    PasswordGenerator passwordGenerator;
    passwordGenerator.setLength(passwordLength);
    passwordGenerator.setCharClasses(PasswordGenerator::LowerLetters | PasswordGenerator::UpperLetters |
                                     PasswordGenerator::Numbers | PasswordGenerator::SpecialCharacters);

    QString password = passwordGenerator.generatePassword();
    entry->beginUpdate();
    entry->setPassword(password);
    entry->endUpdate();

    outputTextStream << "Successfully generated new password for entry!\n";
    outputTextStream.flush();
    return true;

}

bool editEntry(QString entryPath, QString fieldName)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        qCritical("Entry %s does not exist!", qPrintable(entryPath));
        return false;
    }

    if (fieldName == "url") {

        outputTextStream << "enter new url: ";
        outputTextStream.flush();
        QString url = inputTextStream.readLine();

        entry->beginUpdate();
        entry->setUrl(url);
        entry->endUpdate();

    } else if (fieldName == "username") {

        outputTextStream << "enter new username: ";
        outputTextStream.flush();
        QString username = inputTextStream.readLine();

        entry->beginUpdate();
        entry->setUsername(username);
        entry->endUpdate();

    } else if (fieldName == "password") {

        outputTextStream << "enter new password: ";
        outputTextStream.flush();
        QString password = Utils::getPassword();

        outputTextStream << "confirm new password: ";
        outputTextStream.flush();
        QString passwordConfirmation = Utils::getPassword();

        if (password != passwordConfirmation) {
            qCritical("Passwords do not match.");
            return false;
        }

        entry->beginUpdate();
        entry->setPassword(password);
        entry->endUpdate();

    } else {
        qCritical("Invalid field name %s.", qPrintable(fieldName));
        return false;
    }

    outputTextStream << "Successfully edited entry!\n";
    return true;

}

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

bool addEntry(QString entryPath, bool generate = false, int passwordLength = 20)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (entry) {
        qCritical("Entry %s already exists!", qPrintable(entryPath));
        return false;
    }

    entry = database->rootGroup()->addEntryWithPath(entryPath);
    if (!entry) {
        qCritical("Could not create entry with path %s.", qPrintable(entryPath));
        return false;
    }

    outputTextStream << "username: ";
    outputTextStream.flush();
    QString username = inputTextStream.readLine();

    QString password;
    if (generate) {
        PasswordGenerator passwordGenerator;
        passwordGenerator.setLength(passwordLength);
        passwordGenerator.setCharClasses(PasswordGenerator::LowerLetters | PasswordGenerator::UpperLetters |
                                         PasswordGenerator::Numbers | PasswordGenerator::SpecialCharacters);

        password = passwordGenerator.generatePassword();

    } else {

        outputTextStream << "password: ";
        outputTextStream.flush();
        password = Utils::getPassword();

        outputTextStream << "  repeat: ";
        outputTextStream.flush();
        QString passwordConfirmation = Utils::getPassword();

        if (password != passwordConfirmation) {
            qCritical("Passwords do not match.");
            return false;
        }

    }

    outputTextStream << "     URL: ";
    outputTextStream.flush();
    QString url = inputTextStream.readLine();

    entry->setPassword(password);
    entry->setUsername(username);
    entry->setUrl(url);

    outputTextStream << "Successfully added new entry!\n";
    outputTextStream.flush();
    return true;

}

bool addGroup(QString groupPath)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    Group* group = database->rootGroup()->findGroupByPath(groupPath);
    if (group != nullptr) {
	qCritical("Group %s already exists.", qPrintable(groupPath));
	return false;
    }

    group = database->rootGroup()->addGroupWithPath(groupPath);
    if (group == nullptr) {
        return false;
    }

    return true;
}


void createRecycleBin()
{

    Group* recycleBin = database->metadata()->recycleBin();
    if (recycleBin == nullptr) {
        database->createRecycleBin();
        database->metadata()->recycleBin()->setName("trash");
    } else if (recycleBin->name() != "trash") {
        recycleBin->setName("trash");
    }

}

bool removeEntry(QString entryPath)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    Entry* entry = database->rootGroup()->findEntry(entryPath);
    if (!entry) {
        qCritical("Entry %s not found.", qPrintable(entryPath));
        return false;
    }

    createRecycleBin();

    QString entryTitle = entry->title();
    if (Tools::hasChild(database->metadata()->recycleBin(), entry) || !database->metadata()->recycleBinEnabled()) {
        if (!Utils::askYesNoQuestion("You are about to remove entry " + entryTitle + " permanently.", true)) {
            return false;
        }
        delete entry;
        outputTextStream << "Successfully removed entry " << entryTitle << ".\n";
        outputTextStream.flush();
    } else {
        database->recycleEntry(entry);
        outputTextStream << "Successfully recycled entry " << entryTitle << ".\n";
        outputTextStream.flush();
    };

    return true;

}

bool removeGroup(QString groupPath)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    Group* group = database->rootGroup()->findGroupByPath(groupPath);
    if (!group) {
        qCritical("Group %s not found.", qPrintable(groupPath));
        return false;
    }

    createRecycleBin();
    QString groupName = group->name();
    bool inRecycleBin = Tools::hasChild(database->metadata()->recycleBin(), group);
    bool isRecycleBin = (group == database->metadata()->recycleBin());
    bool isRecycleBinSubgroup = Tools::hasChild(group, database->metadata()->recycleBin());
    if (inRecycleBin || isRecycleBin || isRecycleBinSubgroup || !database->metadata()->recycleBinEnabled()) {
        if (!Utils::askYesNoQuestion("You are about to remove group " + groupName + " permanently.", true)) {
            return false;
        }
        delete group;
        outputTextStream << "Successfully removed group.\n";
    } else {
        outputTextStream << "Successfully recycled group " << groupName << ".\n";
        database->recycleGroup(group);
    }

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
    for (Command* command : Command::getCommands()) {
        if (!command->shellUsage.isEmpty()) {
            outputTextStream << command->getDescriptionLine();
        }
    }

    //outputTextStream << "add\t\tAdd an entry to the database.\n";
    //outputTextStream << "clip\t\tCopy an entry's password to the clipboard.\n";
    //outputTextStream << "gen\t\tAdd an entry with generated password to the database.\n";
    //outputTextStream << "regen\t\tGenerate a new password for an entry.\n";
    //outputTextStream << "rm\t\tRemove an entry from the database.\n";
    //outputTextStream << "mv\t\tMove an entry of a directory.\n";
    //outputTextStream << "rmdir\t\tRemove a directory from the database.\n";
    //outputTextStream << "mkdir\t\tCreate a directory in the database.\n";
    //outputTextStream << "save\t\tSave the database.\n";
    //outputTextStream << "show\t\tShow an entry from the database.\n";
    //outputTextStream << "help\t\tShow the available commands.\n";
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

    bool databaseModified = false;

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
      QString line;
      if (databaseModified) {
          line = getLine("KeePassXC*> ");
      } else {
          line = getLine("KeePassXC> ");
      }
      if (line.isEmpty()) {
          continue;
      }


      QStringList arguments = line.trimmed().split(QRegExp(" "));
      QString commandName = arguments.takeFirst();

      Command* command = Command::getCommand(commandName);
      if (command && !command->shellUsage.isEmpty()) {
          command->executeFromShell(database, arguments);
          database->saveToFile(args.at(0));
      } else if (commandName == QString("help")) {
          printHelp();
      } else if (commandName == QString("edit")) {
          if (arguments.length() != 3) {
              out << "Usage: edit entry field_name\n";
              continue;
          }
          databaseModified |= editEntry(arguments.at(1), arguments.at(2));
      } else if (commandName == QString("clip")) {
          if (arguments.length() != 2) {
              out << "Usage: clip entry\n";
              continue;
          }
          clipEntry(arguments.at(1));
      } else if (commandName == QString("gen")) {
          if (arguments.length() < 2) {
              out << "Usage: gen entry [password_length]\n";
              continue;
          }
          int passwordLength = 20;
          if (arguments.length() == 3 && arguments.at(2).toInt()) {
              passwordLength = arguments.at(2).toInt();
          }
          databaseModified |= addEntry(arguments.at(1), true, passwordLength);
      } else if (commandName == QString("regen")) {
          if (arguments.length() < 2) {
              out << "Usage: regen entry [password_length]\n";
              continue;
          }
          int passwordLength = 20;
          if (arguments.length() == 3 && arguments.at(2).toInt()) {
              passwordLength = arguments.at(2).toInt();
          }
          databaseModified |= regenerate(arguments.at(1), passwordLength);
      } else if (commandName == QString("rmdir")) {
          if (arguments.length() != 1) {
              out << "Usage: rm entry\n";
              continue;
          }
          databaseModified |= removeGroup(arguments.at(0));
      } else if (commandName == QString("rm")) {
          if (arguments.length() != 1) {
              out << "Usage: rm entry\n";
              continue;
          }
          databaseModified |= removeEntry(arguments.at(0));
      } else if (commandName == QString("mv")) {
          if (arguments.length() != 3) {
              out << "Usage: mv entry|group group\n";
              continue;
          }
          databaseModified |= move(arguments.at(1), arguments.at(2));
      } else if (commandName == QString("mkdir")) {
          if (arguments.length() != 2) {
              out << "Usage: mkdir group\n";
              continue;
          }
          databaseModified |= addGroup(arguments.at(1));
      } else if (commandName == QString("quit")) {
          if (!databaseModified) {
              break;
          }
          if (!Utils::askYesNoQuestion("The database was modified, do you want to save it?")) {
              break;
          }
          QString errorMessage = database->saveToFile(args.at(0));
          if (errorMessage.isEmpty()) {
              databaseModified = false;
              out << QString("Successfully saved database " + args.at(0) + "\n");
          } else {
              out << errorMessage;
          }
          break;
      } else if (commandName == QString("save")) {
          databaseModified = (databaseModified && !saveDatabase(args.at(0)));
      } else {
          out << QString("Invalid command '" + commandName + "'\n");
      }

    }

    delete database;
    return EXIT_SUCCESS;
}

int Shell::executeFromShell(Database*, QStringList)
{
    return EXIT_FAILURE;
}
