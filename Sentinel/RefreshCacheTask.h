/***************************************************************************
 *   Copyright (C) 2012 by Daniel Nicoletti dantti12@gmail.com             *
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

#ifndef REFRESHCACHETASK_H
#define REFRESHCACHETASK_H

#include "AbstractIsRunning.h"

#include <Transaction>

#include <KNotification>

class RefreshCacheTask : public AbstractIsRunning
{
    Q_OBJECT
public:
    explicit RefreshCacheTask(QObject *parent = 0);

public slots:
    void refreshCache();

private slots:
    void refreshCacheFinished(PackageKit::Transaction::Exit status);
    void errorCode(PackageKit::Transaction::Error error, const QString &errorMessage);
    void notificationClosed();

private:
    KNotification *m_notification;
};

#endif // REFRESHCACHETASK_H
