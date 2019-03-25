/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>

#include "../libhs/common.h"
#include "../libty/common.h"
#include "../libty/class.h"
#include "../tycommander/log_dialog.hpp"
#include "../tycommander/monitor.hpp"
#include "tyupdater.hpp"
#include "updater_window.hpp"

#ifdef QT_STATIC
    #include <QtPlugin>
    #if defined(_WIN32)
        Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    #elif defined(__APPLE__)
        Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    #endif
#endif

using namespace std;

TyUpdater::TyUpdater(int &argc, char *argv[])
    : QApplication(argc, argv)
{
    setOrganizationName("ty");
    setApplicationName(TY_CONFIG_TYUPDATER_NAME);
    setApplicationVersion(ty_version_string());

    ty_message_redirect([](const ty_message_data *msg, void *) {
        ty_message_default_handler(msg, nullptr);

        if (msg->type == TY_MESSAGE_LOG) {
            if (msg->u.log.level <= TY_LOG_WARNING) {
                tyUpdater->reportError(msg->u.log.msg, msg->ctx);
            } else {
                tyUpdater->reportDebug(msg->u.log.msg, msg->ctx);
            }
        }
    }, nullptr);

    log_dialog_ = unique_ptr<LogDialog>(new LogDialog());
    log_dialog_->setAttribute(Qt::WA_QuitOnClose, false);
    log_dialog_->setWindowIcon(QIcon(":/tyupdater"));
    connect(this, &TyUpdater::globalError, log_dialog_.get(), &LogDialog::appendError);
    connect(this, &TyUpdater::globalDebug, log_dialog_.get(), &LogDialog::appendDebug);
}

TyUpdater::~TyUpdater()
{
    ty_message_redirect(ty_message_default_handler, nullptr);
}

void TyUpdater::showLogWindow()
{
    log_dialog_->show();
}

void TyUpdater::reportError(const QString &msg, const QString &ctx)
{
    emit globalError(msg, ctx);
}

void TyUpdater::reportDebug(const QString &msg, const QString &ctx)
{
    emit globalDebug(msg, ctx);
}

int TyUpdater::exec()
{
    return tyUpdater->run();
}

int TyUpdater::run()
{
    monitor_.reset(new Monitor());
    monitor_->setIgnoreGeneric(true);
    monitor_->setSerialByDefault(false);
    monitor_->setSerialLogSize(0);
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

    qRegisterMetaType<ty_log_level>("ty_log_level");
    qRegisterMetaType<std::shared_ptr<void>>("std::shared_ptr<void>");
    qRegisterMetaType<ty_descriptor>("ty_descriptor");
    qRegisterMetaType<uint64_t>("uint64_t");

    TyUpdater app(argc, argv);
    return app.exec();
}
