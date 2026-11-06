/*
 * tdeflatpak - "Search" page: browse and install applications from remotes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_SEARCHPAGE_H
#define TDEFLATPAK_SEARCHPAGE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "flatpaktypes.h"

class TQLabel;
class TQListViewItem;
class TQTimer;
class TDEListView;
class KLineEdit;
class KPushButton;
class FlatpakJob;

class SearchPage : public TQWidget
{
    TQ_OBJECT

public:
    SearchPage(TQWidget *parent = 0, const char *name = 0);
    ~SearchPage();

    /** Update the set of installed refs used to mark installed results. */
    void setInstalled(const FpInstalledRefList &refs);

public slots:
    /** Load the catalog if not done yet. */
    void ensureLoaded();
    /** Reload the catalog, refreshing AppStream data from the network. */
    void reloadCatalog();

signals:
    void modified();

protected:
    virtual void customEvent(TQCustomEvent *ev);

private slots:
    void slotSearch();
    void slotTextChanged(const TQString &);
    void slotSelectionChanged();
    void slotInstall();
    void slotDoubleClicked(TQListViewItem *item);

private:
    void loadCatalog(bool force);
    void setBusy(bool busy, const TQString &text = TQString::null);

    KLineEdit *m_query;
    KPushButton *m_reloadButton;
    KPushButton *m_installButton;
    TDEListView *m_list;
    TQLabel *m_status;
    TQLabel *m_details;
    TQTimer *m_searchTimer;

    FlatpakJob *m_job;
    bool m_loaded;
    FpAppList m_apps;
    TQMap<TQString, bool> m_installed;      // fpRefKey -> true
    TQMap<TQListViewItem *, const FpApp *> m_itemApp;
};

#endif
