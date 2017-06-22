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
#include <string.h>

#include "Shell.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QClipboard>
#include <QStringList>
#include <QTextStream>

#include "gui/UnlockDatabaseDialog.h"
#include "core/Database.h"
#include "core/Metadata.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "gui/Clipboard.h"
#include "keys/CompositeKey.h"
#include "config-keepassx.h"
#include <readline/readline.h>
#include <readline/history.h>

char* commandNames[4] = {
  const_cast<char*>("show"),
  const_cast<char*>("quit"),
  const_cast<char*>("save")
};

//Each string the generator function returns as a match must be allocated with malloc();
char* commandNameCompletion(const char* text, int state)
{
  static int list_index, len;
  char* name;

  /* If this is a new word to complete, initialize now.  This
     includes saving the length of TEXT for efficiency, and
     initializing the index variable to 0. */
  if (state == 0) {
    list_index = 0;
    len = strlen (text);
  }

  /* Return the next name which partially matches from the
     command list. */
  while (name = commandNames[list_index]) {
      list_index++;

      // Each string the generator function returns as a match
      // must be allocated with malloc();
      if (strncmp (name, text, len) == 0) {
        char* response = (char*)malloc(strlen(name));
        response = strcpy(response, name);
        return response;
      }
  }

  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

char** keepassxc_completion (const char* text, int start, int end)
{
  char** matches;

  matches = (char **)NULL;

  QString line = QString::fromUtf8(text);
  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0) {
    matches = rl_completion_matches (text, commandNameCompletion);
  }

  return (matches);
}

int Shell::execute(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Copy a password to the clipboard"));
    parser.addPositionalArgument("database", QCoreApplication::translate("main", "Path of the database."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        parser.showHelp(EXIT_FAILURE);
    }

    static QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    bool databaseModified = false;

    Database* db = UnlockDatabaseDialog::openDatabasePrompt(args.at(0));
    if (!db) {
        return EXIT_FAILURE;
    }

    out << "KeePassXC " << KEEPASSX_VERSION << " interactive shell\n";
    if (!db->metadata()->name().isNull()) {
        out << "Using database " << db->metadata()->name() << '\n';
    } else {
        out << "Using database " << args.at(0) << '\n';
    }
    out.flush();

    rl_read_init_file("~/.inputrc");
    rl_readline_name = "KeePassXC";
    rl_attempted_completion_function = keepassxc_completion;

    while (true) {

      char* chars;
      if (databaseModified) {
          chars = readline("KeePassXC*> ");
      } else {
          chars = readline("KeePassXC> ");
      }

      QString line(chars);
      line = line.trimmed();

      if (line.isEmpty()) {
          continue;
      }

      add_history(chars);

      QString commandName = line.split(" ").at(0);
      if (line.startsWith(QString("clip"))) {
          QString entryId = line.split(" ").at(1);
          Entry* entry = db->rootGroup()->findEntry(entryId);
          if (!entry) {
              qCritical("Entry %s not found.", qPrintable(entryId));
              continue;
          }
          QApplication::clipboard()->setText(entry->password());
          QString copiedMessage("Password for " + entryId + " copied to clipboard!\n");
          out << qPrintable(copiedMessage);
          out.flush();
          continue;
      }
      if (line.startsWith(QString("ls"))) {
          out << db->rootGroup()->print();
          out.flush();
          continue;
      }
      if (line.startsWith(QString("rm"))) {
          QStringList arguments = line.split(" ");
          if (arguments.length() != 2) {
              out << "Usage: rm entry\n";
              out.flush();
              continue;
          }
          QString entryName = arguments.at(1);
          Entry* entry = db->rootGroup()->findEntry(entryName);
          if (!entry) {
              qCritical("Entry %s not found.", qPrintable(entryName));
              continue;
          }
          QString entryTitle = entry->title();
          // TODO send to recycle bin instead!
          delete entry;
          databaseModified = true;
          out << "Successfully removed entry " << entryTitle << ".\n";
          out.flush();
          continue;
      }
      if (commandName == QString("mv")) {
          QStringList arguments = line.split(" ");
          if (arguments.length() != 3) {
              out << "Usage: move entry group\n";
              out.flush();
              continue;
          }
          QString entryId = arguments.at(1);
          QString groupId = arguments.at(2);
          Entry* entry = db->rootGroup()->findEntry(entryId);
          if (!entry) {
              qCritical("Entry %s not found.", qPrintable(entryId));
              continue;
          }
          // Group* group = db->rootGroup()->findChild(groupId);
          Group* group = nullptr;
          if (!group) {
              qCritical("Group %s not found.", qPrintable(groupId));
              continue;
          }
          entry->setGroup(group);
          databaseModified = true;
          out << QString(entry->title() + " moved to " + group->name() + "\n");
          out.flush();
          continue;
      }
      if (commandName == QString("mkdir")) {
          QStringList arguments = line.split(" ");
          if (arguments.length() != 2) {
              out << "Usage: move entry group\n";
              out.flush();
              continue;
          }
          QString groupName = arguments.at(1);

          if (groupName.endsWith("/")) {
              groupName.chop(1);
          }

          QStringList path = groupName.split("/");
          if (path.length() == 1) {
              Group* newGroup = new Group();
              newGroup->setName(path.at(0));
              newGroup->setParent(db->rootGroup());
              out << QString("Created group " + newGroup->name() + "\n");
              out.flush();
              databaseModified = true;
              continue;
          }

          QString newEntryName = path.takeLast();

          // Group* parentGroup = db->rootGroup()->findChildByPath(path.join("/"));
          Group* parentGroup = nullptr;
          if (!parentGroup) {
              qCritical("Group %s not found.", qPrintable(path.join("/")));
              continue;
          }

          Group* newGroup = new Group();
          newGroup->setName(newEntryName);
          newGroup->setParent(parentGroup);
          out << QString("Created group " + newGroup->name() + "\n");
          out.flush();
          databaseModified = true;
          continue;

      }
      if (line == QString("quit")) {
          if (databaseModified) {
              char * chars = readline("The database was modified, do you want to save it? [y/n] ");
              if (QString(chars) == QString("y")) {
                  QString errorMessage = db->saveToFile(args.at(0));
                  if (errorMessage.isEmpty()) {
                      databaseModified = false;
                      out << QString("Successfully saved database " + args.at(0) + "\n");
                  } else {
                      out << errorMessage;
                  }
                  out.flush();
              }

          }
          break;
      }
      if (line == QString("save")) {
          QString errorMessage = db->saveToFile(args.at(0));
          if (errorMessage.isEmpty()) {
              databaseModified = false;
              out << QString("Successfully saved database " + args.at(0));
          } else {
              out << errorMessage;
          }
          continue;
      }
      out << QString("Invalid command '" + commandName + "'\n");
      out.flush();
    }

    return EXIT_SUCCESS;
}
