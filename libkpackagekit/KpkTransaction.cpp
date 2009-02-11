/***************************************************************************
 *   Copyright (C) 2008 by Daniel Nicoletti                                *
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

#include <KLocale>
#include <KMessageBox>

#include <KDebug>

#include "KpkTransaction.h"
#include "KpkStrings.h"
#include "KpkRepoSig.h"
#include "KpkLicenseAgreement.h"
#include "KpkIcons.h"

#include "ui_KpkTransaction.h"

class KpkTransactionPrivate
{
public:
    Ui::KpkTransaction ui;
};

static const int stateCount = 7;

KpkTransaction::KpkTransaction( Transaction *trans, Behaviors flags, QWidget *parent )
 : KDialog(parent),
   m_trans(trans),
   m_handlingGpgOrEula(false),
   m_flags(flags),
   d(new KpkTransactionPrivate)
{
    d->ui.setupUi( mainWidget() );

    // Set Cancel and custom button hide
    setButtons( KDialog::Cancel | KDialog::User1 | KDialog::Details );
    setButtonText( KDialog::User1, i18n("Hide") );
    setButtonToolTip( KDialog::User1, i18n("Allows you to hide the window but keeps running transaction task") );
    setEscapeButton( KDialog::User1 );
    enableButtonCancel(false);
    setDetailsWidget(d->ui.detailGroup);
    setDetailsWidgetVisible(false);
    setTransaction(m_trans);
    enableButton(KDialog::Details, false);
    
    if (m_flags & Modal) {
        setWindowModality(Qt::WindowModal);
        enableButton(KDialog::User1, false);
    }
}

KpkTransaction::~KpkTransaction()
{
    delete d;
}

void KpkTransaction::setTransaction(Transaction *trans)
{
    m_trans = trans;

    setWindowIcon( KpkIcons::actionIcon( m_trans->role().action ) );
    // Sets all the status of the current transaction
    setCaption( KpkStrings::action( m_trans->role().action ) );

    enableButtonCancel( m_trans->allowCancel() );

    d->ui.currentL->setText( KpkStrings::status( m_trans->status() ) );
    
    progressChanged(m_trans->progress());
    currPackage(m_trans->lastPackage());
    statusChanged(m_trans->status());

    connect( m_trans, SIGNAL( package(PackageKit::Package *) ),
	this, SLOT( currPackage(PackageKit::Package *) ) );
    connect( m_trans, SIGNAL( finished(PackageKit::Transaction::ExitStatus, uint) ),
	this, SLOT( finished(PackageKit::Transaction::ExitStatus, uint) ) );
    connect( m_trans, SIGNAL( allowCancelChanged(bool) ),
	this, SLOT( enableButtonCancel(bool) ) );
    connect( m_trans, SIGNAL( errorCode(PackageKit::Client::ErrorType, const QString&) ),
	this, SLOT( errorCode(PackageKit::Client::ErrorType, const QString&) ) );
    connect( m_trans, SIGNAL( progressChanged(PackageKit::Transaction::ProgressInfo) ),
	this, SLOT( progressChanged(PackageKit::Transaction::ProgressInfo) ) );
    connect( m_trans, SIGNAL( statusChanged(PackageKit::Transaction::Status) ),
	this, SLOT( statusChanged(PackageKit::Transaction::Status) ) );
    connect( m_trans, SIGNAL( eulaRequired(PackageKit::Client::EulaInfo) ),
	this, SLOT( eulaRequired(PackageKit::Client::EulaInfo) ) );
    connect( m_trans, SIGNAL( repoSignatureRequired(PackageKit::Client::SignatureInfo) ),
	this, SLOT( repoSignatureRequired(PackageKit::Client::SignatureInfo) ) );
}

void KpkTransaction::progressChanged(PackageKit::Transaction::ProgressInfo info)
{
    if (info.percentage) {
        d->ui.progressBar->setMaximum(100);
        d->ui.progressBar->setValue(info.percentage);
    } else {
        d->ui.progressBar->setMaximum(0);
        d->ui.progressBar->reset();
    }
    if (info.subpercentage) {
        d->ui.subprogressBar->setMaximum(100);
        d->ui.subprogressBar->setValue(info.subpercentage);
    } else {
        d->ui.subprogressBar->setMaximum(0);
        d->ui.subprogressBar->reset();
    }
    if (info.remaining) {
        d->ui.timeL->setText(i18n("%1 remaining",KGlobal::locale()->formatDuration(info.remaining*1000)));
    } else {
        d->ui.timeL->setText("");
    }
}

void KpkTransaction::currPackage(Package *p)
{
    if (p->name() != "") {
        QString packageText(p->name());
        if (p->version() != "")
            packageText+=" "+p->version();
        d->ui.packageL->setText( packageText );
        d->ui.descriptionL->setText( p->summary() );
        enableButton(KDialog::Details, true);
    } else {
        d->ui.packageL->setText("");
        d->ui.descriptionL->setText("");
        enableButton(KDialog::Details, false);
    }
}

void KpkTransaction::slotButtonClicked(int button)
{
    switch(button) {
        case KDialog::Cancel :
            kDebug() << "KDialog::Cancel";
            m_trans->cancel();
            m_flags |= CloseOnFinish;
            break;
        case KDialog::User1 :
            kDebug() << "KDialog::User1";
            emit kTransactionFinished(Success);
            // If you call Close it will
            // come back to hunt you with Cancel
            done(KDialog::User1);
            break;
        case KDialog::Close :
            kDebug() << "KDialog::Close";
            emit kTransactionFinished(Cancelled);
            done(KDialog::Close);
            break;
        default :
            KDialog::slotButtonClicked(button);
    }
}

void KpkTransaction::statusChanged(PackageKit::Transaction::Status status)
{
    d->ui.currentL->setText( KpkStrings::status(status) );
}

void KpkTransaction::errorCode(PackageKit::Client::ErrorType error, const QString &details)
{
    //Q_UNUSED(details);
    if (error == Client::MissingGpgSignature) {
        kDebug() << "Missing GPG!";
        m_handlingGpgOrEula = true;
        int ret = KMessageBox::warningYesNo(this, 
                                             details+
                                             i18n("<br />Installing unsigned packages can compromise your system, "
                                             "as it is impossible to verify if the software came from a trusted "
                                             "source. Are you sure you want to continue installation?"),
                                             i18n("Installing unsigned software"));
        if (ret == KMessageBox::Yes) {
            emit kTransactionFinished(ReQueue);
            kDebug() << "Asking for a re-queue";
        } else {
            emit kTransactionFinished(Cancelled);
            if (m_flags & CloseOnFinish)
                done(QDialog::Rejected);
        }
    }
    /*kDebug() << "errorCode: " << error;
    // check to see if we are already handlying these errors
    if ( error == Client::GpgFailure || error == Client::NoLicenseAgreement )
	if (m_handlingGpgOrEula)
	    return;

// this will be for files signature as seen in gpk
//     if ( error == Client::BadGpgSignature || error Client::MissingGpgSignature)

    // ignoring these as gpk does
    if ( error == Client::TransactionCancelled || error == Client::ProcessKill )
	return;*/

    KMessageBox::detailedSorry( this, KpkStrings::errorMessage(error), details, KpkStrings::error(error), KMessageBox::Notify );
}

void KpkTransaction::eulaRequired(PackageKit::Client::EulaInfo info)
{
    kDebug() << "eula by: " << info.vendorName;
    if (m_handlingGpgOrEula) {
	// if its true means that we alread passed here
	m_handlingGpgOrEula = false;
	return;
    }
    else
	m_handlingGpgOrEula = true;

    KpkLicenseAgreement *frm = new KpkLicenseAgreement(info, true, this);
    if (frm->exec() == KDialog::Yes && Client::instance()->acceptEula(info) )
	m_handlingGpgOrEula = false;
    // Well try again, if fail will show the erroCode
    emit kTransactionFinished(ReQueue);
}

void KpkTransaction::repoSignatureRequired(PackageKit::Client::SignatureInfo info)
{
    kDebug() << "signature by: " << info.keyId;
    if (m_handlingGpgOrEula) {
	// if its true means that we alread passed here
	m_handlingGpgOrEula = false;
	return;
    }
    else
	m_handlingGpgOrEula = true;

    KpkRepoSig *frm = new KpkRepoSig(info, true, this);
    if (frm->exec() == KDialog::Yes &&
    Client::instance()->installSignature(info.type, info.keyId, info.package) )
	m_handlingGpgOrEula = false;
    kDebug() << "Requeue!";
    emit kTransactionFinished(ReQueue);
}

void KpkTransaction::finished(PackageKit::Transaction::ExitStatus status, uint /*runtime*/)
{
    d->ui.progressBar->setMaximum(100);
    d->ui.progressBar->setValue(100);
    switch(status) {
        case Transaction::Success :
	    emit kTransactionFinished(Success);
        if (m_flags & CloseOnFinish)
            done(QDialog::Accepted);
	    break;
	case Transaction::Cancelled :
	    emit kTransactionFinished(Cancelled);
        if (m_flags & CloseOnFinish)
            done(QDialog::Rejected);
        break;
	case Transaction::Failed :
        kDebug() << "Failed.";
        if (!m_handlingGpgOrEula) {
            kDebug() << "Yep, we failed.";
            emit kTransactionFinished(Failed);
            if (m_flags & CloseOnFinish)
                done(QDialog::Rejected);
        }
	    break;
	case Transaction::KeyRequired :
	case Transaction::EulaRequired :
	    kDebug() << "finished KeyRequired or EulaRequired: " << status;
	    break;
	default :
            kDebug() << "finished default" << status;
	    KDialog::slotButtonClicked(KDialog::Close);
            break;
    }
}

#include "KpkTransaction.moc"
