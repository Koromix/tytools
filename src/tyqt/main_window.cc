/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDesktopServices>
#include <QFileDialog>
#include <QScrollBar>
#include <QTextCodec>
#include <QToolButton>
#include <QUrl>

#include "about_dialog.hpp"
#include "arduino_dialog.hpp"
#include "tyqt/board.hpp"
#include "tyqt/board_widget.hpp"
#include "commands.hpp"
#include "main_window.hpp"
#include "tyqt/monitor.hpp"
#include "preferences_dialog.hpp"
#include "tyqt.hpp"

using namespace std;

QStringList MainWindow::codecs_;
QHash<QString, int> MainWindow::codec_indexes_;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), monitor_(tyQt->monitor())
{
    setupUi(this);

#ifdef __APPLE__
    /* Workaround for Qt OSX bug https://bugreports.qt.io/browse/QTBUG-34160
       The actions in menuRecentFirmwares are copied to menuRecentFirmwares2
       in updateFirmwareMenus(). */
    menuRecentFirmwares2 = new QMenu(menuRecentFirmwares->title(), this);
    menuRecentFirmwares3 = new QMenu(menuRecentFirmwares->title(), this);
#endif

    menuUpload = new QMenu(this);
    menuUpload->addAction(actionUploadNew);
    menuUpload->addAction(actionDropFirmware);
#ifdef __APPLE__
    menuUpload->addMenu(menuRecentFirmwares2);
#else
    menuUpload->addMenu(menuRecentFirmwares);
#endif

    menuBrowseFirmware = new QMenu(this);

    menuBoardContext = new QMenu(this);
    menuBoardContext->addAction(actionUpload);
    menuBoardContext->addAction(actionUploadNew);
    menuBoardContext->addAction(actionDropFirmware);
#ifdef __APPLE__
    menuBoardContext->addMenu(menuRecentFirmwares3);
#else
    menuBoardContext->addMenu(menuRecentFirmwares);
#endif
    menuBoardContext->addSeparator();
    menuBoardContext->addAction(actionReset);
    menuBoardContext->addAction(actionReboot);
    menuBoardContext->addSeparator();
    menuBoardContext->addAction(actionRenameBoard);

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

    // We want all action shortcuts to remain available when the menu bar is hidden
    addActions(menubar->actions());

    // Actions menu
    connect(actionUpload, &QAction::triggered, this, &MainWindow::uploadToSelection);
    connect(actionUploadNew, &QAction::triggered, this, &MainWindow::uploadNewToSelection);
    connect(actionDropFirmware, &QAction::triggered, this,
            &MainWindow::dropAssociationForSelection);
    connect(actionReset, &QAction::triggered, this, &MainWindow::resetSelection);
    connect(actionReboot, &QAction::triggered, this, &MainWindow::rebootSelection);
    connect(actionQuit, &QAction::triggered, tyQt, &TyQt::quit);

    // View menu
    connect(actionNewWindow, &QAction::triggered, this, &MainWindow::openCloneWindow);
    connect(actionMinimalInterface, &QAction::triggered, this, &MainWindow::setCompactMode);
    connect(actionClearMonitor, &QAction::triggered, this, &MainWindow::clearMonitor);

    // Tools menu
    connect(actionArduinoTool, &QAction::triggered, this, &MainWindow::openArduinoTool);
    connect(actionOpenLog, &QAction::triggered, tyQt, &TyQt::showLogWindow);
    connect(actionResetApp, &QAction::triggered, tyQt, &TyQt::resetMonitor);
    connect(actionResetSettingsApp, &QAction::triggered, this,
            [=]() { tyQt->clearSettingsAndResetWithConfirmation(this); });
    connect(actionPreferences, &QAction::triggered, this, &MainWindow::openPreferences);

    // About menu
    connect(actionWebsite, &QAction::triggered, &AboutDialog::openWebsite);
    connect(actionReportBug, &QAction::triggered, &AboutDialog::openBugReports);
    connect(actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);

    // Board list
    boardList->setModel(monitor_);
    boardList->setItemDelegate(new BoardItemDelegate(monitor_));
    connect(boardList, &QListView::customContextMenuRequested, this,
            &MainWindow::openBoardListContextMenu);
    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::selectionChanged);
    connect(monitor_, &Monitor::boardAdded, this, [=]() {
        // Select this board if there were none available before
        if (monitor_->boardCount() == 1)
            boardList->setCurrentIndex(monitor_->index(0, 0));
    });
    // The blue selection frame displayed on OSX looks awful
    boardList->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(actionRenameBoard, &QAction::triggered, this, [=]() {
        boardList->edit(boardList->currentIndex());
    });

    // Board dropdown (compact mode)
    boardComboBox = new QComboBox(this);
    setTabOrder(boardList, boardComboBox);
    boardComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    boardComboBox->setMinimumContentsLength(12);
    boardComboBox->setModel(monitor_);
    boardComboBox->setVisible(false);
    auto spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolBar->addWidget(spacer);
#ifdef __APPLE__
    boardComboAction = nullptr;
#else
    boardComboAction = toolBar->addWidget(boardComboBox);
#endif
    connect(boardComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            this, [=](int index) { boardList->setCurrentIndex(monitor_->index(index)); });

    // Monitor tab
    monitorText->setWordWrapMode(QTextOption::NoWrap);
    connect(monitorText, &QPlainTextEdit::customContextMenuRequested, this,
            &MainWindow::openMonitorContextMenu);
    connect(monitorEdit, &QLineEdit::returnPressed, this, &MainWindow::sendMonitorInput);
    connect(sendButton, &QToolButton::clicked, this, &MainWindow::sendMonitorInput);

    auto add_eol_action = [=](const QString &title, const QString &eol) {
        auto action = new QAction(title, actionMonitorEOLGroup);
        action->setCheckable(true);
        action->setProperty("EOL", eol);
        return action;
    };

    menuMonitorOptions = new QMenu(this);
    actionMonitorEOLGroup = new QActionGroup(this);
    add_eol_action(tr("No line ending"), "");
    add_eol_action(tr("Newline (LF)"), "\n")->setChecked(true);
    add_eol_action(tr("Carriage return (CR)"), "\r");
    add_eol_action(tr("Both (CRLF)"), "\r\n");
    menuMonitorOptions->addActions(actionMonitorEOLGroup->actions());
    menuMonitorOptions->addSeparator();
    actionMonitorEcho = menuMonitorOptions->addAction(tr("Echo"));
    actionMonitorEcho->setCheckable(true);
    sendButton->setMenu(menuMonitorOptions);

    // Settings tab
    connect(firmwarePath, &QLineEdit::editingFinished, this, &MainWindow::validateAndSetFirmwarePath);
    connect(firmwareBrowseButton, &QToolButton::clicked, this, &MainWindow::browseForFirmware);
    firmwareBrowseButton->setMenu(menuBrowseFirmware);
    connect(actionAttachMonitor, &QAction::triggered, this,
            &MainWindow::setAttachMonitorForSelection);
    connect(resetAfterCheck, &QCheckBox::clicked, this, &MainWindow::setResetAfterForSelection);
    connect(codecComboBox, &QComboBox::currentTextChanged, this, &MainWindow::setSerialCodecForSelection);
    connect(clearOnResetCheck, &QCheckBox::clicked, this, &MainWindow::setClearOnResetForSelection);
    connect(scrollBackLimitSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setScrollBackLimitForSelection);

    initCodecList();
    for (auto codec: codecs_)
        codecComboBox->addItem(codec);

    // TyQt errors
    connect(tyQt, &TyQt::globalError, this, &MainWindow::showErrorMessage);

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
    statusBar()->showMessage(msg, TY_SHOW_ERROR_TIMEOUT);
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
    if (fws.empty()) {
        for (auto &board: selected_boards_)
            board->notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return;
    }

    for (auto &board: selected_boards_)
        board->startUpload(fws);
}

void MainWindow::dropAssociationForSelection()
{
    for (auto &board: selected_boards_)
        board->setFirmware("");
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
    actionMinimalInterface->setChecked(enable);

    if (enable) {
        menubar->setVisible(false);
        toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        bool focus = boardList->hasFocus();
        if (boardComboAction) {
            tabWidget->setTabPosition(QTabWidget::West);
            boardComboAction->setVisible(true);
        } else {
            tabWidget->setCornerWidget(boardComboBox, Qt::TopRightCorner);
            boardComboBox->setVisible(true);
        }

        boardList->setVisible(false);
        if (focus)
            boardComboBox->setFocus(Qt::OtherFocusReason);

        setContextMenuPolicy(Qt::ActionsContextMenu);
    } else {
        menubar->setVisible(true);
        toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        bool focus = boardComboBox->hasFocus();
        if (boardComboAction) {
            tabWidget->setTabPosition(QTabWidget::North);
            boardComboAction->setVisible(false);
        } else {
            boardComboBox->setVisible(false);
            tabWidget->setCornerWidget(nullptr, Qt::TopRightCorner);
        }

        boardList->setVisible(true);
        if (focus)
            boardList->setFocus(Qt::OtherFocusReason);

        setContextMenuPolicy(Qt::NoContextMenu);
    }
}

void MainWindow::openCloneWindow()
{
    auto win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);

    win->resize(size());
    win->setCompactMode(compactMode());
    win->boardList->selectionModel()->select(boardList->selectionModel()->selection(),
                                           QItemSelectionModel::SelectCurrent);
    win->tabWidget->setCurrentIndex(tabWidget->currentIndex());

    win->show();
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

void MainWindow::openPreferences()
{
    PreferencesDialog(this).exec();
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
    s += actionMonitorEOLGroup->checkedAction()->property("EOL").toString();

    auto echo = actionMonitorEcho->isChecked();
    for (auto &board: selected_boards_) {
        if (echo)
            board->appendToSerialDocument(s);
        board->sendSerial(s);
    }

    monitorEdit->clear();
}

void MainWindow::clearMonitor()
{
    monitorText->clear();
}

void MainWindow::initCodecList()
{
    if (!codecs_.isEmpty())
        return;

    auto mibs = QTextCodec::availableMibs();

    codecs_.reserve(mibs.count());
    for (auto mib: mibs)
        codecs_.append(QTextCodec::codecForMib(mib)->name());
    codecs_.sort(Qt::CaseInsensitive);
    codecs_.removeDuplicates();

    codec_indexes_.reserve(codecs_.count());
    int index = 0;
    for (auto codec: codecs_)
        codec_indexes_.insert(codec, index++);
}

void MainWindow::enableBoardWidgets()
{
    infoTab->setEnabled(true);
    monitorTab->setEnabled(true);
    actionClearMonitor->setEnabled(true);
    uploadTab->setEnabled(true);
    actionAttachMonitor->setEnabled(true);

    monitorText->setDocument(&current_board_->serialDocument());
    monitorText->moveCursor(QTextCursor::End);
    monitorText->verticalScrollBar()->setValue(monitorText->verticalScrollBar()->maximum());

    actionRenameBoard->setEnabled(true);
    ambiguousBoardLabel->setVisible(!current_board_->hasCapability(TY_BOARD_CAPABILITY_UNIQUE));
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

    actionRenameBoard->setEnabled(false);
    ambiguousBoardLabel->setVisible(false);
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
    actionDropFirmware->setText(tr("&Drop firmware association"));
    actionDropFirmware->setEnabled(!selected_boards_.empty());

    if (current_board_) {
        auto firmware = current_board_->firmware();
        if (!firmware.isEmpty()) {
            actionDropFirmware->setText(tr("&Drop association to '%1'")
                                        .arg(QFileInfo(firmware).fileName()));
        } else {
            actionDropFirmware->setEnabled(false);
        }

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

#ifdef __APPLE__
    menuRecentFirmwares2->clear();
    menuRecentFirmwares2->addActions(menuRecentFirmwares->actions());
    menuRecentFirmwares2->setEnabled(menuRecentFirmwares->isEnabled());

    menuRecentFirmwares3->clear();
    menuRecentFirmwares3->addActions(menuRecentFirmwares->actions());
    menuRecentFirmwares3->setEnabled(menuRecentFirmwares->isEnabled());
#endif
}

QString MainWindow::browseFirmwareDirectory() const
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

QString MainWindow::browseFirmwareFilter() const
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

    auto indexes = boardList->selectionModel()->selectedIndexes();
    for (auto &idx: indexes) {
        if (idx.column() == 0)
            selected_boards_.push_back(monitor_->board(idx.row()));
    }

    for (auto &board: selected_boards_) {
        connect(board.get(), &Board::interfacesChanged, this, &MainWindow::refreshActions);
        connect(board.get(), &Board::statusChanged, this, &MainWindow::refreshActions);
    }

    if (selected_boards_.size() == 1) {
        current_board_ = selected_boards_.front().get();
        boardComboBox->setCurrentIndex(indexes.first().row());

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
        boardComboBox->setCurrentIndex(-1);

        disableBoardWidgets();
        refreshActions();
        updateWindowTitle();
        updateFirmwareMenus();
    }
}

void MainWindow::openBoardListContextMenu(const QPoint &pos)
{
    /* Most of the time, the right click changes selection so we can just open a context
       menu with the various actions in their current state. In some seemingly random cases,
       a context menu can be opened on unselected items, and I don't know enough about the
       subtleties of the QAbstractItemView selection system / modes to know if it is normal
       or not. As a quick "fix", only show the menu in the "normal" case. */
    if (!boardList->selectionModel()->isSelected(boardList->indexAt(pos)))
        return;

    menuBoardContext->exec(boardList->viewport()->mapToGlobal(pos));
}

void MainWindow::refreshActions()
{
    bool upload = false, reset = false, reboot = false;
    for (auto &board: selected_boards_) {
        if (board->taskStatus() != TY_TASK_STATUS_READY)
            continue;

        upload |= board->hasCapability(TY_BOARD_CAPABILITY_UPLOAD) ||
                  board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        reset |= board->hasCapability(TY_BOARD_CAPABILITY_RESET) ||
                 board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        reboot |= board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
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
    codecComboBox->blockSignals(true);
    codecComboBox->setCurrentIndex(codec_indexes_.value(current_board_->serialCodecName(), 0));
    codecComboBox->blockSignals(false);
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

void MainWindow::openMonitorContextMenu(const QPoint &pos)
{
    unique_ptr<QMenu> menu(monitorText->createStandardContextMenu());
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

void MainWindow::setSerialCodecForSelection(const QString &codec_name)
{
    for (auto &board: selected_boards_)
        board->setSerialCodecName(codec_name.toUtf8());
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
