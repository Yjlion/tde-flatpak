/*
 * tdeflatpak - background worker running libflatpak operations.
 *
 * Every long-running or privileged libflatpak call (transactions, AppStream
 * refresh, update checks, remote changes) runs in a FlatpakJob thread with
 * its own GMainContext, similar to how GNOME Software and Plasma Discover
 * run FlatpakTransaction off the UI thread.  Progress is reported to a
 * receiver TQObject via posted FlatpakJobEvent events.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATPAKJOB_H
#define TDEFLATPAK_FLATPAKJOB_H

#include "flatpaktypes.h"

#include <tqthread.h>
#include <tqevent.h>
#include <tqcstring.h>
#include <tqstringlist.h>

class TQObject;

/** Event posted from the worker thread to the GUI. */
class FlatpakJobEvent : public TQCustomEvent
{
public:
    enum {
        Progress = TQEvent::User + 4200,  ///< status/percent update
        Log,                              ///< a line for the details log
        Finished                          ///< job is done; see success/error
    };

    enum LogKind {
        Info,
        OperationStarted,
        OperationDone,
        Warning,
        Error
    };

    enum OperationType {
        OpInstall,
        OpUpdate,
        OpUninstall,
        OpInstallBundle,
        OpOther
    };

    FlatpakJobEvent(int type)
        : TQCustomEvent(type), percent(-1), logKind(Info), opType(OpOther),
          opIndex(0), opCount(0), success(false), cancelled(false) {}

    TQString text;       ///< status / log text / error message
    TQString ref;        ///< ref the event is about (if any)
    int percent;         ///< overall progress 0..100, -1 = unknown
    int logKind;
    int opType;
    int opIndex;         ///< 1-based index of the current operation
    int opCount;
    bool success;
    bool cancelled;
};


class FlatpakJob : public TQThread
{
public:
    enum Kind {
        Transaction,     ///< install / update / uninstall refs
        LoadCatalog,     ///< refresh (if needed) and parse AppStream of all remotes
        CheckUpdates,    ///< list installed refs that have updates
        AddRemote,
        RemoveRemote,
        SetRemoteDisabled
    };

    FlatpakJob(Kind kind, TQObject *receiver);
    virtual ~FlatpakJob();

    Kind kind() const { return m_kind; }

    /** Set the object receiving FlatpakJobEvents; call before start(). */
    void setReceiver(TQObject *receiver) { m_receiver = receiver; }

    /** Request cancellation (thread-safe). */
    void cancel();

    // ---- Transaction parameters ------------------------------------------
    void addInstall(int scope, const TQString &remote, const TQString &ref);
    void addUpdate(int scope, const TQString &ref);
    void addUninstall(int scope, const TQString &ref);
    void addInstallFlatpakref(int scope, const TQByteArray &data);
    void addUninstallUnused(int scope);

    // ---- LoadCatalog parameters ------------------------------------------
    void setForceRefresh(bool force) { m_forceRefresh = force; }
    void setLanguage(const TQString &lang) { m_lang = lang; }

    // ---- Remote parameters ------------------------------------------------
    /** Add a remote from .flatpakrepo file contents. */
    void setRemoteFromFile(int scope, const TQString &name, const TQByteArray &repoFile);
    /** Add a remote from a plain repository URL. */
    void setRemoteFromUrl(int scope, const TQString &name, const TQString &url, bool gpgVerify);
    /** Name/scope for RemoveRemote and SetRemoteDisabled. */
    void setRemote(int scope, const TQString &name, bool disabled = false);

    // ---- Results (valid after Finished) -----------------------------------
    bool success() const { return m_success; }
    bool wasCancelled() const;
    TQString errorString() const { return m_error; }
    const FpAppList &apps() const { return m_apps; }
    const FpInstalledRefList &updates() const { return m_updates; }
    TQStringList warnings() const { return m_warnings; }

    // ---- internal, used by the C callbacks ---------------------------------
    void postEvent(FlatpakJobEvent *ev);
    void postLog(int kind, const TQString &text, const TQString &ref = TQString::null, int opType = FlatpakJobEvent::OpOther);
    void postProgress(const TQString &status, int opPercent);
    void operationStarted(const TQString &ref, int opType);
    void setOperationCount(int count) { m_opCount = count; }
    int operationIndex() const { return m_opIndex; }
    int operationCount() const { return m_opCount; }

protected:
    virtual void run();

private:
    struct TxItem {
        enum Op { Install, Update, Uninstall, Flatpakref, UninstallUnused };
        int op;
        int scope;
        TQString remote;
        TQString ref;
        TQByteArray data;
    };

    bool runTransaction();
    bool runTransactionForScope(int scope);
    bool runLoadCatalog();
    bool runCheckUpdates();
    bool runAddRemote();
    bool runRemoveRemote();
    bool runSetRemoteDisabled();

    Kind m_kind;
    TQObject *m_receiver;
    void *m_cancellable;          // GCancellable*
    void *m_context;              // GMainContext* of the worker thread

    TQValueList<TxItem> m_items;

    bool m_forceRefresh;
    TQString m_lang;

    int m_remoteScope;
    TQString m_remoteName;
    TQString m_remoteUrl;
    TQByteArray m_remoteFile;
    bool m_remoteGpgVerify;
    bool m_remoteDisabled;

    int m_opIndex;
    int m_opCount;

    bool m_success;
    TQString m_error;
    TQStringList m_warnings;
    FpAppList m_apps;
    FpInstalledRefList m_updates;
};

#endif
