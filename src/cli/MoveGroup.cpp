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

#include <cstdlib>
#include <stdio.h>

#include "MoveGroup.h"

#include "cli/TextStream.h"
#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"

MoveGroup::MoveGroup()
{
    name = QString("mv");
    description = QObject::tr("Move or rename a group.");
    positionalArguments.append({QString("group"), QObject::tr("Path of the group to move."), QString("")});
    positionalArguments.append({QString("target"), QObject::tr("Target name or path for the group."), QString("")});
}

MoveGroup::~MoveGroup()
{
}

int MoveGroup::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    TextStream outputTextStream(Utils::STDOUT, QIODevice::WriteOnly);
    TextStream errorTextStream(Utils::STDERR, QIODevice::WriteOnly);

    const QStringList args = parser->positionalArguments();
    const QString& groupPath = args.at(1);
    const QString& targetPath = args.at(2);

    Group* group = database->rootGroup()->findGroupByPath(groupPath);
    if (!group) {
        errorTextStream << QObject::tr("Could not find group with path %1.").arg(groupPath) << endl;
        return EXIT_FAILURE;
    }

    Group* destinationGroup = database->rootGroup()->findGroupByPath(targetPath);
    if (!destinationGroup) {
        // TODO handle possibility of renaming operation
        errorTextStream << QObject::tr("Could not find group with path %1.").arg(targetPath) << endl;
        return EXIT_FAILURE;
    }

    if (destinationGroup == group->parent()) {
        errorTextStream << QObject::tr("group is already in %1.").arg(targetPath) << endl;
        return EXIT_FAILURE;
    }

    //entry->beginUpdate();
    group->setParent(destinationGroup);
    //entry->endUpdate();

    QString errorMessage;
    if (!database->save(&errorMessage, true, false)) {
        errorTextStream << QObject::tr("Writing the database failed %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    outputTextStream << QObject::tr("Successfully moved group %1 to group %2.").arg(group->name(), targetPath)
                     << endl;
    return EXIT_SUCCESS;
}
