/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QFileInfo>
#include <QMutexLocker>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>

#include "tyqt/board.hpp"
#include "tyqt/database.hpp"
#include "tyqt/monitor.hpp"

using namespace std;

#define MAX_RECENT_FIRMWARES 4

Board::Board(ty_board *board, QObject *parent)
    : QObject(parent), board_(ty_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setUndoRedoEnabled(false);

    // The monitor will move the serial notifier to a dedicated thread
    connect(&serial_notifier_, &DescriptorNotifier::activated, this, &Board::serialReceived,
            Qt::DirectConnection);

    error_timer_.setInterval(TY_SHOW_ERROR_TIMEOUT);
    error_timer_.setSingleShot(true);
    connect(&error_timer_, &QTimer::timeout, this, &Board::updateStatus);

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
    auto tag = db_.get("tag", "").toString();
    int r = ty_board_set_tag(board_, tag.isEmpty() ? nullptr : tag.toLocal8Bit().constData());
    if (r < 0)
        throw bad_alloc();

    firmware_ = db_.get("firmware", "").toString();
    if (firmware_.isEmpty() || !QFileInfo::exists(firmware_))
        firmware_ = "";
    recent_firmwares_ = db_.get("recentFirmwares", QStringList()).toStringList();
    recent_firmwares_.erase(remove_if(recent_firmwares_.begin(), recent_firmwares_.end(),
                                      [](const QString &filename) { return filename.isEmpty() || !QFileInfo::exists(filename); }),
                            recent_firmwares_.end());
    if (recent_firmwares_.count() > MAX_RECENT_FIRMWARES)
        recent_firmwares_.erase(recent_firmwares_.begin() + MAX_RECENT_FIRMWARES,
                                recent_firmwares_.end());
    reset_after_ = db_.get("resetAfter", true).toBool();
    serial_codec_name_ = db_.get("serialCodec", "UTF-8").toString();
    serial_codec_ = QTextCodec::codecForName(serial_codec_name_.toUtf8());
    if (!serial_codec_) {
        serial_codec_name_ = "UTF-8";
        serial_codec_ = QTextCodec::codecForName("UTF-8");
    }
    serial_decoder_.reset(serial_codec_->makeDecoder());
    clear_on_reset_ = db_.get("clearOnReset", false).toBool();
    serial_document_.setMaximumBlockCount(db_.get("scrollBackLimit", 200000).toInt());
    enable_serial_ = db_.get("enableSerial", enable_serial_default_).toBool();

    /* Even if the user decides to enable persistence for ambiguous identifiers,
       we still don't want to cache the board model. */
    if (!ty_board_model_get_code_size(ty_board_get_model(board_)) &&
            hasCapability(TY_BOARD_CAPABILITY_UNIQUE)) {
        auto model_name = cache_.get("model");
        if (model_name.isValid()) {
            auto model = ty_board_model_find(model_name.toString().toUtf8().constData());
            if (model)
                ty_board_set_model(board_, model);
        }
    }

    updateSerialInterface();
    if (enable_serial_ && hasCapability(TY_BOARD_CAPABILITY_SERIAL) && !serial_iface_)
        enable_serial_ = false;

    updateStatus();
    emit infoChanged();
    emit settingsChanged();
}

void Board::updateSerialInterface()
{
    if (enable_serial_ && hasCapability(TY_BOARD_CAPABILITY_SERIAL)) {
        openSerialInterface();
    } else {
        closeSerialInterface();
    }
}

bool Board::matchesTag(const QString &id)
{
    return ty_board_matches_tag(board_, id.toLocal8Bit().constData());
}

uint16_t Board::capabilities() const
{
    return ty_board_get_capabilities(board_);
}

bool Board::hasCapability(ty_board_capability cap) const
{
    return ty_board_has_capability(board_, cap);
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

QString Board::description() const
{
    return ty_board_get_description(board_);
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

void Board::updateStatus()
{
    const char *icon_name = nullptr;

    switch (ty_board_get_state(board_)) {
    case TY_BOARD_STATE_ONLINE:
        if (hasCapability(TY_BOARD_CAPABILITY_RUN)) {
            status_text_ = status_firmware_.isEmpty() ? tr("(running)") : status_firmware_;
            icon_name = serialOpen() ? ":/board_attached" : ":/board_detached";
            break;
        } else if (hasCapability(TY_BOARD_CAPABILITY_UPLOAD)) {
            status_text_ = tr("(bootloader)");
            icon_name = ":/board_bootloader";
            break;
        }
    case TY_BOARD_STATE_MISSING:
    case TY_BOARD_STATE_DROPPED:
        status_text_ = tr("(missing)");
        icon_name = ":/board_other";
        break;
    }

    if (errorOccured()) {
        icon_name = ":/board_error";
    } else if (task_.status() == TY_TASK_STATUS_PENDING) {
        icon_name = ":/board_pending";
    } else if (task_.status() == TY_TASK_STATUS_RUNNING) {
        icon_name = ":/board_working";
    }
    if (status_icon_name_ != icon_name) {
        status_icon_name_ = icon_name;
        status_icon_ = QIcon(icon_name);
    }

    emit statusChanged();
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

TaskInterface Board::upload(const QString &filename)
{
    shared_ptr<Firmware> fw;
    if (!filename.isEmpty()) {
        fw = Firmware::load(filename);
    } else {
        if (firmware_.isEmpty())
            return watchTask(make_task<FailedTask>(tr("No firmware set for board '%1'").arg(tag())));
        fw = Firmware::load(firmware_);
    }
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
    ty_task_set_pool(task, pool_);

    auto task2 = make_task<TyTask>(task);
    watchTask(task2);
    connect(&task_watcher_, &TaskWatcher::finished, this,
            [=](bool success, shared_ptr<void> result) {
        if (success)
            addUploadedFirmware(static_cast<ty_firmware *>(result.get()));
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
    ty_task_set_pool(task, pool_);

    return watchTask(make_task<TyTask>(task));
}

TaskInterface Board::reboot()
{
    ty_task *task;
    int r;

    r = ty_reboot(board_, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));
    ty_task_set_pool(task, pool_);

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

bool Board::sendSerial(const QString &s)
{
    return sendSerial(serial_codec_->fromUnicode(s));
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
    emit infoChanged();
}

void Board::setFirmware(const QString &firmware)
{
    if (firmware == firmware_)
        return;

    firmware_ = firmware;

    db_.put("firmware", firmware);
    emit settingsChanged();
}

void Board::clearRecentFirmwares()
{
    if (recent_firmwares_.isEmpty())
        return;

    recent_firmwares_.clear();

    db_.remove("recentFirmwares");
    emit settingsChanged();
}

void Board::setResetAfter(bool reset_after)
{
    if (reset_after == reset_after_)
        return;

    reset_after_ = reset_after;

    db_.put("resetAfter", reset_after);
    emit settingsChanged();
}

void Board::setSerialCodecName(QString codec_name)
{
    if (codec_name == serial_codec_name_)
        return;

    auto codec = QTextCodec::codecForName(codec_name.toUtf8());
    if (!codec)
        return;

    serial_codec_name_ = codec_name;
    serial_codec_ = codec;
    serial_decoder_.reset(serial_codec_->makeDecoder());

    db_.put("serialCodec", codec_name);
    emit settingsChanged();
}

void Board::setClearOnReset(bool clear_on_reset)
{
    if (clear_on_reset == clear_on_reset_)
        return;

    clear_on_reset_ = clear_on_reset;

    db_.put("clearOnReset", clear_on_reset);
    emit settingsChanged();
}

void Board::setScrollBackLimit(unsigned int limit)
{
    if (static_cast<int>(limit) == serial_document_.maximumBlockCount())
        return;

    serial_document_.setMaximumBlockCount(static_cast<int>(limit));

    db_.put("scrollBackLimit", limit);
    emit settingsChanged();
}

void Board::setEnableSerial(bool enable)
{
    if (enable == enable_serial_)
        return;

    enable_serial_ = enable;

    updateSerialInterface();
    if (enable && hasCapability(TY_BOARD_CAPABILITY_SERIAL) && !serial_iface_) {
        enable_serial_ = false;
        enable = false;
    } else {
        db_.put("enableSerial", enable);
    }

    updateStatus();
    emit settingsChanged();
}

TaskInterface Board::startUpload(const QString &filename)
{
    auto task = upload(filename);
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
        updateStatus();
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
    auto str = serial_decoder_->toUnicode(serial_buf_, serial_buf_len_);
    serial_buf_len_ = 0;
    locker.unlock();

    appendToSerialDocument(str);
}

void Board::notifyFinished(bool success, std::shared_ptr<void> result)
{
    Q_UNUSED(success);
    Q_UNUSED(result);

    task_ = TaskInterface();
    task_watcher_.setTask(nullptr);

    updateStatus();
}

void Board::refreshBoard()
{
    updateSerialInterface();

    if (ty_board_get_state(board_) == TY_BOARD_STATE_DROPPED) {
        emit dropped();
        return;
    }

    if (clear_on_reset_) {
        if (hasCapability(TY_BOARD_CAPABILITY_SERIAL)) {
            if (serial_clear_when_available_)
                serial_document_.clear();
            serial_clear_when_available_ = false;
        } else {
            serial_clear_when_available_ = true;
        }
    }

    auto model = this->model();
    if (ty_board_model_get_code_size(model))
        cache_.put("model", ty_board_model_get_name(model));

    updateStatus();
    emit infoChanged();
    emit interfacesChanged();
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

    emit interfacesChanged();
    return true;
}

void Board::closeSerialInterface()
{
    if (!serial_iface_)
        return;

    serial_notifier_.clear();
    ty_board_interface_close(serial_iface_);
    serial_iface_ = nullptr;

    emit interfacesChanged();
}

TaskInterface Board::watchTask(TaskInterface task)
{
    task_ = task;

    /* There may be task-specific slots, such as the firmware one from upload(),
       disconnect everyone and restore sane connections. */
    task_watcher_.disconnect();
    connect(&task_watcher_, &TaskWatcher::log, this, &Board::notifyLog);
    connect(&task_watcher_, &TaskWatcher::pending, this, &Board::updateStatus);
    connect(&task_watcher_, &TaskWatcher::started, this, &Board::updateStatus);
    connect(&task_watcher_, &TaskWatcher::finished, this, &Board::notifyFinished);
    connect(&task_watcher_, &TaskWatcher::progress, this, &Board::progressChanged);

    task_watcher_.setTask(&task_);

    return task_;
}

void Board::addUploadedFirmware(ty_firmware *fw)
{
    status_firmware_ = ty_firmware_get_name(fw);

    auto filename = ty_firmware_get_filename(fw);
    recent_firmwares_.removeAll(filename);
    recent_firmwares_.prepend(filename);
    if (recent_firmwares_.count() > MAX_RECENT_FIRMWARES)
        recent_firmwares_.erase(recent_firmwares_.begin() + MAX_RECENT_FIRMWARES,
                                recent_firmwares_.end());
    db_.put("recentFirmwares", recent_firmwares_);

    blockSignals(true);
    setFirmware(filename);
    blockSignals(false);

    updateStatus();
    emit settingsChanged();
}
