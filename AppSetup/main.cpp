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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KDebug>
#include <KApplication>
#include <KMessageBox>
#include <QFileInfo>
#include <glib-object.h>

#include "SetupWizard.h"

int main(int argc, char** argv)
{
    KAboutData aboutData("appsetup-kde", 0, ki18n("KDE Application Installer"), "0.1",
                         ki18n("KDE Application Installer"), KAboutData::License_GPL,
                         ki18n("(C) 2012, Matthias Klumpp"));

    aboutData.addAuthor(ki18nc("@info:credit", "Daniel Nicoletti"), ki18n("Developer"),
                        "dantti12@gmail.com");
    aboutData.addAuthor(ki18nc("@info:credit", "Matthias Klumpp"), ki18n("Developer"),
                        "matthias@tenstral.net");
    aboutData.setProductName("apper/listaller");

    KCmdLineArgs::init(argc, argv, &aboutData);
    // Add --debug as commandline option
    KCmdLineOptions options;
    options.add("debug", ki18n("Show debugging information"));
    options.add("+file", ki18n("IPK package filename"));
    KCmdLineArgs::addCmdLineOptions(options);

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    QString fname = "";
    for(int i = 0; i < args->count(); i++) {
        fname = args->arg(i);
        QFileInfo file(fname);
        if (!file.exists())
            fname = "";
        else
            break;
    }
    args->clear();

    // Initialize GObject type system
    g_type_init();

    KApplication app;

    // Check if we have a package
    if (fname == "") {
        KMessageBox::sorry (0, "Sorry, we didn't get an existing IPK package as parameter.\n",
                            "Package not found!");
        return 1;
    }

    // Create & run the setup wizard
    SetupWizard *wizard = new SetupWizard(fname);
    wizard->show();
    return app.exec();
}