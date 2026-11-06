/*
 * tdeflatpak - background worker running libflatpak operations.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flatpakjob.h"
#include "appstream.h"

#include <tqapplication.h>
#include <tqdatetime.h>
#include <tqdeepcopy.h>
#include <tqfileinfo.h>
#include <tqfile.h>

#include "flatpak_p.h"

#define CANCELLABLE(p) (static_cast<GCancellable *>(p))

// ---------------------------------------------------------------------------
// FlatpakTransaction signal handlers (called in the worker thread)
// ---------------------------------------------------------------------------

static int convertOpType(FlatpakTransactionOperationType t)
{
    switch (t) {
    case FLATPAK_TRANSACTION_OPERATION_INSTALL:
        return FlatpakJobEvent::OpInstall;
    case FLATPAK_TRANSACTION_OPERATION_UPDATE:
        return FlatpakJobEvent::OpUpdate;
    case FLATPAK_TRANSACTION_OPERATION_UNINSTALL:
        return FlatpakJobEvent::OpUninstall;
    case FLATPAK_TRANSACTION_OPERATION_INSTALL_BUNDLE:
        return FlatpakJobEvent::OpInstallBundle;
    default:
        return FlatpakJobEvent::OpOther;
    }
}

static void onProgressChanged(FlatpakTransactionProgress *progress, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    gchar *status = flatpak_transaction_progress_get_status(progress);
    int pct = flatpak_transaction_progress_get_progress(progress);
    job->postProgress(fpStr(status), pct);
    g_free(status);
}

static gboolean onReady(FlatpakTransaction *transaction, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    GList *ops = flatpak_transaction_get_operations(transaction);
    job->setOperationCount(g_list_length(ops));
    g_list_free_full(ops, g_object_unref);
    return TRUE;   // the user already confirmed the operation in the GUI
}

static void onNewOperation(FlatpakTransaction *, FlatpakTransactionOperation *op,
                           FlatpakTransactionProgress *progress, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    job->operationStarted(fpStr(flatpak_transaction_operation_get_ref(op)),
                          convertOpType(flatpak_transaction_operation_get_operation_type(op)));
    flatpak_transaction_progress_set_update_frequency(progress, 150);
    g_signal_connect(progress, "changed", G_CALLBACK(onProgressChanged), data);
}

static void onOperationDone(FlatpakTransaction *, FlatpakTransactionOperation *op,
                            const char *, FlatpakTransactionResult, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    job->postLog(FlatpakJobEvent::OperationDone, TQString::null,
                 fpStr(flatpak_transaction_operation_get_ref(op)),
                 convertOpType(flatpak_transaction_operation_get_operation_type(op)));
}

static gboolean onOperationError(FlatpakTransaction *, FlatpakTransactionOperation *op,
                                 const GError *error, gint details, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    const TQString ref = fpStr(flatpak_transaction_operation_get_ref(op));
    const int opType = convertOpType(flatpak_transaction_operation_get_operation_type(op));

    if (g_error_matches(error, FLATPAK_ERROR, FLATPAK_ERROR_SKIPPED)) {
        job->postLog(FlatpakJobEvent::Info, fpStr(error->message), ref, opType);
        return TRUE;
    }
    if (details & FLATPAK_TRANSACTION_ERROR_DETAILS_NON_FATAL) {
        job->postLog(FlatpakJobEvent::Warning, fpStr(error->message), ref, opType);
        return TRUE;
    }
    job->postLog(FlatpakJobEvent::Error, fpStr(error->message), ref, opType);
    return FALSE;  // abort the transaction
}

static gboolean onAddNewRemote(FlatpakTransaction *, FlatpakTransactionRemoteReason,
                               const char *fromId, const char *suggestedName,
                               const char *url, gpointer data)
{
    // Same policy as GNOME Software: accept remotes needed for runtimes of
    // .flatpakref installs.
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    job->postLog(FlatpakJobEvent::Info,
                 TQString("Adding remote '%1' (%2) required by %3")
                     .arg(fpStr(suggestedName)).arg(fpStr(url)).arg(fpStr(fromId)));
    return TRUE;
}

static int onChooseRemoteForRef(FlatpakTransaction *, const char *, const char *,
                                const char * const *, gpointer)
{
    return 0;   // first (highest priority) remote
}

static void onEndOfLifed(FlatpakTransaction *, const char *ref, const char *reason,
                         const char *rebase, gpointer data)
{
    FlatpakJob *job = static_cast<FlatpakJob *>(data);
    TQString text = TQString("End of life: %1").arg(fpStr(reason));
    if (rebase && *rebase) {
        text += TQString(" (replaced by %1)").arg(fpStr(rebase));
    }
    job->postLog(FlatpakJobEvent::Warning, text, fpStr(ref));
}

// ---------------------------------------------------------------------------
// FlatpakJob
// ---------------------------------------------------------------------------

FlatpakJob::FlatpakJob(Kind kind, TQObject *receiver)
    : TQThread(),
      m_kind(kind),
      m_receiver(receiver),
      m_cancellable(g_cancellable_new()),
      m_context(0),
      m_forceRefresh(false),
      m_remoteScope(FpScopeSystem),
      m_remoteGpgVerify(true),
      m_remoteDisabled(false),
      m_opIndex(0),
      m_opCount(0),
      m_success(false)
{
}

FlatpakJob::~FlatpakJob()
{
    if (running()) {
        cancel();
        wait();
    }
    g_object_unref(CANCELLABLE(m_cancellable));
}

void FlatpakJob::cancel()
{
    g_cancellable_cancel(CANCELLABLE(m_cancellable));
}

bool FlatpakJob::wasCancelled() const
{
    return g_cancellable_is_cancelled(CANCELLABLE(m_cancellable));
}

void FlatpakJob::addInstall(int scope, const TQString &remote, const TQString &ref)
{
    TxItem it;
    it.op = TxItem::Install;
    it.scope = scope;
    it.remote = TQDeepCopy<TQString>(remote);
    it.ref = TQDeepCopy<TQString>(ref);
    m_items.append(it);
}

void FlatpakJob::addUpdate(int scope, const TQString &ref)
{
    TxItem it;
    it.op = TxItem::Update;
    it.scope = scope;
    it.ref = TQDeepCopy<TQString>(ref);
    m_items.append(it);
}

void FlatpakJob::addUninstall(int scope, const TQString &ref)
{
    TxItem it;
    it.op = TxItem::Uninstall;
    it.scope = scope;
    it.ref = TQDeepCopy<TQString>(ref);
    m_items.append(it);
}

void FlatpakJob::addInstallFlatpakref(int scope, const TQByteArray &data)
{
    TxItem it;
    it.op = TxItem::Flatpakref;
    it.scope = scope;
    it.data.duplicate(data);
    m_items.append(it);
}

void FlatpakJob::addUninstallUnused(int scope)
{
    TxItem it;
    it.op = TxItem::UninstallUnused;
    it.scope = scope;
    m_items.append(it);
}

void FlatpakJob::setRemoteFromFile(int scope, const TQString &name, const TQByteArray &repoFile)
{
    m_remoteScope = scope;
    m_remoteName = TQDeepCopy<TQString>(name);
    m_remoteFile.duplicate(repoFile);
}

void FlatpakJob::setRemoteFromUrl(int scope, const TQString &name, const TQString &url, bool gpgVerify)
{
    m_remoteScope = scope;
    m_remoteName = TQDeepCopy<TQString>(name);
    m_remoteUrl = TQDeepCopy<TQString>(url);
    m_remoteGpgVerify = gpgVerify;
}

void FlatpakJob::setRemote(int scope, const TQString &name, bool disabled)
{
    m_remoteScope = scope;
    m_remoteName = TQDeepCopy<TQString>(name);
    m_remoteDisabled = disabled;
}

void FlatpakJob::postEvent(FlatpakJobEvent *ev)
{
    if (m_receiver) {
        TQApplication::postEvent(m_receiver, ev);
    } else {
        delete ev;
    }
}

void FlatpakJob::postLog(int kind, const TQString &text, const TQString &ref, int opType)
{
    if (kind == FlatpakJobEvent::Warning || kind == FlatpakJobEvent::Error) {
        TQString w = ref.isEmpty() ? text : ref + ": " + text;
        m_warnings.append(TQDeepCopy<TQString>(w));
    }
    FlatpakJobEvent *ev = new FlatpakJobEvent(FlatpakJobEvent::Log);
    ev->logKind = kind;
    ev->text = TQDeepCopy<TQString>(text);
    ev->ref = TQDeepCopy<TQString>(ref);
    ev->opType = opType;
    ev->opIndex = m_opIndex;
    ev->opCount = m_opCount;
    postEvent(ev);
}

void FlatpakJob::postProgress(const TQString &status, int opPercent)
{
    FlatpakJobEvent *ev = new FlatpakJobEvent(FlatpakJobEvent::Progress);
    ev->text = TQDeepCopy<TQString>(status);
    if (opPercent >= 0 && m_opCount > 0) {
        int done = (m_opIndex > 0 ? m_opIndex - 1 : 0);
        ev->percent = (done * 100 + opPercent) / m_opCount;
    } else {
        ev->percent = opPercent;
    }
    ev->opIndex = m_opIndex;
    ev->opCount = m_opCount;
    postEvent(ev);
}

void FlatpakJob::operationStarted(const TQString &ref, int opType)
{
    ++m_opIndex;
    postLog(FlatpakJobEvent::OperationStarted, TQString::null, ref, opType);
    postProgress(TQString::null, 0);
}

void FlatpakJob::run()
{
    GMainContext *ctx = g_main_context_new();
    g_main_context_push_thread_default(ctx);
    m_context = ctx;

    switch (m_kind) {
    case Transaction:
        m_success = runTransaction();
        break;
    case LoadCatalog:
        m_success = runLoadCatalog();
        break;
    case CheckUpdates:
        m_success = runCheckUpdates();
        break;
    case AddRemote:
        m_success = runAddRemote();
        break;
    case RemoveRemote:
        m_success = runRemoveRemote();
        break;
    case SetRemoteDisabled:
        m_success = runSetRemoteDisabled();
        break;
    }

    if (!m_success && m_error.isEmpty() && wasCancelled()) {
        m_error = "Operation was cancelled";
    }

    g_main_context_pop_thread_default(ctx);
    g_main_context_unref(ctx);
    m_context = 0;

    FlatpakJobEvent *ev = new FlatpakJobEvent(FlatpakJobEvent::Finished);
    ev->success = m_success;
    ev->cancelled = wasCancelled();
    ev->text = TQDeepCopy<TQString>(m_error);
    postEvent(ev);
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

bool FlatpakJob::runTransaction()
{
    bool ok = true;
    const int scopes[] = { FpScopeUser, FpScopeSystem };
    for (int i = 0; i < 2 && ok; ++i) {
        bool hasItems = false;
        TQValueList<TxItem>::ConstIterator it;
        for (it = m_items.begin(); it != m_items.end(); ++it) {
            if ((*it).scope == scopes[i]) {
                hasItems = true;
                break;
            }
        }
        if (hasItems) {
            ok = runTransactionForScope(scopes[i]);
        }
    }
    return ok;
}

bool FlatpakJob::runTransactionForScope(int scope)
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    GError *err = NULL;

    FlatpakInstallation *inst = fpOpenInstallation(scope, cancellable, &err);
    if (!inst) {
        m_error = fpTakeError(err);
        return false;
    }

    FlatpakTransaction *t = flatpak_transaction_new_for_installation(inst, cancellable, &err);
    if (!t) {
        m_error = fpTakeError(err);
        g_object_unref(inst);
        return false;
    }

    flatpak_transaction_add_default_dependency_sources(t);

    g_signal_connect(t, "ready", G_CALLBACK(onReady), this);
    g_signal_connect(t, "new-operation", G_CALLBACK(onNewOperation), this);
    g_signal_connect(t, "operation-done", G_CALLBACK(onOperationDone), this);
    g_signal_connect(t, "operation-error", G_CALLBACK(onOperationError), this);
    g_signal_connect(t, "add-new-remote", G_CALLBACK(onAddNewRemote), this);
    g_signal_connect(t, "choose-remote-for-ref", G_CALLBACK(onChooseRemoteForRef), this);
    g_signal_connect(t, "end-of-lifed", G_CALLBACK(onEndOfLifed), this);

    bool ok = true;
    bool hasFlatpakref = false;   // not counted by flatpak_transaction_is_empty()
    TQValueList<TxItem>::ConstIterator it;
    for (it = m_items.begin(); it != m_items.end() && ok; ++it) {
        const TxItem &item = *it;
        if (item.scope != scope) {
            continue;
        }
        gboolean added = TRUE;
        switch (item.op) {
        case TxItem::Install:
            added = flatpak_transaction_add_install(t, item.remote.utf8().data(),
                                                    item.ref.utf8().data(), NULL, &err);
            if (!added && g_error_matches(err, FLATPAK_ERROR, FLATPAK_ERROR_ALREADY_INSTALLED)) {
                postLog(FlatpakJobEvent::Info, fpTakeError(err), item.ref, FlatpakJobEvent::OpInstall);
                added = TRUE;
            }
            break;
        case TxItem::Update:
            added = flatpak_transaction_add_update(t, item.ref.utf8().data(), NULL, NULL, &err);
            break;
        case TxItem::Uninstall:
            added = flatpak_transaction_add_uninstall(t, item.ref.utf8().data(), &err);
            break;
        case TxItem::Flatpakref: {
            GBytes *bytes = g_bytes_new(item.data.data(), item.data.size());
            added = flatpak_transaction_add_install_flatpakref(t, bytes, &err);
            g_bytes_unref(bytes);
            hasFlatpakref = true;
            break;
        }
        case TxItem::UninstallUnused: {
            GPtrArray *unused = flatpak_installation_list_unused_refs(inst, NULL, cancellable, &err);
            if (!unused) {
                added = FALSE;
                break;
            }
            if (unused->len == 0) {
                postLog(FlatpakJobEvent::Info, "Nothing unused to uninstall");
            }
            for (guint i = 0; i < unused->len && added; ++i) {
                FlatpakRef *ref = FLATPAK_REF(g_ptr_array_index(unused, i));
                gchar *refStr = flatpak_ref_format_ref(ref);
                added = flatpak_transaction_add_uninstall(t, refStr, &err);
                g_free(refStr);
            }
            g_ptr_array_unref(unused);
            break;
        }
        }
        if (!added) {
            m_error = fpTakeError(err);
            ok = false;
        }
    }

    if (ok && flatpak_transaction_is_empty(t) && !hasFlatpakref) {
        postLog(FlatpakJobEvent::Info, "Nothing to do");
    } else if (ok) {
        m_opIndex = 0;
        m_opCount = 0;
        if (!flatpak_transaction_run(t, cancellable, &err)) {
            if (g_error_matches(err, FLATPAK_ERROR, FLATPAK_ERROR_ABORTED) && !m_warnings.isEmpty()) {
                // the actual reason has already been reported by operation-error
                m_error = m_warnings.last();
                g_clear_error(&err);
            } else {
                m_error = fpTakeError(err);
            }
            ok = false;
        }
    }

    g_object_unref(t);
    g_object_unref(inst);
    return ok;
}

// ---------------------------------------------------------------------------
// AppStream catalog
// ---------------------------------------------------------------------------

bool FlatpakJob::runLoadCatalog()
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    const int scopes[] = { FpScopeUser, FpScopeSystem };
    bool anyRemote = false;

    for (int i = 0; i < 2; ++i) {
        GError *err = NULL;
        FlatpakInstallation *inst = fpOpenInstallation(scopes[i], cancellable, &err);
        if (!inst) {
            postLog(FlatpakJobEvent::Warning, fpTakeError(err));
            continue;
        }
        GPtrArray *remotes = flatpak_installation_list_remotes(inst, cancellable, &err);
        if (!remotes) {
            postLog(FlatpakJobEvent::Warning, fpTakeError(err));
            g_object_unref(inst);
            continue;
        }

        for (guint j = 0; j < remotes->len; ++j) {
            if (g_cancellable_is_cancelled(cancellable)) {
                break;
            }
            FlatpakRemote *remote = FLATPAK_REMOTE(g_ptr_array_index(remotes, j));
            if (flatpak_remote_get_disabled(remote) || flatpak_remote_get_noenumerate(remote)) {
                continue;
            }
            anyRemote = true;
            const TQString name = fpStr(flatpak_remote_get_name(remote));

            GFile *dir = flatpak_remote_get_appstream_dir(remote, NULL);
            gchar *dirPath = g_file_get_path(dir);
            TQString base = fpStr(dirPath);
            g_free(dirPath);
            g_object_unref(dir);

            TQString xml = base + "/appstream.xml.gz";
            if (!TQFile::exists(xml) && TQFile::exists(base + "/appstream.xml")) {
                xml = base + "/appstream.xml";
            }

            bool needRefresh = m_forceRefresh || !TQFile::exists(xml);
            if (!needRefresh) {
                TQFileInfo fi(xml);
                needRefresh = fi.lastModified().secsTo(TQDateTime::currentDateTime()) > 24 * 3600;
            }

            if (needRefresh) {
                FlatpakJobEvent *ev = new FlatpakJobEvent(FlatpakJobEvent::Progress);
                ev->text = TQDeepCopy<TQString>(name);
                ev->opType = FlatpakJobEvent::OpOther;
                postEvent(ev);
                postLog(FlatpakJobEvent::Info, TQString("Refreshing AppStream data of %1").arg(name));

                if (!flatpak_installation_update_appstream_full_sync(inst, flatpak_remote_get_name(remote),
                                                                     NULL, NULL, NULL, NULL,
                                                                     cancellable, &err)) {
                    postLog(FlatpakJobEvent::Warning, fpTakeError(err), name);
                }
                // re-evaluate the location, the "active" link may have changed
                if (!TQFile::exists(xml) && TQFile::exists(base + "/appstream.xml.gz")) {
                    xml = base + "/appstream.xml.gz";
                }
            }

            if (!TQFile::exists(xml)) {
                postLog(FlatpakJobEvent::Warning, "No AppStream data available", name);
                continue;
            }

            TQString parseError;
            if (!AppStream::parseFile(xml, base + "/icons", name, scopes[i], m_lang, m_apps, parseError)) {
                postLog(FlatpakJobEvent::Warning, parseError, name);
            }
        }

        g_ptr_array_unref(remotes);
        g_object_unref(inst);
    }

    if (!anyRemote) {
        m_error = "No enabled remotes are configured";
        return false;
    }
    return !g_cancellable_is_cancelled(cancellable);
}

// ---------------------------------------------------------------------------
// Updates
// ---------------------------------------------------------------------------

bool FlatpakJob::runCheckUpdates()
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    const int scopes[] = { FpScopeUser, FpScopeSystem };
    bool anyOk = false;

    for (int i = 0; i < 2; ++i) {
        GError *err = NULL;
        FlatpakInstallation *inst = fpOpenInstallation(scopes[i], cancellable, &err);
        if (!inst) {
            postLog(FlatpakJobEvent::Warning, fpTakeError(err));
            continue;
        }
        GPtrArray *refs = flatpak_installation_list_installed_refs_for_update(inst, cancellable, &err);
        if (!refs) {
            m_error = fpTakeError(err);
            postLog(FlatpakJobEvent::Warning, m_error);
            g_object_unref(inst);
            continue;
        }
        anyOk = true;
        for (guint j = 0; j < refs->len; ++j) {
            m_updates.append(fpConvertInstalledRef(FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, j)), scopes[i]));
        }
        g_ptr_array_unref(refs);
        g_object_unref(inst);
    }
    if (anyOk) {
        m_error = TQString::null;
    }
    return anyOk;
}

// ---------------------------------------------------------------------------
// Remotes
// ---------------------------------------------------------------------------

/** Does the .flatpakrepo data contain a (non-empty) GPGKey entry? */
static bool repoFileHasGpgKey(const TQByteArray &data)
{
    GKeyFile *keyfile = g_key_file_new();
    bool hasKey = false;
    if (g_key_file_load_from_data(keyfile, data.data(), data.size(), G_KEY_FILE_NONE, NULL)) {
        gchar *key = g_key_file_get_string(keyfile, "Flatpak Repo", "GPGKey", NULL);
        hasKey = key && *g_strstrip(key);
        g_free(key);
    }
    g_key_file_free(keyfile);
    return hasKey;
}

bool FlatpakJob::runAddRemote()
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    GError *err = NULL;
    FlatpakRemote *remote = NULL;
    const TQCString name = m_remoteName.utf8();

    if (!m_remoteFile.isEmpty()) {
        GBytes *bytes = g_bytes_new(m_remoteFile.data(), m_remoteFile.size());
        remote = flatpak_remote_new_from_file(name.data(), bytes, &err);
        g_bytes_unref(bytes);
        if (!remote) {
            m_error = fpTakeError(err);
            return false;
        }

        // libflatpak quirk (seen in 1.16): flatpak_remote_new_from_file()
        // imports the GPG key but never marks gpg-verify as set, so neither
        // gpg-verify nor gpg-verify-summary is written to the repo config.
        // OSTree then defaults gpg-verify-summary to false and system-wide
        // pulls via the system helper fail with "Can't pull from untrusted
        // non-gpg verified remote".  Set it explicitly, like
        // "flatpak remote-add" does.
        const bool hasKey = repoFileHasGpgKey(m_remoteFile);
        flatpak_remote_set_gpg_verify(remote, hasKey);
        if (!hasKey) {
            postLog(FlatpakJobEvent::Warning,
                    "The repository file contains no GPG key; downloads from this remote are not verified",
                    m_remoteName);
        }
    } else {
        remote = flatpak_remote_new(name.data());
        flatpak_remote_set_url(remote, m_remoteUrl.utf8().data());
        flatpak_remote_set_gpg_verify(remote, m_remoteGpgVerify);
    }

    FlatpakInstallation *inst = fpOpenInstallation(m_remoteScope, cancellable, &err);
    if (!inst) {
        m_error = fpTakeError(err);
        g_object_unref(remote);
        return false;
    }

    // If the remote already exists, update it with the new settings instead
    // of silently doing nothing.  This also repairs remotes that were added
    // with a broken configuration.
    FlatpakRemote *existing = flatpak_installation_get_remote_by_name(inst, name.data(), cancellable, NULL);
    gboolean ok;
    if (existing) {
        g_object_unref(existing);
        ok = flatpak_installation_modify_remote(inst, remote, cancellable, &err);
        if (ok) {
            postLog(FlatpakJobEvent::Info,
                    TQString("Remote '%1' already existed; its configuration was updated").arg(m_remoteName));
        }
    } else {
        ok = flatpak_installation_add_remote(inst, remote, FALSE, cancellable, &err);
        if (ok) {
            postLog(FlatpakJobEvent::Info, TQString("Remote '%1' added").arg(m_remoteName));
        }
    }
    if (!ok) {
        m_error = fpTakeError(err);
    }
    g_object_unref(inst);
    g_object_unref(remote);
    return ok;
}

bool FlatpakJob::runRemoveRemote()
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    GError *err = NULL;
    FlatpakInstallation *inst = fpOpenInstallation(m_remoteScope, cancellable, &err);
    if (!inst) {
        m_error = fpTakeError(err);
        return false;
    }
    gboolean ok = flatpak_installation_remove_remote(inst, m_remoteName.utf8().data(), cancellable, &err);
    if (!ok) {
        m_error = fpTakeError(err);
    } else {
        postLog(FlatpakJobEvent::Info, TQString("Remote '%1' removed").arg(m_remoteName));
    }
    g_object_unref(inst);
    return ok;
}

bool FlatpakJob::runSetRemoteDisabled()
{
    GCancellable *cancellable = CANCELLABLE(m_cancellable);
    GError *err = NULL;
    FlatpakInstallation *inst = fpOpenInstallation(m_remoteScope, cancellable, &err);
    if (!inst) {
        m_error = fpTakeError(err);
        return false;
    }
    FlatpakRemote *remote = flatpak_installation_get_remote_by_name(inst, m_remoteName.utf8().data(),
                                                                    cancellable, &err);
    if (!remote) {
        m_error = fpTakeError(err);
        g_object_unref(inst);
        return false;
    }
    flatpak_remote_set_disabled(remote, m_remoteDisabled);
    gboolean ok = flatpak_installation_modify_remote(inst, remote, cancellable, &err);
    if (!ok) {
        m_error = fpTakeError(err);
    }
    g_object_unref(remote);
    g_object_unref(inst);
    return ok;
}
