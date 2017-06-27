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

#include "Clip.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QTextStream>

#include "gui/UnlockDatabaseDialog.h"
#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "gui/Clipboard.h"

Clip::Clip()
{
    this->name = QString("clip");
    this->shellUsage = QString("clip entry_path [clip_clear_timeout]");
    this->description = QString("Copy an entry's password to the clipboard.");
}

Clip::~Clip()
{
}

int Clip::execute(int argc, char** argv)
{

    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << QString(argv[i]);
    }
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Copy a password to the clipboard"));
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "Path of the database."));
    QCommandLineOption guiPrompt(
        QStringList() << "g"
                      << "gui-prompt",
        QCoreApplication::translate("main", "Use a GUI prompt unlocking the database."));
    parser.addOption(guiPrompt);
    parser.addPositionalArgument("entry", QCoreApplication::translate("main", "Path of the entry to clip."));
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        QCoreApplication app(argc, argv);
        parser.showHelp(EXIT_FAILURE);
    }

    Database* db = nullptr;
    QApplication app(argc, argv);
    if (parser.isSet("gui-prompt")) {
        db = UnlockDatabaseDialog::openDatabasePrompt(args.at(0));
    } else {
        db = Database::unlockFromStdin(args.at(0));
    }

    if (!db) {
        return EXIT_FAILURE;
    }
    return this->clipEntry(db, args.at(1));
}

int Clip::executeFromShell(Database* database, QString, QStringList arguments)
{
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    if (arguments.size() != 1) {
        outputTextStream << this->shellUsage << "\n";
        outputTextStream.flush();
        return EXIT_FAILURE;
    }
    return this->clipEntry(database, arguments.at(0));
}

int Clip::clipEntry(Database* database, QString entryPath)
{

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    Entry* entry = database->rootGroup()->findEntry(entryPath);
    if (!entry) {
        qCritical("Entry %s not found.", qPrintable(entryPath));
        return EXIT_FAILURE;
    }

    int exitCode = Utils::clipText(entry->password());
    if (exitCode == 0) {
        outputTextStream << "Entry's password copied to the clipboard!\n";
        outputTextStream.flush();
    }

    return exitCode;

}

QStringList Clip::getSuggestions(Database* database, QStringList arguments)
{
    if (arguments.size() != 1) {
        return QStringList();
    }
    QString currentText = arguments.last();
    return database->rootGroup()->getSuggestions(arguments.at(0), true);
}
