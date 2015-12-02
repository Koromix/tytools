/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QFileInfo>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <QTimer>

#include "commands.hh"
#include "tyqt.hh"

using namespace std;

TyQt::TyQt(int &argc, char *argv[])
    : QApplication(argc, argv)
{
    setApplicationName("TyQt");
    setApplicationVersion(TY_VERSION);

    setupOptionParser(parser_);

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

    connect(&channel_, &SessionChannel::received, this, &TyQt::executeAction);
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

TyQt *TyQt::instance()
{
    return static_cast<TyQt *>(QCoreApplication::instance());
}

Manager *TyQt::manager()
{
    return &manager_;
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

    if (!client_console_ && main_windows_.empty())
        showClientError(msg);
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

bool TyQt::visible() const
{
    return action_visible_->isChecked();
}

void TyQt::setClientConsole(bool console)
{
    client_console_ = console;
}

bool TyQt::clientConsole() const
{
    return client_console_;
}

void TyQt::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifndef __APPLE__
    if (reason == QSystemTrayIcon::Trigger)
        setVisible(!visible());
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
        peer.send({"start"});
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
        if (!parser_.isSet("wait")) {
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

void TyQt::setupOptionParser(QCommandLineParser &parser)
{
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption waitOption(QStringList{"w", "wait"}, tr("Wait until task completion."));
    parser.addOption(waitOption);

    QCommandLineOption deviceOption(QStringList{"b", "board"}, tr("Work with specific board."),
                                    tr("id"));
    parser.addOption(deviceOption);

    QCommandLineOption uploadCommand(QStringList{"u", "upload"}, tr("Upload new firmware."),
                                     tr("firmware"));
    parser.addOption(uploadCommand);

    QCommandLineOption activateCommand("activate", tr("Bring TyQt to foreground."));
    parser.addOption(activateCommand);

    QCommandLineOption experimentalOption("experimental", tr("Enable experimental features (use with caution)."));
    parser.addOption(experimentalOption);
}

int TyQt::run()
{
    if (!parser_.parse(arguments())) {
        showClientError(QString("%1\n%2").arg(parser_.errorText(), parser_.helpText()));
        return 1;
    }

    if (parser_.isSet("version")) {
        showClientMessage(QString("%1 %2").arg(applicationName(), applicationVersion()));
        return 0;
    }
    if (parser_.isSet("help")) {
        showClientMessage(parser_.helpText());
        return 0;
    }

    if (!parser_.positionalArguments().isEmpty()) {
        showClientError(QString("%1\n%2").arg(tr("Positional arguments are not allowed."), parser_.helpText()));
        return 1;
    }

    unsigned int commandCount = parser_.isSet("activate") + parser_.isSet("upload");
    if (commandCount > 1) {
        showClientError(QString("%1\n%2").arg(tr("Multiple commands are not allowed."), parser_.helpText()));
        return 1;
    }

#ifdef _WIN32
    if (client_console_ && !commandCount) {
        showClientMessage(parser_.helpText());
        return 0;
    }
#endif

    if (parser_.isSet("experimental")) {
        ty_config_experimental = true;

#ifdef _WIN32
        _putenv("TY_EXPERIMENTAL=1");
#else
        setenv("TY_EXPERIMENTAL", "1", 1);
#endif
    }

    if (channel_.lock() && !commandCount) {
        return runServer();
    } else {
        channel_.disconnect(this);
        connect(&channel_, &SessionChannel::received, this, &TyQt::readAnswer);

        return runClient();
    }
}

int TyQt::runServer()
{
    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_message_redirect([](ty_task *task, ty_message_type type, const void *data, void *udata) {
        ty_message_default_handler(task, type, data, udata);

        if (type == TY_MESSAGE_LOG) {
            auto print = static_cast<const ty_log_message *>(data);
            if (print->level >= TY_LOG_WARNING)
                tyQt->reportError(print->msg);
        }
    }, nullptr);

    if (!manager_.start())
        return 1;

    tray_icon_.show();
    openMainWindow();

    if (!channel_.listen())
        reportError(tr("Failed to start session channel, single-instance mode won't work"));

    return QApplication::exec();
}

int TyQt::runClient()
{
    if (channel_.isLocked()) {
        channel_.unlock();

#ifdef _WIN32
        if (client_console_) {
            showClientError("Cannot find main TyQt instance");
            return 1;
        }
#endif

        if (!startBackgroundServer()) {
            showClientError(tr("Failed to start TyQt main instance"));
            return 1;
        }

        QThread::sleep(1);
    }

    if (!channel_.connectToMaster()) {
        showClientError(tr("Cannot connect to main TyQt instance"));
        return 1;
    }

    if (parser_.isSet("activate")) {
        channel_.send("activate");
    } else if (parser_.isSet("upload")) {
        auto tag = parser_.value("board");
        auto firmware = QFileInfo(parser_.value("upload")).canonicalFilePath();
        if (firmware.isEmpty()) {
            showClientError(tr("Firmware '%1' does not exist").arg(parser_.value("upload")));
            return 1;
        }

        channel_.send({"upload", tag, firmware});
    } else {
        channel_.send("open");
    }

    connect(&channel_, &SessionChannel::masterClosed, this, [=]() {
        showClientError(tr("Main TyQt instance closed the connection"));
        exit(1);
    });

    return QApplication::exec();
}

bool TyQt::startBackgroundServer()
{
    return QProcess::startDetached(applicationFilePath(), {});
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
