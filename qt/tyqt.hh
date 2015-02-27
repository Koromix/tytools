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

#include "board_proxy.hh"
#include "main_window.hh"
#include "session_channel.hh"

#define tyQt (TyQt::instance())

class TyQt : public QApplication {
    Q_OBJECT

    QCommandLineParser parser_;

    SessionChannel channel_;
    bool server_;

    QString last_error_;

    BoardManagerProxy manager_;

    std::vector<MainWindow *> main_windows_;

    QAction *action_visible_;
    QAction *action_quit_;
    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;

public:
    TyQt(int &argc, char *argv[]);
    virtual ~TyQt();

    static int exec();

    static TyQt *instance();

    bool visible();

public slots:
    void newMainWindow();
    void reportError(const QString &msg);

    void setVisible(bool visible);

signals:
    void errorMessage(const QString &msg);

private slots:
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

    void executeAction(SessionPeer &peer, const QStringList &arguments);
    void readAnswer(SessionPeer &peer, const QStringList &arguments);

private:
    void setupOptionParser(QCommandLineParser &parser);

    void initServer();
    int execServer();

    std::shared_ptr<BoardProxy> getBoard(std::function<bool(BoardProxy &board)> filter, bool show_selector);

    void initClient();
    int execClient();

    bool startBackgroundServer();
    void showClientMessage(const QString &msg);
    void showClientError(const QString &msg);

};

#endif
