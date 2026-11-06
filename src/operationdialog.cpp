/*
 * tdeflatpak - modal dialog that runs a FlatpakJob and shows its progress.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "operationdialog.h"
#include "flatpakjob.h"

#include <tqlabel.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <tdeapplication.h>
#include <tdelocale.h>
#include <kprogress.h>
#include <kstdguiitem.h>
#include <ktextedit.h>
#include <kiconloader.h>

bool OperationDialog::run(TQWidget *parent, FlatpakJob *job, const TQString &caption,
                          const TQString &description, bool autoClose)
{
    OperationDialog dlg(parent, job, caption, description, autoClose);
    dlg.exec();
    return dlg.succeeded();
}

OperationDialog::OperationDialog(TQWidget *parent, FlatpakJob *job, const TQString &caption,
                                 const TQString &description, bool autoClose)
    : KDialogBase(parent, "operationdialog", true, caption,
                  KDialogBase::Cancel, KDialogBase::Cancel, true),
      m_job(job),
      m_started(false),
      m_finished(false),
      m_success(false),
      m_autoClose(autoClose)
{
    TQWidget *page = new TQWidget(this);
    setMainWidget(page);

    TQVBoxLayout *layout = new TQVBoxLayout(page, 0, spacingHint());

    TQHBoxLayout *top = new TQHBoxLayout(layout, spacingHint());
    TQLabel *icon = new TQLabel(page);
    icon->setPixmap(DesktopIcon("system-software-update", 48));
    top->addWidget(icon, 0, TQt::AlignTop);
    m_title = new TQLabel(description, page);
    m_title->setTextFormat(TQt::RichText);
    m_title->setAlignment(TQt::AlignVCenter | TQt::WordBreak);
    top->addWidget(m_title, 1);

    m_status = new TQLabel(i18n("Starting..."), page);
    layout->addWidget(m_status);

    m_progress = new KProgress(100, page);
    layout->addWidget(m_progress);

    TQLabel *detailsLabel = new TQLabel(i18n("Details:"), page);
    layout->addWidget(detailsLabel);

    m_log = new KTextEdit(page);
    m_log->setReadOnly(true);
    m_log->setTextFormat(TQt::LogText);
    m_log->setMinimumSize(480, 160);
    layout->addWidget(m_log, 1);

    resize(560, 360);
}

OperationDialog::~OperationDialog()
{
    if (m_job) {
        m_job->cancel();
        m_job->wait();
        delete m_job;
    }
}

void OperationDialog::showEvent(TQShowEvent *ev)
{
    KDialogBase::showEvent(ev);
    if (!m_started) {
        m_started = true;
        TQTimer::singleShot(0, this, TQ_SLOT(startJob()));
    }
}

void OperationDialog::startJob()
{
    if (!m_job) {
        return;
    }
    m_job->setReceiver(this);
    m_job->start();
}

TQString OperationDialog::operationText(int opType, const TQString &ref)
{
    switch (opType) {
    case FlatpakJobEvent::OpInstall:
        return i18n("Installing %1").arg(ref);
    case FlatpakJobEvent::OpUpdate:
        return i18n("Updating %1").arg(ref);
    case FlatpakJobEvent::OpUninstall:
        return i18n("Uninstalling %1").arg(ref);
    case FlatpakJobEvent::OpInstallBundle:
        return i18n("Installing bundle %1").arg(ref);
    default:
        return ref;
    }
}

void OperationDialog::appendLog(const TQString &html)
{
    m_log->append(html);
}

void OperationDialog::customEvent(TQCustomEvent *e)
{
    switch ((int)e->type()) {
    case FlatpakJobEvent::Progress: {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        if (ev->opCount > 0 && ev->opIndex > 0) {
            TQString s = i18n("Step %1 of %2").arg(ev->opIndex).arg(ev->opCount);
            if (!ev->text.isEmpty()) {
                s += " - " + ev->text;
            }
            m_status->setText(s);
        } else if (!ev->text.isEmpty()) {
            m_status->setText(ev->text);
        }
        if (ev->percent >= 0) {
            m_progress->setTotalSteps(100);
            m_progress->setProgress(ev->percent);
        }
        break;
    }
    case FlatpakJobEvent::Log: {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        const TQString ref = TQStyleSheet::escape(ev->ref);
        const TQString text = TQStyleSheet::escape(ev->text);
        switch (ev->logKind) {
        case FlatpakJobEvent::OperationStarted:
            appendLog("<b>" + TQStyleSheet::escape(operationText(ev->opType, ev->ref)) + "</b>");
            m_status->setText(operationText(ev->opType, ev->ref));
            break;
        case FlatpakJobEvent::OperationDone:
            appendLog(i18n("Done: %1").arg(ref));
            break;
        case FlatpakJobEvent::Warning:
            appendLog("<font color=\"#a06000\">" + i18n("Warning: %1").arg(ref.isEmpty() ? text : ref + ": " + text) + "</font>");
            break;
        case FlatpakJobEvent::Error:
            appendLog("<font color=\"#c00000\">" + i18n("Error: %1").arg(ref.isEmpty() ? text : ref + ": " + text) + "</font>");
            break;
        default:
            appendLog(ref.isEmpty() ? text : ref + ": " + text);
            break;
        }
        break;
    }
    case FlatpakJobEvent::Finished: {
        FlatpakJobEvent *ev = static_cast<FlatpakJobEvent *>(e);
        m_finished = true;
        m_success = ev->success;
        if (m_job) {
            m_job->wait();
        }
        if (ev->success) {
            m_progress->setProgress(100);
            m_status->setText(i18n("Finished successfully."));
            appendLog("<b>" + i18n("Finished successfully.") + "</b>");
        } else if (ev->cancelled) {
            m_status->setText(i18n("Cancelled."));
            appendLog("<b>" + i18n("Cancelled.") + "</b>");
        } else {
            m_status->setText(i18n("Failed."));
            appendLog("<font color=\"#c00000\"><b>" + i18n("Failed: %1").arg(TQStyleSheet::escape(ev->text)) + "</b></font>");
        }
        // the Cancel button becomes the Close button
        setButtonCancel(KStdGuiItem::close());
        enableButtonCancel(true);
        if (TQPushButton *b = actionButton(KDialogBase::Cancel)) {
            b->setDefault(true);
            b->setFocus();
        }
        if (ev->success && m_autoClose) {
            accept();
        }
        break;
    }
    default:
        KDialogBase::customEvent(e);
    }
}

void OperationDialog::slotCancel()
{
    if (m_finished || !m_job) {
        KDialogBase::slotCancel();
        return;
    }
    m_status->setText(i18n("Cancelling..."));
    enableButtonCancel(false);
    m_job->cancel();
}

void OperationDialog::closeEvent(TQCloseEvent *ev)
{
    if (!m_finished && m_job && m_started) {
        slotCancel();
        ev->ignore();
        return;
    }
    KDialogBase::closeEvent(ev);
}

#include "operationdialog.moc"
