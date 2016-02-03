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
}

void LogWindow::appendLog(const QString &log)
{
    logText->appendPlainText(log);
}

void LogWindow::keyPressEvent(QKeyEvent *e)
{
    if (!e->modifiers() && e->key() == Qt::Key_Escape)
        close();
}

void LogWindow::on_logText_customContextMenuRequested(const QPoint &pos)
{
    auto menu = logText->createStandardContextMenu();

    menu->addAction(tr("Clear"), logText, SLOT(clear()));
    menu->exec(logText->viewport()->mapToGlobal(pos));
}
