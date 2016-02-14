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

Board::Board(ty_board *board, QObject *parent)
    : QObject(parent), board_(ty_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setUndoRedoEnabled(false);

    // The monitor will move the serial notifier to a dedicated thread
    connect(&serial_notifier_, &DescriptorNotifier::activated, this, &Board::serialReceived,
            Qt::DirectConnection);

    error_timer_.setInterval(SHOW_ERROR_TIMEOUT);
    error_timer_.setSingleShot(true);
    connect(&error_timer_, &QTimer::timeout, this, &Board::taskChanged);

    loadSettings();
}

shared_ptr<Board> Board::createBoard(ty_board *board)
{
    // Work around the private constructor for make_shared()
    struct BoardSharedEnabler : public Board {
        BoardSharedEnabler(ty_board *board)
            : Board(board) {}
    };

    return make_shared<BoardSharedEnabler>(board);
}

Board::~Board()
{
    ty_board_interface_close(serial_iface_);
    ty_board_unref(board_);
}

void Board::loadSettings()
{
    setTag(db_.get("tag", "").toString());
    setResetAfter(db_.get("resetAfter", true).toBool());
    setClearOnReset(db_.get("clearOnReset", false).toBool());
    setScrollBackLimit(db_.get("scrollBackLimit", 200000).toUInt());
    setAttachMonitor(db_.get("attachMonitor", true).toBool());
}

ty_board *Board::board() const
{
    return board_;
}

bool Board::matchesTag(const QString &id)
{
    return ty_board_matches_tag(board_, id.toLocal8Bit().constData());
}

ty_board_state Board::state() const
{
    return ty_board_get_state(board_);
}

uint16_t Board::capabilities() const
{
    return ty_board_get_capabilities(board_);
}

const ty_board_model *Board::model() const
{
    return ty_board_get_model(board_);
}

QString Board::modelName() const
{
    auto model = ty_board_get_model(board_);
    if (!model)
        return tr("(unknown)");

    return ty_board_model_get_name(model);
}

QString Board::tag() const
{
    return ty_board_get_tag(board_);
}

QString Board::id() const
{
    return ty_board_get_id(board_);
}

QString Board::location() const
{
    return ty_board_get_location(board_);
}

uint64_t Board::serialNumber() const
{
    return ty_board_get_serial_number(board_);
}

std::vector<BoardInterfaceInfo> Board::interfaces() const
{
    std::vector<BoardInterfaceInfo> vec;

    ty_board_list_interfaces(board_, [](ty_board_interface *iface, void *udata) {
        BoardInterfaceInfo info;
        info.name = ty_board_interface_get_name(iface);
        info.path = ty_board_interface_get_path(iface);
        info.capabilities = ty_board_interface_get_capabilities(iface);
        info.number = ty_board_interface_get_interface_number(iface);
        info.open = ty_board_interface_get_handle(iface);

        auto vec = reinterpret_cast<std::vector<BoardInterfaceInfo> *>(udata);
        vec->push_back(info);

        return 0;
    }, &vec);

    return vec;
}

bool Board::isRunning() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_RUN);
}

bool Board::uploadAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_UPLOAD) || rebootAvailable();
}

bool Board::resetAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_RESET) || rebootAvailable();
}

bool Board::rebootAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_REBOOT);
}

bool Board::serialAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_SERIAL);
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
        return serialOpen() ? ":/board_attached" : ":/board_detached";
    if (uploadAvailable())
        return ":/board_bootloader";

    return ":/board_missing";
}

QString Board::firmwareName() const
{
    return firmware_name_;
}

QString Board::statusText() const
{
    if (isRunning())
        return firmware_name_.isEmpty() ? tr("(running)") : firmware_name_;
    if (uploadAvailable())
        return tr("(bootloader)");
    return tr("(missing)");
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

    for (unsigned int i = 0; i < TY_BOARD_CAPABILITY_COUNT; i++) {
        if (capabilities & (1 << i))
            list.append(ty_board_capability_get_name(static_cast<ty_board_capability>(i)));
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

TaskInterface Board::upload()
{
    if (firmware_.isEmpty())
        return watchTask(make_task<FailedTask>(tr("No firmware set for board '%1'").arg(tag())));

    auto fw = Firmware::load(firmware_);
    if (!fw)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));

    return upload({fw});
}

TaskInterface Board::upload(const vector<shared_ptr<Firmware>> &fws)
{
    return upload(fws, reset_after_);
}

TaskInterface Board::upload(const vector<shared_ptr<Firmware>> &fws, bool reset_after)
{
    vector<ty_firmware *> fws2;
    ty_task *task;
    int r;

    fws2.reserve(fws.size());
    for (auto &fw: fws)
        fws2.push_back(fw->firmware());

    r = ty_upload(board_, &fws2[0], fws2.size(), reset_after ? 0 : TY_UPLOAD_NORESET, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));

    auto task2 = make_task<TyTask>(task);
    watchTask(task2);
    connect(&task_watcher_, &TaskWatcher::finished,
            this, [=](bool success, shared_ptr<void> result) {
        if (!success)
            return;

        auto fw = static_cast<ty_firmware *>(result.get());
        setFirmware(ty_firmware_get_filename(fw));
        firmware_name_ = ty_firmware_get_name(fw);

        emit boardChanged();
    });

    return task2;
}

TaskInterface Board::reset()
{
    ty_task *task;
    int r;

    r = ty_reset(board_, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));

    return watchTask(make_task<TyTask>(task));
}

TaskInterface Board::reboot()
{
    ty_task *task;
    int r;

    r = ty_reboot(board_, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));

    return watchTask(make_task<TyTask>(task));
}

bool Board::sendSerial(const QByteArray &buf)
{
    ssize_t r = ty_board_serial_write(board_, buf.data(), buf.size());
    if (r < 0) {
        emit notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return false;
    }

    return true;
}

void Board::setTag(const QString &tag)
{
    if (tag.isEmpty() && ty_board_get_tag(board_) == ty_board_get_id(board_))
        return;
    if (tag == ty_board_get_tag(board_))
        return;

    int r = ty_board_set_tag(board_, tag.isEmpty() ? nullptr : tag.toLocal8Bit().constData());
    if (r < 0)
        throw bad_alloc();

    db_.put("tag", tag);
    emit settingChanged("tag", tag);
}

void Board::setFirmware(const QString &firmware)
{
    if (firmware == firmware_)
        return;

    firmware_ = firmware;

    emit settingChanged("firmware", firmware);
}

void Board::setResetAfter(bool reset_after)
{
    if (reset_after == reset_after_)
        return;

    reset_after_ = reset_after;

    db_.put("resetAfter", reset_after);
    emit settingChanged("resetAfter", reset_after);
}

void Board::setClearOnReset(bool clear_on_reset)
{
    if (clear_on_reset == clear_on_reset_)
        return;

    clear_on_reset_ = clear_on_reset;

    db_.put("clearOnReset", clear_on_reset);
    emit settingChanged("clearOnReset", clear_on_reset);
}

void Board::setScrollBackLimit(unsigned int limit)
{
    if (static_cast<int>(limit) == serial_document_.maximumBlockCount())
        return;

    serial_document_.setMaximumBlockCount(static_cast<int>(limit));

    db_.put("scrollBackLimit", limit);
    emit settingChanged("scrollBackLimit", limit);
}

void Board::setAttachMonitor(bool attach_monitor)
{
    if (attach_monitor == serial_attach_)
        return;

    if (attach_monitor && serialAvailable()) {
        attach_monitor = openSerialInterface();
    } else {
        closeSerialInterface();
    }

    serial_attach_ = attach_monitor;

    db_.put("attachMonitor", attach_monitor);
    emit settingChanged("attachMonitor", attach_monitor);
}

TaskInterface Board::startUpload()
{
    auto task = upload();
    task.start();
    return task;
}

TaskInterface Board::startUpload(const vector<shared_ptr<Firmware>> &fws)
{
    auto task = upload(fws);
    task.start();
    return task;
}

TaskInterface Board::startUpload(const vector<shared_ptr<Firmware>> &fws, bool reset_after)
{
    auto task = upload(fws, reset_after);
    task.start();
    return task;
}

TaskInterface Board::startReset()
{
    auto task = reset();
    task.start();
    return task;
}

TaskInterface Board::startReboot()
{
    auto task = reboot();
    task.start();
    return task;
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

        int r = ty_board_serial_read(board_, serial_buf_ + serial_buf_len_,
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
    Q_UNUSED(result);

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
    if (ty_board_has_capability(board_, TY_BOARD_CAPABILITY_SERIAL) && serial_attach_) {
        openSerialInterface();
    } else {
        closeSerialInterface();
    }

    if (state() == TY_BOARD_STATE_DROPPED) {
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

    r = ty_board_open_interface(board_, TY_BOARD_CAPABILITY_SERIAL, &serial_iface_);
    if (r < 0) {
        notifyLog(TY_LOG_ERROR, ty_error_last_message());
        return false;
    }
    if (!r)
        return false;
    ty_board_interface_get_descriptors(serial_iface_, &set, 1);
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
    ty_board_interface_close(serial_iface_);
    serial_iface_ = nullptr;
}

TaskInterface Board::watchTask(TaskInterface task)
{
    running_task_ = task;

    /* There may be task-specific slots, such as the firmware one from upload(),
       disconnect everyone and restore sane connections. */
    task_watcher_.disconnect();
    connect(&task_watcher_, &TaskWatcher::log, this, &Board::notifyLog);
    connect(&task_watcher_, &TaskWatcher::started, this, &Board::taskChanged);
    connect(&task_watcher_, &TaskWatcher::finished, this, &Board::notifyFinished);
    connect(&task_watcher_, &TaskWatcher::progress, this, &Board::notifyProgress);

    task_watcher_.setTask(&running_task_);

    return running_task_;
}
