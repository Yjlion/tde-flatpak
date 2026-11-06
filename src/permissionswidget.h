/*
 * tdeflatpak - per-application permission editor ("Flatseal for TDE").
 *
 * Shows the sandbox permissions an application requests and lets the user
 * override them for their account (flatpak override --user), plus the
 * portal permissions, the application's data folder and its update mask.
 * Used both by the manager (PermissionsDialog) and the Control Center
 * module.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_PERMISSIONSWIDGET_H
#define TDEFLATPAK_PERMISSIONSWIDGET_H

#include <tqwidget.h>
#include <tqmap.h>

#include <kdialogbase.h>

#include "flatpaktypes.h"

class TQCheckBox;
class TQLabel;
class TQListViewItem;
class TQTabWidget;
class TDEListView;
class KPushButton;
namespace TDEIO { class Job; }

class PermissionsWidget : public TQWidget
{
    TQ_OBJECT

public:
    PermissionsWidget(TQWidget *parent = 0, const char *name = 0);

    /** (Re)read installed applications and override files, discarding unsaved changes. */
    void load();
    /** Write all modified override layers. */
    bool save(TQString &error);
    bool isModified() const;

    /** Select @p appId in the list (empty = all applications). */
    void selectApp(const TQString &appId);
    /** The application list can be hidden when the editor is opened for one app. */
    void setAppListVisible(bool visible);

public slots:
    /** Drop the overrides of the selected application (applied on save). */
    void resetCurrent();

signals:
    void changed(bool modified);
    /** An immediate action changed an installation (update mask). */
    void installationChanged();

private slots:
    void slotAppSelected();
    void slotCheckToggled();
    void slotTabChanged(TQWidget *tab);

    void slotFsAdd();
    void slotFsRemove();
    void slotEnvAdd();
    void slotEnvRemove();
    void slotBusAdd();
    void slotBusRemove();
    void slotListSelectionChanged();

    void slotPortalRevoke();
    void slotPortalRevokeAll();

    void slotOpenDataFolder();
    void slotDeleteData();
    void slotDirSizeResult(TDEIO::Job *job);
    void slotMaskToggled();

private:
    struct AppState {
        AppState() : modified(false) {}
        TQString id;
        TQString name;
        FpPermissions metadata;   // what the app requests
        FpPermissions layer;      // user override layer (edited)
        bool modified;
    };

    struct Check {
        TQString key;     // shared, sockets, devices, features
        TQString value;
    };

    TQWidget *createPermissionsTab();
    TQWidget *createFilesTab();
    TQWidget *createEnvTab();
    TQWidget *createPortalsTab();
    TQWidget *createStorageTab();
    void addCheck(TQWidget *parent, const TQString &key, const TQString &value,
                  const TQString &label, const TQString &whatsThis);

    AppState &current();
    bool isGlobal() const { return m_currentId.isEmpty(); }
    /** metadata of the current app with the (edited) global layer applied */
    FpPermissions currentBase();
    TQStringList &layerList(FpPermissions &p, const TQString &key);

    void showCurrent();
    void fillChecks();
    void fillFiles();
    void fillEnv();
    void fillPortals();
    void fillStorage();
    void markModified();

    TDEListView *m_appList;
    TQLabel *m_header;
    TQLabel *m_fileLabel;
    TQTabWidget *m_tabs;
    KPushButton *m_resetButton;

    TQWidget *m_permTab, *m_filesTab, *m_envTab, *m_portalTab, *m_storageTab;
    TQMap<TQCheckBox *, Check> m_checks;

    TDEListView *m_fsList;
    KPushButton *m_fsRemove;
    TDEListView *m_envList;
    KPushButton *m_envRemove;
    TDEListView *m_busList;
    KPushButton *m_busRemove;

    TDEListView *m_portalList;
    TQLabel *m_portalStatus;
    KPushButton *m_portalRevoke;
    KPushButton *m_portalRevokeAll;
    FpPortalPermissionList m_portalPerms;
    bool m_portalsLoaded;

    TQLabel *m_dataPath;
    TQLabel *m_dataSize;
    KPushButton *m_openData;
    KPushButton *m_deleteData;
    TQCheckBox *m_maskUser;
    TQCheckBox *m_maskSystem;
    TQString m_maskRef[2];      // installed ref per scope, empty if not installed
    TDEIO::Job *m_sizeJob;

    bool m_filling;
    TQString m_currentId;
    TQMap<TQString, AppState> m_state;          // app id ("" = global) -> state
    TQMap<TQListViewItem *, TQString> m_itemId;
    FpInstalledRefList m_installed;
};

/** Modal dialog around PermissionsWidget. */
class PermissionsDialog : public KDialogBase
{
    TQ_OBJECT

public:
    PermissionsDialog(TQWidget *parent, const TQString &appId = TQString::null);

    /** Show the editor for @p appId; returns true if an installation changed. */
    static bool edit(TQWidget *parent, const TQString &appId);

    bool installationChanged() const { return m_installationChanged; }

protected slots:
    virtual void slotOk();
    virtual void slotApply();

private slots:
    void slotChanged(bool modified);
    void slotInstallationChanged();

private:
    bool apply();

    PermissionsWidget *m_widget;
    bool m_installationChanged;
};

#endif
