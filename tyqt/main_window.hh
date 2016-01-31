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

class Board;
class Monitor;

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    Monitor *monitor_;
    std::shared_ptr<Board> current_board_;
    std::vector<std::shared_ptr<Board>> selected_boards_;

    bool monitor_autoscroll_ = true;
    QTextCursor monitor_cursor_;

public:
    MainWindow(Monitor *monitor, QWidget *parent = nullptr);

    bool event(QEvent *ev) override;

public slots:
    void showErrorMessage(const QString &msg);

private:
    static QString makeFirmwareFilter();

    void updateWindowTitle();
    void selectFirstBoard();

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void refreshBoardsInfo();
    void updateSettingField(const QString &name, const QVariant &value);

    void monitorTextChanged();
    void monitorTextScrolled(const QRect &rect, int dy);

    void clearMonitor();

    void on_firmwarePath_editingFinished();
    void on_resetAfterUpload_clicked(bool checked);

    void on_actionNewWindow_triggered();

    void on_actionUpload_triggered();
    void on_actionUploadNew_triggered();
    void on_actionReset_triggered();
    void on_actionReboot_triggered();

    void on_monitorEdit_returnPressed();
    void on_sendButton_clicked();
    void on_clearOnReset_clicked(bool checked);
    void on_scrollBackLimitSpin_valueChanged(int value);
    void on_actionAttachMonitor_triggered(bool checked);
    void on_actionClearMonitor_triggered();

    void on_actionMinimalInterface_toggled(bool checked);

    void on_firmwareBrowseButton_clicked();

    void on_monitorText_customContextMenuRequested(const QPoint &pos);
    void on_logText_customContextMenuRequested(const QPoint &pos);

    void on_actionResetApp_triggered();
    void on_actionResetSettingsApp_triggered();

    void on_actionWebsite_triggered();
    void on_actionReportBug_triggered();
    void on_actionIntegrateToArduino_triggered();
    void on_actionAbout_triggered();
};

#endif
