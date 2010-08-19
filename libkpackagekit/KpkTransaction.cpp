/***************************************************************************
 *   Copyright (C) 2008-2010 by Daniel Nicoletti                           *
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

#include "KpkTransaction.h"

#include <KLocale>
#include <KMessageBox>
#include <KPushButton>

#include <KDebug>

#include <QMovie>
#include <QPropertyAnimation>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <QtGui/QTreeView>

#include "KpkMacros.h"
#include "KpkEnum.h"
#include "KpkStrings.h"
#include "KpkRepoSig.h"
#include "KpkLicenseAgreement.h"
#include "KpkIcons.h"
#include "ProgressView.h"

#include "KpkSimulateModel.h"

#include "ui_KpkTransaction.h"

class KpkTransactionPrivate
{
public:
    Ui::KpkTransaction ui;

    QString tid;
    bool showDetails;
    bool finished;
    bool allowDeps;
    bool onlyTrusted;
    Enum::Role role;
    Enum::Error error;
    QString errorDetails;
    QList<QSharedPointer<PackageKit::Package> > packages;
    QStringList files;
    KpkSimulateModel *simulateModel;
    ProgressView *progressView;
};

KpkTransaction::KpkTransaction(Transaction *trans, Behaviors flags, QWidget *parent)
 : KDialog(parent),
   m_trans(trans),
   m_handlingActionRequired(false),
   m_showingError(false),
   m_flags(flags),
   m_exitStatus(Success),
   m_status(Enum::UnknownStatus),
   d(new KpkTransactionPrivate)
{
    d->ui.setupUi(mainWidget());

    d->finished = true; // for sanity we are finished till some transaction is set
    d->onlyTrusted = true; // for sanity we are trusted till an error is given and the user accepts
    d->simulateModel = 0;

    setButtons(KDialog::Details | KDialog::User1 | KDialog::Cancel);
    enableButton(KDialog::Details, false);
    button(KDialog::Details)->setCheckable(true);
    // Setup HIDE custom button
    setButtonText(KDialog::User1, i18n("Hide"));
    setButtonToolTip(KDialog::User1,
                     i18n("Allows you to hide the window whilst keeping the transaction task running."));
    setEscapeButton(KDialog::User1);

    KConfig config("KPackageKit");
    KConfigGroup transactionGroup(&config, "Transaction");

    d->progressView = new ProgressView;

    if (m_flags & Modal) {
        setWindowModality(Qt::WindowModal);
    }

    // We need to track when the user close the dialog using the [X] button
    connect(this, SIGNAL(finished()), SLOT(finishedDialog()));

    // after ALL set, lets set the transaction
    setTransaction(m_trans);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setMaximumSize(QWIDGETSIZE_MAX, size().height());

    KConfigGroup transactionDialog(&config, "TransactionDialog");
    restoreDialogSize(transactionDialog);
}

KpkTransaction::~KpkTransaction()
{
    KConfig config("KPackageKit");
    if (isButtonEnabled(KDialog::Details)) {
        KConfigGroup transactionGroup(&config, "Transaction");
        transactionGroup.writeEntry("ShowDetails", d->showDetails);
    }
    KConfigGroup transactionDialog(&config, "TransactionDialog");
    saveDialogSize(transactionDialog);

    // DO NOT disconnect the transaction here,
    // it might not exist when this happen
    delete d->progressView;
    delete d;
}

void KpkTransaction::slotButtonClicked(int bt)
{
    switch(bt) {
    case KDialog::Cancel :
//         kDebug() << "KDialog::Cancel";
        m_trans->cancel();
        m_flags |= CloseOnFinish;
        break;
    case KDialog::User1 :
//         kDebug() << "KDialog::User1";
        // when we're done finishedDialog() is called
        done(KDialog::User1);
        break;
    case KDialog::Close :
//         kDebug() << "KDialog::Close";
        // Always disconnect BEFORE emitting finished
        unsetTransaction();
        setExitStatus(Cancelled);
        done(KDialog::Close);
        break;
    case KDialog::Details :
    {
        d->showDetails = !d->progressView->isVisible();
        button(KDialog::Details)->setChecked(d->showDetails);
        if (d->progressView->isVisible()) {
            QSize windowSize = size();
            windowSize -= QSize(0, d->progressView->size().height());
            d->progressView->setVisible(false);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
            setMaximumSize(QWIDGETSIZE_MAX, windowSize.height());
            d->ui.gridLayout->removeWidget(d->progressView);
        } else {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            d->ui.gridLayout->addWidget(d->progressView, 1, 0, 1, 2);
            d->progressView->setVisible(true);
        }
    }
        break;
    default : // Should be only details
        KDialog::slotButtonClicked(bt);
    }
}

void KpkTransaction::setTransaction(Transaction *trans)
{
    if (!trans) {
        // 0 pointer passed
        return;
    }

    m_trans = trans;
    if (trans->role() != Enum::RoleInstallSignature &&
        trans->role() != Enum::RoleAcceptEula) {
        // We need to keep the original role for requeuing
        d->role = trans->role();
    }
    d->tid = trans->tid();
    d->finished = false;
    d->error = Enum::UnknownError;
    d->errorDetails.clear();
    d->progressView->clear();

    KConfig config("KPackageKit");
    KConfigGroup transactionGroup(&config, "Transaction");
    // enable the Details button just on these roles
    if (m_trans->role() == Enum::RoleInstallPackages ||
        m_trans->role() == Enum::RoleInstallFiles ||
        m_trans->role() == Enum::RoleRemovePackages ||
        m_trans->role() == Enum::RoleUpdatePackages ||
        m_trans->role() == Enum::RoleUpdateSystem) {
        // DISCONNECT THIS SIGNAL BEFORE SETTING A NEW ONE
        connect(m_trans, SIGNAL(package(QSharedPointer<PackageKit::Package>)),
                d->progressView, SLOT(currentPackage(QSharedPointer<PackageKit::Package>)));
        d->showDetails = transactionGroup.readEntry("ShowDetails", false);
        enableButton(KDialog::Details, true);
        if (d->showDetails != d->progressView->isVisible()) {
            slotButtonClicked(KDialog::Details);
        }
    } else {
        if (m_trans->role() == Enum::RoleSimulateInstallPackages ||
            m_trans->role() == Enum::RoleSimulateInstallFiles ||
            m_trans->role() == Enum::RoleSimulateRemovePackages ||
            m_trans->role() == Enum::RoleSimulateUpdatePackages) {
            // DISCONNECT THIS SIGNAL BEFORE SETTING A NEW ONE
            if (!d->simulateModel) {
                d->simulateModel = new KpkSimulateModel(this, d->packages);
            }
            d->simulateModel->clear();
            connect(m_trans, SIGNAL(package(QSharedPointer<PackageKit::Package>)),
                    d->simulateModel, SLOT(addPackage(QSharedPointer<PackageKit::Package>)));
        }

        if (d->progressView->isVisible()) {
            slotButtonClicked(KDialog::Details);
        }
//         d->ui.gridLayout->removeWidget(d->progressView);
        enableButton(KDialog::Details, false);
    }

    // check to see if we can cancel
    enableButtonCancel(m_trans->allowCancel());

    // sets the action icon to be the window icon
    setWindowIcon(KpkIcons::actionIcon(m_trans->role()));
    // Sets the kind of transaction
    setCaption(KpkStrings::action(m_trans->role()));

    // Now sets the last package
    d->progressView->currentPackage(m_trans->lastPackage());

    // sets ui
    updateUi();

    // DISCONNECT ALL THESE SIGNALS BEFORE SETTING A NEW ONE
    connect(m_trans, SIGNAL(finished(PackageKit::Enum::Exit, uint)),
            this, SLOT(transactionFinished(PackageKit::Enum::Exit)));
    connect(m_trans, SIGNAL(errorCode(PackageKit::Enum::Error, const QString &)),
            this, SLOT(errorCode(PackageKit::Enum::Error, const QString &)));
    connect(m_trans, SIGNAL(changed()),
            this, SLOT(updateUi()));
    connect(m_trans, SIGNAL(eulaRequired(PackageKit::Client::EulaInfo)),
            this, SLOT(eulaRequired(PackageKit::Client::EulaInfo)));
    connect(m_trans, SIGNAL(mediaChangeRequired(PackageKit::Enum::MediaType, const QString &, const QString &)),
            this, SLOT(mediaChangeRequired(PackageKit::Enum::MediaType, const QString &, const QString &)));
    connect(m_trans, SIGNAL(repoSignatureRequired(PackageKit::Client::SignatureInfo)),
            this, SLOT(repoSignatureRequired(PackageKit::Client::SignatureInfo)));
    // DISCONNECT ALL THESE SIGNALS BEFORE SETTING A NEW ONE
}

void KpkTransaction::unsetTransaction()
{
    disconnect(m_trans, SIGNAL(package(QSharedPointer<PackageKit::Package>)),
               this, SLOT(currPackage(QSharedPointer<PackageKit::Package>)));
    disconnect(m_trans, SIGNAL(finished(PackageKit::Enum::Exit, uint)),
               this, SLOT(transactionFinished(PackageKit::Enum::Exit)));
    disconnect(m_trans, SIGNAL(errorCode(PackageKit::Enum::Error, const QString &)),
               this, SLOT(errorCode(PackageKit::Enum::Error, const QString &)));
    disconnect(m_trans, SIGNAL(changed()),
               this, SLOT(updateUi()));
    disconnect(m_trans, SIGNAL(eulaRequired(PackageKit::Client::EulaInfo)),
               this, SLOT(eulaRequired(PackageKit::Client::EulaInfo)));
    disconnect(m_trans, SIGNAL(mediaChangeRequired(PackageKit::Enum::MediaType,
                                                   const QString &, const QString &)),
               this, SLOT(mediaChangeRequired(PackageKit::Enum::MediaType, const QString &, const QString &)));
    disconnect(m_trans, SIGNAL(repoSignatureRequired(PackageKit::Client::SignatureInfo)),
               this, SLOT(repoSignatureRequired(PackageKit::Client::SignatureInfo)));
}

void KpkTransaction::requeueTransaction()
{
    SET_PROXY
    Transaction *trans;
    Client *client = Client::instance();
    switch (d->role) {
    case Enum::RoleRemovePackages :
        trans = client->removePackages(d->packages,
                                       d->allowDeps,
                                       AUTOREMOVE);
        break;
    case Enum::RoleInstallPackages :
        trans = client->installPackages(d->onlyTrusted,
                                        d->packages);
        break;
    case Enum::RoleInstallFiles :
        trans = client->installFiles(d->files,
                                     d->onlyTrusted);
        break;
    case Enum::RoleUpdatePackages :
        trans = client->updatePackages(d->onlyTrusted,
                                       d->packages);
        break;
    default :
        setExitStatus(Failed);
        return;
    }

    if (trans->error()) {
        KMessageBox::sorry(this,
                           KpkStrings::daemonError(trans->error()),
                           KpkStrings::action(trans->role()));
        setExitStatus(Failed);
    } else {
        setTransaction(trans);
    }
}

void KpkTransaction::updateUi()
{
    uint percentage = m_trans->percentage();
    if (percentage <= 100) {
        d->ui.progressBar->setMaximum(100);
        d->ui.progressBar->setValue(percentage);
    } else if (d->ui.progressBar->maximum() != 0) {
        d->ui.progressBar->setMaximum(0);
        d->ui.progressBar->reset();
    }

    d->progressView->setSubProgress(m_trans->subpercentage());
    d->ui.progressBar->setRemaining(m_trans->remainingTime());

    // Status & Speed
    Enum::Status status = m_trans->status();
    if (m_status != status) {
        m_status = status;
        d->ui.currentL->setText(KpkStrings::status(status));

        QMovie *movie;
        // Grab the right icon name
        QString icon(KpkIcons::statusAnimation(status));
        movie = KIconLoader::global()->loadMovie(icon,
                                                KIconLoader::NoGroup,
                                                48,
                                                this);
        if (movie) {
            // If the movie is set we KIconLoader it,
            // set it and start
            d->ui.label->setMovie(movie);
            movie->start();
        } else {
            // Else it's probably a static icon so try to load
            d->ui.label->setPixmap(KpkIcons::getIcon(icon).pixmap(48,48));
        }
    } else if (status == Enum::StatusDownload && m_trans->speed() != 0) {
        uint speed = m_trans->speed();
        if (speed) {
            d->ui.currentL->setText(i18n("Downloading packages at %1/s",
                                         KGlobal::locale()->formatByteSize(speed)));
        }
    }

    // Allow cancel
    enableButtonCancel(m_trans->allowCancel());
}

// Return value: if the error code suggests to try with only_trusted %FALSE
static bool untrustedIsNeed(Enum::Error error)
{
    switch (error) {
    case Enum::ErrorGpgFailure:
    case Enum::ErrorBadGpgSignature:
    case Enum::ErrorMissingGpgSignature:
    case Enum::ErrorCannotInstallRepoUnsigned:
    case Enum::ErrorCannotUpdateRepoUnsigned:
        return true;
    default:
        return false;
    }
}

void KpkTransaction::errorCode(PackageKit::Enum::Error error, const QString &details)
{
//     kDebug() << "errorCode: " << error << details;
    d->error = error;
    d->errorDetails = details;
    // obvious message, don't tell the user
    if (m_handlingActionRequired ||
        error == Enum::ErrorTransactionCancelled ||
        error == Enum::ErrorProcessKill) {
        return;
    }

    if (untrustedIsNeed(error)) {
        m_handlingActionRequired = true;
        int ret = KMessageBox::warningYesNo(this,
                                            i18n("You are about to install unsigned packages that can compromise your system, "
                                            "as it is impossible to verify if the software came from a trusted "
                                            "source.\n\nAre you sure you want to proceed with the installation?"),
                                            i18n("Installing unsigned software"));
        if (ret == KMessageBox::Yes) {
            // Set only trusted to false, to do as the user asked
            d->onlyTrusted = false;
            requeueTransaction();
        } else {
            setExitStatus(Cancelled);
            if (m_flags & CloseOnFinish) {
                done(QDialog::Rejected);
            }
        }
        m_handlingActionRequired = false;
        return;
    }

    // check to see if we are already handlying these errors
    if (error == Enum::ErrorNoLicenseAgreement ||
        error == Enum::ErrorMediaChangeRequired)
    {
        if (m_handlingActionRequired) {
            return;
        }
    }

    m_showingError = true;
    KMessageBox::detailedSorry(this,
                               KpkStrings::errorMessage(error),
                               QString(details).replace('\n', "<br />"),
                               KpkStrings::error(error),
                               KMessageBox::Notify);
    m_showingError = false;

    // when we receive an error we are done
    setExitStatus(Failed);
    // TODO maybe this should go in the above method
    if (m_flags & CloseOnFinish) {
        done(QDialog::Rejected);
    }
}

void KpkTransaction::eulaRequired(PackageKit::Client::EulaInfo info)
{
    if (m_handlingActionRequired) {
        // if its true means that we alread passed here
        m_handlingActionRequired = false;
        return;
    } else {
        m_handlingActionRequired = true;
    }

    QPointer<KpkLicenseAgreement> frm = new KpkLicenseAgreement(info, true, this);
    if (frm->exec() == KDialog::Yes) {
        m_handlingActionRequired = false;
        Transaction *trans = Client::instance()->Client::instance()->acceptEula(info);
        if (trans->error()) {
            KMessageBox::sorry(this,
                               KpkStrings::daemonError(trans->error()),
                               i18n("Failed to accept EULA"));
        } else {
            setTransaction(trans);
        }
    } else {
        setExitStatus(Cancelled);
        m_handlingActionRequired = false;
    }
    delete frm;
}

void KpkTransaction::mediaChangeRequired(PackageKit::Enum::MediaType type, const QString &id, const QString &text)
{
    Q_UNUSED(id)

    m_handlingActionRequired = true;
    int ret = KMessageBox::questionYesNo(this,
                                         KpkStrings::mediaMessage(type, text),
                                         i18n("A media change is required"),
                                         KStandardGuiItem::cont(),
                                         KStandardGuiItem::cancel());
    m_handlingActionRequired = false;

    // if the user clicked continue we got yes
    if (ret == KMessageBox::Yes) {
        requeueTransaction();
    } else {
        setExitStatus(Cancelled);
    }
}

void KpkTransaction::repoSignatureRequired(PackageKit::Client::SignatureInfo info)
{
    if (m_handlingActionRequired) {
        // if its true means that we alread passed here
        m_handlingActionRequired = false;
        return;
    } else {
        m_handlingActionRequired = true;
    }

    QPointer<KpkRepoSig> frm = new KpkRepoSig(info, true, this);
    if (frm->exec() == KDialog::Yes) {
        m_handlingActionRequired = false;
        Transaction *trans = Client::instance()->installSignature(info.type, info.keyId, info.package);
        if (trans->error()) {
            KMessageBox::sorry(this,
                               KpkStrings::daemonError(trans->error()),
                               i18n("Failed to install signature"));
        } else {
            setTransaction(trans);
        }
    } else {
        setExitStatus(Cancelled);
        m_handlingActionRequired = false;
    }
    delete frm;
}

void KpkTransaction::transactionFinished(PackageKit::Enum::Exit status)
{
    Transaction *trans = qobject_cast<Transaction*>(sender());
//     kDebug() << status;
    d->finished = true;
    switch(status) {
    case Enum::ExitSuccess :
        d->ui.progressBar->setMaximum(100);
        d->ui.progressBar->setValue(100);
        if (trans->role() != Enum::RoleInstallSignature &&
            trans->role() != Enum::RoleAcceptEula) {
            setExitStatus(Success);
        } else {
            d->finished = false;
            requeueTransaction();
        }
        break;
    case Enum::ExitCancelled :
        d->ui.progressBar->setMaximum(100);
        d->ui.progressBar->setValue(100);
        // Avoid crash in case we are showing an error
        if (!m_showingError) {
            setExitStatus(Cancelled);
        }
        break;
    case Enum::ExitFailed :
        kDebug() << "Failed.";
        if (!m_handlingActionRequired && !m_showingError) {
            d->ui.progressBar->setMaximum(0);
            d->ui.progressBar->reset();
            kDebug() << "Yep, we failed.";
            setExitStatus(Failed);
        }
        break;
    case Enum::ExitKeyRequired :
    case Enum::ExitEulaRequired :
    case Enum::ExitMediaChangeRequired :
    case Enum::ExitNeedUntrusted :
        kDebug() << "finished KeyRequired or EulaRequired: " << status;
        d->ui.currentL->setText(KpkStrings::status(Enum::StatusSetup));
        if (!m_handlingActionRequired) {
            setExitStatus(Failed);
        }
        break;
    default :
        d->ui.progressBar->setMaximum(100);
        d->ui.progressBar->setValue(100);
        kDebug() << "finished default" << status;
        KDialog::slotButtonClicked(KDialog::Close);
        setExitStatus(Failed);
        break;
    }
    // if we're not showing an error or don't have the
    // CloseOnFinish flag don't close the dialog
    if (m_flags & CloseOnFinish && !m_handlingActionRequired && !m_showingError) {
//         kDebug() << "CloseOnFinish && !m_handlingActionRequired && !m_showingError";
        done(QDialog::Rejected);
        deleteLater();
    }
}

void KpkTransaction::finishedDialog()
{
    if (!d->finished) {
        // We are going to hide the transaction,
        // which can make the user even close System Settings or KPackageKit
        // so we call the tray icon to keep watching the transaction so if the
        // transaction receives some error we can display them
        QDBusMessage message;
        message = QDBusMessage::createMethodCall("org.kde.KPackageKitSmartIcon",
                                                 "/",
                                                 "org.kde.KPackageKitSmartIcon",
                                                 QLatin1String("WatchTransaction"));
        // Use our own cached tid to avoid crashes
        message << qVariantFromValue(d->tid);
        QDBusMessage reply = QDBusConnection::sessionBus().call(message);
        if (reply.type() != QDBusMessage::ReplyMessage) {
            kWarning() << "Message did not receive a reply";
        }
        // Always disconnect BEFORE emitting finished
        unsetTransaction();

        setExitStatus(Success);
    }
}

KpkTransaction::ExitStatus KpkTransaction::exitStatus() const
{
    return m_exitStatus;
}

void KpkTransaction::setExitStatus(KpkTransaction::ExitStatus status)
{
    m_exitStatus = status;
    emit finished(status);
}

QString KpkTransaction::tid() const
{
    return d->tid;
}

bool KpkTransaction::allowDeps() const
{
    return d->allowDeps;
}

bool KpkTransaction::onlyTrusted() const
{
    return d->onlyTrusted;
}

Enum::Role KpkTransaction::role() const
{
    return d->role;
}

Enum::Error KpkTransaction::error() const
{
    return d->error;
}

QString KpkTransaction::errorDetails() const
{
    return d->errorDetails;
}

QList<QSharedPointer<PackageKit::Package> > KpkTransaction::packages() const
{
    return d->packages;
}

QStringList KpkTransaction::files() const
{
    return d->files;
}

KpkSimulateModel* KpkTransaction::simulateModel() const
{
    return d->simulateModel;
}

void KpkTransaction::setAllowDeps(bool allowDeps)
{
    d->allowDeps = allowDeps;
}

void KpkTransaction::setPackages(const QList<QSharedPointer<PackageKit::Package> > &packages)
{
    d->packages = packages;
}

void KpkTransaction::setFiles(const QStringList &files)
{
    d->files = files;
}

#include "KpkTransaction.moc"
