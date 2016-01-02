/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDesktopServices>
#include <QFileDialog>
#include <QScrollBar>
#include <QToolButton>
#include <QUrl>

#include "about_dialog.hh"
#include "arduino_dialog.hh"
#include "board.hh"
#include "board_widget.hh"
#include "commands.hh"
#include "main_window.hh"
#include "tyqt.hh"

using namespace std;

MainWindow::MainWindow(Manager *manager, QWidget *parent)
    : QMainWindow(parent), manager_(manager)
{
    setupUi(this);
    refreshBoardsInfo();

    auto uploadButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(actionUpload));
    if (uploadButton) {
        auto uploadMenu = new QMenu(this);
        uploadMenu->addAction(actionUploadNew);

        uploadButton->setMenu(uploadMenu);
        uploadButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    connect(actionQuit, &QAction::triggered, TyQt::instance(), &TyQt::quit);

    boardList->setModel(manager);
    boardList->setItemDelegate(new BoardItemDelegate(manager));
    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::selectionChanged);
    connect(manager, &Manager::boardAdded, this, &MainWindow::setBoardDefaults);

    // The blue selection frame displayed on OSX looks awful
    boardList->setAttribute(Qt::WA_MacShowFocusRect, false);

    monitorText->setWordWrapMode(QTextOption::NoWrap);
    connect(monitorText, &QPlainTextEdit::textChanged, this, &MainWindow::monitorTextChanged);
    connect(monitorText, &QPlainTextEdit::updateRequest, this, &MainWindow::monitorTextScrolled);

    logText->setMaximumBlockCount(1000);

    for (auto &board: *manager)
        setBoardDefaults(board.get());
}

bool MainWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::StatusTip)
        return true;

    return QMainWindow::event(ev);
}

QString MainWindow::makeFirmwareFilter()
{
    QString exts;
    for (auto format = tyb_firmware_formats; format->name; format++)
        exts += QString("*%1 ").arg(format->ext);
    exts.chop(1);

    return tr("Binary Files (%1);;All Files (*)").arg(exts);
}

void MainWindow::setBoardDefaults(Board *board)
{
    board->setProperty("resetAfter", true);

    if (!boardList->currentIndex().isValid() && manager_->boardCount())
        boardList->setCurrentIndex(manager_->index(0, 0));
}

void MainWindow::selectionChanged(const QItemSelection &newsel, const QItemSelection &previous)
{
    Q_UNUSED(newsel);
    Q_UNUSED(previous);

    monitorText->setDocument(nullptr);
    for (auto &board: selected_boards_)
        board->disconnect(this);
    if (current_board_)
        current_board_ = nullptr;

    selected_boards_.clear();
    auto selected = boardList->selectionModel()->selectedIndexes();
    for (auto &idx: selected) {
        if (idx.column() == 0)
            selected_boards_.push_back(manager_->board(idx.row()));
    }

    if (selected_boards_.size() == 1) {
        current_board_ = selected_boards_.front();

        firmwarePath->setText(current_board_->firmware());
        resetAfterUpload->setChecked(current_board_->property("resetAfter").toBool());
        clearOnReset->setChecked(current_board_->clearOnReset());

        monitor_autoscroll_ = true;
        monitor_cursor_ = QTextCursor();
        monitorText->setDocument(&current_board_->serialDocument());
        monitorText->moveCursor(QTextCursor::End);
        monitorText->verticalScrollBar()->setValue(monitorText->verticalScrollBar()->maximum());

        connect(current_board_.get(), &Board::propertyChanged, this, &MainWindow::updatePropertyField);
    } else {
        firmwarePath->clear();
    }
    for (auto &board: selected_boards_)
        connect(board.get(), &Board::boardChanged, this, &MainWindow::refreshBoardsInfo);

    refreshBoardsInfo();
}

void MainWindow::refreshBoardsInfo()
{
    if (current_board_) {
        setWindowTitle(QString("TyQt - %1 - %2")
                       .arg(current_board_->modelName())
                       .arg(current_board_->tag()));

        infoTab->setEnabled(true);
        modelText->setText(current_board_->modelName());
        locationText->setText(current_board_->location());
        serialText->setText(QString::number(current_board_->serialNumber()));
        firmwareText->setText(current_board_->firmwareName());

        interfaceTree->clear();
        for (auto iface: current_board_->interfaces()) {
            auto item = new QTreeWidgetItem(QStringList{iface.name, iface.path});
            item->setToolTip(1, iface.path);

            new QTreeWidgetItem(item, QStringList{tr("capabilities"),
                                Board::makeCapabilityList(current_board_->capabilities()).join(", ")});
            new QTreeWidgetItem(item, QStringList{tr("location"),
                                QString("%1:%2").arg(current_board_->location(), QString::number(iface.number))});

            interfaceTree->addTopLevelItem(item);
        }

        monitorTab->setEnabled(true);
        monitorEdit->setEnabled(current_board_->isSerialAvailable());
        actionClearMonitor->setEnabled(true);
        uploadTab->setEnabled(true);
    } else {
        setWindowTitle("TyQt");

        infoTab->setEnabled(false);
        modelText->clear();
        locationText->clear();
        serialText->clear();
        interfaceTree->clear();

        monitorTab->setEnabled(false);
        actionClearMonitor->setEnabled(false);
        uploadTab->setEnabled(false);
    }

    bool upload = false, reset = false, reboot = false;
    for (auto &board: selected_boards_) {
        upload |= board->isUploadAvailable();
        reset |= board->isResetAvailable();
        reboot |= board->isRebootAvailable();
    }
    actionUpload->setEnabled(upload);
    actionUploadNew->setEnabled(upload);
    actionReset->setEnabled(reset);
    actionReboot->setEnabled(reboot);
}

void MainWindow::updatePropertyField(const QByteArray &name, const QVariant &value)
{
    if (name == "firmware") {
        firmwarePath->setText(value.toString());
    } else if (name == "resetAfter") {
        resetAfterUpload->setChecked(value.toBool());
    } else if (name == "clearOnReset") {
        clearOnReset->setChecked(value.toBool());
    }
}

void MainWindow::monitorTextChanged()
{
    auto vbar = monitorText->verticalScrollBar();

    if (monitor_autoscroll_) {
        vbar->setValue(vbar->maximum());
    } else {
        auto hbar = monitorText->horizontalScrollBar();
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

void MainWindow::monitorTextScrolled(const QRect &rect, int dy)
{
    Q_UNUSED(rect);

    if (!dy)
        return;

    QScrollBar *vbar = monitorText->verticalScrollBar();

    monitor_autoscroll_ = vbar->value() >= vbar->maximum() - 1;
    monitor_cursor_ = monitorText->cursorForPosition(QPoint(0, 0));
}

void MainWindow::clearMonitor()
{
    monitor_cursor_ = QTextCursor();
    monitorText->clear();
}

void MainWindow::showErrorMessage(const QString &msg)
{
    statusBar()->showMessage(msg, SHOW_ERROR_TIMEOUT);
    logText->appendPlainText(msg);
}

void MainWindow::on_firmwarePath_editingFinished()
{
    if (!current_board_)
        return;

    if (!firmwarePath->text().isEmpty()) {
        QString filename = QFileInfo(firmwarePath->text()).canonicalFilePath();
        if (filename.isEmpty()) {
            tyQt->reportError(tr("Path '%1' is not valid").arg(firmwarePath->text()));
            return;
        }
        filename = QDir::toNativeSeparators(filename);

        current_board_->setFirmware(filename);
    } else {
        current_board_->setFirmware("");
    }
}

void MainWindow::on_resetAfterUpload_toggled(bool checked)
{
    if (!current_board_)
        return;

    current_board_->setProperty("resetAfter", checked);
}

void MainWindow::on_actionNewWindow_triggered()
{
    tyQt->openMainWindow();
}

void MainWindow::on_actionUpload_triggered()
{
    if (current_board_ && current_board_->firmware().isEmpty()) {
        on_actionUploadNew_triggered();
        return;
    }

    unsigned int uploaded = 0;
    for (auto &board: selected_boards_) {
        if (!board->firmware().isEmpty()) {
            auto fw = Firmware::load(board->firmware());
            if (!fw) {
                board->notifyLog(TY_LOG_ERROR, ty_error_last_message());
                continue;
            }

            board->upload({fw}, board->property("resetAfter").toBool()).start();
            uploaded++;
        }
    }
    if (!uploaded)
        tyQt->reportError("No board has a set firmware, try using 'Upload New Firmware'");
}

void MainWindow::on_actionUploadNew_triggered()
{
    auto filenames = QFileDialog::getOpenFileNames(this, tr("Open Firmwares"), "",
                                                   makeFirmwareFilter());
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
        board->upload(fws, board->property("resetAfter").toBool()).start();
}

void MainWindow::on_actionReset_triggered()
{
    for (auto &board: selected_boards_)
        board->reset().start();
}

void MainWindow::on_actionReboot_triggered()
{
    for (auto &board: selected_boards_)
        board->reboot().start();
}

void MainWindow::on_monitorEdit_returnPressed()
{
    if (!current_board_)
        return;

    QString s = monitorEdit->text();
    monitorEdit->clear();

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

    if (echo->isChecked())
        current_board_->appendToSerialDocument(s);

    current_board_->sendSerial(s.toUtf8());
}

void MainWindow::on_clearOnReset_toggled(bool checked)
{
    if (!current_board_)
        return;

    current_board_->setClearOnReset(checked);
}

void MainWindow::on_actionClearMonitor_triggered()
{
    clearMonitor();
}

void MainWindow::on_actionMinimalInterface_toggled(bool checked)
{
    toolBar->setVisible(!checked);
    boardList->setVisible(!checked);
    statusbar->setVisible(!checked);
}

void MainWindow::on_firmwareBrowseButton_clicked()
{
    if (!current_board_)
        return;

    auto filename = QFileDialog::getOpenFileName(this, tr("Open Firmware"), "",
                                                 makeFirmwareFilter());
    if (filename.isEmpty())
        return;
    filename = QDir::toNativeSeparators(filename);

    firmwarePath->setText(filename);
    current_board_->setFirmware(filename);
}

void MainWindow::on_monitorText_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = monitorText->createStandardContextMenu();

    menu->addAction(actionClearMonitor);
    menu->exec(monitorText->viewport()->mapToGlobal(pos));
}

void MainWindow::on_logText_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = logText->createStandardContextMenu();

    menu->addAction(tr("Clear"), logText, SLOT(clear()));
    menu->exec(logText->viewport()->mapToGlobal(pos));
}

void MainWindow::on_actionWebsite_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty/"));
}

void MainWindow::on_actionReportBug_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Koromix/ty/issues"));
}

void MainWindow::on_actionIntegrateToArduino_triggered()
{
    ArduinoDialog(this).exec();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog(this).exec();
}
