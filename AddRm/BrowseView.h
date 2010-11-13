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

#ifndef BROWSE_VIEW_H
#define BROWSE_VIEW_H

#include <KCategorizedSortFilterProxyModel>

#include "ui_BrowseView.h"

class KpkPackageModel;

class KpkSearchableTreeView;
class BrowseView : public QWidget, Ui::BrowseView
{
Q_OBJECT
public:
    BrowseView(QWidget *parent = 0);
    ~BrowseView();

    void showInstalledPanel(bool visible);
    void setCategoryModel(QAbstractItemModel *model);
    void setParentCategory(const QModelIndex &index);
    KpkPackageModel*                  model() const;
    KCategorizedSortFilterProxyModel* proxy() const;
    KPixmapSequenceOverlayPainter*    busyCursor() const;

    void disableExportInstalledPB();
    bool goBack();
    void cleanUi();

signals:
    void categoryActivated(const QModelIndex &index);

public slots:
    void enableExportInstalledPB();

private slots:
    void showVersions(bool enabled);
    void showArchs(bool enabled);
    void on_packageView_customContextMenuRequested(const QPoint &pos);
    void on_packageView_activated(const QModelIndex &);
    void on_categoryMvLeft_clicked();
    void on_categoryMvRight_clicked();

    void on_exportInstalledPB_clicked();
    void on_importInstalledPB_clicked();

private:
    bool showPageHeader() const;

    QAction                          *m_showPackageVersion;
    QAction                          *m_showPackageArch;
    KpkPackageModel                  *m_model;
    KCategorizedSortFilterProxyModel *m_proxy;
    KpkSearchableTreeView            *m_packageView;
    KPixmapSequenceOverlayPainter    *m_busySeq;
};

#endif
