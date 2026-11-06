/*
 * tdeflatpak - asynchronous client for the Flathub web API.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flathubapi.h"
#include "flathubjson.h"
#include "settings.h"

#include <tqdatetime.h>
#include <tqfile.h>
#include <tqfileinfo.h>
#include <tqtimer.h>

#include <tdeglobal.h>
#include <tdeio/job.h>
#include <kstandarddirs.h>
#include <kstaticdeleter.h>
#include <kurl.h>

static const char apiBase[] = "https://flathub.org/api/v2/";

FlathubApi *FlathubApi::s_self = 0;
static KStaticDeleter<FlathubApi> s_deleter;

FlathubApi *FlathubApi::self()
{
    if (!s_self) {
        s_deleter.setObject(s_self, new FlathubApi());
    }
    return s_self;
}

FlathubApi::FlathubApi()
    : TQObject(0, "flathubapi")
{
}

bool FlathubApi::enabled()
{
    return TdeFlatpakSettings::useFlathubApi();
}

bool FlathubApi::isFlathubUrl(const TQString &url)
{
    KURL u(url);
    return u.host() == "dl.flathub.org" || u.host() == "flathub.org";
}

void FlathubApi::fetchAppstream(const TQString &appId)
{
    Request r = { "appstream", appId, "appstream/" + appId, 24 };
    request(r);
}

void FlathubApi::fetchSummary(const TQString &appId)
{
    Request r = { "summary", appId, "summary/" + appId, 24 };
    request(r);
}

void FlathubApi::fetchStats(const TQString &appId)
{
    Request r = { "stats", appId, "stats/" + appId, 72 };
    request(r);
}

void FlathubApi::fetchCollection(const TQString &name)
{
    // the API answers "per_page" only together with "page"
    Request r = { "collection", name, "collection/" + name + "?page=1&per_page=60", 6 };
    request(r);
}

TQString FlathubApi::cacheFile(const Request &r) const
{
    TQString name = r.path;
    for (uint i = 0; i < name.length(); ++i) {
        const TQChar c = name[i];
        if (!c.isLetterOrNumber() && c != '.' && c != '-') {
            name[i] = '_';
        }
    }
    return TDEGlobal::dirs()->saveLocation("cache", "tdeflatpak/api/") + name + ".json";
}

void FlathubApi::request(const Request &r)
{
    if (!enabled()) {
        fail(r, "The Flathub web service is disabled");
        return;
    }
    if (m_inFlight.contains(r.path)) {
        return;   // the pending request will deliver the result
    }

    // fresh cached copy?
    const TQString file = cacheFile(r);
    TQFileInfo fi(file);
    if (fi.exists() && fi.lastModified().secsTo(TQDateTime::currentDateTime()) < r.ttlHours * 3600) {
        TQFile f(file);
        if (f.open(IO_ReadOnly)) {
            m_cachedQueue.append(r);
            m_cachedData.append(f.readAll());
            TQTimer::singleShot(0, this, TQ_SLOT(slotDeliverCached()));
            return;
        }
    }

    TDEIO::StoredTransferJob *job = TDEIO::storedGet(KURL(TQString(apiBase) + r.path), false, false);
    job->addMetaData("errorPage", "false");     // report HTTP errors as job errors
    job->addMetaData("accept", "application/json");
    connect(job, TQ_SIGNAL(result(TDEIO::Job *)), this, TQ_SLOT(slotResult(TDEIO::Job *)));
    m_jobs[job] = r;
    m_inFlight[r.path] = true;
}

void FlathubApi::slotDeliverCached()
{
    if (m_cachedQueue.isEmpty()) {
        return;
    }
    Request r = m_cachedQueue.first();
    TQByteArray data = m_cachedData.first();
    m_cachedQueue.remove(m_cachedQueue.begin());
    m_cachedData.remove(m_cachedData.begin());
    deliver(r, data);
}

void FlathubApi::slotResult(TDEIO::Job *job)
{
    if (!m_jobs.contains(job)) {
        return;
    }
    const Request r = m_jobs[job];
    m_jobs.remove(job);
    m_inFlight.remove(r.path);

    TDEIO::StoredTransferJob *sjob = static_cast<TDEIO::StoredTransferJob *>(job);
    if (job->error()) {
        // fall back to a stale cached copy
        TQFile f(cacheFile(r));
        if (f.open(IO_ReadOnly)) {
            deliver(r, f.readAll());
        } else {
            fail(r, job->errorString());
        }
        return;
    }

    const TQByteArray data = sjob->data();
    TQFile f(cacheFile(r));
    if (f.open(IO_WriteOnly)) {
        f.writeBlock(data);
        f.close();
    }
    deliver(r, data);
}

void FlathubApi::fail(const Request &r, const TQString &error)
{
    emit requestFailed(r.kind, r.key, error);
}

void FlathubApi::deliver(const Request &r, const TQByteArray &data)
{
    TQString error;
    if (r.kind == "appstream") {
        FpAppDetails d;
        if (FlathubJson::parseAppstream(data, d, error)) {
            emit appstreamReady(r.key, d);
            return;
        }
    } else if (r.kind == "summary") {
        FpAppDetails d;
        if (FlathubJson::parseSummary(data, d, error)) {
            emit summaryReady(r.key, d);
            return;
        }
    } else if (r.kind == "stats") {
        long long installs = -1;
        if (FlathubJson::parseStats(data, installs, error)) {
            emit statsReady(r.key, installs);
            return;
        }
    } else if (r.kind == "collection") {
        FpStoreHitList hits;
        if (FlathubJson::parseCollection(data, hits, error)) {
            emit collectionReady(r.key, hits);
            return;
        }
    }
    fail(r, error);
}

#include "flathubapi.moc"
