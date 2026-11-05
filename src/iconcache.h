/*
 * tdeflatpak - small pixmap cache for application icons.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_ICONCACHE_H
#define TDEFLATPAK_ICONCACHE_H

#include <tqpixmap.h>
#include <tqstring.h>

namespace IconCache
{
    /**
     * Load the icon at @p path scaled to @p size pixels.  Falls back to the
     * themed icon @p fallback if the file does not exist or cannot be read.
     * Must be called from the GUI thread.
     */
    TQPixmap load(const TQString &path, int size, const char *fallback);
}

#endif
