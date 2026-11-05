/*
 * tdeflatpak - small pixmap cache for application icons.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "iconcache.h"

#include <tqimage.h>
#include <tqmap.h>

#include <kiconloader.h>

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
