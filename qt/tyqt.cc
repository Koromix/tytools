/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include "tyqt.hh"

using namespace std;

TyQt::TyQt(int &argc, char *argv[])
    : QApplication(argc, argv)
{
    setApplicationName("TyQt");
    setApplicationVersion(TY_VERSION);

    setupOptionParser(parser_);

    // This can be triggered from multiple threads, but Qt can queue signals appropriately
    ty_error_redirect([](ty_err err, const char *msg, void *udata) {
        TY_UNUSED(err);
        TY_UNUSED(udata);

        tyQt->reportError(msg);
    }, nullptr);

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

    ty_error_redirect(nullptr, nullptr);
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
            tyQt->quit();
    });
    main_windows_.push_back(win);

    connect(this, &TyQt::errorMessage, win, &MainWindow::showErrorMessage);

    win->show();
}

void TyQt::activateMainWindow()
{
    if (tyQt->main_windows_.empty())
        return;

    auto win = main_windows_.front();
    win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
    win->raise();
    win->activateWindow();
}

void TyQt::reportError(const QString &msg)
{
    fprintf(stderr, "%s\n", qPrintable(msg));
    last_error_ = msg;

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
    switch (reason) {
#ifndef __APPLE__
    case QSystemTrayIcon::Trigger:
        setVisible(!visible());
        break;
#endif

    default:
        break;
    }
}

void TyQt::executeAction(SessionPeer &peer, const QStringList &arguments)
{
    if (arguments.isEmpty()) {
        peer.send(tr("Command not specified"));
        return;
    }

    auto command = arguments[0];

    if (command == "new") {
        openMainWindow();
        peer.send("OK");
    } else if (command == "activate") {
        if (!main_windows_.empty()) {
            auto win = main_windows_.front();

            win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
            win->raise();
            win->activateWindow();
        }

        peer.send("OK");
    } else if (command == "upload") {
        if (arguments.count() < 3) {
            peer.send(tr("Not enough arguments for command 'upload'"));
            return;
        }

        if (!manager_.boardCount()) {
            peer.send(tr("No board available"));
            return;
        }

        auto firmware = arguments[1];
        auto tag = arguments[2];

        shared_ptr<Board> board;

        if (!tag.isEmpty()) {
            board = getBoard([=](Board &board) { return board.matchesTag(tag); }, false);
            if (!board) {
                peer.send(tr("Board '%1' not found").arg(tag));
                return;
            }

            peer.send("OK");
        } else {
            // Don't let the client wait because a selector dialog may show up
            peer.send("OK");

            board = getBoard([=](Board &board) { return board.property("firmware") == firmware; }, true);
            if (!board)
                return;
        }

        board->setProperty("firmware", firmware);
        board->upload(firmware, board->property("resetAfter").toBool());
    } else {
        peer.send(tr("Unknown command '%1'").arg(command));
    }
}

void TyQt::readAnswer(SessionPeer &peer, const QStringList &arguments)
{
    TY_UNUSED(peer);

    if (arguments.isEmpty()) {
        showClientError(tr("Got empty answer from main TyQt instance"));
        exit(1);
    }

    if (arguments[0] == "OK") {
        quit();
    } else {
        for (auto line: arguments)
            showClientError(line);
        exit(1);
    }
}

void TyQt::setupOptionParser(QCommandLineParser &parser)
{
    parser.addHelpOption();
    parser.addVersionOption();

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
    if (!manager_.start()) {
        QMessageBox::critical(nullptr, tr("TyQt (critical error)"), last_error_, QMessageBox::Close);
        return 1;
    }

    QThreadPool::globalInstance()->setMaxThreadCount(16);

    tray_icon_.show();
    openMainWindow();

    if (!channel_.listen())
        tyQt->reportError(tr("Failed to start session channel, single-instance mode won't work"));

    return QApplication::exec();
}

int TyQt::runClient()
{
    if (channel_.isLocked()) {
        channel_.unlock();

#ifdef _WIN32
        if (client_console_) {
            showClientError("Cannot find main TyQt instance");
            return false;
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
        auto firmware = QFileInfo(parser_.value("upload")).canonicalFilePath();
        if (firmware.isEmpty()) {
            showClientError(tr("Firmware '%1' does not exist").arg(parser_.value("upload")));
            return 1;
        }
        auto tag = parser_.value("board");

        channel_.send({"upload", firmware, tag});
    } else {
        channel_.send("new");
    }

    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&timeout, &QTimer::timeout, this, [=]() {
        showClientError(tr("Main TyQt instance is not responding"));
        exit(1);
    });
    timeout.start(3000);

    return QApplication::exec();
}

shared_ptr<Board> TyQt::getBoard(function<bool(Board &board)> filter, bool show_selector)
{
    auto board = find_if(manager_.begin(), manager_.end(), [&](std::shared_ptr<Board> &ptr) { return filter(*ptr); });

    if (board == manager_.end()) {
        if (show_selector && manager_.boardCount()) {
            return SelectorDialog::getBoard(&manager_, main_windows_.front());
        } else {
            return nullptr;
        }
    }

    return *board;
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
