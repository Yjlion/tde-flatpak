/*
 * tdeflatpak - application details: description, screenshots, releases,
 * sizes, download statistics and requested permissions.
 *
 * The dialog fills in progressively: catalog data first, then the full
 * local AppStream entry (parsed in a worker thread), then - for Flathub
 * applications - the Flathub web API results as they arrive.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_APPDETAILSDIALOG_H
#define TDEFLATPAK_APPDETAILSDIALOG_H

#include <kdialogbase.h>

#include "flatpaktypes.h"

class TQLabel;
class KPushButton;
class KTextBrowser;
class LocalDetailsThread;

class AppDetailsDialog : public KDialogBase
{
    TQ_OBJECT

public:
    /**
     * Show details of a catalog entry and/or an installed application.
     * Returns true if the dialog installed or removed something.
     */
    static bool showDetails(TQWidget *parent, const FpApp *app, const FpInstalledRef *installed = 0);

    AppDetailsDialog(TQWidget *parent, const FpApp *app, const FpInstalledRef *installed);
    ~AppDetailsDialog();

    bool modified() const { return m_modified; }

protected:
    virtual void customEvent(TQCustomEvent *ev);

private slots:
    void slotAppstream(const TQString &appId, const FpAppDetails &d);
    void slotSummary(const TQString &appId, const FpAppDetails &d);
    void slotStats(const TQString &appId, long long installs);
    void slotRemoteFailed(const TQString &kind, const TQString &key, const TQString &error);
    void slotImageLoaded(const TQString &url, const TQString &path);
    void slotPrevShot();
    void slotNextShot();

    void slotInstall();
    void slotLaunch();
    void slotUninstall();
    void slotPermissions();

private:
    void updateInstalledState();
    void updateHeader();
    void updateBody();
    void updateButtons();
    void showShot();

    FpApp m_app;
    bool m_hasApp;
    FpInstalledRef m_installed;
    bool m_isInstalled;
    bool m_isFlathub;
    bool m_modified;

    FpAppDetails m_local;       // catalog / local AppStream
    FpAppDetails m_remote;      // Flathub API (appstream)
    FpAppDetails m_summary;     // Flathub API (summary)
    long long m_installs;
    FpPermissions m_installedPerms;
    bool m_hasInstalledPerms;
    bool m_localDone;
    int m_pendingRemote;

    LocalDetailsThread *m_thread;

    TQLabel *m_icon;
    TQLabel *m_title;
    TQLabel *m_shot;
    TQLabel *m_shotCaption;
    KPushButton *m_prev;
    KPushButton *m_next;
    TQWidget *m_shotBox;
    KTextBrowser *m_body;
    KPushButton *m_installButton;
    KPushButton *m_launchButton;
    KPushButton *m_uninstallButton;
    KPushButton *m_permButton;
    int m_shotIndex;
};

#endif
