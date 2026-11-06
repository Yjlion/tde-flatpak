/*
 * tdeflatpak - permissions granted through xdg-desktop-portal.
 *
 * Portals store the user's decisions ("allow notifications", "allow the
 * camera", ...) in the permission store, org.freedesktop.impl.portal.PermissionStore
 * on the session bus.  This is what "flatpak permission-show" displays.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_PORTALPERMISSIONS_H
#define TDEFLATPAK_PORTALPERMISSIONS_H

#include "flatpaktypes.h"

namespace PortalPermissions
{
    /** All permission store entries that mention @p appId. */
    bool list(const TQString &appId, FpPortalPermissionList &out, TQString &error);

    /** Remove the entry @p perm of @p appId from the permission store. */
    bool revoke(const FpPortalPermission &perm, const TQString &appId, TQString &error);

    /** Remove every entry of @p appId (like "flatpak permission-reset"). */
    bool revokeAll(const TQString &appId, TQString &error);

    /** Human readable description of a permission store entry. */
    TQString describe(const FpPortalPermission &perm);
}

#endif
