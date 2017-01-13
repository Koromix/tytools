/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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

    void reportError(const QString &msg, const QString &ctx = QString());
    void reportDebug(const QString &msg, const QString &ctx = QString());

signals:
    void globalError(const QString &msg, const QString &ctx);
    void globalDebug(const QString &msg, const QString &ctx);
};

#endif
