/*
 * tdeflatpak - "Search" page: search and install applications from remotes.
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

class SearchPage : public TQWidget
{
    TQ_OBJECT

public:
    SearchPage(TQWidget *parent = 0, const char *name = 0);

public slots:
    /** Load the catalog if not done yet. */
    void ensureLoaded();
    /** Reload the catalog, refreshing AppStream data from the network. */
    void reloadCatalog();

signals:
    void modified();

private slots:
    void slotSearch();
    void slotTextChanged(const TQString &);
    void slotSelectionChanged();
    void slotInstall();
    void slotDetails();
    void slotDoubleClicked(TQListViewItem *item);
    void slotContextMenu(TDEListView *, TQListViewItem *item, const TQPoint &pos);
    void slotCatalogLoading();
    void slotCatalogProgress(const TQString &remote);
    void slotCatalogLoaded(bool ok);

private:
    void setBusy(bool busy);

    KLineEdit *m_query;
    KPushButton *m_reloadButton;
    KPushButton *m_detailsButton;
    KPushButton *m_installButton;
    TDEListView *m_list;
    TQLabel *m_status;
    TQLabel *m_details;
    TQTimer *m_searchTimer;
    bool m_busy;

    TQMap<TQListViewItem *, const FpApp *> m_itemApp;
};

#endif
