/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDesktopServices>
#include <QFileDialog>
#include <QScrollBar>
#include <QToolButton>
#include <QUrl>

#include "ty.h"
#include "about_dialog.hh"
#include "board_widget.hh"
#include "commands.hh"
#include "main_window.hh"
#include "tyqt.hh"

using namespace std;

MainWindow::MainWindow(Manager *manager, QWidget *parent)
    : QMainWindow(parent), manager_(manager)
{
    setupUi(this);

    connect(actionQuit, &QAction::triggered, TyQt::instance(), &TyQt::quit);

    QObject *obj = toolBar->widgetForAction(actionUpload);
    if (obj) {
        QToolButton* uploadButton = qobject_cast<QToolButton *>(obj);
        if (uploadButton) {
            QMenu *uploadMenu = new QMenu(this);
            uploadMenu->addAction(actionUploadNew);
            uploadMenu->addSeparator();
            uploadMenu->addAction(actionUploadAll);

            uploadButton->setMenu(uploadMenu);
            uploadButton->setPopupMode(QToolButton::MenuButtonPopup);
        }
    }

    disableBoardWidgets();
    monitorText->setWordWrapMode(QTextOption::WrapAnywhere);

    boardList->setModel(manager);
    boardList->setItemDelegate(new BoardItemDelegate(manager));

    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::selectionChanged);
    connect(manager, &Manager::boardAdded, this, &MainWindow::setBoardDefaults);

    connect(monitorText, &QPlainTextEdit::textChanged, this, &MainWindow::monitorTextChanged);
    connect(monitorText, &QPlainTextEdit::updateRequest, this, &MainWindow::monitorTextScrolled);

    for (auto &board: *manager)
        setBoardDefaults(board);

    logText->setMaximumBlockCount(1000);
}

void MainWindow::disableBoardWidgets()
{
    setWindowTitle("TyQt");

    infoTab->setEnabled(false);
    modelText->clear();
    locationText->clear();
    serialText->clear();
    interfaceTree->clear();

    monitorTab->setEnabled(false);
    monitorEdit->setEnabled(false);

    actionUpload->setEnabled(false);
    actionUploadNew->setEnabled(false);
    uploadTab->setEnabled(false);
    firmwarePath->clear();

    actionReset->setEnabled(false);
    actionReboot->setEnabled(false);
}

QString MainWindow::browseForFirmware()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Firmware"), QString(),
                                                    tr("Binary Files (*.elf *.hex);;All Files (*)"));
    if (filename.isEmpty())
        return QString();

    firmwarePath->setText(filename);
    emit firmwarePath->editingFinished();

    return filename;
}

void MainWindow::setBoardDefaults(shared_ptr<Board> board)
{
    board->setProperty("resetAfter", true);

    if (!boardList->currentIndex().isValid() && manager_->boardCount())
        boardList->setCurrentIndex(manager_->index(0, 0));
}

void MainWindow::selectionChanged(const QItemSelection &selected, const QItemSelection &previous)
{
    TY_UNUSED(previous);

    if (current_board_)
        current_board_->disconnect(this);

    if (selected.indexes().isEmpty()) {
        monitorText->setDocument(nullptr);

        current_board_ = nullptr;
        disableBoardWidgets();

        return;
    }

    current_board_ = manager_->board(selected.indexes().front().row());

    firmwarePath->setText(current_board_->property("firmware").toString());
    resetAfterUpload->setChecked(current_board_->property("resetAfter").toBool());

    monitor_autoscroll_ = true;
    clearOnReset->setChecked(current_board_->clearOnReset());

    monitor_cursor_ = QTextCursor();
    monitorText->setDocument(&current_board_->serialDocument());
    monitorText->moveCursor(QTextCursor::End);
    monitorText->verticalScrollBar()->setValue(monitorText->verticalScrollBar()->maximum());

    connect(current_board_.get(), &Board::boardChanged, this, &MainWindow::refreshBoardInfo);
    connect(current_board_.get(), &Board::propertyChanged, this, &MainWindow::updatePropertyField);

    refreshBoardInfo();
}

void MainWindow::refreshBoardInfo()
{
    setWindowTitle(QString("TyQt - %1 - %2")
                   .arg(current_board_->modelName())
                   .arg(current_board_->tag()));

    infoTab->setEnabled(true);
    modelText->setText(current_board_->modelName());
    locationText->setText(current_board_->location());
    serialText->setText(QString::number(current_board_->serialNumber()));

    interfaceTree->clear();
    for (auto iface: current_board_->interfaces()) {
        auto item = new QTreeWidgetItem(QStringList{iface.desc, iface.path});
        item->setToolTip(1, iface.path);

        new QTreeWidgetItem(item, QStringList{tr("capabilities"),
                            Board::makeCapabilityList(current_board_->capabilities()).join(", ")});
        new QTreeWidgetItem(item, QStringList{tr("location"),
                            QString("%1:%2").arg(current_board_->location(), QString::number(iface.number))});

        interfaceTree->addTopLevelItem(item);
    }

    monitorTab->setEnabled(true);
    monitorEdit->setEnabled(current_board_->isSerialAvailable());

    if (current_board_->isUploadAvailable()) {
        actionUpload->setEnabled(true);
        actionUploadNew->setEnabled(true);
        uploadTab->setEnabled(true);
    } else {
        actionUpload->setEnabled(false);
        actionUploadNew->setEnabled(false);
        uploadTab->setEnabled(false);
    }

    actionReset->setEnabled(current_board_->isResetAvailable());
    actionReboot->setEnabled(current_board_->isRebootAvailable());
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
    if (monitor_autoscroll_) {
        monitorText->verticalScrollBar()->setValue(monitorText->verticalScrollBar()->maximum());
    } else {
        QTextCursor old_cursor = monitorText->textCursor();

        monitorText->setTextCursor(monitor_cursor_);
        monitorText->ensureCursorVisible();

        int position = monitorText->verticalScrollBar()->value();

        monitorText->setTextCursor(old_cursor);
        monitorText->verticalScrollBar()->setValue(position);
    }
}

void MainWindow::monitorTextScrolled(const QRect &rect, int dy)
{
    TY_UNUSED(rect);

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
    statusBar()->showMessage(msg, 5000);
    logText->appendPlainText(msg);
}

void MainWindow::on_firmwarePath_editingFinished()
{
    if (!current_board_)
        return;

    if (!firmwarePath->text().isEmpty()) {
        QString firmware = QFileInfo(firmwarePath->text()).canonicalFilePath();
        if (firmware.isEmpty()) {
            tyQt->reportError(tr("Path '%1' is not valid").arg(firmwarePath->text()));
            return;
        }

        current_board_->setProperty("firmware", firmware);
    } else {
        current_board_->setProperty("firmware", QVariant());
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
    if (!current_board_)
        return;

    if (current_board_->property("firmware").toString().isEmpty()) {
        QString filename = browseForFirmware();
        if (filename.isEmpty())
            return;

        Commands::upload(*current_board_, filename).start();
    } else {
        Commands::upload(*current_board_, "").start();
    }
}

void MainWindow::on_actionUploadNew_triggered()
{
    if (!current_board_)
        return;

    QString filename = browseForFirmware();
    if (filename.isEmpty())
        return;

    Commands::upload(*current_board_, filename).start();
}

void MainWindow::on_actionUploadAll_triggered()
{
    Commands::uploadAll().start();
}

void MainWindow::on_actionReset_triggered()
{
    if (!current_board_)
        return;

    current_board_->reset().start();
}

void MainWindow::on_actionReboot_triggered()
{
    if (!current_board_)
        return;

    current_board_->reboot().start();
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

void MainWindow::on_actionMinimalInterface_toggled(bool checked)
{
    toolBar->setVisible(!checked);
    boardList->setVisible(!checked);
    statusbar->setVisible(!checked);
}

void MainWindow::on_actionClearMonitor_triggered()
{
    clearMonitor();
}

void MainWindow::on_firmwareBrowseButton_clicked()
{
    browseForFirmware();
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

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog(this).exec();
}
