/* This file is part of Apper
 *
 * Copyright (C) 2012 Matthias Klumpp <matthias@tenstral.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef SetupWizard_H
#define SetupWizard_H

#include <KDialog>

namespace Ui {
    class SetupWizard;
}

class SetupWizardPrivate;

class SetupWizard : public KDialog
{
    Q_OBJECT
public:
    SetupWizard(const QString& ipkFName, QWidget *parent = 0);
    virtual ~SetupWizard();

    virtual void slotButtonClicked(int button);

    bool initialize();

    void setCurrentPage(QWidget *widget);

    SetupWizardPrivate *getPriv() { return d; };

private slots:
    void currentPageChanged(int index);

private:
    void constructWizardLayout();

    SetupWizardPrivate *const d;
    Ui::SetupWizard *ui;
};

#endif // SetupWizard_H