/*
 * tdeflatpak - "Configure Flatpak Manager" dialog.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configdialog.h"
#include "permissionswidget.h"
#include "preferenceswidget.h"

#include <tqlayout.h>
#include <tqvbox.h>

#include <kiconloader.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

ConfigDialog::ConfigDialog(TQWidget *parent)
    : KDialogBase(IconList, i18n("Configure Flatpak Manager"),
                  Default | Ok | Apply | Cancel, Ok, parent, "configdialog", true, true)
{
    TQVBox *page = addVBoxPage(i18n("General"), i18n("General Settings"), DesktopIcon("configure", 32));
    m_prefs = new PreferencesWidget(page);
    m_prefs->load();

    page = addVBoxPage(i18n("Permissions"), i18n("Application Permissions"), DesktopIcon("password", 32));
    m_perms = new PermissionsWidget(page);
    m_perms->load();

    connect(m_prefs, TQ_SIGNAL(changed()), this, TQ_SLOT(slotChanged()));
    connect(m_perms, TQ_SIGNAL(changed(bool)), this, TQ_SLOT(slotChanged()));
    enableButtonApply(false);
    resize(900, 620);
}

void ConfigDialog::slotChanged()
{
    enableButtonApply(true);
}

bool ConfigDialog::apply()
{
    m_prefs->save();
    TQString error;
    bool ok = !m_perms->isModified() || m_perms->save(error);
    if (!ok) {
        KMessageBox::error(this, i18n("Could not save the permissions:\n%1").arg(error));
    }
    enableButtonApply(!ok);
    emit settingsChanged();
    return ok;
}

void ConfigDialog::slotApply()
{
    apply();
}

void ConfigDialog::slotOk()
{
    if (apply()) {
        accept();
    }
}

void ConfigDialog::slotDefault()
{
    // only the general settings have defaults; permission overrides are
    // reset per application with "Reset to Defaults"
    m_prefs->defaults();
}

#include "configdialog.moc"
