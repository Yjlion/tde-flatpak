/*
 * tdeflatpak - "Updates" page.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_UPDATESPAGE_H
#define TDEFLATPAK_UPDATESPAGE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "flatpaktypes.h"

class TQCheckListItem;
class TQLabel;
class TQListViewItem;
class TDEListView;
class KPushButton;
class FlatpakJob;

class UpdatesPage : public TQWidget
{
    TQ_OBJECT

public:
    UpdatesPage(TQWidget *parent = 0, const char *name = 0);
    ~UpdatesPage();

    int updateCount() const { return m_updates.count(); }

public slots:
    void checkForUpdates();
    void ensureChecked();
    /** Check without a busy cursor (used by the periodic check). */
    void checkInBackground();

signals:
    void modified();
    void updatesAvailable(int count);

protected:
    virtual void customEvent(TQCustomEvent *ev);

private slots:
    void slotUpdateSelected();
    void slotUpdateAll();
    void slotItemClicked(TQListViewItem *);

private:
    void runUpdates(bool onlyChecked);
    void updateButtons();

    TDEListView *m_list;
    TQLabel *m_status;
    KPushButton *m_checkButton;
    KPushButton *m_updateSelectedButton;
    KPushButton *m_updateAllButton;
    FlatpakJob *m_job;
    bool m_checked;
    bool m_quiet;
    FpInstalledRefList m_updates;
    TQMap<TQListViewItem *, int> m_itemIndex;
    TQMap<TQListViewItem *, bool> m_masked;
};

#endif
