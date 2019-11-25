/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QDir>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextCodec>
#include <QThread>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include "arduino_install.hpp"
#include "client_handler.hpp"
#include "../libty/common.h"
#include "log_dialog.hpp"
#include "main_window.hpp"
#include "../libty/optline.h"
#include "task.hpp"
#include "tycommander.hpp"

struct ClientCommand {
    const char *name;

    int (TyCommander::*f)(int argc, char *argv[]);

    const char *arg;
    const char *description;
};

static const ClientCommand commands[] = {
    {"run",       &TyCommander::runMainInstance,      NULL,                        NULL},
    {"open",      &TyCommander::executeRemoteCommand, NULL,                        QT_TR_NOOP("Open a new window (default)")},
    {"reset",     &TyCommander::executeRemoteCommand, NULL,                        QT_TR_NOOP("Reset board")},
    {"reboot",    &TyCommander::executeRemoteCommand, NULL,                        QT_TR_NOOP("Reboot board")},
    {"upload",    &TyCommander::executeRemoteCommand, QT_TR_NOOP("[<firmwares>]"), QT_TR_NOOP("Upload current or new firmware")},
    {"attach",    &TyCommander::executeRemoteCommand, NULL,                        QT_TR_NOOP("Attach serial monitor")},
    {"detach",    &TyCommander::executeRemoteCommand, NULL,                        QT_TR_NOOP("Detach serial monitor")},
    {"integrate", &TyCommander::integrateArduino,     NULL,                        NULL},
    {"restore",   &TyCommander::integrateArduino,     NULL,                        NULL},
    // Hidden command for Arduino 1.0.6 integration
    {"avrdude",   &TyCommander::fakeAvrdudeUpload,    NULL,                        NULL},
    {0}
};

using namespace std;

TyCommander::TyCommander(int &argc, char *argv[])
    : QApplication(argc, argv), argc_(argc), argv_(argv)
{
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    setOrganizationName("TyTools");
    setApplicationName(TY_CONFIG_TYCOMMANDER_NAME);
    setApplicationVersion(ty_version_string());

    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_message_redirect([](const ty_message_data *msg, void *) {
        ty_message_default_handler(msg, nullptr);

        if (msg->type == TY_MESSAGE_LOG) {
            if (msg->u.log.level <= TY_LOG_WARNING) {
                tyCommander->reportError(msg->u.log.msg, msg->ctx);
            } else {
                tyCommander->reportDebug(msg->u.log.msg, msg->ctx);
            }
        }
    }, nullptr);

    initDatabase("tyqt", tycommander_db_);
    setDatabase(&tycommander_db_);
    loadSettings();

    action_visible_ = new QAction(tr("&Visible"), this);
    action_visible_->setCheckable(true);
    action_visible_->setChecked(true);

    action_quit_ = new QAction(tr("&Quit"), this);

    tray_menu_.addAction(action_visible_);
    tray_menu_.addSeparator();
    tray_menu_.addAction(action_quit_);

    tray_icon_.setIcon(QIcon(":/tycommander"));
    tray_icon_.setContextMenu(&tray_menu_);

    connect(&tray_icon_, &QSystemTrayIcon::activated, this, &TyCommander::trayActivated);
    connect(action_visible_, &QAction::toggled, this, &TyCommander::setVisible);
    connect(action_quit_, &QAction::triggered, this, &TyCommander::quit);

    channel_.init();
}

TyCommander::~TyCommander()
{
    ty_message_redirect(ty_message_default_handler, nullptr);
}

QString TyCommander::clientFilePath()
{
#ifdef _WIN32
    return applicationDirPath() + "/" TY_CONFIG_TYCOMMANDER_EXECUTABLE "C.exe";
#else
    return applicationFilePath();
#endif
}

void TyCommander::loadSettings()
{
    /* FIXME: Fix (most likely) broken behavior of hideOnStartup with
       Cmd+H on OSX when my MacBook is repaired. */
#ifdef __APPLE__
    show_tray_icon_ = db_.get("UI/showTrayIcon", false).toBool();
    hide_on_startup_ = db_.get("UI/hideOnStartup", false).toBool();
#else
    show_tray_icon_ = db_.get("UI/showTrayIcon", true).toBool();
    hide_on_startup_ = show_tray_icon_ && db_.get("UI/hideOnStartup", false).toBool();
#endif

    emit settingsChanged();
}

int TyCommander::exec()
{
    return tyCommander->run(tyCommander->argc_, tyCommander->argv_);
}

void TyCommander::showLogWindow()
{
    log_dialog_->show();
}

void TyCommander::reportError(const QString &msg, const QString &ctx)
{
    emit globalError(msg, ctx);
}

void TyCommander::reportDebug(const QString &msg, const QString &ctx)
{
    emit globalDebug(msg, ctx);
}

void TyCommander::setVisible(bool visible)
{
    if (visible) {
        for (auto widget: topLevelWidgets()) {
            if (widget->inherits("MainWindow")) {
                widget->move(widget->property("position").toPoint());
                widget->show();
            }
        }
    } else {
        for (auto widget: topLevelWidgets()) {
            if (widget->inherits("MainWindow")) {
                widget->setProperty("position", widget->pos());
                widget->hide();
            }
        }
    }

    action_visible_->setChecked(visible);
}

void TyCommander::setShowTrayIcon(bool show_tray_icon)
{
    show_tray_icon_ = show_tray_icon;
    tray_icon_.setVisible(show_tray_icon);

    db_.put("UI/showTrayIcon", show_tray_icon);
    emit settingsChanged();
}

void TyCommander::setHideOnStartup(bool hide_on_startup)
{
    hide_on_startup_ = hide_on_startup;

    db_.put("UI/hideOnStartup", hide_on_startup);
    emit settingsChanged();
}

int TyCommander::run(int argc, char *argv[])
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
            showClientMessage(helpText());
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[1], "--version") == 0) {
            showClientMessage(QString("%1 %2").arg(applicationName(), applicationVersion()));
            return EXIT_SUCCESS;
        }

        if (argv[1][0] != '-') {
            swap(argv[0], argv[1]);
            command_ = argv[0];
            argc--;
            argv++;
        }
    }

#ifdef _WIN32
    // TyCommanderC should not launch TyCommander, it's only a console interface
    if (command_.isEmpty() && client_console_) {
        showClientMessage(helpText());
        return EXIT_SUCCESS;
    }
#endif

    if (command_.isEmpty()) {
        if (channel_.lock()) {
            command_ = "run";
        } else {
            command_ = "open";
        }
    }

    for (auto cmd = commands; cmd->name; cmd++) {
        if (command_ == cmd->name)
            return (this->*(cmd->f))(argc, argv);
    }

    showClientError(tr("Unknown command '%1'\n%2").arg(command_, helpText()));
    return EXIT_FAILURE;
}

int TyCommander::runMainInstance(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        QString opt2 = opt;
        if (opt2 == "--help") {
            showClientMessage(helpText());
            return EXIT_SUCCESS;
        } else if (opt2 == "--quiet" || opt2 == "-q") {
            ty_config_verbosity--;
        } else {
            showClientError(tr("Unknown option '%1'\n%2").arg(opt2, helpText()));
            return EXIT_FAILURE;
        }
    }

    if (!channel_.lock()) {
        showClientError(tr("Cannot start main instance, lock file in place"));
        return EXIT_FAILURE;
    }

    connect(&channel_, &SessionChannel::newConnection, this, &TyCommander::acceptClient);

    initDatabase("boards", monitor_db_);
    monitor_.setDatabase(&monitor_db_);
    initCache("boards", monitor_cache_);
    monitor_.setCache(&monitor_cache_);
    monitor_.loadSettings();

    log_dialog_ = unique_ptr<LogDialog>(new LogDialog());
    log_dialog_->setAttribute(Qt::WA_QuitOnClose, false);
    log_dialog_->setWindowIcon(QIcon(":/tycommander"));
    connect(this, &TyCommander::globalError, log_dialog_.get(), &LogDialog::appendError);
    connect(this, &TyCommander::globalDebug, log_dialog_.get(), &LogDialog::appendDebug);

    if (show_tray_icon_)
        tray_icon_.show();
    action_visible_->setChecked(!hide_on_startup_);
    auto win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);
    if (!hide_on_startup_)
        win->show();

    /* Some environments (such as KDE Plasma) keep the application running when a tray
       icon/status notifier exists, and we don't want that. Not sure I get why that
       happens because quitWhenLastClosed is true, but this works. */
    connect(this, &TyCommander::lastWindowClosed, this, &TyCommander::quit);

    if (!monitor_.start()) {
        showClientError(ty_error_last_message());
        return EXIT_FAILURE;
    }

    if (!channel_.listen())
        reportError(tr("Failed to start session channel, single-instance mode won't work"));

    return QApplication::exec();
}

int TyCommander::executeRemoteCommand(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;
    bool autostart = false;
    bool multi = false;
    bool persist = false;
    QStringList filters;
    QString usbtype;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        QString opt2 = opt;
        if (opt2 == "--help") {
            showClientMessage(helpText());
            return EXIT_SUCCESS;
        } else if (opt2 == "--quiet" || opt2 == "-q") {
            ty_config_verbosity--;
        } else if (opt2 == "--autostart") {
            autostart = true;
        } else if (opt2 == "--wait" || opt2 == "-w") {
            wait_ = true;
        } else if (opt2 == "--multi" || opt2 == "-m") {
            multi = true;
        } else if (opt2 == "--persist" || opt2 == "-p") {
            persist = true;
        } else if (opt2 == "--board" || opt2 == "-B") {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                showClientError(tr("Option '--board' takes an argument\n%1").arg(helpText()));
                return EXIT_FAILURE;
            }

            filters.append(value);
        } else if (opt2 == "--usbtype") {
            /* Hidden option to improve the Arduino integration. Basically, if mode is set and
               does not contain "_SERIAL", --board is ignored. This way the IDE serial port
               selection is ignored when uploading to a non-serial board. */
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                showClientError(tr("Option '--usbtype' takes an argument\n%1").arg(helpText()));
                return EXIT_FAILURE;
            }

            usbtype = value;
        } else {
            showClientError(tr("Unknown option '%1'\n%2").arg(opt2, helpText()));
            return EXIT_FAILURE;
        }
    }

    auto client = channel_.connectToServer();
    if (!client) {
        if (autostart) {
            if (!QProcess::startDetached(applicationFilePath(), {"-qqq"})) {
                showClientError(tr("Failed to start TyCommander main instance"));
                return EXIT_FAILURE;
            }

            QElapsedTimer timer;
            timer.start();
            while (!client && timer.elapsed() < 3000) {
                QThread::msleep(20);
                client = channel_.connectToServer();
            }
        }

        if (!client) {
            showClientError(tr("Cannot connect to main instance"));
            return EXIT_FAILURE;
        }
    }

    connect(client.get(), &SessionPeer::received, this, &TyCommander::processServerAnswer);

    // Hack for Arduino integration, see option loop above
    if (!usbtype.isEmpty() && !usbtype.contains("_SERIAL"))
        filters.clear();

    client->send({"workdir", QDir::currentPath()});
    if (multi)
        client->send("multi");
    if (persist)
        client->send("persist");
    if (!filters.isEmpty())
        client->send(QStringList{"select"} + filters);
    QStringList command_arglist = {command_};
    while ((opt = ty_optline_consume_non_option(&optl)))
        command_arglist.append(opt);
    client->send(command_arglist);

    connect(client.get(), &SessionPeer::closed, this, [=](SessionPeer::CloseReason reason) {
        if (reason != SessionPeer::LocalClose) {
            showClientError(tr("Main instance closed the connection"));
            exit(1);
        }
    });

    return QApplication::exec();
}

int TyCommander::integrateArduino(int argc, char *argv[])
{
    if (argc < 2) {
        showClientError(helpText());
        return EXIT_FAILURE;
    }

    ArduinoInstallation install(argv[1]);

    connect(&install, &ArduinoInstallation::log, [](const QString &msg) {
        printf("%s\n", msg.toLocal8Bit().constData());
        fflush(stdout);
    });
    connect(&install, &ArduinoInstallation::error, [](const QString &msg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    });

    if (command_ == "integrate") {
        return install.integrate() ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        return install.restore() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}

int TyCommander::fakeAvrdudeUpload(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;
    QString upload;
    bool verbose = false;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        QString opt2 = opt;
        if (opt2 == "-U") {
            upload = ty_optline_get_value(&optl);
        } else if (opt2 == "-v") {
            verbose = true;
        /* Ignore most switches, we need to get the value of the ones taking an argument
           or they will treated as concatenated single-character switches. */
        } else if (opt2 == "-p" || opt2 == "-b" || opt2 == "-B" || opt2 == "-c" || opt2 == "-C" ||
                   opt2 == "-E" || opt2 == "-i" || opt2 == "-P" || opt2 == "-x") {
            ty_optline_get_value(&optl);
        }
    }

    /* The only avrdude operation we support is -Uflash:w:filename[:format] (format is ignored)
       and of course filename can contain colons (the Windows drive separator, for example). */
    auto op_parts = QString(upload).split(":");
    if (op_parts.count() < 3 || op_parts.takeFirst() != "flash" || op_parts.takeFirst() != "w") {
        showClientError(tr("Invalid '-U' upload string '%1'").arg(upload));
        return EXIT_FAILURE;
    }
    if (op_parts.count() > 1)
        op_parts.removeLast();
    upload = op_parts.join(':');

    /* I could factorize sendRemoteCommand() but I like to keep the whole fakeAvrdudeUpload()
       thing isolated. Ugly, but non-invasive. */
    command_ = "upload";
    const char *fake_argv[8];
    int fake_argc = 0;
    fake_argv[fake_argc++] = argv[0];
    fake_argv[fake_argc++] = "--autostart";
    fake_argv[fake_argc++] = "--wait";
    fake_argv[fake_argc++] = "--multi";
    if (!verbose)
        fake_argv[fake_argc++] = "--quiet";
    auto filename = upload.toLocal8Bit();
    fake_argv[fake_argc++] = filename.constData();

    return executeRemoteCommand(fake_argc, const_cast<char **>(fake_argv));
}

void TyCommander::resetMonitor()
{
    monitor_cache_.clear();
    monitor_.loadSettings();
}

void TyCommander::clearSettingsAndReset()
{
    tycommander_db_.clear();
    loadSettings();

    monitor_db_.clear();
    monitor_cache_.clear();
    monitor_.loadSettings();
}

void TyCommander::clearSettingsAndResetWithConfirmation(QWidget *parent)
{
    QMessageBox msgbox(parent);

    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setWindowTitle(tr("Reset Settings & Application"));
    msgbox.setText(tr("Reset will erase all your settings, including individual board settings and tags."));
    auto reset = msgbox.addButton(tr("Reset"), QMessageBox::AcceptRole);
    msgbox.addButton(QMessageBox::Cancel);
    msgbox.setDefaultButton(QMessageBox::Cancel);

    msgbox.exec();
    if (msgbox.clickedButton() != reset)
        return;

    tyCommander->clearSettingsAndReset();
}

void TyCommander::initDatabase(const QString &name, SettingsDatabase &db)
{
    auto settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                  organizationName(), name, this);
    settings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    db.setSettings(settings);
}

void TyCommander::initCache(const QString &name, SettingsDatabase &cache)
{
    /* QStandardPaths adds organizationName()/applicationName() to the generic OS cache path,
       but we put our files in organizationName() to share them with tycmd. On Windows, Qt uses
       AppData/Local/organizationName()/applicationName()/cache so we need to special case that. */
#ifdef _WIN32
    auto path = QString("%1/../%2.ini")
                .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation), name);
#else
    auto path = QString("%1/../%2.ini")
                .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation), name);
#endif
    auto settings = new QSettings(path, QSettings::IniFormat, this);
    settings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    cache.setSettings(settings);
}

QString TyCommander::helpText()
{
    QString help = tr("usage: %1 <command> [options]\n\n"
                      "General options:\n"
                      "       --help               Show help message\n"
                      "       --version            Display version information\n"
                      "   -q, --quiet              Disable output, use -qqq to silence errors\n\n"
                      "Client options:\n"
                      "       --autostart          Start main instance if it is not available\n"
                      "   -w, --wait               Wait until full completion\n\n"
                      "   -B, --board <tag>        Work with board <tag> instead of first detected\n"
                      "   -m, --multi              Select all matching boards (first match by default)\n"
                      "   -p, --persist            Save new board settings (e.g. command attach)\n\n"
                      "Commands:\n").arg(QFileInfo(QApplication::applicationFilePath()).fileName());

    for (auto cmd = commands; cmd->name; cmd++) {
        if (!cmd->description)
            continue;

        QString name = cmd->name;
        if (cmd->arg)
            name += QString(" %1").arg(tr(cmd->arg));
        help += QString("   %1 %2\n").arg(name, -24).arg(tr(cmd->description));
    }
    help.chop(1);

    return help;
}

void TyCommander::showClientMessage(const QString &msg)
{
    if (client_console_) {
        printf("%s\n", msg.toLocal8Bit().constData());
    } else {
        QMessageBox::information(nullptr, QApplication::applicationName(), msg);
    }
}

void TyCommander::showClientError(const QString &msg)
{
    if (client_console_) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    } else {
        QMessageBox::critical(nullptr, tr("%1 (error)").arg(QApplication::applicationName()), msg);
    }
}

void TyCommander::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifndef __APPLE__
    if (reason == QSystemTrayIcon::Trigger)
        setVisible(!visible());
#else
    Q_UNUSED(reason);
#endif
}

void TyCommander::acceptClient()
{
    auto peer = channel_.nextPendingConnection();
    auto client = new ClientHandler(move(peer), this);
    connect(client, &ClientHandler::closed, client, &ClientHandler::deleteLater);
}

void TyCommander::processServerAnswer(const QStringList &arguments)
{
    QStringList parameters = arguments;
    QString cmd;

    if (!arguments.count())
        goto error;
    cmd = parameters.takeFirst();

    if (cmd == "log") {
        if (parameters.count() < 2)
            goto error;

        ty_message_data msg = {};
        QByteArray ctx_buf;
        if (!parameters[0].isEmpty()) {
            ctx_buf = parameters[0].toLocal8Bit();
            msg.ctx = ctx_buf.constData();
        }
        msg.type = TY_MESSAGE_LOG;
        msg.u.log.level = static_cast<ty_log_level>(QString(parameters[1]).toInt());
        QByteArray msg_buf = parameters[2].toLocal8Bit();
        msg.u.log.msg = msg_buf.constData();

        ty_message(&msg);
    } else if (cmd == "progress") {
        if (parameters.count() < 3)
            goto error;

        ty_message_data msg = {};
        QByteArray ctx_buf;
        if (!parameters[0].isEmpty()) {
            ctx_buf = parameters[0].toLocal8Bit();
            msg.ctx = ctx_buf.constData();
        }
        msg.type = TY_MESSAGE_PROGRESS;
        QByteArray action_buf = parameters[1].toLocal8Bit();
        msg.u.progress.action = action_buf.constData();
        msg.u.progress.value = parameters[2].toULongLong();
        msg.u.progress.max = parameters[3].toULongLong();

        ty_message(&msg);
    } else if (cmd == "start") {
        if (!wait_)
            exit(0);
    } else if (cmd == "exit") {
        exit(parameters.value(0, "0").toInt());
#ifdef _WIN32
    } else if (cmd == "allowsetforegroundwindow") {
        if (parameters.count() < 1)
            goto error;

        /* The server may show a window for some commands, such as the board dialog. Executables
           launched from an application with focus can pop on top, so this instance can probably
           do it but the TyCommander main instance cannot unless we call
           AllowSetForegroundWindow(). It also works if this instance is run through tyqtc
           (to provide console I/O) because tyqtc calls AllowSetForegroundWindow() for this
           process too.

           We could use GetNamedPipeServerProcessId() instead of sending the PID through the
           channel, but it is not available on XP. */
        AllowSetForegroundWindow(parameters[0].toUInt());
#endif
    } else {
        goto error;
    }

    return;

error:
    showClientError(tr("Received incorrect data from main instance"));
    exit(1);
}
