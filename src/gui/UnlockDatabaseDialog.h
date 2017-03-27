/*
 *  Copyright (C) 2016 KeePassXC Team
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

#ifndef KEEPASSX_UNLOCKDATABASEDIALOG_H
#define KEEPASSX_UNLOCKDATABASEDIALOG_H

#include <QDialog>

#include "core/Global.h"

class UnlockDatabaseWidget;
class Database;

class UnlockDatabaseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UnlockDatabaseDialog(QWidget *parent = Q_NULLPTR);
    void setDBFilename(const QString& filename);
    void clearForms();
    Database* database();

signals:
    void unlockDone(bool);

public slots:
    void complete(bool r);

private:
    UnlockDatabaseWidget* const m_view;
};

#endif // KEEPASSX_UNLOCKDATABASEDIALOG_H
