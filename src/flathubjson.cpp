/*
 * tdeflatpak - parsers for the JSON documents served by the Flathub web API.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flathubjson.h"

#undef signals
#undef slots
#undef emit
#include <json-glib/json-glib.h>

namespace
{

TQString str(const char *s)
{
    return s ? TQString::fromUtf8(s) : TQString::null;
}

/** Parse @p data; returns the root node (owned by *parser) or NULL. */
JsonNode *parseRoot(const TQByteArray &data, JsonParser **parser, TQString &error)
{
    *parser = json_parser_new();
    GError *err = NULL;
    if (!json_parser_load_from_data(*parser, data.data(), data.size(), &err)) {
        error = str(err ? err->message : "invalid JSON");
        g_clear_error(&err);
        return NULL;
    }
    JsonNode *root = json_parser_get_root(*parser);
    if (!root) {
        error = "empty document";
    }
    return root;
}

JsonObject *rootObject(JsonNode *root, TQString &error)
{
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        error = "unexpected document (not an object)";
        return NULL;
    }
    return json_node_get_object(root);
}

JsonNode *member(JsonObject *obj, const char *name)
{
    return (obj && json_object_has_member(obj, name)) ? json_object_get_member(obj, name) : NULL;
}

TQString memberString(JsonObject *obj, const char *name)
{
    JsonNode *n = member(obj, name);
    if (!n || !JSON_NODE_HOLDS_VALUE(n)) {
        return TQString::null;
    }
    GType t = json_node_get_value_type(n);
    if (t == G_TYPE_STRING) {
        return str(json_node_get_string(n));
    }
    if (t == G_TYPE_INT64) {
        return TQString::number((long long)json_node_get_int(n));
    }
    if (t == G_TYPE_DOUBLE) {
        return TQString::number(json_node_get_double(n));
    }
    if (t == G_TYPE_BOOLEAN) {
        return json_node_get_boolean(n) ? "true" : "false";
    }
    return TQString::null;
}

long long memberInt(JsonObject *obj, const char *name, long long def = 0)
{
    JsonNode *n = member(obj, name);
    if (!n || !JSON_NODE_HOLDS_VALUE(n)) {
        return def;
    }
    GType t = json_node_get_value_type(n);
    if (t == G_TYPE_INT64) {
        return json_node_get_int(n);
    }
    if (t == G_TYPE_DOUBLE) {
        return (long long)json_node_get_double(n);
    }
    if (t == G_TYPE_STRING) {
        bool ok = false;
        long long v = str(json_node_get_string(n)).toLongLong(&ok);
        return ok ? v : def;
    }
    return def;
}

JsonObject *memberObject(JsonObject *obj, const char *name)
{
    JsonNode *n = member(obj, name);
    return (n && JSON_NODE_HOLDS_OBJECT(n)) ? json_node_get_object(n) : NULL;
}

JsonArray *memberArray(JsonObject *obj, const char *name)
{
    JsonNode *n = member(obj, name);
    return (n && JSON_NODE_HOLDS_ARRAY(n)) ? json_node_get_array(n) : NULL;
}

TQStringList stringArray(JsonArray *arr)
{
    TQStringList out;
    if (!arr) {
        return out;
    }
    for (guint i = 0; i < json_array_get_length(arr); ++i) {
        JsonNode *n = json_array_get_element(arr, i);
        if (n && JSON_NODE_HOLDS_VALUE(n) && json_node_get_value_type(n) == G_TYPE_STRING) {
            out.append(str(json_node_get_string(n)));
        }
    }
    return out;
}

/** "session-bus": { "talk": [...], "own": [...] }  ->  name -> policy */
void busPolicies(JsonObject *obj, TQMap<TQString, TQString> &out)
{
    if (!obj) {
        return;
    }
    const char *policies[] = { "see", "talk", "own", 0 };
    for (int i = 0; policies[i]; ++i) {
        const TQStringList names = stringArray(memberArray(obj, policies[i]));
        for (TQStringList::ConstIterator it = names.begin(); it != names.end(); ++it) {
            out[*it] = policies[i];
        }
    }
}

void parseScreenshot(JsonObject *shot, FpAppDetails &out)
{
    FpScreenshot s;
    s.caption = memberString(shot, "caption");

    JsonArray *sizes = memberArray(shot, "sizes");
    if (sizes) {
        // current format: [{width, height, src}, ...]
        int bestLarge = 0;
        int bestThumb = -1;
        for (guint i = 0; i < json_array_get_length(sizes); ++i) {
            JsonNode *n = json_array_get_element(sizes, i);
            if (!n || !JSON_NODE_HOLDS_OBJECT(n)) {
                continue;
            }
            JsonObject *o = json_node_get_object(n);
            const int w = (int)memberInt(o, "width");
            const TQString src = memberString(o, "src");
            if (src.isEmpty()) {
                continue;
            }
            if (w > bestLarge) {
                bestLarge = w;
                s.imageUrl = src;
            }
            const int score = TQABS(w - 624);
            if (bestThumb < 0 || score < bestThumb) {
                bestThumb = score;
                s.thumbnailUrl = src;
            }
        }
    } else {
        // older format: { "624x351": "https://...", ... }
        GList *names = json_object_get_members(shot);
        int bestLarge = 0;
        int bestThumb = -1;
        for (GList *l = names; l; l = l->next) {
            const TQString key = str(static_cast<const char *>(l->data));
            const int w = key.section('x', 0, 0).toInt();
            if (w <= 0) {
                continue;
            }
            const TQString src = memberString(shot, static_cast<const char *>(l->data));
            if (w > bestLarge) {
                bestLarge = w;
                s.imageUrl = src;
            }
            const int score = TQABS(w - 624);
            if (bestThumb < 0 || score < bestThumb) {
                bestThumb = score;
                s.thumbnailUrl = src;
            }
        }
        g_list_free(names);
    }
    if (!s.imageUrl.isEmpty()) {
        out.screenshots.append(s);
    }
}

}

namespace FlathubJson
{

bool parseAppstream(const TQByteArray &data, FpAppDetails &out, TQString &error)
{
    JsonParser *parser = 0;
    JsonObject *obj = rootObject(parseRoot(data, &parser, error), error);
    if (!obj) {
        g_object_unref(parser);
        return false;
    }

    out.id = memberString(obj, "id");
    out.name = memberString(obj, "name");
    out.summary = memberString(obj, "summary");
    out.description = memberString(obj, "description");
    out.developer = memberString(obj, "developer_name");
    out.license = memberString(obj, "project_license");
    out.iconUrl = memberString(obj, "icon");
    out.categories = stringArray(memberArray(obj, "categories"));

    JsonObject *urls = memberObject(obj, "urls");
    if (urls) {
        GList *names = json_object_get_members(urls);
        for (GList *l = names; l; l = l->next) {
            const TQString url = memberString(urls, static_cast<const char *>(l->data));
            if (!url.isEmpty()) {
                out.urls[str(static_cast<const char *>(l->data))] = url;
            }
        }
        g_list_free(names);
    }

    JsonObject *bundle = memberObject(obj, "bundle");
    if (bundle) {
        out.runtime = memberString(bundle, "runtime");
    }

    out.screenshots.clear();
    JsonArray *shots = memberArray(obj, "screenshots");
    for (guint i = 0; shots && i < json_array_get_length(shots); ++i) {
        JsonNode *n = json_array_get_element(shots, i);
        if (n && JSON_NODE_HOLDS_OBJECT(n)) {
            parseScreenshot(json_node_get_object(n), out);
        }
    }

    out.releases.clear();
    JsonArray *releases = memberArray(obj, "releases");
    for (guint i = 0; releases && i < json_array_get_length(releases); ++i) {
        JsonNode *n = json_array_get_element(releases, i);
        if (!n || !JSON_NODE_HOLDS_OBJECT(n)) {
            continue;
        }
        JsonObject *r = json_node_get_object(n);
        FpRelease rel;
        rel.version = memberString(r, "version");
        rel.timestamp = (unsigned long)memberInt(r, "timestamp");
        rel.description = memberString(r, "description").stripWhiteSpace();
        if (!rel.version.isEmpty()) {
            out.releases.append(rel);
        }
    }

    g_object_unref(parser);
    return true;
}

bool parseSummary(const TQByteArray &data, FpAppDetails &out, TQString &error)
{
    JsonParser *parser = 0;
    JsonObject *obj = rootObject(parseRoot(data, &parser, error), error);
    if (!obj) {
        g_object_unref(parser);
        return false;
    }

    out.downloadSize = (unsigned long long)memberInt(obj, "download_size");
    out.installedSize = (unsigned long long)memberInt(obj, "installed_size");
    out.arches = stringArray(memberArray(obj, "arches"));

    JsonObject *meta = memberObject(obj, "metadata");
    if (meta) {
        const TQString runtime = memberString(meta, "runtime");
        if (!runtime.isEmpty()) {
            out.runtime = runtime;
        }
        JsonObject *perms = memberObject(meta, "permissions");
        if (perms) {
            FpPermissions &p = out.permissions;
            p = FpPermissions();
            p.shared = stringArray(memberArray(perms, "shared"));
            p.sockets = stringArray(memberArray(perms, "sockets"));
            p.devices = stringArray(memberArray(perms, "devices"));
            p.features = stringArray(memberArray(perms, "features"));
            p.filesystems = stringArray(memberArray(perms, "filesystems"));
            p.persistent = stringArray(memberArray(perms, "persistent"));
            busPolicies(memberObject(perms, "session-bus"), p.sessionBus);
            busPolicies(memberObject(perms, "system-bus"), p.systemBus);
            out.hasPermissions = true;
        }
    }

    g_object_unref(parser);
    return true;
}

bool parseStats(const TQByteArray &data, long long &installsTotal, TQString &error)
{
    JsonParser *parser = 0;
    JsonNode *root = parseRoot(data, &parser, error);
    installsTotal = -1;
    if (!root || JSON_NODE_HOLDS_NULL(root)) {
        if (root) {
            error = "no statistics available";
        }
        g_object_unref(parser);
        return false;
    }
    JsonObject *obj = rootObject(root, error);
    if (obj) {
        installsTotal = memberInt(obj, "installs_total", -1);
    }
    g_object_unref(parser);
    return obj != NULL && installsTotal >= 0;
}

bool parseCollection(const TQByteArray &data, FpStoreHitList &out, TQString &error)
{
    JsonParser *parser = 0;
    JsonObject *obj = rootObject(parseRoot(data, &parser, error), error);
    JsonArray *hits = obj ? memberArray(obj, "hits") : NULL;
    if (!hits) {
        if (obj) {
            error = "no \"hits\" in response";
        }
        g_object_unref(parser);
        return false;
    }
    for (guint i = 0; i < json_array_get_length(hits); ++i) {
        JsonNode *n = json_array_get_element(hits, i);
        if (!n || !JSON_NODE_HOLDS_OBJECT(n)) {
            continue;
        }
        JsonObject *h = json_node_get_object(n);
        FpStoreHit hit;
        hit.appId = memberString(h, "app_id");
        hit.name = memberString(h, "name");
        hit.summary = memberString(h, "summary");
        hit.iconUrl = memberString(h, "icon");
        if (!hit.appId.isEmpty()) {
            out.append(hit);
        }
    }
    g_object_unref(parser);
    return true;
}

}
