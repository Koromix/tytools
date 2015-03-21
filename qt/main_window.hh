/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MAIN_WINDOW_HH
#define MAIN_WINDOW_HH

#include "board.hh"
#include "ui_main_window.h"

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    Manager *manager_;
    std::shared_ptr<Board> current_board_;

    bool monitor_autoscroll_ = true;
    QTextCursor monitor_cursor_;

public:
    MainWindow(Manager *manager, QWidget *parent = nullptr);

public slots:
    void showErrorMessage(const QString &msg);

private:
    void disableBoardWidgets();

    QString browseForFirmware();
    void uploadCurrentFirmware();

private slots:
    void setBoardDefaults(std::shared_ptr<Board> board);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void refreshBoardInfo();

    void updatePropertyField(const char *name, const QVariant &value);

    void monitorTextChanged();
    void monitorTextScrolled(const QRect &rect, int dy);

    void clearMonitor();

    void on_firmwarePath_editingFinished();
    void on_resetAfterUpload_toggled(bool checked);

    void on_actionNewWindow_triggered();

    void on_actionUpload_triggered();
    void on_actionUploadNew_triggered();
    void on_actionReset_triggered();
    void on_actionReboot_triggered();
    void on_monitorEdit_returnPressed();
    void on_clearOnReset_toggled(bool checked);

    void on_actionMinimalInterface_toggled(bool checked);

    void on_browseButton_clicked();

    void on_monitorText_customContextMenuRequested(const QPoint &pos);
    void on_logText_customContextMenuRequested(const QPoint &pos);

    void on_actionWebsite_triggered();
    void on_actionReportBug_triggered();
    void on_actionAbout_triggered();
};

#endif
