/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QMenu>

#include <memory>

#include "tyqt/log_dialog.hpp"

using namespace std;

LogDialog::LogDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    connect(closeButton, &QPushButton::clicked, this, &LogDialog::close);
    connect(clearButton, &QPushButton::clicked, this, &LogDialog::clearAll);
    connect(errorLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogDialog::showLogContextMenu);
    connect(fullLogText, &QPlainTextEdit::customContextMenuRequested, this,
            &LogDialog::showLogContextMenu);
}

void LogDialog::appendError(const QString &msg)
{
    errorLogText->appendPlainText(msg);
    fullLogText->appendPlainText(msg);
}

void LogDialog::appendDebug(const QString &msg)
{
    fullLogText->appendPlainText(msg);
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
