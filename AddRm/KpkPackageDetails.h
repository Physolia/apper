/***************************************************************************
 *   Copyright (C) 2009-2010 by Daniel Nicoletti                           *
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

#ifndef KPK_PACKAGE_DETAILS_H
#define KPK_PACKAGE_DETAILS_H

#include <QWidget>
#include <QPackageKit>

#include <KTextBrowser>
#include <QPlainTextEdit>
#include <QListView>

#include "ui_KpkPackageDetails.h"

using namespace PackageKit;

class KpkSimplePackageModel;
class KpkPackageDetails : public QWidget, Ui::KpkPackageDetails
{
Q_OBJECT
public:
    KpkPackageDetails(QSharedPointer<PackageKit::Package>package, const Enum::Roles &roles, QWidget *parent = 0);
    ~KpkPackageDetails();

private slots:
    void description(QSharedPointer<PackageKit::Package>package);
    void files(QSharedPointer<PackageKit::Package>package, const QStringList &files);

    void getDetailsFinished(PackageKit::Enum::Exit status, uint runtime);
    void getFilesFinished(PackageKit::Enum::Exit status, uint runtime);
    void getDependsFinished(PackageKit::Enum::Exit status, uint runtime);
    void getRequiresFinished(PackageKit::Enum::Exit status, uint runtime);

    void on_descriptionTB_clicked();
    void on_fileListTB_clicked();
    void on_dependsOnTB_clicked();
    void on_requiredByTB_clicked();

private:
    QSharedPointer<PackageKit::Package>m_package;

    KpkSimplePackageModel *m_pkg_model_dep;
    KpkSimplePackageModel *m_pkg_model_req;

    KTextBrowser   *descriptionKTB;
    QPlainTextEdit *filesPTE;
    QListView      *dependsOnLV;
    QListView      *requiredByLV;
    QWidget        *currentWidget;

    bool m_gettingOrGotDescription;
    bool m_gettingOrGotFiles;
    bool m_gettingOrGotDepends;
    bool m_gettingOrGotRequires;

    void setCurrentWidget(QWidget *widget);

    void getDetails(QSharedPointer<PackageKit::Package>p);
    void getFiles(QSharedPointer<PackageKit::Package>p);
    void getDepends(QSharedPointer<PackageKit::Package>p);
    void getRequires(QSharedPointer<PackageKit::Package>p);
};

#endif
