/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>

#include "board.hpp"
#include "../libhs/device.h"
#include "../libhs/serial.h"
#include "../libty/class.h"
#include "database.hpp"
#include "monitor.hpp"

using namespace std;

#define MAX_RECENT_FIRMWARES 4
#define SERIAL_LOG_DELIMITER "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"

Board::Board(ty_board *board, QObject *parent)
    : QObject(parent), board_(ty_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setUndoRedoEnabled(false);

    /* Doing font changes in Board is ugly, but the whole shared serial document thing
       we do is ugly and will need to change eventually. */
    {
        QFont font("monospace", 9);
        if (!QFontInfo(font).fixedPitch()) {
            font.setStyleHint(QFont::Monospace);
            if (!QFontInfo(font).fixedPitch())
                font.setStyleHint(QFont::TypeWriter);
        }
        serial_document_.setDefaultFont(font);
    }

    // The monitor will move the serial notifier to a dedicated thread
    connect(&serial_notifier_, &DescriptorNotifier::activated, this, &Board::serialReceived,
            Qt::DirectConnection);

    error_timer_.setInterval(TY_SHOW_ERROR_TIMEOUT);
    error_timer_.setSingleShot(true);
    connect(&error_timer_, &QTimer::timeout, this, &Board::updateStatus);
}

Board::~Board()
{
    ty_board_interface_close(serial_iface_);
    ty_board_unref(board_);
}

void Board::loadSettings(Monitor *monitor)
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
    {
        bool default_serial;
        if (model() != TY_MODEL_GENERIC && monitor) {
            default_serial = monitor->serialByDefault();
        } else {
            default_serial = false;
        }
        enable_serial_ = db_.get("enableSerial", default_serial).toBool();
    }
    serial_log_size_ = db_.get(
        "serialLogSize",
        static_cast<quint64>(monitor ? monitor->serialLogSize() : 0)).toULongLong();
    serial_rate_ = db_.get("serialRate", 115200).toUInt();

    /* Even if the user decides to enable persistence for ambiguous identifiers,
       we still don't want to cache the board model. */
    if (hasCapability(TY_BOARD_CAPABILITY_UNIQUE)) {
        auto model_name = cache_.get("model");
        if (model_name.isValid()) {
            auto model = ty_models_find(model_name.toString().toUtf8().constData());
            if (model)
                ty_board_set_model(board_, model);
        }
    }

    updateSerialInterface();
    updateSerialLogState(false);

    updateStatus();
    emit infoChanged();
    emit settingsChanged();
}

bool Board::updateSerialInterface()
{
    if (enable_serial_ && hasCapability(TY_BOARD_CAPABILITY_SERIAL)) {
        openSerialInterface();
        if (!serial_iface_) {
            enable_serial_ = false;
            return false;
        }
    } else {
        closeSerialInterface();
    }

    return true;
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

ty_model Board::model() const
{
    return ty_board_get_model(board_);
}

QString Board::modelName() const
{
    ty_model model = ty_board_get_model(board_);
    return ty_models[model].name;
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

QString Board::serialNumber() const
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

bool Board::serialIsSerial() const
{
    if (serial_iface_) {
        hs_device *dev = ty_board_interface_get_device(serial_iface_);
        return dev->type == HS_DEVICE_TYPE_SERIAL;
    } else {
        return false;
    }
}

void Board::updateStatus()
{
    const char *icon_name = nullptr;

    switch (ty_board_get_status(board_)) {
    case TY_BOARD_STATUS_ONLINE:
        if (hasCapability(TY_BOARD_CAPABILITY_RUN)) {
            status_text_ = status_firmware_.isEmpty() ? tr("(running)") : status_firmware_;
            icon_name = serialOpen() ? ":/board_attached" : ":/board_detached";
        } else if (hasCapability(TY_BOARD_CAPABILITY_UPLOAD)) {
            status_text_ = tr("(bootloader)");
            icon_name = ":/board_bootloader";
        } else {
            status_text_ = tr("(available)");
            icon_name = serialOpen() ? ":/board_attached" : ":/board_detached";
        }
        break;
    case TY_BOARD_STATUS_MISSING:
    case TY_BOARD_STATUS_DROPPED:
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

    r = ty_upload(board_, &fws2[0], static_cast<unsigned int>(fws2.size()),
                  reset_after ? 0 : TY_UPLOAD_NORESET, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));
    task->pool = pool_;

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
    task->pool = pool_;

    return watchTask(make_task<TyTask>(task));
}

TaskInterface Board::reboot()
{
    ty_task *task;
    int r;

    r = ty_reboot(board_, &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));
    task->pool = pool_;

    return watchTask(make_task<TyTask>(task));
}

TaskInterface Board::sendSerial(const QByteArray &buf)
{
    ty_task *task;
    int r;

    r = ty_send(board_, buf.data(), buf.size(), &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));
    task->pool = pool_;

    return watchTask(make_task<TyTask>(task));
}

TaskInterface Board::sendSerial(const QString &s)
{
    return sendSerial(serial_codec_->fromUnicode(s));
}

TaskInterface Board::sendFile(const QString &filename)
{
    ty_task *task;
    int r;

    r = ty_send_file(board_, filename.toLocal8Bit().constData(), &task);
    if (r < 0)
        return watchTask(make_task<FailedTask>(ty_error_last_message()));
    task->pool = pool_;

    return watchTask(make_task<TyTask>(task));
}

void Board::appendFakeSerialRead(const QString &s)
{
    if (serial_log_file_.isOpen()) {
        auto buf = serial_codec_->fromUnicode(s);
        QMutexLocker locker(&serial_lock_);
        writeToSerialLog(buf.constData(), buf.size());
        locker.unlock();
    }

    QTextCursor cursor(&serial_document_);
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(s);
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

void Board::setSerialRate(unsigned int rate)
{
    if (rate == serial_rate_)
        return;

    serial_rate_ = rate;
    if (serial_iface_) {
        closeSerialInterface();
        if (!openSerialInterface())
            updateStatus();
    }

    db_.put("serialRate", rate);
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

void Board::setEnableSerial(bool enable, bool persist)
{
    if (enable == enable_serial_)
        return;

    enable_serial_ = enable;
    if (updateSerialInterface() && persist)
        db_.put("enableSerial", enable);

    updateStatus();
    emit settingsChanged();
}

void Board::setSerialLogSize(size_t size)
{
    if (size == serial_log_size_)
        return;

    serial_log_size_ = size;
    updateSerialLogState(false);

    db_.put("serialLogSize", static_cast<quint64>(size));
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

TaskInterface Board::startSendSerial(const QByteArray &buf)
{
    auto task = sendSerial(buf);
    task.start();
    return task;
}

TaskInterface Board::startSendSerial(const QString &s)
{
    auto task = sendSerial(s);
    task.start();
    return task;
}

TaskInterface Board::startSendFile(const QString &filename)
{
    auto task = sendFile(filename);
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

    size_t previous_len = serial_buf_len_;
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

    if (serial_log_file_.isOpen())
        writeToSerialLog(serial_buf_ + previous_len, serial_buf_len_ - previous_len);

    locker.unlock();

    if (!previous_len && serial_buf_len_)
        QMetaObject::invokeMethod(this, "appendBufferToSerialDocument", Qt::QueuedConnection);
}

// You need to lock serial_lock_ before you call this
void Board::writeToSerialLog(const char *buf, size_t len)
{
    serial_log_file_.unsetError();

    qint64 pos = serial_log_file_.pos();
    if (pos + len > serial_log_size_) {
        auto part_len = serial_log_size_ - pos;
        serial_log_file_.write(buf, part_len);
        serial_log_file_.seek(0);
        serial_log_file_.write(buf + part_len, len - part_len);
    } else {
        serial_log_file_.write(buf, len);
    }

    if (!serial_log_file_.atEnd()) {
        pos = serial_log_file_.pos();
        if (pos + sizeof(SERIAL_LOG_DELIMITER) >= serial_log_size_) {
            serial_log_file_.resize(pos);
            serial_log_file_.seek(0);
        } else {
            serial_log_file_.write(SERIAL_LOG_DELIMITER);
            serial_log_file_.seek(pos);
        }
    }

    if (serial_log_file_.error() != QFileDevice::NoError) {
        auto error_msg = QString("Closed serial log file after error: %1")
                         .arg(serial_log_file_.errorString());
        ty_log(TY_LOG_ERROR, "%s", error_msg.toUtf8().constData());
        QMetaObject::invokeMethod(this, "notifyLog", Qt::QueuedConnection,
                                  Q_ARG(ty_log_level, TY_LOG_ERROR), Q_ARG(QString, error_msg));

        serial_log_file_.close();
        emit settingsChanged();
    }
}

void Board::appendBufferToSerialDocument()
{
    QMutexLocker locker(&serial_lock_);
    auto str = serial_decoder_->toUnicode(serial_buf_, static_cast<int>(serial_buf_len_));
    serial_buf_len_ = 0;
    locker.unlock();

    // Hack to fix extra empty lines when CR and LF are put in separate buffers.
    // That's something that will go away with VT-100 support.
    if (str.endsWith('\r'))
        str.resize(str.size() - 1);

    QTextCursor cursor(&serial_document_);
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(str);
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

    if (ty_board_get_status(board_) == TY_BOARD_STATUS_DROPPED) {
        emit dropped();
        return;
    }

    if (clear_on_reset_) {
        if (hasCapability(TY_BOARD_CAPABILITY_SERIAL)) {
            if (serial_clear_when_available_) {
                serial_document_.clear();
                updateSerialLogState(true);
            }
            serial_clear_when_available_ = false;
        } else {
            serial_clear_when_available_ = true;
        }
    }

    ty_model model = this->model();
    // FIXME: Hack to cache Teensy model, move to libty and drop ty_board_set_model()
    if (ty_models[model].mcu)
        cache_.put("model", ty_models[model].name);

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

    hs_device *dev = ty_board_interface_get_device(serial_iface_);
    hs_port *port = ty_board_interface_get_handle(serial_iface_);

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        hs_serial_config config = {};
        config.baudrate = serial_rate_;

        hs_serial_set_config(port, &config);
    }

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

void Board::updateSerialLogState(bool new_file)
{
    if (!hasCapability(TY_BOARD_CAPABILITY_UNIQUE)) {
        return;
    }

    QMutexLocker locker(&serial_lock_);

    if (serial_log_file_.fileName().isEmpty() || new_file) {
        serial_log_file_.close();
        serial_log_file_.setFileName(findLogFilename(id(), 4));
    }

    if (serial_log_size_) {
        if (!serial_log_file_.isOpen()) {
            if (!serial_log_file_.open(QIODevice::WriteOnly)) {
                ty_log(TY_LOG_ERROR, "Cannot open board log '%s' for writing",
                       serial_log_file_.fileName().toUtf8().constData());
            }
        }
        if (serial_log_file_.isOpen() &&
                static_cast<size_t>(serial_log_file_.size()) > serial_log_size_)
            serial_log_file_.resize(serial_log_size_);
    } else {
        serial_log_file_.close();
        serial_log_file_.remove();
    }
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
    status_firmware_ = fw->name;

    auto filename = fw->filename;
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

QString Board::findLogFilename(const QString &id, unsigned int max)
{
    QDateTime oldest_mtime;
    QString oldest_filename;

    auto dir = serial_log_dir_.isEmpty() ? QDir::tempPath() : serial_log_dir_;
    auto prefix = QString("%1/%2-%3").arg(dir, QCoreApplication::applicationName(), id);
    for (unsigned int i = 1; i <= max; i++) {
        auto filename = QString("%1-%2.txt").arg(prefix).arg(i);
        QFileInfo info(filename);

        if (!info.exists())
            return filename;
        if (oldest_filename.isEmpty() || info.lastModified() < oldest_mtime) {
            oldest_filename = filename;
            oldest_mtime = info.lastModified();
        }
    }

    return oldest_filename;
}
