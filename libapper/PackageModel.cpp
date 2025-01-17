/***************************************************************************
 *   Copyright (C) 2008-2018 by Daniel Nicoletti                           *
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

#include <Daemon>

#include <QPainter>

#include <KIconLoader>
#include <QLoggingCategory>
#include <PkIcons.h>
#include <KLocalizedString>
#include <KCategorizedSortFilterProxyModel>
#include <KFormat>

#ifdef HAVE_APPSTREAM
#include <AppStreamQt/icon.h>
#include <AppStream.h>
#endif

#define ICON_SIZE 22
#define OVERLAY_SIZE 16

using namespace PackageKit;

Q_DECLARE_LOGGING_CATEGORY(APPER_LIB)

PackageModel::PackageModel(QObject *parent)
: QAbstractItemModel(parent),
  m_finished(false),
  m_checkable(false),
  m_fetchSizesTransaction(nullptr),
  m_fetchInstalledVersionsTransaction(nullptr)
{
    m_installedEmblem = PkIcons::getIcon(QLatin1String("dialog-ok-apply"), QString()).pixmap(16, 16);

    m_roles[SortRole] = "rSort";
    m_roles[NameRole] = "rName";
    m_roles[SummaryRole] = "rSummary";
    m_roles[VersionRole] = "rVersion";
    m_roles[ArchRole] = "rArch";
    m_roles[IconRole] = "rIcon";
    m_roles[IdRole] = "rId";
    m_roles[CheckStateRole] = "rChecked";
    m_roles[InfoRole] = "rInfo";
    m_roles[ApplicationId] = "rApplicationId";
    m_roles[IsPackageRole] = "rIsPackageRole";
    m_roles[PackageName] = "rPackageName";
    m_roles[InfoIconRole] = "rInfoIcon";
}

void PackageModel::addSelectedPackagesFromModel(PackageModel *model)
{
    const QList<InternalPackage> packages = model->internalSelectedPackages();
    for (const InternalPackage &package : packages) {
        addPackage(package.info, package.packageID, package.summary, true);
    }
    finished();
}

void PackageModel::addNotSelectedPackage(Transaction::Info info, const QString &packageID, const QString &summary)
{
    addPackage(info, packageID, summary);
}

void PackageModel::addPackage(Transaction::Info info, const QString &packageID, const QString &summary, bool selected)
{
    if (m_finished) {
        qDebug() << Q_FUNC_INFO << "we are finished calling clear";
        clear();
    }

    switch(info) {
    case Transaction::InfoBlocked:
    case Transaction::InfoFinished:
    case Transaction::InfoCleanup:
        return;
    default:
        break;
    }

#ifdef HAVE_APPSTREAM
    QList<AppStream::Component> applications;
    if (!m_checkable) {
        const QString packageName = Transaction::packageName(packageID);
        applications = AppStreamHelper::instance()->applications(packageName);

        for (const AppStream::Component &app : applications) {
            InternalPackage iPackage;
            iPackage.info = info;
            iPackage.packageID = packageID;
            iPackage.pkgName = packageName;
            iPackage.version = Transaction::packageVersion(packageID);
            iPackage.arch = Transaction::packageArch(packageID);
            iPackage.repo = Transaction::packageData(packageID);
            iPackage.isPackage = false;
            if (app.name().isEmpty()) {
                iPackage.displayName = packageName;
            } else {
                iPackage.displayName = app.name();
            }
            if (app.summary().isEmpty()) {
                iPackage.summary = summary;
            } else {
                iPackage.summary = app.summary();
            }

            const QList<AppStream::Icon> icons = app.icons();
            for (const AppStream::Icon &icon : icons) {
                if (icon.url().isEmpty()) {
                    iPackage.icon = icon.name();
                } else {
                    iPackage.icon = icon.url().toLocalFile();
                }
                break;
            }
            iPackage.appId = app.id();
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
        iPackage.pkgName = Transaction::packageName(packageID);
        iPackage.displayName = iPackage.pkgName;
        iPackage.version = Transaction::packageVersion(packageID);
        iPackage.arch = Transaction::packageArch(packageID);
        iPackage.repo = Transaction::packageData(packageID);
        iPackage.summary = summary;

#ifdef HAVE_APPSTREAM
        iPackage.icon = AppStreamHelper::instance()->genericIcon(iPackage.pkgName);

        if (m_checkable) {
            // in case of updates model only check if it's an app
            applications = AppStreamHelper::instance()->applications(iPackage.pkgName);
            if (!applications.isEmpty()) {
                iPackage.isPackage = false;
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

void PackageModel::addSelectedPackage(Transaction::Info info, const QString &packageID, const QString &summary)
{
    addPackage(info, packageID, summary, true);
}

QVariant PackageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant ret;
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case NameCol:
            if (m_checkable) {
                ret = PkStrings::packageQuantity(true,
                                                 m_packages.size(),
                                                 m_checkedPackages.size());
            } else {
                ret = i18n("Name");
            }
            break;
        case VersionCol:
            ret = i18n("Version");
            break;
        case CurrentVersionCol:
            ret = i18n("Installed Version");
            break;
        case ArchCol:
            ret = i18n("Arch");
            break;
        case OriginCol:
            ret = i18n("Origin");
            break;
        case SizeCol:
            ret = i18n("Size");
            break;
        case ActionCol:
            ret = i18n("Action");
            break;
        }
    }
    return ret;
}

int PackageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_finished) {
        return 0;
    }
    return m_packages.size();
}

QModelIndex PackageModel::index(int row, int column, const QModelIndex &parent) const
{
//   kDebug() << parent.isValid() << m_packageCount << row << column;
    // Check to see if the index isn't out of list
    if (!parent.isValid() && m_packages.size() > row) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QModelIndex PackageModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

QHash<int, QByteArray> PackageModel::roleNames() const
{
    return m_roles;
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
            if (containsChecked(package.packageID)) {
                return Qt::Checked;
            }
            return Qt::Unchecked;
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
                QPixmap pixmap;
                if (package.icon.startsWith(QLatin1String("/"))) {
                    pixmap = QPixmap();
                    pixmap.load(package.icon);
                    pixmap = pixmap.scaledToHeight(ICON_SIZE);
                } else {
                    pixmap = KIconLoader::global()->loadIcon(package.icon,
                                                             KIconLoader::NoGroup,
                                                             ICON_SIZE,
                                                             KIconLoader::DefaultState,
                                                             QStringList(),
                                                             nullptr,
                                                             true);
                }

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
            return package.pkgName;
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
            KFormat f;
            return package.size ? f.formatByteSize(package.size) : QString();
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
    if (role == Qt::CheckStateRole && m_packages.size() > index.row()) {
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

void PackageModel::removePackage(const QString &packageID)
{
    int i = 0;
    while (i < m_packages.size()) {
        InternalPackage iPackage = m_packages[i];
        if (iPackage.packageID == packageID &&
                iPackage.info != Transaction::InfoUntrusted) {
            beginRemoveRows(QModelIndex(), i, i);
            m_packages.remove(i);
            endRemoveRows();

            // since we removed one entry we don't
            // need to increase the counter
            continue;
        }
        ++i;
    }
}

void PackageModel::checkAll()
{
    m_checkedPackages.clear();
    for (const InternalPackage &package : qAsConst(m_packages)) {
        checkPackage(package, false);
    }
    emit dataChanged(createIndex(0, 0),
                     createIndex(m_packages.size(), 0));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::clear()
{
    qDebug() << Q_FUNC_INFO;
    beginRemoveRows(QModelIndex(), 0, m_packages.size());
    m_finished = false;
    m_packages.clear();
    m_fetchSizesTransaction = nullptr;
    m_fetchInstalledVersionsTransaction = nullptr;

    if (m_getUpdatesTransaction) {
        m_getUpdatesTransaction->disconnect(this);
        m_getUpdatesTransaction->cancel();
    }
    endRemoveRows();
}

void PackageModel::clearSelectedNotPresent()
{
    auto it = m_checkedPackages.begin();
    while (it != m_checkedPackages.end()) {
        const InternalPackage &package = it.value();

        bool notFound = true;
        for (const InternalPackage &iPackage : qAsConst(m_packages)) {
            if (iPackage.packageID == package.packageID) {
                notFound = false;
                break;
            }
        }

        if (notFound) {
            // Uncheck the package If it's not in the model
            it = m_checkedPackages.erase(it);
            uncheckPackageLogic(package.packageID);
        } else {
            ++it;
        }
    }
}

bool PackageModel::checkable() const
{
    return m_checkable;
}

void PackageModel::uncheckInstalledPackages()
{
    auto it = m_checkedPackages.begin();
    while (it != m_checkedPackages.end()) {
        const InternalPackage &package = it.value();
        if (package.info == Transaction::InfoInstalled ||
                package.info == Transaction::InfoCollectionInstalled) {
            const QString pkgId = it.key();
            it = m_checkedPackages.erase(it);
            uncheckPackageLogic(pkgId, true);
        } else {
            ++it;
        }
    }
}

void PackageModel::uncheckAvailablePackages()
{
    auto it = m_checkedPackages.begin();
    while (it != m_checkedPackages.end()) {
        const InternalPackage &package = it.value();
        if (package.info == Transaction::InfoAvailable ||
                package.info == Transaction::InfoCollectionAvailable) {
            const QString pkgId = it.key();
            it = m_checkedPackages.erase(it);
            uncheckPackageLogic(pkgId, true);
        } else {
            ++it;
        }
    }
}

void PackageModel::finished()
{
    auto trans = qobject_cast<Transaction*>(sender());
    qDebug() << Q_FUNC_INFO << trans << sender();
    if (trans /*== m_getUpdatesTransaction*/) {
//        m_getUpdatesTransaction = 0;
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        disconnect(trans, &Transaction::finished, this, &PackageModel::finished);
    }

    // The whole structure is about to change
    if (!m_packages.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, m_packages.size() - 1);
        m_finished = true;
        endInsertRows();
    }

    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::fetchSizes()
{
    if (m_fetchSizesTransaction) {
        return;
    }

    // get package size
    QStringList pkgs;
    for (const InternalPackage &p : qAsConst(m_packages)) {
        pkgs << p.packageID;
    }
    if (!pkgs.isEmpty()) {
        m_fetchSizesTransaction = Daemon::getDetails(pkgs);
        connect(m_fetchSizesTransaction, &Transaction::details, this, &PackageModel::updateSize);
        connect(m_fetchSizesTransaction, &Transaction::finished, this, &PackageModel::fetchSizesFinished);
    }
}

void PackageModel::fetchSizesFinished()
{
    auto trans = qobject_cast<Transaction*>(sender());
    if (trans) {
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        disconnect(trans, &Transaction::finished, this, &PackageModel::fetchSizesFinished);
    }
    // emit this after all is changed otherwise on large models it will
    // be hell slow...
    emit dataChanged(createIndex(0, SizeCol), createIndex(m_packages.size(), SizeCol));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::updateSize(const PackageKit::Details &details)
{
    // if size is 0 don't waste time looking for the package
    qulonglong size  = details.size();
    if (size == 0) {
        return;
    }

    for (int i = 0; i < m_packages.size(); ++i) {
        const QString packageId = details.packageId();
        if (packageId == m_packages[i].packageID) {
            m_packages[i].size = size;
            if (m_checkable) {
                // updates the checked packages as well
                if (m_checkedPackages.contains(packageId)) {
                    // Avoid checking packages that aren't checked
                    m_checkedPackages[packageId].size = size;
                }
                break;
            }

#ifdef HAVE_APPSTREAM
            if (m_checkable) {
                // checkable models don't have duplicated package ids
                // so don't waste time scanning all list
                break;
            }
#else
            // Without AppStream we don't have duplicated package ids
            break;
#endif // HAVE_APPSTREAM
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
    for (const InternalPackage &p : qAsConst(m_packages)) {
        pkgs << p.pkgName;
    }

    if (!pkgs.isEmpty()) {
        m_fetchInstalledVersionsTransaction = Daemon::resolve(pkgs, Transaction::FilterInstalled);;
        connect(m_fetchInstalledVersionsTransaction, &Transaction::package, this, &PackageModel::updateCurrentVersion);
        connect(m_fetchInstalledVersionsTransaction, &Transaction::finished, this, &PackageModel::fetchCurrentVersionsFinished);
    }
}

void PackageModel::fetchCurrentVersionsFinished()
{
    auto trans = qobject_cast<Transaction*>(sender());
    if (trans) {
        // When pkd dies this method is called twice
        // pk-qt2 bug..
        disconnect(trans, &Transaction::finished, this, &PackageModel::fetchCurrentVersionsFinished);
    }
    // emit this after all is changed otherwise on large models it will
    // be hell slow...
    emit dataChanged(createIndex(0, CurrentVersionCol), createIndex(m_packages.size(), CurrentVersionCol));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::updateCurrentVersion(Transaction::Info info, const QString &packageID, const QString &summary)
{
    Q_UNUSED(info)
    Q_UNUSED(summary)
    // if current version is empty don't waste time looking
    if (!Transaction::packageVersion(packageID).isEmpty()) {
        for (int i = 0; i < m_packages.size(); ++i) {
            if (Transaction::packageName(packageID) == m_packages[i].pkgName &&
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
    m_getUpdatesTransaction = Daemon::getUpdates();
    if (selected) {
        connect(m_getUpdatesTransaction, &Transaction::package, this, &PackageModel::addSelectedPackage);
    } else {
        connect(m_getUpdatesTransaction, &Transaction::package, this, &PackageModel::addNotSelectedPackage);
    }
//    connect(m_getUpdatesTransaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
//            m_busySeq, SLOT(stop()));
//    connect(m_getUpdatesTransaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
//            this, SLOT(finished()));
    // This is required to estimate download size
    connect(m_getUpdatesTransaction, &Transaction::finished, this, &PackageModel::fetchSizes);

    if (fetchCurrentVersions) {
        connect(m_getUpdatesTransaction, &Transaction::finished, this, &PackageModel::fetchCurrentVersions);
    }

    connect(m_getUpdatesTransaction, SIGNAL(finished(PackageKit::Transaction::Exit,uint)),
            this, SLOT(getUpdatesFinished()));
    // get all updates
}

void PackageModel::toggleSelection(const QString &packageID)
{
    if (containsChecked(packageID)) {
        uncheckPackage(packageID, true);
    } else {
        for (const InternalPackage &package : qAsConst(m_packages)) {
            if (package.packageID == packageID) {
                checkPackage(package);
                break;
            }
        }
    }
}

QString PackageModel::selectionStateText() const
{
    return headerData(NameCol, Qt::Horizontal).toString();
}

bool PackageModel::hasChanges() const
{
    return !m_checkedPackages.isEmpty();
}

int PackageModel::countInfo(PackageKit::Transaction::Info info) const
{
    int ret = 0;
    for (const InternalPackage &package : qAsConst(m_packages)) {
        if (package.info == info) {
            ++ret;
        }
    }
    return ret;
}

void PackageModel::checkPackage(const InternalPackage &package, bool emitDataChanged)
{
    QString pkgId = package.packageID;
    if (!containsChecked(pkgId)) {
        m_checkedPackages[pkgId] = package;

        // A checkable model does not have duplicated entries
        if (emitDataChanged || !m_checkable || !m_packages.isEmpty()) {
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
            if (m_finished) {
                emit changed(!m_checkedPackages.isEmpty());
            }
        }
    }
}

void PackageModel::uncheckAll()
{
    auto it = m_checkedPackages.begin();
    while (it != m_checkedPackages.end()) {
        const QString pkgId = it.key();
        it = m_checkedPackages.erase(it);
        uncheckPackageLogic(pkgId, true, false);
    }
    emit dataChanged(createIndex(0, 0),
                     createIndex(m_packages.size(), 0));
    emit changed(!m_checkedPackages.isEmpty());
}

void PackageModel::uncheckPackageDefault(const QString &packageID)
{
    uncheckPackage(packageID);
}

void PackageModel::uncheckPackage(const QString &packageID,
                                  bool forceEmitUnchecked,
                                  bool emitDataChanged)
{
    auto it = m_checkedPackages.find(packageID);
    if (it != m_checkedPackages.end()) {
        m_checkedPackages.erase(it);
        uncheckPackageLogic(packageID, forceEmitUnchecked, emitDataChanged);
    }
}

void PackageModel::uncheckPackageLogic(const QString &packageID, bool forceEmitUnchecked, bool emitDataChanged)
{
    if (forceEmitUnchecked || sender() == nullptr) {
        // The package might be removed by rmSelectedPackage
        // If we don't copy it the browse model won't uncheck there
        // right package
        emit packageUnchecked(packageID);
    }

    if (emitDataChanged || !m_checkable) {
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
        if (m_finished) {
            emit changed(!m_checkedPackages.isEmpty());
        }
    }
}

QList<PackageModel::InternalPackage> PackageModel::internalSelectedPackages() const
{
    QList<InternalPackage> ret;
    QHash<QString, InternalPackage>::const_iterator i = m_checkedPackages.constBegin();
    while (i != m_checkedPackages.constEnd()) {
        ret << i.value();
        ++i;
    }
    return ret;
}

bool PackageModel::containsChecked(const QString &pid) const
{
    return m_checkedPackages.contains(pid);
}

void PackageModel::setAllChecked(bool checked)
{
    if (checked) {
        checkAll();
    } else {
        uncheckAll();
    }
}

QStringList PackageModel::selectedPackagesToInstall() const
{
    QStringList list;
    for (const InternalPackage &package : qAsConst(m_checkedPackages)) {
        if (package.info != Transaction::InfoInstalled &&
                package.info != Transaction::InfoCollectionInstalled) {
            // append the packages are not installed
            list << package.packageID;
        }
    }
    return list;
}

QStringList PackageModel::selectedPackagesToRemove() const
{
    QStringList list;
    for (const InternalPackage &package : qAsConst(m_checkedPackages)) {
        if (package.info == Transaction::InfoInstalled ||
                package.info == Transaction::InfoCollectionInstalled) {
            // check what packages are installed and marked to be removed
            list << package.packageID;
        }
    }
    return list;
}

QStringList PackageModel::packagesWithInfo(Transaction::Info info) const
{
    QStringList list;
    for (const InternalPackage &package : qAsConst(m_packages)) {
        if (package.info == info) {
            // Append to the list if the package matches the info value
            list << package.packageID;
        }
    }
    return list;
}

QStringList PackageModel::packageIDs() const
{
    QStringList list;
    for (const InternalPackage &package : qAsConst(m_packages)) {
        list << package.packageID;
    }
    return list;
}

unsigned long PackageModel::downloadSize() const
{
    unsigned long size = 0;
    for (const InternalPackage &package : qAsConst(m_checkedPackages)) {
        size += package.size;
    }
    return size;
}

bool PackageModel::allSelected() const
{
    for (const InternalPackage &package : qAsConst(m_packages)) {
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

#include "moc_PackageModel.cpp"
