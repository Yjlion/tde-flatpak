/*
 * tdeflatpak - minimal streaming AppStream (collection XML) parser.
 *
 * Only extracts what the search page needs: id, name, summary, cached icon
 * and the flatpak bundle ref.  Pure TQt + zlib, safe to use from a worker
 * thread (no GUI objects are created).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_APPSTREAM_H
#define TDEFLATPAK_APPSTREAM_H

#include "flatpaktypes.h"

namespace AppStream
{
    /**
     * Parse @p xmlFile (plain or gzip-compressed) and append every flatpak
     * application found to @p out.
     *
     * @param iconsDir  directory holding the cached icons ("<size>/<name>")
     * @param lang      preferred language for name/summary (e.g. "de"), may be empty
     */
    bool parseFile(const TQString &xmlFile, const TQString &iconsDir,
                   const TQString &remote, int scope, const TQString &lang,
                   FpAppList &out, TQString &error);
}

#endif
