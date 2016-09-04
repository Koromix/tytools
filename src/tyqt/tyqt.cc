/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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
#include "commands.hpp"
#include "ty/common.h"
#include "tyqt/log_dialog.hpp"
#include "main_window.hpp"
#include "ty/optline.h"
#include "tyqt/task.hpp"
#include "tyqt.hpp"

struct ClientCommand {
    const char *name;

    int (TyQt::*f)(int argc, char *argv[]);

    const char *arg;
    const char *description;
};

static const ClientCommand commands[] = {
    {"run",       &TyQt::runMainInstance,      NULL,                        NULL},
    {"open",      &TyQt::executeRemoteCommand, NULL,                        QT_TR_NOOP("Open a new window (default)")},
    {"reset",     &TyQt::executeRemoteCommand, NULL,                        QT_TR_NOOP("Reset board")},
    {"reboot",    &TyQt::executeRemoteCommand, NULL,                        QT_TR_NOOP("Reboot board")},
    {"upload",    &TyQt::executeRemoteCommand, QT_TR_NOOP("[<firmwares>]"), QT_TR_NOOP("Upload current or new firmware")},
    {"integrate", &TyQt::integrateArduino,     NULL,                        NULL},
    {"restore",   &TyQt::integrateArduino,     NULL,                        NULL},
    // Hidden command for Arduino 1.0.6 integration
    {"avrdude",   &TyQt::fakeAvrdudeUpload,    NULL,                        NULL},
    {0}
};

using namespace std;

TyQt::TyQt(int &argc, char *argv[])
    : QApplication(argc, argv), argc_(argc), argv_(argv)
{
    setOrganizationName("ty");
    setApplicationName(TY_CONFIG_TYQT_NAME);
    setApplicationVersion(ty_version_string());

    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_message_redirect([](const ty_message_data *msg, void *) {
        ty_message_default_handler(msg, nullptr);

        if (msg->type == TY_MESSAGE_LOG) {
            if (msg->u.log.level <= TY_LOG_WARNING) {
                tyQt->reportError(msg->u.log.msg, msg->ctx);
            } else {
                tyQt->reportDebug(msg->u.log.msg, msg->ctx);
            }
        }
    }, nullptr);

    initDatabase("tyqt", tyqt_db_);
    setDatabase(&tyqt_db_);
    loadSettings();

    action_visible_ = new QAction(tr("&Visible"), this);
    action_visible_->setCheckable(true);
    action_visible_->setChecked(true);

    action_quit_ = new QAction(tr("&Quit"), this);

    tray_menu_.addAction(action_visible_);
    tray_menu_.addSeparator();
    tray_menu_.addAction(action_quit_);

    tray_icon_.setIcon(QIcon(":/tyqt"));
    tray_icon_.setContextMenu(&tray_menu_);

    connect(&tray_icon_, &QSystemTrayIcon::activated, this, &TyQt::trayActivated);
    connect(action_visible_, &QAction::toggled, this, &TyQt::setVisible);
    connect(action_quit_, &QAction::triggered, this, &TyQt::quit);

    channel_.init();
}

TyQt::~TyQt()
{
    ty_message_redirect(ty_message_default_handler, nullptr);
}

QString TyQt::clientFilePath()
{
#ifdef _WIN32
    return applicationDirPath() + "/tyqtc.exe";
#else
    return applicationFilePath();
#endif
}

void TyQt::loadSettings()
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

int TyQt::exec()
{
    return tyQt->run(tyQt->argc_, tyQt->argv_);
}

void TyQt::showLogWindow()
{
    log_dialog_->show();
}

void TyQt::reportError(const QString &msg, const QString &ctx)
{
    emit globalError(msg, ctx);
}

void TyQt::reportDebug(const QString &msg, const QString &ctx)
{
    emit globalDebug(msg, ctx);
}

void TyQt::setVisible(bool visible)
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

void TyQt::setShowTrayIcon(bool show_tray_icon)
{
    show_tray_icon_ = show_tray_icon;
    tray_icon_.setVisible(show_tray_icon);

    db_.put("UI/showTrayIcon", show_tray_icon);
    emit settingsChanged();
}

void TyQt::setHideOnStartup(bool hide_on_startup)
{
    hide_on_startup_ = hide_on_startup;

    db_.put("UI/hideOnStartup", hide_on_startup);
    emit settingsChanged();
}

int TyQt::run(int argc, char *argv[])
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
    // tyqtc should not launch TyQt, it's only a console interface
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

int TyQt::runMainInstance(int argc, char *argv[])
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

    connect(&channel_, &SessionChannel::newConnection, this, &TyQt::acceptClient);

    initDatabase("boards", monitor_db_);
    monitor_.setDatabase(&monitor_db_);
    initCache("boards", monitor_cache_);
    monitor_.setCache(&monitor_cache_);
    monitor_.loadSettings();

    log_dialog_ = unique_ptr<LogDialog>(new LogDialog());
    log_dialog_->setAttribute(Qt::WA_QuitOnClose, false);
    log_dialog_->setWindowIcon(QIcon(":/tyqt"));
    connect(this, &TyQt::globalError, log_dialog_.get(), &LogDialog::appendError);
    connect(this, &TyQt::globalDebug, log_dialog_.get(), &LogDialog::appendDebug);

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
    connect(this, &TyQt::lastWindowClosed, this, &TyQt::quit);

    if (!monitor_.start()) {
        showClientError(ty_error_last_message());
        return EXIT_FAILURE;
    }

    if (!channel_.listen())
        reportError(tr("Failed to start session channel, single-instance mode won't work"));

    return QApplication::exec();
}

int TyQt::executeRemoteCommand(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;
    bool autostart = false;
    QString board, usbtype;

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
        } else if (opt2 == "--board" || opt2 == "-B") {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                showClientError(tr("Option '--board' takes an argument\n%1").arg(helpText()));
                return EXIT_FAILURE;
            }

            board = value;
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
                showClientError(tr("Failed to start TyQt main instance"));
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

    connect(client.get(), &SessionPeer::received, this, &TyQt::processServerAnswer);

    // Hack for Arduino integration, see option loop in TyQt::run()
    if (!usbtype.isEmpty() && !usbtype.contains("_SERIAL"))
        board = "";

    QStringList arguments = {command_, QDir::currentPath(), board};
    while ((opt = ty_optline_consume_non_option(&optl)))
        arguments.append(opt);
    client->send(arguments);

    connect(client.get(), &SessionPeer::closed, this, [=](SessionPeer::CloseReason reason) {
        if (reason != SessionPeer::LocalClose) {
            showClientError(tr("Main instance closed the connection"));
            exit(1);
        }
    });

    return QApplication::exec();
}

int TyQt::integrateArduino(int argc, char *argv[])
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

int TyQt::fakeAvrdudeUpload(int argc, char *argv[])
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
    if (!verbose)
        fake_argv[fake_argc++] = "--quiet";
    auto filename = upload.toLocal8Bit();
    fake_argv[fake_argc++] = filename.constData();

    return executeRemoteCommand(fake_argc, const_cast<char **>(fake_argv));
}

void TyQt::resetMonitor()
{
    monitor_cache_.clear();
    monitor_.stop();
    monitor_.loadSettings();
    monitor_.start();
}

void TyQt::clearSettingsAndReset()
{
    tyqt_db_.clear();
    loadSettings();

    monitor_db_.clear();
    monitor_cache_.clear();
    monitor_.stop();
    monitor_.loadSettings();
    monitor_.start();
}

void TyQt::clearSettingsAndResetWithConfirmation(QWidget *parent)
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

    tyQt->clearSettingsAndReset();
}

void TyQt::initDatabase(const QString &name, SettingsDatabase &db)
{
    auto settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                  organizationName(), name, this);
    settings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    db.setSettings(settings);
}

void TyQt::initCache(const QString &name, SettingsDatabase &cache)
{
    /* QStandardPaths adds organizationName()/applicationName() to the generic OS cache path,
       but we put our files in organizationName() to share them with tyc. On Windows, Qt uses
       AppData/Local/organizationName()/applicationName()/cache so we need to special case that. */
#ifdef _WIN32
    auto path = QString("%1/../cache/%2.ini")
                .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation), name);
#else
    auto path = QString("%1/../%2.ini")
                .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation), name);
#endif
    auto settings = new QSettings(path, QSettings::IniFormat, this);
    settings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    cache.setSettings(settings);
}

QString TyQt::helpText()
{
    QString help = tr("usage: %1 <command> [options]\n\n"
                      "General options:\n"
                      "       --help               Show help message\n"
                      "       --version            Display version information\n"
                      "   -q, --quiet              Disable output, use -qqq to silence errors\n\n"
                      "Client options:\n"
                      "       --autostart          Start main instance if it is not available\n"
                      "   -w, --wait               Wait until task completion\n"
                      "   -B, --board <tag>        Work with board <tag> instead of first detected\n\n"
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

void TyQt::showClientMessage(const QString &msg)
{
    if (client_console_) {
        printf("%s\n", msg.toLocal8Bit().constData());
    } else {
        QMessageBox::information(nullptr, QApplication::applicationName(), msg);
    }
}

void TyQt::showClientError(const QString &msg)
{
    if (client_console_) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    } else {
        QMessageBox::critical(nullptr, tr("%1 (error)").arg(QApplication::applicationName()), msg);
    }
}

void TyQt::executeAction(SessionPeer &peer, const QStringList &arguments)
{
    if (arguments.isEmpty()) {
        peer.send({"log", QString::number(TY_LOG_ERROR), tr("Command not specified")});
        peer.send({"exit", "1"});
        return;
    }

    QStringList parameters = arguments;
    QString cmd = parameters.takeFirst();

    auto task = Commands::execute(cmd, parameters);
    auto watcher = new TaskWatcher(&peer);

    connect(watcher, &TaskWatcher::log, &peer, [&peer](int level, const QString &msg) {
        peer.send({"log", QString::number(level), msg});
    });
    connect(watcher, &TaskWatcher::started, &peer, [&peer]() {
        peer.send("start");
    });
    connect(watcher, &TaskWatcher::finished, &peer, [&peer](bool success) {
        peer.send({"exit", success ? "0" : "1"});
    });
    connect(watcher, &TaskWatcher::progress, &peer, [&peer](const QString &action, uint64_t value, uint64_t max) {
        peer.send({"progress", action, QString::number(value), QString::number(max)});
    });
    watcher->setTask(&task);

    task.start();
}

void TyQt::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifndef __APPLE__
    if (reason == QSystemTrayIcon::Trigger)
        setVisible(!visible());
#else
    Q_UNUSED(reason);
#endif
}

void TyQt::acceptClient()
{
    auto peer = channel_.nextPendingConnection().release();

    connect(peer, &SessionPeer::closed, peer, &SessionPeer::deleteLater);
    connect(peer, &SessionPeer::received, this, [=](const QStringList &arguments) {
        executeAction(*peer, arguments);
    });

#ifdef _WIN32
    peer->send({"allowsetforegroundwindow", QString::number(GetCurrentProcessId())});
#endif
}

void TyQt::processServerAnswer(const QStringList &arguments)
{
    QStringList parameters = arguments;
    QString cmd;

    if (!arguments.count())
        goto error;
    cmd = parameters.takeFirst();

    if (cmd == "log") {
        if (parameters.count() < 2)
            goto error;

        int level = QString(parameters[0]).toInt();
        QString msg = parameters[1];

        ty_log(static_cast<ty_log_level>(level), "%s", msg.toLocal8Bit().constData());
    } else if (cmd == "start") {
        if (!wait_)
            exit(0);
    } else if (cmd == "exit") {
        exit(parameters.value(0, "0").toInt());
    } else if (cmd == "progress") {
        if (parameters.count() < 3)
            goto error;

        QString action = parameters[0];
        uint64_t progress = parameters[1].toULongLong();
        uint64_t total = parameters[2].toULongLong();

        ty_progress(action.toLocal8Bit().constData(), progress, total);
#ifdef _WIN32
    } else if (cmd == "allowsetforegroundwindow") {
        if (parameters.count() < 1)
            goto error;

        /* The server may show a window for some commands, such as the board dialog. Executables
           launched from an application with focus can pop on top, so this instance can probably
           do it but the TyQt main instance cannot unless we call AllowSetForegroundWindow(). It
           also works if this instance is run through tyqtc (to provide console I/O) because
           tyqtc calls AllowSetForegroundWindow() for this process too.

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
