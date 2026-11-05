/*
 * tdeflatpak - "Remotes" page.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_REMOTESPAGE_H
#define TDEFLATPAK_REMOTESPAGE_H

#include <tqwidget.h>
#include <tqmap.h>

#include "flatpaktypes.h"

class TQListViewItem;
class TDEListView;
class KPushButton;

class RemotesPage : public TQWidget
{
    TQ_OBJECT

public:
    RemotesPage(TQWidget *parent = 0, const char *name = 0);

    const FpRemoteList &remotes() const { return m_remotes; }

public slots:
    void refresh();
    void addRemote(const TQString &location = TQString::null);
    void addFlathub();
    void removeUnused();

signals:
    void modified();

private slots:
    void slotAdd();
    void slotRemove();
    void slotToggleEnabled();
    void slotSelectionChanged();

private:
    const FpRemote *selectedRemote() const;

    TDEListView *m_list;
    KPushButton *m_addButton;
    KPushButton *m_flathubButton;
    KPushButton *m_toggleButton;
    KPushButton *m_removeButton;
    KPushButton *m_unusedButton;
    FpRemoteList m_remotes;
    TQMap<TQListViewItem *, int> m_itemIndex;
};

#endif
