/*
 * tdeflatpak - Trinity Control Center module "Flatpak Applications".
 *
 * Holds the tdeflatpak preferences and the per-application permission
 * editor, and starts the Flatpak Manager for everything else.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_KCM_H
#define TDEFLATPAK_KCM_H

#include <tdecmodule.h>

class PreferencesWidget;
class PermissionsWidget;

class TDEFlatpakModule : public TDECModule
{
    TQ_OBJECT

public:
    TDEFlatpakModule(TQWidget *parent = 0, const char *name = 0);

    virtual void load();
    virtual void save();
    virtual void defaults();
    virtual TQString quickHelp() const;

private slots:
    void slotChanged();
    void slotOpenManager();

private:
    PreferencesWidget *m_prefs;
    PermissionsWidget *m_perms;
};

#endif
