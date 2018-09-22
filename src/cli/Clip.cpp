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

#include <chrono>
#include <cstdlib>
#include <stdio.h>
#include <thread>

#include "Clip.h"

#include <QCommandLineParser>
#include <QTextStream>

#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"

Clip::Clip()
{
    name = QString("clip");
    description = QObject::tr("Copy an entry's password to the clipboard.");
}

Clip::~Clip()
{
}

int Clip::execute(const QStringList& arguments)
{

    QCommandLineParser parser;
    parser.setApplicationDescription(this->description);
    parser.addOption(Command::SilentOption);
    parser.addOption(Command::KeyFileOption);
    parser.addPositionalArgument("database", QObject::tr("Path of the database."));
    parser.addPositionalArgument("entry", QObject::tr("Path of the entry to clip.", "clip = copy to clipboard"));
    parser.addPositionalArgument(
        "timeout", QObject::tr("Timeout in seconds before clearing the clipboard."), QString("[timeout]"));
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2 && args.size() != 3) {
        QTextStream errorTextStream(stderr, QIODevice::WriteOnly);
        errorTextStream << parser.helpText().replace("keepassxc-cli", "keepassxc-cli clip");
        return EXIT_FAILURE;
    }

    Database* db = Database::unlockFromStdin(
        args.at(0), parser.value(Command::KeyFileOption), parser.isSet(Command::SilentOption));
    if (!db) {
        return EXIT_FAILURE;
    }

    return this->clipEntry(db, args.at(1), args.value(2), parser.isSet(Command::SilentOption));
}

int Clip::clipEntry(Database* database, QString entryPath, QString timeout, bool silent)
{

    int timeoutSeconds = 0;
    if (!timeout.isEmpty() && !timeout.toInt()) {
        qCritical("Invalid timeout value %s.", qPrintable(timeout));
        return EXIT_FAILURE;
    } else if (!timeout.isEmpty()) {
        timeoutSeconds = timeout.toInt();
    }

    Entry* entry = database->rootGroup()->findEntry(entryPath);
    if (!entry) {
        qCritical("Entry %s not found.", qPrintable(entryPath));
        return EXIT_FAILURE;
    }

    int exitCode = Utils::clipText(entry->password());
    if (exitCode != EXIT_SUCCESS) {
        return exitCode;
    }

    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);
    if (!silent) {
        outputTextStream << "Entry's password copied to the clipboard!" << endl;
    }

    if (!timeoutSeconds) {
        return exitCode;
    }

    while (timeoutSeconds > 0) {
        if (!silent) {
            outputTextStream << "\rClearing the clipboard in " << timeoutSeconds << " ..." << endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        timeoutSeconds--;
    }
    Utils::clipText("");
    if (!silent) {
        outputTextStream << "\nClipboard cleared!" << endl;
    }

    return EXIT_SUCCESS;
}
