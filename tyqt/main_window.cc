/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolButton>
#include <QUrl>

#include "about_dialog.hh"
#include "arduino_dialog.hh"
#include "board.hh"
#include "board_widget.hh"
#include "commands.hh"
#include "main_window.hh"
#include "monitor.hh"
#include "tyqt.hh"

using namespace std;

MainWindow::MainWindow(Monitor *monitor, QWidget *parent)
    : QMainWindow(parent), monitor_(monitor)
{
    setupUi(this);

    menuUpload = new QMenu(this);
    menuUpload->addAction(actionUploadNew);
    menuUpload->addMenu(menuRecentFirmwares);

    menuBrowseFirmware = new QMenu(this);

    auto uploadButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(actionUpload));
    if (uploadButton) {
        uploadButton->setMenu(menuUpload);
        uploadButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    /* Only stretch the tab widget when resizing the window, I can't manage to replicate
       this with the Designer alone. */
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({1, 1});

    // Actions menu
    connect(actionUpload, &QAction::triggered, this, &MainWindow::uploadToSelection);
    connect(actionUploadNew, &QAction::triggered, this, &MainWindow::uploadNewToSelection);
    connect(actionReset, &QAction::triggered, this, &MainWindow::resetSelection);
    connect(actionReboot, &QAction::triggered, this, &MainWindow::rebootSelection);
    connect(actionQuit, &QAction::triggered, tyQt, &TyQt::quit);

    // View menu
    connect(actionNewWindow, &QAction::triggered, tyQt, &TyQt::openMainWindow);
    connect(actionMinimalInterface, &QAction::triggered, this, &MainWindow::setCompactMode);
    connect(actionClearMonitor, &QAction::triggered, this, &MainWindow::clearMonitor);

    // Tools menu
    connect(actionArduinoTool, &QAction::triggered, this, &MainWindow::openArduinoTool);
    connect(actionResetApp, &QAction::triggered, tyQt, &TyQt::resetMonitor);
    connect(actionResetSettingsApp, &QAction::triggered, this,
            &MainWindow::resetAppSettingsWithConfirmation);
    connect(actionOpenLog, &QAction::triggered, tyQt, &TyQt::openLogWindow);

    // About menu
    connect(actionWebsite, &QAction::triggered, &AboutDialog::openWebsite);
    connect(actionReportBug, &QAction::triggered, &AboutDialog::openBugReports);
    connect(actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);

    // Board list
    boardList->setModel(monitor);
    boardList->setItemDelegate(new BoardItemDelegate(monitor));
    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::selectionChanged);
    connect(monitor, &Monitor::boardAdded, this, [=]() {
        // Select this board if there were none available before
        if (monitor_->boardCount() == 1)
            boardList->setCurrentIndex(monitor_->index(0, 0));
    });
    // The blue selection frame displayed on OSX looks awful
    boardList->setAttribute(Qt::WA_MacShowFocusRect, false);

    // Monitor tab
    monitorText->setWordWrapMode(QTextOption::NoWrap);
    connect(monitorText, &QPlainTextEdit::updateRequest, this,
            &MainWindow::cacheMonitorScrollValues);
    connect(monitorText, &QPlainTextEdit::textChanged, this, &MainWindow::updateMonitorScroll);
    connect(monitorText, &QPlainTextEdit::customContextMenuRequested, this,
            &MainWindow::openMonitorContextMenu);
    connect(monitorEdit, &QLineEdit::returnPressed, this, &MainWindow::sendMonitorInput);
    connect(sendButton, &QToolButton::clicked, this, &MainWindow::sendMonitorInput);

    // Settings tab
    connect(firmwarePath, &QLineEdit::editingFinished, this, &MainWindow::validateAndSetFirmwarePath);
    connect(firmwareBrowseButton, &QToolButton::clicked, this, &MainWindow::browseForFirmware);
    firmwareBrowseButton->setMenu(menuBrowseFirmware);
    connect(actionAttachMonitor, &QAction::triggered, this,
            &MainWindow::setAttachMonitorForSelection);
    connect(resetAfterCheck, &QCheckBox::clicked, this, &MainWindow::setResetAfterForSelection);
    connect(clearOnResetCheck, &QCheckBox::clicked, this, &MainWindow::setClearOnResetForSelection);
    connect(scrollBackLimitSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setScrollBackLimitForSelection);

    if (monitor_->boardCount()) {
        boardList->setCurrentIndex(monitor_->index(0, 0));
    } else {
        disableBoardWidgets();
        refreshActions();
        updateFirmwareMenus();
    }
}

bool MainWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::StatusTip)
        return true;

    return QMainWindow::event(ev);
}

void MainWindow::showErrorMessage(const QString &msg)
{
    statusBar()->showMessage(msg, SHOW_ERROR_TIMEOUT);
}

void MainWindow::uploadToSelection()
{
    if (selected_boards_.empty())
        return;

    if (current_board_ && current_board_->firmware().isEmpty()) {
        uploadNewToSelection();
        return;
    }

    unsigned int fws_count = 0;
    for (auto &board: selected_boards_) {
        if (!board->firmware().isEmpty()) {
            fws_count++;
            board->startUpload();
        }
    }
    if (!fws_count)
        tyQt->reportError("No board has a set firmware, try using 'Upload New Firmware'");
}

void MainWindow::uploadNewToSelection()
{
    if (selected_boards_.empty())
        return;

    auto filenames = QFileDialog::getOpenFileNames(this, tr("Open Firmwares"),
                                                   browseFirmwareDirectory(),
                                                   browseFirmwareFilter());
    if (filenames.isEmpty())
        return;

    vector<shared_ptr<Firmware>> fws;
    fws.reserve(filenames.count());
    for (auto filename: filenames) {
        auto fw = Firmware::load(QDir::toNativeSeparators(filename));
        if (!fw)
            continue;

        fws.push_back(fw);
    }
    if (fws.empty())
        return;

    for (auto &board: selected_boards_)
        board->startUpload(fws);
}

void MainWindow::resetSelection()
{
    for (auto &board: selected_boards_)
        board->startReset();
}

void MainWindow::rebootSelection()
{
    for (auto &board: selected_boards_)
        board->startReboot();
}

void MainWindow::setCompactMode(bool enable)
{
    toolBar->setVisible(!enable);
    boardList->setVisible(!enable);
    statusbar->setVisible(!enable);
}

void MainWindow::openArduinoTool()
{
    if (!arduino_dialog_) {
        arduino_dialog_ = new ArduinoDialog(this);

        /* We don't want to open multiple dialogs (for each main window anyway), hence why
           we need to keep the pointer around. Unfortunately QPointer is broken with forward
           declarations, so we can't use that + WA_DeleteOnClose. */
        connect(arduino_dialog_, &QDialog::finished, this, [=]() {
            arduino_dialog_->deleteLater();
            arduino_dialog_ = nullptr;
        });
    }

    arduino_dialog_->show();
}

void MainWindow::resetAppSettingsWithConfirmation()
{
    QMessageBox msgbox;

    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setWindowTitle(tr("Reset Settings & TyQt"));
    msgbox.setText(tr("Reset will erase all your TyQt settings."));
    auto reset = msgbox.addButton(tr("Reset"), QMessageBox::AcceptRole);
    msgbox.addButton(QMessageBox::Cancel);
    msgbox.setDefaultButton(QMessageBox::Cancel);

    msgbox.exec();
    if (msgbox.clickedButton() != reset)
        return;

    tyQt->clearConfig();
    tyQt->resetMonitor();
}

void MainWindow::openAboutDialog()
{
    if (!about_dialog_) {
        about_dialog_ = new AboutDialog(this);

        // WA_DeleteOnClose is not enough, see openArduinoTool() for details
        connect(about_dialog_, &QDialog::finished, this, [=]() {
            about_dialog_->deleteLater();
            about_dialog_ = nullptr;
        });
    }

    about_dialog_->show();
}

void MainWindow::sendMonitorInput()
{
    auto s = monitorEdit->text();
    switch (newlineComboBox->currentIndex()) {
    case 1:
        s += '\n';
        break;
    case 2:
        s += '\r';
        break;
    case 3:
        s += "\r\n";
        break;
    default:
        break;
    }
    monitorEdit->clear();

    auto echo = echoCheck->isChecked();
    auto bytes = s.toUtf8();
    for (auto &board: selected_boards_) {
        if (echo)
            board->appendToSerialDocument(s);
        board->sendSerial(bytes);
    }
}

void MainWindow::clearMonitor()
{
    monitorText->clear();
}

void MainWindow::enableBoardWidgets()
{
    infoTab->setEnabled(true);
    monitorTab->setEnabled(true);
    actionClearMonitor->setEnabled(true);
    uploadTab->setEnabled(true);
    actionAttachMonitor->setEnabled(true);

    monitor_autoscroll_ = true;
    monitor_cursor_ = QTextCursor();
    monitorText->setDocument(&current_board_->serialDocument());
    monitorText->moveCursor(QTextCursor::End);
    monitorText->verticalScrollBar()->setValue(monitorText->verticalScrollBar()->maximum());
}

void MainWindow::disableBoardWidgets()
{
    firmwarePath->clear();
    resetAfterCheck->setChecked(false);
    clearOnResetCheck->setChecked(false);

    infoTab->setEnabled(false);
    idText->clear();
    statusText->clear();
    modelText->clear();
    locationText->clear();
    serialText->clear();
    interfaceTree->clear();

    monitorTab->setEnabled(false);
    actionClearMonitor->setEnabled(false);
    uploadTab->setEnabled(false);
    actionAttachMonitor->setEnabled(false);
}

void MainWindow::updateWindowTitle()
{
    if (current_board_) {
        setWindowTitle(QString("%1 | %2 | %3").arg(current_board_->tag(), current_board_->modelName(),
                                                   QCoreApplication::applicationName()));
    } else if (selected_boards_.size() > 0) {
        setWindowTitle(tr("%1 boards selected | %2").arg(selected_boards_.size())
                                                    .arg(QCoreApplication::applicationName()));
    } else {
        setWindowTitle(QCoreApplication::applicationName());
    }
}

void MainWindow::updateFirmwareMenus()
{
    // Start by restoring sane menus
    menuRecentFirmwares->clear();
    menuBrowseFirmware->clear();

    if (current_board_) {
        auto firmware = current_board_->firmware();

        for (auto &firmware: current_board_->recentFirmwares()) {
            QAction *action;

            action = menuRecentFirmwares->addAction(tr("Upload '%1'").arg(QFileInfo(firmware).fileName()));
            connect(action, &QAction::triggered, current_board_,
                    [=]() { current_board_->startUpload(firmware); });
            action = menuBrowseFirmware->addAction(tr("Set to '%1'").arg(firmware));
            connect(action, &QAction::triggered, current_board_,
                    [=]() { current_board_->setFirmware(firmware); });
        }
    }

    if (!menuRecentFirmwares->isEmpty()) {
        menuRecentFirmwares->setEnabled(true);
        menuBrowseFirmware->setEnabled(true);

        auto action = new QAction(tr("&Clear recent firmwares"), this);
        connect(action, &QAction::triggered, current_board_, &Board::clearRecentFirmwares);

        menuRecentFirmwares->addSeparator();
        menuRecentFirmwares->addAction(action);
        menuBrowseFirmware->addSeparator();
        menuBrowseFirmware->addAction(action);
    } else {
        menuRecentFirmwares->setEnabled(false);
        menuBrowseFirmware->setEnabled(false);
    }
}

QString MainWindow::browseFirmwareDirectory()
{
    if (selected_boards_.empty())
        return "";

    /* If only one board is selected, point to its current firmware by default. Otherwise, just
       show the directory of the first board's firmware without pre-selecting any file. */
    if (current_board_) {
        return current_board_->firmware();
    } else {
        return QFileInfo(selected_boards_[0]->firmware()).path();
    }
}

QString MainWindow::browseFirmwareFilter()
{
    QString exts;
    for (auto format = ty_firmware_formats; format->name; format++)
        exts += QString("*%1 ").arg(format->ext);
    exts.chop(1);

    return tr("Binary Files (%1);;All Files (*)").arg(exts);
}

void MainWindow::selectionChanged(const QItemSelection &newsel, const QItemSelection &previous)
{
    Q_UNUSED(newsel);
    Q_UNUSED(previous);

    for (auto &board: selected_boards_)
        board->disconnect(this);
    monitorText->setDocument(nullptr);
    selected_boards_.clear();
    current_board_ = nullptr;

    for (auto &idx: boardList->selectionModel()->selectedIndexes()) {
        if (idx.column() == 0)
            selected_boards_.push_back(monitor_->board(idx.row()));
    }

    if (selected_boards_.size() == 1) {
        current_board_ = selected_boards_.front().get();

        connect(current_board_, &Board::interfacesChanged, this, &MainWindow::refreshActions);
        connect(current_board_, &Board::infoChanged, this, &MainWindow::refreshInfo);
        connect(current_board_, &Board::settingsChanged, this, &MainWindow::refreshSettings);
        connect(current_board_, &Board::interfacesChanged, this, &MainWindow::refreshInterfaces);
        connect(current_board_, &Board::statusChanged, this, &MainWindow::refreshStatus);

        enableBoardWidgets();
        refreshActions();
        refreshInfo();
        refreshSettings();
        refreshInterfaces();
        refreshStatus();
    } else {
        for (auto &board: selected_boards_)
            connect(board.get(), &Board::interfacesChanged, this, &MainWindow::refreshActions);

        disableBoardWidgets();
        refreshActions();
        updateWindowTitle();
        updateFirmwareMenus();
    }
}

void MainWindow::refreshActions()
{
    bool upload = false, reset = false, reboot = false;
    for (auto &board: selected_boards_) {
        upload |= board->uploadAvailable();
        reset |= board->resetAvailable();
        reboot |= board->rebootAvailable();
    }

    actionUpload->setEnabled(upload);
    actionUploadNew->setEnabled(upload);
    actionReset->setEnabled(reset);
    actionReboot->setEnabled(reboot);
}

void MainWindow::refreshInfo()
{
    updateWindowTitle();

    idText->setText(current_board_->id());
    modelText->setText(current_board_->modelName());
    locationText->setText(current_board_->location());
    serialText->setText(QString::number(current_board_->serialNumber()));
}

void MainWindow::refreshSettings()
{
    actionAttachMonitor->setChecked(current_board_->attachMonitor());
    monitorEdit->setEnabled(current_board_->serialOpen());

    firmwarePath->setText(current_board_->firmware());
    resetAfterCheck->setChecked(current_board_->resetAfter());
    clearOnResetCheck->setChecked(current_board_->clearOnReset());
    scrollBackLimitSpin->blockSignals(true);
    scrollBackLimitSpin->setValue(current_board_->scrollBackLimit());
    scrollBackLimitSpin->blockSignals(false);

    updateFirmwareMenus();
}

void MainWindow::refreshInterfaces()
{
    interfaceTree->clear();
    for (auto &iface: current_board_->interfaces()) {
        auto title = tr("%1 %2").arg(iface.name, iface.open ? tr("(open)") : "");

        auto item = new QTreeWidgetItem();
        item->setText(0, title);
        item->setText(1, iface.path);

        auto tooltip = tr("%1\n+ Location: %2\n+ Interface Number: %3\n+ Capabilities: %4")
                       .arg(title).arg(iface.path).arg(iface.number)
                       .arg(Board::makeCapabilityList(iface.capabilities).join(", "));
        item->setToolTip(0, tooltip);
        item->setToolTip(1, tooltip);

        interfaceTree->addTopLevelItem(item);
    }

    monitorEdit->setEnabled(current_board_->serialOpen());
}

void MainWindow::refreshStatus()
{
    statusText->setText(current_board_->statusText());
}

/* Memorize the scroll value whenever the user scrolls the widget (QPlainTextEdit::updateRequest)
   and enable autoscroll when he scrolls to the end. */
void MainWindow::cacheMonitorScrollValues(const QRect &rect, int dy)
{
    Q_UNUSED(rect);

    if (!dy)
        return;

    auto vbar = monitorText->verticalScrollBar();
    /* Disable autoscroll when the user scrolls in the document, unless he scrolls to
       end of the document. */
    monitor_autoscroll_ = vbar->value() >= vbar->maximum() - 1;
    monitor_cursor_ = monitorText->cursorForPosition(QPoint(0, 0));
}

/* Fix the scrollbar position whenever the text changes (QPlainTextEdit::textChanged),
   depending on whether autoscroll is enabled or not. */
void MainWindow::updateMonitorScroll()
{
    auto hbar = monitorText->horizontalScrollBar();
    auto vbar = monitorText->verticalScrollBar();

    // Look at monitorTextScrolled() for the rules regarding monitor_autoscroll_
    if (monitor_autoscroll_) {
        vbar->setValue(vbar->maximum());
    } else {
        /* QPlainTextEdit does a good job of keeping the text steady when we append to the
           end... until maximumBlockCount kicks in. We use our own cursor monitor_cursor_,
           updated in cacheMonitorScrollValues() to fix that behavior. */
        QTextCursor old_cursor = monitorText->textCursor();
        int hpos = hbar->value();

        monitorText->setTextCursor(monitor_cursor_);
        monitorText->ensureCursorVisible();
        int vpos = vbar->value();

        monitorText->setTextCursor(old_cursor);
        hbar->setValue(hpos);
        vbar->setValue(vpos);
    }
}

void MainWindow::openMonitorContextMenu(const QPoint &pos)
{
    auto menu = monitorText->createStandardContextMenu();
    menu->addAction(actionClearMonitor);
    menu->exec(monitorText->viewport()->mapToGlobal(pos));
}

void MainWindow::validateAndSetFirmwarePath()
{
    if (selected_boards_.empty())
        return;

    QString filename;
    if (!firmwarePath->text().isEmpty()) {
        filename = QFileInfo(firmwarePath->text()).canonicalFilePath();
        if (filename.isEmpty()) {
            tyQt->reportError(tr("Path '%1' does not exist").arg(firmwarePath->text()));
            return;
        }
        filename = QDir::toNativeSeparators(filename);
    }

    for (auto &board: selected_boards_)
        board->setFirmware(filename);
}

void MainWindow::browseForFirmware()
{
    if (selected_boards_.empty())
        return;

    auto filename = QFileDialog::getOpenFileName(this, tr("Open Firmware"),
                                                 browseFirmwareDirectory(),
                                                 browseFirmwareFilter());
    if (filename.isEmpty())
        return;
    filename = QDir::toNativeSeparators(filename);

    for (auto &board: selected_boards_)
        board->setFirmware(filename);
}

void MainWindow::setResetAfterForSelection(bool reset_after)
{
    for (auto &board: selected_boards_)
        board->setResetAfter(reset_after);
}

void MainWindow::setClearOnResetForSelection(bool clear_on_reset)
{
    for (auto &board: selected_boards_)
        board->setClearOnReset(clear_on_reset);
}

void MainWindow::setScrollBackLimitForSelection(int limit)
{
    for (auto &board: selected_boards_)
        board->setScrollBackLimit(limit);
}

void MainWindow::setAttachMonitorForSelection(bool attach_monitor)
{
    for (auto &board: selected_boards_)
        board->setAttachMonitor(attach_monitor);
}
