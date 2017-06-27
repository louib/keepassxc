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

#include "Add.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "keys/CompositeKey.h"
#include "cli/Utils.h"

Add::Add()
{
    this->name = QString("add");
    this->shellUsage = QString("add new_entry_path");
    this->description = QString("Add an entry to the database.");
}

Add::~Add()
{
}

int Add::execute(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Add an entry to the database."));
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "Path of the database."));
    parser.addPositionalArgument("entry", QCoreApplication::translate("main", "Name of the entry to add."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
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

    return this->addEntry(db, args.at(0), args.at(1));

}

int Add::executeFromShell(Database* database, QString databasePath, QStringList arguments)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    if (arguments.size() != 1) {
        outputTextStream << this->shellUsage << "\n";
        outputTextStream.flush();
        return EXIT_FAILURE;
    }
    return this->addEntry(database, databasePath, arguments.at(0));
}

int Add::addEntry(Database* database, QString databasePath, QString entryPath)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
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

    outputTextStream << "password: ";
    outputTextStream.flush();
    QString password = Utils::getPassword();

    outputTextStream << "  repeat: ";
    outputTextStream.flush();
    QString passwordConfirmation = Utils::getPassword();

    if (password != passwordConfirmation) {
        qCritical("Passwords do not match.");
        return EXIT_FAILURE;
    }

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
