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
#include <QMenu>
#include <QSystemTrayIcon>

#include <memory>

#include "database.hh"
#include "monitor.hh"
#include "session_channel.hh"

class LogDialog;
class MainWindow;
class SelectorDialog;

#define tyQt (TyQt::instance())

#define SHOW_ERROR_TIMEOUT 5000

class TyQt : public QApplication {
    Q_OBJECT

    int argc_;
    char **argv_;
    QString command_;
    bool wait_ = false;

    SessionChannel channel_;

    Monitor monitor_;

    SettingsDatabase tyqt_db_;
    SettingsDatabase monitor_db_;

    DatabaseInterface db_;

    QAction *action_visible_;
    QAction *action_quit_;
    QSystemTrayIcon tray_icon_;
    QMenu tray_menu_;

#ifdef _WIN32
    bool client_console_ = false;
#else
    bool client_console_ = true;
#endif

    bool show_tray_icon_;
    bool hide_on_startup_;

    std::unique_ptr<LogDialog> log_dialog_;

public:
    TyQt(int &argc, char *argv[]);
    virtual ~TyQt();

    void setDatabase(DatabaseInterface db) { db_ = db; }
    DatabaseInterface database() const { return db_; }
    void loadSettings();

    static int exec();

    QString clientFilePath() const;

    static TyQt *instance() { return static_cast<TyQt *>(QCoreApplication::instance()); }
    Monitor *monitor() { return &monitor_; }

    SelectorDialog *openSelector(const QString &action = QString(), const QString &desc = QString());

    MainWindow *getMainWindow() const;

    bool visible() const { return action_visible_->isChecked(); }

    void setClientConsole(bool console) { client_console_ = console; }
    bool clientConsole() const { return client_console_; }

    bool showTrayIcon() const { return show_tray_icon_; }
    bool hideOnStartup() const { return hide_on_startup_; }

    int run(int argc, char *argv[]);
    int runMainInstance(int argc, char *argv[]);
    int executeRemoteCommand(int argc, char *argv[]);
    int integrateArduino(int argc, char *argv[]);
    int fakeAvrdudeUpload(int argc, char *argv[]);

    void resetMonitor();
    void clearSettingsAndReset();
    void clearSettingsAndResetWithConfirmation(QWidget *parent = nullptr);

public slots:
    void openMainWindow();
    void activateMainWindow(MainWindow *win = nullptr);
    void showLogWindow();

    void reportError(const QString &msg);
    void reportDebug(const QString &msg);

    void setShowTrayIcon(bool show_tray_icon);
    void setHideOnStartup(bool hide_on_startup);

    void setVisible(bool visible);

signals:
    void settingsChanged();

    void globalError(const QString &msg);
    void globalDebug(const QString &msg);

private:
    void initDatabase(const QString &name, SettingsDatabase &db);

    QString helpText();
    void showClientMessage(const QString &msg);
    void showClientError(const QString &msg);

private slots:
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

    void executeAction(SessionPeer &peer, const QStringList &arguments);
    void readAnswer(SessionPeer &peer, const QStringList &arguments);
};

#endif
