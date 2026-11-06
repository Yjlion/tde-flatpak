/*
 * tdeflatpak - the application catalog shared by the Browse and Search pages.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "catalog.h"
#include "flatpakjob.h"
#include "flathubapi.h"
#include "settings.h"

#include <tdeglobal.h>
#include <tdelocale.h>
#include <kstaticdeleter.h>

Catalog *Catalog::s_self = 0;
static KStaticDeleter<Catalog> s_deleter;

Catalog *Catalog::self()
{
    if (!s_self) {
        s_deleter.setObject(s_self, new Catalog());
    }
    return s_self;
}

Catalog::Catalog()
    : TQObject(0, "catalog"),
      m_job(0),
      m_loaded(false)
{
}

Catalog::~Catalog()
{
    if (m_job) {
        m_job->cancel();
        m_job->wait();
        delete m_job;
    }
}

void Catalog::ensureLoaded()
{
    if (!m_loaded && !m_job) {
        load(false);
    }
}

void Catalog::reload()
{
    if (!m_job) {
        load(true);
    }
}

void Catalog::load(bool force)
{
    m_job = new FlatpakJob(FlatpakJob::LoadCatalog, this);
    m_job->setForceRefresh(force);
    m_job->setLanguage(TDEGlobal::locale()->language());
    m_job->setCatalogMaxAge(TdeFlatpakSettings::catalogMaxAge());
    emit loadingStarted();
    m_job->start();
}

void Catalog::customEvent(TQCustomEvent *e)
{
    if ((int)e->type() == FlatpakJobEvent::Progress) {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        if (!ev->text.isEmpty()) {
            emit progress(ev->text);
        }
        return;
    }
    if ((int)e->type() != FlatpakJobEvent::Finished || !m_job) {
        return;
    }
    FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
    m_job->wait();
    m_apps = m_job->apps();
    m_warnings = m_job->warnings();
    delete m_job;
    m_job = 0;
    m_loaded = true;
    m_lastError = ev->success ? TQString::null : ev->text;

    // index by id; the job lists the user installation first
    m_byId.clear();
    const FpAppList &apps = m_apps;   // const access: never detach the list
    for (FpAppList::ConstIterator it = apps.begin(); it != apps.end(); ++it) {
        if (!m_byId.contains((*it).id)) {
            m_byId[(*it).id] = &(*it);
        }
    }
    emit loaded(ev->success || !m_apps.isEmpty());
}

const FpApp *Catalog::findById(const TQString &appId) const
{
    TQMap<TQString, const FpApp *>::ConstIterator it = m_byId.find(appId);
    return it != m_byId.end() ? *it : 0;
}

bool Catalog::isInstalled(const FpApp &app) const
{
    return m_installed.contains(fpRefKey(app.scope, app.ref));
}

bool Catalog::isInstalledId(const TQString &appId) const
{
    return m_installedIds.contains(appId);
}

void Catalog::setInstalled(const FpInstalledRefList &refs)
{
    m_installedRefs = refs;
    m_installed.clear();
    m_installedIds.clear();
    for (FpInstalledRefList::ConstIterator it = refs.begin(); it != refs.end(); ++it) {
        m_installed[fpRefKey((*it).scope, (*it).ref)] = true;
        if ((*it).isApp) {
            m_installedIds[(*it).id] = true;
        }
    }
    emit installedChanged();
}

void Catalog::setRemotes(const FpRemoteList &remotes)
{
    m_flathubRemotes.clear();
    for (FpRemoteList::ConstIterator it = remotes.begin(); it != remotes.end(); ++it) {
        if (!(*it).disabled && FlathubApi::isFlathubUrl((*it).url)) {
            m_flathubRemotes[TQString::number((*it).scope) + ":" + (*it).name] = true;
        }
    }
}

bool Catalog::isFlathubRemote(int scope, const TQString &remote) const
{
    return m_flathubRemotes.contains(TQString::number(scope) + ":" + remote);
}

bool Catalog::hasFlathub() const
{
    return !m_flathubRemotes.isEmpty();
}

#include "catalog.moc"
