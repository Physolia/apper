/***************************************************************************
 *   Copyright (C) 2009 by Daniel Nicoletti                                *
 *   dantti85-pk@yahoo.com.br                                              *
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
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef KPK_DISTRO_UPGRADE_H
#define KPK_DISTRO_UPGRADE_H

#include "KpkAbstractSmartIcon.h"

#include <QPackageKit>

using namespace PackageKit;

class KpkDistroUpgrade : public KpkAbstractSmartIcon
{
Q_OBJECT
public:
    KpkDistroUpgrade(QObject *parent = 0);
    ~KpkDistroUpgrade();

    void checkDistroUpgrades();

private slots:
    void distroUpgrade(PackageKit::Client::UpgradeType type, const QString &name, const QString &description);
    void handleDistroUpgradeAction(uint action);
    void distroUpgradeError(QProcess::ProcessError error);
    void distroUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleDistroUpgradeActionClosed();

private:
    QProcess *m_distroUpgradeProcess;
};

#endif