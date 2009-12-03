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

#include "PkInstallProvideFiles.h"

#include <KpkReviewChanges.h>
#include <KpkStrings.h>

#include <KLocale>
#include <KMessageBox>

#include <KDebug>

PkInstallProvideFiles::PkInstallProvideFiles(uint xid,
                                             const QStringList &files,
                                             const QString &interaction,
                                             const QDBusMessage &message,
                                             QWidget *parent)
 : KpkAbstractTask(xid, interaction, message, parent),
   m_args(files)
{
}

PkInstallProvideFiles::~PkInstallProvideFiles()
{
}

void PkInstallProvideFiles::start()
{
    int ret = KMessageBox::Yes;
    if (showConfirmSearch()) {
        QString message = i18np("The following file is required: <ul><li>%2</li></ul>"
                                "Do you want to search for this now?",
                                "The following files are required: <ul><li>%2</li></ul>"
                                "Do you want to search for these now?",
                                m_args.size(),
                                m_args.join("</li><li>"));
        QString title;
        // this will come from DBus interface
        if (parentTitle.isNull()) {
            title = i18np("A program wants to install a file",
                          "A program wants to install files",
                          m_args.size());
        } else {
            title = i18np("%2 wants to install a file",
                          "%2 wants to install files",
                          m_args.size(),
                          parentTitle);
        }

        QString msg = "<h3>" + title + "</h3>" + message;
        KGuiItem searchBt = KStandardGuiItem::yes();
        searchBt.setText(i18nc("Search for a package that provides these files and install it", "Install"));
        searchBt.setIcon(KIcon::KIcon("edit-find"));
        ret = KMessageBox::questionYesNo(0,
                                         msg,
                                         title,
                                         searchBt);
    }

    if (ret == KMessageBox::Yes) {
        Transaction *t = Client::instance()->searchFile(m_args.first(),
                                                        Client::FilterArch |
                                                        Client::FilterNewest);
        if (t->error()) {
            if (showWarning()) {
                KMessageBox::sorry(0,
                                   KpkStrings::daemonError(t->error()),
                                   i18n("Failed to start search file transaction"));
            }
            sendErrorFinished(Failed, "Failed to start search file transaction");
        } else {
            connect(t, SIGNAL(finished(PackageKit::Transaction::ExitStatus, uint)),
                    this, SLOT(searchFinished(PackageKit::Transaction::ExitStatus, uint)));
            connect(t, SIGNAL(package(PackageKit::Package *)),
                    this, SLOT(addPackage(PackageKit::Package *)));
            if (showProgress()) {
                KpkTransaction *trans;
                trans = new KpkTransaction(t,
                                           KpkTransaction::CloseOnFinish);
                trans->show();
            }
        }
    } else {
        sendErrorFinished(Cancelled, i18n("did not agree to search"));
    }
}

void PkInstallProvideFiles::searchFinished(PackageKit::Transaction::ExitStatus status, uint runtime)
{
    Q_UNUSED(runtime)
    if (status == Transaction::ExitSuccess) {
        if (m_alreadyInstalled.size()) {
            if (showWarning()) {
                KMessageBox::sorry(0,
                                   i18n("The %1 package already provides this file",
                                        m_alreadyInstalled),
                                   i18n("Failed to install file"));
            }
            sendErrorFinished(Failed, "already provided");
        } else if (m_foundPackages.size()) {
            KpkReviewChanges *frm = new KpkReviewChanges(m_foundPackages);
            frm->setTitle(i18np("The following package will be installed",
                                "The following packages will be installed",
                                m_foundPackages.size()));
            if (frm->exec(operationModes()) == 0) {
                sendErrorFinished(Failed, "Transaction did not finish with success");
            } else {
                finishTaskOk();
            }
        } else {
            if (showWarning()) {
                KMessageBox::sorry(0,
                                   i18np("The file could not be found in any packages",
                                         "The files could not be found in any packages",
                                         m_args.size()),
                                   i18n("Failed to find package"));
            }
            sendErrorFinished(NoPackagesFound, "no files found");
        }
    } else {
        sendErrorFinished(Failed, "failed to resolve");
    }
}

void PkInstallProvideFiles::addPackage(PackageKit::Package *package)
{
    if (package->state() != Package::StateInstalled) {
        m_foundPackages.append(package);
    } else {
        m_alreadyInstalled = package->name();
    }
}

#include "PkInstallProvideFiles.moc"
