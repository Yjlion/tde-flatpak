/*
 * tdeflatpak - "Remotes" page.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "remotespage.h"
#include "addremotedialog.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"

#include <tqlabel.h>
#include <tqlayout.h>

#include <kdialog.h>
#include <kiconloader.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>

static const char FlathubRepoUrl[] = "https://dl.flathub.org/repo/flathub.flatpakrepo";

enum { ColName = 0, ColTitle, ColUrl, ColInstallation, ColStatus, ColPrio };

RemotesPage::RemotesPage(TQWidget *parent, const char *name)
    : TQWidget(parent, name)
{
    TQHBoxLayout *layout = new TQHBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());

    m_list = new TDEListView(this);
    m_list->addColumn(i18n("Name"));
    m_list->addColumn(i18n("Title"));
    m_list->addColumn(i18n("URL"));
    m_list->addColumn(i18n("Installation"));
    m_list->addColumn(i18n("Status"));
    m_list->addColumn(i18n("Priority"));
    m_list->setAllColumnsShowFocus(true);
    m_list->setShowSortIndicator(true);
    layout->addWidget(m_list, 1);

    TQVBoxLayout *buttons = new TQVBoxLayout(layout, KDialog::spacingHint());
    m_addButton = new KPushButton(SmallIconSet("list-add"), i18n("&Add..."), this);
    m_flathubButton = new KPushButton(SmallIconSet("network"), i18n("Add &Flathub"), this);
    m_toggleButton = new KPushButton(i18n("&Disable"), this);
    m_removeButton = new KPushButton(SmallIconSet("edit-delete"), i18n("&Remove"), this);
    m_unusedButton = new KPushButton(SmallIconSet("edit-clear"), i18n("Remove &Unused..."), this);
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_flathubButton);
    buttons->addWidget(m_toggleButton);
    buttons->addWidget(m_removeButton);
    buttons->addSpacing(KDialog::spacingHint() * 2);
    buttons->addWidget(m_unusedButton);
    buttons->addStretch(1);

    connect(m_addButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotAdd()));
    connect(m_flathubButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(addFlathub()));
    connect(m_toggleButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotToggleEnabled()));
    connect(m_removeButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotRemove()));
    connect(m_unusedButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(removeUnused()));
    connect(m_list, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotSelectionChanged()));

    slotSelectionChanged();
}

void RemotesPage::refresh()
{
    TQString error;
    FpRemoteList remotes;
    if (!FlatpakBackend::listRemotes(remotes, error)) {
        KMessageBox::error(this, i18n("Could not list remotes:\n%1").arg(error));
    }
    m_remotes = remotes;

    m_list->clear();
    m_itemIndex.clear();
    int idx = 0;
    for (FpRemoteList::ConstIterator it = m_remotes.begin(); it != m_remotes.end(); ++it, ++idx) {
        const FpRemote &r = *it;
        TQString status = r.disabled ? i18n("Disabled") : i18n("Enabled");
        if (!r.gpgVerify) {
            status += ", " + i18n("unsigned");
        }
        TQListViewItem *item = new TQListViewItem(m_list, r.name, r.title, r.url,
                                                  FlatpakBackend::scopeName(r.scope), status,
                                                  TQString::number(r.prio));
        item->setPixmap(ColName, SmallIcon(r.disabled ? "network_disconnected" : "network"));
        m_itemIndex[item] = idx;
    }
    slotSelectionChanged();
}

const FpRemote *RemotesPage::selectedRemote() const
{
    TQListViewItem *item = m_list->selectedItem();
    if (!item || !m_itemIndex.contains(item)) {
        return 0;
    }
    int idx = m_itemIndex[item];
    FpRemoteList::ConstIterator it = m_remotes.at(idx);
    return &(*it);
}

void RemotesPage::slotSelectionChanged()
{
    const FpRemote *r = selectedRemote();
    m_removeButton->setEnabled(r != 0);
    m_toggleButton->setEnabled(r != 0);
    m_toggleButton->setText(r && r->disabled ? i18n("&Enable") : i18n("&Disable"));
}

void RemotesPage::slotAdd()
{
    addRemote();
}

void RemotesPage::addRemote(const TQString &location)
{
    AddRemoteDialog dlg(this, location);
    if (dlg.exec() != TQDialog::Accepted) {
        return;
    }
    if (addRemoteInteractive(this, dlg.scope(), dlg.remoteName(), dlg.location(),
                             dlg.isRepoFile(), dlg.gpgVerify())) {
        refresh();
        emit modified();
    }
}

void RemotesPage::addFlathub()
{
    int answer = KMessageBox::questionYesNoCancel(this,
        i18n("<qt>Add the <b>Flathub</b> repository?<br><br>"
             "A system-wide remote is available to all users; a user remote only to you.</qt>"),
        i18n("Add Flathub"),
        KGuiItem(i18n("&System-wide"), "system"),
        KGuiItem(i18n("For &Me Only"), "user"));
    if (answer == KMessageBox::Cancel) {
        return;
    }
    int scope = (answer == KMessageBox::Yes) ? FpScopeSystem : FpScopeUser;
    if (addRemoteInteractive(this, scope, "flathub", FlathubRepoUrl, true, true)) {
        refresh();
        emit modified();
    }
}

void RemotesPage::slotRemove()
{
    const FpRemote *r = selectedRemote();
    if (!r) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this,
            i18n("<qt>Remove the remote <b>%1</b> (%2)?<br><br>"
                 "Applications installed from it must be uninstalled first.</qt>")
                .arg(r->name).arg(FlatpakBackend::scopeName(r->scope)),
            i18n("Remove Remote"), KGuiItem(i18n("&Remove"), "edit-delete")) != KMessageBox::Continue) {
        return;
    }
    FlatpakJob *job = new FlatpakJob(FlatpakJob::RemoveRemote, 0);
    job->setRemote(r->scope, r->name);
    OperationDialog::run(this, job, i18n("Remove Remote"),
                         i18n("<b>Removing remote %1</b>").arg(r->name), true);
    refresh();
    emit modified();
}

void RemotesPage::slotToggleEnabled()
{
    const FpRemote *r = selectedRemote();
    if (!r) {
        return;
    }
    const bool disable = !r->disabled;
    FlatpakJob *job = new FlatpakJob(FlatpakJob::SetRemoteDisabled, 0);
    job->setRemote(r->scope, r->name, disable);
    OperationDialog::run(this, job, disable ? i18n("Disable Remote") : i18n("Enable Remote"),
                         (disable ? i18n("<b>Disabling remote %1</b>") : i18n("<b>Enabling remote %1</b>")).arg(r->name),
                         true);
    refresh();
    emit modified();
}

void RemotesPage::removeUnused()
{
    if (KMessageBox::warningContinueCancel(this,
            i18n("<qt>Uninstall all runtimes and extensions that are no longer used by any "
                 "installed application, in both the user and the system installation?</qt>"),
            i18n("Remove Unused"), KGuiItem(i18n("&Remove Unused"), "edit-clear")) != KMessageBox::Continue) {
        return;
    }
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);
    job->addUninstallUnused(FpScopeUser);
    job->addUninstallUnused(FpScopeSystem);
    OperationDialog::run(this, job, i18n("Remove Unused"), i18n("<b>Removing unused runtimes</b>"));
    emit modified();
}

#include "remotespage.moc"
