/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef MAIN_WINDOW_HH
#define MAIN_WINDOW_HH

#include <QHash>
#include <QStringList>

#include <memory>
#include <vector>

#include "ui_main_window.h"

class AboutDialog;
class ArduinoDialog;
class Board;
class Monitor;

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    static QStringList codecs_;
    static QHash<QString, int> codec_indexes_;

    QMenu *menuUpload;
    QMenu *menuBrowseFirmware;
    QMenu *menuBoardContext;

#ifdef __APPLE__
    // See MainWindow::MainWindow() in main_window.cc for more information about that
    QMenu *menuRecentFirmwares2;
    QMenu *menuRecentFirmwares3;
#endif

    QMenu *menuMonitorOptions;
    QAction *actionMonitorEcho;
    QActionGroup *actionMonitorEOLGroup;

    QComboBox *boardComboBox;
    // We need to keep this around to show/hide the board QComboBox
    QAction *boardComboAction;

    Monitor *monitor_;
    std::vector<std::shared_ptr<Board>> selected_boards_;
    Board *current_board_ = nullptr;

    ArduinoDialog *arduino_dialog_ = nullptr;
    AboutDialog *about_dialog_ = nullptr;

public:
    MainWindow(QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

    std::vector<std::shared_ptr<Board>> selectedBoards() const { return selected_boards_; }
    Board *currentBoard() const { return current_board_; }

    bool compactMode() const { return !boardList->isVisible(); }

public slots:
    void showErrorMessage(const QString &msg);

    void uploadToSelection();
    void uploadNewToSelection();
    void dropAssociationForSelection();
    void resetSelection();
    void rebootSelection();

    void setCompactMode(bool enable);

    void openCloneWindow();
    void openArduinoTool();
    void openPreferences();
    void openAboutDialog();

    void sendMonitorInput();
    void clearMonitor();

private:
    static void initCodecList();

    void enableBoardWidgets();
    void disableBoardWidgets();
    void updateWindowTitle();
    void updateFirmwareMenus();

    QString browseFirmwareDirectory() const;
    QString browseFirmwareFilter() const;

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void openBoardListContextMenu(const QPoint &pos);

    void refreshActions();
    void refreshInfo();
    void refreshSettings();
    void refreshInterfaces();
    void refreshStatus();

    void openMonitorContextMenu(const QPoint &pos);

    void validateAndSetFirmwarePath();
    void browseForFirmware();

    void setResetAfterForSelection(bool reset_after);
    void setSerialCodecForSelection(const QString &codec_name);
    void setClearOnResetForSelection(bool clear_on_reset);
    void setScrollBackLimitForSelection(int limit);
    void setAttachMonitorForSelection(bool attach_monitor);
};

#endif
