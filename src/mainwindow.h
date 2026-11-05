/*
 * tdeflatpak - main window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_MAINWINDOW_H
#define TDEFLATPAK_MAINWINDOW_H

#include <tdemainwindow.h>

class KTabWidget;
class InstalledPage;
class SearchPage;
class UpdatesPage;
class RemotesPage;
class KURL;

class MainWindow : public TDEMainWindow
{
    TQ_OBJECT

public:
    MainWindow();

    /** Open a .flatpakref or .flatpakrepo file (local path or URL). */
    void openFile(const KURL &url);

public slots:
    void refreshAll();
    void showSearch();
    void showUpdates();

private slots:
    void slotCurrentChanged(TQWidget *page);
    void slotInstalledChanged();
    void slotUpdatesAvailable(int count);
    void slotOpenFile();
    void slotReloadCatalog();
    void slotCheckUpdates();
    void slotAddFlathub();
    void slotRemoveUnused();

private:
    void setupActions();
    void installFlatpakref(const KURL &url);

    KTabWidget *m_tabs;
    InstalledPage *m_installed;
    SearchPage *m_search;
    UpdatesPage *m_updates;
    RemotesPage *m_remotes;
};

#endif
