/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QDesktopServices>
#include <QFileDialog>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCodec>
#include <QToolButton>
#include <QUrl>

#include "about_dialog.hpp"
#include "arduino_dialog.hpp"
#include "board.hpp"
#include "board_widget.hpp"
#include "main_window.hpp"
#include "monitor.hpp"
#include "preferences_dialog.hpp"
#include "tycommander.hpp"

using namespace std;

QStringList MainWindow::codecs_;
QHash<QString, int> MainWindow::codec_indexes_;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), monitor_(tyCommander->monitor())
{
    setupUi(this);
    setWindowTitle(QApplication::applicationName());

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
    menuBoardContext->addAction(actionSendFile);
    menuBoardContext->addAction(actionClearSerial);
    menuBoardContext->addSeparator();
    menuBoardContext->addAction(actionRenameBoard);

    menuEnableSerial = new QMenu(this);
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
    connect(actionQuit, &QAction::triggered, tyCommander, &TyCommander::quit);

    // Serial menu
    connect(actionEnableSerial, &QAction::triggered, this,
            &MainWindow::setEnableSerialForSelection);
    connect(actionSendFile, &QAction::triggered, this, &MainWindow::makeSendFileCommand);
    connect(actionClearSerial, &QAction::triggered, this, &MainWindow::clearSerialDocument);

    // View menu
    connect(actionNewWindow, &QAction::triggered, this, &MainWindow::openCloneWindow);
    connect(actionCompactMode, &QAction::triggered, this, &MainWindow::setCompactMode);
    connect(actionShowAppLog, &QAction::triggered, tyCommander, &TyCommander::showLogWindow);

    // Tools menu
    connect(actionArduinoTool, &QAction::triggered, this, &MainWindow::openArduinoTool);
    connect(actionResetApp, &QAction::triggered, tyCommander, &TyCommander::resetMonitor);
    connect(actionResetSettingsApp, &QAction::triggered, this,
            [=]() { tyCommander->clearSettingsAndResetWithConfirmation(this); });
    connect(actionPreferences, &QAction::triggered, this, &MainWindow::openPreferences);

    // About menu
    if (TY_CONFIG_URL_WEBSITE[0]) {
        connect(actionWebsite, &QAction::triggered, &AboutDialog::openWebsite);
    } else {
        actionWebsite->setVisible(false);
    }
    if (TY_CONFIG_URL_BUGS[0]) {
        connect(actionReportBug, &QAction::triggered, &AboutDialog::openBugReports);
    } else {
        actionReportBug->setVisible(false);
    }

    connect(actionAbout, &QAction::triggered, this, &MainWindow::openAboutDialog);

    // Ctrl+Tab board nagivation
    connect(new QShortcut(QKeySequence::NextChild, this),
            &QShortcut::activated, this, &MainWindow::selectNextBoard);
    /* Work around broken QKeySequence::PreviousChild, see (old) Qt bug report at
       https://bugreports.qt.io/browse/QTBUG-15746 */
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab, this),
            &QShortcut::activated, this, &MainWindow::selectPreviousBoard);
#ifdef _WIN32
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_F6, this),
            &QShortcut::activated, this, &MainWindow::selectPreviousBoard);
#endif
#ifdef __APPLE__
    connect(new QShortcut(Qt::CTRL | Qt::Key_BraceLeft, this),
            &QShortcut::activated, this, &MainWindow::selectPreviousBoard);
#endif

    // Board list
    boardList->setModel(monitor_);
    boardList->setItemDelegate(new BoardItemDelegate(monitor_));
    connect(boardList, &QListView::customContextMenuRequested, this,
            &MainWindow::openBoardListContextMenu);
    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::selectionChanged);
    /* Select board on insertion and removal if nothing is selected. Use Qt::QueuedConnection
       for removals to make sure we get the insertion before the removal when a board is
       replaced by the user. */
    connect(monitor_, &Monitor::rowsInserted, this, &MainWindow::fixEmptySelection);
    connect(monitor_, &Monitor::rowsRemoved, this, &MainWindow::fixEmptySelection,
            Qt::QueuedConnection);
    /* serialEdit->setFocus() is not called in selectionChanged() if the board list
       has the focus to prevent stealing keyboard focus. We need to do it here. */
    connect(boardList, &QListView::clicked, this, &MainWindow::autoFocusBoardWidgets);
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
    boardComboBox->setFocusPolicy(Qt::TabFocus);
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
        // Focus the serial input widget if we can, but don't be a jerk to keyboard users
        if (!tabWidget->hasFocus())
            autoFocusBoardWidgets();
    });
    serialText->setWordWrapMode(QTextOption::NoWrap);
    connect(serialText, &QPlainTextEdit::customContextMenuRequested, this,
            &MainWindow::openSerialContextMenu);
    connect(serialEdit, &EnhancedLineInput::textCommitted, this, &MainWindow::sendToSelectedBoards);
    connect(sendButton, &QToolButton::clicked, serialEdit, &EnhancedLineInput::commit);
    serialEdit->lineEdit()->setPlaceholderText(tr("Send data..."));

    auto add_eol_action = [=](const QString &title, const QString &eol) {
        auto action = new QAction(title, actionSerialEOLGroup);
        action->setCheckable(true);
        action->setProperty("EOL", eol);
        return action;
    };

    menuSerialOptions = new QMenu(this);
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
    connect(resetAfterCheck, &QCheckBox::clicked, this, &MainWindow::setResetAfterForSelection);
    connect(rateComboBox, &QComboBox::currentTextChanged, this, [=](const QString &str) {
        unsigned int rate = str.toUInt();
        setSerialRateForSelection(rate);
    });
    connect(codecComboBox, &QComboBox::currentTextChanged, this, &MainWindow::setSerialCodecForSelection);
    connect(clearOnResetCheck, &QCheckBox::clicked, this, &MainWindow::setClearOnResetForSelection);
    connect(scrollBackLimitSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setScrollBackLimitForSelection);
    connect(serialLogSizeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setSerialLogSizeForSelection);

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

    // TyCommander errors
    connect(tyCommander, &TyCommander::globalError, this, &MainWindow::showErrorMessage);

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

void MainWindow::selectNextBoard()
{
    if (!monitor_->rowCount())
        return;

    auto indexes = boardList->selectionModel()->selectedIndexes();
    qSort(indexes);

    QModelIndex new_index;
    if (indexes.isEmpty()) {
        new_index = monitor_->index(0, 0);
    } else if (indexes.count() == 1) {
        if (monitor_->rowCount() == 1)
            return;

        auto row = indexes.first().row();
        if (row + 1 < monitor_->rowCount()) {
            new_index = monitor_->index(row + 1, 0);
        } else {
            new_index = monitor_->index(0, 0);
        }
    } else {
        new_index = indexes.first();
    }

    if (new_index.isValid()) {
        boardList->selectionModel()->select(new_index, QItemSelectionModel::ClearAndSelect);
        autoFocusBoardWidgets();
    }
}

void MainWindow::selectPreviousBoard()
{
    if (!monitor_->rowCount())
        return;

    auto indexes = boardList->selectionModel()->selectedIndexes();
    qSort(indexes);

    QModelIndex new_index;
    if (indexes.isEmpty()) {
        new_index = monitor_->index(monitor_->rowCount() - 1, 0);
    } else if (indexes.count() == 1) {
        if (monitor_->rowCount() == 1)
            return;

        auto row = indexes.first().row();
        if (row > 0) {
            new_index = monitor_->index(row - 1, 0);
        } else {
            new_index = monitor_->index(monitor_->rowCount() - 1, 0);
        }
    } else {
        new_index = indexes.last();
    }

    if (new_index.isValid()) {
        boardList->selectionModel()->select(new_index, QItemSelectionModel::ClearAndSelect);
        autoFocusBoardWidgets();
    }
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
        tyCommander->reportError(tr("No board has a set firmware, try using 'Upload New Firmware'"));
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

void MainWindow::sendToSelectedBoards(const QString &s)
{
    QString cmd;
    QString value;
    if (s.startsWith('@')) {
        cmd = s.mid(1, s.indexOf(' ', 1) - 1).trimmed();
        value = s.mid(cmd.length() + 1).trimmed();
    } else {
        cmd = "send";
        value = s;
    }

    QString echo_str;
    if (cmd == "send_file") {
        echo_str = s + "\n";

        for (auto &board: selected_boards_)
            board->startSendFile(value);
    } else if (cmd == "send") {
        echo_str = value +
                   actionSerialEOLGroup->checkedAction()->property("EOL").toString();

        for (auto &board: selected_boards_)
            board->startSendSerial(echo_str);
    } else {
        ty_log(TY_LOG_ERROR, "Unknown command '%s' (prefix with '@send ' if your string starts with character '@')",
               cmd.toUtf8().constData());
        for (auto &board: selected_boards_)
            board->notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return;
    }

    if (actionSerialEcho->isChecked()) {
        for (auto &board: selected_boards_)
            board->appendFakeSerialRead(echo_str);
    }
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

void MainWindow::makeSendFileCommand()
{
    if (selected_boards_.empty())
        return;

    auto filename = QFileDialog::getOpenFileName(this, tr("Send File"));
    if (filename.isEmpty())
        return;

    auto cmd = QString("@send_file %1").arg(filename);
    serialEdit->setCurrentText(cmd);
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

void MainWindow::fixEmptySelection(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(end);

    if (selected_boards_.empty() && monitor_->rowCount()) {
        if (start >= monitor_->rowCount())
            start = monitor_->rowCount() - 1;
        boardList->setCurrentIndex(monitor_->index(start, 0));
    }
}

void MainWindow::enableBoardWidgets()
{
    infoTab->setEnabled(true);
    serialTab->setEnabled(true);
    actionClearSerial->setEnabled(true);
    optionsTab->setEnabled(true);
    actionEnableSerial->setEnabled(true);

    QTextDocument *document = &current_board_->serialDocument();
    serialText->setDocument(document);
    serialText->moveCursor(QTextCursor::End);
    serialText->verticalScrollBar()->setValue(serialText->verticalScrollBar()->maximum());
    serialText->setFont(document->defaultFont());
    serialEdit->setFont(document->defaultFont());

    actionRenameBoard->setEnabled(true);
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
    updateSerialLogLink();
    ambiguousBoardLabel->setVisible(false);

    actionRenameBoard->setEnabled(false);
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

void MainWindow::updateSerialLogLink()
{
    QString log_filename;
    if (current_board_ && current_board_->serialLogSize())
        log_filename = current_board_->serialLogFilename();

    QFont link_font = serialLogFileLabel->font();
    if (!log_filename.isEmpty()) {
        QFileInfo log_info(log_filename);
        serialLogDirLabel->setText(QString("<a href=\"%1\">%2</a>")
                                   .arg(QUrl::fromLocalFile(log_info.path()).toString(),
                                        QDir::toNativeSeparators(log_info.dir().dirName() + '/')));
        serialLogDirLabel->setToolTip(QDir::toNativeSeparators(log_info.path()));
        serialLogFileLabel->setText(QString("<a href=\"%1\">%2</a>")
                                    .arg(QUrl::fromLocalFile(log_filename).toString(),
                                         log_info.fileName()));
        serialLogFileLabel->setToolTip(QDir::toNativeSeparators(log_filename));
        link_font.setItalic(false);
    } else {
        serialLogDirLabel->setText(tr("No serial log available"));
        serialLogDirLabel->setToolTip("");
        serialLogFileLabel->setText("");
        serialLogFileLabel->setToolTip("");
        link_font.setItalic(true);
    }
    serialLogDirLabel->setFont(link_font);
    serialLogFileLabel->setFont(link_font);
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
    for (unsigned int i = 0; i < ty_firmware_formats_count; i++)
        exts += QString("*%1 ").arg(ty_firmware_formats[i].ext);
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
    qSort(indexes);
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

        /* Focus the serial input widget if we can, but don't be a jerk. Unfortunately
           this also prevents proper edit focus when the user clicks a board in the
           list, we fix that by handling boardList::clicked() in the constructor. */
        if (!boardList->hasFocus() && !boardComboBox->hasFocus())
            autoFocusBoardWidgets();
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

void MainWindow::autoFocusBoardWidgets()
{
    if (tabWidget->currentWidget() == serialTab && serialEdit->isEnabled())
        serialEdit->setFocus();
}

void MainWindow::refreshActions()
{
    bool upload = false, reset = false, reboot = false, send = false;
    for (auto &board: selected_boards_) {
        if (board->taskStatus() == TY_TASK_STATUS_READY) {
            upload |= board->hasCapability(TY_BOARD_CAPABILITY_UPLOAD) ||
                      board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
            reset |= board->hasCapability(TY_BOARD_CAPABILITY_RESET) ||
                     board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
            reboot |= board->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        }
        send |= board->serialOpen();
    }

    actionUpload->setEnabled(upload);
    actionUploadNew->setEnabled(upload);
    actionReset->setEnabled(reset);
    actionReboot->setEnabled(reboot);

    actionSendFile->setEnabled(send);
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
    serialNumberText->setText(current_board_->serialNumber());
    descriptionText->setText(current_board_->description());

    updateSerialLogLink();
}

void MainWindow::refreshSettings()
{
    actionEnableSerial->setChecked(current_board_->enableSerial());
    serialEdit->setEnabled(current_board_->serialOpen());

    firmwarePath->setText(current_board_->firmware());
    resetAfterCheck->setChecked(current_board_->resetAfter());
    rateComboBox->blockSignals(true);
    rateComboBox->setCurrentText(QString::number(current_board_->serialRate()));
    rateComboBox->blockSignals(false);
    codecComboBox->blockSignals(true);
    codecComboBox->setCurrentIndex(codec_indexes_.value(current_board_->serialCodecName(), 0));
    codecComboBox->blockSignals(false);
    clearOnResetCheck->setChecked(current_board_->clearOnReset());
    scrollBackLimitSpin->blockSignals(true);
    scrollBackLimitSpin->setValue(current_board_->scrollBackLimit());
    scrollBackLimitSpin->blockSignals(false);
    updateSerialLogLink();
    serialLogSizeSpin->blockSignals(true);
    serialLogSizeSpin->setValue(static_cast<int>(current_board_->serialLogSize() / 1000));
    serialLogSizeSpin->blockSignals(false);

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

    ambiguousBoardLabel->setVisible(!current_board_->hasCapability(TY_BOARD_CAPABILITY_UNIQUE));
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

    rateComboBox->setEnabled(current_board_->serialIsSerial());
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
            tyCommander->reportError(tr("Path '%1' does not exist").arg(firmwarePath->text()));
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

void MainWindow::setSerialRateForSelection(unsigned int rate)
{
    for (auto &board: selected_boards_)
        board->setSerialRate(rate);
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

void MainWindow::setSerialLogSizeForSelection(int size)
{
    for (auto &board: selected_boards_)
        board->setSerialLogSize(size * 1000);
}
