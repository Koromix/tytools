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

#define MAX_SERIAL_HISTORY 10

QStringList MainWindow::codecs_;
QHash<QString, int> MainWindow::codec_indexes_;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), monitor_(tyQt->monitor())
{
    setupUi(this);
    setWindowTitle(QApplication::applicationName());

#ifdef __APPLE__
    /* Workaround for Qt OSX bug https://bugreports.qt.io/browse/QTBUG-34160
       The actions in menuRecentFirmwares are copied to menuRecentFirmwares2
       in updateFirmwareMenus(). */
    menuRecentFirmwares2 = new QMenu(menuRecentFirmwares->title(), this);
    menuRecentFirmwares3 = new QMenu(menuRecentFirmwares->title(), this);
    menuSendHistory2 = new QMenu(menuSendHistory->title(), this);
    menuSendHistory3 = new QMenu(menuSendHistory->title(), this);
#endif

    menuUpload = new QMenu(this);
    menuUpload->addAction(actionUploadNew);
    menuUpload->addAction(actionDropFirmware);
#ifdef __APPLE__
    menuUpload->addMenu(menuRecentFirmwares2);
#else
    menuUpload->addMenu(menuRecentFirmwares);
#endif

    auto uploadButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(actionUpload));
    if (uploadButton) {
        uploadButton->setMenu(menuUpload);
        uploadButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

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
    menuBoardContext->addAction(actionEnableSerial);
#ifdef __APPLE__
    menuBoardContext->addMenu(menuSendHistory2);
#else
    menuBoardContext->addMenu(menuSendHistory);
#endif
    menuBoardContext->addAction(actionSendFile);
    menuBoardContext->addAction(actionClearSerial);
    menuBoardContext->addSeparator();
    menuBoardContext->addAction(actionRenameBoard);

    menuEnableSerial = new QMenu(this);
#ifdef __APPLE__
    menuEnableSerial->addMenu(menuSendHistory3);
#else
    menuEnableSerial->addMenu(menuSendHistory);
#endif
    menuEnableSerial->addAction(actionSendFile);
    menuEnableSerial->addAction(actionClearSerial);

    auto serialButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(actionEnableSerial));
    if (serialButton) {
        serialButton->setMenu(menuEnableSerial);
        serialButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    /* Only stretch the tab widget when resizing the window, I can't manage to replicate
       this with the Designer alone. */
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(1, false);
    connect(splitter, &QSplitter::splitterMoved, this, [=](int pos) {
        bool collapsed = !pos;
        if (collapsed != compact_mode_)
            setCompactMode(collapsed);
    });
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
    connect(actionSendFile, &QAction::triggered, this, &MainWindow::sendFileToSelection);
    connect(actionQuit, &QAction::triggered, tyQt, &TyQt::quit);

    // View menu
    connect(actionNewWindow, &QAction::triggered, this, &MainWindow::openCloneWindow);
    connect(actionCompactMode, &QAction::triggered, this, &MainWindow::setCompactMode);
    connect(actionClearSerial, &QAction::triggered, this, &MainWindow::clearSerialDocument);

    // Tools menu
    connect(actionArduinoTool, &QAction::triggered, this, &MainWindow::openArduinoTool);
    connect(actionOpenLog, &QAction::triggered, tyQt, &TyQt::showLogWindow);
    connect(actionResetApp, &QAction::triggered, tyQt, &TyQt::resetMonitor);
    connect(actionResetSettingsApp, &QAction::triggered, this,
            [=]() { tyQt->clearSettingsAndResetWithConfirmation(this); });
    connect(actionPreferences, &QAction::triggered, this, &MainWindow::openPreferences);

    // About menu
#ifdef TY_CONFIG_URL_WEBSITE
    connect(actionWebsite, &QAction::triggered, &AboutDialog::openWebsite);
#else
    actionWebsite->setVisible(false);
#endif
#ifdef TY_CONFIG_URL_BUGS
    connect(actionReportBug, &QAction::triggered, &AboutDialog::openBugReports);
#else
    actionReportBug->setVisible(false);
#endif

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
        if (monitor_->rowCount() == 1)
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
    actionBoardComboBox = nullptr;
#else
    actionBoardComboBox = toolBar->addWidget(boardComboBox);
#endif
    connect(boardComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            this, [=](int index) { boardList->setCurrentIndex(monitor_->index(index)); });

    // Task progress bar (compact mode)
    statusProgressBar = new QProgressBar();
    statusProgressBar->setTextVisible(false);
    statusProgressBar->setFixedWidth(150);
    statusbar->addPermanentWidget(statusProgressBar);
    statusProgressBar->hide();

    // Serial tab
    connect(tabWidget, &QTabWidget::currentChanged, this, [=]() {
        /* Focus the serial input widget if we can, but don't be a jerk to the user. */
        if (tabWidget->currentWidget() == serialTab && serialEdit->isEnabled() &&
                !tabWidget->hasFocus())
            serialEdit->setFocus();
    });
    serialText->setWordWrapMode(QTextOption::NoWrap);
    connect(serialText, &QPlainTextEdit::customContextMenuRequested, this,
            &MainWindow::openSerialContextMenu);
    connect(serialEdit, &QLineEdit::returnPressed, this, &MainWindow::sendSerialInput);
    connect(sendButton, &QToolButton::clicked, this, &MainWindow::sendSerialInput);

    auto add_eol_action = [=](const QString &title, const QString &eol) {
        auto action = new QAction(title, actionSerialEOLGroup);
        action->setCheckable(true);
        action->setProperty("EOL", eol);
        return action;
    };

    menuSerialOptions = new QMenu(this);
    menuBrowseHistory = new QMenu(tr("Set &Recent"), this);
    menuSerialOptions->addMenu(menuBrowseHistory);
    menuSerialOptions->addAction(actionSendFile);
    menuSerialOptions->addSeparator();
    actionSerialEOLGroup = new QActionGroup(this);
    add_eol_action(tr("No line ending"), "");
    add_eol_action(tr("Newline (LF)"), "\n")->setChecked(true);
    add_eol_action(tr("Carriage return (CR)"), "\r");
    add_eol_action(tr("Both (CRLF)"), "\r\n");
    menuSerialOptions->addActions(actionSerialEOLGroup->actions());
    menuSerialOptions->addSeparator();
    actionSerialEcho = menuSerialOptions->addAction(tr("Echo"));
    actionSerialEcho->setCheckable(true);
    sendButton->setMenu(menuSerialOptions);

    // Settings tab
    connect(firmwarePath, &QLineEdit::editingFinished, this, &MainWindow::validateAndSetFirmwarePath);
    connect(firmwareBrowseButton, &QToolButton::clicked, this, &MainWindow::browseForFirmware);
    firmwareBrowseButton->setMenu(menuBrowseFirmware);
    connect(actionEnableSerial, &QAction::triggered, this,
            &MainWindow::setEnableSerialForSelection);
    connect(resetAfterCheck, &QCheckBox::clicked, this, &MainWindow::setResetAfterForSelection);
    connect(codecComboBox, &QComboBox::currentTextChanged, this, &MainWindow::setSerialCodecForSelection);
    connect(clearOnResetCheck, &QCheckBox::clicked, this, &MainWindow::setClearOnResetForSelection);
    connect(scrollBackLimitSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setScrollBackLimitForSelection);

    initCodecList();
    for (auto codec: codecs_)
        codecComboBox->addItem(codec);

    // Toggle collapsed option groups in Compact Mode
    for (auto &object: optionsTab->children()) {
        auto group_box = qobject_cast<EnhancedGroupBox *>(object);
        if (group_box) {
            if (!lastOpenOptionBox)
                lastOpenOptionBox = group_box;

            /* We set StrongFocus in the Designer to put them in the correct tab position. The
               policy is set to StrongFocus when the group is made collapsible in Compact Mode. */
            group_box->setFocusPolicy(Qt::NoFocus);
            connect(group_box, &EnhancedGroupBox::clicked, this, [=](bool checked) {
                if (checked && group_box != lastOpenOptionBox) {
                    lastOpenOptionBox->collapse();
                    lastOpenOptionBox = group_box;
                }
            });
        }
    }

    // TyQt errors
    connect(tyQt, &TyQt::globalError, this, &MainWindow::showErrorMessage);

    if (monitor_->rowCount()) {
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
    actionCompactMode->setChecked(enable);

    if (enable == compact_mode_)
        return;
    compact_mode_ = enable;

    if (enable) {
        menubar->setVisible(false);
        toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        bool focus = boardList->hasFocus();
        if (actionBoardComboBox) {
            tabWidget->setTabPosition(QTabWidget::West);
            actionBoardComboBox->setVisible(true);
        } else {
            tabWidget->setCornerWidget(boardComboBox, Qt::TopRightCorner);
            boardComboBox->setVisible(true);
        }

        if (current_board_ && current_board_->taskStatus() != TY_TASK_STATUS_READY)
            statusProgressBar->show();

        saved_splitter_pos_ = splitter->sizes().first();
        if (!saved_splitter_pos_)
            saved_splitter_pos_ = 1;

        /* Unfortunately, even collapsed the board list still constrains the minimum
           width of the splitter. This is the simplest jerk-free way I know to work
           around this behaviour. */
        int list_width = boardList->minimumSize().width();
        int splitter_width = splitter->minimumSizeHint().width();
        splitter->setMinimumWidth(splitter_width - list_width);
        splitter->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        splitter->setSizes({0, 1});
        if (focus)
            boardComboBox->setFocus(Qt::OtherFocusReason);

        for (auto &object: optionsTab->children()) {
            auto group_box = qobject_cast<EnhancedGroupBox *>(object);
            if (group_box) {
                group_box->setCollapsible(true);
                group_box->setExpanded(lastOpenOptionBox == group_box);
            }
        }

        setContextMenuPolicy(Qt::ActionsContextMenu);
    } else {
        menubar->setVisible(true);
        toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        bool focus = boardComboBox->hasFocus();
        if (actionBoardComboBox) {
            tabWidget->setTabPosition(QTabWidget::North);
            actionBoardComboBox->setVisible(false);
        } else {
            boardComboBox->setVisible(false);
            tabWidget->setCornerWidget(nullptr, Qt::TopRightCorner);
        }

        statusProgressBar->hide();

        /* Remove the splitter layout hack used for compact mode. */
        splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        splitter->setMinimumWidth(0);

        splitter->setSizes({saved_splitter_pos_, 1});
        if (focus)
            boardList->setFocus(Qt::OtherFocusReason);

        for (auto &object: optionsTab->children()) {
            auto group_box = qobject_cast<EnhancedGroupBox *>(object);
            if (group_box)
                group_box->setCollapsible(false);
        }

        setContextMenuPolicy(Qt::NoContextMenu);
    }
}

void MainWindow::openCloneWindow()
{
    auto win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);

    win->resize(size());
    win->setCompactMode(compact_mode_);
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

void MainWindow::sendSerialInput()
{
    sendToSelectedBoards(serialEdit->text());
    serialEdit->clear();
}

void MainWindow::sendFileToSelection()
{
    if (selected_boards_.empty())
        return;

    auto filename = QFileDialog::getOpenFileName(this, tr("Send File"));
    if (filename.isEmpty())
        return;

    for (auto &board: selected_boards_)
        board->startSendFile(filename);

    appendToSerialHistory(QString("@%1").arg(filename));
}

void MainWindow::clearSerialDocument()
{
    serialText->clear();
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
    serialTab->setEnabled(true);
    actionClearSerial->setEnabled(true);
    optionsTab->setEnabled(true);
    actionEnableSerial->setEnabled(true);

    serialText->setDocument(&current_board_->serialDocument());
    serialText->moveCursor(QTextCursor::End);
    serialText->verticalScrollBar()->setValue(serialText->verticalScrollBar()->maximum());

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
    serialNumberText->clear();
    descriptionText->clear();
    interfaceTree->clear();

    serialTab->setEnabled(false);
    actionClearSerial->setEnabled(false);
    optionsTab->setEnabled(false);
    actionEnableSerial->setEnabled(false);

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
            action->setEnabled(actionUpload->isEnabled());
            action = menuBrowseFirmware->addAction(tr("Set to '%1'").arg(firmware));
            connect(action, &QAction::triggered, current_board_,
                    [=]() { current_board_->setFirmware(firmware); });
        }
    }

    if (!menuRecentFirmwares->isEmpty()) {
        menuRecentFirmwares->setEnabled(true);
        menuBrowseFirmware->setEnabled(true);

        if (!actionClearRecentFirmwares) {
            actionClearRecentFirmwares = new QAction(tr("&Clear recent firmwares"), this);
        } else {
            actionClearRecentFirmwares->disconnect(SIGNAL(triggered()));
        }
        connect(actionClearRecentFirmwares, &QAction::triggered, current_board_,
                &Board::clearRecentFirmwares);

        menuRecentFirmwares->addSeparator();
        menuRecentFirmwares->addAction(actionClearRecentFirmwares);
        menuBrowseFirmware->addSeparator();
        menuBrowseFirmware->addAction(actionClearRecentFirmwares);
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

void MainWindow::sendToSelectedBoards(const QString &s)
{
    if (s.startsWith('@')) {
        auto filename = s.mid(1);
        for (auto &board: selected_boards_)
            board->startSendFile(filename);
    } else {
        QString newline = actionSerialEOLGroup->checkedAction()->property("EOL").toString();
        bool echo = actionSerialEcho->isChecked();

        auto s2 = s + newline;
        for (auto &board: selected_boards_) {
            if (echo)
                board->appendToSerialDocument(s2);
            board->startSendSerial(s2);
        }
    }

    appendToSerialHistory(s);
}

void MainWindow::appendToSerialHistory(const QString &s)
{
    serial_history_.removeAll(s);
    serial_history_.prepend(s);
    if (serial_history_.count() > MAX_SERIAL_HISTORY)
        serial_history_.erase(serial_history_.begin() + MAX_SERIAL_HISTORY, serial_history_.end());

    menuSendHistory->clear();
    menuBrowseHistory->clear();

    if (!serial_history_.isEmpty()) {
        for (auto &sent: serial_history_) {
            QAction *action;

            action = menuSendHistory->addAction(tr("Send '%1'").arg(sent));
            connect(action, &QAction::triggered, this,
                    [=]() { sendToSelectedBoards(sent); });
            action = menuBrowseHistory->addAction(sent);
            connect(action, &QAction::triggered, serialEdit,
                    [=]() { serialEdit->setText(sent); });
        }

        if (!actionClearSerialHistory) {
            actionClearSerialHistory = new QAction(tr("&Clear serial history"), this);
            connect(actionClearSerialHistory, &QAction::triggered, this,
                    &MainWindow::clearSerialHistory);
        }

        menuSendHistory->addSeparator();
        menuSendHistory->addAction(actionClearSerialHistory);
        menuBrowseHistory->addSeparator();
        menuBrowseHistory->addAction(actionClearSerialHistory);

        menuSendHistory->setEnabled(actionSendFile->isEnabled());
        menuBrowseHistory->setEnabled(actionSendFile->isEnabled());
    } else {
        menuSendHistory->setEnabled(false);
        menuBrowseHistory->setEnabled(false);
    }

#ifdef __APPLE__
    menuSendHistory2->clear();
    menuSendHistory2->addActions(menuSendHistory->actions());
    menuSendHistory2->setEnabled(menuSendHistory->isEnabled());

    menuSendHistory3->clear();
    menuSendHistory3->addActions(menuSendHistory->actions());
    menuSendHistory3->setEnabled(menuSendHistory->isEnabled());
#endif
}

void MainWindow::clearSerialHistory()
{
    serial_history_.clear();

    menuSendHistory->clear();
    menuSendHistory->setEnabled(false);
#ifdef __APPLE__
    menuSendHistory2->clear();
    menuSendHistory2->setEnabled(false);
    menuSendHistory3->clear();
    menuSendHistory3->setEnabled(false);
#endif
    menuBrowseHistory->clear();
    menuBrowseHistory->setEnabled(false);
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
    serialText->setDocument(nullptr);
    selected_boards_.clear();
    current_board_ = nullptr;

    auto indexes = boardList->selectionModel()->selectedIndexes();
    for (auto &idx: indexes) {
        if (idx.column() == 0)
            selected_boards_.push_back(Monitor::boardFromModel(monitor_, idx));
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
        connect(current_board_, &Board::progressChanged, this, &MainWindow::refreshProgress);

        enableBoardWidgets();
        refreshActions();
        refreshInfo();
        refreshSettings();
        refreshInterfaces();
        refreshStatus();

        /* Focus the serial input widget if we can, but don't be a jerk to the user. */
        if (tabWidget->currentWidget() == serialTab && serialEdit->isEnabled() &&
                !boardList->hasFocus() && !boardComboBox->hasFocus())
            serialEdit->setFocus();
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
    bool upload = false, reset = false, reboot = false, send = false;
    for (auto &board: selected_boards_) {
        if (board->taskStatus() != TY_TASK_STATUS_READY)
            continue;

        upload |= board->hasCapability(TY_BOARD_CAPABILITY_UPLOAD) ||
                  board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        reset |= board->hasCapability(TY_BOARD_CAPABILITY_RESET) ||
                 board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        reboot |= board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        send |= board->serialOpen();
    }

    actionUpload->setEnabled(upload);
    actionUploadNew->setEnabled(upload);
    actionReset->setEnabled(reset);
    actionReboot->setEnabled(reboot);

    actionSendFile->setEnabled(send);
    menuSendHistory->setEnabled(send && !serial_history_.isEmpty());
#ifdef __APPLE__
    menuSendHistory2->setEnabled(menuSendHistory->isEnabled());
    menuSendHistory3->setEnabled(menuSendHistory->isEnabled());
#endif
    menuBrowseHistory->setEnabled(send && !serial_history_.isEmpty());
    bool focus = !serialEdit->isEnabled() && sendButton->hasFocus();
    serialEdit->setEnabled(send);
    if (focus)
        serialEdit->setFocus();
}

void MainWindow::refreshInfo()
{
    updateWindowTitle();

    idText->setText(current_board_->id());
    modelText->setText(current_board_->modelName());
    locationText->setText(current_board_->location());
    serialNumberText->setText(QString::number(current_board_->serialNumber()));
    descriptionText->setText(current_board_->description());
}

void MainWindow::refreshSettings()
{
    actionEnableSerial->setChecked(current_board_->enableSerial());
    serialEdit->setEnabled(current_board_->serialOpen());

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
        auto item = new QTreeWidgetItem();
        item->setText(0, iface.name);
        item->setText(1, iface.path);

        auto tooltip = tr("%1\n+ Location: %2\n+ Interface Number: %3\n+ Capabilities: %4")
                       .arg(iface.name).arg(iface.path).arg(iface.number)
                       .arg(Board::makeCapabilityList(iface.capabilities).join(", "));
        item->setToolTip(0, tooltip);
        item->setToolTip(1, tooltip);

        interfaceTree->addTopLevelItem(item);
    }
}

void MainWindow::refreshStatus()
{
    statusText->setText(current_board_->statusText());

    if (compact_mode_ && current_board_->taskStatus() != TY_TASK_STATUS_READY) {
        // Set both to 0 to show busy indicator
        statusProgressBar->setValue(0);
        statusProgressBar->setMaximum(0);
        statusProgressBar->show();
    } else {
        statusProgressBar->hide();
    }
}

void MainWindow::refreshProgress()
{
    auto task = current_board_->task();
    statusProgressBar->setMaximum(task.progressMaximum());
    statusProgressBar->setValue(task.progress());
}

void MainWindow::openSerialContextMenu(const QPoint &pos)
{
    unique_ptr<QMenu> menu(serialText->createStandardContextMenu());
    menu->addAction(actionClearSerial);
    menu->exec(serialText->viewport()->mapToGlobal(pos));
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

void MainWindow::setEnableSerialForSelection(bool enable)
{
    for (auto &board: selected_boards_)
        board->setEnableSerial(enable);
}
