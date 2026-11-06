/*
 * tdeflatpak - "Configure Flatpak Manager" dialog.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_CONFIGDIALOG_H
#define TDEFLATPAK_CONFIGDIALOG_H

#include <kdialogbase.h>

class PreferencesWidget;
class PermissionsWidget;

class ConfigDialog : public KDialogBase
{
    TQ_OBJECT

public:
    ConfigDialog(TQWidget *parent);

signals:
    /** Settings were saved. */
    void settingsChanged();

protected slots:
    virtual void slotOk();
    virtual void slotApply();
    virtual void slotDefault();

private slots:
    void slotChanged();

private:
    bool apply();

    PreferencesWidget *m_prefs;
    PermissionsWidget *m_perms;
};

#endif
