/*
 * tdeflatpak - human readable descriptions of sandbox permissions.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_PERMISSIONTEXT_H
#define TDEFLATPAK_PERMISSIONTEXT_H

#include "flatpaktypes.h"

namespace PermissionText
{
    struct Line {
        TQString text;
        bool risky;      ///< weakens the sandbox considerably
    };
    typedef TQValueList<Line> LineList;

    /** Friendly name of a filesystem token ("xdg-download" -> "Downloads folder"). */
    TQString filesystem(const TQString &path);

    /** Access mode text for "rw", "ro" and "create". */
    TQString access(const TQString &mode);

    /** One line per noteworthy permission, risky ones first. */
    LineList summarize(const FpPermissions &p);

    /** Render summarize() as a rich text list. */
    TQString html(const FpPermissions &p);
}

#endif
