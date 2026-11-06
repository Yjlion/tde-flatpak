/*
 * tdeflatpak - per-application permission editor.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permissionswidget.h"
#include "appactions.h"
#include "flatpakbackend.h"
#include "iconcache.h"
#include "overrides.h"
#include "permissiontext.h"
#include "portalpermissions.h"

#include <tqcheckbox.h>
#include <tqdir.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqscrollview.h>
#include <tqsplitter.h>
#include <tqstylesheet.h>
#include <tqtabwidget.h>
#include <tqvbox.h>
#include <tqwhatsthis.h>

#include <tdeapplication.h>
#include <kcombobox.h>
#include <kdialog.h>
#include <kdirsize.h>
#include <kiconloader.h>
#include <tdeio/global.h>
#include <tdeio/job.h>
#include <klineedit.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>
#include <krun.h>
#include <kstdguiitem.h>
#include <kurl.h>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

namespace
{

TQString policyText(const TQString &policy)
{
    if (policy == "talk") {
        return i18n("Talk");
    }
    if (policy == "own") {
        return i18n("Own");
    }
    if (policy == "see") {
        return i18n("See");
    }
    if (policy == "none") {
        return i18n("No access");
    }
    return policy;
}

TQStringList removeEntries(const TQStringList &list, const TQString &value)
{
    TQStringList out;
    for (TQStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        if (*it != value && *it != "!" + value) {
            out.append(*it);
        }
    }
    return out;
}

TQStringList removeFilesystem(const TQStringList &list, const TQString &path)
{
    TQStringList out;
    for (TQStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        if (Overrides::filesystemPath(*it) != path) {
            out.append(*it);
        }
    }
    return out;
}

bool hasFilesystem(const TQStringList &list, const TQString &path)
{
    for (TQStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
        if (Overrides::filesystemPath(*it) == path) {
            return true;
        }
    }
    return false;
}

/** Small modal dialog with labelled input rows. */
class InputDialog : public KDialogBase
{
public:
    InputDialog(TQWidget *parent, const TQString &caption, const TQString &intro)
        : KDialogBase(parent, "inputdialog", true, caption, Ok | Cancel, Ok, true)
    {
        TQWidget *page = new TQWidget(this);
        setMainWidget(page);
        m_grid = new TQGridLayout(page, 1, 2, 0, spacingHint());
        m_row = 0;
        if (!intro.isEmpty()) {
            TQLabel *l = new TQLabel(intro, page);
            l->setAlignment(TQt::WordBreak);
            m_grid->addMultiCellWidget(l, 0, 0, 0, 1);
            ++m_row;
        }
    }

    KComboBox *addCombo(const TQString &label, const TQStringList &items, bool editable)
    {
        KComboBox *c = new KComboBox(editable, mainWidget());
        c->insertStringList(items);
        if (editable) {
            c->setCurrentText(TQString::null);
        }
        addRow(label, c);
        return c;
    }

    KLineEdit *addLine(const TQString &label)
    {
        KLineEdit *e = new KLineEdit(mainWidget());
        addRow(label, e);
        return e;
    }

private:
    void addRow(const TQString &label, TQWidget *w)
    {
        TQLabel *l = new TQLabel(label, mainWidget());
        l->setBuddy(w);
        m_grid->addWidget(l, m_row, 0);
        m_grid->addWidget(w, m_row, 1);
        ++m_row;
    }

    TQGridLayout *m_grid;
    int m_row;
};

}

// ---------------------------------------------------------------------------
// PermissionsWidget
// ---------------------------------------------------------------------------

PermissionsWidget::PermissionsWidget(TQWidget *parent, const char *name)
    : TQWidget(parent, name),
      m_portalsLoaded(false),
      m_sizeJob(0),
      m_filling(false)
{
    TQHBoxLayout *layout = new TQHBoxLayout(this, 0, KDialog::spacingHint());
    TQSplitter *splitter = new TQSplitter(TQt::Horizontal, this);
    layout->addWidget(splitter);

    m_appList = new TDEListView(splitter);
    m_appList->addColumn(i18n("Application"));
    m_appList->setFullWidth(true);
    m_appList->setSorting(-1);
    m_appList->setMinimumWidth(180);

    TQWidget *right = new TQWidget(splitter);
    TQVBoxLayout *rl = new TQVBoxLayout(right, 0, KDialog::spacingHint());

    m_header = new TQLabel(right);
    m_header->setTextFormat(TQt::RichText);
    rl->addWidget(m_header);

    m_tabs = new TQTabWidget(right);
    rl->addWidget(m_tabs, 1);
    m_permTab = createPermissionsTab();
    m_filesTab = createFilesTab();
    m_envTab = createEnvTab();
    m_portalTab = createPortalsTab();
    m_storageTab = createStorageTab();
    m_tabs->addTab(m_permTab, i18n("&Permissions"));
    m_tabs->addTab(m_filesTab, i18n("&Files"));
    m_tabs->addTab(m_envTab, i18n("&Environment && D-Bus"));
    m_tabs->addTab(m_portalTab, i18n("Por&tals"));
    m_tabs->addTab(m_storageTab, i18n("&Storage && Updates"));

    TQHBoxLayout *bottom = new TQHBoxLayout(rl, KDialog::spacingHint());
    m_fileLabel = new TQLabel(right);
    m_fileLabel->setTextFormat(TQt::RichText);
    bottom->addWidget(m_fileLabel, 1);
    m_resetButton = new KPushButton(SmallIconSet("edit-undo"), i18n("&Reset to Defaults"), right);
    TQWhatsThis::add(m_resetButton, i18n("Remove all your changes to the permissions of this application. "
                                         "The application then gets exactly what it requests."));
    bottom->addWidget(m_resetButton);

    splitter->setResizeMode(m_appList, TQSplitter::KeepSize);
    TQValueList<int> sizes;
    sizes << 200 << 500;
    splitter->setSizes(sizes);

    connect(m_appList, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotAppSelected()));
    connect(m_resetButton, TQ_SIGNAL(clicked()), this, TQ_SLOT(resetCurrent()));
    connect(m_tabs, TQ_SIGNAL(currentChanged(TQWidget *)), this, TQ_SLOT(slotTabChanged(TQWidget *)));
}

void PermissionsWidget::addCheck(TQWidget *parent, const TQString &key, const TQString &value,
                                 const TQString &label, const TQString &whatsThis)
{
    TQCheckBox *cb = new TQCheckBox(label, parent);
    TQWhatsThis::add(cb, whatsThis + "<p><i>" + key + "=" + value + "</i></p>");
    Check c;
    c.key = key;
    c.value = value;
    m_checks[cb] = c;
    connect(cb, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotCheckToggled()));
}

TQWidget *PermissionsWidget::createPermissionsTab()
{
    TQScrollView *sv = new TQScrollView(m_tabs);
    sv->setResizePolicy(TQScrollView::AutoOneFit);
    sv->setFrameStyle(TQFrame::NoFrame);
    TQVBox *box = new TQVBox(sv->viewport());
    box->setMargin(KDialog::marginHint());
    box->setSpacing(KDialog::spacingHint());
    sv->addChild(box);

    TQGroupBox *g = new TQGroupBox(1, TQt::Horizontal, i18n("Network and Communication"), box);
    addCheck(g, "shared", "network", i18n("&Network access"),
             i18n("The application can connect to the internet and the local network."));
    addCheck(g, "shared", "ipc", i18n("Inter-process communication"),
             i18n("Share the IPC namespace with the host. Needed by X11 applications for shared memory."));

    g = new TQGroupBox(1, TQt::Horizontal, i18n("Display and Sound"), box);
    addCheck(g, "sockets", "x11", i18n("X11 display"),
             i18n("Access to the X11 display server. X11 does not isolate applications: "
                  "an application with X11 access can read keystrokes and the screen contents of other windows."));
    addCheck(g, "sockets", "fallback-x11", i18n("X11 display when Wayland is not available"),
             i18n("Access to X11 only when the session is not a Wayland session (TDE sessions are X11 sessions)."));
    addCheck(g, "sockets", "wayland", i18n("Wayland display"), i18n("Access to the Wayland compositor."));
    addCheck(g, "sockets", "pulseaudio", i18n("Sound (PulseAudio / PipeWire)"),
             i18n("Play and record sound through the PulseAudio socket."));
    addCheck(g, "devices", "dri", i18n("Graphics acceleration (GPU)"),
             i18n("Direct access to the graphics card for OpenGL / Vulkan rendering."));

    g = new TQGroupBox(1, TQt::Horizontal, i18n("Devices"), box);
    addCheck(g, "devices", "input", i18n("Input devices (joysticks, game controllers)"),
             i18n("Access to /dev/input."));
    addCheck(g, "devices", "usb", i18n("USB devices"), i18n("Raw access to USB devices."));
    addCheck(g, "devices", "kvm", i18n("Virtualization (KVM)"), i18n("Access to /dev/kvm."));
    addCheck(g, "devices", "shm", i18n("Shared memory (/dev/shm)"), i18n("Access to the host /dev/shm."));
    addCheck(g, "devices", "all", i18n("All devices (webcams, scanners, ...)"),
             i18n("Access to every device in /dev, including cameras and other hardware."));
    addCheck(g, "features", "bluetooth", i18n("Bluetooth"), i18n("Use Bluetooth sockets."));
    addCheck(g, "features", "canbus", i18n("CAN bus"), i18n("Use CAN bus sockets."));

    g = new TQGroupBox(1, TQt::Horizontal, i18n("Services and Advanced"), box);
    addCheck(g, "sockets", "session-bus", i18n("Full session bus access"),
             i18n("Unrestricted access to the D-Bus session bus. The application can talk to every service of your session."));
    addCheck(g, "sockets", "system-bus", i18n("Full system bus access"),
             i18n("Unrestricted access to the D-Bus system bus."));
    addCheck(g, "sockets", "ssh-auth", i18n("SSH agent"), i18n("Use your SSH keys through the SSH agent."));
    addCheck(g, "sockets", "gpg-agent", i18n("GnuPG agent"), i18n("Use your GnuPG keys through gpg-agent."));
    addCheck(g, "sockets", "pcsc", i18n("Smart cards (PC/SC)"), i18n("Access smart cards through pcscd."));
    addCheck(g, "sockets", "cups", i18n("Printing (CUPS)"), i18n("Talk to the printing system directly."));
    addCheck(g, "features", "devel", i18n("Development syscalls (ptrace, perf)"),
             i18n("Allow system calls used by debuggers and profilers."));
    addCheck(g, "features", "multiarch", i18n("Run programs for other architectures"),
             i18n("Allow running e.g. 32-bit programs on a 64-bit system."));
    addCheck(g, "features", "per-app-dev-shm", i18n("Private /dev/shm shared by all instances"),
             i18n("Share a per-application /dev/shm between instances of the application."));

    new TQWidget(box);   // stretch
    return sv;
}

TQWidget *PermissionsWidget::createFilesTab()
{
    TQWidget *w = new TQWidget(m_tabs);
    TQVBoxLayout *l = new TQVBoxLayout(w, KDialog::marginHint(), KDialog::spacingHint());
    TQLabel *intro = new TQLabel(i18n("Folders and files the application can access in addition to its own data:"), w);
    intro->setAlignment(TQt::WordBreak);
    l->addWidget(intro);

    m_fsList = new TDEListView(w);
    m_fsList->addColumn(i18n("Location"));
    m_fsList->addColumn(i18n("Access"));
    m_fsList->addColumn(i18n("Set By"));
    m_fsList->setAllColumnsShowFocus(true);
    l->addWidget(m_fsList, 1);

    TQHBoxLayout *b = new TQHBoxLayout(l, KDialog::spacingHint());
    b->addStretch(1);
    KPushButton *add = new KPushButton(SmallIconSet("list-add"), i18n("A&dd..."), w);
    m_fsRemove = new KPushButton(SmallIconSet("list-remove"), i18n("Remo&ve"), w);
    TQWhatsThis::add(m_fsRemove, i18n("Removes a location you added, or revokes access the application requested."));
    b->addWidget(add);
    b->addWidget(m_fsRemove);

    connect(add, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotFsAdd()));
    connect(m_fsRemove, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotFsRemove()));
    connect(m_fsList, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotListSelectionChanged()));
    return w;
}

TQWidget *PermissionsWidget::createEnvTab()
{
    TQWidget *w = new TQWidget(m_tabs);
    TQVBoxLayout *l = new TQVBoxLayout(w, KDialog::marginHint(), KDialog::spacingHint());

    l->addWidget(new TQLabel(i18n("Environment variables:"), w));
    m_envList = new TDEListView(w);
    m_envList->addColumn(i18n("Variable"));
    m_envList->addColumn(i18n("Value"));
    m_envList->addColumn(i18n("Set By"));
    m_envList->setAllColumnsShowFocus(true);
    l->addWidget(m_envList, 1);
    TQHBoxLayout *b = new TQHBoxLayout(l, KDialog::spacingHint());
    b->addStretch(1);
    KPushButton *envAdd = new KPushButton(SmallIconSet("list-add"), i18n("Add &Variable..."), w);
    m_envRemove = new KPushButton(SmallIconSet("list-remove"), i18n("Re&move"), w);
    b->addWidget(envAdd);
    b->addWidget(m_envRemove);

    l->addWidget(new TQLabel(i18n("D-Bus services the application may use:"), w));
    m_busList = new TDEListView(w);
    m_busList->addColumn(i18n("Name"));
    m_busList->addColumn(i18n("Bus"));
    m_busList->addColumn(i18n("Access"));
    m_busList->addColumn(i18n("Set By"));
    m_busList->setAllColumnsShowFocus(true);
    l->addWidget(m_busList, 1);
    b = new TQHBoxLayout(l, KDialog::spacingHint());
    b->addStretch(1);
    KPushButton *busAdd = new KPushButton(SmallIconSet("list-add"), i18n("Add &D-Bus Name..."), w);
    m_busRemove = new KPushButton(SmallIconSet("list-remove"), i18n("Remove Bus &Name"), w);
    b->addWidget(busAdd);
    b->addWidget(m_busRemove);

    connect(envAdd, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotEnvAdd()));
    connect(m_envRemove, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotEnvRemove()));
    connect(busAdd, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotBusAdd()));
    connect(m_busRemove, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotBusRemove()));
    connect(m_envList, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotListSelectionChanged()));
    connect(m_busList, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotListSelectionChanged()));
    return w;
}

TQWidget *PermissionsWidget::createPortalsTab()
{
    TQWidget *w = new TQWidget(m_tabs);
    TQVBoxLayout *l = new TQVBoxLayout(w, KDialog::marginHint(), KDialog::spacingHint());
    TQLabel *intro = new TQLabel(i18n("Permissions you granted when the application asked through a portal. "
                                      "Revoking one makes the application ask again."), w);
    intro->setAlignment(TQt::WordBreak);
    l->addWidget(intro);

    m_portalList = new TDEListView(w);
    m_portalList->addColumn(i18n("Permission"));
    m_portalList->addColumn(i18n("Decision"));
    m_portalList->setAllColumnsShowFocus(true);
    l->addWidget(m_portalList, 1);

    TQHBoxLayout *b = new TQHBoxLayout(l, KDialog::spacingHint());
    m_portalStatus = new TQLabel(w);
    m_portalStatus->setAlignment(TQt::WordBreak);
    b->addWidget(m_portalStatus, 1);
    m_portalRevoke = new KPushButton(SmallIconSet("edit-delete"), i18n("Re&voke"), w);
    m_portalRevokeAll = new KPushButton(SmallIconSet("edit-clear"), i18n("Revoke A&ll"), w);
    b->addWidget(m_portalRevoke);
    b->addWidget(m_portalRevokeAll);

    connect(m_portalRevoke, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotPortalRevoke()));
    connect(m_portalRevokeAll, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotPortalRevokeAll()));
    connect(m_portalList, TQ_SIGNAL(selectionChanged()), this, TQ_SLOT(slotListSelectionChanged()));
    return w;
}

TQWidget *PermissionsWidget::createStorageTab()
{
    TQWidget *w = new TQWidget(m_tabs);
    TQVBoxLayout *l = new TQVBoxLayout(w, KDialog::marginHint(), KDialog::spacingHint());

    TQGroupBox *g = new TQGroupBox(1, TQt::Horizontal, i18n("Personal Data"), w);
    l->addWidget(g);
    m_dataPath = new TQLabel(g);
    m_dataPath->setTextFormat(TQt::RichText);
    m_dataSize = new TQLabel(g);
    TQHBox *hb = new TQHBox(g);
    hb->setSpacing(KDialog::spacingHint());
    new TQWidget(hb);
    m_openData = new KPushButton(SmallIconSet("folder_open"), i18n("Ope&n Folder"), hb);
    m_deleteData = new KPushButton(SmallIconSet("edit-delete"), i18n("&Delete Data..."), hb);
    TQWhatsThis::add(m_deleteData, i18n("Delete the settings, caches and files the application keeps in your home folder, "
                                        "and the portal permissions you granted it."));

    g = new TQGroupBox(1, TQt::Horizontal, i18n("Updates"), w);
    l->addWidget(g);
    m_maskSystem = new TQCheckBox(i18n("Block updates of the s&ystem-wide installation"), g);
    m_maskUser = new TQCheckBox(i18n("Block updates of the &user installation"), g);
    TQLabel *maskInfo = new TQLabel(i18n("A blocked application stays at its current version until you allow updates again "
                                         "(this is what \"flatpak mask\" does)."), g);
    maskInfo->setAlignment(TQt::WordBreak);
    l->addStretch(1);

    connect(m_openData, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotOpenDataFolder()));
    connect(m_deleteData, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotDeleteData()));
    connect(m_maskSystem, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotMaskToggled()));
    connect(m_maskUser, TQ_SIGNAL(clicked()), this, TQ_SLOT(slotMaskToggled()));
    return w;
}

void PermissionsWidget::setAppListVisible(bool visible)
{
    if (visible) {
        m_appList->show();
    } else {
        m_appList->hide();
    }
}

void PermissionsWidget::load()
{
    const TQString previous = m_currentId;
    m_state.clear();
    m_itemId.clear();
    m_appList->clear();
    m_currentId = TQString::null;

    TQString error;
    FlatpakBackend::listInstalled(m_installed, error);

    // global entry first; TDEListView inserts at the top, so add in reverse
    TQMap<TQString, const FpInstalledRef *> apps;
    TQMap<TQString, TQString> sortKey;
    for (FpInstalledRefList::ConstIterator it = m_installed.begin(); it != m_installed.end(); ++it) {
        if ((*it).isApp && !apps.contains((*it).id)) {
            apps[(*it).id] = &(*it);
            sortKey[(*it).name.lower() + "\n" + (*it).id] = (*it).id;
        }
    }

    TQListViewItem *global = new TQListViewItem(m_appList, i18n("All Applications"));
    global->setPixmap(0, SmallIcon("application-x-executable", 22));
    m_itemId[global] = TQString::null;

    TQListViewItem *after = global;
    TQListViewItem *select = global;
    for (TQMap<TQString, TQString>::ConstIterator it = sortKey.begin(); it != sortKey.end(); ++it) {
        const FpInstalledRef *r = apps[it.data()];
        TQListViewItem *item = new TQListViewItem(m_appList, after, r->name);
        item->setPixmap(0, IconCache::load(FlatpakBackend::installedIconPath(*r), 22, "application-x-executable"));
        m_itemId[item] = r->id;
        after = item;
        if (r->id == previous) {
            select = item;
        }
    }

    m_appList->setSelected(select, true);
    m_appList->ensureItemVisible(select);
    slotAppSelected();
    emit changed(false);
}

void PermissionsWidget::selectApp(const TQString &appId)
{
    for (TQMap<TQListViewItem *, TQString>::ConstIterator it = m_itemId.begin(); it != m_itemId.end(); ++it) {
        if (it.data() == appId) {
            m_appList->setSelected(it.key(), true);
            m_appList->ensureItemVisible(it.key());
            return;
        }
    }
}

PermissionsWidget::AppState &PermissionsWidget::current()
{
    TQMap<TQString, AppState>::Iterator it = m_state.find(m_currentId);
    if (it != m_state.end()) {
        return *it;
    }
    AppState st;
    st.id = m_currentId;
    TQString error;
    if (m_currentId.isEmpty()) {
        st.name = i18n("All Applications");
    } else {
        for (FpInstalledRefList::ConstIterator r = m_installed.begin(); r != m_installed.end(); ++r) {
            if ((*r).isApp && (*r).id == m_currentId) {
                st.name = (*r).name;
                Overrides::loadAppPermissions(*r, st.metadata, error);
                break;
            }
        }
    }
    Overrides::loadOverride(m_currentId, st.layer, error);
    m_state[m_currentId] = st;
    return m_state[m_currentId];
}

FpPermissions PermissionsWidget::currentBase()
{
    if (isGlobal()) {
        return FpPermissions();
    }
    const AppState &app = current();
    const TQString saved = m_currentId;
    m_currentId = TQString::null;
    const FpPermissions globalLayer = current().layer;
    m_currentId = saved;
    return Overrides::merge(app.metadata, globalLayer);
}

TQStringList &PermissionsWidget::layerList(FpPermissions &p, const TQString &key)
{
    if (key == "shared") {
        return p.shared;
    }
    if (key == "sockets") {
        return p.sockets;
    }
    if (key == "devices") {
        return p.devices;
    }
    if (key == "features") {
        return p.features;
    }
    if (key == "persistent") {
        return p.persistent;
    }
    return p.filesystems;
}

void PermissionsWidget::slotAppSelected()
{
    TQListViewItem *item = m_appList->selectedItem();
    if (!item || !m_itemId.contains(item)) {
        return;
    }
    m_currentId = m_itemId[item];
    m_portalsLoaded = false;
    showCurrent();
}

void PermissionsWidget::showCurrent()
{
    AppState &st = current();
    if (isGlobal()) {
        m_header->setText(i18n("<b>All Applications</b><br><small>Changes here apply to every Flatpak application of your account.</small>"));
    } else {
        m_header->setText(TQString("<b>%1</b><br><small>%2</small>")
                              .arg(TQStyleSheet::escape(st.name)).arg(TQStyleSheet::escape(st.id)));
    }
    const TQString file = Overrides::overrideFile(m_currentId);
    m_fileLabel->setText(TQFile::exists(file) || st.modified
                             ? i18n("<small>Your changes are stored in %1</small>").arg(TQStyleSheet::escape(file))
                             : i18n("<small>No changes: the application gets what it requests.</small>"));
    m_resetButton->setEnabled(!st.layer.isEmpty());

    m_tabs->setTabEnabled(m_portalTab, !isGlobal());
    m_tabs->setTabEnabled(m_storageTab, !isGlobal());

    fillChecks();
    fillFiles();
    fillEnv();
    if (!isGlobal() && m_tabs->currentPage() == m_portalTab) {
        fillPortals();
    } else {
        m_portalList->clear();
    }
    fillStorage();
    slotListSelectionChanged();
}

void PermissionsWidget::fillChecks()
{
    m_filling = true;
    AppState &st = current();
    const FpPermissions base = currentBase();
    const FpPermissions effective = Overrides::merge(base, st.layer);

    for (TQMap<TQCheckBox *, Check>::ConstIterator it = m_checks.begin(); it != m_checks.end(); ++it) {
        TQCheckBox *cb = it.key();
        const Check &c = it.data();
        TQStringList &layer = layerList(st.layer, c.key);
        const bool inLayer = layer.contains(c.value) || layer.contains("!" + c.value);

        if (isGlobal()) {
            cb->setTristate(true);
            if (layer.contains(c.value)) {
                cb->setChecked(true);
            } else if (layer.contains("!" + c.value)) {
                cb->setChecked(false);
            } else {
                cb->setNoChange();
            }
        } else {
            cb->setTristate(false);
            FpPermissions e = effective;
            cb->setChecked(layerList(e, c.key).contains(c.value));
        }
        TQFont f = cb->font();
        f.setBold(inLayer);
        cb->setFont(f);
    }
    m_filling = false;
}

void PermissionsWidget::slotCheckToggled()
{
    if (m_filling) {
        return;
    }
    TQCheckBox *cb = const_cast<TQCheckBox *>(dynamic_cast<const TQCheckBox *>(sender()));
    if (!cb || !m_checks.contains(cb)) {
        return;
    }
    const Check c = m_checks[cb];
    AppState &st = current();
    TQStringList &layer = layerList(st.layer, c.key);
    layer = removeEntries(layer, c.value);

    if (isGlobal()) {
        if (cb->state() == TQButton::On) {
            layer.append(c.value);
        } else if (cb->state() == TQButton::Off) {
            layer.append("!" + c.value);
        }
    } else {
        FpPermissions base = currentBase();
        const bool baseHas = layerList(base, c.key).contains(c.value);
        if (cb->isChecked() != baseHas) {
            layer.append(cb->isChecked() ? c.value : "!" + c.value);
        }
    }
    markModified();
    fillChecks();
}

void PermissionsWidget::fillFiles()
{
    m_fsList->clear();
    AppState &st = current();
    const FpPermissions base = currentBase();
    const FpPermissions effective = Overrides::merge(base, st.layer);

    for (TQStringList::ConstIterator it = effective.filesystems.begin(); it != effective.filesystems.end(); ++it) {
        const TQString path = Overrides::filesystemPath(*it);
        TQString setBy;
        if (hasFilesystem(st.layer.filesystems, path)) {
            setBy = isGlobal() ? i18n("All applications") : i18n("You");
        } else if (hasFilesystem(st.metadata.filesystems, path)) {
            setBy = i18n("Application");
        } else {
            setBy = i18n("All applications");
        }
        TQListViewItem *item = new TQListViewItem(m_fsList, PermissionText::filesystem(path),
                                                  PermissionText::access(Overrides::filesystemMode(*it)), setBy);
        item->setText(3, path);
        item->setPixmap(0, SmallIcon(path == "host" || path == "host-os" || path == "host-etc"
                                         ? "drive-harddisk-mounted" : "folder"));
    }
    for (TQStringList::ConstIterator it = st.layer.filesystems.begin(); it != st.layer.filesystems.end(); ++it) {
        if ((*it).startsWith("!")) {
            const TQString path = Overrides::filesystemPath(*it);
            TQListViewItem *item = new TQListViewItem(m_fsList, PermissionText::filesystem(path), i18n("No access"),
                                                      isGlobal() ? i18n("All applications") : i18n("You"));
            item->setText(3, path);
            item->setPixmap(0, SmallIcon("process-stop"));
        }
    }
}

void PermissionsWidget::slotFsAdd()
{
    InputDialog dlg(this, i18n("Add Location"),
                    i18n("Choose a folder token or enter a path. Paths starting with ~/ are relative to your home folder."));
    TQStringList tokens;
    tokens << "xdg-download" << "xdg-documents" << "xdg-pictures" << "xdg-music" << "xdg-videos"
           << "xdg-desktop" << "home" << "host" << "host-os" << "host-etc" << "~/" << "/media" << "/run/media";
    KComboBox *path = dlg.addCombo(i18n("&Location:"), tokens, true);
    TQStringList modes;
    modes << i18n("Read/write") << i18n("Read-only") << i18n("Read/write, create if missing");
    KComboBox *mode = dlg.addCombo(i18n("&Access:"), modes, false);
    path->setFocus();
    if (dlg.exec() != TQDialog::Accepted) {
        return;
    }
    TQString p = path->currentText().stripWhiteSpace();
    while (p.length() > 1 && p.endsWith("/")) {
        p.truncate(p.length() - 1);
    }
    if (p.isEmpty() || p == "~") {
        if (p == "~") {
            p = "home";
        } else {
            return;
        }
    }
    if (p.contains(';')) {
        KMessageBox::sorry(this, i18n("A location must not contain a semicolon."));
        return;
    }
    const char *suffix[] = { "", ":ro", ":create" };
    AppState &st = current();
    st.layer.filesystems = removeFilesystem(st.layer.filesystems, p);
    st.layer.filesystems.append(p + suffix[mode->currentItem()]);
    markModified();
    fillFiles();
}

void PermissionsWidget::slotFsRemove()
{
    TQListViewItem *item = m_fsList->selectedItem();
    if (!item) {
        return;
    }
    const TQString path = item->text(3);
    AppState &st = current();
    if (hasFilesystem(st.layer.filesystems, path)) {
        // undo the user's own entry (an addition or a revocation)
        st.layer.filesystems = removeFilesystem(st.layer.filesystems, path);
    } else {
        // revoke what the application requests
        st.layer.filesystems.append("!" + path);
    }
    markModified();
    fillFiles();
}

void PermissionsWidget::fillEnv()
{
    m_envList->clear();
    m_busList->clear();
    AppState &st = current();
    const FpPermissions effective = Overrides::merge(currentBase(), st.layer);
    const TQString you = isGlobal() ? i18n("All applications") : i18n("You");

    TQMap<TQString, TQString>::ConstIterator it;
    for (it = effective.environment.begin(); it != effective.environment.end(); ++it) {
        TQString setBy = st.layer.environment.contains(it.key()) ? you
                         : (st.metadata.environment.contains(it.key()) ? i18n("Application") : i18n("All applications"));
        new TQListViewItem(m_envList, it.key(), it.data().isEmpty() ? i18n("(empty)") : it.data(), setBy);
    }

    const TQMap<TQString, TQString> *buses[] = { &effective.sessionBus, &effective.systemBus };
    const TQMap<TQString, TQString> *layers[] = { &st.layer.sessionBus, &st.layer.systemBus };
    const TQMap<TQString, TQString> *meta[] = { &st.metadata.sessionBus, &st.metadata.systemBus };
    const TQString busNames[] = { i18n("Session"), i18n("System") };
    for (int b = 0; b < 2; ++b) {
        for (it = buses[b]->begin(); it != buses[b]->end(); ++it) {
            TQString setBy = layers[b]->contains(it.key()) ? you
                             : (meta[b]->contains(it.key()) ? i18n("Application") : i18n("All applications"));
            TQListViewItem *i = new TQListViewItem(m_busList, it.key(), busNames[b], policyText(it.data()), setBy);
            i->setText(4, TQString::number(b));
        }
        // revocations of requested names
        for (it = layers[b]->begin(); it != layers[b]->end(); ++it) {
            if (it.data() == "none") {
                TQListViewItem *i = new TQListViewItem(m_busList, it.key(), busNames[b], policyText("none"), you);
                i->setText(4, TQString::number(b));
            }
        }
    }
}

void PermissionsWidget::slotEnvAdd()
{
    InputDialog dlg(this, i18n("Set Environment Variable"), TQString::null);
    KLineEdit *name = dlg.addLine(i18n("&Name:"));
    KLineEdit *value = dlg.addLine(i18n("&Value:"));
    TQListViewItem *sel = m_envList->selectedItem();
    if (sel) {
        name->setText(sel->text(0));
        AppState &st = current();
        FpPermissions eff = Overrides::merge(currentBase(), st.layer);
        value->setText(eff.environment[sel->text(0)]);
    }
    name->setFocus();
    if (dlg.exec() != TQDialog::Accepted) {
        return;
    }
    const TQString n = name->text().stripWhiteSpace();
    if (n.isEmpty() || n.contains('=') || n.contains(' ')) {
        KMessageBox::sorry(this, i18n("\"%1\" is not a valid variable name.").arg(n));
        return;
    }
    current().layer.environment[n] = value->text();
    markModified();
    fillEnv();
}

void PermissionsWidget::slotEnvRemove()
{
    TQListViewItem *item = m_envList->selectedItem();
    if (!item) {
        return;
    }
    AppState &st = current();
    const TQString n = item->text(0);
    if (st.layer.environment.contains(n)) {
        st.layer.environment.remove(n);
    } else {
        st.layer.environment[n] = TQString::null;   // override the requested value with an empty one
    }
    markModified();
    fillEnv();
}

void PermissionsWidget::slotBusAdd()
{
    InputDialog dlg(this, i18n("Allow D-Bus Service"),
                    i18n("Enter a well-known bus name, for example org.freedesktop.Notifications or org.kde.*"));
    KLineEdit *name = dlg.addLine(i18n("&Name:"));
    TQStringList buses;
    buses << i18n("Session bus") << i18n("System bus");
    KComboBox *bus = dlg.addCombo(i18n("&Bus:"), buses, false);
    TQStringList policies;
    policies << i18n("Talk") << i18n("Own") << i18n("See") << i18n("No access");
    KComboBox *policy = dlg.addCombo(i18n("&Access:"), policies, false);
    name->setFocus();
    if (dlg.exec() != TQDialog::Accepted) {
        return;
    }
    const TQString n = name->text().stripWhiteSpace();
    if (n.isEmpty() || n.contains(' ') || n.contains('=')) {
        return;
    }
    const char *values[] = { "talk", "own", "see", "none" };
    AppState &st = current();
    (bus->currentItem() == 0 ? st.layer.sessionBus : st.layer.systemBus)[n] = values[policy->currentItem()];
    markModified();
    fillEnv();
}

void PermissionsWidget::slotBusRemove()
{
    TQListViewItem *item = m_busList->selectedItem();
    if (!item) {
        return;
    }
    AppState &st = current();
    TQMap<TQString, TQString> &layer = item->text(4) == "1" ? st.layer.systemBus : st.layer.sessionBus;
    const TQString n = item->text(0);
    if (layer.contains(n)) {
        layer.remove(n);
    } else {
        layer[n] = "none";
    }
    markModified();
    fillEnv();
}

void PermissionsWidget::slotListSelectionChanged()
{
    m_fsRemove->setEnabled(m_fsList->selectedItem() != 0);
    m_envRemove->setEnabled(m_envList->selectedItem() != 0);
    m_busRemove->setEnabled(m_busList->selectedItem() != 0);
    m_portalRevoke->setEnabled(m_portalList->selectedItem() != 0);
    m_portalRevokeAll->setEnabled(m_portalList->firstChild() != 0);
}

void PermissionsWidget::slotTabChanged(TQWidget *tab)
{
    if (tab == m_portalTab && !m_portalsLoaded && !isGlobal()) {
        fillPortals();
    } else if (tab == m_storageTab && !isGlobal()) {
        fillStorage();   // the data folder may have changed since
    }
}

void PermissionsWidget::fillPortals()
{
    m_portalList->clear();
    m_portalPerms.clear();
    m_portalsLoaded = true;
    TQString error;
    if (!PortalPermissions::list(m_currentId, m_portalPerms, error)) {
        m_portalStatus->setText(i18n("The permission store is not available: %1").arg(error));
        slotListSelectionChanged();
        return;
    }
    int idx = 0;
    for (FpPortalPermissionList::ConstIterator it = m_portalPerms.begin(); it != m_portalPerms.end(); ++it, ++idx) {
        TQListViewItem *item = new TQListViewItem(m_portalList, PortalPermissions::describe(*it),
                                                  (*it).values.join(", "));
        item->setText(2, TQString::number(idx));
    }
    m_portalStatus->setText(m_portalPerms.isEmpty() ? i18n("No portal permissions are stored for this application.")
                                                    : TQString::null);
    slotListSelectionChanged();
}

void PermissionsWidget::slotPortalRevoke()
{
    TQListViewItem *item = m_portalList->selectedItem();
    if (!item) {
        return;
    }
    const int idx = item->text(2).toInt();
    if (idx < 0 || idx >= (int)m_portalPerms.count()) {
        return;
    }
    TQString error;
    if (!PortalPermissions::revoke(m_portalPerms[idx], m_currentId, error)) {
        KMessageBox::error(this, i18n("Could not revoke the permission:\n%1").arg(error));
    }
    fillPortals();
}

void PermissionsWidget::slotPortalRevokeAll()
{
    if (KMessageBox::warningContinueCancel(this,
            i18n("Revoke all portal permissions of %1?").arg(current().name),
            i18n("Revoke Permissions"), KGuiItem(i18n("Revoke &All"), "edit-clear")) != KMessageBox::Continue) {
        return;
    }
    TQString error;
    if (!PortalPermissions::revokeAll(m_currentId, error)) {
        KMessageBox::error(this, i18n("Could not revoke the permissions:\n%1").arg(error));
    }
    fillPortals();
}

void PermissionsWidget::fillStorage()
{
    if (m_sizeJob) {
        m_sizeJob->kill();
        m_sizeJob = 0;
    }
    if (isGlobal()) {
        m_dataPath->clear();
        m_dataSize->clear();
        return;
    }
    const TQString dir = FlatpakBackend::appDataDir(m_currentId);
    const bool exists = TQDir(dir).exists();
    m_dataPath->setText(i18n("Folder: <tt>%1</tt>").arg(TQStyleSheet::escape(dir)));
    m_openData->setEnabled(exists);
    m_deleteData->setEnabled(exists);
    if (exists) {
        m_dataSize->setText(i18n("Size: calculating..."));
        KDirSize *job = KDirSize::dirSizeJob(KURL::fromPathOrURL(dir));
        connect(job, TQ_SIGNAL(result(TDEIO::Job *)), this, TQ_SLOT(slotDirSizeResult(TDEIO::Job *)));
        m_sizeJob = job;
    } else {
        m_dataSize->setText(i18n("The application has not stored any data yet."));
    }

    // update masks
    TQString refs[2];
    for (FpInstalledRefList::ConstIterator it = m_installed.begin(); it != m_installed.end(); ++it) {
        if ((*it).isApp && (*it).id == m_currentId) {
            refs[(*it).scope] = (*it).ref;
        }
    }
    TQCheckBox *boxes[2];
    boxes[FpScopeUser] = m_maskUser;
    boxes[FpScopeSystem] = m_maskSystem;
    m_filling = true;
    for (int s = 0; s < 2; ++s) {
        boxes[s]->setEnabled(!refs[s].isEmpty());
        boxes[s]->setChecked(!refs[s].isEmpty() && AppActions::isMasked(s, refs[s]));
        m_maskRef[s] = refs[s];
    }
    m_filling = false;
}

void PermissionsWidget::slotDirSizeResult(TDEIO::Job *job)
{
    if (job != m_sizeJob) {
        return;
    }
    m_sizeJob = 0;
    if (!job->error()) {
        m_dataSize->setText(i18n("Size: %1").arg(TDEIO::convertSize(static_cast<KDirSize *>(job)->totalSize())));
    } else {
        m_dataSize->setText(job->errorString());
    }
}

void PermissionsWidget::slotOpenDataFolder()
{
    new KRun(KURL::fromPathOrURL(FlatpakBackend::appDataDir(m_currentId)));
}

void PermissionsWidget::slotDeleteData()
{
    if (AppActions::deleteData(this, m_currentId, current().name)) {
        m_portalsLoaded = false;
        fillStorage();
        if (m_tabs->currentPage() == m_portalTab) {
            fillPortals();
        }
    }
}

void PermissionsWidget::slotMaskToggled()
{
    if (m_filling) {
        return;
    }
    TQCheckBox *cb = const_cast<TQCheckBox *>(dynamic_cast<const TQCheckBox *>(sender()));
    if (!cb) {
        return;
    }
    const int scope = (cb == m_maskUser) ? FpScopeUser : FpScopeSystem;
    const TQString ref = m_maskRef[scope];
    if (!ref.isEmpty()) {
        AppActions::setMasked(this, scope, ref, cb->isChecked());
        emit installationChanged();
    }
    fillStorage();
}

void PermissionsWidget::markModified()
{
    AppState &st = current();
    st.modified = true;
    m_resetButton->setEnabled(!st.layer.isEmpty());
    m_fileLabel->setText(i18n("<small>Your changes are stored in %1</small>")
                             .arg(TQStyleSheet::escape(Overrides::overrideFile(m_currentId))));
    if (isGlobal()) {
        // the global layer changes what every application gets
        fillFiles();
        fillEnv();
    }
    emit changed(true);
}

void PermissionsWidget::resetCurrent()
{
    AppState &st = current();
    if (st.layer.isEmpty()) {
        return;
    }
    st.layer = FpPermissions();
    markModified();
    showCurrent();
}

bool PermissionsWidget::isModified() const
{
    for (TQMap<TQString, AppState>::ConstIterator it = m_state.begin(); it != m_state.end(); ++it) {
        if ((*it).modified) {
            return true;
        }
    }
    return false;
}

bool PermissionsWidget::save(TQString &error)
{
    bool ok = true;
    for (TQMap<TQString, AppState>::Iterator it = m_state.begin(); it != m_state.end(); ++it) {
        if (!(*it).modified) {
            continue;
        }
        TQString err;
        if (Overrides::saveOverride((*it).id, (*it).layer, err)) {
            (*it).modified = false;
        } else {
            error += ((*it).id.isEmpty() ? TQString("global") : (*it).id) + ": " + err + "\n";
            ok = false;
        }
    }
    if (!m_state.isEmpty()) {
        showCurrent();
    }
    emit changed(isModified());
    return ok;
}

// ---------------------------------------------------------------------------
// PermissionsDialog
// ---------------------------------------------------------------------------

PermissionsDialog::PermissionsDialog(TQWidget *parent, const TQString &appId)
    : KDialogBase(parent, "permissionsdialog", true, i18n("Application Permissions"),
                  Ok | Apply | Cancel, Ok, true),
      m_installationChanged(false)
{
    m_widget = new PermissionsWidget(this);
    setMainWidget(m_widget);
    m_widget->load();
    if (!appId.isNull()) {
        m_widget->selectApp(appId);
    }
    enableButtonApply(false);
    connect(m_widget, TQ_SIGNAL(changed(bool)), this, TQ_SLOT(slotChanged(bool)));
    connect(m_widget, TQ_SIGNAL(installationChanged()), this, TQ_SLOT(slotInstallationChanged()));
    resize(820, 600);
}

bool PermissionsDialog::edit(TQWidget *parent, const TQString &appId)
{
    PermissionsDialog dlg(parent, appId);
    dlg.exec();
    return dlg.installationChanged();
}

void PermissionsDialog::slotChanged(bool modified)
{
    enableButtonApply(modified);
}

void PermissionsDialog::slotInstallationChanged()
{
    m_installationChanged = true;
}

bool PermissionsDialog::apply()
{
    TQString error;
    if (!m_widget->save(error)) {
        KMessageBox::error(this, i18n("Could not save the permissions:\n%1").arg(error));
        return false;
    }
    enableButtonApply(false);
    return true;
}

void PermissionsDialog::slotApply()
{
    apply();
}

void PermissionsDialog::slotOk()
{
    if (!m_widget->isModified() || apply()) {
        accept();
    }
}

#include "permissionswidget.moc"
