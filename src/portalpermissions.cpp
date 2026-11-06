/*
 * tdeflatpak - permissions granted through xdg-desktop-portal.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "portalpermissions.h"

#include <tdelocale.h>

#include "flatpak_p.h"

namespace
{

const char Service[] = "org.freedesktop.impl.portal.PermissionStore";
const char Path[] = "/org/freedesktop/impl/portal/PermissionStore";
const char Interface[] = "org.freedesktop.impl.portal.PermissionStore";
const int Timeout = 5000;

/** Tables that hold per-application yes/no style decisions. */
const char *const Tables[] = {
    "notifications", "background", "devices", "location", "screenshot",
    "wallpaper", "realtime", "inhibit", "screencast", "remote-desktop", 0
};

GDBusConnection *sessionBus(TQString &error)
{
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!bus) {
        error = fpTakeError(err);
    }
    return bus;
}

bool isNotFound(const GError *err)
{
    if (!err) {
        return false;
    }
    gchar *remote = g_dbus_error_get_remote_error(err);
    bool nf = remote && (g_str_has_suffix(remote, ".NotFound") || g_str_has_suffix(remote, "NotFound"));
    g_free(remote);
    return nf;
}

}

namespace PortalPermissions
{

bool list(const TQString &appId, FpPortalPermissionList &out, TQString &error)
{
    out.clear();
    GDBusConnection *bus = sessionBus(error);
    if (!bus) {
        return false;
    }
    const TQCString app = appId.utf8();

    bool ok = true;
    for (int t = 0; Tables[t] && ok; ++t) {
        GError *err = NULL;
        GVariant *ids = g_dbus_connection_call_sync(bus, Service, Path, Interface, "List",
                                                    g_variant_new("(s)", Tables[t]),
                                                    G_VARIANT_TYPE("(as)"),
                                                    G_DBUS_CALL_FLAGS_NONE, Timeout, NULL, &err);
        if (!ids) {
            if (isNotFound(err)) {
                g_clear_error(&err);
                continue;
            }
            error = fpTakeError(err);
            ok = false;
            break;
        }
        GVariantIter *iter = NULL;
        const gchar *id = NULL;
        g_variant_get(ids, "(as)", &iter);
        while (g_variant_iter_next(iter, "&s", &id)) {
            GVariant *entry = g_dbus_connection_call_sync(bus, Service, Path, Interface, "Lookup",
                                                          g_variant_new("(ss)", Tables[t], id),
                                                          G_VARIANT_TYPE("(a{sas}v)"),
                                                          G_DBUS_CALL_FLAGS_NONE, Timeout, NULL, &err);
            if (!entry) {
                g_clear_error(&err);
                continue;
            }
            GVariant *perms = g_variant_get_child_value(entry, 0);
            GVariant *values = g_variant_lookup_value(perms, app.data(), G_VARIANT_TYPE("as"));
            if (values) {
                FpPortalPermission p;
                p.table = fpStr(Tables[t]);
                p.object = fpStr(id);
                gsize n = 0;
                const gchar **strv = g_variant_get_strv(values, &n);
                for (gsize i = 0; i < n; ++i) {
                    p.values.append(fpStr(strv[i]));
                }
                g_free(strv);
                out.append(p);
                g_variant_unref(values);
            }
            g_variant_unref(perms);
            g_variant_unref(entry);
        }
        g_variant_iter_free(iter);
        g_variant_unref(ids);
    }
    g_object_unref(bus);
    return ok;
}

bool revoke(const FpPortalPermission &perm, const TQString &appId, TQString &error)
{
    GDBusConnection *bus = sessionBus(error);
    if (!bus) {
        return false;
    }
    GError *err = NULL;
    GVariant *res = g_dbus_connection_call_sync(bus, Service, Path, Interface, "DeletePermission",
                                                g_variant_new("(sss)", perm.table.utf8().data(),
                                                              perm.object.utf8().data(),
                                                              appId.utf8().data()),
                                                NULL, G_DBUS_CALL_FLAGS_NONE, Timeout, NULL, &err);
    g_object_unref(bus);
    if (!res) {
        error = fpTakeError(err);
        return false;
    }
    g_variant_unref(res);
    return true;
}

bool revokeAll(const TQString &appId, TQString &error)
{
    FpPortalPermissionList perms;
    if (!list(appId, perms, error)) {
        return false;
    }
    bool ok = true;
    for (FpPortalPermissionList::ConstIterator it = perms.begin(); it != perms.end(); ++it) {
        ok = revoke(*it, appId, error) && ok;
    }
    return ok;
}

TQString describe(const FpPortalPermission &p)
{
    if (p.table == "notifications") {
        return i18n("Show notifications");
    }
    if (p.table == "background") {
        return i18n("Run in the background");
    }
    if (p.table == "devices") {
        if (p.object == "camera") {
            return i18n("Use the camera");
        }
        if (p.object == "microphone") {
            return i18n("Use the microphone");
        }
        if (p.object == "speakers") {
            return i18n("Play sound on speakers");
        }
        return i18n("Use device: %1").arg(p.object);
    }
    if (p.table == "location") {
        return i18n("Access your location");
    }
    if (p.table == "screenshot") {
        return i18n("Take screenshots");
    }
    if (p.table == "wallpaper") {
        return i18n("Change the wallpaper");
    }
    if (p.table == "realtime") {
        return i18n("Use realtime scheduling");
    }
    if (p.table == "inhibit") {
        return i18n("Prevent suspend or logout");
    }
    if (p.table == "screencast") {
        return i18n("Record the screen (remembered selection)");
    }
    if (p.table == "remote-desktop") {
        return i18n("Remote control the desktop (remembered selection)");
    }
    return TQString("%1 / %2").arg(p.table).arg(p.object);
}

}
