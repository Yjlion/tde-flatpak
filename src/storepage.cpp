/*
 * tdeflatpak - "Browse" page: an app store view of the catalog.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "storepage.h"
#include "appactions.h"
#include "appdetailsdialog.h"
#include "catalog.h"
#include "flathubapi.h"
#include "iconcache.h"

#include <tqapplication.h>
#include <tqcursor.h>
#include <tqimage.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpainter.h>
#include <tqsplitter.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <kdialog.h>
#include <kiconloader.h>
#include <kiconview.h>
#include <tdelistbox.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdepopupmenu.h>
#include <kpushbutton.h>

static const int MaxTiles = 300;
static const int TileIconSize = 48;

namespace
{

struct Section {
    const char *key;
    const char *label;
    const char *icons[4];
};

const Section featured[] = {
    { "collection:popular", I18N_NOOP("Popular"), { "emblem-favorite", "favorites", 0, 0 } },
    { "collection:trending", I18N_NOOP("Trending"), { "go-up", "up", 0, 0 } },
    { "collection:recently-added", I18N_NOOP("New"), { "document-new", "filenew", 0, 0 } },
    { "collection:recently-updated", I18N_NOOP("Recently Updated"), { "view-refresh", "reload", 0, 0 } },
    { 0, 0, { 0, 0, 0, 0 } }
};

// freedesktop names first, then the Trinity menu icons, then icons every
// tdelibs installation has
const Section categories[] = {
    { "cat:AudioVideo", I18N_NOOP("Audio & Video"), { "applications-multimedia", "package_multimedia", "kmix", 0 } },
    { "cat:Development", I18N_NOOP("Development"), { "applications-development", "package_development", "utilities-terminal", 0 } },
    { "cat:Education", I18N_NOOP("Education"), { "applications-education", "package_edutainment", "accessories-dictionary", 0 } },
    { "cat:Game", I18N_NOOP("Games"), { "applications-games", "package_games", "folder_red", 0 } },
    { "cat:Graphics", I18N_NOOP("Graphics & Photography"), { "applications-graphics", "package_graphics", "folder_image", 0 } },
    { "cat:Network", I18N_NOOP("Internet & Communication"), { "applications-internet", "package_network", "www", 0 } },
    { "cat:Office", I18N_NOOP("Office"), { "applications-office", "package_wordprocessing", "folder_wordprocessing", 0 } },
    { "cat:Science", I18N_NOOP("Science"), { "applications-science", "kalzium", "accessories-calculator", 0 } },
    { "cat:System", I18N_NOOP("System"), { "applications-system", "package_system", "utilities-system-monitor", 0 } },
    { "cat:Utility", I18N_NOOP("Utilities"), { "applications-utilities", "package_utilities", "accessories-character-map", 0 } },
    { "cat:Other", I18N_NOOP("Other"), { "applications-other", "package", "folder_grey", 0 } },
    { 0, 0, { 0, 0, 0, 0 } }
};

const char *const mainCategories[] = {
    "AudioVideo", "Audio", "Video", "Development", "Education", "Game", "Graphics",
    "Network", "Office", "Science", "System", "Utility", 0
};

TQPixmap themedIcon(const char *const names[], int size)
{
    for (int i = 0; names[i]; ++i) {
        TQPixmap p = TDEGlobal::iconLoader()->loadIcon(names[i], TDEIcon::Small, size,
                                                       TDEIcon::DefaultState, 0, true);
        if (!p.isNull()) {
            return p;
        }
    }
    return SmallIcon("folder", size);
}

bool inCategory(const FpApp &app, const TQString &cat)
{
    if (cat == "Other") {
        for (int i = 0; mainCategories[i]; ++i) {
            if (app.categories.contains(mainCategories[i])) {
                return false;
            }
        }
        return true;
    }
    if (cat == "AudioVideo") {
        return app.categories.contains("AudioVideo") || app.categories.contains("Audio")
               || app.categories.contains("Video");
    }
    return app.categories.contains(cat);
}

}

StorePage::StorePage(TQWidget *parent, const char *name)
    : TQWidget(parent, name),
      m_sectionsBuilt(false)
{
    TQHBoxLayout *layout = new TQHBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
    TQSplitter *splitter = new TQSplitter(TQt::Horizontal, this);
    layout->addWidget(splitter);

    m_sections = new TDEListBox(splitter);
    m_sections->setMinimumWidth(170);

    TQWidget *right = new TQWidget(splitter);
    TQVBoxLayout *rl = new TQVBoxLayout(right, 0, KDialog::spacingHint());
    TQHBoxLayout *top = new TQHBoxLayout(rl, KDialog::spacingHint());
    TQLabel *fl = new TQLabel(i18n("&Filter:"), right);
    top->addWidget(fl);
    m_filter = new KLineEdit(right);
    m_filter->setClickMessage(i18n("Name or keyword"));
    fl->setBuddy(m_filter);
    top->addWidget(m_filter, 1);

    m_view = new TDEIconView(right);
    m_view->setArrangement(TQIconView::LeftToRight);
    m_view->setResizeMode(TQIconView::Adjust);
    m_view->setItemTextPos(TQIconView::Bottom);
    m_view->setGridX(140);
    m_view->setGridY(TileIconSize + 52);
    m_view->setWordWrapIconText(true);
    m_view->setItemsMovable(false);
    m_view->setAutoArrange(true);
    m_view->setSorting(false);
    m_view->setSelectionMode(TQIconView::Extended);
    m_view->setShowToolTips(true);
    m_view->setMode(TDEIconView::Select);
    rl->addWidget(m_view, 1);

    TQHBoxLayout *bottom = new TQHBoxLayout(rl, KDialog::spacingHint());
    m_status = new TQLabel(right);
    bottom->addWidget(m_status, 1);
    m_detailsButton = new KPushButton(SmallIconSet("help-about"), i18n("&Details..."), right);
    m_installButton = new KPushButton(SmallIconSet("go-down"), i18n("&Install"), right);
    bottom->addWidget(m_detailsButton);
    bottom->addWidget(m_installButton);

    TQValueList<int> sizes;
    sizes << 190 << 600;
    splitter->setSizes(sizes);
    splitter->setResizeMode(m_sections, TQSplitter::KeepSize);

    m_filterTimer = new TQTimer(this);

    Catalog *catalog = Catalog::self();
    connect(catalog, TQ_SIGNAL(loadingStarted()), this, TQ_SLOT(slotCatalogLoading()));
    connect(catalog, TQ_SIGNAL(progress(const TQString &)), this, TQ_SLOT(slotCatalogProgress(const TQString &)));
    connect(catalog, TQ_SIGNAL(loaded(bool)), this, TQ_SLOT(slotCatalogLoaded(bool)));
    connect(catalog, TQ_SIGNAL(installedChanged()), this, TQ_SLOT(populate()));

    FlathubApi *api = FlathubApi::self();
    connect(api, TQ_SIGNAL(collectionReady(const TQString &, const FpStoreHitList &)),
            this, TQ_SLOT(slotCollectionReady(const TQString &, const FpStoreHitList &)));
    connect(api, TQ_SIGNAL(requestFailed(const TQString &, const TQString &, const TQString &)),
            this, TQ_SLOT(slotCollectionFailed(const TQString &, const TQString &, const TQString &)));

    connect(m_sections, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotSectionChanged()));
    connect(m_filter, TQ_SIGNAL(textChanged(const TQString &)), this, TQ_SLOT(slotFilterChanged(const TQString &)));
    connect(m_filterTimer, TQ_SIGNAL(timeout()), this, TQ_SLOT(populate()));
    connect(m_view, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotSelectionChanged()));
    connect(m_view, TQ_SIGNAL(executed(TQIconViewItem *)), this, TQ_SLOT(slotExecuted(TQIconViewItem *)));
    connect(m_view, TQ_SIGNAL(returnPressed(TQIconViewItem *)), this, TQ_SLOT(slotExecuted(TQIconViewItem *)));
    connect(m_view, TQ_SIGNAL(contextMenuRequested(TQIconViewItem *, const TQPoint &)),
            this, TQ_SLOT(slotContextMenu(TQIconViewItem *, const TQPoint &)));
    connect(m_detailsButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotDetails()));
    connect(m_installButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotInstall()));

    m_status->setText(i18n("The application catalog has not been loaded yet."));
    slotSelectionChanged();
}

void StorePage::buildSections()
{
    const TQString current = m_sections->currentItem() >= 0 && m_sections->currentItem() < (int)m_sectionKeys.count()
                                 ? m_sectionKeys[m_sections->currentItem()] : TQString::null;
    m_sections->blockSignals(true);
    m_sections->clear();
    m_sectionKeys.clear();

    if (Catalog::self()->hasFlathub() && FlathubApi::enabled()) {
        for (int i = 0; featured[i].key; ++i) {
            m_sections->insertItem(themedIcon(featured[i].icons, 22), i18n(featured[i].label));
            m_sectionKeys.append(featured[i].key);
        }
    }
    for (int i = 0; categories[i].key; ++i) {
        m_sections->insertItem(themedIcon(categories[i].icons, 22), i18n(categories[i].label));
        m_sectionKeys.append(categories[i].key);
    }

    int idx = m_sectionKeys.findIndex(current);
    m_sections->setCurrentItem(idx >= 0 ? idx : 0);
    m_sections->setSelected(idx >= 0 ? idx : 0, true);
    m_sections->blockSignals(false);
    m_sectionsBuilt = true;
}

void StorePage::ensureLoaded()
{
    if (!m_sectionsBuilt) {
        buildSections();
    }
    if (Catalog::self()->isLoaded()) {
        slotSectionChanged();
    } else {
        Catalog::self()->ensureLoaded();
    }
}

void StorePage::settingsChanged()
{
    if (m_sectionsBuilt) {
        buildSections();
        populate();
    }
}

void StorePage::slotCatalogLoading()
{
    m_view->clear();
    m_itemApp.clear();
    m_status->setText(i18n("Loading application catalog..."));
    slotSelectionChanged();
}

void StorePage::slotCatalogProgress(const TQString &remote)
{
    m_status->setText(i18n("Downloading catalog of %1...").arg(remote));
}

void StorePage::slotCatalogLoaded(bool ok)
{
    if (!ok) {
        m_status->setText(Catalog::self()->lastError().contains("No enabled remotes")
                              ? i18n("No remotes are configured. Add one (e.g. Flathub) on the Remotes page.")
                              : i18n("Could not load the catalog: %1").arg(Catalog::self()->lastError()));
    }
    buildSections();
    if (isVisible()) {
        slotSectionChanged();
    } else {
        populate();
    }
}

void StorePage::slotSectionChanged()
{
    const int idx = m_sections->currentItem();
    if (idx < 0 || idx >= (int)m_sectionKeys.count()) {
        return;
    }
    const TQString key = m_sectionKeys[idx];
    if (key.startsWith("collection:")) {
        const TQString name = key.mid(11);
        if (!m_collections.contains(name)) {
            m_collectionErrors.remove(name);
            FlathubApi::self()->fetchCollection(name);
        }
    }
    populate();
}

void StorePage::slotCollectionReady(const TQString &name, const FpStoreHitList &hits)
{
    m_collections[name] = hits;
    const int idx = m_sections->currentItem();
    if (idx >= 0 && idx < (int)m_sectionKeys.count() && m_sectionKeys[idx] == "collection:" + name) {
        populate();
    }
}

void StorePage::slotCollectionFailed(const TQString &kind, const TQString &key, const TQString &error)
{
    if (kind != "collection") {
        return;
    }
    m_collectionErrors[key] = error;
    const int idx = m_sections->currentItem();
    if (idx >= 0 && idx < (int)m_sectionKeys.count() && m_sectionKeys[idx] == "collection:" + key) {
        populate();
    }
}

void StorePage::slotFilterChanged(const TQString &)
{
    m_filterTimer->start(300, true);
}

TQPixmap StorePage::tileIcon(const FpApp &app)
{
    TQPixmap pix = IconCache::load(app.iconPath, TileIconSize, "application-x-executable");
    if (!Catalog::self()->isInstalled(app)) {
        return pix;
    }
    // "installed" badge in the bottom right corner
    static const char *const badgeNames[] = { "dialog-ok", "button_ok", "apply", 0 };
    TQPixmap badge = themedIcon(badgeNames, 16);
    TQPixmap out;
    TQImage img = pix.convertToImage().convertDepth(32);
    TQImage canvas(TileIconSize, TileIconSize, 32);
    canvas.setAlphaBuffer(true);
    canvas.fill(0);
    const int x0 = (TileIconSize - img.width()) / 2, y0 = (TileIconSize - img.height()) / 2;
    bitBlt(&canvas, x0, y0, &img, 0, 0, -1, -1, 0);
    TQImage b = badge.convertToImage().convertDepth(32);
    // alpha-blend the badge
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            const TQRgb src = b.pixel(x, y);
            const int a = b.hasAlphaBuffer() ? tqAlpha(src) : 255;
            if (a == 0) {
                continue;
            }
            const int cx = TileIconSize - b.width() + x, cy = TileIconSize - b.height() + y;
            const TQRgb dst = canvas.pixel(cx, cy);
            const int da = tqAlpha(dst);
            const int oa = a + da * (255 - a) / 255;
            if (oa == 0) {
                continue;
            }
            const int r = (tqRed(src) * a + tqRed(dst) * da * (255 - a) / 255) / oa;
            const int g = (tqGreen(src) * a + tqGreen(dst) * da * (255 - a) / 255) / oa;
            const int bl = (tqBlue(src) * a + tqBlue(dst) * da * (255 - a) / 255) / oa;
            canvas.setPixel(cx, cy, tqRgba(r, g, bl, oa));
        }
    }
    out.convertFromImage(canvas);
    return out;
}

void StorePage::populate()
{
    m_view->clear();
    m_itemApp.clear();

    Catalog *catalog = Catalog::self();
    const int idx = m_sections->currentItem();
    if (!catalog->isLoaded() || idx < 0 || idx >= (int)m_sectionKeys.count()) {
        slotSelectionChanged();
        return;
    }
    const TQString key = m_sectionKeys[idx];
    const TQStringList words = TQStringList::split(' ', m_filter->text().stripWhiteSpace());

    TQValueList<const FpApp *> apps;
    int skipped = 0;
    if (key.startsWith("collection:")) {
        const TQString name = key.mid(11);
        if (!m_collections.contains(name)) {
            m_status->setText(m_collectionErrors.contains(name)
                                  ? i18n("Could not load the list from Flathub: %1").arg(m_collectionErrors[name])
                                  : i18n("Loading the list from Flathub..."));
            slotSelectionChanged();
            return;
        }
        const FpStoreHitList &hits = m_collections[name];
        for (FpStoreHitList::ConstIterator it = hits.begin(); it != hits.end(); ++it) {
            const FpApp *a = catalog->findById((*it).appId);
            if (a && catalog->isFlathubRemote(a->scope, a->remote)) {
                apps.append(a);
            } else {
                ++skipped;   // not in the local catalog (e.g. a different architecture)
            }
        }
    } else {
        const TQString cat = key.mid(4);
        TQMap<TQString, const FpApp *> sorted;
        const FpAppList &all = catalog->apps();
        for (FpAppList::ConstIterator it = all.begin(); it != all.end(); ++it) {
            if (inCategory(*it, cat)) {
                // one tile per application id, even if several remotes offer it
                const TQString k = (*it).name.lower() + "\n" + (*it).id;
                if (!sorted.contains(k)) {
                    sorted[k] = &(*it);
                }
            }
        }
        for (TQMap<TQString, const FpApp *>::ConstIterator it = sorted.begin(); it != sorted.end(); ++it) {
            apps.append(*it);
        }
    }

    int shown = 0, matching = 0;
    for (TQValueList<const FpApp *>::ConstIterator it = apps.begin(); it != apps.end(); ++it) {
        const FpApp &a = **it;
        bool ok = true;
        for (TQStringList::ConstIterator w = words.begin(); w != words.end() && ok; ++w) {
            ok = a.name.contains(*w, false) || a.summary.contains(*w, false) || a.id.contains(*w, false);
        }
        if (!ok) {
            continue;
        }
        ++matching;
        if (shown >= MaxTiles) {
            continue;
        }
        TQIconViewItem *item = new TQIconViewItem(m_view, m_view->lastItem(), a.name, tileIcon(a));
        item->setDragEnabled(false);
        item->setDropEnabled(false);
        item->setRenameEnabled(false);
        m_itemApp[item] = &a;
        ++shown;
    }

    if (matching == 0) {
        m_status->setText(words.isEmpty() ? i18n("No applications in this section.") : i18n("No matches."));
    } else if (matching > shown) {
        m_status->setText(i18n("Showing %1 of %2 applications. Type to filter.").arg(shown).arg(matching));
    } else {
        m_status->setText(i18n("%n application", "%n applications", shown));
    }
    slotSelectionChanged();
}

TQValueList<const FpApp *> StorePage::selectedApps() const
{
    TQValueList<const FpApp *> out;
    for (TQIconViewItem *i = m_view->firstItem(); i; i = i->nextItem()) {
        if (i->isSelected() && m_itemApp.contains(i)) {
            out.append(m_itemApp[i]);
        }
    }
    return out;
}

void StorePage::slotSelectionChanged()
{
    const TQValueList<const FpApp *> sel = selectedApps();
    int installable = 0;
    for (TQValueList<const FpApp *>::ConstIterator it = sel.begin(); it != sel.end(); ++it) {
        if (!Catalog::self()->isInstalled(**it)) {
            ++installable;
        }
    }
    m_installButton->setEnabled(installable > 0);
    m_detailsButton->setEnabled(sel.count() == 1);

    // summary of the current tile as tooltip-like status
    if (sel.count() == 1) {
        const FpApp *a = sel.first();
        m_status->setText(TQString("<b>%1</b> &mdash; %2")
                              .arg(TQStyleSheet::escape(a->name)).arg(TQStyleSheet::escape(a->summary)));
    }
}

void StorePage::slotExecuted(TQIconViewItem *item)
{
    if (!item || !m_itemApp.contains(item)) {
        return;
    }
    if (AppDetailsDialog::showDetails(this, m_itemApp[item])) {
        emit modified();
    }
}

void StorePage::slotDetails()
{
    const TQValueList<const FpApp *> sel = selectedApps();
    if (sel.count() == 1 && AppDetailsDialog::showDetails(this, sel.first())) {
        emit modified();
    }
}

void StorePage::slotInstall()
{
    TQValueList<const FpApp *> sel = selectedApps();
    FpAppPtrList list;
    for (TQValueList<const FpApp *>::ConstIterator it = sel.begin(); it != sel.end(); ++it) {
        if (!Catalog::self()->isInstalled(**it)) {
            list.append(*it);
        }
    }
    if (AppActions::install(this, list)) {
        emit modified();
    }
}

void StorePage::slotContextMenu(TQIconViewItem *item, const TQPoint &pos)
{
    if (!item || !m_itemApp.contains(item)) {
        return;
    }
    if (!item->isSelected()) {
        m_view->clearSelection();
        item->setSelected(true);
    }
    TDEPopupMenu menu(this);
    menu.insertItem(SmallIconSet("help-about"), i18n("&Details..."), this, TQ_SLOT(slotDetails()));
    int install = menu.insertItem(SmallIconSet("go-down"), i18n("&Install"), this, TQ_SLOT(slotInstall()));
    menu.setItemEnabled(install, m_installButton->isEnabled());
    menu.exec(pos);
}

#include "storepage.moc"
