/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>

#include "../tycommander/board.hpp"
#include "../tycommander/monitor.hpp"
#include "../tycommander/task.hpp"
#include "tyuploader.hpp"
#include "uploader_window.hpp"

using namespace std;

QVariant UploaderWindowModelFilter::data(const QModelIndex &index, int role) const
{
    if (index.column() == Monitor::COLUMN_BOARD && role == Qt::DisplayRole) {
        auto board = Monitor::boardFromModel(this, index);
        return QString("%1 %2").arg(board->description()).arg(board->serialNumber());
    }

    return QIdentityProxyModel::data(index, role);
}

UploaderWindow::UploaderWindow(QWidget *parent)
    : QMainWindow(parent), monitor_(tyUploader->monitor())
{
    setupUi(this);
    setWindowTitle(QApplication::applicationName());

    if (QFile::exists(":/logo"))
        logoLabel->setPixmap(QPixmap(":/logo"));
    resize(0, 0);

    connect(actionUpload, &QAction::triggered, this, &UploaderWindow::uploadNewToCurrent);
    connect(actionReset, &QAction::triggered, this, &UploaderWindow::resetCurrent);
    connect(actionQuit, &QAction::triggered, this, &TyUploader::quit);

    connect(actionOpenLog, &QAction::triggered, tyUploader, &TyUploader::showLogWindow);
    if (TY_CONFIG_URL_WEBSITE[0]) {
        connect(actionWebsite, &QAction::triggered, this, &UploaderWindow::openWebsite);
    } else {
        actionWebsite->setVisible(false);
    }
    if (TY_CONFIG_URL_BUGS[0]) {
        connect(actionReportBug, &QAction::triggered, this, &UploaderWindow::openBugReports);
    } else {
        actionReportBug->setVisible(false);
    }

    connect(boardComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UploaderWindow::currentChanged);
    monitor_model_.setSourceModel(monitor_);
    boardComboBox->setModel(&monitor_model_);
    connect(uploadButton, &QToolButton::clicked, this, &UploaderWindow::uploadNewToCurrent);
    connect(resetButton, &QToolButton::clicked, this, &UploaderWindow::resetCurrent);

    // Error messages
    connect(tyUploader, &TyUploader::globalError, this, &UploaderWindow::showErrorMessage);

    if (!current_board_)
        changeCurrentBoard(nullptr);
}

bool UploaderWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::StatusTip)
        return true;

    return QMainWindow::event(ev);
}

void UploaderWindow::showErrorMessage(const QString &msg)
{
    statusBar()->showMessage(msg, TY_SHOW_ERROR_TIMEOUT);
}

void UploaderWindow::uploadNewToCurrent()
{
    if (!current_board_)
        return;

    auto filename = QFileDialog::getOpenFileName(this, tr("Select a firmware for this device"),
                                                 current_board_->firmware(), browseFirmwareFilter());
    if (filename.isEmpty())
        return;

    current_board_->startUpload(filename);
}

void UploaderWindow::resetCurrent()
{
    if (!current_board_)
        return;

    current_board_->startReset();
}

void UploaderWindow::openWebsite()
{
    QDesktopServices::openUrl(QUrl(TY_CONFIG_URL_WEBSITE));
}

void UploaderWindow::openBugReports()
{
    QDesktopServices::openUrl(QUrl(TY_CONFIG_URL_BUGS));
}

void UploaderWindow::changeCurrentBoard(Board *board)
{
    if (current_board_) {
        current_board_->disconnect(this);
        current_board_ = nullptr;
    }

    if (board) {
        current_board_ = board->shared_from_this();

        connect(board, &Board::interfacesChanged, this, &UploaderWindow::refreshActions);
        connect(board, &Board::statusChanged, this, &UploaderWindow::refreshActions);
        connect(board, &Board::statusChanged, this, &UploaderWindow::refreshProgress);
        connect(board, &Board::progressChanged, this, &UploaderWindow::refreshProgress);
    }

    refreshActions();
}

void UploaderWindow::refreshActions()
{
    bool upload = false, reset = false;

    if (current_board_) {
        if (current_board_->taskStatus() == TY_TASK_STATUS_READY) {
            upload = current_board_->hasCapability(TY_BOARD_CAPABILITY_UPLOAD) ||
                     current_board_->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
            reset = current_board_->hasCapability(TY_BOARD_CAPABILITY_RESET) ||
                    current_board_->hasCapability(TY_BOARD_CAPABILITY_REBOOT);
        }
    } else {
        stackedWidget->setCurrentIndex(0);
    }

    uploadButton->setEnabled(upload);
    actionUpload->setEnabled(upload);
    resetButton->setEnabled(reset);
    actionReset->setEnabled(reset);
}

void UploaderWindow::refreshProgress()
{
    auto task = current_board_->task();

    if (task.status() == TY_TASK_STATUS_PENDING || task.status() == TY_TASK_STATUS_RUNNING) {
        stackedWidget->setCurrentIndex(1);
        taskProgress->setRange(0, task.progressMaximum());
        taskProgress->setValue(task.progress());
    } else {
        stackedWidget->setCurrentIndex(0);
    }
}

QString UploaderWindow::browseFirmwareFilter() const
{
    QString exts;
    for (unsigned int i = 0; i < ty_firmware_formats_count; i++)
        exts += QString("*%1 ").arg(ty_firmware_formats[i].ext);
    exts.chop(1);

    return tr("Binary Files (%1);;All Files (*)").arg(exts);
}

void UploaderWindow::currentChanged(int index)
{
    changeCurrentBoard(Monitor::boardFromModel(&monitor_model_, index).get());
}
