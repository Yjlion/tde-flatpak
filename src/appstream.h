/*
 * tdeflatpak - minimal streaming AppStream (collection XML) parser.
 *
 * parseFile() extracts what the catalog needs (id, name, summary, cached
 * icon, bundle ref, categories, developer, license); parseDetails() reads
 * everything about a single component.  Pure TQt + zlib, safe to use from a
 * worker thread (no GUI objects are created).
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

    /**
     * Find the application @p id in @p xmlFile and return its description,
     * screenshots, releases, URLs, categories, developer and license.
     * Slower than a lookup in the catalog (the whole file is scanned), so
     * call it on demand only.
     */
    bool parseDetails(const TQString &xmlFile, const TQString &iconsDir, const TQString &id,
                      const TQString &lang, FpAppDetails &out, TQString &error);
}

#endif
