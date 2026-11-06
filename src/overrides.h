/*
 * tdeflatpak - application permissions and per-user permission overrides.
 *
 * Reads the permissions an application requests from its metadata file and
 * reads/writes the per-user override files in
 * $XDG_DATA_HOME/flatpak/overrides/<app-id> (and "global" for all
 * applications).  These are the same files "flatpak override --user" and
 * Flatseal use, so changes are picked up by "flatpak run" immediately.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_OVERRIDES_H
#define TDEFLATPAK_OVERRIDES_H

#include "flatpaktypes.h"

namespace Overrides
{
    /** Path of the user override file of @p appId (empty id = global overrides). */
    TQString overrideFile(const TQString &appId);

    /** Permissions requested by an installed application (its metadata). */
    bool loadAppPermissions(const FpInstalledRef &ref, FpPermissions &out, TQString &error);

    /** Load the user override layer of @p appId (empty = global).  A missing file is not an error. */
    bool loadOverride(const TQString &appId, FpPermissions &out, TQString &error);

    /**
     * Write the user override layer of @p appId.  Keys tdeflatpak does not
     * know about are preserved; an empty layer removes the file.
     */
    bool saveOverride(const TQString &appId, const FpPermissions &layer, TQString &error);

    /** Remove all user overrides of @p appId. */
    bool resetOverride(const TQString &appId, TQString &error);

    /** Apply an override @p layer on top of @p base and return the effective permissions. */
    FpPermissions merge(const FpPermissions &base, const FpPermissions &layer);

    /** "home:ro" -> "home",  "!host" -> "host" */
    TQString filesystemPath(const TQString &entry);
    /** "home:ro" -> "ro", "home" -> "rw" */
    TQString filesystemMode(const TQString &entry);
}

#endif
