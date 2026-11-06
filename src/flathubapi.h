/*
 * tdeflatpak - asynchronous client for the Flathub web API
 * (https://flathub.org/api/v2).
 *
 * Requests run through TDEIO, so they honour the TDE proxy settings and
 * never block the GUI.  Responses are cached on disk; when the network is
 * unavailable a stale cached copy is used instead.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATHUBAPI_H
#define TDEFLATPAK_FLATHUBAPI_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqcstring.h>

#include "flatpaktypes.h"

namespace TDEIO { class Job; }

class FlathubApi : public TQObject
{
    TQ_OBJECT

public:
    static FlathubApi *self();

    /** Is the use of the web service enabled in the settings? */
    static bool enabled();

    /** Is @p url (a remote URL) the Flathub repository? */
    static bool isFlathubUrl(const TQString &url);

    void fetchAppstream(const TQString &appId);
    void fetchSummary(const TQString &appId);
    void fetchStats(const TQString &appId);
    /**
     * @p name is one of "popular", "trending", "recently-added",
     * "recently-updated" or "category/<main category>".
     */
    void fetchCollection(const TQString &name);

signals:
    void appstreamReady(const TQString &appId, const FpAppDetails &details);
    void summaryReady(const TQString &appId, const FpAppDetails &details);
    void statsReady(const TQString &appId, long long installsTotal);
    void collectionReady(const TQString &name, const FpStoreHitList &hits);
    /** @p kind is "appstream", "summary", "stats" or "collection" */
    void requestFailed(const TQString &kind, const TQString &key, const TQString &error);

private slots:
    void slotResult(TDEIO::Job *job);
    void slotDeliverCached();

private:
    FlathubApi();

    struct Request {
        TQString kind;   // appstream, summary, stats, collection
        TQString key;    // app id or collection name
        TQString path;   // relative to the API base URL
        int ttlHours;
    };

    void request(const Request &r);
    void deliver(const Request &r, const TQByteArray &data);
    void fail(const Request &r, const TQString &error);
    TQString cacheFile(const Request &r) const;

    TQMap<TDEIO::Job *, Request> m_jobs;
    TQMap<TQString, bool> m_inFlight;   // path -> true
    TQValueList<Request> m_cachedQueue;
    TQValueList<TQByteArray> m_cachedData;

    static FlathubApi *s_self;
};

#endif
