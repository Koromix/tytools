/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDir>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <QTimer>

#include <getopt.h>

#include "arduino_install.hh"
#include "commands.hh"
#include "tyqt.hh"

struct ClientCommand {
    const char *name;

    int (TyQt::*f)();

    const char *arg;
    const char *description;
};

enum {
    OPTION_HELP = 0x100,
    OPTION_VERSION,
    OPTION_EXPERIMENTAL,
    OPTION_USBTYPE
};

static const ClientCommand commands[] = {
    {"open",      &TyQt::sendRemoteCommand, NULL,                      QT_TR_NOOP("Open a new TyQt window (default)")},
    {"activate",  &TyQt::sendRemoteCommand, NULL,                      QT_TR_NOOP("Bring TyQt window to foreground")},
    {"reset",     &TyQt::sendRemoteCommand, NULL,                      QT_TR_NOOP("Reset board")},
    {"reboot",    &TyQt::sendRemoteCommand, NULL,                      QT_TR_NOOP("Reboot board")},
    {"upload",    &TyQt::sendRemoteCommand, QT_TR_NOOP("[firmwares]"), QT_TR_NOOP("Upload current or new firmware")},
    {"integrate", &TyQt::integrateArduino,  NULL,                      NULL},
    {"restore",   &TyQt::integrateArduino,  NULL,                      NULL},
    {0}
};

static const char *short_options_ = ":b:wq";
static const struct option long_options_[] = {
    {"help",         no_argument,       NULL, OPTION_HELP},
    {"version",      no_argument,       NULL, OPTION_VERSION},
    {"board",        required_argument, NULL, 'b'},
    {"wait",         no_argument,       NULL, 'w'},
    {"quiet",        no_argument,       NULL, 'q'},
    {"experimental", no_argument,       NULL, OPTION_EXPERIMENTAL},
    {"usbtype",      required_argument, NULL, OPTION_USBTYPE},
    {0}
};

using namespace std;

TyQt::TyQt(int &argc, char *argv[])
    : QApplication(argc, argv), argc_(argc), argv_(argv)
{
    setApplicationName("TyQt");
    setApplicationVersion(TY_VERSION);

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
    for (auto win: main_windows_) {
        win->disconnect(this);
        delete win;
    }

    ty_message_redirect(ty_message_default_handler, nullptr);
}

int TyQt::exec()
{
    return tyQt->run();
}

SelectorDialog *TyQt::openSelector()
{
    if (main_windows_.empty())
        return nullptr;

    auto dialog = new SelectorDialog(&manager_, main_windows_.front());
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    activateMainWindow();

    return dialog;
}

void TyQt::openMainWindow()
{
    MainWindow *win = new MainWindow(&manager_);

    win->setAttribute(Qt::WA_DeleteOnClose);
    connect(win, &MainWindow::destroyed, this, [=]() {
        auto it = find(main_windows_.begin(), main_windows_.end(), win);
        if (it != main_windows_.end())
            main_windows_.erase(it);

        /* Some environments (such as KDE Plasma) keep the application running when a tray
           icon/status notifier exists, and we don't want that. */
        if (main_windows_.empty())
            quit();
    });
    main_windows_.push_back(win);

    connect(this, &TyQt::errorMessage, win, &MainWindow::showErrorMessage);

    win->show();
}

void TyQt::activateMainWindow()
{
    if (main_windows_.empty())
        return;

    auto win = main_windows_.front();
    win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
    win->raise();
    win->activateWindow();
}

void TyQt::reportError(const QString &msg)
{
    emit errorMessage(msg);
}

void TyQt::setVisible(bool visible)
{
    if (visible) {
        for (auto &win: main_windows_) {
            win->move(win->property("position").toPoint());
            win->show();
        }
    } else {
        for (auto &win: main_windows_) {
            win->setProperty("position", win->pos());
            win->hide();
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
    TY_UNUSED(reason);
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
    TY_UNUSED(peer);

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

int TyQt::run()
{
    if (argc_ >= 2 && argv_[1][0] != '-') {
        command_ = argv_[1];
        argv_++;
        argc_--;
    }

    opterr = 0;
    int c;
    while ((c = getopt_long(argc_, argv_, short_options_, long_options_, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            showClientMessage(helpText());
            return 0;
        case OPTION_VERSION:
            showClientMessage(QString("%1 %2").arg(applicationName(), applicationVersion()));
            return 0;

        case 'b':
            board_ = optarg;
            break;
        case 'w':
            wait_ = true;
            break;

        case 'q':
            ty_config_quiet = static_cast<ty_log_level>(static_cast<int>(ty_config_quiet) + 1);
            break;
        case OPTION_EXPERIMENTAL:
            ty_config_experimental = true;
            break;

        /* Hidden option to improve the Arduino integration. Basically, if mode is set and does
           not contain "_SERIAL", --board is ignored. This way the IDE serial port selection
           is ignored when uploading to a non-serial board. */
        case OPTION_USBTYPE:
            usbtype_ = optarg;
            break;

        case ':':
            showClientError(tr("Option '%1' takes an argument\n%2").arg(argv_[optind - 1])
                                                                   .arg(helpText()));
            return 1;
        case '?':
            showClientError(tr("Unknown option '%1'\n%2").arg(argv_[optind - 1]).arg(helpText()));
            return 1;
        }
    }

    if (command_.isEmpty() && optind < argc_) {
        showClientError(tr("Command must be placed first\n%1").arg(helpText()));
        return 1;
    }

#ifdef _WIN32
    // tyqtc should not launch TyQt, it's only a console interface
    if (command_.isEmpty() && client_console_) {
        showClientMessage(helpText());
        return 0;
    }
#endif

    if (!command_.isEmpty())
        return runClient();

    if (channel_.lock()) {
        return runServer();
    } else {
        command_ = "open";
        return runClient();
    }
}

int TyQt::runClient()
{
    for (const ClientCommand *cmd = commands; cmd->name; cmd++) {
        if (command_ == cmd->name)
            return (this->*(cmd->f))();
    }

    showClientError(tr("Unknown command '%1'\n%2").arg(command_, helpText()));
    return 1;
}

int TyQt::sendRemoteCommand()
{
    if (!channel_.connectToMaster()) {
        showClientError(tr("Cannot connect to main TyQt instance"));
        return 1;
    }

    connect(&channel_, &SessionChannel::received, this, &TyQt::readAnswer);

    // Hack for Arduino integration, see getopt loop in TyQt::run()
    if (!usbtype_.isEmpty() && !usbtype_.contains("_SERIAL"))
        board_ = "";

    QStringList arguments = {command_, QDir::currentPath(), board_};
    for (int i = optind; i < argc_; i++)
        arguments.append(argv_[i]);
    channel_.send(arguments);

    connect(&channel_, &SessionChannel::masterClosed, this, [=]() {
        showClientError(tr("Main TyQt instance closed the connection"));
        exit(1);
    });

    return QApplication::exec();
}

int TyQt::integrateArduino()
{
    if (optind >= argc_) {
        showClientError(helpText());
        return 1;
    }

    ArduinoInstallation install(argv_[1]);

    connect(&install, &ArduinoInstallation::log, [](const QString &msg) {
        printf("%s\n", msg.toLocal8Bit().constData());
        fflush(stdout);
    });
    connect(&install, &ArduinoInstallation::error, [](const QString &msg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    });

    if (command_ == "integrate") {
        return !install.integrate();
    } else {
        return !install.restore();
    }
}

int TyQt::runServer()
{
    if (!channel_.lock()) {
        showClientError(tr("Cannot start main TyQt instance, lock file in place"));
        return 1;
    }

    connect(&channel_, &SessionChannel::received, this, &TyQt::executeAction);

    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_message_redirect([](ty_task *task, ty_message_type type, const void *data, void *udata) {
        ty_message_default_handler(task, type, data, udata);

        if (type == TY_MESSAGE_LOG) {
            auto print = static_cast<const ty_log_message *>(data);
            if (print->level >= TY_LOG_WARNING)
                tyQt->reportError(print->msg);
        }
    }, nullptr);

    tray_icon_.show();
    openMainWindow();

    if (!manager_.start()) {
        showClientError(ty_error_last_message());
        return 1;
    }

    if (!channel_.listen())
        reportError(tr("Failed to start session channel, single-instance mode won't work"));

    return QApplication::exec();
}

QString TyQt::helpText()
{
    QString help = tr("usage: tyqt <command> [options]\n\n"
                      "General options:\n"
                      "       --help               Show help message\n"
                      "       --version            Display version information\n\n"
                      "   -w, --wait               Wait until task completion\n"
                      "   -b, --board <tag>        Work with board <tag> instead of first detected\n"
                      "   -q, --quiet              Disable output, use -qqq to silence errors\n"
                      "       --experimental       Enable experimental features (use with caution)\n\n"
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
