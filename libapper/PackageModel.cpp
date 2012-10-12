/***************************************************************************
 *   Copyright (C) 2008-2011 by Daniel Nicoletti                           *
 *   dantti12@gmail.com                                                    *
 *   Copyright (C) 2008 by Trever Fischer                                  *
 *   wm161@wm161.net                                                       *
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

#include <config.h>

#include "PackageModel.h"
#include <PkStrings.h>

#include <QPainter>
#include <QStringBuilder>

#include <KIconLoader>
#include <KDebug>
#include <PkIcons.h>
#include <KLocale>
#include <KCategorizedSortFilterProxyModel>

#ifdef HAVE_APPSTREAM
#include <AppStream/AppStreamDb.h>
#endif

#ifndef HAVE_APPSTREAM
#include <QSqlDatabase>
#include <QSqlQuery>
#endif

#define ICON_SIZE 22
#define OVERLAY_SIZE 16

using namespace PackageKit;

PackageModel::PackageModel(QObject *parent)
: QAbstractItemModel(parent),
  m_packageCount(0),
  m_checkable(false),
  m_fetchSizesTransaction(0),
  m_fetchInstalledVersionsTransaction(0)
{
    m_installedEmblem = PkIcons::getIcon("dialog-ok-apply", QString()).pixmap(16, 16);

    QHash<int, QByteArray> roles = roleNames();
    roles[SortRole] = "rSort";
    roles[NameRole] = "rName";
    roles[SummaryRole] = "rSummary";
    roles[VersionRole] = "rVersion";
    roles[ArchRole] = "rArch";
    roles[IconRole] = "rIcon";
    roles[IdRole] = "rId";
    roles[CheckStateRole] = "rChecked";
    roles[InfoRole] = "rInfo";
    roles[ApplicationId] = "rApplicationId";
    roles[IsPackageRole] = "rIsPackageRole";
    roles[PackageName] = "rPackageName";
    roles[InfoIconRole] = "rInfoIcon";
    setRoleNames(roles);
}

void PackageModel::addPackage(Transaction::Info info, const QString &packageID, const QString &summary, bool selected)
{
    if (info == Transaction::InfoBlocked) {
        return;
    }

#ifdef HAVE_APPSTREAM
    QList<AppStreamDb::Application> applications;
    if (!m_checkable) {
        applications = AppStreamDb::instance()->applications(Transaction::packageName(packageID));

        foreach (const AppStreamDb::Application &app, applications) {
            InternalPackage iPackage;
            iPackage.info = info;
            iPackage.packageID = packageID;
            iPackage.version = Transaction::packageVersion(packageID);
            iPackage.arch = Transaction::packageArch(packageID);
            iPackage.repo = Transaction::packageData(packageID);
            iPackage.isPackage = false;
            if (app.name.isEmpty()) {
                iPackage.displayName = Transaction::packageName(packageID);
            } else {
                iPackage.displayName = app.name;
            }
            if (app.summary.isEmpty()) {
                iPackage.summary = summary;
            } else {
                iPackage.summary = app.summary;
            }
            iPackage.icon  = app.icon;
            iPackage.appId = app.id;
            iPackage.size  = 0;

            if (selected) {
                checkPackage(iPackage, false);
            }
            m_packages.append(iPackage);
        }
    }

    if (applications.isEmpty()) {
#endif //HAVE_APPSTREAM

        InternalPackage iPackage;
        iPackage.info = info;
        iPackage.packageID = packageID;
        iPackage.displayName = Transaction::packageName(packageID);
        iPackage.version = Transaction::packageVersion(packageID);
        iPackage.arch = Transaction::packageArch(packageID);
        iPackage.repo = Transaction::packageData(packageID);
        iPackage.summary = summary;
        iPackage.size = 0;

#ifdef HAVE_APPSTREAM
        iPackage.icon = AppStreamDb::instance()->genericIcon(Transaction::packageName(packageID));
        if (m_checkable) {
            // in case of updates model only check if it's an app
            applications = AppStreamDb::instance()->applications(Transaction::packageName(packageID));
            if (!applications.isEmpty() || !Transaction::packageIcon(packageID).isEmpty()) {
                iPackage.isPackage = false;
            } else {
                iPackage.isPackage = true;
            }
        } else {
            iPackage.isPackage = true;
        }
#else
        iPackage.icon = Transaction::packageIcon(packageID);
        if (iPackage.icon.isEmpty()) {
            iPackage.isPackage = true;
        } else {
            iPackage.isPackage = false;
            QSqlDatabase db = QSqlDatabase::database();
            QSqlQuery query(db);
            query.prepare("SELECT filename FROM cache WHERE package = :name");
            query.bindValue(":name", Transaction::packageName(packageID));
            if (query.exec()) {
                if (query.next()) {
                    QString filename = query.value(0).toString();
                    filename.remove(QRegExp(".desktop$")).remove(QRegExp("^/.*/"));
                    iPackage.appId = filename;
                }
            }
        }
#endif // HAVE_APPSTREAM

        if (selected) {
            checkPackage(iPackage, false);
        }
        m_packages.append(iPackage);

#ifdef HAVE_APPSTREAM
    }
#endif // HAVE_APPSTREAM
}

//void PackageModel::addPackages(const QStringList &packages, bool selected)
//{
//    foreach(const Package &package, packages) {
//        addPackage(package, selected);
//    }
//    finished();
//}

void PackageModel::addSelectedPackage(Transaction::Info info, const QString &packageID, const QString &summary)
{
    kDebug() << packageID;
    addPackage(info, packageID, summary, true);
}

QVariant PackageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (/*m_packageCount && */role == Qt::DisplayRole) {
        if (section == NameCol) {
            if (m_checkable) {
                return PkStrings::packageQuantity(true,
                                                   m_packages.size(),
                                                   m_checkedPackages.size());
            }
            return i18n("Name");
        } else if (section == VersionCol) {
            return i18n("Version");
        } else if (section == CurrentVersionCol) {
            return i18n("Installed Version");
        } else if (section == ArchCol) {
            return i18n("Arch");
        } else if (section == OriginCol) {
            return i18n("Origin");
        } else if (section == SizeCol) {
            return i18n("Size");
        } else if (section == ActionCol) {
            return i18n("Action");
        }
    }
    return QVariant();
}

int PackageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_packageCount;
}

QModelIndex PackageModel::index(int row, int column, const QModelIndex &parent) const
{
//   kDebug() << parent.isValid() << m_packageCount << row << column;
    // Check to see if the index isn't out of list
    if (!parent.isValid() && m_packageCount > row) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QModelIndex PackageModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

QVariant PackageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const InternalPackage &package = m_packages[index.row()];

    if (index.column() == NameCol) {
        switch (role) {
        case Qt::CheckStateRole:
            if (!m_checkable) {
                return QVariant();
            }
        case CheckStateRole:
            if (containsChecked(package.packageID)) {
                return Qt::Checked;
            }
            return Qt::Unchecked;
        case IsPackageRole:
            return package.isPackage;
        case Qt::DisplayRole:
            return package.displayName;
        case Qt::DecorationRole:
        {
            QPixmap icon = QPixmap(44, ICON_SIZE);
            icon.fill(Qt::transparent);
            if (!package.icon.isNull()) {
                QPixmap pixmap = KIconLoader::global()->loadIcon(package.icon,
                                                                 KIconLoader::NoGroup,
                                                                 ICON_SIZE,
                                                                 KIconLoader::DefaultState,
                                                                 QStringList(),
                                                                 0L,
                                                                 true);
                if (!pixmap.isNull()) {
                    QPainter painter(&icon);
                    painter.drawPixmap(QPoint(2, 0), pixmap);
                }
            }

            if (package.info == Transaction::InfoInstalled ||
                package.info == Transaction::InfoCollectionInstalled) {
                QPainter painter(&icon);
                QPoint startPoint;
                // bottom right corner
                startPoint = QPoint(44 - OVERLAY_SIZE, 4);
                painter.drawPixmap(startPoint, m_installedEmblem);
            } else if (m_checkable) {
                QIcon emblemIcon = PkIcons::packageIcon(package.info);
                QPainter painter(&icon);
                QPoint startPoint;
                // bottom right corner
                startPoint = QPoint(44 - OVERLAY_SIZE, 4);
                painter.drawPixmap(startPoint, emblemIcon.pixmap(OVERLAY_SIZE, OVERLAY_SIZE));
            }
            return icon;
        }
        case PackageName:
            return Transaction::packageName(package.packageID);
        case Qt::ToolTipRole:
            if (m_checkable) {
                return PkStrings::info(package.info);
            } else {
                return i18n("Version: %1\nArchitecture: %2", package.version, package.arch);
            }
        }
    } else if (role == Qt::DisplayRole) {
        if (index.column() == VersionCol) {
            return package.version;
        } else if (index.column() == CurrentVersionCol) {
                return package.currentVersion;
        } else if (index.column() == ArchCol) {
            return package.arch;
        } else if (index.column() == OriginCol) {
            return package.repo;
        } else if (index.column() == SizeCol) {
            return package.size ? KGlobal::locale()->formatByteSize(package.size) : QString();
        }
    } else if (index.column() == SizeCol && role == Qt::TextAlignmentRole) {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }

    switch (role) {
    case IconRole:
        return package.icon;
    case SortRole:
        return QString(package.displayName % QLatin1Char(' ') % package.version % QLatin1Char(' ') % package.arch);
    case CheckStateRole:
        if (containsChecked(package.packageID)) {
            return Qt::Checked;
        }
        return Qt::Unchecked;
    case IdRole:
        return package.packageID;
    case NameRole:
        return package.displayName;
    case SummaryRole:
        return package.summary;
    case VersionRole:
        return package.version;
    case ArchRole:
        return package.arch;
    case OriginCol:
        return package.repo;
    case InfoRole:
        return qVariantFromValue(package.info);
    case KCategorizedSortFilterProxyModel::CategoryDisplayRole:
        if (package.info == Transaction::InfoInstalled ||
            package.info == Transaction::InfoCollectionInstalled) {
            return i18n("To be Removed");
        } else {
            return i18n("To be Installed");
        }
    case KCategorizedSortFilterProxyModel::CategorySortRole:
        // USING 0 here seems to let things unsorted
        return package.isPackage ? 1 : 0; // Packages comes after applications
    case ApplicationId:
        return package.appId;
    case InfoIconRole:
        return PkIcons::packageIcon(package.info);
    default:
        return QVariant();
    }

    return QVariant();
}

bool PackageModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole && m_packageCount > index.row()) {
        if (value.toBool()) {
            checkPackage(m_packages[index.row()]);
        } else {
            uncheckPackage(m_packages[index.row()].packageID);
        }

        emit changed(!m_checkedPackages.isEmpty());

        return true;
    }
    return false;
}

Qt::ItemFlags PackageModel::flags(const QModelIndex &index) const
{
    if (index.column() == NameCol) {
        return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | QAbstractItemModel::flags(index);
    }
    return QAbstractItemModel::flags(index);
}

int PackageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_checkable) {
        // when the model is checkable the action column is not shown
        return ActionCol;
    } else {
        return ActionCol + 1;
    }
}

void PackageModel::rmSelectedPackage(const QString &packageID)
{
    for (int i = 0; i < m_packages.size(); i++) {
        if (m_packages[i].packageID == packageID) {
            beginRemoveRows(QModelIndex(), i, i);
            m_packages.remove(i);
            endRemoveRows();
            i--; // we have to decrease the pointer otherwise
                 // we will miss some packages
        }
    }
}

void PackageModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, m_packageCount);
    m_packageCount = 0;
    m_packages.clear();
    m_fetchSizesTransaction = 0;
    m_fetchInstalledVersionsTransaction = 0;
    endRemoveRows();
}

void PackageModel::clearSelectedNotPresent()
{
    foreach (const InternalPackage &package, m_checkedPackages) {
        foreach (const InternalPackage &iPackage, m_packages) {
            if (iPackage.packageID == package.packageID) {
                // Uncheck the package If it's not in the model
                uncheckPackage(package.packageID);
                break;
            }
        }
    }
}

bool PackageModel::checkable() const
{
    return m_checkable;
}

void PackageModel::uncheckInstalledPackages()
{
    foreach (const InternalPackage &package, m_checkedPackages) {
        if (package.info == Transaction::InfoInstalled ||
                package.info == Transaction::InfoCollectionInstalled) {
            uncheckPackage(package.packageID, true);
        }
    }
}

void PackageModel::uncheckAvailablePackages()
{
    foreach (const InternalPackage &package, m_checkedPackages) {
        if (package.info == Transaction::InfoAvailable ||
                package.info == Transaction::InfoCollectionAvailable) {
            uncheckPackage(package.packageID, true);
        }
    }
}

void PackageModel::finished()
{
    Transaction *trans = qobject_cast<Transaction*>(sender());
    if (trans) {
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        trans->disconnect(this, SLOT(finished()));
    }

    // The whole structure is about to change
    beginInsertRows(QModelIndex(), 0, m_packages.size() - 1);
    m_packageCount = m_packages.size();
    endInsertRows();

    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::fetchSizes()
{
    if (m_fetchSizesTransaction) {
        return;
    }

    // get package size
    QStringList pkgs;
    foreach (const InternalPackage &p, m_packages) {
        pkgs << p.packageID;
    }

    if (!pkgs.isEmpty()) {
        m_fetchSizesTransaction = new Transaction(this);
        connect(m_fetchSizesTransaction, SIGNAL(details(QString,QString,PackageKit::Transaction::Group,QString,QString,qulonglong)),
                this, SLOT(updateSize(QString,QString,PackageKit::Transaction::Group,QString,QString,qulonglong)));
        connect(m_fetchSizesTransaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
                this, SLOT(fetchSizesFinished()));
        m_fetchSizesTransaction->getDetails(pkgs);
    }
}

void PackageModel::fetchSizesFinished()
{
    Transaction *trans = qobject_cast<Transaction*>(sender());
    if (trans) {
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        trans->disconnect(this, SLOT(fetchSizesFinished()));
    }
    // emit this after all is changed other =wise on large models it will
    // be hell slow...
    emit dataChanged(createIndex(0, SizeCol), createIndex(m_packageCount, SizeCol));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::updateSize(const QString &packageID,
                              const QString &license,
                              PackageKit::Transaction::Group group,
                              const QString &detail,
                              const QString &url,
                              qulonglong size)
{
    Q_UNUSED(license)
    Q_UNUSED(group)
    Q_UNUSED(detail)
    Q_UNUSED(url)
    // if size is 0 don't waste time looking for the package
    if (size) {
        return;
    }

    for (int i = 0; i < m_packages.size(); ++i) {
        if (packageID == m_packages[i].packageID) {
            m_packages[i].size = size;
            if (m_checkable) {
                // updates the checked packages as well
                if (m_checkedPackages.contains(packageID)) {
                    // Avoid checking packages that aren't checked
                    m_checkedPackages[packageID].size = size;
                }
                break;
            }
        }
    }
}

void PackageModel::fetchCurrentVersions()
{
    if (m_fetchInstalledVersionsTransaction) {
        return;
    }

    // get package current version
    QStringList pkgs;
    foreach (const InternalPackage &p, m_packages) {
        pkgs << Transaction::packageName(p.packageID);
    }

    if (!pkgs.isEmpty()) {
        m_fetchInstalledVersionsTransaction = new Transaction(this);
        connect(m_fetchInstalledVersionsTransaction, SIGNAL(package(PackageKit::Transaction::Info,QString,QString)),
                this, SLOT(updateCurrentVersion(PackageKit::Transaction::Info,QString,QString)));
        connect(m_fetchInstalledVersionsTransaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
                this, SLOT(fetchCurrentVersionsFinished()));
        m_fetchInstalledVersionsTransaction->resolve(pkgs, Transaction::FilterInstalled);
    }
}

void PackageModel::fetchCurrentVersionsFinished()
{
    Transaction *trans = qobject_cast<Transaction*>(sender());
    if (trans) {
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        trans->disconnect(this, SLOT(fetchCurrentVersionsFinished()));
    }
    // emit this after all is changed otherwise on large models it will
    // be hell slow...
    emit dataChanged(createIndex(0, CurrentVersionCol), createIndex(m_packageCount, CurrentVersionCol));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::updateCurrentVersion(Transaction::Info info, const QString &packageID, const QString &summary)
{
    Q_UNUSED(info)
    Q_UNUSED(summary)
    // if current version is empty don't waste time looking
    if (!Transaction::packageVersion(packageID).isEmpty()) {
        for (int i = 0; i < m_packages.size(); ++i) {
            if (Transaction::packageName(packageID) == Transaction::packageName(m_packages[i].packageID) &&
                Transaction::packageArch(packageID) == m_packages[i].arch) {
                m_packages[i].currentVersion = Transaction::packageVersion(packageID);
                if (m_checkable) {
                    // updates the checked packages as well
                    if (m_checkedPackages.contains(m_packages[i].packageID)) {
                        // Avoid checking packages that aren't checked
                        m_checkedPackages[m_packages[i].packageID].currentVersion = Transaction::packageVersion(packageID);
                    }
                    break;
                }
            }
        }
    }
}

void PackageModel::getUpdates(bool fetchCurrentVersions, bool selected)
{
    clear();
    Transaction *transaction = new Transaction(this);
    if (selected) {
        connect(transaction, SIGNAL(package(PackageKit::Transaction::Info,QString,QString)),
                this, SLOT(addSelectedPackage(PackageKit::Transaction::Info,QString,QString)));
    } else {
        connect(transaction, SIGNAL(package(PackageKit::Transaction::Info,QString,QString)),
                this, SLOT(addPackage(PackageKit::Transaction::Info,QString,QString)));
    }
    connect(transaction, SIGNAL(errorCode(PackageKit::Transaction::Error,QString)),
            this, SLOT(errorCode(PackageKit::Transaction::Error,QString)));
//    connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
//            m_busySeq, SLOT(stop()));
    connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
            this, SLOT(finished()));
    // This is required to estimate download size
    connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
            this, SLOT(fetchSizes()));
    if (fetchCurrentVersions) {
        connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
                this, SLOT(fetchCurrentVersions()));
    }
//    connect(transaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
//            this, SLOT(getUpdatesFinished()));
    // get all updates
    transaction->getUpdates();

    Transaction::InternalError error = transaction->error();
    if (error) {
        transaction->deleteLater();
//        KMessageBox::sorry(this, PkStrings::daemonError(error));
    } else {
//        m_busySeq->start();
    }
}

bool PackageModel::hasChanges() const
{
    return !m_checkedPackages.isEmpty();
}

void PackageModel::checkPackage(const InternalPackage &package, bool emitDataChanged)
{
    QString pkgId = package.packageID;
    if (!containsChecked(pkgId)) {
        m_checkedPackages[pkgId] = package;

        // A checkable model does not have duplicated entries
        if (emitDataChanged && m_packageCount && !m_checkable) {
            // This is a slow operation so in case the user
            // is unchecking all of the packages there is
            // no need to emit data changed for every item
            for (int i = 0; i < m_packages.size(); ++i) {
                if (m_packages[i].packageID == pkgId) {
                    QModelIndex index = createIndex(i, 0);
                    emit dataChanged(index, index);
                }
            }

            // The model might not be displayed yet
            if (m_packageCount) {
                emit changed(!m_checkedPackages.isEmpty());
            }
        }
    }
}

void PackageModel::uncheckPackage(const QString &packageID,
                                  bool forceEmitUnchecked,
                                  bool emitDataChanged)
{
    if (containsChecked(packageID)) {
        m_checkedPackages.remove(packageID);
        if (forceEmitUnchecked || sender() == 0) {
            // The package might be removed by rmSelectedPackage
            // If we don't copy it the browse model won't uncheck there
            // right package
            emit packageUnchecked(packageID);
        }

        if (emitDataChanged && !m_checkable) {
            // This is a slow operation so in case the user
            // is unchecking all of the packages there is
            // no need to emit data changed for every item
            for (int i = 0; i < m_packages.size(); ++i) {
                if (m_packages[i].packageID == packageID) {
                    QModelIndex index = createIndex(i, 0);
                    emit dataChanged(index, index);
                }
            }

            // The model might not be displayed yet
            if (m_packageCount) {
                emit changed(!m_checkedPackages.isEmpty());
            }
        }
    }
}

bool PackageModel::containsChecked(const QString &pid) const
{
    if (m_checkedPackages.isEmpty()) {
        return false;
    }
    return m_checkedPackages.contains(pid);
}

void PackageModel::setAllChecked(bool checked)
{
    if (checked) {
        m_checkedPackages.clear();
        foreach (const InternalPackage &package, m_packages) {
            checkPackage(package, false);
        }
        emit dataChanged(createIndex(0, 0),
                         createIndex(m_packages.size(), 0));
    } else {
        // This is a very slow operation, which in here we try to optimize
        foreach (const InternalPackage &package, m_checkedPackages) {
            uncheckPackage(package.packageID, true, false);
        }
        emit dataChanged(createIndex(0, 0),
                         createIndex(m_packages.size(), 0));
    }
    emit changed(!m_checkedPackages.isEmpty());
}

QStringList PackageModel::selectedPackagesToInstall() const
{
    QStringList list;
    foreach (const InternalPackage &package, m_checkedPackages) {
        if (package.info == Transaction::InfoAvailable ||
                package.info == Transaction::InfoCollectionAvailable) {
            // check what packages are available and marked to be installed
            list << package.packageID;
        }
    }
    return list;
}

QStringList PackageModel::selectedPackagesToRemove() const
{
    QStringList list;
    foreach (const InternalPackage &package, m_checkedPackages) {
        if (package.info == Transaction::InfoInstalled ||
                package.info == Transaction::InfoCollectionInstalled) {
            // check what packages are installed and marked to be removed
            list << package.packageID;
        }
    }
    return list;
}

unsigned long PackageModel::downloadSize() const
{
    unsigned long size = 0;
    foreach (const InternalPackage &package, m_checkedPackages) {
        size += package.size;
    }
    return size;
}

bool PackageModel::allSelected() const
{
    foreach (const InternalPackage &package, m_packages) {
        if (!containsChecked(package.packageID)) {
            return false;
        }
    }
    return true;
}

void PackageModel::setCheckable(bool checkable)
{
    m_checkable = checkable;
}

#include "PackageModel.moc"
