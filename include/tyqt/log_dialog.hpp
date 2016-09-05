/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef LOG_DIALOG_HH
#define LOG_DIALOG_HH

#include "ui_log_dialog.h"

class LogDialog: public QDialog, private Ui::LogDialog {
    Q_OBJECT

public:
    LogDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

public slots:
    void appendError(const QString &msg, const QString &ctx);
    void appendDebug(const QString &msg, const QString &ctx);
    void clearAll();

private:
    void keyPressEvent(QKeyEvent *e);

private slots:
    void showLogContextMenu(const QPoint &pos);
};

#endif
