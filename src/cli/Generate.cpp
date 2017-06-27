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
#include <stdio.h>

#include "Generate.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/PasswordGenerator.h"
#include "keys/CompositeKey.h"
#include "cli/Utils.h"

Generate::Generate()
{
    this->name = QString("gen");
    this->shellUsage = QString("gen new_entry_path [password_length]");
    this->description = QString("Add an entry with generated password.");
}

Generate::~Generate()
{
}

int Generate::execute(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "Add an entry with generated password."));
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "Path of the database."));
    parser.addPositionalArgument("entry", QCoreApplication::translate("main", "Path of the entry to add."));
    parser.addPositionalArgument(
        "length",
        QCoreApplication::translate("main", "Password length (default is 20)"),
        QString("[password_length]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() < 2 || args.size() > 3) {
        parser.showHelp(EXIT_FAILURE);
    }

    out << "Insert the database password\n> ";
    out.flush();

    QString line = Utils::getPassword();
    CompositeKey key = CompositeKey::readFromLine(line);

    Database* db = Database::openDatabaseFile(args.at(0), key);
    if (db == nullptr) {
        return EXIT_FAILURE;
    }

    return this->addGeneratedEntry(db, args.at(0), args.at(1), args.value(2));

}

int Generate::executeFromShell(Database* database, QString databasePath, QStringList arguments)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    if (arguments.size() < 1 || arguments.size() > 2) {
        outputTextStream << this->shellUsage << "\n";
        outputTextStream.flush();
        return EXIT_FAILURE;
    }
    return this->addGeneratedEntry(database, databasePath, arguments.at(0), arguments.value(1));
}

int Generate::addGeneratedEntry(Database* database, QString databasePath, QString entryPath, QString passwordLength)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    int passwordLengthInt = 20;
    if (!passwordLength.isEmpty()) {
        passwordLengthInt = passwordLength.toInt();
        // TODO test negative number!!
        if (!passwordLengthInt) {
            qCritical("Invalid password length %s!", qPrintable(passwordLength));
            return EXIT_FAILURE;
        }
    }

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (entry) {
        qCritical("Entry %s already exists!", qPrintable(entryPath));
        return EXIT_FAILURE;
    }

    entry = database->rootGroup()->addEntryWithPath(entryPath);
    if (!entry) {
        qCritical("Could not create entry with path %s.", qPrintable(entryPath));
        return EXIT_FAILURE;
    }

    outputTextStream << "username: ";
    outputTextStream.flush();
    QString username = inputTextStream.readLine();

    PasswordGenerator passwordGenerator;
    passwordGenerator.setLength(passwordLengthInt);
    passwordGenerator.setCharClasses(PasswordGenerator::LowerLetters | PasswordGenerator::UpperLetters |
                                     PasswordGenerator::Numbers | PasswordGenerator::SpecialCharacters);

    QString password = passwordGenerator.generatePassword();

    outputTextStream << "     URL: ";
    outputTextStream.flush();
    QString url = inputTextStream.readLine();

    entry->setPassword(password);
    entry->setUsername(username);
    entry->setUrl(url);

    QString errorMessage = database->saveToFile(databasePath);
    if (!errorMessage.isEmpty()) {
        qCritical("Unable to save database to file : %s", qPrintable(errorMessage));
        return EXIT_FAILURE;
    }

    outputTextStream << "Successfully added new entry!\n";
    outputTextStream.flush();

    return EXIT_SUCCESS;

}
