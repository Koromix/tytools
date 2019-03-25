/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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
