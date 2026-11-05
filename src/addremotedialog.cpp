/*
 * tdeflatpak - dialog for adding a remote.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "addremotedialog.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqfile.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>

#include <tdefile.h>
#include <tdeio/netaccess.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kurl.h>
#include <kurlrequester.h>

AddRemoteDialog::AddRemoteDialog(TQWidget *parent, const TQString &location)
    : KDialogBase(parent, "addremotedialog", true, i18n("Add Remote"),
                  KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, true)
{
    TQWidget *page = new TQWidget(this);
    setMainWidget(page);

    TQGridLayout *grid = new TQGridLayout(page, 5, 2, 0, spacingHint());

    TQLabel *intro = new TQLabel(i18n("Enter a <b>.flatpakrepo</b> file or URL (recommended), "
                                      "or the URL of a Flatpak repository."), page);
    intro->setAlignment(TQt::WordBreak);
    grid->addMultiCellWidget(intro, 0, 0, 0, 1);

    TQLabel *l = new TQLabel(i18n("&Location:"), page);
    m_location = new KURLRequester(page);
    m_location->setMode(KFile::File | KFile::ExistingOnly);
    m_location->setFilter("*.flatpakrepo|" + i18n("Flatpak repository files") + "\n*|" + i18n("All Files"));
    m_location->setURL(location);
    l->setBuddy(m_location);
    grid->addWidget(l, 1, 0);
    grid->addWidget(m_location, 1, 1);

    l = new TQLabel(i18n("&Name:"), page);
    m_name = new KLineEdit(page);
    l->setBuddy(m_name);
    grid->addWidget(l, 2, 0);
    grid->addWidget(m_name, 2, 1);

    l = new TQLabel(i18n("&Installation:"), page);
    m_scope = new TQComboBox(page);
    m_scope->insertItem(i18n("System (all users)"), FpScopeSystem);
    m_scope->insertItem(i18n("User (only me)"), FpScopeUser);
    l->setBuddy(m_scope);
    grid->addWidget(l, 3, 0);
    grid->addWidget(m_scope, 3, 1);

    m_gpg = new TQCheckBox(i18n("&Verify GPG signatures (repository URLs only)"), page);
    m_gpg->setChecked(true);
    grid->addMultiCellWidget(m_gpg, 4, 4, 0, 1);

    connect(m_location, TQ_SIGNAL(textChanged(const TQString &)), this, TQ_SLOT(slotLocationChanged(const TQString &)));
    slotLocationChanged(location);
    m_location->setFocus();
    setMinimumWidth(460);
}

TQString AddRemoteDialog::remoteName() const
{
    return m_name->text().stripWhiteSpace();
}

TQString AddRemoteDialog::location() const
{
    return m_location->url().stripWhiteSpace();
}

int AddRemoteDialog::scope() const
{
    return m_scope->currentItem() == 0 ? FpScopeSystem : FpScopeUser;
}

bool AddRemoteDialog::gpgVerify() const
{
    return m_gpg->isChecked();
}

bool AddRemoteDialog::isRepoFile() const
{
    const TQString loc = location();
    if (loc.endsWith(".flatpakrepo")) {
        return true;
    }
    // local files are always treated as .flatpakrepo files
    KURL url = KURL::fromPathOrURL(loc);
    return url.isLocalFile() && TQFile::exists(url.path());
}

void AddRemoteDialog::slotLocationChanged(const TQString &text)
{
    m_gpg->setEnabled(!isRepoFile());
    if (m_name->isModified() && !m_name->text().isEmpty()) {
        return;
    }
    // suggest a name from the file name / host
    KURL url = KURL::fromPathOrURL(text.stripWhiteSpace());
    TQString suggestion = url.fileName();
    if (suggestion.endsWith(".flatpakrepo")) {
        suggestion.truncate(suggestion.length() - 12);
    } else {
        suggestion = url.host().section('.', -2, -2);
    }
    suggestion.replace(TQRegExp("[^A-Za-z0-9_.-]"), "-");
    m_name->setText(suggestion);
    m_name->clearModified();
}

void AddRemoteDialog::slotOk()
{
    const TQString name = remoteName();
    if (location().isEmpty()) {
        KMessageBox::sorry(this, i18n("Please enter a location."));
        return;
    }
    if (name.isEmpty() || name.find(TQRegExp("[^A-Za-z0-9_.-]")) >= 0 || name.startsWith("-")) {
        KMessageBox::sorry(this, i18n("Please enter a valid remote name (letters, digits, '.', '_' and '-')."));
        return;
    }
    if (!isRepoFile() && !gpgVerify()) {
        if (KMessageBox::warningContinueCancel(this,
                i18n("Without GPG verification the integrity of downloaded software cannot be checked. Continue?"),
                i18n("Add Remote")) != KMessageBox::Continue) {
            return;
        }
    }
    KDialogBase::slotOk();
}

bool addRemoteInteractive(TQWidget *parent, int scope, const TQString &name,
                          const TQString &location, bool isRepoFile, bool gpgVerify)
{
    FlatpakJob *job = new FlatpakJob(FlatpakJob::AddRemote, 0);

    if (isRepoFile) {
        KURL url = KURL::fromPathOrURL(location);
        TQString tmp;
        if (!TDEIO::NetAccess::download(url, tmp, parent)) {
            KMessageBox::error(parent, i18n("Could not download %1:\n%2")
                                           .arg(url.prettyURL()).arg(TDEIO::NetAccess::lastErrorString()));
            delete job;
            return false;
        }
        TQFile f(tmp);
        TQByteArray data;
        if (f.open(IO_ReadOnly)) {
            data = f.readAll();
            f.close();
        }
        TDEIO::NetAccess::removeTempFile(tmp);
        if (data.isEmpty()) {
            KMessageBox::error(parent, i18n("The file %1 is empty or could not be read.").arg(url.prettyURL()));
            delete job;
            return false;
        }
        job->setRemoteFromFile(scope, name, data);
    } else {
        job->setRemoteFromUrl(scope, name, location, gpgVerify);
    }

    return OperationDialog::run(parent, job, i18n("Add Remote"),
                                i18n("<b>Adding remote %1</b>").arg(name), true);
}

#include "addremotedialog.moc"
