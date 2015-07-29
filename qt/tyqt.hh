/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TYQT_HH
#define TYQT_HH

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QSystemTrayIcon>

#include <functional>
#include <memory>

#include "board.hh"
#include "main_window.hh"
#include "session_channel.hh"

#define tyQt (TyQt::instance())

class TyQt : public QApplication {
    Q_OBJECT

    QCommandLineParser parser_;

    SessionChannel channel_;

    QString last_error_;

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

    bool visible() const;

    void setClientConsole(bool console);
    bool clientConsole() const;

public slots:
    void newMainWindow();
    void reportError(const QString &msg);

    void setVisible(bool visible);

signals:
    void errorMessage(const QString &msg);

private:
    void setupOptionParser(QCommandLineParser &parser);

    int run();
    int runServer();
    int runClient();

    std::shared_ptr<Board> getBoard(std::function<bool(Board &board)> filter, bool show_selector);

    bool startBackgroundServer();
    void showClientMessage(const QString &msg);
    void showClientError(const QString &msg);

private slots:
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

    void executeAction(SessionPeer &peer, const QStringList &arguments);
    void readAnswer(SessionPeer &peer, const QStringList &arguments);
};

#endif
