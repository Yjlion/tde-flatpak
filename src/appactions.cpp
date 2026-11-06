/*
 * tdeflatpak - user level actions shared by the pages and dialogs.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appactions.h"
#include "flatpakbackend.h"
#include "flatpakjob.h"
#include "operationdialog.h"
#include "portalpermissions.h"
#include "settings.h"

#include <tqdir.h>
#include <tqwidget.h>

#include <tdeio/netaccess.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kstdguiitem.h>
#include <kurl.h>

namespace
{

bool runSetConfig(TQWidget *parent, int scope, const TQString &key, const TQStringList &patterns,
                  const TQString &caption)
{
    FlatpakJob *job = new FlatpakJob(FlatpakJob::SetConfig, 0);
    job->setConfig(scope, key, patterns.join(";"));
    return OperationDialog::run(parent, job, caption, "<b>" + caption + "</b>", true);
}

/** @p patterns without those matching @p ref */
TQStringList withoutPatternsMatching(const TQStringList &patterns, const TQString &ref)
{
    TQStringList out;
    for (TQStringList::ConstIterator it = patterns.begin(); it != patterns.end(); ++it) {
        if (!FlatpakBackend::patternsMatch(TQStringList(*it), ref)) {
            out.append(*it);
        }
    }
    return out;
}

}

namespace AppActions
{

bool install(TQWidget *parent, const FpAppPtrList &apps)
{
    TQStringList names;
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);
    for (FpAppPtrList::ConstIterator it = apps.begin(); it != apps.end(); ++it) {
        const FpApp *a = *it;
        names.append(i18n("%1 from %2 (%3)").arg(a->name).arg(a->remote).arg(FlatpakBackend::scopeName(a->scope)));
        job->addInstall(a->scope, a->remote, a->ref);
    }
    if (names.isEmpty()) {
        delete job;
        return false;
    }

    int answer = KMessageBox::questionYesNoList(parent,
        i18n("Install the following application?", "Install the following %n applications?", names.count()),
        names, i18n("Install"),
        KGuiItem(i18n("&Install"), "go-down"), KStdGuiItem::cancel());
    if (answer != KMessageBox::Yes) {
        delete job;
        return false;
    }

    OperationDialog::run(parent, job, i18n("Install"),
                         i18n("<b>Installing %n application</b>", "<b>Installing %n applications</b>", names.count()));
    return true;
}

bool uninstall(TQWidget *parent, const FpInstalledRefList &refs)
{
    TQStringList names;
    FlatpakJob *job = new FlatpakJob(FlatpakJob::Transaction, 0);
    for (FpInstalledRefList::ConstIterator it = refs.begin(); it != refs.end(); ++it) {
        const FpInstalledRef &r = *it;
        names.append(TQString("%1 (%2, %3)").arg(r.name).arg(r.branch).arg(FlatpakBackend::scopeName(r.scope)));
        job->addUninstall(r.scope, r.ref);
    }
    if (names.isEmpty()) {
        delete job;
        return false;
    }

    int answer = KMessageBox::warningContinueCancelList(parent,
        i18n("Do you really want to uninstall the following item?",
             "Do you really want to uninstall the following %n items?", names.count()),
        names, i18n("Uninstall"),
        KGuiItem(i18n("&Uninstall"), "edit-delete"));
    if (answer != KMessageBox::Continue) {
        delete job;
        return false;
    }

    const bool ok = OperationDialog::run(parent, job, i18n("Uninstall"),
                         i18n("<b>Uninstalling %n item</b>", "<b>Uninstalling %n items</b>", names.count()));
    if (!ok || TdeFlatpakSettings::deleteDataOnUninstall() == TdeFlatpakSettings::EnumDeleteDataOnUninstall::Never) {
        return true;
    }

    // applications that are gone from every installation and left data behind
    FpInstalledRefList remaining;
    TQString error;
    FlatpakBackend::listInstalled(remaining, error);
    TQMap<TQString, bool> stillInstalled;
    for (FpInstalledRefList::ConstIterator it = remaining.begin(); it != remaining.end(); ++it) {
        stillInstalled[(*it).id] = true;
    }
    TQStringList ids, dataNames;
    TQMap<TQString, TQString> nameOf;
    for (FpInstalledRefList::ConstIterator it = refs.begin(); it != refs.end(); ++it) {
        const FpInstalledRef &r = *it;
        if (!r.isApp || stillInstalled.contains(r.id) || ids.contains(r.id)) {
            continue;
        }
        if (TQDir(FlatpakBackend::appDataDir(r.id)).exists()) {
            ids.append(r.id);
            nameOf[r.id] = r.name;
            dataNames.append(TQString("%1 (%2)").arg(r.name).arg(FlatpakBackend::appDataDir(r.id)));
        }
    }
    if (ids.isEmpty()) {
        return true;
    }
    if (TdeFlatpakSettings::deleteDataOnUninstall() == TdeFlatpakSettings::EnumDeleteDataOnUninstall::Ask) {
        int del = KMessageBox::questionYesNoList(parent,
            i18n("The uninstalled application left personal data and settings behind. Delete them too?",
                 "The uninstalled applications left personal data and settings behind. Delete them too?",
                 ids.count()),
            dataNames, i18n("Delete Application Data"),
            KGuiItem(i18n("&Delete Data"), "edit-delete"), KGuiItem(i18n("&Keep Data"), "process-stop"));
        if (del != KMessageBox::Yes) {
            return true;
        }
    }
    for (TQStringList::ConstIterator it = ids.begin(); it != ids.end(); ++it) {
        deleteData(parent, *it, nameOf[*it], false);
    }
    return true;
}

bool launch(TQWidget *parent, const FpInstalledRef &ref)
{
    TQString error;
    if (!FlatpakBackend::launch(ref.scope, ref.ref, error)) {
        KMessageBox::error(parent, i18n("Could not start %1:\n%2").arg(ref.name).arg(error));
        return false;
    }
    return true;
}

bool isMasked(int scope, const TQString &ref)
{
    return FlatpakBackend::patternsMatch(FlatpakBackend::configPatterns(scope, "masked"), ref);
}

bool setMasked(TQWidget *parent, int scope, const TQString &ref, bool masked)
{
    const TQString appId = ref.section('/', 1, 1);
    const TQStringList current = FlatpakBackend::configPatterns(scope, "masked");
    TQStringList patterns;
    if (masked) {
        patterns = current;
        if (!FlatpakBackend::patternsMatch(current, ref)) {
            patterns.append(appId);    // what "flatpak mask <id>" stores
        }
    } else {
        patterns = withoutPatternsMatching(current, ref);
    }
    if (patterns == current) {
        return true;
    }
    return runSetConfig(parent, scope, "masked", patterns,
                        masked ? i18n("Blocking updates of %1").arg(appId)
                               : i18n("Allowing updates of %1").arg(appId));
}

bool isPinned(int scope, const TQString &ref)
{
    return FlatpakBackend::patternsMatch(FlatpakBackend::configPatterns(scope, "pinned"), ref);
}

bool setPinned(TQWidget *parent, int scope, const TQString &ref, bool pinned)
{
    const TQStringList current = FlatpakBackend::configPatterns(scope, "pinned");
    TQStringList patterns;
    if (pinned) {
        patterns = current;
        if (!FlatpakBackend::patternsMatch(current, ref)) {
            patterns.append(ref);
        }
    } else {
        patterns = withoutPatternsMatching(current, ref);
    }
    if (patterns == current) {
        return true;
    }
    return runSetConfig(parent, scope, "pinned", patterns,
                        pinned ? i18n("Pinning %1").arg(ref.section('/', 1, 1))
                               : i18n("Unpinning %1").arg(ref.section('/', 1, 1)));
}

bool deleteData(TQWidget *parent, const TQString &appId, const TQString &appName, bool confirm)
{
    const TQString dir = FlatpakBackend::appDataDir(appId);
    if (FlatpakBackend::isRunning(appId)) {
        KMessageBox::sorry(parent, i18n("%1 is running. Close it before deleting its data.").arg(appName));
        return false;
    }
    if (confirm) {
        int answer = KMessageBox::warningContinueCancel(parent,
            i18n("<qt>Delete all personal data and settings of <b>%1</b>?<br><br>"
                 "The folder <tt>%2</tt> is removed and portal permissions you granted are revoked. "
                 "This cannot be undone.</qt>").arg(appName).arg(dir),
            i18n("Delete Application Data"), KGuiItem(i18n("&Delete Data"), "edit-delete"));
        if (answer != KMessageBox::Continue) {
            return false;
        }
    }
    bool ok = true;
    if (TQDir(dir).exists() && !TDEIO::NetAccess::del(KURL::fromPathOrURL(dir), parent)) {
        KMessageBox::error(parent, i18n("Could not delete %1:\n%2").arg(dir).arg(TDEIO::NetAccess::lastErrorString()));
        ok = false;
    }
    TQString error;
    PortalPermissions::revokeAll(appId, error);   // best effort; the store may not be running
    return ok;
}

}
