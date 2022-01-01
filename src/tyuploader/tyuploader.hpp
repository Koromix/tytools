/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TYUPLOADER_HH
#define TYUPLOADER_HH

#include <QApplication>

#include <memory>

#define tyUploader (TyUploader::instance())

class LogDialog;
class Monitor;

class TyUploader: public QApplication {
    Q_OBJECT

    std::unique_ptr<Monitor> monitor_;
    std::unique_ptr<LogDialog> log_dialog_;

public:
    TyUploader(int &argc, char *argv[]);
    virtual ~TyUploader();

    static int exec();

    static TyUploader *instance() { return qobject_cast<TyUploader *>(QCoreApplication::instance()); }
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
