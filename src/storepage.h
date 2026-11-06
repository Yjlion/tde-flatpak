/*
 * tdeflatpak - "Browse" page: an app store view of the catalog.
 *
 * Categories come from the local AppStream data of every remote, so they
 * work offline and for any repository.  For Flathub, the Popular, Trending,
 * New and Recently Updated lists of the Flathub web API are shown too.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_STOREPAGE_H
#define TDEFLATPAK_STOREPAGE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "flatpaktypes.h"

class TQIconViewItem;
class TQLabel;
class TQListBoxItem;
class TQTimer;
class TDEIconView;
class TDEListBox;
class KLineEdit;
class KPushButton;

class StorePage : public TQWidget
{
    TQ_OBJECT

public:
    StorePage(TQWidget *parent = 0, const char *name = 0);

public slots:
    /** Load the catalog (if needed) and show the current section. */
    void ensureLoaded();
    /** Re-read settings that affect the page (the Flathub web service). */
    void settingsChanged();

signals:
    void modified();

private slots:
    void slotCatalogLoading();
    void slotCatalogProgress(const TQString &remote);
    void slotCatalogLoaded(bool ok);
    void slotSectionChanged();
    void slotCollectionReady(const TQString &name, const FpStoreHitList &hits);
    void slotCollectionFailed(const TQString &kind, const TQString &key, const TQString &error);
    void slotFilterChanged(const TQString &);
    void slotSelectionChanged();
    void slotExecuted(TQIconViewItem *item);
    void slotContextMenu(TQIconViewItem *item, const TQPoint &pos);
    void slotInstall();
    void slotDetails();
    void populate();

private:
    void buildSections();
    TQPixmap tileIcon(const FpApp &app);
    TQValueList<const FpApp *> selectedApps() const;

    TDEListBox *m_sections;
    KLineEdit *m_filter;
    TDEIconView *m_view;
    TQLabel *m_status;
    KPushButton *m_detailsButton;
    KPushButton *m_installButton;
    TQTimer *m_filterTimer;

    TQStringList m_sectionKeys;                 // index -> "collection:popular" / "cat:Graphics"
    TQMap<TQString, FpStoreHitList> m_collections;
    TQMap<TQString, TQString> m_collectionErrors;
    TQMap<TQIconViewItem *, const FpApp *> m_itemApp;
    bool m_sectionsBuilt;
};

#endif
