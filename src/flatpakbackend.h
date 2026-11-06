/*
 * tdeflatpak - synchronous libflatpak queries used from the GUI thread.
 *
 * Long running or privileged operations are performed by FlatpakJob instead.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATPAKBACKEND_H
#define TDEFLATPAK_FLATPAKBACKEND_H

#include "flatpaktypes.h"

#include <tqstringlist.h>

namespace FlatpakBackend
{
    /** List all installed apps and runtimes of the user and system installations. */
    bool listInstalled(FpInstalledRefList &out, TQString &error);

    /** List all remotes configured in the user and system installations. */
    bool listRemotes(FpRemoteList &out, TQString &error);

    /** Launch an installed application. */
    bool launch(int scope, const TQString &ref, TQString &error);

    /** Path to an icon for an installed application, or an empty string. */
    TQString installedIconPath(const FpInstalledRef &ref);

    /** libflatpak version string */
    TQString libraryVersion();

    /** Human readable name of an installation scope. */
    TQString scopeName(int scope);

    /**
     * Patterns stored in the installation configuration key @p key
     * ("masked" or "pinned"), as managed by "flatpak mask" / "flatpak pin".
     */
    TQStringList configPatterns(int scope, const TQString &key);

    /** Does one of the mask/pin @p patterns match the full @p ref? */
    bool patternsMatch(const TQStringList &patterns, const TQString &ref);

    /** Is an instance of @p appId currently running? */
    bool isRunning(const TQString &appId);

    /** Per-user data directory of an application (~/.var/app/<id>). */
    TQString appDataDir(const TQString &appId);
}

#endif
