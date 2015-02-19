/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MAIN_WINDOW_HH
#define MAIN_WINDOW_HH

#include "board_proxy.hh"
#include "ui_main_window.h"

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT

    BoardManagerProxy *manager_;
    std::shared_ptr<BoardProxy> current_board_;

    QString last_error_;

public:
    MainWindow(BoardManagerProxy *manager, QWidget *parent = nullptr);
    virtual ~MainWindow();

    QString lastError() const;

signals:
    void errorMessage(const QString &msg);

private:
    void disableBoardWidgets();

    QString browseForFirmware();
    void uploadCurrentFirmware();

private slots:
    void setBoardDefaults(std::shared_ptr<BoardProxy> board);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &previous);
    void refreshBoardInfo();

    void monitorTextChanged();

    void showErrorMessage(const QString &msg);

    void on_firmwarePath_editingFinished();
    void on_resetAfterUpload_toggled(bool checked);

    void on_actionUpload_triggered();
    void on_actionUploadNew_triggered();
    void on_actionReset_triggered();
    void on_actionReboot_triggered();
    void on_monitorEdit_returnPressed();

    void on_browseButton_clicked();

    void on_monitorText_customContextMenuRequested(const QPoint &pos);
    void on_logText_customContextMenuRequested(const QPoint &pos);

    void on_actionWebsite_triggered();
    void on_actionReportBug_triggered();
    void on_actionAbout_triggered();
};

#endif
