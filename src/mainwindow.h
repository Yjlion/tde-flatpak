/*
 * tdeflatpak - main window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_MAINWINDOW_H
#define TDEFLATPAK_MAINWINDOW_H

#include <tdemainwindow.h>

class KTabWidget;
class TQTimer;
class StorePage;
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

    /** Show the details dialog of @p appId once the catalog is loaded. */
    void showDetails(const TQString &appId);
    /** Open the permission editor for @p appId (after the window is shown). */
    void showPermissions(const TQString &appId);

public slots:
    void refreshAll();
    void showBrowse();
    void showSearch();
    void showUpdates();
    void showRemotes();

private slots:
    void slotCurrentChanged(TQWidget *page);
    void slotInstalledChanged();
    void slotUpdatesAvailable(int count);
    void slotOpenFile();
    void slotReloadCatalog();
    void slotCheckUpdates();
    void slotAddFlathub();
    void slotRemoveUnused();
    void slotPermissions();
    void slotConfigure();
    void slotControlCenter();
    void slotSettingsChanged();
    void slotPeriodicUpdateCheck();
    void slotShowPendingDetails();
    void slotShowPendingPermissions();

private:
    void setupActions();
    void installFlatpakref(const KURL &url);
    void refreshRemotes();

    KTabWidget *m_tabs;
    StorePage *m_store;
    InstalledPage *m_installed;
    SearchPage *m_search;
    UpdatesPage *m_updates;
    RemotesPage *m_remotes;
    TQTimer *m_updateTimer;
    TQString m_pendingDetails;
    TQString m_pendingPermissions;
};

#endif
