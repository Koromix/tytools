/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef MAIN_WINDOW_HH
#define MAIN_WINDOW_HH

#include <memory>
#include <vector>

#include "ui_main_window.h"

class AboutDialog;
class ArduinoDialog;
class Board;
class Monitor;

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    Monitor *monitor_;
    std::shared_ptr<Board> current_board_;
    std::vector<std::shared_ptr<Board>> selected_boards_;

    bool monitor_autoscroll_ = true;
    QTextCursor monitor_cursor_;

    ArduinoDialog *arduino_dialog_ = nullptr;
    AboutDialog *about_dialog_ = nullptr;

public:
    MainWindow(Monitor *monitor, QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

    std::shared_ptr<Board> currentBoard() const { return current_board_; }
    std::vector<std::shared_ptr<Board>> selectedBoards() const { return selected_boards_; }

    bool compactMode() const { return !boardList->isVisible(); }

public slots:
    void showErrorMessage(const QString &msg);

    void uploadToSelection();
    void uploadNewToSelection();
    void resetSelection();
    void rebootSelection();

    void setCompactMode(bool enable);

    void openArduinoTool();
    void resetAppSettingsWithConfirmation();
    void openAboutDialog();

    void sendMonitorInput();
    void clearMonitor();

    static QString fileDialogFirmwareFilter();

private:
    void selectFirstBoard();

    void enableBoardWidgets();
    void disableBoardWidgets();
    void updateWindowTitle();

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);

    void refreshActions();
    void refreshInfo();
    void refreshSettings();
    void refreshInterfaces();
    void refreshStatus();

    void cacheMonitorScrollValues(const QRect &rect, int dy);
    void updateMonitorScroll();
    void openMonitorContextMenu(const QPoint &pos);

    void validateAndSetFirmwarePath();
    void browseForFirmware();

    void setResetAfterForSelection(bool reset_after);
    void setClearOnResetForSelection(bool clear_on_reset);
    void setScrollBackLimitForSelection(int limit);
    void setAttachMonitorForSelection(bool attach_monitor);
};

#endif
