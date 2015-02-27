/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>

#include "ty.h"
#include "about_dialog.hh"
#include "board_widget.hh"
#include "main_window.hh"

using namespace std;

MainWindow::MainWindow(BoardManagerProxy *manager, QWidget *parent)
    : QMainWindow(parent), manager_(manager)
{
    setupUi(this);

    disableBoardWidgets();
    monitorText->setWordWrapMode(QTextOption::WrapAnywhere);

    // Errors may be thrown from a worker thread, so use signals to queue them on the GUI thread
    ty_error_redirect([](ty_err err, const char *msg, void *udata) {
        TY_UNUSED(err);

        MainWindow *main = static_cast<MainWindow *>(udata);
        emit main->errorMessage(msg);
    }, this);
    connect(this, &MainWindow::errorMessage, this, &MainWindow::showErrorMessage);

    boardList->setModel(manager);
    boardList->setItemDelegate(new BoardItemDelegate(manager));

    connect(boardList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::selectionChanged);
    connect(manager, &BoardManagerProxy::boardAdded, this, &MainWindow::setBoardDefaults);

    connect(monitorText, &QPlainTextEdit::textChanged, this, &MainWindow::monitorTextChanged);

    for (auto &board: *manager)
        setBoardDefaults(board);
}

MainWindow::~MainWindow()
{
    ty_error_redirect(nullptr, nullptr);
}

QString MainWindow::lastError() const
{
    return last_error_;
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
    if (!current_board_)
        return QString();

    QString filename = QFileDialog::getOpenFileName(this, tr("Open Firmware"), QString(),
                                                    tr("Binary Files (*.elf *.hex);;All Files (*)"));
    if (filename.isEmpty())
        return QString();

    firmwarePath->setText(filename);
    emit firmwarePath->editingFinished();

    return filename;
}

void MainWindow::uploadCurrentFirmware()
{
    if (!current_board_)
        return;

    current_board_->upload(current_board_->property("firmware").toString(),
                           current_board_->property("resetAfter").toBool());
}

void MainWindow::setBoardDefaults(shared_ptr<BoardProxy> board)
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

    monitorText->setDocument(&current_board_->serialDocument());

    connect(current_board_.get(), &BoardProxy::boardChanged, this, &MainWindow::refreshBoardInfo);
    connect(current_board_.get(), &BoardProxy::propertyChanged, this, &MainWindow::updatePropertyField);

    refreshBoardInfo();
}

void MainWindow::refreshBoardInfo()
{
    setWindowTitle(QString("TyQt - %1 - %2")
                   .arg(current_board_->modelDesc())
                   .arg(current_board_->identity()));

    infoTab->setEnabled(true);
    modelText->setText(current_board_->modelDesc());
    locationText->setText(current_board_->location());
    serialText->setText(QString::number(current_board_->serialNumber()));

    interfaceTree->clear();
    for (auto iface: current_board_->interfaces()) {
        auto item = new QTreeWidgetItem(QStringList{iface.desc, iface.path});
        item->setToolTip(1, iface.path);

        new QTreeWidgetItem(item, QStringList{tr("capabilities"),
                            BoardProxy::makeCapabilityList(current_board_->capabilities()).join(", ")});
        new QTreeWidgetItem(item, QStringList{tr("location"),
                            QString("%1@%2").arg(current_board_->location(), QString::number(iface.number))});

        interfaceTree->addTopLevelItem(item);
    }

    monitorTab->setEnabled(true);
    if (current_board_->isSerialAvailable()) {
        monitorEdit->setEnabled(true);
    } else {
        monitorEdit->setEnabled(false);
    }

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

void MainWindow::updatePropertyField(const char *name, const QVariant &value)
{
    if (strcmp(name, "firmware") == 0) {
        firmwarePath->setText(value.toString());
    } else if (strcmp(name, "resetAfter") == 0) {
        resetAfterUpload->setChecked(value.toBool());
    }
}

void MainWindow::monitorTextChanged()
{
    if (autoscroll->isChecked()) {
        monitorText->moveCursor(QTextCursor::End);
        monitorText->ensureCursorVisible();
    }
}

void MainWindow::showErrorMessage(const QString &msg)
{
    last_error_ = msg;

    fprintf(stderr, "%s\n", qPrintable(msg));

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

void MainWindow::on_actionUpload_triggered()
{
    if (!current_board_)
        return;

    if (current_board_->property("firmware").toString().isEmpty()) {
        QString filename = browseForFirmware();
        if (filename.isEmpty())
            return;
    }

    uploadCurrentFirmware();
}

void MainWindow::on_actionUploadNew_triggered()
{
    if (!current_board_)
        return;

    QString filename = browseForFirmware();
    if (filename.isEmpty())
        return;

    uploadCurrentFirmware();
}

void MainWindow::on_actionReset_triggered()
{
    if (!current_board_)
        return;

    current_board_->reset();
}

void MainWindow::on_actionReboot_triggered()
{
    if (!current_board_)
        return;

    current_board_->reboot();
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

void MainWindow::on_actionMinimalInterface_toggled(bool checked)
{
    toolBar->setVisible(!checked);
    boardList->setVisible(!checked);
    statusbar->setVisible(!checked);
}

void MainWindow::on_browseButton_clicked()
{
    browseForFirmware();
}

void MainWindow::on_monitorText_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = monitorText->createStandardContextMenu();

    menu->addAction(tr("Clear"), monitorText, SLOT(clear()));
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
