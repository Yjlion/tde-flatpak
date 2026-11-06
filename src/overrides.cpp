/*
 * tdeflatpak - application permissions and per-user permission overrides.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "overrides.h"

#include <tqfile.h>

#include "flatpak_p.h"

#include <glib/gstdio.h>

namespace
{

const char ContextGroup[] = "Context";
const char SessionBusGroup[] = "Session Bus Policy";
const char SystemBusGroup[] = "System Bus Policy";
const char EnvironmentGroup[] = "Environment";

TQStringList readList(GKeyFile *kf, const char *key)
{
    TQStringList out;
    gsize len = 0;
    gchar **values = g_key_file_get_string_list(kf, ContextGroup, key, &len, NULL);
    for (gsize i = 0; values && i < len; ++i) {
        const TQString v = fpStr(values[i]).stripWhiteSpace();
        if (!v.isEmpty()) {
            out.append(v);
        }
    }
    g_strfreev(values);
    return out;
}

void readGroup(GKeyFile *kf, const char *group, TQMap<TQString, TQString> &out)
{
    gsize len = 0;
    gchar **keys = g_key_file_get_keys(kf, group, &len, NULL);
    for (gsize i = 0; keys && i < len; ++i) {
        gchar *v = g_key_file_get_value(kf, group, keys[i], NULL);
        out[fpStr(keys[i])] = fpStr(v);
        g_free(v);
    }
    g_strfreev(keys);
}

void readPermissions(GKeyFile *kf, FpPermissions &p)
{
    p.shared = readList(kf, "shared");
    p.sockets = readList(kf, "sockets");
    p.devices = readList(kf, "devices");
    p.features = readList(kf, "features");
    p.filesystems = readList(kf, "filesystems");
    p.persistent = readList(kf, "persistent");
    readGroup(kf, SessionBusGroup, p.sessionBus);
    readGroup(kf, SystemBusGroup, p.systemBus);
    readGroup(kf, EnvironmentGroup, p.environment);
}

void writeList(GKeyFile *kf, const char *key, const TQStringList &values)
{
    if (values.isEmpty()) {
        g_key_file_remove_key(kf, ContextGroup, key, NULL);
        return;
    }
    TQValueList<TQCString> utf8;
    for (TQStringList::ConstIterator it = values.begin(); it != values.end(); ++it) {
        utf8.append((*it).utf8());
    }
    const gchar **list = g_new0(const gchar *, utf8.count() + 1);
    int i = 0;
    for (TQValueList<TQCString>::ConstIterator it = utf8.begin(); it != utf8.end(); ++it) {
        list[i++] = (*it).data();
    }
    g_key_file_set_string_list(kf, ContextGroup, key, list, utf8.count());
    g_free(list);
}

void writeGroup(GKeyFile *kf, const char *group, const TQMap<TQString, TQString> &values)
{
    g_key_file_remove_group(kf, group, NULL);
    for (TQMap<TQString, TQString>::ConstIterator it = values.begin(); it != values.end(); ++it) {
        g_key_file_set_value(kf, group, it.key().utf8().data(), it.data().utf8().data());
    }
}

bool keyFileIsEmpty(GKeyFile *kf)
{
    gsize groups = 0;
    gchar **names = g_key_file_get_groups(kf, &groups);
    bool empty = true;
    for (gsize i = 0; names && i < groups && empty; ++i) {
        gsize keys = 0;
        gchar **k = g_key_file_get_keys(kf, names[i], &keys, NULL);
        empty = (keys == 0);
        g_strfreev(k);
    }
    g_strfreev(names);
    return empty;
}

/** Name of a list entry, used to find the entry an override replaces. */
TQString entryName(const TQString &key, const TQString &entry)
{
    TQString e = entry.startsWith("!") ? entry.mid(1) : entry;
    return key == "filesystems" ? Overrides::filesystemPath(e) : e;
}

void mergeList(const char *key, TQStringList &base, const TQStringList &layer)
{
    for (TQStringList::ConstIterator it = layer.begin(); it != layer.end(); ++it) {
        const TQString name = entryName(key, *it);
        // drop whatever the base says about this entry ...
        for (TQStringList::Iterator b = base.begin(); b != base.end();) {
            if (entryName(key, *b) == name) {
                b = base.remove(b);
            } else {
                ++b;
            }
        }
        // ... and add the override unless it revokes the permission
        if (!(*it).startsWith("!")) {
            base.append(*it);
        }
    }
}

}

namespace Overrides
{

TQString overrideFile(const TQString &appId)
{
    return fpStr(g_get_user_data_dir()) + "/flatpak/overrides/"
           + (appId.isEmpty() ? TQString("global") : appId);
}

TQString filesystemPath(const TQString &entry)
{
    TQString e = entry.startsWith("!") ? entry.mid(1) : entry;
    const int colon = e.findRev(':');
    if (colon > 0) {
        const TQString mode = e.mid(colon + 1);
        if (mode == "ro" || mode == "rw" || mode == "create" || mode == "reset") {
            e.truncate(colon);
        }
    }
    return e;
}

TQString filesystemMode(const TQString &entry)
{
    const int colon = entry.findRev(':');
    if (colon > 0) {
        const TQString mode = entry.mid(colon + 1);
        if (mode == "ro" || mode == "rw" || mode == "create") {
            return mode;
        }
    }
    return "rw";
}

bool loadAppPermissions(const FpInstalledRef &ref, FpPermissions &out, TQString &error)
{
    out = FpPermissions();
    if (ref.deployDir.isEmpty()) {
        error = "The application is not deployed";
        return false;
    }
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    const TQCString path = TQFile::encodeName(ref.deployDir + "/metadata");
    bool ok = g_key_file_load_from_file(kf, path.data(), G_KEY_FILE_NONE, &err);
    if (ok) {
        readPermissions(kf, out);
    } else {
        error = fpTakeError(err);
    }
    g_key_file_free(kf);
    return ok;
}

bool loadOverride(const TQString &appId, FpPermissions &out, TQString &error)
{
    out = FpPermissions();
    const TQString file = overrideFile(appId);
    if (!TQFile::exists(file)) {
        return true;
    }
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    bool ok = g_key_file_load_from_file(kf, TQFile::encodeName(file).data(), G_KEY_FILE_KEEP_COMMENTS, &err);
    if (ok) {
        readPermissions(kf, out);
    } else {
        error = fpTakeError(err);
    }
    g_key_file_free(kf);
    return ok;
}

bool saveOverride(const TQString &appId, const FpPermissions &layer, TQString &error)
{
    const TQString file = overrideFile(appId);
    const TQCString path = TQFile::encodeName(file);

    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    if (TQFile::exists(file)
        && !g_key_file_load_from_file(kf, path.data(), G_KEY_FILE_KEEP_COMMENTS, &err)) {
        error = fpTakeError(err);
        g_key_file_free(kf);
        return false;
    }

    writeList(kf, "shared", layer.shared);
    writeList(kf, "sockets", layer.sockets);
    writeList(kf, "devices", layer.devices);
    writeList(kf, "features", layer.features);
    writeList(kf, "filesystems", layer.filesystems);
    writeList(kf, "persistent", layer.persistent);
    writeGroup(kf, SessionBusGroup, layer.sessionBus);
    writeGroup(kf, SystemBusGroup, layer.systemBus);
    writeGroup(kf, EnvironmentGroup, layer.environment);

    bool ok = true;
    if (keyFileIsEmpty(kf)) {
        if (TQFile::exists(file) && g_unlink(path.data()) != 0) {
            error = TQString("Cannot remove %1").arg(file);
            ok = false;
        }
    } else {
        gchar *dir = g_path_get_dirname(path.data());
        g_mkdir_with_parents(dir, 0755);
        g_free(dir);
        if (!g_key_file_save_to_file(kf, path.data(), &err)) {
            error = fpTakeError(err);
            ok = false;
        }
    }
    g_key_file_free(kf);
    return ok;
}

bool resetOverride(const TQString &appId, TQString &error)
{
    const TQString file = overrideFile(appId);
    if (TQFile::exists(file) && !TQFile::remove(file)) {
        error = TQString("Cannot remove %1").arg(file);
        return false;
    }
    return true;
}

FpPermissions merge(const FpPermissions &base, const FpPermissions &layer)
{
    FpPermissions p = base;
    mergeList("shared", p.shared, layer.shared);
    mergeList("sockets", p.sockets, layer.sockets);
    mergeList("devices", p.devices, layer.devices);
    mergeList("features", p.features, layer.features);
    mergeList("filesystems", p.filesystems, layer.filesystems);
    mergeList("persistent", p.persistent, layer.persistent);

    TQMap<TQString, TQString>::ConstIterator it;
    for (it = layer.environment.begin(); it != layer.environment.end(); ++it) {
        p.environment[it.key()] = it.data();
    }
    for (it = layer.sessionBus.begin(); it != layer.sessionBus.end(); ++it) {
        if (it.data() == "none") {
            p.sessionBus.remove(it.key());
        } else {
            p.sessionBus[it.key()] = it.data();
        }
    }
    for (it = layer.systemBus.begin(); it != layer.systemBus.end(); ++it) {
        if (it.data() == "none") {
            p.systemBus.remove(it.key());
        } else {
            p.systemBus[it.key()] = it.data();
        }
    }
    return p;
}

}
