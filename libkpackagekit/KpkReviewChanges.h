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

#ifndef KPK_REVIEW_CHANGES_H
#define KPK_REVIEW_CHANGES_H

#include <KDialog>

#include <QPackageKit>

using namespace PackageKit;

class KpkDelegate;
class KpkTransaction;
class KpkSimulateModel;
class KpkPackageModel;
class KpkReviewChangesPrivate;
class KDE_EXPORT KpkReviewChanges : public KDialog
{
    Q_OBJECT
public:
    explicit KpkReviewChanges(const QList<QSharedPointer<PackageKit::Package> > &packages, QWidget *parent = 0);
    ~KpkReviewChanges();

    enum OperationMode {
        // Doesn't show confirmation and exits if some transaction is hidden
        Default                 = 0x00,
        ShowConfirmation        = 0x01,
        ReturnOnlyWhenFinished  = 0x02,
        HideProgress            = 0x04,
        HideConfirmDeps         = 0x10
    };
    Q_DECLARE_FLAGS(OperationModes, OperationMode)

    void setTitle(const QString &title);
    void setText(const QString &text);

    int exec(OperationModes flags = 0);

private slots:
    void requeueTransaction();
    void simulateFinished(PackageKit::Enum::Exit status);
    void ensureTransactionFinished(PackageKit::Enum::Exit status);

    void doAction();
    void checkChanged();

private:
    void installPackages();
    void removePackages(bool allow_deps = true);
    void taskDone(PackageKit::Enum::Role role);

    void updateColumnsWidth(bool force = false);
    void checkTask();

    KpkReviewChangesPrivate *d;
    OperationModes m_flags;

protected slots:
    virtual void slotButtonClicked(int button);

protected:
    virtual void resizeEvent(QResizeEvent *event);
    virtual bool event(QEvent *event);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KpkReviewChanges::OperationModes)

#endif
