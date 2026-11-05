/*
 * tdeflatpak - Flatpak manager for the Trinity Desktop Environment
 *
 * Plain data types shared between the libflatpak backend and the GUI.
 * This header deliberately does not include any GLib / libflatpak headers.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATPAKTYPES_H
#define TDEFLATPAK_FLATPAKTYPES_H

#include <tqstring.h>
#include <tqvaluelist.h>

/** Which Flatpak installation an object belongs to. */
enum FpScope {
    FpScopeUser   = 0,
    FpScopeSystem = 1
};

/** An installed application or runtime. */
struct FpInstalledRef
{
    FpInstalledRef() : isApp(false), scope(FpScopeSystem), installedSize(0) {}

    TQString ref;        ///< full ref, e.g. app/org.gimp.GIMP/x86_64/stable
    TQString id;         ///< e.g. org.gimp.GIMP
    TQString name;       ///< human readable name (appdata), falls back to id
    TQString summary;
    TQString arch;
    TQString branch;
    TQString version;
    TQString origin;     ///< remote it was installed from
    TQString deployDir;
    bool isApp;
    int scope;
    unsigned long long installedSize;
};
typedef TQValueList<FpInstalledRef> FpInstalledRefList;

/** A configured remote (repository). */
struct FpRemote
{
    FpRemote() : disabled(false), noEnumerate(false), gpgVerify(true), scope(FpScopeSystem), prio(1) {}

    TQString name;
    TQString title;
    TQString url;
    bool disabled;
    bool noEnumerate;
    bool gpgVerify;
    int scope;
    int prio;
};
typedef TQValueList<FpRemote> FpRemoteList;

/** An application available from a remote (from its AppStream data). */
struct FpApp
{
    FpApp() : scope(FpScopeSystem) {}

    TQString id;
    TQString name;
    TQString summary;
    TQString iconPath;   ///< local path to a cached icon, may be empty
    TQString ref;        ///< flatpak bundle ref, e.g. app/org.gimp.GIMP/x86_64/stable
    TQString remote;
    int scope;
};
typedef TQValueList<FpApp> FpAppList;

/** Helper: a key identifying a ref within an installation. */
inline TQString fpRefKey(int scope, const TQString &ref)
{
    return TQString::number(scope) + TQString::fromLatin1(":") + ref;
}

#endif
