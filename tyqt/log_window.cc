/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QMenu>

#include "log_window.hh"

LogWindow::LogWindow(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    setupUi(this);

    connect(closeButton, &QPushButton::clicked, this, &LogWindow::close);
    connect(clearButton, &QPushButton::clicked, this, &LogWindow::clearAll);
    connect(errorLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogWindow::showLogContextMenu);
    connect(fullLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogWindow::showLogContextMenu);
}

void LogWindow::appendError(const QString &msg)
{
    errorLogText->appendPlainText(msg);
    fullLogText->appendPlainText(msg);
}

void LogWindow::appendDebug(const QString &msg)
{
    fullLogText->appendPlainText(msg);
}

void LogWindow::clearAll()
{
    errorLogText->clear();
    fullLogText->clear();
}

void LogWindow::keyPressEvent(QKeyEvent *e)
{
    if (!e->modifiers() && e->key() == Qt::Key_Escape)
        close();
}

void LogWindow::showLogContextMenu(const QPoint &pos)
{
    auto edit = qobject_cast<QPlainTextEdit *>(sender());
    auto menu = edit->createStandardContextMenu();
    if (menu) {
        menu->addAction(tr("Clear"), edit, SLOT(clear()));
        menu->exec(edit->viewport()->mapToGlobal(pos));
    }
}
