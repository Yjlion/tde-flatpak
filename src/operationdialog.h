/*
 * tdeflatpak - modal dialog that runs a FlatpakJob and shows its progress.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_OPERATIONDIALOG_H
#define TDEFLATPAK_OPERATIONDIALOG_H

#include <kdialogbase.h>

class TQLabel;
class KProgress;
class KTextEdit;
class FlatpakJob;

class OperationDialog : public KDialogBase
{
    TQ_OBJECT

public:
    /**
     * Run @p job (a FlatpakJob created with a null receiver) in a modal
     * dialog.  The dialog takes ownership of the job.
     *
     * @param autoClose close the dialog automatically on success
     * @return true if the job succeeded
     */
    static bool run(TQWidget *parent, FlatpakJob *job, const TQString &caption,
                    const TQString &description, bool autoClose = false);

    OperationDialog(TQWidget *parent, FlatpakJob *job, const TQString &caption,
                    const TQString &description, bool autoClose);
    ~OperationDialog();

    bool succeeded() const { return m_success; }

    /** Human readable text for an operation type */
    static TQString operationText(int opType, const TQString &ref);

protected:
    virtual void customEvent(TQCustomEvent *ev);
    virtual void closeEvent(TQCloseEvent *ev);
    virtual void showEvent(TQShowEvent *ev);

protected slots:
    virtual void slotCancel();
    void startJob();

private:
    void appendLog(const TQString &html);

    FlatpakJob *m_job;
    TQLabel *m_title;
    TQLabel *m_status;
    KProgress *m_progress;
    KTextEdit *m_log;
    bool m_started;
    bool m_finished;
    bool m_success;
    bool m_autoClose;
};

#endif
