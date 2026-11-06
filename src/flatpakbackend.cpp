/*
 * tdeflatpak - synchronous libflatpak queries used from the GUI thread.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flatpakbackend.h"

#include <tqdir.h>
#include <tqfile.h>
#include <tqregexp.h>
#include <tdelocale.h>

#include "flatpak_p.h"

FlatpakInstallation *fpOpenInstallation(int scope, GCancellable *cancellable, GError **error)
{
    if (scope == FpScopeUser) {
        return flatpak_installation_new_user(cancellable, error);
    }
    return flatpak_installation_new_system(cancellable, error);
}

TQString fpTakeError(GError *&error)
{
    if (!error) {
        return TQString::null;
    }
    TQString msg = fpStr(error->message);
    g_clear_error(&error);
    return msg;
}

FpInstalledRef fpConvertInstalledRef(FlatpakInstalledRef *iref, int scope)
{
    FlatpakRef *ref = FLATPAK_REF(iref);
    FpInstalledRef r;

    gchar *full = flatpak_ref_format_ref(ref);
    r.ref = fpStr(full);
    g_free(full);

    r.id = fpStr(flatpak_ref_get_name(ref));
    r.arch = fpStr(flatpak_ref_get_arch(ref));
    r.branch = fpStr(flatpak_ref_get_branch(ref));
    r.isApp = (flatpak_ref_get_kind(ref) == FLATPAK_REF_KIND_APP);
    r.origin = fpStr(flatpak_installed_ref_get_origin(iref));
    r.deployDir = fpStr(flatpak_installed_ref_get_deploy_dir(iref));
    r.installedSize = flatpak_installed_ref_get_installed_size(iref);
    r.scope = scope;

    r.name = fpStr(flatpak_installed_ref_get_appdata_name(iref));
    r.summary = fpStr(flatpak_installed_ref_get_appdata_summary(iref));
    r.version = fpStr(flatpak_installed_ref_get_appdata_version(iref));
    if (r.name.isEmpty()) {
        r.name = r.id;
    }
    return r;
}

namespace FlatpakBackend
{

bool listInstalled(FpInstalledRefList &out, TQString &error)
{
    out.clear();
    bool anyOk = false;
    const int scopes[] = { FpScopeUser, FpScopeSystem };

    for (int i = 0; i < 2; ++i) {
        GError *err = NULL;
        FlatpakInstallation *inst = fpOpenInstallation(scopes[i], NULL, &err);
        if (!inst) {
            error = fpTakeError(err);
            continue;
        }
        GPtrArray *refs = flatpak_installation_list_installed_refs(inst, NULL, &err);
        if (!refs) {
            error = fpTakeError(err);
            g_object_unref(inst);
            continue;
        }
        anyOk = true;
        for (guint j = 0; j < refs->len; ++j) {
            out.append(fpConvertInstalledRef(FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, j)), scopes[i]));
        }
        g_ptr_array_unref(refs);
        g_object_unref(inst);
    }
    return anyOk;
}

bool listRemotes(FpRemoteList &out, TQString &error)
{
    out.clear();
    bool anyOk = false;
    const int scopes[] = { FpScopeUser, FpScopeSystem };

    for (int i = 0; i < 2; ++i) {
        GError *err = NULL;
        FlatpakInstallation *inst = fpOpenInstallation(scopes[i], NULL, &err);
        if (!inst) {
            error = fpTakeError(err);
            continue;
        }
        GPtrArray *remotes = flatpak_installation_list_remotes(inst, NULL, &err);
        if (!remotes) {
            error = fpTakeError(err);
            g_object_unref(inst);
            continue;
        }
        anyOk = true;
        for (guint j = 0; j < remotes->len; ++j) {
            FlatpakRemote *remote = FLATPAK_REMOTE(g_ptr_array_index(remotes, j));
            FpRemote r;
            r.name = fpStr(flatpak_remote_get_name(remote));
            gchar *s = flatpak_remote_get_title(remote);
            r.title = fpStr(s);
            g_free(s);
            s = flatpak_remote_get_url(remote);
            r.url = fpStr(s);
            g_free(s);
            r.disabled = flatpak_remote_get_disabled(remote);
            r.noEnumerate = flatpak_remote_get_noenumerate(remote);
            r.gpgVerify = flatpak_remote_get_gpg_verify(remote);
            r.prio = flatpak_remote_get_prio(remote);
            r.scope = scopes[i];
            out.append(r);
        }
        g_ptr_array_unref(remotes);
        g_object_unref(inst);
    }
    return anyOk;
}

bool launch(int scope, const TQString &refStr, TQString &error)
{
    GError *err = NULL;
    FlatpakRef *ref = flatpak_ref_parse(refStr.utf8().data(), &err);
    if (!ref) {
        error = fpTakeError(err);
        return false;
    }
    FlatpakInstallation *inst = fpOpenInstallation(scope, NULL, &err);
    if (!inst) {
        error = fpTakeError(err);
        g_object_unref(ref);
        return false;
    }
    gboolean ok = flatpak_installation_launch(inst,
                                              flatpak_ref_get_name(ref),
                                              flatpak_ref_get_arch(ref),
                                              flatpak_ref_get_branch(ref),
                                              NULL, NULL, &err);
    if (!ok) {
        error = fpTakeError(err);
    }
    g_object_unref(inst);
    g_object_unref(ref);
    return ok;
}

TQString installedIconPath(const FpInstalledRef &ref)
{
    if (ref.deployDir.isEmpty() || !ref.isApp) {
        return TQString::null;
    }
    const char *sizes[] = { "64x64", "48x48", "128x128", "256x256", "32x32", 0 };
    for (int i = 0; sizes[i]; ++i) {
        TQString path = ref.deployDir + "/export/share/icons/hicolor/" + sizes[i]
                        + "/apps/" + ref.id + ".png";
        if (TQFile::exists(path)) {
            return path;
        }
    }
    return TQString::null;
}

TQString libraryVersion()
{
    return TQString("%1.%2.%3").arg(FLATPAK_MAJOR_VERSION).arg(FLATPAK_MINOR_VERSION).arg(FLATPAK_MICRO_VERSION);
}

TQString scopeName(int scope)
{
    return scope == FpScopeUser ? i18n("User") : i18n("System");
}

TQStringList configPatterns(int scope, const TQString &key)
{
    TQStringList out;
    GError *err = NULL;
    FlatpakInstallation *inst = fpOpenInstallation(scope, NULL, &err);
    if (!inst) {
        g_clear_error(&err);
        return out;
    }
    gchar *value = flatpak_installation_get_config(inst, key.utf8().data(), NULL, &err);
    g_clear_error(&err);   // not set
    out = TQStringList::split(';', fpStr(value));
    g_free(value);
    g_object_unref(inst);
    return out;
}

static bool globMatch(const TQString &pattern, const TQString &text)
{
    if (pattern.isEmpty() || pattern == "*") {
        return true;
    }
    TQRegExp re(pattern, true, true);
    return re.exactMatch(text);
}

bool patternsMatch(const TQStringList &patterns, const TQString &ref)
{
    // ref: kind/id/arch/branch
    const TQString kind = ref.section('/', 0, 0);
    const TQString id = ref.section('/', 1, 1);
    const TQString arch = ref.section('/', 2, 2);
    const TQString branch = ref.section('/', 3, 3);

    for (TQStringList::ConstIterator it = patterns.begin(); it != patterns.end(); ++it) {
        TQString p = (*it).stripWhiteSpace();
        if (p.isEmpty()) {
            continue;
        }
        TQString pkind;
        if (p.startsWith("app/") || p.startsWith("runtime/")) {
            pkind = p.section('/', 0, 0);
            p = p.section('/', 1);
        }
        if (!pkind.isEmpty() && pkind != kind) {
            continue;
        }
        if (globMatch(p.section('/', 0, 0), id)
            && globMatch(p.section('/', 1, 1), arch)
            && globMatch(p.section('/', 2, 2), branch)) {
            return true;
        }
    }
    return false;
}

bool isRunning(const TQString &appId)
{
    GPtrArray *instances = flatpak_instance_get_all();
    bool running = false;
    for (guint i = 0; instances && i < instances->len && !running; ++i) {
        FlatpakInstance *inst = FLATPAK_INSTANCE(g_ptr_array_index(instances, i));
        running = flatpak_instance_is_running(inst) && fpStr(flatpak_instance_get_app(inst)) == appId;
    }
    if (instances) {
        g_ptr_array_unref(instances);
    }
    return running;
}

TQString appDataDir(const TQString &appId)
{
    return TQDir::homeDirPath() + "/.var/app/" + appId;
}

}
