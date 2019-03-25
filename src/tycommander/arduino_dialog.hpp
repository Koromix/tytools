/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef ARDUINO_DIALOG_HH
#define ARDUINO_DIALOG_HH

#include "arduino_install.hpp"
#include "ui_arduino_dialog.h"

class ArduinoDialog: public QDialog, private Ui::ArduinoDialog {
    Q_OBJECT

    ArduinoInstallation install_;
    bool background_process_ = false;

public:
    ArduinoDialog(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    void keyPressEvent(QKeyEvent *ev) override;

private slots:
    void refresh();
    void addLog(const QString &msg);
    void addError(const QString &msg);

    void browseForArduino();

    void integrate();
    void restore();

private:
    void appendMessage(const QString &msg, const QTextCharFormat &fmt = QTextCharFormat());

    void executeAsRoot(const QString &command);
#ifdef _WIN32
    void installWithUAC(const QString &command);
#endif
};

#endif
