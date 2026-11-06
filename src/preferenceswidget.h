/*
 * tdeflatpak - the preferences form, used by the configuration dialog of
 * the manager and by the Control Center module.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_PREFERENCESWIDGET_H
#define TDEFLATPAK_PREFERENCESWIDGET_H

#include <tqwidget.h>

class TQCheckBox;
class KComboBox;
class KIntNumInput;

class PreferencesWidget : public TQWidget
{
    TQ_OBJECT

public:
    PreferencesWidget(TQWidget *parent = 0, const char *name = 0);

    void load();
    void save();
    void defaults();

signals:
    void changed();

private slots:
    void slotClearCache();

private:
    KComboBox *m_scope;
    KComboBox *m_deleteData;
    KIntNumInput *m_updateInterval;
    KIntNumInput *m_catalogAge;
    TQCheckBox *m_useApi;
};

#endif
