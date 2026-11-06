/*
 * tdeflatpak - dialog for adding a remote.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_ADDREMOTEDIALOG_H
#define TDEFLATPAK_ADDREMOTEDIALOG_H

#include <kdialogbase.h>

class TQCheckBox;
class TQComboBox;
class KLineEdit;
class KURLRequester;

class AddRemoteDialog : public KDialogBase
{
    TQ_OBJECT

public:
    AddRemoteDialog(TQWidget *parent = 0, const TQString &location = TQString::null);

    TQString remoteName() const;
    TQString location() const;
    int scope() const;
    bool gpgVerify() const;

    /** true if location() points to a .flatpakrepo file */
    bool isRepoFile() const;

protected slots:
    virtual void slotOk();

private slots:
    void slotLocationChanged(const TQString &);

private:
    KLineEdit *m_name;
    KURLRequester *m_location;
    TQComboBox *m_scope;
    TQCheckBox *m_gpg;
};

/**
 * Add a remote via a FlatpakJob, showing an OperationDialog.
 * Downloads @p location first when it is a .flatpakrepo URL.
 */
bool addRemoteInteractive(TQWidget *parent, int scope, const TQString &name,
                          const TQString &location, bool isRepoFile, bool gpgVerify);

#endif
