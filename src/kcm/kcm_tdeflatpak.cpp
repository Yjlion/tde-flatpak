/*
 * tdeflatpak - Trinity Control Center module "Flatpak Applications".
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kcm_tdeflatpak.h"
#include "config.h"
#include "permissionswidget.h"
#include "preferenceswidget.h"

#include <tqlabel.h>
#include <tqlayout.h>
#include <tqtabwidget.h>

#include <tdeaboutdata.h>
#include <tdeapplication.h>
#include <kdialog.h>
#include <tdeglobal.h>
#include <kiconloader.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>

extern "C"
{
    TDE_EXPORT TDECModule *create_tdeflatpak(TQWidget *parent, const char *name)
    {
        TDEGlobal::locale()->insertCatalogue("tdeflatpak");
        return new TDEFlatpakModule(parent, name);
    }
}

TDEFlatpakModule::TDEFlatpakModule(TQWidget *parent, const char *name)
    : TDECModule(parent, name)
{
    TDEAboutData *about = new TDEAboutData("kcmtdeflatpak", I18N_NOOP("Flatpak Applications"),
                                           TDEFLATPAK_VERSION,
                                           I18N_NOOP("Settings and permissions of Flatpak applications"),
                                           TDEAboutData::License_GPL_V2,
                                           "(c) 2026 The tdeflatpak authors", 0,
                                           "https://github.com/Yjlion/tde-flatpak");
    about->addAuthor("tdeflatpak contributors", I18N_NOOP("Developer"));
    setAboutData(about);
    setButtons(Help | Default | Apply);

    TQVBoxLayout *layout = new TQVBoxLayout(this, 0, KDialog::spacingHint());

    TQHBoxLayout *top = new TQHBoxLayout(layout, KDialog::spacingHint());
    TQLabel *intro = new TQLabel(i18n("Change how Flatpak applications are installed and what they may access. "
                                      "To install, update or remove applications use the Flatpak Manager."), this);
    intro->setAlignment(TQt::WordBreak | TQt::AlignVCenter);
    top->addWidget(intro, 1);
    KPushButton *open = new KPushButton(SmallIconSet("tdeflatpak"), i18n("Open Flatpak &Manager"), this);
    top->addWidget(open);

    TQTabWidget *tabs = new TQTabWidget(this);
    layout->addWidget(tabs, 1);

    TQWidget *general = new TQWidget(tabs);
    TQVBoxLayout *gl = new TQVBoxLayout(general, KDialog::marginHint(), KDialog::spacingHint());
    m_prefs = new PreferencesWidget(general);
    gl->addWidget(m_prefs);
    tabs->addTab(general, i18n("&General"));

    TQWidget *perms = new TQWidget(tabs);
    TQVBoxLayout *pl = new TQVBoxLayout(perms, KDialog::marginHint(), KDialog::spacingHint());
    m_perms = new PermissionsWidget(perms);
    pl->addWidget(m_perms);
    tabs->addTab(perms, i18n("&Permissions"));

    connect(m_prefs, TQ_SIGNAL(changed()), this, TQ_SLOT(slotChanged()));
    connect(m_perms, TQ_SIGNAL(changed(bool)), this, TQ_SLOT(slotChanged()));
    connect(open, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotOpenManager()));

    load();
}

void TDEFlatpakModule::load()
{
    m_prefs->load();
    m_perms->load();
    emit changed(false);
}

void TDEFlatpakModule::save()
{
    m_prefs->save();
    TQString error;
    if (m_perms->isModified() && !m_perms->save(error)) {
        KMessageBox::error(this, i18n("Could not save the permissions:\n%1").arg(error));
        emit changed(true);
        return;
    }
    emit changed(false);
}

void TDEFlatpakModule::defaults()
{
    // permission overrides have no global "default" short of deleting
    // them all; they are reset per application with "Reset to Defaults"
    m_prefs->defaults();
}

TQString TDEFlatpakModule::quickHelp() const
{
    return i18n("<h1>Flatpak Applications</h1>"
                "<p>Here you can choose where Flatpak applications are installed, how often the "
                "Flatpak Manager checks for updates and whether it uses the Flathub web service.</p>"
                "<p>On the <b>Permissions</b> tab you can restrict or extend what each application may "
                "access: files, network, devices, D-Bus services and more. These changes apply to your "
                "user account only, exactly like <tt>flatpak override --user</tt>.</p>");
}

void TDEFlatpakModule::slotChanged()
{
    emit changed(true);
}

void TDEFlatpakModule::slotOpenManager()
{
    TQString error;
    if (TDEApplication::tdeinitExec("tdeflatpak", TQStringList(), &error) != 0) {
        KMessageBox::error(this, i18n("Could not start the Flatpak Manager:\n%1").arg(error));
    }
}

#include "kcm_tdeflatpak.moc"
