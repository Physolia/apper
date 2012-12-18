/***************************************************************************
 *   Copyright (C) 2008 by Trever Fischer                                  *
 *   wm161@wm161.net                                                       *
 *   Copyright (C) 2008-2011 by Daniel Nicoletti                           *
 *   dantti12@gmail.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; see the file COPYING. If not, write to       *
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,  *
 *   Boston, MA 02110-1301, USA.                                           *
 ***************************************************************************/

#ifndef UPDATE_ICON_H
#define UPDATE_ICON_H

//#include "AbstractIsRunning.h"

#include <QStringList>

#include <Transaction>

using namespace PackageKit;

class StatusNotifierItem;
class UpdateIcon : public QObject
{
    Q_OBJECT
public:
    typedef enum{
        Normal,
        Important,
        Security
    } UpdateType;
    UpdateIcon(QObject *parent = 0);
    ~UpdateIcon();

    void setConfig(const QVariantHash &configs);

signals:
    void watchTransaction(const QDBusObjectPath &tid, bool interactive);

public slots:
    void checkForUpdates(bool system_ready);

private slots:
    void packageToUpdate(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary);
    void getUpdateFinished();
    void autoUpdatesFinished(PackageKit::Transaction::Exit exit);

    void showSettings();
    void showUpdates();
    void removeStatusNotifierItem();

private:
    void updateStatusNotifierIcon(UpdateType type);

    Transaction *m_getUpdatesT;
    StatusNotifierItem *m_statusNotifierItem;
    QStringList m_oldUpdateList;
    QStringList m_updateList;
    QStringList m_importantList;
    QStringList m_securityList;
    QVariantHash m_configs;
};

#endif
