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

#include "PkInstallMimeTypes.h"

#include <KpkReviewChanges.h>
#include <KpkStrings.h>

#include <KLocale>
#include <KMessageBox>

#include <KDebug>

PkInstallMimeTypes::PkInstallMimeTypes(uint xid,
                                     const QStringList &mime_types,
                                     const QString &interaction,
                                     const QDBusMessage &message,
                                     QWidget *parent)
 : KpkAbstractTask(xid, interaction, message, parent),
   m_mimeTypes(mime_types)
{
}

PkInstallMimeTypes::~PkInstallMimeTypes()
{
}

void PkInstallMimeTypes::start()
{
    kDebug() << m_mimeTypes.first();
    int ret = KMessageBox::Yes;
    if (showConfirmSearch()) {
        QString message = i18n("An additional program is required to open this type of file:"
                            "<ul><li>%1</li></ul>"
                            "Do you want to search for a program to open this file type now?",
                            m_mimeTypes.first());
        QString title;
        // this will come from DBus interface
        if (parentTitle.isNull()) {
            title = i18np("A program requires a new mime type",
                        "A program requires new mime types",
                        m_mimeTypes.size());
        } else {
            title = i18np("%1 requires a new mime type",
                        "%1 requires new mime types",
                        parentTitle,
                        m_mimeTypes.size());
        }
        QString msg = "<h3>" + title + "</h3>" + message;
        KGuiItem searchBt = KStandardGuiItem::yes();
        searchBt.setText(i18nc("Search for a new mime type" ,"Search"));
        searchBt.setIcon(KIcon::KIcon("edit-find"));
        ret = KMessageBox::questionYesNo(0,
                                        msg,
                                        title,
                                        searchBt);
    }

    if (ret == KMessageBox::Yes) {
        Transaction *t = Client::instance()->whatProvides(Client::ProvidesMimetype,
                                                          m_mimeTypes.first(),
                                                          Client::FilterNotInstalled |
                                                          Client::FilterArch |
                                                          Client::FilterNewest);
        if (t->error()) {
            if (showWarning()) {
                KMessageBox::sorry(0,
                                   KpkStrings::daemonError(t->error()),
                                   i18n("Failed to search for provides"));
            }
            sendErrorFinished(Failed, "Failed to search for provides");
        } else {
            connect(t, SIGNAL(finished(PackageKit::Transaction::ExitStatus, uint)),
                    this, SLOT(whatProvidesFinished(PackageKit::Transaction::ExitStatus, uint)));
            connect(t, SIGNAL(package(PackageKit::Package *)),
                    this, SLOT(addPackage(PackageKit::Package *)));
            if (showProgress()) {
                KpkTransaction *trans = new KpkTransaction(t, KpkTransaction::CloseOnFinish);
                trans->show();
            }
        }
    } else {
        sendErrorFinished(Cancelled, i18n("did not agree to search"));
    }
}

void PkInstallMimeTypes::whatProvidesFinished(PackageKit::Transaction::ExitStatus status, uint runtime)
{
    Q_UNUSED(runtime)
    kDebug() << "Finished.";
    if (status == Transaction::ExitSuccess) {
        if (m_foundPackages.size()) {
            KpkReviewChanges *frm = new KpkReviewChanges(m_foundPackages);
            frm->setTitle(i18np("Application that can open this type of file",
                                "Applications that can open this type of file", m_foundPackages.size()));
            if (frm->exec(operationModes()) == 0) {
                sendErrorFinished(Failed, i18n("Transaction did not finish with success"));
            } else {
                finishTaskOk();
            }
        } else {
            if (showWarning()) {
                KMessageBox::sorry(0,
                                   i18n("No new applications can be found "
                                        "to handle this type of file"),
                                   i18n("Failed to find software"));
            }
            sendErrorFinished(NoPackagesFound, "nothing was found to handle mime type");
        }
    } else {
        sendErrorFinished(Failed, "what provides failed");
    }
}

void PkInstallMimeTypes::addPackage(PackageKit::Package *package)
{
    m_foundPackages.append(package);
}

#include "PkInstallMimeTypes.moc"