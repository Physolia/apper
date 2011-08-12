/**************************************************************************
*   Copyright (C) 2009-2011 by Daniel Nicoletti                           *
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

#include "PkInstallPrinterDrivers.h"

#include <KpkStrings.h>

#include <KLocale>

#include <KDebug>

#include <QTextStream>

PkInstallPrinterDrivers::PkInstallPrinterDrivers(uint xid,
                                                 const QStringList &resources,
                                                 const QString &interaction,
                                                 const QDBusMessage &message,
                                                 QWidget *parent) :
    SessionTask(xid, interaction, message, parent),
    m_resources(resources)
{
    // TODO confirm operation
    QStringList search;
    foreach (const QString &deviceid, m_resources) {
        QString mfg, mdl;
        QStringList fields = deviceid.split(';');
        foreach (const QString &field, fields) {
            QString keyvalue = field.trimmed();
            if (keyvalue.startsWith(QLatin1String("MFG:"))) {
                mfg = keyvalue.mid(4);
            } else if (keyvalue.startsWith(QLatin1String("MDL:"))) {
                mdl = keyvalue.mid(4);
            }
        }

        if (!mfg.isEmpty() && !mdl.isEmpty()) {
            QString prov;
            QTextStream out(&prov);
            out << mfg.toLower().replace(' ', '_') << ';'
                << mdl.toLower().replace(' ', '_') << ';';
            search << prov;
        }
    }

    Transaction *t = new Transaction(this);
    connect(t, SIGNAL(finished(PackageKit::Transaction::Exit, uint)),
            this, SLOT(whatProvidesFinished(PackageKit::Transaction::Exit)));
    connect(t, SIGNAL(package(PackageKit::Package)),
               this, SLOT(addPackage(PackageKit::Package)));
    PkTransaction *trans = new PkTransaction(t, this);
    setMainWidget(trans);
    t->whatProvides(Transaction::ProvidesPostscriptDriver,
                    search,
                    Transaction::FilterNotInstalled | Transaction::FilterArch |  Transaction::FilterNewest);

    if (t->error()) {
        QString msg(i18n("Failed to search for provides"));
        setError(msg, KpkStrings::daemonError(t->error()));
        sendErrorFinished(InternalError, msg);
    }
}

PkInstallPrinterDrivers::~PkInstallPrinterDrivers()
{
}

void PkInstallPrinterDrivers::whatProvidesFinished(PackageKit::Transaction::Exit status)
{
    kDebug() << "Finished.";
    if (status == Transaction::ExitSuccess) {
        if (m_foundPackages.size()) {
            ReviewChanges *frm = new ReviewChanges(m_foundPackages, this);
            setMainWidget(frm);
            setTitle(frm->title());
//            if (frm->exec(operationModes()) == 0) {
//                sendErrorFinished(Failed, i18n("Transaction did not finish with success"));
//            } else {
//                finishTaskOk();
//            }
        } else {
            if (showWarning()) {
                setInfo(i18n("Failed to search for printer driver"),
                        i18n("Could not find printer driver "
                             "in any configured software source"));
            }
            sendErrorFinished(NoPackagesFound, "failed to find printer driver");
        }
    } else {
        sendErrorFinished(Failed, "what provides failed");
    }
}

void PkInstallPrinterDrivers::addPackage(const PackageKit::Package &package)
{
    m_foundPackages.append(package);
}

#include "PkInstallPrinterDrivers.moc"
