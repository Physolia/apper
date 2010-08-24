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

#include "AddRmKCM.h"

#include "KpkPackageModel.h"

#include <KGenericFactory>
#include <KAboutData>

#include <version.h>

#include "KpkFiltersMenu.h"
#include "KpkPackageDetails.h"

#include <KLocale>
#include <KStandardDirs>
#include <KMessageBox>
#include <KFileItemDelegate>
#include <KFileDialog>
#include <KCategorizedSortFilterProxyModel>

#include <QPalette>
#include <QColor>
#include <QDBusConnection>
#include <QDBusMessage>

#include <KpkReviewChanges.h>
#include <KpkPackageModel.h>
#include <KpkDelegate.h>
#include <KpkStrings.h>
#include <KpkIcons.h>

#include <KDebug>

#define UNIVERSAL_PADDING 6

KCONFIGGROUP_DECLARE_ENUM_QOBJECT(Enum, Filter)

K_PLUGIN_FACTORY(KPackageKitFactory, registerPlugin<AddRmKCM>();)
K_EXPORT_PLUGIN(KPackageKitFactory("kcm_kpk_addrm"))

AddRmKCM::AddRmKCM(QWidget *parent, const QVariantList &args)
 : KCModule(KPackageKitFactory::componentData(), parent, args),
   m_currentAction(0),
   m_databaseChanged(true),
   m_searchTransaction(0),
   m_findIcon("edit-find"),
   m_cancelIcon("dialog-cancel"),
   m_searchRole(Enum::UnknownRole)
{
    KAboutData *aboutData;
    aboutData = new KAboutData("kpackagekit",
                               "kpackagekit",
                               ki18n("Add and Remove Software"),
                               KPK_VERSION,
                               ki18n("KDE interface for managing software"),
                               KAboutData::License_GPL,
                               ki18n("(C) 2008-2010 Daniel Nicoletti"));
    KGlobal::locale()->insertCatalog("kpackagekit");
    setAboutData(aboutData);

    setupUi(this);

    // Create a new daemon
    m_client = Client::instance();
    QString locale(KGlobal::locale()->language() + '.' + KGlobal::locale()->encoding());
    m_client->setHints("locale=" + locale);
    // store the actions supported by the backend
    m_roles = m_client->actions();

    // Browse TAB
    backTB->setIcon(KIcon("go-previous"));

    // create our toolbar
    QToolBar *toolBar = new QToolBar(this);
    gridLayout_2->addWidget(toolBar);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Create a stacked layout so only homeView or packageView are displayed
    m_viewLayout = new QStackedLayout(stackedWidget);
    m_viewLayout->addWidget(homeView);
    m_viewLayout->addWidget(packageView);

    QMenu *findMenu = new QMenu(this);
    // find is just a generic name in case we don't have any search method
    m_genericActionK = new KToolBarPopupAction(m_findIcon, i18n("Find"), this);
    toolBar->addAction(m_genericActionK);

    // Add actions that the backend supports
    if (m_roles & Enum::RoleSearchName) {
        findMenu->addAction(actionFindName);
        setCurrentAction(actionFindName);
    }
    if (m_roles & Enum::RoleSearchDetails) {
        findMenu->addAction(actionFindDescription);
        if (!m_currentAction) {
            setCurrentAction(actionFindDescription);
        }
    }
    if (m_roles & Enum::RoleSearchFile) {
        findMenu->addAction(actionFindFile);
        if (!m_currentAction) {
            setCurrentAction(actionFindFile);
        }
    }

    // If no action was set we can't use this search
    if (m_currentAction == 0) {
        m_genericActionK->setEnabled(false);
        searchKLE->setEnabled(false);
    } else {
        // Check to see if we need the KToolBarPopupAction
        setCurrentActionCancel(false);
        if (findMenu->actions().size() > 1) {
            m_currentAction->setVisible(false);
            m_genericActionK->setMenu(findMenu);
        } else {
            m_currentAction->setVisible(true);
            toolBar->removeAction(m_genericActionK);
            toolBar->addAction(m_currentAction);
        }
        connect(m_genericActionK, SIGNAL(triggered()),
                this, SLOT(genericActionKTriggered()));
    }

    // Create the groups model
    m_groupsModel = new QStandardItemModel(this);

    homeView->setSpacing(KDialog::spacingHint());
    homeView->viewport()->setAttribute(Qt::WA_Hover);

    //initialize the groups
    Enum::Groups groups = m_client->groups();
    QStandardItem *groupItem;
    foreach (const Enum::Group &group, groups) {
        if (group != Enum::UnknownGroup) {
            groupItem = new QStandardItem(KpkStrings::groups(group));
            groupItem->setData(group, Qt::UserRole);
            groupItem->setData(i18n("Groups"), KCategorizedSortFilterProxyModel::CategoryDisplayRole);
            groupItem->setData(0, KCategorizedSortFilterProxyModel::CategorySortRole);
            groupItem->setIcon(KpkIcons::groupsIcon(group));
            if (!(m_roles & Enum::RoleSearchGroup)) {
                groupItem->setSelectable(false);
            }
            m_groupsModel->appendRow(groupItem);
        }
    }

    KFileItemDelegate *delegate = new KFileItemDelegate(this);
    delegate->setWrapMode(QTextOption::WordWrap);
    homeView->setItemDelegate(delegate);

    KCategorizedSortFilterProxyModel *proxy = new KCategorizedSortFilterProxyModel(this);
    proxy->setSourceModel(m_groupsModel);
    proxy->setCategorizedModel(true);
    proxy->sort(0);
    homeView->setModel(proxy);

    // install the backend filters
    filtersTB->setMenu(m_filtersMenu = new KpkFiltersMenu(m_client->filters(), this));
    filtersTB->setIcon(KIcon("view-filter"));

    transactionBar->setBehaviors(KpkTransactionBar::AutoHide | KpkTransactionBar::HideCancel);

    //initialize the model, delegate, client and  connect it's signals
    setupView(&m_browseModel, packageView);

    // INSTALLED TAB
    setupView(&m_installedModel, installedView);
    tabWidget->setTabIcon(1, KIcon("dialog-ok"));
    exportInstalledPB->setIcon(KIcon("document-export"));
    importInstalledPB->setIcon(KIcon("document-import"));

    // CHANGES TAB
    changesView->viewport()->setAttribute(Qt::WA_Hover);
    m_changesModel = new KpkPackageModel(this, changesView);
    KCategorizedSortFilterProxyModel *changedProxy = new KCategorizedSortFilterProxyModel(this);
    changedProxy->setSourceModel(m_changesModel);
    changedProxy->setCategorizedModel(true);
    changedProxy->sort(0);
    changedProxy->setDynamicSortFilter(true);
    changedProxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    changedProxy->setSortRole(KpkPackageModel::SortRole);
    changesView->setModel(changedProxy);
    KpkDelegate *changesDelegate = new KpkDelegate(changesView);
    changesDelegate->setExtendPixmapWidth(0);
    changesView->setItemDelegate(changesDelegate);

    // Connect this signal anyway so users that have backend that
    // do not support install or remove can be informed properly
    connect(m_changesModel, SIGNAL(rowsInserted(const QModelIndex, int, int)),
            this, SLOT(checkChanged()));
    connect(m_changesModel, SIGNAL(rowsRemoved(const QModelIndex, int, int)),
            this, SLOT(checkChanged()));

    // Make the models talk to each other
    // packageCheced from browse model
    connect(m_browseModel, SIGNAL(packageChecked(const QSharedPointer<PackageKit::Package> &)),
            m_installedModel, SLOT(checkPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_browseModel, SIGNAL(packageChecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(addSelectedPackage(const QSharedPointer<PackageKit::Package> &)));

    // packageCheced from installed model
    connect(m_installedModel, SIGNAL(packageChecked(const QSharedPointer<PackageKit::Package> &)),
            m_browseModel, SLOT(checkPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_installedModel, SIGNAL(packageChecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(addSelectedPackage(const QSharedPointer<PackageKit::Package> &)));

    // packageUnchecked from browse model
    connect(m_browseModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_installedModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_browseModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_browseModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(rmSelectedPackage(const QSharedPointer<PackageKit::Package> &)));

    // packageUnchecked from installed model
    connect(m_installedModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_browseModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_installedModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_installedModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(rmSelectedPackage(const QSharedPointer<PackageKit::Package> &)));

    // packageUnchecked from changes model
    connect(m_changesModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_changesModel, SLOT(rmSelectedPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_changesModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_browseModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));
    connect(m_changesModel, SIGNAL(packageUnchecked(const QSharedPointer<PackageKit::Package> &)),
            m_installedModel, SLOT(uncheckPackage(const QSharedPointer<PackageKit::Package> &)));

    // colapse package description when removing rows
//     connect(m_changesModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),
//             this, SLOT(rowsAboutToBeRemoved(const QModelIndex &, int, int)));
}

void AddRmKCM::setupView(KpkPackageModel **model, QTreeView *view)
{
    *model = new KpkPackageModel(this, view);
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(*model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortRole(KpkPackageModel::SortRole);
    view->setModel(proxyModel);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->header()->setDefaultAlignment(Qt::AlignCenter);
    KpkDelegate *delegate = new KpkDelegate(view);
    view->setItemDelegate(delegate);
    connect(delegate, SIGNAL(showExtendItem(const QModelIndex &)),
            this, SLOT(showExtendItem(const QModelIndex &)));
    connect(proxyModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)),
            this, SLOT(rowsAboutToBeRemoved(const QModelIndex &, int, int)));
}

void AddRmKCM::genericActionKTriggered()
{
    m_currentAction->trigger();
}

void AddRmKCM::setCurrentAction(QAction *action)
{
    // just load the new action if it changes this
    // also ensures that our menu has more than one action
    if (m_currentAction != action) {
        // hides the item from the list
        action->setVisible(false);
        // ensures the current action was created
        if (m_currentAction) {
            // show the item back in the list
            m_currentAction->setVisible(true);
        }
        m_currentAction = action;
        // copy data from the curront action
        m_genericActionK->setText(m_currentAction->text());
        m_genericActionK->setIcon(m_currentAction->icon());
    }
}

void AddRmKCM::setCurrentActionEnabled(bool state)
{
    if (m_currentAction) {
        m_currentAction->setEnabled(state);
    }
    m_genericActionK->setEnabled(state);
}

void AddRmKCM::setCurrentActionCancel(bool cancel)
{
    if (cancel) {
        // every action should like cancel
        actionFindName->setText(i18n("&Cancel"));
        actionFindFile->setText(i18n("&Cancel"));
        actionFindDescription->setText(i18n("&Cancel"));
        m_genericActionK->setText(i18n("&Cancel"));
        // set cancel icons
        actionFindFile->setIcon(m_cancelIcon);
        actionFindDescription->setIcon(m_cancelIcon);
        actionFindName->setIcon(m_cancelIcon);
        m_genericActionK->setIcon(m_cancelIcon);
    } else {
        actionFindName->setText(i18n("Find by &name"));
        actionFindFile->setText(i18n("Find by f&ile name"));
        actionFindDescription->setText(i18n("Find by &description"));
        // Define actions icon
        actionFindFile->setIcon(KIcon("document-open"));
        actionFindDescription->setIcon(KIcon("document-edit"));
        actionFindName->setIcon(m_findIcon);
        m_genericActionK->setIcon(m_findIcon);
        if (m_currentAction) {
            m_genericActionK->setText(m_currentAction->text());
        } else {
            // This might happen when the backend can
            // only search groups
            m_genericActionK->setText(i18n("Find"));
        }
    }
}

void AddRmKCM::checkChanged()
{
    int size = m_changesModel->rowCount();
    if (size > 0) {
        tabWidget->setTabText(2, i18np("1 Change Pending", "%1 Changes Pending", size));
        emit changed(true);
    } else {
        tabWidget->setTabText(2, i18n("No Change Pending"));
        emit changed(false);
    }
}

void AddRmKCM::showExtendItem(const QModelIndex &index)
{
    if (index.column() == 0) {
        KpkDelegate *delegate = qobject_cast<KpkDelegate*>(sender());
        const QSortFilterProxyModel *proxy;
        const KpkPackageModel *model;
        proxy = qobject_cast<const QSortFilterProxyModel*>(index.model());
        model = qobject_cast<const KpkPackageModel*>(proxy->sourceModel());
        QModelIndex origIndex = proxy->mapToSource(index);
        QSharedPointer<PackageKit::Package> package = model->package(origIndex);
        if (package) {
            if (delegate->isExtended(index)) {
                delegate->contractItem(index);
            } else {
                delegate->extendItem(new KpkPackageDetails(package, m_roles), index);
            }
        }
    }
}

void AddRmKCM::rowsAboutToBeRemoved(const QModelIndex &index, int start, int end)
{
    Q_UNUSED(end)
    Q_UNUSED(index)
    // If the item is going to be removed and it's extend item is expanded
    // we need to contract it in order to don't show it parent less
    QAbstractItemModel *model;
    model = qobject_cast<QAbstractItemModel*>(sender());
    QModelIndex removingIndex = model->index(start, 0);
    KpkDelegate *delegate = qobject_cast<KpkDelegate*>(changesView->itemDelegate());
    delegate->contractItem(removingIndex);
}

void AddRmKCM::errorCode(PackageKit::Enum::Error error, const QString &details)
{
    if (error != Enum::ErrorTransactionCancelled) {
        KMessageBox::detailedSorry(this, KpkStrings::errorMessage(error), details, KpkStrings::error(error), KMessageBox::Notify);
    }
}

AddRmKCM::~AddRmKCM()
{
    KConfig config("KPackageKit");
    KConfigGroup filterMenuGroup(&config, "FilterMenu");

    // For usability we will only save ViewInGroups settings and Newest filter,
    // - The user might get angry when he does not find any packages because he didn't
    //   see that a filter is set by config

    // This entry does not depend on the backend it's ok to call this pointer
//     filterMenuGroup.writeEntry("ViewInGroups", m_filtersMenu->actionGrouped());

    // This entry does not depend on the backend it's ok to call this pointer
    if (m_client->filters() & Enum::FilterNewest) {
        filterMenuGroup.writeEntry("FilterNewest",
                                   static_cast<bool>(m_filtersMenu->filters() & Enum::FilterNewest));
    }
}

void AddRmKCM::on_actionFindName_triggered()
{
    setCurrentAction(actionFindName);
    if (m_searchTransaction) {
        m_searchTransaction->cancel();
    } else if (!searchKLE->text().isEmpty()) {
        // cache the search
        m_searchRole    = Enum::RoleSearchName;
        m_searchString  = searchKLE->text();
        m_searchFilters = m_filtersMenu->filters();
        // create the main transaction
        search();
    }
}

void AddRmKCM::on_actionFindDescription_triggered()
{
    setCurrentAction(actionFindDescription);
    if (m_searchTransaction) {
        m_searchTransaction->cancel();
    } else if (!searchKLE->text().isEmpty()) {
        // cache the search
        m_searchRole    = Enum::RoleSearchDetails;
        m_searchString  = searchKLE->text();
        m_searchFilters = m_filtersMenu->filters();
        // create the main transaction
        search();
    }
}

void AddRmKCM::on_actionFindFile_triggered()
{
    setCurrentAction(actionFindFile);
    if (m_searchTransaction) {
        m_searchTransaction->cancel();
    } else if (!searchKLE->text().isEmpty()) {
        // cache the search
        m_searchRole    = Enum::RoleSearchFile;
        m_searchString  = searchKLE->text();
        m_searchFilters = m_filtersMenu->filters();
        // create the main transaction
        search();
    }
}

void AddRmKCM::on_homeView_activated(const QModelIndex &index)
{
    if (index.isValid()) {
        // cache the search
        m_searchRole    = Enum::RoleSearchGroup;
        m_searchGroup   = static_cast<Enum::Group>(index.data(Qt::UserRole).toUInt());
        m_searchFilters = m_filtersMenu->filters();
        // create the main transaction
        search();
    }
}

void AddRmKCM::on_backTB_clicked()
{
    m_viewLayout->setCurrentIndex(0);
    backTB->setEnabled(false);
    // reset the search role
    m_searchRole = Enum::UnknownRole;
}

void AddRmKCM::on_tabWidget_currentChanged(int index)
{
    if (index == 1 && m_databaseChanged == true) {
        exportInstalledPB->setEnabled(false);
        m_databaseChanged = false;
        m_installedModel->clear();
        Transaction *trans = m_client->getPackages(Enum::FilterInstalled);
        connectTransaction(trans, m_installedModel);
        connect(trans, SIGNAL(finished(PackageKit::Enum::Exit, uint)),
                this, SLOT(enableExportInstalledPB()));
    }
}

void AddRmKCM::search()
{
    // search
    if (m_searchRole == Enum::RoleSearchName) {
        m_searchTransaction = m_client->searchNames(m_searchString, m_searchFilters);
    } else if (m_searchRole == Enum::RoleSearchDetails) {
        m_searchTransaction = m_client->searchDetails(m_searchString, m_searchFilters);
    } else if (m_searchRole == Enum::RoleSearchFile) {
        m_searchTransaction = m_client->searchFiles(m_searchString, m_searchFilters);
    } else if (m_searchRole == Enum::RoleSearchGroup) {
        m_searchTransaction = m_client->searchGroups(m_searchGroup, m_searchFilters);
    } else {
        kDebug() << "Search type not defined yet";
        return;
    }

    m_viewLayout->setCurrentIndex(1);
    backTB->setEnabled(true);

    if (m_searchTransaction->error()) {
        KMessageBox::sorry(this, KpkStrings::daemonError(m_searchTransaction->error()));
        setCurrentActionEnabled(true);
        m_searchTransaction = 0;
    } else {
        setCurrentActionCancel(true);
        connectTransaction(m_searchTransaction, m_browseModel);
        connect(m_searchTransaction, SIGNAL(finished(PackageKit::Enum::Exit, uint)),
                this, SLOT(finished(PackageKit::Enum::Exit, uint)));
        setCurrentActionEnabled(m_searchTransaction->allowCancel());
        // contract and delete and details widgets
        KpkDelegate *delegate = qobject_cast<KpkDelegate*>(packageView->itemDelegate());
        delegate->contractAll();
        // cleans the models
        m_browseModel->clear();
    }
}

void AddRmKCM::connectTransaction(Transaction *transaction, KpkPackageModel *model)
{
    connect(transaction, SIGNAL(package(QSharedPointer<PackageKit::Package>)),
            model, SLOT(addPackage(QSharedPointer<PackageKit::Package>)));
    connect(transaction, SIGNAL(errorCode(PackageKit::Enum::Error, const QString &)),
            this, SLOT(errorCode(PackageKit::Enum::Error, const QString &)));
    transactionBar->addTransaction(transaction);
}

void AddRmKCM::changed()
{
    Transaction *trans = qobject_cast<Transaction*>(sender());
    setCurrentActionEnabled(trans->allowCancel());
}

void AddRmKCM::save()
{
    QPointer<KpkReviewChanges> frm = new KpkReviewChanges(m_changesModel->selectedPackages(), this);
    frm->exec();

    // This avoid crashing as the above function does not always quit it's event loop
    if (!frm.isNull()) {
        delete frm;
        // The database might have changed
        m_changesModel->resolveSelected();
        m_databaseChanged = true;
        on_tabWidget_currentChanged(tabWidget->currentIndex());
        search();
        QTimer::singleShot(0, this, SLOT(checkChanged()));
    }
}

void AddRmKCM::load()
{
    // set focus on the search lineEdit
    searchKLE->setFocus(Qt::OtherFocusReason);
//     m_browseModel->uncheckAll();
}

void AddRmKCM::finished(PackageKit::Enum::Exit status, uint runtime)
{
    Q_UNUSED(runtime)
    Q_UNUSED(status)
    // if m_currentAction is false means that our
    // find button should be disable as there aren't any
    // search methods
    setCurrentActionEnabled(m_currentAction);
    setCurrentActionCancel(false);
    m_searchTransaction = 0;
}

void AddRmKCM::keyPressEvent(QKeyEvent *event)
{
    if (searchKLE->hasFocus() &&
        (event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter)) {
        // special tab handling here
        m_currentAction->trigger();
        return;
    }
    KCModule::keyPressEvent(event);
}

void AddRmKCM::on_exportInstalledPB_clicked()
{
    // We will assume the installed model
    // is populated since the user is seeing it.
    QString fileName;
    fileName = KFileDialog::getSaveFileName(KUrl(), "*.catalog", this);
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    file.open(QIODevice::WriteOnly);
    QTextStream out(&file);

    out << "[PackageKit Catalog]\n\n";
    out << "InstallPackages(" << m_client->distroId() << ")=";
    QStringList packages;
    for (int i = 0; i < m_installedModel->rowCount(); i++) {
        packages << m_installedModel->data(m_installedModel->index(i, 0),
                                           KpkPackageModel::NameRole).toString();
    }
    out << packages.join(";");
}

void AddRmKCM::on_importInstalledPB_clicked()
{
    QString fileName;
    fileName = KFileDialog::getOpenFileName(KUrl(), "*.catalog", this);
    if (fileName.isEmpty()) {
        return;
    }

    // send a DBus message to install this catalog
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.freedesktop.PackageKit",
                                             "/org/freedesktop/PackageKit",
                                             "org.freedesktop.PackageKit.Modify",
                                             "InstallCatalogs");
    message << static_cast<uint>(effectiveWinId());
    message << (QStringList() << fileName);
    message << QString();

    // This call must block otherwise this application closes before
    // smarticon is activated
    QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block);
}

void AddRmKCM::enableExportInstalledPB()
{
    exportInstalledPB->setEnabled(true);
}

#include "AddRmKCM.moc"
