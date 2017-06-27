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

#include "Create.h"

#include <QApplication>
#include <QFile>
#include <QCommandLineParser>
#include <QStringList>
#include <QTextStream>
#include "keys/PasswordKey.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "cli/Utils.h"

Create::Create()
{
    this->name = QString("create");
    this->description = QString("Create a new database.");
}

Create::~Create()
{
}

int Create::execute(int argc, char** argv)
{

    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << QString(argv[i]);
    }
    QTextStream outputTextStream(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Create a new database"));
    parser.addPositionalArgument("path", QCoreApplication::translate("main", "Path of the new database."));
    parser.addPositionalArgument("name", QCoreApplication::translate("main", "Name of the new database."));
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        QCoreApplication app(argc, argv);
        parser.showHelp(EXIT_FAILURE);
    }

    QString databasePath = args.at(0);
    QString databaseName = args.at(1);

    if (QFile::exists(databasePath)) {
        qCritical("File %s already exists!", qPrintable(databasePath));
        return EXIT_FAILURE;
    }

    Database* newDb = new Database();

    outputTextStream << "master password: ";
    outputTextStream.flush();
    QString password = Utils::getPassword();

    outputTextStream << "confirm password: ";
    outputTextStream.flush();
    QString passwordConfirmation = Utils::getPassword();

    if (password != passwordConfirmation) {
        qCritical("Passwords do not match.");
        return EXIT_FAILURE;
    }

    CompositeKey key;
    key.addKey(PasswordKey(password));

    newDb->setKey(key);
    newDb->metadata()->setName(databaseName);
    newDb->setTransformRounds(CompositeKey::transformKeyBenchmark(2000));
    newDb->rootGroup()->setName("root");

    newDb->saveToFile(databasePath);

    outputTextStream << "Successfully created new KeePassXC database.\n";
    outputTextStream.flush();
    return EXIT_SUCCESS;
}
