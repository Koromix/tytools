/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QMutexLocker>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>

#include "board.hh"
#include "database.hh"
#include "tyqt.hh"

using namespace std;

Board::Board(tyb_board *board, QObject *parent)
    : QObject(parent), board_(tyb_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setMaximumBlockCount(200000);

    // The monitor will move the serial notifier to a dedicated thread
    connect(&serial_notifier_, &DescriptorNotifier::activated, this, &Board::serialReceived,
            Qt::DirectConnection);

    error_timer_.setInterval(SHOW_ERROR_TIMEOUT);
    error_timer_.setSingleShot(true);
    connect(&error_timer_, &QTimer::timeout, this, &Board::taskChanged);

    connect(&task_watcher_, &TaskWatcher::log, this, &Board::notifyLog);
    connect(&task_watcher_, &TaskWatcher::started, this, &Board::taskChanged);
    connect(&task_watcher_, &TaskWatcher::finished, this, &Board::notifyFinished);
    connect(&task_watcher_, &TaskWatcher::progress, this, &Board::notifyProgress);

    refreshBoard();
}

shared_ptr<Board> Board::createBoard(tyb_board *board)
{
    // Work around the private constructor for make_shared()
    struct BoardSharedEnabler : public Board {
        BoardSharedEnabler(tyb_board *board)
            : Board(board) {}
    };

    return make_shared<BoardSharedEnabler>(board);
}

Board::~Board()
{
    tyb_board_interface_close(serial_iface_);
    tyb_board_unref(board_);
}

tyb_board *Board::board() const
{
    return board_;
}

bool Board::matchesTag(const QString &id)
{
    return tyb_board_matches_tag(board_, id.toLocal8Bit().constData());
}

tyb_board_state Board::state() const
{
    return tyb_board_get_state(board_);
}

uint16_t Board::capabilities() const
{
    return tyb_board_get_capabilities(board_);
}

const tyb_board_model *Board::model() const
{
    return tyb_board_get_model(board_);
}

QString Board::modelName() const
{
    auto model = tyb_board_get_model(board_);
    if (!model)
        return tr("(unknown)");

    return tyb_board_model_get_name(model);
}

QString Board::id() const
{
    return tyb_board_get_id(board_);
}

QString Board::location() const
{
    return tyb_board_get_location(board_);
}

uint64_t Board::serialNumber() const
{
    return tyb_board_get_serial_number(board_);
}

std::vector<BoardInterfaceInfo> Board::interfaces() const
{
    std::vector<BoardInterfaceInfo> vec;

    tyb_board_list_interfaces(board_, [](tyb_board_interface *iface, void *udata) {
        BoardInterfaceInfo info;
        info.name = tyb_board_interface_get_name(iface);
        info.path = tyb_board_interface_get_path(iface);
        info.capabilities = tyb_board_interface_get_capabilities(iface);
        info.number = tyb_board_interface_get_interface_number(iface);
        info.open = tyb_board_interface_get_handle(iface);

        auto vec = reinterpret_cast<std::vector<BoardInterfaceInfo> *>(udata);
        vec->push_back(info);

        return 0;
    }, &vec);

    return vec;
}

bool Board::isRunning() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_RUN);
}

bool Board::isUploadAvailable() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_UPLOAD) || isRebootAvailable();
}

bool Board::isResetAvailable() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_RESET) || isRebootAvailable();
}

bool Board::isRebootAvailable() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_REBOOT);
}

bool Board::isSerialAvailable() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL);
}

bool Board::errorOccured() const
{
    return error_timer_.remainingTime() > 0;
}

QString Board::statusIconFileName() const
{
    if (errorOccured())
        return ":/board_error";
    if (running_task_.status() == TY_TASK_STATUS_RUNNING)
        return ":/board_working";
    if (isRunning())
        return isMonitorAttached() ? ":/board_attached" : ":/board_detached";
    if (isUploadAvailable())
        return ":/board_bootloader";

    return ":/board_missing";
}

QString Board::firmwareName() const
{
    return firmware_name_;
}

void Board::setTag(const QString &tag)
{
    int r = tyb_board_set_tag(board_, tag.isEmpty() ? nullptr : tag.toLocal8Bit().constData());
    if (r < 0)
        throw bad_alloc();
    emit settingChanged("tag", tag);
}

void Board::setFirmware(const QString &firmware)
{
    firmware_ = firmware;
    emit settingChanged("firmware", firmware);
}

void Board::setResetAfter(bool reset_after)
{
    reset_after_ = reset_after;
    emit settingChanged("resetAfter", reset_after);
}

void Board::setClearOnReset(bool clear_on_reset)
{
    clear_on_reset_ = clear_on_reset;
    emit settingChanged("clearOnReset", clear_on_reset);
}

QTextDocument &Board::serialDocument()
{
    return serial_document_;
}

void Board::appendToSerialDocument(const QString &s)
{
    QTextCursor cursor(&serial_document_);
    cursor.movePosition(QTextCursor::End);

    cursor.insertText(s);
}

QStringList Board::makeCapabilityList(uint16_t capabilities)
{
    QStringList list;

    for (unsigned int i = 0; i < TYB_BOARD_CAPABILITY_COUNT; i++) {
        if (capabilities & (1 << i))
            list.append(tyb_board_capability_get_name(static_cast<tyb_board_capability>(i)));
    }

    return list;
}

QString Board::makeCapabilityString(uint16_t capabilities, QString empty_str)
{
    QStringList list = makeCapabilityList(capabilities);

    if (list.isEmpty()) {
        return empty_str;
    } else {
        return list.join(", ");
    }
}

TaskInterface Board::upload(const std::vector<std::shared_ptr<Firmware>> &fws)
{
    return upload(fws, resetAfter());
}

TaskInterface Board::upload(const vector<shared_ptr<Firmware>> &fws, bool reset_after)
{
    vector<tyb_firmware *> fws2;
    ty_task *task;
    int r;

    fws2.reserve(fws.size());
    for (auto &fw: fws)
        fws2.push_back(fw->firmware());

    r = tyb_upload(board_, &fws2[0], fws2.size(), reset_after ? 0 : TYB_UPLOAD_NORESET, &task);
    if (r < 0)
        return make_task<FailedTask>(ty_error_last_message());

    return wrapBoardTask(task, [this](bool success, shared_ptr<void> result) {
        if (!success)
            return;

        auto fw = static_cast<tyb_firmware *>(result.get());
        setFirmware(tyb_firmware_get_filename(fw));
        firmware_name_ = tyb_firmware_get_name(fw);

        emit boardChanged();
    });
}

TaskInterface Board::reset()
{
    ty_task *task;
    int r;

    r = tyb_reset(board_, &task);
    if (r < 0)
        return make_task<FailedTask>(ty_error_last_message());

    return wrapBoardTask(task);
}

TaskInterface Board::reboot()
{
    ty_task *task;
    int r;

    r = tyb_reboot(board_, &task);
    if (r < 0)
        return make_task<FailedTask>(ty_error_last_message());

    return wrapBoardTask(task);
}

bool Board::attachMonitor()
{
    if (isSerialAvailable()) {
        serial_attach_ = openSerialInterface();
        if (!serial_attach_)
            return false;
    } else {
        serial_attach_ = true;
    }

    emit boardChanged();
    return true;
}

void Board::detachMonitor()
{
    closeSerialInterface();
    serial_attach_ = false;

    emit boardChanged();
}

bool Board::sendSerial(const QByteArray &buf)
{
    ssize_t r = tyb_board_serial_write(board_, buf.data(), buf.size());
    if (r < 0) {
        emit notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return false;
    }

    return true;

}

TaskInterface Board::runningTask() const
{
    return running_task_;
}

void Board::notifyLog(ty_log_level level, const QString &msg)
{
    Q_UNUSED(msg);

    if (level == TY_LOG_ERROR) {
        error_timer_.start();
        emit taskChanged();
    }
}

void Board::serialReceived(ty_descriptor desc)
{
    Q_UNUSED(desc);

    QMutexLocker locker(&serial_lock_);
    ty_error_mask(TY_ERROR_MODE);
    ty_error_mask(TY_ERROR_IO);

    bool was_empty = !serial_buf_len_;
    /* On OSX El Capitan (at least), serial device reads are often partial (512 and 1020 bytes
       reads happen pretty often), so try hard to empty the OS buffer. The Qt event loop may not
       give us back control before some time, and we want to avoid buffer overruns. */
    for (unsigned int i = 0; i < 4; i++) {
        if (serial_buf_len_ == sizeof(serial_buf_))
            break;

        int r = tyb_board_serial_read(board_, serial_buf_ + serial_buf_len_,
                                      sizeof(serial_buf_) - serial_buf_len_, 0);
        if (r < 0) {
            serial_notifier_.clear();
            break;
        }
        if (!r)
            break;
        serial_buf_len_ += static_cast<size_t>(r);
    }

    ty_error_unmask();
    ty_error_unmask();
    locker.unlock();

    if (was_empty && serial_buf_len_)
        QMetaObject::invokeMethod(this, "updateSerialDocument", Qt::QueuedConnection);
}

void Board::updateSerialDocument()
{
    QMutexLocker locker(&serial_lock_);
    auto str = QString::fromLocal8Bit(serial_buf_, serial_buf_len_);
    serial_buf_len_ = 0;
    locker.unlock();

    // FIXME: behavior with partial characters (UTF-8 or other multibyte encodings)
    appendToSerialDocument(str);
}

void Board::notifyFinished(bool success, std::shared_ptr<void> result)
{
    Q_UNUSED(success);

    if (task_finish_) {
        task_finish_(success, result);
        task_finish_ = nullptr;
    }

    running_task_ = TaskInterface();
    task_watcher_.setTask(nullptr);
    emit taskChanged();
}

void Board::notifyProgress(const QString &action, unsigned int value, unsigned int max)
{
    Q_UNUSED(action);
    Q_UNUSED(value);
    Q_UNUSED(max);

    emit taskChanged();
}

void Board::refreshBoard()
{
    if (tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL) && serial_attach_) {
        openSerialInterface();
    } else {
        closeSerialInterface();
    }

    if (state() == TYB_BOARD_STATE_DROPPED) {
        emit boardDropped();
    } else {
        emit boardChanged();
    }
}

bool Board::openSerialInterface()
{
    if (serial_iface_)
        return true;

    ty_descriptor_set set = {};
    int r;

    r = tyb_board_open_interface(board_, TYB_BOARD_CAPABILITY_SERIAL, &serial_iface_);
    if (r < 0) {
        notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return false;
    }
    tyb_board_interface_get_descriptors(serial_iface_, &set, 1);
    serial_notifier_.setDescriptorSet(&set);

    if (clear_on_reset_)
        serial_document_.clear();

    return true;
}

void Board::closeSerialInterface()
{
    if (!serial_iface_)
        return;

    serial_notifier_.clear();
    tyb_board_interface_close(serial_iface_);
    serial_iface_ = nullptr;
}

TaskInterface Board::wrapBoardTask(ty_task *task, function<void(bool success, shared_ptr<void> result)> finish)
{
    task_finish_ = finish;

    running_task_ = make_task<TyTask>(task);
    task_watcher_.setTask(&running_task_);

    return running_task_;
}
