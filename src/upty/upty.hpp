/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef UPTY_HH
#define UPTY_HH

#include <QApplication>

#include <memory>

#define upTy (UpTy::instance())

class LogDialog;
class Monitor;

class UpTy: public QApplication {
    Q_OBJECT

    std::unique_ptr<Monitor> monitor_;
    std::unique_ptr<LogDialog> log_dialog_;

public:
    UpTy(int &argc, char *argv[]);
    virtual ~UpTy();

    static int exec();

    static UpTy *instance() { return qobject_cast<UpTy *>(QCoreApplication::instance()); }
    Monitor *monitor() { return monitor_.get(); }

    int run();

public slots:
    void showLogWindow();

    void reportError(const QString &msg);
    void reportDebug(const QString &msg);

signals:
    void globalError(const QString &msg);
    void globalDebug(const QString &msg);
};

#endif
