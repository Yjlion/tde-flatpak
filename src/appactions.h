/*
 * tdeflatpak - user level actions shared by the pages, the details dialog
 * and the permissions editor: confirm, run the job, report errors.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_APPACTIONS_H
#define TDEFLATPAK_APPACTIONS_H

#include "flatpaktypes.h"

class TQWidget;

typedef TQValueList<const FpApp *> FpAppPtrList;

namespace AppActions
{
    /** Ask for confirmation and install @p apps.  Returns true if a transaction ran. */
    bool install(TQWidget *parent, const FpAppPtrList &apps);

    /**
     * Ask for confirmation and uninstall @p refs.  Afterwards offers to delete
     * the data of applications that are no longer installed anywhere,
     * according to the DeleteDataOnUninstall setting.  Returns true if a
     * transaction ran.
     */
    bool uninstall(TQWidget *parent, const FpInstalledRefList &refs);

    /** Start an installed application, reporting errors. */
    bool launch(TQWidget *parent, const FpInstalledRef &ref);

    /** Is @p ref blocked from updates ("flatpak mask")? */
    bool isMasked(int scope, const TQString &ref);
    /** Block or allow updates of the installed application @p ref in installation @p scope. */
    bool setMasked(TQWidget *parent, int scope, const TQString &ref, bool masked);

    /** Is the runtime @p ref pinned (kept even when unused, "flatpak pin")? */
    bool isPinned(int scope, const TQString &ref);
    bool setPinned(TQWidget *parent, int scope, const TQString &ref, bool pinned);

    /**
     * Delete ~/.var/app/<appId> and the app's portal permissions.  Refuses
     * while the application is running.  Asks first if @p confirm is set.
     */
    bool deleteData(TQWidget *parent, const TQString &appId, const TQString &appName, bool confirm = true);
}

#endif
