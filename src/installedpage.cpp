/*
 * tdeflatpak - "Installed" page: installed applications and runtimes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "installedpage.h"
#include "appactions.h"
#include "appdetailsdialog.h"
#include "flatpakbackend.h"
#include "iconcache.h"
#include "permissionswidget.h"

#include <tqcheckbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqstylesheet.h>
#include <tqtooltip.h>

#include <tdeapplication.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <tdeio/global.h>
#include <tdelistview.h>
#include <tdelistviewsearchline.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <tdepopupmenu.h>
#include <kpushbutton.h>

enum { ColName = 0, ColId, ColVersion, ColBranch, ColOrigin, ColInstallation, ColSize, ColNotes };

InstalledPage::InstalledPage(TQWidget *parent, const char *name)
    : TQWidget(parent, name)
{
    TQVBoxLayout *layout = new TQVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());

    TQHBoxLayout *top = new TQHBoxLayout(layout, KDialog::spacingHint());
    TQLabel *filterLabel = new TQLabel(i18n("&Filter:"), this);
    top->addWidget(filterLabel);

    m_list = new TDEListView(this);
    m_search = new TDEListViewSearchLine(this, m_list);
    top->addWidget(m_search, 1);
    filterLabel->setBuddy(m_search);

    m_showRuntimes = new TQCheckBox(i18n("Show &runtimes"), this);
    top->addWidget(m_showRuntimes);

    m_list->addColumn(i18n("Name"));
    m_list->addColumn(i18n("Application ID"));
    m_list->addColumn(i18n("Version"));
    m_list->addColumn(i18n("Branch"));
    m_list->addColumn(i18n("Origin"));
    m_list->addColumn(i18n("Installation"));
    m_list->addColumn(i18n("Size"));
    m_list->addColumn(i18n("Notes"));
    m_list->setColumnAlignment(ColSize, TQt::AlignRight);
    m_list->setSelectionMode(TQListView::Extended);
    m_list->setAllColumnsShowFocus(true);
    m_list->setShowSortIndicator(true);
    m_list->setFullWidth(false);
    layout->addWidget(m_list, 1);

    TQHBoxLayout *bottom = new TQHBoxLayout(layout, KDialog::spacingHint());
    m_summary = new TQLabel(this);
    bottom->addWidget(m_summary, 1);

    m_refreshButton = new KPushButton(SmallIconSet("view-refresh"), i18n("Re&fresh"), this);
    m_runButton = new KPushButton(SmallIconSet("system-run"), i18n("&Run"), this);
    m_detailsButton = new KPushButton(SmallIconSet("help-about"), i18n("&Details..."), this);
    m_permButton = new KPushButton(SmallIconSet("configure"), i18n("&Permissions..."), this);
    m_uninstallButton = new KPushButton(SmallIconSet("edit-delete"), i18n("&Uninstall"), this);
    bottom->addWidget(m_refreshButton);
    bottom->addWidget(m_runButton);
    bottom->addWidget(m_detailsButton);
    bottom->addWidget(m_permButton);
    bottom->addWidget(m_uninstallButton);

    connect(m_refreshButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(refresh()));
    connect(m_runButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotRun()));
    connect(m_uninstallButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotUninstall()));
    connect(m_detailsButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotDetails()));
    connect(m_permButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotPermissions()));
    connect(m_list, TQ_SIGNAL(contextMenu(TDEListView *, TQListViewItem *, const TQPoint &)),
            this, TQ_SLOT(slotContextMenu(TDEListView *, TQListViewItem *, const TQPoint &)));
    connect(m_list, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotSelectionChanged()));
    connect(m_list, TQ_SIGNAL(doubleClicked(TQListViewItem *)), this, TQ_SLOT(slotDoubleClicked(TQListViewItem *)));
    connect(m_showRuntimes, TQ_SIGNAL(toggled(bool)), this, TQ_SLOT(slotShowRuntimesToggled(bool)));

    slotSelectionChanged();
}

void InstalledPage::refresh()
{
    TQString error;
    FpInstalledRefList refs;
    if (!FlatpakBackend::listInstalled(refs, error)) {
        KMessageBox::error(this, i18n("Could not list installed Flatpaks:\n%1").arg(error));
    }
    m_refs = refs;

    // update masks and pins, as shown in the Notes column
    m_masked.clear();
    m_pinned.clear();
    for (int scope = 0; scope < 2; ++scope) {
        const TQStringList masked = FlatpakBackend::configPatterns(scope, "masked");
        const TQStringList pinned = FlatpakBackend::configPatterns(scope, "pinned");
        for (FpInstalledRefList::ConstIterator it = m_refs.begin(); it != m_refs.end(); ++it) {
            if ((*it).scope != scope) {
                continue;
            }
            if (!masked.isEmpty() && FlatpakBackend::patternsMatch(masked, (*it).ref)) {
                m_masked[fpRefKey(scope, (*it).ref)] = true;
            }
            if (!pinned.isEmpty() && FlatpakBackend::patternsMatch(pinned, (*it).ref)) {
                m_pinned[fpRefKey(scope, (*it).ref)] = true;
            }
        }
    }
    populate();
    emit installedChanged();
}

void InstalledPage::populate()
{
    m_list->clear();
    m_itemIndex.clear();

    int apps = 0, runtimes = 0;
    const bool showRuntimes = m_showRuntimes->isChecked();

    int idx = 0;
    for (FpInstalledRefList::ConstIterator it = m_refs.begin(); it != m_refs.end(); ++it, ++idx) {
        const FpInstalledRef &r = *it;
        if (r.isApp) {
            ++apps;
        } else {
            ++runtimes;
            if (!showRuntimes) {
                continue;
            }
        }
        TQListViewItem *item = new TQListViewItem(m_list,
            r.name, r.id, r.version, r.branch, r.origin,
            FlatpakBackend::scopeName(r.scope),
            r.installedSize ? TDEIO::convertSize(r.installedSize) : TQString::null);

        TQPixmap pix;
        if (r.isApp) {
            pix = IconCache::load(FlatpakBackend::installedIconPath(r), 22, "application-x-executable");
        } else {
            pix = SmallIcon("package_settings", 22);
            if (pix.isNull()) {
                pix = SmallIcon("misc", 22);
            }
        }
        item->setPixmap(ColName, pix);
        TQStringList notes;
        if (m_masked.contains(fpRefKey(r.scope, r.ref))) {
            notes.append(i18n("Updates blocked"));
        }
        if (m_pinned.contains(fpRefKey(r.scope, r.ref))) {
            notes.append(i18n("Pinned"));
        }
        item->setText(ColNotes, notes.join(", "));
        m_itemIndex[item] = idx;
    }

    m_summary->setText(i18n("installed apps, installed runtimes", "%1, %2 installed")
                           .arg(i18n("%n application", "%n applications", apps))
                           .arg(i18n("%n runtime", "%n runtimes", runtimes)));
    m_search->updateSearch();
    slotSelectionChanged();
}

void InstalledPage::slotShowRuntimesToggled(bool)
{
    populate();
}

void InstalledPage::slotSelectionChanged()
{
    int selected = 0;
    bool singleApp = false;
    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected | TQListViewItemIterator::Visible);
    for (; it.current(); ++it) {
        ++selected;
        if (selected == 1 && m_itemIndex.contains(it.current())) {
            singleApp = m_refs[m_itemIndex[it.current()]].isApp;
        }
    }
    m_runButton->setEnabled(selected == 1 && singleApp);
    m_detailsButton->setEnabled(selected == 1 && singleApp);
    m_permButton->setEnabled(selected == 1 && singleApp);
    m_uninstallButton->setEnabled(selected > 0);
}

const FpInstalledRef *InstalledPage::currentRef() const
{
    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected | TQListViewItemIterator::Visible);
    if (!it.current() || !m_itemIndex.contains(it.current())) {
        return 0;
    }
    return &m_refs[m_itemIndex[it.current()]];
}

void InstalledPage::slotDetails()
{
    const FpInstalledRef *r = currentRef();
    if (r && r->isApp) {
        const FpInstalledRef copy = *r;
        if (AppDetailsDialog::showDetails(this, 0, &copy)) {
            emit modified();
        }
    }
}

void InstalledPage::slotPermissions()
{
    const FpInstalledRef *r = currentRef();
    if (r && r->isApp && PermissionsDialog::edit(this, r->id)) {
        emit modified();
    }
}

void InstalledPage::slotToggleMask()
{
    const FpInstalledRef *r = currentRef();
    if (!r || !r->isApp) {
        return;
    }
    const FpInstalledRef copy = *r;
    AppActions::setMasked(this, copy.scope, copy.ref, !m_masked.contains(fpRefKey(copy.scope, copy.ref)));
    emit modified();
}

void InstalledPage::slotTogglePin()
{
    const FpInstalledRef *r = currentRef();
    if (!r || r->isApp) {
        return;
    }
    const FpInstalledRef copy = *r;
    AppActions::setPinned(this, copy.scope, copy.ref, !m_pinned.contains(fpRefKey(copy.scope, copy.ref)));
    emit modified();
}

void InstalledPage::slotContextMenu(TDEListView *, TQListViewItem *item, const TQPoint &pos)
{
    if (!item || !m_itemIndex.contains(item)) {
        return;
    }
    const FpInstalledRefList &refs = m_refs;   // const access: no detach, stable addresses
    const FpInstalledRef &r = refs[m_itemIndex[item]];
    const bool single = currentRef() == &r;
    TDEPopupMenu menu(this);
    if (r.isApp && single) {
        menu.insertItem(SmallIconSet("system-run"), i18n("&Run"), this, TQ_SLOT(slotRun()));
        menu.insertItem(SmallIconSet("help-about"), i18n("&Details..."), this, TQ_SLOT(slotDetails()));
        menu.insertItem(SmallIconSet("configure"), i18n("&Permissions..."), this, TQ_SLOT(slotPermissions()));
        int id = menu.insertItem(i18n("&Block Updates"), this, TQ_SLOT(slotToggleMask()));
        menu.setItemChecked(id, m_masked.contains(fpRefKey(r.scope, r.ref)));
        menu.insertSeparator();
    } else if (!r.isApp && single) {
        int id = menu.insertItem(i18n("&Keep When Unused (Pin)"), this, TQ_SLOT(slotTogglePin()));
        menu.setItemChecked(id, m_pinned.contains(fpRefKey(r.scope, r.ref)));
        menu.insertSeparator();
    }
    menu.insertItem(SmallIconSet("edit-delete"), i18n("&Uninstall"), this, TQ_SLOT(slotUninstall()));
    menu.exec(pos);
}

void InstalledPage::slotDoubleClicked(TQListViewItem *item)
{
    if (item && m_itemIndex.contains(item) && m_refs[m_itemIndex[item]].isApp) {
        slotRun();
    }
}

void InstalledPage::slotRun()
{
    const FpInstalledRef *r = currentRef();
    if (r) {
        AppActions::launch(this, *r);
    }
}

void InstalledPage::slotUninstall()
{
    FpInstalledRefList refs;
    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected | TQListViewItemIterator::Visible);
    for (; it.current(); ++it) {
        if (m_itemIndex.contains(it.current())) {
            refs.append(m_refs[m_itemIndex[it.current()]]);
        }
    }
    if (AppActions::uninstall(this, refs)) {
        emit modified();
    }
}

#include "installedpage.moc"
