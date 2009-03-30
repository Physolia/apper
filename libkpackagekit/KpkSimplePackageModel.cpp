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
 *   along with this program; see the file COPYING. If not, write to       *
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,  *
 *   Boston, MA 02110-1301, USA.                                           *
 ***************************************************************************/

#include "KpkSimplePackageModel.h"

#include <KDebug>
#include <KpkIcons.h>

using namespace PackageKit;

KpkSimplePackageModel::KpkSimplePackageModel(QObject *parent)
: QStandardItemModel(parent)
{
    setSortRole(Qt::DisplayRole);
}

void KpkSimplePackageModel::addPackage(PackageKit::Package *p)
{
    QStandardItem *item = new QStandardItem;
    item->setText(p->name() + " - " + p->version() + (p->arch().isNull() ? NULL : " (" + p->arch() + ')'));
    item->setIcon(KpkIcons::packageIcon(p->state()));
    item->setSelectable(false);
    item->setEditable(false);
    item->setToolTip(p->summary());
    appendRow(item);
}
