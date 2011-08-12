/***************************************************************************
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

#include "PkInstallGStreamerResources.h"

#include "IntroDialog.h"

#include <KpkStrings.h>

#include <QStandardItemModel>
#include <KLocale>

#include <KDebug>

PkInstallGStreamerResources::PkInstallGStreamerResources(uint xid,
                                                         const QStringList &resources,
                                                         const QString &interaction,
                                                         const QDBusMessage &message,
                                                         QWidget *parent)
    : SessionTask(xid, interaction, message, parent)
{
    m_introDialog = new IntroDialog(this);
    setMainWidget(m_introDialog);
    QStandardItemModel *model = new QStandardItemModel(this);

    bool encoder = false;
    bool decoder = false;
    // Resources are strings like "ID3 tag|gstreamer0.10(decoder-application/x-id3)()(64bit)"
    foreach (const QString &codec, resources) {
        if (codec.contains("|gstreamer0.10(decoder")) {
            decoder = true;
        } else if (codec.contains("|gstreamer0.10(encoder")) {
            encoder = true;
        }

        QStandardItem *item = new QStandardItem(codec.section('|', 0, 0));
        item->setIcon(KIcon("x-kde-nsplugin-generated").pixmap(32, 32));
        model->appendRow(item);

        m_resources << codec.section('|', 1, -1);
    }


    QString description;
    description = i18np("The following plugin is required. "
                        "Do you want to search for this now?",
                        "The following plugins are required. "
                        "Do you want to search for these now?",
                        m_resources.size());

    QString title;
    // this will come from DBus interface
    if (parentTitle.isNull()) {
        if (decoder && !encoder) {
            // TRANSLATORS: a random program which we can't get the name wants to decode something
            title = i18np("A program requires an additional plugin to decode this file",
                          "A program requires additional plugins to decode this file",
                          m_resources.size());
        } else if (!decoder && encoder) {
            // TRANSLATORS: a random program which we can't get the name wants to encode something
            title = i18np("A program requires an additional plugin to encode this file",
                          "A program requires additional plugins to encode this file",
                          m_resources.size());
        } else {
            // TRANSLATORS: a random program which we can't get the name wants to do something (unknown)
            title = i18np("A program requires an additional plugin for this operation",
                          "A program requires additional plugins for this operation",
                          m_resources.size());
        }
    } else {
        if (decoder && !encoder) {
            // TRANSLATORS: a program wants to decode something (unknown) -- string is a program name, e.g. "Movie Player"
            title = i18np("%2 requires an additional plugin to decode this file",
                        "%2 requires additional plugins to decode this file",
                        m_resources.size(),
                        parentTitle);
        } else if (!decoder && encoder) {
            // TRANSLATORS: a program wants to encode something (unknown) -- string is a program name, e.g. "Movie Player"
            title = i18np("%2 requires an additional plugin to encode this file",
                        "%2 requires additional plugins to encode this file",
                        m_resources.size(),
                        parentTitle);
        } else {
            // TRANSLATORS: a program wants to do something (unknown) -- string is a program name, e.g. "Movie Player"
            title = i18np("%2 requires an additional plugin for this operation",
                        "%2 requires additional plugins for this operation",
                        m_resources.size(),
                        parentTitle);
        }
    }

    m_introDialog->setDescription(description);
    setTitle(title);
}

PkInstallGStreamerResources::~PkInstallGStreamerResources()
{
}

void PkInstallGStreamerResources::search()
{
    Transaction *t = new Transaction(this);
    connect(t, SIGNAL(finished(PackageKit::Transaction::Exit, uint)),
            this, SLOT(whatProvidesFinished(PackageKit::Transaction::Exit)));
    connect(t, SIGNAL(package(PackageKit::Package)),
            this, SLOT(addPackage(PackageKit::Package)));
    PkTransaction *trans = new PkTransaction(t, this);
    setMainWidget(trans);
    t->whatProvides(Transaction::ProvidesCodec,
                    m_resources,
                    Transaction::FilterNotInstalled | Transaction::FilterArch | Transaction::FilterNewest);
    if (t->error()) {
        QString msg(i18n("Failed to search for provides"));
        if (showWarning()) {
            setError(msg, KpkStrings::daemonError(t->error()));
        }
        sendErrorFinished(Failed, msg);
    }

}

void PkInstallGStreamerResources::whatProvidesFinished(PackageKit::Transaction::Exit status)
{
    kDebug() << "Finished.";
    if (status == Transaction::ExitSuccess) {
        if (m_foundPackages.size()) {
            ReviewChanges *frm = new ReviewChanges(m_foundPackages, this);
            setMainWidget(frm);
//             if (frm->exec(operationModes()) == 0) {
//                 sendErrorFinished(Failed, "Transaction did not finish with success");
//             } else {
//                 finishTaskOk();
//             }
        } else {
            if (showWarning()) {
                QString mgs = i18n("No results found");
                setInfo(mgs,
                        i18n("Could not find plugin "
                             "in any configured software source"));
            }
            sendErrorFinished(NoPackagesFound, "failed to find codec");
        }
    } else {
        sendErrorFinished(Failed, "what provides failed");
    }
}

void PkInstallGStreamerResources::addPackage(const PackageKit::Package &package)
{
    m_foundPackages.append(package);
}

#include "PkInstallGStreamerResources.moc"
