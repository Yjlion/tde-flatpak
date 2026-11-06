/*
 * tdeflatpak - application details dialog.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appdetailsdialog.h"
#include "appactions.h"
#include "appstream.h"
#include "catalog.h"
#include "flatpakbackend.h"
#include "flathubapi.h"
#include "iconcache.h"
#include "overrides.h"
#include "permissiontext.h"
#include "permissionswidget.h"

#include <tqapplication.h>
#include <tqdatetime.h>
#include <tqdeepcopy.h>
#include <tqimage.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqstylesheet.h>
#include <tqthread.h>
#include <tqvbox.h>

#include <tdeglobal.h>
#include <kiconloader.h>
#include <tdeio/global.h>
#include <tdelocale.h>
#include <kpushbutton.h>
#include <ktextbrowser.h>

static const int LocalDetailsEvent = TQEvent::User + 4300;

// ---------------------------------------------------------------------------

class LocalDetailsThread : public TQThread
{
public:
    LocalDetailsThread(TQObject *receiver, const TQString &xml, const TQString &iconsDir,
                       const TQString &id, const TQString &lang)
        : m_receiver(receiver),
          m_xml(TQDeepCopy<TQString>(xml)),
          m_iconsDir(TQDeepCopy<TQString>(iconsDir)),
          m_id(TQDeepCopy<TQString>(id)),
          m_lang(TQDeepCopy<TQString>(lang)),
          m_ok(false)
    {
    }

    FpAppDetails result() const { return m_details; }
    bool ok() const { return m_ok; }

protected:
    virtual void run()
    {
        TQString error;
        m_ok = AppStream::parseDetails(m_xml, m_iconsDir, m_id, m_lang, m_details, error);
        TQApplication::postEvent(m_receiver, new TQCustomEvent(LocalDetailsEvent));
    }

private:
    TQObject *m_receiver;
    TQString m_xml, m_iconsDir, m_id, m_lang;
    FpAppDetails m_details;
    bool m_ok;
};

// ---------------------------------------------------------------------------

bool AppDetailsDialog::showDetails(TQWidget *parent, const FpApp *app, const FpInstalledRef *installed)
{
    AppDetailsDialog dlg(parent, app, installed);
    dlg.exec();
    return dlg.modified();
}

AppDetailsDialog::AppDetailsDialog(TQWidget *parent, const FpApp *app, const FpInstalledRef *installed)
    : KDialogBase(parent, "appdetails", true, i18n("Application Details"), Close, Close, true),
      m_hasApp(app != 0),
      m_isInstalled(installed != 0),
      m_isFlathub(false),
      m_modified(false),
      m_installs(-1),
      m_hasInstalledPerms(false),
      m_localDone(false),
      m_pendingRemote(0),
      m_thread(0),
      m_shotIndex(0)
{
    if (app) {
        m_app = *app;
    }
    if (installed) {
        m_installed = *installed;
    }

    // what we know without any lookups
    m_local.id = app ? app->id : installed->id;
    m_local.name = app ? app->name : installed->name;
    m_local.summary = app ? app->summary : installed->summary;
    if (app) {
        m_local.developer = app->developer;
        m_local.license = app->license;
        m_local.categories = app->categories;
        m_local.iconUrl = app->iconPath;
    }
    if (!m_hasApp) {
        // an installed app: use the catalog entry if there is one
        const FpApp *c = Catalog::self()->findById(m_local.id);
        if (c) {
            m_app = *c;
            m_hasApp = true;
            m_local.developer = c->developer;
            m_local.license = c->license;
            m_local.categories = c->categories;
            m_local.iconUrl = c->iconPath;
        }
    }

    // ---- layout
    TQWidget *page = new TQWidget(this);
    setMainWidget(page);
    TQVBoxLayout *layout = new TQVBoxLayout(page, 0, spacingHint());

    TQHBoxLayout *top = new TQHBoxLayout(layout, spacingHint());
    m_icon = new TQLabel(page);
    m_icon->setFixedSize(72, 72);
    m_icon->setAlignment(TQt::AlignCenter);
    top->addWidget(m_icon, 0, TQt::AlignTop);
    m_title = new TQLabel(page);
    m_title->setTextFormat(TQt::RichText);
    m_title->setAlignment(TQt::AlignTop | TQt::WordBreak);
    top->addWidget(m_title, 1);

    TQVBoxLayout *buttons = new TQVBoxLayout(top, spacingHint());
    m_installButton = new KPushButton(SmallIconSet("go-down"), i18n("&Install"), page);
    m_launchButton = new KPushButton(SmallIconSet("system-run"), i18n("&Run"), page);
    m_uninstallButton = new KPushButton(SmallIconSet("edit-delete"), i18n("&Uninstall"), page);
    m_permButton = new KPushButton(SmallIconSet("configure"), i18n("&Permissions..."), page);
    buttons->addWidget(m_installButton);
    buttons->addWidget(m_launchButton);
    buttons->addWidget(m_uninstallButton);
    buttons->addWidget(m_permButton);
    buttons->addStretch(1);

    m_shotBox = new TQWidget(page);
    TQVBoxLayout *sl = new TQVBoxLayout(m_shotBox, 0, spacingHint());
    TQHBoxLayout *shotRow = new TQHBoxLayout(sl, spacingHint());
    m_prev = new KPushButton(SmallIconSet("go-previous"), TQString::null, m_shotBox);
    m_next = new KPushButton(SmallIconSet("go-next"), TQString::null, m_shotBox);
    m_shot = new TQLabel(m_shotBox);
    m_shot->setAlignment(TQt::AlignCenter);
    m_shot->setMinimumSize(480, 270);
    m_shot->setFrameStyle(TQFrame::StyledPanel | TQFrame::Sunken);
    shotRow->addWidget(m_prev, 0, TQt::AlignVCenter);
    shotRow->addWidget(m_shot, 1);
    shotRow->addWidget(m_next, 0, TQt::AlignVCenter);
    m_shotCaption = new TQLabel(m_shotBox);
    m_shotCaption->setAlignment(TQt::AlignHCenter | TQt::WordBreak);
    sl->addWidget(m_shotCaption);
    layout->addWidget(m_shotBox);
    m_shotBox->hide();

    m_body = new KTextBrowser(page);
    m_body->setMinimumHeight(200);
    layout->addWidget(m_body, 1);

    connect(m_prev, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotPrevShot()));
    connect(m_next, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotNextShot()));
    connect(m_installButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotInstall()));
    connect(m_launchButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotLaunch()));
    connect(m_uninstallButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotUninstall()));
    connect(m_permButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotPermissions()));
    connect(RemoteImageLoader::self(), TQ_SIGNAL(loaded(const TQString &, const TQString &)),
            this, TQ_SLOT(slotImageLoaded(const TQString &, const TQString &)));

    updateInstalledState();

    // ---- start the lookups
    if (m_hasApp && !m_app.appstreamFile.isEmpty()) {
        m_thread = new LocalDetailsThread(this, m_app.appstreamFile, m_app.iconsDir, m_app.id,
                                          TDEGlobal::locale()->language());
        m_thread->start();
    } else {
        m_localDone = true;
    }

    const int scope = m_hasApp ? m_app.scope : m_installed.scope;
    const TQString remote = m_hasApp ? m_app.remote : m_installed.origin;
    m_isFlathub = Catalog::self()->isFlathubRemote(scope, remote)
                  || (m_isInstalled && Catalog::self()->isFlathubRemote(m_installed.scope, m_installed.origin));
    if (m_isFlathub && FlathubApi::enabled()) {
        FlathubApi *api = FlathubApi::self();
        connect(api, TQ_SIGNAL(appstreamReady(const TQString &, const FpAppDetails &)),
                this, TQ_SLOT(slotAppstream(const TQString &, const FpAppDetails &)));
        connect(api, TQ_SIGNAL(summaryReady(const TQString &, const FpAppDetails &)),
                this, TQ_SLOT(slotSummary(const TQString &, const FpAppDetails &)));
        connect(api, TQ_SIGNAL(statsReady(const TQString &, long long)),
                this, TQ_SLOT(slotStats(const TQString &, long long)));
        connect(api, TQ_SIGNAL(requestFailed(const TQString &, const TQString &, const TQString &)),
                this, TQ_SLOT(slotRemoteFailed(const TQString &, const TQString &, const TQString &)));
        m_pendingRemote = 3;
        api->fetchAppstream(m_local.id);
        api->fetchSummary(m_local.id);
        api->fetchStats(m_local.id);
    }

    updateHeader();
    updateBody();
    resize(760, 680);
}

AppDetailsDialog::~AppDetailsDialog()
{
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
    }
}

void AppDetailsDialog::updateInstalledState()
{
    // find the installed ref of this application (prefer the catalog entry's installation)
    const FpInstalledRefList &refs = Catalog::self()->installed();
    bool found = false;
    for (int pass = 0; pass < 2 && !found; ++pass) {
        for (FpInstalledRefList::ConstIterator it = refs.begin(); it != refs.end(); ++it) {
            if ((*it).isApp && (*it).id == m_local.id
                && (pass == 1 || !m_hasApp || (*it).scope == m_app.scope)) {
                m_installed = *it;
                found = true;
                break;
            }
        }
    }
    if (!found && m_isInstalled && refs.isEmpty()) {
        found = true;   // catalog has no installed list (e.g. not refreshed yet): trust the caller
    }
    m_isInstalled = found;

    m_hasInstalledPerms = false;
    if (m_isInstalled) {
        TQString error;
        m_hasInstalledPerms = Overrides::loadAppPermissions(m_installed, m_installedPerms, error);
    }
    updateButtons();
}

void AppDetailsDialog::updateButtons()
{
    m_installButton->setShown(m_hasApp && !m_isInstalled);
    m_launchButton->setShown(m_isInstalled);
    m_uninstallButton->setShown(m_isInstalled);
    m_permButton->setShown(m_isInstalled);
}

void AppDetailsDialog::customEvent(TQCustomEvent *e)
{
    if ((int)e->type() != LocalDetailsEvent || !m_thread) {
        KDialogBase::customEvent(e);
        return;
    }
    m_thread->wait();
    if (m_thread->ok()) {
        FpAppDetails d = m_thread->result();
        // keep what the catalog already knew where the details entry is empty
        if (d.developer.isEmpty()) {
            d.developer = m_local.developer;
        }
        if (d.name.isEmpty()) {
            d.name = m_local.name;
        }
        if (d.summary.isEmpty()) {
            d.summary = m_local.summary;
        }
        if (d.iconUrl.isEmpty()) {
            d.iconUrl = m_local.iconUrl;
        }
        m_local = d;
    }
    delete m_thread;
    m_thread = 0;
    m_localDone = true;
    updateHeader();
    updateBody();
}

void AppDetailsDialog::slotAppstream(const TQString &appId, const FpAppDetails &d)
{
    if (appId != m_local.id) {
        return;
    }
    m_remote = d;
    --m_pendingRemote;
    m_shotIndex = 0;
    updateHeader();
    updateBody();
}

void AppDetailsDialog::slotSummary(const TQString &appId, const FpAppDetails &d)
{
    if (appId != m_local.id) {
        return;
    }
    m_summary = d;
    --m_pendingRemote;
    updateBody();
}

void AppDetailsDialog::slotStats(const TQString &appId, long long installs)
{
    if (appId != m_local.id) {
        return;
    }
    m_installs = installs;
    --m_pendingRemote;
    updateBody();
}

void AppDetailsDialog::slotRemoteFailed(const TQString &kind, const TQString &key, const TQString &)
{
    if (key != m_local.id || kind == "collection") {
        return;
    }
    --m_pendingRemote;
    updateBody();
}

void AppDetailsDialog::updateHeader()
{
    const TQString name = !m_local.name.isEmpty() ? m_local.name : m_remote.name;
    const TQString summary = !m_local.summary.isEmpty() ? m_local.summary : m_remote.summary;
    const TQString developer = !m_local.developer.isEmpty() ? m_local.developer : m_remote.developer;

    setCaption(i18n("%1 - Application Details").arg(name));
    TQString html = "<font size=\"+2\"><b>" + TQStyleSheet::escape(name) + "</b></font>";
    if (!developer.isEmpty()) {
        html += "<br>" + i18n("by %1").arg(TQStyleSheet::escape(developer));
    }
    if (!summary.isEmpty()) {
        html += "<br><br>" + TQStyleSheet::escape(summary);
    }
    if (m_isInstalled) {
        html += "<br><small>" + i18n("Installed: version %1, %2 installation")
                                    .arg(m_installed.version.isEmpty() ? m_installed.branch : m_installed.version)
                                    .arg(FlatpakBackend::scopeName(m_installed.scope).lower()) + "</small>";
    }
    m_title->setText(html);

    // icon: installed export, cached catalog icon, then the Flathub URL
    TQString iconPath;
    if (m_isInstalled) {
        iconPath = FlatpakBackend::installedIconPath(m_installed);
    }
    if (iconPath.isEmpty() && !m_local.iconUrl.isEmpty() && m_local.iconUrl.startsWith("/")) {
        iconPath = m_local.iconUrl;
    }
    if (iconPath.isEmpty() && !m_remote.iconUrl.isEmpty()) {
        iconPath = RemoteImageLoader::self()->cachedPath(m_remote.iconUrl);
        if (iconPath.isEmpty()) {
            RemoteImageLoader::self()->request(m_remote.iconUrl);
        }
    }
    m_icon->setPixmap(IconCache::load(iconPath, 64, "application-x-executable"));

    // screenshots: prefer the API (it has more sizes), else local AppStream
    const FpScreenshotList &shots = !m_remote.screenshots.isEmpty() ? m_remote.screenshots : m_local.screenshots;
    m_shotBox->setShown(!shots.isEmpty());
    if (!shots.isEmpty()) {
        showShot();
    }
}

void AppDetailsDialog::showShot()
{
    const FpScreenshotList &shots = !m_remote.screenshots.isEmpty() ? m_remote.screenshots : m_local.screenshots;
    if (shots.isEmpty()) {
        return;
    }
    if (m_shotIndex < 0 || m_shotIndex >= (int)shots.count()) {
        m_shotIndex = 0;
    }
    const FpScreenshot &s = shots[m_shotIndex];
    m_prev->setEnabled(m_shotIndex > 0);
    m_next->setEnabled(m_shotIndex + 1 < (int)shots.count());
    m_shotCaption->setText(TQString("%1 (%2/%3)")
                               .arg(TQStyleSheet::escape(s.caption))
                               .arg(m_shotIndex + 1).arg(shots.count()));

    const TQString url = s.thumbnailUrl.isEmpty() ? s.imageUrl : s.thumbnailUrl;
    const TQString path = RemoteImageLoader::self()->cachedPath(url);
    if (path.isEmpty()) {
        m_shot->setText(i18n("Loading screenshot..."));
        RemoteImageLoader::self()->request(url);
        return;
    }
    TQImage img;
    if (!img.load(path)) {
        m_shot->setText(i18n("The screenshot could not be loaded."));
        return;
    }
    const int w = TQMAX(m_shot->width() - 8, 480);
    const int h = TQMAX(m_shot->height() - 8, 270);
    if (img.width() > w || img.height() > h) {
        img = img.smoothScale(w, h, TQImage::ScaleMin);
    }
    TQPixmap pix;
    pix.convertFromImage(img);
    m_shot->setPixmap(pix);
}

void AppDetailsDialog::slotImageLoaded(const TQString &url, const TQString &)
{
    if (url == m_remote.iconUrl) {
        updateHeader();
        return;
    }
    const FpScreenshotList &shots = !m_remote.screenshots.isEmpty() ? m_remote.screenshots : m_local.screenshots;
    if (m_shotIndex >= 0 && m_shotIndex < (int)shots.count()) {
        const FpScreenshot &s = shots[m_shotIndex];
        if (url == s.thumbnailUrl || url == s.imageUrl) {
            showShot();
        }
    }
}

void AppDetailsDialog::slotPrevShot()
{
    --m_shotIndex;
    showShot();
}

void AppDetailsDialog::slotNextShot()
{
    ++m_shotIndex;
    showShot();
}

static TQString row(const TQString &label, const TQString &value)
{
    if (value.isEmpty()) {
        return TQString::null;
    }
    return "<tr><td valign=\"top\"><b>" + TQStyleSheet::escape(label) + "</b>&nbsp;&nbsp;</td><td>" + value + "</td></tr>";
}

void AppDetailsDialog::updateBody()
{
    TQString html;

    // description: local data is translated, the API is not
    TQString desc = !m_local.description.isEmpty() ? m_local.description : m_remote.description;
    if (desc.isEmpty()) {
        desc = (m_localDone && m_pendingRemote <= 0) ? i18n("<p>No description available.</p>")
                                                     : i18n("<p><i>Loading details...</i></p>");
    }
    html += desc;

    // ---- information table
    html += "<h3>" + i18n("Information") + "</h3><table cellspacing=\"0\" cellpadding=\"1\">";
    const TQString license = !m_local.license.isEmpty() ? m_local.license : m_remote.license;
    html += row(i18n("License:"), TQStyleSheet::escape(license));
    const TQStringList cats = !m_local.categories.isEmpty() ? m_local.categories : m_remote.categories;
    html += row(i18n("Categories:"), TQStyleSheet::escape(cats.join(", ")));
    html += row(i18n("Application ID:"), TQStyleSheet::escape(m_local.id));
    if (m_hasApp) {
        html += row(i18n("Source:"), TQStyleSheet::escape(i18n("%1 (%2), branch %3")
                                                             .arg(m_app.remote)
                                                             .arg(FlatpakBackend::scopeName(m_app.scope))
                                                             .arg(m_app.ref.section('/', 3, 3))));
    }
    if (m_isInstalled) {
        html += row(i18n("Installed size:"), TDEIO::convertSize(m_installed.installedSize));
    } else if (m_summary.installedSize > 0) {
        html += row(i18n("Download size:"), TDEIO::convertSize(m_summary.downloadSize));
        html += row(i18n("Installed size:"), TDEIO::convertSize(m_summary.installedSize));
    }
    const TQString runtime = !m_summary.runtime.isEmpty() ? m_summary.runtime : m_remote.runtime;
    html += row(i18n("Runtime:"), TQStyleSheet::escape(runtime));
    html += row(i18n("Architectures:"), TQStyleSheet::escape(m_summary.arches.join(", ")));
    if (m_installs >= 0) {
        html += row(i18n("Installs (Flathub):"), TDEGlobal::locale()->formatNumber((double)m_installs, 0));
    }
    html += "</table>";

    // ---- permissions
    if (m_hasInstalledPerms) {
        html += "<h3>" + i18n("Permissions") + "</h3>" + PermissionText::html(m_installedPerms);
        html += "<p><small>" + i18n("These are the permissions the installed version requests. "
                                    "Use <i>Permissions...</i> to change them.") + "</small></p>";
    } else if (m_summary.hasPermissions) {
        html += "<h3>" + i18n("Permissions") + "</h3>" + PermissionText::html(m_summary.permissions);
    }

    // ---- releases
    const FpReleaseList &releases = !m_remote.releases.isEmpty() ? m_remote.releases : m_local.releases;
    if (!releases.isEmpty()) {
        html += "<h3>" + i18n("Releases") + "</h3>";
        int n = 0;
        for (FpReleaseList::ConstIterator it = releases.begin(); it != releases.end() && n < 3; ++it, ++n) {
            TQString date;
            if ((*it).timestamp > 0) {
                TQDateTime dt;
                dt.setTime_t((*it).timestamp);
                date = TDEGlobal::locale()->formatDate(dt.date(), true);
            }
            html += "<p><b>" + TQStyleSheet::escape((*it).version) + "</b>";
            if (!date.isEmpty()) {
                html += " &mdash; " + date;
            }
            html += "</p>" + (*it).description;
        }
    }

    // ---- links
    TQMap<TQString, TQString> urls = m_remote.urls;
    for (TQMap<TQString, TQString>::ConstIterator it = m_local.urls.begin(); it != m_local.urls.end(); ++it) {
        urls[it.key()] = it.data();
    }
    if (!urls.isEmpty()) {
        const struct { const char *key; const char *label; } names[] = {
            { "homepage", I18N_NOOP("Homepage") },
            { "help", I18N_NOOP("Help") },
            { "faq", I18N_NOOP("FAQ") },
            { "bugtracker", I18N_NOOP("Report a problem") },
            { "donation", I18N_NOOP("Donate") },
            { "translate", I18N_NOOP("Translate") },
            { "contribute", I18N_NOOP("Contribute") },
            { "vcs-browser", I18N_NOOP("Source code") },
            { "vcs_browser", I18N_NOOP("Source code") },
            { 0, 0 }
        };
        TQStringList links;
        for (int i = 0; names[i].key; ++i) {
            if (urls.contains(names[i].key)) {
                links.append(TQString("<a href=\"%1\">%2</a>")
                                 .arg(TQStyleSheet::escape(urls[names[i].key])).arg(i18n(names[i].label)));
            }
        }
        if (!links.isEmpty()) {
            html += "<h3>" + i18n("Links") + "</h3><p>" + links.join(" &middot; ") + "</p>";
        }
    }

    if (m_isFlathub && !FlathubApi::enabled()) {
        html += "<p><small>" + i18n("More details (screenshots, sizes, download counts) are available when the "
                                    "Flathub web service is enabled in the settings.") + "</small></p>";
    }

    const int y = m_body->contentsY();
    m_body->setText(html);
    m_body->setContentsPos(0, y);
}

void AppDetailsDialog::slotInstall()
{
    if (!m_hasApp) {
        return;
    }
    FpAppPtrList list;
    list.append(&m_app);
    if (AppActions::install(this, list)) {
        m_modified = true;
        FpInstalledRefList refs;
        TQString error;
        FlatpakBackend::listInstalled(refs, error);
        Catalog::self()->setInstalled(refs);
        updateInstalledState();
        updateHeader();
        updateBody();
    }
}

void AppDetailsDialog::slotLaunch()
{
    AppActions::launch(this, m_installed);
}

void AppDetailsDialog::slotUninstall()
{
    FpInstalledRefList list;
    list.append(m_installed);
    if (AppActions::uninstall(this, list)) {
        m_modified = true;
        FpInstalledRefList refs;
        TQString error;
        FlatpakBackend::listInstalled(refs, error);
        Catalog::self()->setInstalled(refs);
        updateInstalledState();
        updateHeader();
        updateBody();
    }
}

void AppDetailsDialog::slotPermissions()
{
    if (PermissionsDialog::edit(this, m_installed.id)) {
        m_modified = true;
    }
}

#include "appdetailsdialog.moc"
