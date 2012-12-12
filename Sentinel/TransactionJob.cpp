/***************************************************************************
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

#include "TransactionJob.h"

#include <PkStrings.h>
#include <PkIcons.h>

#include <KLocale>
#include <KGlobal>
#include <KNotification>

#include <KDebug>

Q_DECLARE_METATYPE(PackageKit::Transaction::Error)

TransactionJob::TransactionJob(Transaction *transaction, QObject *parent) :
    KJob(parent),
    m_transaction(transaction),
    m_status(Transaction::StatusUnknown),
    m_role(Transaction::RoleUnknown),
    m_percentage(0),
    m_speed(0),
    m_downloadSizeRemainingTotal(0),
    m_finished(false)
{
    setCapabilities(Killable);

    connect(transaction, SIGNAL(changed()), this, SLOT(updateJob()));
    connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
            this, SLOT(finished(PackageKit::Transaction::Exit)));
    connect(transaction, SIGNAL(destroyed()),
            this, SLOT(transactionDestroyed()));
    connect(transaction, SIGNAL(package(PackageKit::Transaction::Info,QString,QString)),
            this, SLOT(package(PackageKit::Transaction::Info,QString,QString)));
    connect(transaction, SIGNAL(repoDetail(QString,QString,bool)),
            this, SLOT(repoDetail(QString,QString)));
}

TransactionJob::~TransactionJob()
{
}

void TransactionJob::transactionDestroyed()
{
    finished(Transaction::ExitCancelled);
}

void TransactionJob::finished(PackageKit::Transaction::Exit exit)
{
    // emit the description so the Speed: xxx KiB/s
    // don't get confused to a destination URL
    emit description(this, PkStrings::action(m_role));
    if (exit == Transaction::ExitCancelled) {
        setError(KilledJobError);
    }
    m_finished = true;
    emitResult();
}

void TransactionJob::package(Transaction::Info info, const QString &packageID, const QString &summary)
{
    Q_UNUSED(summary)
    if (!packageID.isEmpty()) {
        bool changed = false;
        if (info == Transaction::InfoFinished) {
            changed = m_packages.removeOne(Transaction::packageName(packageID));
        } else if (!m_packages.contains(Transaction::packageName(packageID))) {
            m_packages << Transaction::packageName(packageID);
            changed = true;
        }

        if (changed) {
            m_details = m_packages.join(QLatin1String(", "));
            emitDescription();
        }
    }
}

void TransactionJob::repoDetail(const QString &repoId, const QString &repoDescription)
{
    Q_UNUSED(repoId)
    QString first = PkStrings::status(m_status);
    emit description(this, PkStrings::action(m_role), qMakePair(first, repoDescription));
}

void TransactionJob::emitDescription()
{
    QString details = m_details;
    if (details.isEmpty()) {
        details = QLatin1String("...");
    }

    QString first = PkStrings::status(m_status);
    emit description(this, PkStrings::action(m_role), qMakePair(first, details));
}

void TransactionJob::updateJob()
{
    Transaction::Role role = m_transaction->role();
    if (m_role != role) {
        m_role = role;
        emitDescription();
    }

    // Status & Speed
    Transaction::Status status = m_transaction->status();
    if (m_status != status) {
        m_status = status;
        emitDescription();
    }

    uint percentage = m_transaction->percentage();
    if (percentage <= 100) {
        emitPercent(percentage, 100);
    } else if (m_percentage != 0) {
        percentage = 0;
        emitPercent(0, 0);
    }
    m_percentage = percentage;

    uint speed = m_transaction->speed();
    if (m_speed != speed) {
        m_speed = speed;
        emitSpeed(m_speed);
    }

    if (m_downloadSizeRemainingTotal == 0) {
        m_downloadSizeRemainingTotal = m_transaction->downloadSizeRemaining();
    }

    if (m_downloadSizeRemainingTotal) {
        qulonglong processed;
        processed = m_downloadSizeRemainingTotal - m_transaction->downloadSizeRemaining();
        emitPercent(processed, m_downloadSizeRemainingTotal);
    }
}

void TransactionJob::start()
{
    m_role = Transaction::RoleUnknown;
    m_speed = 0;
    m_downloadSizeRemainingTotal = 0;
    m_details = Transaction::packageName(m_transaction->lastPackage());
    updateJob();
}

bool TransactionJob::isFinished() const
{
    return m_finished;
}

Transaction *TransactionJob::transaction() const
{
    return m_transaction;
}

bool TransactionJob::doKill()
{
    // emit the description so the Speed: xxx KiB/s
    // don't get confused to a destination URL
    emit description(this, PkStrings::action(m_role));
    m_transaction->cancel();
    emit canceled();

    return m_transaction->role() == Transaction::RoleCancel;
}

#include "TransactionJob.moc"
