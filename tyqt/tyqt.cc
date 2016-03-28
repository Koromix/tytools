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
#include <QTextCodec>
#include <QThread>

#include <getopt.h>

#include "arduino_install.hh"
#include "commands.hh"
#include "log_window.hh"
#include "main_window.hh"
#include "selector_dialog.hh"
#include "task.hh"
#include "tyqt.hh"
#include "ty/version.h"

struct ClientCommand {
    const char *name;

    int (TyQt::*f)(int argc, char *argv[]);

    const char *arg;
    const char *description;
};

enum {
    OPTION_HELP = 0x100,
    OPTION_AUTOSTART,
    OPTION_USBTYPE
};

static const ClientCommand commands[] = {
    {"run",       &TyQt::runMainInstance,      NULL,                      NULL},
    {"open",      &TyQt::executeRemoteCommand, NULL,                      QT_TR_NOOP("Open a new TyQt window (default)")},
    {"activate",  &TyQt::executeRemoteCommand, NULL,                      QT_TR_NOOP("Bring TyQt window to foreground")},
    {"reset",     &TyQt::executeRemoteCommand, NULL,                      QT_TR_NOOP("Reset board")},
    {"reboot",    &TyQt::executeRemoteCommand, NULL,                      QT_TR_NOOP("Reboot board")},
    {"upload",    &TyQt::executeRemoteCommand, QT_TR_NOOP("[firmwares]"), QT_TR_NOOP("Upload current or new firmware")},
    {"integrate", &TyQt::integrateArduino,     NULL,                      NULL},
    {"restore",   &TyQt::integrateArduino,     NULL,                      NULL},
    // Hidden command for Arduino 1.0.6 integration
    {"avrdude",   &TyQt::fakeAvrdudeUpload,    NULL,                      NULL},
    {0}
};

static const char *short_options = ":qwb:";
static const struct option long_options[] = {
    {"help",         no_argument,       NULL, OPTION_HELP},
    {"quiet",        no_argument,       NULL, 'q'},
    {"autostart",    no_argument,       NULL, OPTION_AUTOSTART},
    {"wait",         no_argument,       NULL, 'w'},
    {"board",        required_argument, NULL, 'b'},
    {"usbtype",      required_argument, NULL, OPTION_USBTYPE},
    {0}
};

using namespace std;

TyQt::TyQt(int &argc, char *argv[])
    : QApplication(argc, argv), argc_(argc), argv_(argv)
{
    setOrganizationName("ty");
    setApplicationName("TyQt");
    setApplicationVersion(TY_VERSION);

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

QString TyQt::clientFilePath() const
{
#ifdef _WIN32
    return applicationDirPath() + "/tyqtc.exe";
#else
    return applicationFilePath();
#endif
}

SelectorDialog *TyQt::openSelector(const QString &action, const QString &desc)
{
    auto dialog = new SelectorDialog(&monitor_, getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    if (!action.isEmpty())
        dialog->setAction(action);
    if (!desc.isEmpty())
        dialog->setDescription(desc);
    activateMainWindow();

    return dialog;
}

MainWindow *TyQt::getMainWindow() const
{
    for (auto widget: topLevelWidgets()) {
        if (widget->inherits("MainWindow"))
            return qobject_cast<MainWindow *>(widget);
    }

    return nullptr;
}

void TyQt::openMainWindow()
{
    auto win = new MainWindow(&monitor_);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
}

void TyQt::activateMainWindow(MainWindow *win)
{
    if (!win) {
        win = getMainWindow();
        if (!win)
            return;
    }

    win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
    win->raise();
    win->activateWindow();
}

void TyQt::openLogWindow()
{
    log_window_->show();
}

void TyQt::reportError(const QString &msg)
{
    emit globalError(msg);
}

void TyQt::reportDebug(const QString &msg)
{
    emit globalDebug(msg);
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

void TyQt::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifndef __APPLE__
    if (reason == QSystemTrayIcon::Trigger)
        setVisible(!visible());
#else
    Q_UNUSED(reason);
#endif
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
        peer.send({"exit", success ? "1" : "0"});
    });
    connect(watcher, &TaskWatcher::progress, &peer, [&peer](const QString &action, unsigned int value, unsigned int max) {
        peer.send({"progress", action, QString::number(value), QString::number(max)});
    });
    watcher->setTask(&task);

    task.start();
}

void TyQt::readAnswer(SessionPeer &peer, const QStringList &arguments)
{
    Q_UNUSED(peer);

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
        if (!wait_) {
            channel_.disconnect(this);
            exit(0);
        }
    } else if (cmd == "exit") {
        exit(parameters.value(0, "0").toInt());
    } else if (cmd == "progress") {
        if (parameters.count() < 3)
            goto error;

        QString action = parameters[0];
        unsigned int progress = parameters[1].toUInt();
        unsigned int total = parameters[2].toUInt();

        ty_progress(action.toLocal8Bit().constData(), progress, total);
    } else {
        goto error;
    }

    return;

error:
    showClientError(tr("Received incorrect data from main TyQt instance"));
    exit(1);
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
            command_ = argv[1];
            argv[1] = argv[0];
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

    // We'll print our own, for consistency
    opterr = 0;

    for (auto cmd = commands; cmd->name; cmd++) {
        if (command_ == cmd->name)
            return (this->*(cmd->f))(argc, argv);
    }

    showClientError(tr("Unknown command '%1'\n%2").arg(command_, helpText()));
    return EXIT_FAILURE;
}

int TyQt::runMainInstance(int argc, char *argv[])
{
    optind = 0;
    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            showClientMessage(helpText());
            return EXIT_SUCCESS;
        case 'q':
            ty_config_quiet = static_cast<ty_log_level>(static_cast<int>(ty_config_quiet) + 1);
            break;
        }
    }

    if (!channel_.lock()) {
        showClientError(tr("Cannot start main TyQt instance, lock file in place"));
        return EXIT_FAILURE;
    }

    connect(&channel_, &SessionChannel::received, this, &TyQt::executeAction);

    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_message_redirect([](ty_task *task, ty_message_type type, const void *data, void *udata) {
        ty_message_default_handler(task, type, data, udata);

        if (type == TY_MESSAGE_LOG) {
            auto print = static_cast<const ty_log_message *>(data);
            if (print->level >= TY_LOG_WARNING) {
                tyQt->reportError(print->msg);
            } else {
                tyQt->reportDebug(print->msg);
            }
        }
    }, nullptr);

    initDatabase("boards", monitor_db_);
    monitor_.setDatabase(&monitor_db_);
    monitor_.loadSettings();

    log_window_ = unique_ptr<LogWindow>(new LogWindow());
    log_window_->setAttribute(Qt::WA_QuitOnClose, false);
    connect(this, &TyQt::globalError, log_window_.get(), &LogWindow::appendError);
    connect(this, &TyQt::globalDebug, log_window_.get(), &LogWindow::appendDebug);

    if (show_tray_icon_)
        tray_icon_.show();
    action_visible_->setChecked(!hide_on_startup_);
    auto win = new MainWindow(&monitor_);
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
    bool autostart = false;
    QString board, usbtype;

    optind = 0;
    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            showClientMessage(helpText());
            return EXIT_SUCCESS;
        case 'q':
            ty_config_quiet = static_cast<ty_log_level>(static_cast<int>(ty_config_quiet) + 1);
            break;

        case OPTION_AUTOSTART:
            autostart = true;
            break;
        case 'w':
            wait_ = true;
            break;
        case 'b':
            board = optarg;
            break;
        /* Hidden option to improve the Arduino integration. Basically, if mode is set and does
           not contain "_SERIAL", --board is ignored. This way the IDE serial port selection
           is ignored when uploading to a non-serial board. */
        case OPTION_USBTYPE:
            usbtype = optarg;
            break;

        case ':':
            showClientError(tr("Option '%1' takes an argument\n%2").arg(argv[optind - 1])
                                                                   .arg(helpText()));
            return EXIT_FAILURE;
        case '?':
            showClientError(tr("Unknown option '%1'\n%2").arg(argv[optind - 1]).arg(helpText()));
            return EXIT_FAILURE;
        }
    }

    if (!channel_.connectToMaster()) {
        if (autostart) {
            if (!QProcess::startDetached(applicationFilePath(), {"-qqq"})) {
                showClientError(tr("Failed to start TyQt main instance"));
                return EXIT_FAILURE;
            }

            QElapsedTimer timer;
            timer.start();
            while (!channel_.connectToMaster() && timer.elapsed() < 3000)
                QThread::msleep(20);
        }

        if (!channel_.isConnected()) {
            showClientError(tr("Cannot connect to main TyQt instance"));
            return EXIT_FAILURE;
        }
    }

    connect(&channel_, &SessionChannel::received, this, &TyQt::readAnswer);

    // Hack for Arduino integration, see getopt loop in TyQt::run()
    if (!usbtype.isEmpty() && !usbtype.contains("_SERIAL"))
        board = "";

    QStringList arguments = {command_, QDir::currentPath(), board};
    for (int i = optind; i < argc; i++)
        arguments.append(argv[i]);
    channel_.send(arguments);

    connect(&channel_, &SessionChannel::masterClosed, this, [=]() {
        showClientError(tr("Main TyQt instance closed the connection"));
        exit(1);
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
    QString upload;
    bool verbose = false;

    optind = 0;
    int c;
    /* Ignore most switches, we need to pass the ones taking an argument to getopt() or
       their arguments will be treated as concatenated single-character switches. */
    while ((c = getopt(argc, argv, "U:vp:b:B:c:C:E:i:P:x:")) != -1) {
        switch (c) {
        case 'U':
            upload = optarg;
            break;
        case 'v':
            verbose = true;
            break;
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
    monitor_.stop();
    monitor_.start();
}

void TyQt::clearSettingsAndReset()
{
    tyqt_db_.clear();
    loadSettings();

    monitor_db_.clear();
    monitor_.loadSettings();
    monitor_.stop();
    monitor_.start();
}

void TyQt::clearSettingsAndResetWithConfirmation(QWidget *parent)
{
    QMessageBox msgbox(parent);

    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setWindowTitle(tr("Reset Settings & TyQt"));
    msgbox.setText(tr("Reset will erase all your TyQt settings, including individual board settings and tags."));
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

QString TyQt::helpText()
{
    QString help = tr("usage: tyqt <command> [options]\n\n"
                      "General options:\n"
                      "       --help               Show help message\n"
                      "       --version            Display version information\n\n"
                      "   -w, --wait               Wait until task completion\n"
                      "   -b, --board <tag>        Work with board <tag> instead of first detected\n"
                      "   -q, --quiet              Disable output, use -qqq to silence errors\n\n"
                      "Commands:\n");

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
        printf("%s\n", qPrintable(msg));
    } else {
        QMessageBox::information(nullptr, "TyQt", msg);
    }
}

void TyQt::showClientError(const QString &msg)
{
    if (client_console_) {
        fprintf(stderr, "%s\n", qPrintable(msg));
    } else {
        QMessageBox::critical(nullptr, tr("TyQt (error)"), msg);
    }
}
