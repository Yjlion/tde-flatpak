/*
 * tdeflatpak - the preferences form.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "preferenceswidget.h"
#include "settings.h"

#include <tqcheckbox.h>
#include <tqdir.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqwhatsthis.h>

#include <kcombobox.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <tdeglobal.h>
#include <tdeio/netaccess.h>
#include <knuminput.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>
#include <kstandarddirs.h>
#include <kurl.h>

PreferencesWidget::PreferencesWidget(TQWidget *parent, const char *name)
    : TQWidget(parent, name)
{
    TQVBoxLayout *layout = new TQVBoxLayout(this, 0, KDialog::spacingHint());

    // ---- installing and removing
    TQGroupBox *g = new TQGroupBox(i18n("Installing and Removing"), this);
    layout->addWidget(g);
    TQGridLayout *grid = new TQGridLayout(g, 3, 2, KDialog::marginHint(), KDialog::spacingHint());
    grid->addRowSpacing(0, g->fontMetrics().lineSpacing());

    m_scope = new KComboBox(g);
    m_scope->insertItem(i18n("Ask every time"));
    m_scope->insertItem(i18n("System-wide (all users)"));
    m_scope->insertItem(i18n("For me only (user installation)"));
    TQLabel *l = new TQLabel(m_scope, i18n("Install Flatpak &files:"), g);
    grid->addWidget(l, 1, 0);
    grid->addWidget(m_scope, 1, 1);
    TQWhatsThis::add(m_scope, i18n("Where applications from .flatpakref files are installed. "
                                   "Applications from the catalog go to the installation of the remote they come from."));

    m_deleteData = new KComboBox(g);
    m_deleteData->insertItem(i18n("Ask"));
    m_deleteData->insertItem(i18n("Keep the data"));
    m_deleteData->insertItem(i18n("Delete the data"));
    l = new TQLabel(m_deleteData, i18n("After &uninstalling an application:"), g);
    grid->addWidget(l, 2, 0);
    grid->addWidget(m_deleteData, 2, 1);
    TQWhatsThis::add(m_deleteData, i18n("What happens to the settings and files an application stored in "
                                        "~/.var/app when it is uninstalled."));
    grid->setColStretch(1, 1);

    // ---- updates and catalog
    g = new TQGroupBox(i18n("Updates and Catalog"), this);
    layout->addWidget(g);
    grid = new TQGridLayout(g, 3, 2, KDialog::marginHint(), KDialog::spacingHint());
    grid->addRowSpacing(0, g->fontMetrics().lineSpacing());

    m_updateInterval = new KIntNumInput(g);
    m_updateInterval->setRange(0, 168, 1, false);
    m_updateInterval->setSuffix(i18n(" hours"));
    m_updateInterval->setSpecialValueText(i18n("Never"));
    l = new TQLabel(m_updateInterval, i18n("&Check for updates every:"), g);
    grid->addWidget(l, 1, 0);
    grid->addWidget(m_updateInterval, 1, 1);
    TQWhatsThis::add(m_updateInterval, i18n("While the Flatpak Manager is running it checks for updates in the background "
                                            "and shows the number of available updates on the Updates tab."));

    m_catalogAge = new KIntNumInput(g);
    m_catalogAge->setRange(1, 720, 1, false);
    m_catalogAge->setSuffix(i18n(" hours"));
    l = new TQLabel(m_catalogAge, i18n("&Download the catalog again after:"), g);
    grid->addWidget(l, 2, 0);
    grid->addWidget(m_catalogAge, 2, 1);
    grid->setColStretch(1, 1);

    // ---- online services
    g = new TQGroupBox(i18n("Online Services"), this);
    layout->addWidget(g);
    TQVBoxLayout *vl = new TQVBoxLayout(g, KDialog::marginHint(), KDialog::spacingHint());
    vl->addSpacing(g->fontMetrics().lineSpacing());
    m_useApi = new TQCheckBox(i18n("Use the &Flathub web service"), g);
    vl->addWidget(m_useApi);
    TQLabel *info = new TQLabel(i18n("Shows screenshots, download counts, sizes and requested permissions of Flathub "
                                     "applications, and the Popular, Trending and New lists. The web service at "
                                     "flathub.org learns which applications you look at."), g);
    info->setAlignment(TQt::WordBreak);
    vl->addWidget(info);
    TQHBoxLayout *hl = new TQHBoxLayout(vl);
    hl->addStretch(1);
    KPushButton *clear = new KPushButton(SmallIconSet("edit-clear"), i18n("C&lear Cached Data"), g);
    hl->addWidget(clear);

    layout->addStretch(1);

    connect(m_scope, TQ_SIGNAL(activated(int)), this, TQ_SIGNAL(changed()));
    connect(m_deleteData, TQ_SIGNAL(activated(int)), this, TQ_SIGNAL(changed()));
    connect(m_updateInterval, TQ_SIGNAL(valueChanged(int)), this, TQ_SIGNAL(changed()));
    connect(m_catalogAge, TQ_SIGNAL(valueChanged(int)), this, TQ_SIGNAL(changed()));
    connect(m_useApi, TQ_SIGNAL(toggled(bool)), this, TQ_SIGNAL(changed()));
    connect(clear, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotClearCache()));
}

void PreferencesWidget::load()
{
    TdeFlatpakSettings::self()->readConfig();
    blockSignals(true);
    m_scope->setCurrentItem(TdeFlatpakSettings::defaultScope());
    m_deleteData->setCurrentItem(TdeFlatpakSettings::deleteDataOnUninstall());
    m_updateInterval->setValue(TdeFlatpakSettings::updateCheckInterval());
    m_catalogAge->setValue(TdeFlatpakSettings::catalogMaxAge());
    m_useApi->setChecked(TdeFlatpakSettings::useFlathubApi());
    blockSignals(false);
}

void PreferencesWidget::save()
{
    TdeFlatpakSettings::setDefaultScope(m_scope->currentItem());
    TdeFlatpakSettings::setDeleteDataOnUninstall(m_deleteData->currentItem());
    TdeFlatpakSettings::setUpdateCheckInterval(m_updateInterval->value());
    TdeFlatpakSettings::setCatalogMaxAge(m_catalogAge->value());
    TdeFlatpakSettings::setUseFlathubApi(m_useApi->isChecked());
    TdeFlatpakSettings::self()->writeConfig();
}

void PreferencesWidget::defaults()
{
    TdeFlatpakSettings::self()->useDefaults(true);
    m_scope->setCurrentItem(TdeFlatpakSettings::defaultScope());
    m_deleteData->setCurrentItem(TdeFlatpakSettings::deleteDataOnUninstall());
    m_updateInterval->setValue(TdeFlatpakSettings::updateCheckInterval());
    m_catalogAge->setValue(TdeFlatpakSettings::catalogMaxAge());
    m_useApi->setChecked(TdeFlatpakSettings::useFlathubApi());
    TdeFlatpakSettings::self()->useDefaults(false);
    emit changed();
}

void PreferencesWidget::slotClearCache()
{
    const TQString base = TDEGlobal::dirs()->saveLocation("cache", "tdeflatpak/");
    const char *subdirs[] = { "api", "images", 0 };
    for (int i = 0; subdirs[i]; ++i) {
        const TQString dir = base + subdirs[i];
        if (TQDir(dir).exists()) {
            TDEIO::NetAccess::del(KURL::fromPathOrURL(dir), this);
        }
    }
    KMessageBox::information(this, i18n("The cached Flathub data and images have been deleted."));
}

#include "preferenceswidget.moc"
