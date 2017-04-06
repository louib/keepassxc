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

#include <cstdlib>
#include <stdio.h>

#include "List.h"

#include <QCommandLineParser>
#include <QApplication>
#include <QStringList>
#include <QTextStream>

#include "core/Database.h"
#include "gui/UnlockDatabaseDialog.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "keys/CompositeKey.h"

void printGroup(Group* group, QString baseName, int depth) {

    QTextStream out(stdout);

    QString groupName = baseName + group->name() + "/";
    QString indentation = QString("  ").repeated(depth);

    out << indentation << groupName << " " << group->uuid().toHex() << "\n";
    out.flush();

    if (group->entries().isEmpty() && group->children().isEmpty()) {
        out << indentation << "  [empty]\n";
        return;
    }

    for (Entry* entry : group->entries()) {
      out << indentation << "  " << entry->title() << " " << entry->uuid().toHex() << "\n";
    }

    for (Group* innerGroup : group->children()) {
        printGroup(innerGroup, groupName, depth + 1);
    }

}

int List::execute(int argc, char **argv)
{


    QApplication app(argc, argv);
    Database* database = UnlockDatabaseDialog::prompt(QString("lol"));
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main",
                                                                 "List database entries."));
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "Path of the database."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        parser.showHelp();
        return EXIT_FAILURE;
    }

    out << "Insert the database password\n> ";
    out.flush();

    static QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QString line = inputTextStream.readLine();
    CompositeKey key = CompositeKey::readFromLine(line);

    Database* db = Database::openDatabaseFile(args.at(0), key);
    if (db == nullptr) {
        return EXIT_FAILURE;
    }

    printGroup(db->rootGroup(), QString(""), 0);
    return EXIT_SUCCESS;
}
