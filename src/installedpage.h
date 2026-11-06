/*
 * tdeflatpak - "Installed" page: installed applications and runtimes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_INSTALLEDPAGE_H
#define TDEFLATPAK_INSTALLEDPAGE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "flatpaktypes.h"

class TQCheckBox;
class TQLabel;
class TQListViewItem;
class TDEListView;
class TDEListViewSearchLine;
class KPushButton;

class InstalledPage : public TQWidget
{
    TQ_OBJECT

public:
    InstalledPage(TQWidget *parent = 0, const char *name = 0);

    const FpInstalledRefList &installedRefs() const { return m_refs; }

public slots:
    void refresh();

signals:
    /** The set of installed refs changed (after refresh). */
    void installedChanged();
    /** A transaction modified the installations. */
    void modified();

private slots:
    void slotSelectionChanged();
    void slotRun();
    void slotUninstall();
    void slotDoubleClicked(TQListViewItem *item);
    void slotShowRuntimesToggled(bool on);
    void slotDetails();
    void slotPermissions();
    void slotToggleMask();
    void slotTogglePin();
    void slotContextMenu(TDEListView *, TQListViewItem *item, const TQPoint &pos);

private:
    void populate();
    const FpInstalledRef *currentRef() const;

    TDEListView *m_list;
    TDEListViewSearchLine *m_search;
    TQCheckBox *m_showRuntimes;
    KPushButton *m_runButton;
    KPushButton *m_detailsButton;
    KPushButton *m_permButton;
    KPushButton *m_uninstallButton;
    KPushButton *m_refreshButton;
    TQLabel *m_summary;
    FpInstalledRefList m_refs;
    TQMap<TQListViewItem *, int> m_itemIndex;
    TQMap<TQString, bool> m_masked;   // fpRefKey -> true
    TQMap<TQString, bool> m_pinned;   // fpRefKey -> true
};

#endif
