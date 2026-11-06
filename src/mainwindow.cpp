/*
 * tdeflatpak - main window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mainwindow.h"
#include "installedpage.h"
#include "searchpage.h"
#include "updatespage.h"
#include "remotespage.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"

#include <tqfile.h>
#include <tqtimer.h>

#include <tdeaction.h>
#include <tdeapplication.h>
#include <tdefiledialog.h>
#include <kiconloader.h>
#include <tdeio/netaccess.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <tdepopupmenu.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <ktabwidget.h>
#include <kurl.h>
#include <tdemenubar.h>

MainWindow::MainWindow()
    : TDEMainWindow(0, "tdeflatpak_mainwindow")
{
    setCaption(i18n("Flatpak Manager"));

    m_tabs = new KTabWidget(this);
    m_installed = new InstalledPage(m_tabs, "installed");
    m_search = new SearchPage(m_tabs, "search");
    m_updates = new UpdatesPage(m_tabs, "updates");
    m_remotes = new RemotesPage(m_tabs, "remotes");

    m_tabs->addTab(m_installed, SmallIconSet("system-software-update"), i18n("Installed"));
    m_tabs->addTab(m_search, SmallIconSet("edit-find"), i18n("Search & Install"));
    m_tabs->addTab(m_updates, SmallIconSet("view-refresh"), i18n("Updates"));
    m_tabs->addTab(m_remotes, SmallIconSet("network"), i18n("Remotes"));
    setCentralWidget(m_tabs);

    connect(m_tabs, TQ_SIGNAL(currentChanged(TQWidget *)), this, TQ_SLOT(slotCurrentChanged(TQWidget *)));
    connect(m_installed, TQ_SIGNAL(installedChanged()), this, TQ_SLOT(slotInstalledChanged()));
    connect(m_installed, TQ_SIGNAL(modified()), this, TQ_SLOT(refreshAll()));
    connect(m_search, TQ_SIGNAL(modified()), this, TQ_SLOT(refreshAll()));
    connect(m_updates, TQ_SIGNAL(modified()), this, TQ_SLOT(refreshAll()));
    connect(m_updates, TQ_SIGNAL(updatesAvailable(int)), this, TQ_SLOT(slotUpdatesAvailable(int)));
    connect(m_remotes, TQ_SIGNAL(modified()), this, TQ_SLOT(refreshAll()));

    setupActions();

    statusBar()->message(i18n("Using libflatpak %1").arg(FlatpakBackend::libraryVersion()));

    m_installed->refresh();
    m_remotes->refresh();

    resize(860, 560);
    setAutoSaveSettings();
}

void MainWindow::setupActions()
{
    TDEPopupMenu *fileMenu = new TDEPopupMenu(this);
    TDEAction *open = new TDEAction(i18n("&Open Flatpak File..."), "document-open", CTRL + Key_O,
                                    this, TQ_SLOT(slotOpenFile()), actionCollection(), "open_file");
    open->plug(fileMenu);
    fileMenu->insertSeparator();
    KStdAction::quit(this, TQ_SLOT(close()), actionCollection())->plug(fileMenu);
    menuBar()->insertItem(i18n("&File"), fileMenu);

    TDEPopupMenu *toolsMenu = new TDEPopupMenu(this);
    TDEAction *a;
    a = new TDEAction(i18n("&Refresh"), "view-refresh", Key_F5,
                      this, TQ_SLOT(refreshAll()), actionCollection(), "refresh");
    a->plug(toolsMenu);
    a = new TDEAction(i18n("&Check for Updates"), "system-software-update", 0,
                      this, TQ_SLOT(slotCheckUpdates()), actionCollection(), "check_updates");
    a->plug(toolsMenu);
    a = new TDEAction(i18n("Refresh Application &Catalog"), "view-refresh", 0,
                      this, TQ_SLOT(slotReloadCatalog()), actionCollection(), "reload_catalog");
    a->plug(toolsMenu);
    toolsMenu->insertSeparator();
    a = new TDEAction(i18n("Add &Flathub Remote..."), "network", 0,
                      this, TQ_SLOT(slotAddFlathub()), actionCollection(), "add_flathub");
    a->plug(toolsMenu);
    a = new TDEAction(i18n("Remove &Unused Runtimes..."), "edit-clear", 0,
                      this, TQ_SLOT(slotRemoveUnused()), actionCollection(), "remove_unused");
    a->plug(toolsMenu);
    menuBar()->insertItem(i18n("&Tools"), toolsMenu);

    menuBar()->insertItem(i18n("&Help"), helpMenu());
}

void MainWindow::refreshAll()
{
    m_installed->refresh();
    m_remotes->refresh();
    if (m_tabs->currentPage() == m_updates) {
        m_updates->checkForUpdates();
    }
}

void MainWindow::slotInstalledChanged()
{
    m_search->setInstalled(m_installed->installedRefs());
}

void MainWindow::slotUpdatesAvailable(int count)
{
    m_tabs->setTabLabel(m_updates, count > 0 ? i18n("Updates (%1)").arg(count) : i18n("Updates"));
}

void MainWindow::slotCurrentChanged(TQWidget *page)
{
    if (page == m_search) {
        m_search->ensureLoaded();
    } else if (page == m_updates) {
        m_updates->ensureChecked();
    }
}

void MainWindow::showSearch()
{
    m_tabs->showPage(m_search);
}

void MainWindow::showUpdates()
{
    m_tabs->showPage(m_updates);
}

void MainWindow::showRemotes()
{
    m_tabs->showPage(m_remotes);
}

void MainWindow::slotReloadCatalog()
{
    showSearch();
    m_search->reloadCatalog();
}

void MainWindow::slotCheckUpdates()
{
    showUpdates();
    m_updates->checkForUpdates();
}

void MainWindow::slotAddFlathub()
{
    m_tabs->showPage(m_remotes);
    m_remotes->addFlathub();
}

void MainWindow::slotRemoveUnused()
{
    m_remotes->removeUnused();
}

void MainWindow::slotOpenFile()
{
    KURL url = KFileDialog::getOpenURL(TQString::null,
        "*.flatpakref *.flatpakrepo|" + i18n("Flatpak Files") +
        "\n*.flatpakref|" + i18n("Flatpak Application References") +
        "\n*.flatpakrepo|" + i18n("Flatpak Repository Files"),
        this, i18n("Open Flatpak File"));
    if (!url.isEmpty()) {
        openFile(url);
    }
}

void MainWindow::openFile(const KURL &url)
{
    const TQString name = url.fileName();
    if (name.endsWith(".flatpakrepo")) {
        m_tabs->showPage(m_remotes);
        m_remotes->addRemote(url.url());
    } else if (name.endsWith(".flatpakref")) {
        installFlatpakref(url);
    } else {
        KMessageBox::sorry(this, i18n("%1 is neither a .flatpakref nor a .flatpakrepo file.").arg(url.prettyURL()));
    }
}

void MainWindow::installFlatpakref(const KURL &url)
{
    TQString tmp;
    if (!TDEIO::NetAccess::download(url, tmp, this)) {
        KMessageBox::error(this, i18n("Could not download %1:\n%2")
                                     .arg(url.prettyURL()).arg(TDEIO::NetAccess::lastErrorString()));
        return;
    }
    TQFile f(tmp);
    TQByteArray data;
    if (f.open(IO_ReadOnly)) {
        data = f.readAll();
        f.close();
    }
    TDEIO::NetAccess::removeTempFile(tmp);
    if (data.isEmpty()) {
        KMessageBox::error(this, i18n("The file %1 is empty or could not be read.").arg(url.prettyURL()));
        return;
    }

    // find a human readable title in the file
    TQString title = url.fileName();
    TQStringList lines = TQStringList::split('\n', TQString::fromUtf8(data.data(), data.size()));
    for (TQStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it) {
        if ((*it).startsWith("Title=")) {
            title = (*it).mid(6).stripWhiteSpace();
            break;
        }
    }

    int answer = KMessageBox::questionYesNoCancel(this,
        i18n("<qt>Install <b>%1</b>?<br><br>"
             "A system-wide installation is available to all users; a user installation only to you.</qt>").arg(title),
        i18n("Install"),
        KGuiItem(i18n("&System-wide"), "system"),
        KGuiItem(i18n("For &Me Only"), "user"));
    if (answer == KMessageBox::Cancel) {
        return;
    }
    int scope = (answer == KMessageBox::Yes) ? FpScopeSystem : FpScopeUser;

    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);
    job->addInstallFlatpakref(scope, data);
    OperationDialog::run(this, job, i18n("Install"), i18n("<b>Installing %1</b>").arg(title));
    refreshAll();
}

#include "mainwindow.moc"
