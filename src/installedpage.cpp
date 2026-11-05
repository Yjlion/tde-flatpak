/*
 * tdeflatpak - "Installed" page: installed applications and runtimes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "installedpage.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"
#include "iconcache.h"

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
#include <kpushbutton.h>

enum { ColName = 0, ColId, ColVersion, ColBranch, ColOrigin, ColInstallation, ColSize };

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
    m_uninstallButton = new KPushButton(SmallIconSet("edit-delete"), i18n("&Uninstall"), this);
    bottom->addWidget(m_refreshButton);
    bottom->addWidget(m_runButton);
    bottom->addWidget(m_uninstallButton);

    connect(m_refreshButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(refresh()));
    connect(m_runButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotRun()));
    connect(m_uninstallButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotUninstall()));
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
    m_uninstallButton->setEnabled(selected > 0);
}

void InstalledPage::slotDoubleClicked(TQListViewItem *item)
{
    if (item && m_itemIndex.contains(item) && m_refs[m_itemIndex[item]].isApp) {
        slotRun();
    }
}

void InstalledPage::slotRun()
{
    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected | TQListViewItemIterator::Visible);
    if (!it.current() || !m_itemIndex.contains(it.current())) {
        return;
    }
    const FpInstalledRef &r = m_refs[m_itemIndex[it.current()]];
    TQString error;
    if (!FlatpakBackend::launch(r.scope, r.ref, error)) {
        KMessageBox::error(this, i18n("Could not start %1:\n%2").arg(r.name).arg(error));
    }
}

void InstalledPage::slotUninstall()
{
    TQStringList names;
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);

    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected | TQListViewItemIterator::Visible);
    for (; it.current(); ++it) {
        if (!m_itemIndex.contains(it.current())) {
            continue;
        }
        const FpInstalledRef &r = m_refs[m_itemIndex[it.current()]];
        names.append(TQString("%1 (%2, %3)").arg(r.name).arg(r.branch).arg(FlatpakBackend::scopeName(r.scope)));
        job->addUninstall(r.scope, r.ref);
    }
    if (names.isEmpty()) {
        delete job;
        return;
    }

    int answer = KMessageBox::warningContinueCancelList(this,
        i18n("Do you really want to uninstall the following item?",
             "Do you really want to uninstall the following %n items?", names.count()),
        names, i18n("Uninstall"),
        KGuiItem(i18n("&Uninstall"), "edit-delete"));
    if (answer != KMessageBox::Continue) {
        delete job;
        return;
    }

    OperationDialog::run(this, job, i18n("Uninstall"),
                         i18n("<b>Uninstalling %n item</b>", "<b>Uninstalling %n items</b>", names.count()));
    emit modified();
}

#include "installedpage.moc"
