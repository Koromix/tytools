/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TYQT_HH
#define TYQT_HH

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QSystemTrayIcon>

#include <memory>

#include "board.hh"
#include "main_window.hh"
#include "selector_dialog.hh"
#include "session_channel.hh"

#define tyQt (TyQt::instance())

class TyQt : public QApplication {
    Q_OBJECT

    QCommandLineParser parser_;

    SessionChannel channel_;

    Manager manager_;

    std::vector<MainWindow *> main_windows_;

    QAction *action_visible_;
    QAction *action_quit_;
    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;

#ifdef _WIN32
    bool client_console_ = false;
#else
    bool client_console_ = true;
#endif

public:
    TyQt(int &argc, char *argv[]);
    virtual ~TyQt();

    static int exec();

    static TyQt *instance();

    Manager *manager();
    SelectorDialog *openSelector();

    bool visible() const;

    void setClientConsole(bool console);
    bool clientConsole() const;

public slots:
    void openMainWindow();
    void activateMainWindow();

    void reportError(const QString &msg);

    void setVisible(bool visible);

signals:
    void errorMessage(const QString &msg);

private:
    void setupOptionParser(QCommandLineParser &parser);

    int run();
    int runServer();
    int runClient();

    bool startBackgroundServer();
    void showClientMessage(const QString &msg);
    void showClientError(const QString &msg);

private slots:
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

    void executeAction(SessionPeer &peer, const QStringList &arguments);
    void readAnswer(SessionPeer &peer, const QStringList &arguments);
};

#endif
