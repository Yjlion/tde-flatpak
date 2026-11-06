/*
 * tdeflatpak - parsers for the JSON documents served by the Flathub web
 * API (https://flathub.org/api/v2).  Kept free of network code so they can
 * be tested against saved responses.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATHUBJSON_H
#define TDEFLATPAK_FLATHUBJSON_H

#include "flatpaktypes.h"

#include <tqcstring.h>

namespace FlathubJson
{
    /** /appstream/<id>: name, description, screenshots, releases, urls, ... */
    bool parseAppstream(const TQByteArray &data, FpAppDetails &out, TQString &error);

    /** /summary/<id>: sizes, arches, runtime and requested permissions. */
    bool parseSummary(const TQByteArray &data, FpAppDetails &out, TQString &error);

    /** /stats/<id>: total number of installs.  A "null" document means no data. */
    bool parseStats(const TQByteArray &data, long long &installsTotal, TQString &error);

    /** /collection/... : the "hits" array. */
    bool parseCollection(const TQByteArray &data, FpStoreHitList &out, TQString &error);
}

#endif
