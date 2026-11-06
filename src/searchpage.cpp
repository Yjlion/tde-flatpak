/*
 * tdeflatpak - "Search" page: browse and install applications from remotes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchpage.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"
#include "iconcache.h"

#include <tqcursor.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <tdeapplication.h>
#include <tdeglobal.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>

enum { ColName = 0, ColSummary, ColId, ColBranch, ColRemote, ColStatus };

static const int MaxResults = 300;

SearchPage::SearchPage(TQWidget *parent, const char *name)
    : TQWidget(parent, name),
      m_job(0),
      m_loaded(false)
{
    TQVBoxLayout *layout = new TQVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());

    TQHBoxLayout *top = new TQHBoxLayout(layout, KDialog::spacingHint());
    TQLabel *label = new TQLabel(i18n("&Search:"), this);
    top->addWidget(label);
    m_query = new KLineEdit(this);
    m_query->setClickMessage(i18n("Application name, ID or keyword"));
    label->setBuddy(m_query);
    top->addWidget(m_query, 1);
    m_reloadButton = new KPushButton(SmallIconSet("view-refresh"), i18n("Refresh &Catalog"), this);
    top->addWidget(m_reloadButton);

    m_list = new TDEListView(this);
    m_list->addColumn(i18n("Name"));
    m_list->addColumn(i18n("Summary"));
    m_list->addColumn(i18n("Application ID"));
    m_list->addColumn(i18n("Branch"));
    m_list->addColumn(i18n("Remote"));
    m_list->addColumn(i18n("Status"));
    m_list->setSelectionMode(TQListView::Extended);
    m_list->setAllColumnsShowFocus(true);
    m_list->setShowSortIndicator(true);
    m_list->setColumnWidthMode(ColSummary, TQListView::Manual);
    m_list->setColumnWidth(ColSummary, 280);
    layout->addWidget(m_list, 1);

    m_details = new TQLabel(this);
    m_details->setTextFormat(TQt::RichText);
    m_details->setAlignment(TQt::AlignTop | TQt::WordBreak);
    m_details->setMinimumHeight(fontMetrics().lineSpacing() * 3);
    layout->addWidget(m_details);

    TQHBoxLayout *bottom = new TQHBoxLayout(layout, KDialog::spacingHint());
    m_status = new TQLabel(this);
    bottom->addWidget(m_status, 1);
    m_installButton = new KPushButton(SmallIconSet("go-down"), i18n("&Install"), this);
    bottom->addWidget(m_installButton);

    m_searchTimer = new TQTimer(this);

    connect(m_query, TQ_SIGNAL(textChanged(const TQString &)), this, TQ_SLOT(slotTextChanged(const TQString &)));
    connect(m_query, TQ_SIGNAL(returnPressed()), this, TQ_SLOT(slotSearch()));
    connect(m_searchTimer, TQ_SIGNAL(timeout()), this, TQ_SLOT(slotSearch()));
    connect(m_reloadButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(reloadCatalog()));
    connect(m_installButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotInstall()));
    connect(m_list, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotSelectionChanged()));
    connect(m_list, TQ_SIGNAL(doubleClicked(TQListViewItem *)), this, TQ_SLOT(slotDoubleClicked(TQListViewItem *)));

    m_status->setText(i18n("The application catalog has not been loaded yet."));
    slotSelectionChanged();
}

SearchPage::~SearchPage()
{
    if (m_job) {
        m_job->cancel();
        m_job->wait();
        delete m_job;
    }
}

void SearchPage::setInstalled(const FpInstalledRefList &refs)
{
    m_installed.clear();
    for (FpInstalledRefList::ConstIterator it = refs.begin(); it != refs.end(); ++it) {
        m_installed[fpRefKey((*it).scope, (*it).ref)] = true;
    }
    if (m_loaded) {
        slotSearch();
    }
}

void SearchPage::ensureLoaded()
{
    if (!m_loaded && !m_job) {
        loadCatalog(false);
    }
}

void SearchPage::reloadCatalog()
{
    if (!m_job) {
        loadCatalog(true);
    }
}

void SearchPage::loadCatalog(bool force)
{
    m_job = new FlatpakJob(FlatpakJob::LoadCatalog, this);
    m_job->setForceRefresh(force);
    m_job->setLanguage(TDEGlobal::locale()->language());
    setBusy(true, i18n("Loading application catalog..."));
    m_job->start();
}

void SearchPage::setBusy(bool busy, const TQString &text)
{
    m_reloadButton->setEnabled(!busy);
    if (busy) {
        TQApplication::setOverrideCursor(TQCursor(TQt::BusyCursor));
    } else {
        TQApplication::restoreOverrideCursor();
    }
    if (!text.isNull()) {
        m_status->setText(text);
    }
    slotSelectionChanged();
}

void SearchPage::customEvent(TQCustomEvent *e)
{
    if ((int)e->type() == FlatpakJobEvent::Progress) {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        if (!ev->text.isEmpty()) {
            m_status->setText(i18n("Downloading catalog of %1...").arg(ev->text));
        }
    } else if ((int)e->type() == FlatpakJobEvent::Finished) {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        if (!m_job) {
            return;
        }
        m_job->wait();
        m_apps = m_job->apps();
        const TQStringList warnings = m_job->warnings();
        delete m_job;
        m_job = 0;
        m_loaded = true;
        setBusy(false);

        if (!ev->success && m_apps.isEmpty()) {
            m_status->setText(i18n("Could not load the catalog: %1").arg(ev->text));
            if (ev->text.contains("No enabled remotes")) {
                m_status->setText(i18n("No remotes are configured. Add one (e.g. Flathub) on the Remotes page."));
            }
        }
        slotSearch();
        if (!warnings.isEmpty() && m_apps.isEmpty()) {
            KMessageBox::informationList(this, i18n("There were problems loading the catalog:"),
                                         warnings, i18n("Catalog"));
        }
    } else {
        TQWidget::customEvent(e);
    }
}

void SearchPage::slotTextChanged(const TQString &)
{
    m_searchTimer->start(300, true);
}

void SearchPage::slotSearch()
{
    m_searchTimer->stop();
    if (!m_loaded) {
        ensureLoaded();
        return;
    }

    m_list->clear();
    m_itemApp.clear();
    m_details->clear();

    const TQString q = m_query->text().stripWhiteSpace();
    if (q.isEmpty()) {
        m_status->setText(i18n("%n application available. Type to search.",
                               "%n applications available. Type to search.", m_apps.count()));
        slotSelectionChanged();
        return;
    }

    const TQStringList words = TQStringList::split(' ', q);
    int matches = 0;
    for (FpAppList::ConstIterator it = m_apps.begin(); it != m_apps.end(); ++it) {
        const FpApp &a = *it;
        bool ok = true;
        for (TQStringList::ConstIterator w = words.begin(); w != words.end() && ok; ++w) {
            ok = a.name.contains(*w, false) || a.id.contains(*w, false) || a.summary.contains(*w, false);
        }
        if (!ok) {
            continue;
        }
        ++matches;
        if (matches > MaxResults) {
            continue;
        }
        const bool installed = m_installed.contains(fpRefKey(a.scope, a.ref));
        TQString remote = a.remote + " (" + FlatpakBackend::scopeName(a.scope) + ")";
        TQListViewItem *item = new TQListViewItem(m_list, a.name, a.summary, a.id,
                                                  a.ref.section('/', 3, 3), remote,
                                                  installed ? i18n("Installed") : TQString::null);
        item->setPixmap(ColName, IconCache::load(a.iconPath, 32, "application-x-executable"));
        m_itemApp[item] = &a;
    }

    if (matches > MaxResults) {
        m_status->setText(i18n("Showing %1 of %2 matches. Refine your search to see more.")
                              .arg(MaxResults).arg(matches));
    } else {
        m_status->setText(i18n("%n match", "%n matches", matches));
    }
    slotSelectionChanged();
}

void SearchPage::slotSelectionChanged()
{
    int installable = 0;
    const FpApp *current = 0;
    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected);
    for (; it.current(); ++it) {
        const FpApp *a = m_itemApp.contains(it.current()) ? m_itemApp[it.current()] : 0;
        if (!a) {
            continue;
        }
        if (!current) {
            current = a;
        }
        if (!m_installed.contains(fpRefKey(a->scope, a->ref))) {
            ++installable;
        }
    }
    m_installButton->setEnabled(installable > 0 && !m_job);

    if (current) {
        m_details->setText(TQString("<b>%1</b> &mdash; %2<br><small>%3 &middot; %4</small>")
                               .arg(TQStyleSheet::escape(current->name))
                               .arg(TQStyleSheet::escape(current->summary))
                               .arg(TQStyleSheet::escape(current->ref))
                               .arg(TQStyleSheet::escape(current->remote)));
    } else {
        m_details->clear();
    }
}

void SearchPage::slotDoubleClicked(TQListViewItem *item)
{
    if (item && m_itemApp.contains(item)) {
        slotInstall();
    }
}

void SearchPage::slotInstall()
{
    TQStringList names;
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);

    TQListViewItemIterator it(m_list, TQListViewItemIterator::Selected);
    for (; it.current(); ++it) {
        const FpApp *a = m_itemApp.contains(it.current()) ? m_itemApp[it.current()] : 0;
        if (!a || m_installed.contains(fpRefKey(a->scope, a->ref))) {
            continue;
        }
        names.append(i18n("%1 from %2 (%3)").arg(a->name).arg(a->remote).arg(FlatpakBackend::scopeName(a->scope)));
        job->addInstall(a->scope, a->remote, a->ref);
    }
    if (names.isEmpty()) {
        delete job;
        return;
    }

    int answer = KMessageBox::questionYesNoList(this,
        i18n("Install the following application?", "Install the following %n applications?", names.count()),
        names, i18n("Install"),
        KGuiItem(i18n("&Install"), "go-down"), KStdGuiItem::cancel());
    if (answer != KMessageBox::Yes) {
        delete job;
        return;
    }

    OperationDialog::run(this, job, i18n("Install"),
                         i18n("<b>Installing %n application</b>", "<b>Installing %n applications</b>", names.count()));
    emit modified();
}

#include "searchpage.moc"
