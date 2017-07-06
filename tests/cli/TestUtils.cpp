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

#include "TestUtils.h"

#include <QPointer>
#include <QSignalSpy>
#include <QDebug>
#include <QTest>

#include "crypto/Crypto.h"
#include "cli/Utils.h"

QTEST_GUILESS_MAIN(TestUtils)

void TestUtils::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestUtils::testGetArguments()
{

    QStringList arguments;

    arguments = Utils::getArguments(QString("command arg1 arg2"));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "arg1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("command arg\\ 1 arg2"));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "arg 1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("  command arg\\ 1 arg2     "));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "arg 1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("command arg\\\\1 arg2"));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "arg\\1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("command \"arg1\" arg2"));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "arg1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("command \"ar\\\"g1\" arg2"));
    QVERIFY(arguments.size() == 3);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "ar\"g1");
    QVERIFY(arguments.at(2) == "arg2");

    arguments = Utils::getArguments(QString("command \"only command line argument\"    "));
    QVERIFY(arguments.size() == 2);
    QVERIFY(arguments.at(0) == "command");
    QVERIFY(arguments.at(1) == "only command line argument");

    arguments = Utils::getArguments(QString("command with invalid escaped ch\\aracter   "));
    QVERIFY(arguments.size() == 0);

    arguments = Utils::getArguments(QString("command argument \"unterminated double quote!  "));
    QVERIFY(arguments.size() == 0);

}
