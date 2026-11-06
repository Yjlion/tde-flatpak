/*
 * tdeflatpak - small pixmap cache for application icons.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "iconcache.h"

#include <tqfile.h>
#include <tqimage.h>
#include <tqtimer.h>

#include <kiconloader.h>
#include <tdeglobal.h>
#include <tdeio/job.h>
#include <kmdcodec.h>
#include <kstandarddirs.h>
#include <kstaticdeleter.h>
#include <kurl.h>

namespace IconCache
{

TQPixmap load(const TQString &path, int size, const char *fallback)
{
    static TQMap<TQString, TQPixmap> cache;

    if (!path.isEmpty()) {
        const TQString key = TQString::number(size) + ":" + path;
        TQMap<TQString, TQPixmap>::ConstIterator it = cache.find(key);
        if (it != cache.end()) {
            return *it;
        }
        TQImage img;
        if (img.load(path)) {
            TQPixmap pix;
            if (img.width() != size || img.height() != size) {
                img = img.smoothScale(size, size, TQImage::ScaleMin);
            }
            pix.convertFromImage(img);
            cache.insert(key, pix);
            return pix;
        }
    }
    return TDEGlobal::iconLoader()->loadIcon(TQString::fromLatin1(fallback), TDEIcon::Small, size);
}

}

// ---------------------------------------------------------------------------

RemoteImageLoader *RemoteImageLoader::s_self = 0;
static KStaticDeleter<RemoteImageLoader> s_loaderDeleter;

RemoteImageLoader *RemoteImageLoader::self()
{
    if (!s_self) {
        s_loaderDeleter.setObject(s_self, new RemoteImageLoader());
    }
    return s_self;
}

RemoteImageLoader::RemoteImageLoader()
    : TQObject(0, "remoteimageloader")
{
}

TQString RemoteImageLoader::cacheFileFor(const TQString &url) const
{
    KMD5 md5(url.utf8());
    TQString ext = KURL(url).fileName().section('.', -1).lower();
    if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "svg" && ext != "webp") {
        ext = "img";
    }
    return TDEGlobal::dirs()->saveLocation("cache", "tdeflatpak/images/")
           + TQString::fromLatin1(md5.hexDigest()) + "." + ext;
}

TQString RemoteImageLoader::cachedPath(const TQString &url) const
{
    if (url.isEmpty()) {
        return TQString::null;
    }
    if (url.startsWith("/")) {
        return TQFile::exists(url) ? url : TQString::null;
    }
    const TQString file = cacheFileFor(url);
    return TQFile::exists(file) ? file : TQString::null;
}

void RemoteImageLoader::request(const TQString &url)
{
    if (url.isEmpty() || m_pending.contains(url)) {
        return;
    }
    if (!cachedPath(url).isNull()) {
        // deliver asynchronously so callers can rely on the signal
        m_pending[url] = true;
        m_ready.append(url);
        TQTimer::singleShot(0, this, TQ_SLOT(slotDeliverReady()));
        return;
    }
    if (url.startsWith("/")) {
        emit failed(url);
        return;
    }
    TDEIO::StoredTransferJob *job = TDEIO::storedGet(KURL(url), false, false);
    job->addMetaData("errorPage", "false");
    connect(job, TQ_SIGNAL(result(TDEIO::Job *)), this, TQ_SLOT(slotResult(TDEIO::Job *)));
    m_jobs[job] = url;
    m_pending[url] = true;
}

void RemoteImageLoader::slotDeliverReady()
{
    while (!m_ready.isEmpty()) {
        const TQString url = m_ready.first();
        m_ready.remove(m_ready.begin());
        m_pending.remove(url);
        emit loaded(url, cachedPath(url));
    }
}

void RemoteImageLoader::slotResult(TDEIO::Job *job)
{
    if (!m_jobs.contains(job)) {
        return;
    }
    const TQString url = m_jobs[job];
    m_jobs.remove(job);
    m_pending.remove(url);
    if (job->error()) {
        emit failed(url);
        return;
    }
    const TQString file = cacheFileFor(url);
    TQFile f(file);
    if (!f.open(IO_WriteOnly)) {
        emit failed(url);
        return;
    }
    f.writeBlock(static_cast<TDEIO::StoredTransferJob *>(job)->data());
    f.close();
    emit loaded(url, file);
}

#include "iconcache.moc"
