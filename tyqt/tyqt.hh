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
#include <QSystemTrayIcon>

#include <memory>

#include "board.hh"
#include "main_window.hh"
#include "selector_dialog.hh"
#include "session_channel.hh"

#define tyQt (TyQt::instance())

#define SHOW_ERROR_TIMEOUT 5000

class TyQt : public QApplication {
    Q_OBJECT

    int argc_;
    char **argv_;
    QString command_;
    QString board_;
    bool wait_ = false;
    QString usbtype_;

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

    static TyQt *instance() { return static_cast<TyQt *>(QCoreApplication::instance()); }
    Manager *manager() { return &manager_; }

    SelectorDialog *openSelector();

    bool visible() const { return action_visible_->isChecked(); }

    void setClientConsole(bool console) { client_console_ = console; }
    bool clientConsole() const { return client_console_; }

    int run();
    int runClient();
    int sendRemoteCommand();

    int runServer();

public slots:
    void openMainWindow();
    void activateMainWindow();

    void reportError(const QString &msg);

    void setVisible(bool visible);

signals:
    void errorMessage(const QString &msg);

private:
    QString helpText();
    void showClientMessage(const QString &msg);
    void showClientError(const QString &msg);

private slots:
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

    void executeAction(SessionPeer &peer, const QStringList &arguments);
    void readAnswer(SessionPeer &peer, const QStringList &arguments);
};

#endif
