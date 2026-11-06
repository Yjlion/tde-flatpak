/*
 * tdeflatpak - small pixmap cache for application icons, and an
 * asynchronous loader for remote images (Flathub icons and screenshots).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_ICONCACHE_H
#define TDEFLATPAK_ICONCACHE_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqpixmap.h>
#include <tqstring.h>
#include <tqstringlist.h>

namespace TDEIO { class Job; }

namespace IconCache
{
    /**
     * Load the icon at @p path scaled to @p size pixels.  Falls back to the
     * themed icon @p fallback if the file does not exist or cannot be read.
     * Must be called from the GUI thread.
     */
    TQPixmap load(const TQString &path, int size, const char *fallback);
}

/**
 * Downloads images over http(s) into a disk cache.  Emits loaded() with
 * the local file once the image is available (immediately on the next
 * event loop iteration when it is already cached).
 */
class RemoteImageLoader : public TQObject
{
    TQ_OBJECT

public:
    static RemoteImageLoader *self();

    /** Local cache path of @p url if it has been downloaded, else null. */
    TQString cachedPath(const TQString &url) const;

    /** Start downloading @p url (no-op if it is cached or already pending). */
    void request(const TQString &url);

signals:
    void loaded(const TQString &url, const TQString &localPath);
    void failed(const TQString &url);

private slots:
    void slotResult(TDEIO::Job *job);
    void slotDeliverReady();

private:
    RemoteImageLoader();
    TQString cacheFileFor(const TQString &url) const;

    TQMap<TDEIO::Job *, TQString> m_jobs;
    TQMap<TQString, bool> m_pending;
    TQStringList m_ready;
    static RemoteImageLoader *s_self;
};

#endif
