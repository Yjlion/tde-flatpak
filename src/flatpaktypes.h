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

#include <tqmap.h>
#include <tqstring.h>
#include <tqstringlist.h>
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
    TQString developer;
    TQString license;
    TQStringList categories;   ///< freedesktop categories, e.g. "Graphics", "2DGraphics"
    TQString appstreamFile;    ///< AppStream XML the entry was read from (for details)
    TQString iconsDir;         ///< icon cache directory belonging to appstreamFile
    int scope;
};
typedef TQValueList<FpApp> FpAppList;

/**
 * Sandbox permissions, either as requested by an application (its
 * metadata) or as an override layer.  In an override layer, list entries
 * prefixed with "!" revoke a permission (same syntax as flatpak's override
 * files).  Bus policies map a bus name to "talk", "own", "see" or "none".
 */
struct FpPermissions
{
    TQStringList shared;       ///< network, ipc
    TQStringList sockets;      ///< x11, wayland, pulseaudio, ...
    TQStringList devices;      ///< dri, input, usb, kvm, shm, all
    TQStringList features;     ///< devel, multiarch, bluetooth, ...
    TQStringList filesystems;  ///< host, home:ro, xdg-download, ~/Games:create, ...
    TQStringList persistent;   ///< directories persisted in ~/.var/app/<id>
    TQMap<TQString, TQString> environment;
    TQMap<TQString, TQString> sessionBus;
    TQMap<TQString, TQString> systemBus;

    bool isEmpty() const
    {
        return shared.isEmpty() && sockets.isEmpty() && devices.isEmpty() && features.isEmpty()
            && filesystems.isEmpty() && persistent.isEmpty() && environment.isEmpty()
            && sessionBus.isEmpty() && systemBus.isEmpty();
    }
};

struct FpScreenshot
{
    TQString thumbnailUrl;   ///< roughly 624px wide image
    TQString imageUrl;       ///< full size image
    TQString caption;
};
typedef TQValueList<FpScreenshot> FpScreenshotList;

struct FpRelease
{
    FpRelease() : timestamp(0) {}
    TQString version;
    unsigned long timestamp;   ///< seconds since the epoch, 0 = unknown
    TQString description;      ///< simple HTML
};
typedef TQValueList<FpRelease> FpReleaseList;

/**
 * Detailed information about an application, merged from the local
 * AppStream data and, for Flathub apps, the Flathub web API.
 */
struct FpAppDetails
{
    FpAppDetails() : downloadSize(0), installedSize(0), installsTotal(-1), hasPermissions(false) {}

    TQString id;
    TQString name;
    TQString summary;
    TQString developer;
    TQString license;
    TQString description;          ///< simple HTML (p, ul, ol, li, em, code)
    TQString iconUrl;
    TQString runtime;
    TQStringList categories;
    TQStringList arches;
    FpScreenshotList screenshots;
    FpReleaseList releases;
    TQMap<TQString, TQString> urls;   ///< homepage, bugtracker, donation, help, ...
    unsigned long long downloadSize;
    unsigned long long installedSize;
    long long installsTotal;         ///< -1 = unknown
    FpPermissions permissions;       ///< permissions the app requests
    bool hasPermissions;
};

/** An application listed in a Flathub collection (popular, trending, ...). */
struct FpStoreHit
{
    TQString appId;
    TQString name;
    TQString summary;
    TQString iconUrl;
};
typedef TQValueList<FpStoreHit> FpStoreHitList;

/** A permission granted through an xdg-desktop-portal (permission store entry). */
struct FpPortalPermission
{
    TQString table;          ///< e.g. "notifications", "devices"
    TQString object;         ///< e.g. "notification", "camera"
    TQStringList values;     ///< e.g. "yes", "no"
};
typedef TQValueList<FpPortalPermission> FpPortalPermissionList;

/** Helper: a key identifying a ref within an installation. */
inline TQString fpRefKey(int scope, const TQString &ref)
{
    return TQString::number(scope) + TQString::fromLatin1(":") + ref;
}

#endif
