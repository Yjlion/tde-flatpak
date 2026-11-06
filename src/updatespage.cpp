/*
 * tdeflatpak - "Updates" page.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updatespage.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"
#include "iconcache.h"

#include <tqcursor.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqlistview.h>

#include <tdeapplication.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>

enum { ColName = 0, ColId, ColBranch, ColOrigin, ColInstallation, ColKind };

UpdatesPage::UpdatesPage(TQWidget *parent, const char *name)
    : TQWidget(parent, name),
      m_job(0),
      m_checked(false)
{
    TQVBoxLayout *layout = new TQVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());

    m_list = new TDEListView(this);
    m_list->addColumn(i18n("Name"));
    m_list->addColumn(i18n("Application ID"));
    m_list->addColumn(i18n("Branch"));
    m_list->addColumn(i18n("Origin"));
    m_list->addColumn(i18n("Installation"));
    m_list->addColumn(i18n("Type"));
    m_list->setAllColumnsShowFocus(true);
    m_list->setShowSortIndicator(true);
    layout->addWidget(m_list, 1);

    TQHBoxLayout *bottom = new TQHBoxLayout(layout, KDialog::spacingHint());
    m_status = new TQLabel(this);
    bottom->addWidget(m_status, 1);
    m_checkButton = new KPushButton(SmallIconSet("view-refresh"), i18n("&Check for Updates"), this);
    m_updateSelectedButton = new KPushButton(SmallIconSet("system-software-update"), i18n("Update &Selected"), this);
    m_updateAllButton = new KPushButton(SmallIconSet("system-software-update"), i18n("Update &All"), this);
    bottom->addWidget(m_checkButton);
    bottom->addWidget(m_updateSelectedButton);
    bottom->addWidget(m_updateAllButton);

    connect(m_checkButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(checkForUpdates()));
    connect(m_updateSelectedButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotUpdateSelected()));
    connect(m_updateAllButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotUpdateAll()));
    connect(m_list, TQ_SIGNAL(clicked(TQListViewItem *)), this, TQ_SLOT(slotItemClicked(TQListViewItem *)));
    connect(m_list, TQ_SIGNAL(spacePressed(TQListViewItem *)), this, TQ_SLOT(slotItemClicked(TQListViewItem *)));

    m_status->setText(i18n("Updates have not been checked yet."));
    updateButtons();
}

UpdatesPage::~UpdatesPage()
{
    if (m_job) {
        m_job->cancel();
        m_job->wait();
        delete m_job;
    }
}

void UpdatesPage::ensureChecked()
{
    if (!m_checked && !m_job) {
        checkForUpdates();
    }
}

void UpdatesPage::checkForUpdates()
{
    if (m_job) {
        return;
    }
    m_list->clear();
    m_itemIndex.clear();
    m_updates.clear();
    m_status->setText(i18n("Checking for updates..."));
    TQApplication::setOverrideCursor(TQCursor(TQt::BusyCursor));
    m_job = new FlatpakJob(FlatpakJob::CheckUpdates, this);
    m_job->start();
    updateButtons();
}

void UpdatesPage::customEvent(TQCustomEvent *e)
{
    if ((int)e->type() != FlatpakJobEvent::Finished) {
        TQWidget::customEvent(e);
        return;
    }
    FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
    if (!m_job) {
        return;
    }
    m_job->wait();
    m_updates = m_job->updates();
    delete m_job;
    m_job = 0;
    m_checked = true;
    TQApplication::restoreOverrideCursor();

    int idx = 0;
    for (FpInstalledRefList::ConstIterator it = m_updates.begin(); it != m_updates.end(); ++it, ++idx) {
        const FpInstalledRef &r = *it;
        TQCheckListItem *item = new TQCheckListItem(m_list, r.name, TQCheckListItem::CheckBox);
        item->setText(ColId, r.id);
        item->setText(ColBranch, r.branch);
        item->setText(ColOrigin, r.origin);
        item->setText(ColInstallation, FlatpakBackend::scopeName(r.scope));
        item->setText(ColKind, r.isApp ? i18n("Application") : i18n("Runtime"));
        item->setOn(true);
        if (r.isApp) {
            item->setPixmap(ColName, IconCache::load(FlatpakBackend::installedIconPath(r), 22, "application-x-executable"));
        }
        m_itemIndex[item] = idx;
    }

    if (!ev->success) {
        m_status->setText(i18n("Could not check for updates: %1").arg(ev->text));
    } else if (m_updates.isEmpty()) {
        m_status->setText(i18n("Everything is up to date."));
    } else {
        m_status->setText(i18n("%n update available", "%n updates available", m_updates.count()));
    }
    updateButtons();
    emit updatesAvailable(m_updates.count());
}

void UpdatesPage::slotItemClicked(TQListViewItem *)
{
    updateButtons();
}

void UpdatesPage::updateButtons()
{
    int checked = 0;
    for (TQListViewItem *i = m_list->firstChild(); i; i = i->nextSibling()) {
        if (static_cast<TQCheckListItem *>(i)->isOn()) {
            ++checked;
        }
    }
    m_checkButton->setEnabled(!m_job);
    m_updateSelectedButton->setEnabled(!m_job && checked > 0);
    m_updateAllButton->setEnabled(!m_job && !m_updates.isEmpty());
}

void UpdatesPage::slotUpdateSelected()
{
    runUpdates(true);
}

void UpdatesPage::slotUpdateAll()
{
    runUpdates(false);
}

void UpdatesPage::runUpdates(bool onlyChecked)
{
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);
    int count = 0;
    for (TQListViewItem *i = m_list->firstChild(); i; i = i->nextSibling()) {
        if (onlyChecked && !static_cast<TQCheckListItem *>(i)->isOn()) {
            continue;
        }
        if (!m_itemIndex.contains(i)) {
            continue;
        }
        const FpInstalledRef &r = m_updates[m_itemIndex[i]];
        job->addUpdate(r.scope, r.ref);
        ++count;
    }
    if (count == 0) {
        delete job;
        return;
    }
    OperationDialog::run(this, job, i18n("Update"),
                         i18n("<b>Updating %n item</b>", "<b>Updating %n items</b>", count));
    emit modified();
}

#include "updatespage.moc"
