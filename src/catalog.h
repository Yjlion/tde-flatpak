/*
 * tdeflatpak - the application catalog shared by the Browse and Search pages.
 *
 * Owns the AppStream data of all enabled remotes (loaded by a FlatpakJob in
 * the background) and the set of installed refs used to mark entries as
 * installed.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_CATALOG_H
#define TDEFLATPAK_CATALOG_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqstringlist.h>

#include "flatpaktypes.h"

class FlatpakJob;

class Catalog : public TQObject
{
    TQ_OBJECT

public:
    static Catalog *self();
    ~Catalog();

    bool isLoaded() const { return m_loaded; }
    bool isLoading() const { return m_job != 0; }

    /** All applications.  Pointers into this list stay valid until loaded() is emitted again. */
    const FpAppList &apps() const { return m_apps; }

    /** First catalog entry with @p appId (user installation entries come first). */
    const FpApp *findById(const TQString &appId) const;

    /** Is this catalog entry installed? */
    bool isInstalled(const FpApp &app) const;
    /** Is @p appId installed in any installation? */
    bool isInstalledId(const TQString &appId) const;

    /** Installed refs, as last reported by setInstalled(). */
    const FpInstalledRefList &installed() const { return m_installedRefs; }
    void setInstalled(const FpInstalledRefList &refs);

    /** Remotes, as last reported by setRemotes(). */
    void setRemotes(const FpRemoteList &remotes);
    /** Does @p remote of installation @p scope serve Flathub? */
    bool isFlathubRemote(int scope, const TQString &remote) const;
    /** Is any enabled Flathub remote configured? */
    bool hasFlathub() const;

    TQString lastError() const { return m_lastError; }
    TQStringList warnings() const { return m_warnings; }

public slots:
    /** Load the catalog if not done yet. */
    void ensureLoaded();
    /** Reload, downloading fresh AppStream data. */
    void reload();

signals:
    void loadingStarted();
    void progress(const TQString &remote);
    /** The catalog was (re)loaded; @p ok is false if nothing could be loaded. */
    void loaded(bool ok);
    void installedChanged();

protected:
    virtual void customEvent(TQCustomEvent *ev);

private:
    Catalog();
    void load(bool force);

    FlatpakJob *m_job;
    bool m_loaded;
    FpAppList m_apps;
    TQMap<TQString, const FpApp *> m_byId;
    TQMap<TQString, bool> m_installed;      // fpRefKey -> true
    TQMap<TQString, bool> m_installedIds;   // app id -> true
    FpInstalledRefList m_installedRefs;
    TQMap<TQString, bool> m_flathubRemotes; // "<scope>:<name>" -> true
    TQString m_lastError;
    TQStringList m_warnings;

    static Catalog *s_self;
};

#endif
