/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>

#include "hs/common.h"
#include "ty/common.h"
#include "tyqt/log_dialog.hpp"
#include "tyqt/monitor.hpp"
#include "updater_window.hpp"
#include "upty.hpp"

#ifdef QT_STATIC
    #include <QtPlugin>
    #if defined(_WIN32)
        Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    #elif defined(__APPLE__)
        Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif
#endif

using namespace std;

UpTy::UpTy(int &argc, char *argv[])
    : QApplication(argc, argv)
{
    setOrganizationName("ty");
    setApplicationName(TY_CONFIG_UPTY_NAME);
    setApplicationVersion(ty_version_string());

    ty_message_redirect([](ty_task *task, ty_message_type type, const void *data, void *udata) {
        ty_message_default_handler(task, type, data, udata);

        if (type == TY_MESSAGE_LOG) {
            auto print = static_cast<const ty_log_message *>(data);
            if (print->level <= TY_LOG_WARNING) {
                upTy->reportError(print->msg);
            } else {
                upTy->reportDebug(print->msg);
            }
        }
    }, nullptr);

    log_dialog_ = unique_ptr<LogDialog>(new LogDialog());
    log_dialog_->setAttribute(Qt::WA_QuitOnClose, false);
    log_dialog_->setWindowIcon(QIcon(":/upty"));
    connect(this, &UpTy::globalError, log_dialog_.get(), &LogDialog::appendError);
    connect(this, &UpTy::globalDebug, log_dialog_.get(), &LogDialog::appendDebug);
}

UpTy::~UpTy()
{
    ty_message_redirect(ty_message_default_handler, nullptr);
}

void UpTy::showLogWindow()
{
    log_dialog_->show();
}

void UpTy::reportError(const QString &msg)
{
    emit globalError(msg);
}

void UpTy::reportDebug(const QString &msg)
{
    emit globalDebug(msg);
}

int UpTy::exec()
{
    return upTy->run();
}

int UpTy::run()
{
    monitor_.reset(new Monitor());
    monitor_->setSerialByDefault(false);
    if (!monitor_->start()) {
        QMessageBox::critical(nullptr, tr("%1 (error)").arg(applicationName()),
                              ty_error_last_message());
        return EXIT_FAILURE;
    }

    UpdaterWindow win;
    win.show();

    return QApplication::exec();
}

int main(int argc, char *argv[])
{
    hs_log_set_handler(ty_libhs_log_handler, NULL);

    // TODO: move to libtyqt function
    Q_INIT_RESOURCE(libtyqt);
    qRegisterMetaType<ty_log_level>("ty_log_level");
    qRegisterMetaType<std::shared_ptr<void>>("std::shared_ptr<void>");
    qRegisterMetaType<ty_descriptor>("ty_descriptor");
    qRegisterMetaType<uint64_t>("uint64_t");

    UpTy app(argc, argv);
    return app.exec();
}
