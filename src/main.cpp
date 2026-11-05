/*
 * tdeflatpak - Flatpak manager for the Trinity Desktop Environment
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "mainwindow.h"
#include "flatpakbackend.h"

#include <tdeaboutdata.h>
#include <tdeapplication.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>
#include <kurl.h>

static const char description[] = I18N_NOOP("Manage Flatpak applications, runtimes and remotes");

static const char disclaimer[] = I18N_NOOP(
    "DISCLAIMER: This application was largely \"vibe coded\" with the help of an AI "
    "coding assistant. It has not been thoroughly reviewed or tested, may contain bugs "
    "and can modify the Flatpak installations on your system. It is provided as-is, "
    "without any warranty. Use it at your own risk.");

static TDECmdLineOptions options[] =
{
    { "+[file]", I18N_NOOP(".flatpakref or .flatpakrepo file to open"), 0 },
    { "updates", I18N_NOOP("Show the updates page"), 0 },
    { "search", I18N_NOOP("Show the search page"), 0 },
    TDECmdLineLastOption
};

int main(int argc, char **argv)
{
    TDEAboutData about("tdeflatpak", I18N_NOOP("Flatpak Manager"), TDEFLATPAK_VERSION,
                       description, TDEAboutData::License_GPL_V2,
                       "(c) 2026 The tdeflatpak authors", disclaimer,
                       "https://github.com/Yjlion/tde-flatpak");
    about.addAuthor("tdeflatpak contributors", I18N_NOOP("Developer"));
    about.setProductName("tdeflatpak");

    TDECmdLineArgs::init(argc, argv, &about);
    TDECmdLineArgs::addCmdLineOptions(options);

    TDEApplication app;
    MainWindow *win = new MainWindow();
    app.setMainWidget(win);
    win->show();

    TDECmdLineArgs *args = TDECmdLineArgs::parsedArgs();
    if (args->isSet("updates")) {
        win->showUpdates();
    } else if (args->isSet("search")) {
        win->showSearch();
    }
    if (args->count() > 0) {
        win->openFile(args->url(0));
    }
    args->clear();

    return app.exec();
}
