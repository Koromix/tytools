/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QMenu>

#include <memory>

#include "log_dialog.hpp"

using namespace std;

LogDialog::LogDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);
    setWindowTitle(tr("%1 Log").arg(QApplication::applicationName()));

    connect(closeButton, &QPushButton::clicked, this, &LogDialog::close);
    connect(clearButton, &QPushButton::clicked, this, &LogDialog::clearAll);
    connect(errorLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogDialog::showLogContextMenu);
    connect(fullLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogDialog::showLogContextMenu);
}

void LogDialog::appendError(const QString &msg, const QString &ctx)
{
    if (!ctx.isEmpty()) {
        auto full_msg = QString("[%1] %2").arg(ctx, msg);
        errorLogText->appendPlainText(full_msg);
        fullLogText->appendPlainText(full_msg);
    } else {
        errorLogText->appendPlainText(msg);
        fullLogText->appendPlainText(msg);
    }
}

void LogDialog::appendDebug(const QString &msg, const QString &ctx)
{
    if (!ctx.isEmpty()) {
        auto full_msg = QString("[%1] %2").arg(ctx, msg);
        fullLogText->appendPlainText(full_msg);
    } else {
        fullLogText->appendPlainText(msg);
    }
}

void LogDialog::clearAll()
{
    errorLogText->clear();
    fullLogText->clear();
}

void LogDialog::keyPressEvent(QKeyEvent *e)
{
    if (!e->modifiers() && e->key() == Qt::Key_Escape)
        close();
}

void LogDialog::showLogContextMenu(const QPoint &pos)
{
    auto edit = qobject_cast<QPlainTextEdit *>(sender());

    unique_ptr<QMenu> menu(edit->createStandardContextMenu());
    menu->addAction(tr("Clear"), edit, SLOT(clear()));
    menu->exec(edit->viewport()->mapToGlobal(pos));
}
