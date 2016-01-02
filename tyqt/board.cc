/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QCoreApplication>
#include <QDynamicPropertyChangeEvent>
#include <QIcon>
#include <QMutexLocker>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>

#include <functional>

#include "board.hh"
#include "tyqt.hh"

using namespace std;

Board::Board(tyb_board *board, QObject *parent)
    : QObject(parent), board_(tyb_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setMaximumBlockCount(200000);

    // The manager will move the serial notifier to a dedicated thread
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

QString Board::tag() const
{
    return tyb_board_get_tag(board_);
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
        return ":/board_running";
    if (isUploadAvailable())
        return ":/board_bootloader";

    return ":/board_missing";
}

void Board::setFirmware(const QString &firmware)
{
    firmware_ = firmware;
    emit propertyChanged("firmware", firmware);
}

QString Board::firmware() const
{
    return firmware_;
}

QString Board::firmwareName() const
{
    return firmware_name_;
}

void Board::setClearOnReset(bool clear)
{
    clear_on_reset_ = clear;
    emit propertyChanged("clearOnReset", clear);
}

bool Board::clearOnReset() const
{
    return clear_on_reset_;
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

bool Board::event(QEvent *e)
{
    if (e->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *ce = static_cast<QDynamicPropertyChangeEvent *>(e);
        emit propertyChanged(ce->propertyName(), property(ce->propertyName()));
    }

    return QObject::event(e);
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

void Board::refreshBoard()
{
    if (tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL)) {
        if (!serial_available_) {
            if (clear_on_reset_)
                serial_document_.clear();

            ty_descriptor_set set = {};
            tyb_board_get_descriptors(board_, TYB_BOARD_CAPABILITY_SERIAL, &set, 1);

            serial_notifier_.setDescriptorSet(&set);
            serial_available_ = true;
        }
    } else if (serial_available_) {
        serial_notifier_.clear();
        serial_available_ = false;
    }
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

bool Board::sendSerial(const QByteArray &buf)
{
    return tyb_board_serial_write(board_, buf.data(), buf.size()) > 0;
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

TaskInterface Board::wrapBoardTask(ty_task *task, function<void(bool success, shared_ptr<void> result)> finish)
{
    task_finish_ = finish;

    running_task_ = make_task<TyTask>(task);
    task_watcher_.setTask(&running_task_);

    return running_task_;
}

Manager::~Manager()
{
    serial_thread_.quit();
    serial_thread_.wait();

    // Just making sure nothing depends on the manager when it's destroyed
    manager_notifier_.clear();
    boards_.clear();

    tyb_monitor_free(manager_);
}

bool Manager::start()
{
    if (manager_)
        return true;

    int r = tyb_monitor_new(TYB_MONITOR_PARALLEL_WAIT, &manager_);
    if (r < 0)
        return false;
    r = tyb_monitor_register_callback(manager_, [](tyb_board *board, tyb_monitor_event event, void *udata) {
        Manager *model = static_cast<Manager *>(udata);
        return model->handleEvent(board, event);
    }, this);
    if (r < 0) {
        tyb_monitor_free(manager_);
        manager_ = nullptr;

        return false;
    }

    ty_descriptor_set set = {};
    tyb_monitor_get_descriptors(manager_, &set, 1);
    manager_notifier_.setDescriptorSet(&set);
    connect(&manager_notifier_, &DescriptorNotifier::activated, this, &Manager::refreshManager);

    serial_thread_.start();

    tyb_monitor_refresh(manager_);

    return true;
}

vector<shared_ptr<Board>> Manager::boards()
{
    return boards_;
}

shared_ptr<Board> Manager::board(unsigned int i)
{
    if (i >= boards_.size())
        return nullptr;

    return boards_[i];
}

unsigned int Manager::boardCount() const
{
    return boards_.size();
}

shared_ptr<Board> Manager::find(function<bool(Board &board)> filter)
{
    auto board = find_if(boards_.begin(), boards_.end(), [&](shared_ptr<Board> &ptr) { return filter(*ptr); });

    if (board == boards_.end())
        return nullptr;

    return *board;
}

int Manager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return boards_.size();
}

int Manager::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return 2;
}

QVariant Manager::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return tr("Model");
        case 1:
            return tr("Location");
        }
    }

    return QVariant();
}

QVariant Manager::data(const QModelIndex &index, int role) const
{
    if (index.row() >= static_cast<int>(boards_.size()))
        return QVariant();

    auto board = boards_[index.row()];

    if (index.column() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return board->modelName();
        case Qt::DecorationRole:
            return QIcon(board->statusIconFileName());
        case Qt::ToolTipRole:
            return QString(tr("%1\n\nCapabilities: %2\nLocation: %3\nSerial Number: %4\n\nFirmware: %5")
                           .arg(board->modelName())
                           .arg(Board::makeCapabilityString(board->capabilities(), tr("(none)")))
                           .arg(board->location())
                           .arg(QString::number(board->serialNumber()))
                           .arg(!board->firmwareName().isEmpty() ? board->firmwareName() : tr("(running)")));
        case Qt::SizeHintRole:
            return QSize(0, 24);
        }
    } else if (index.column() == 1) {
        /* I don't like putting selector stuff into the base model but we can always
           make a proxy later if there's a problem. */
        switch (role) {
        case Qt::DisplayRole:
            return board->tag();
        case Qt::ForegroundRole:
            return QBrush(Qt::darkGray);
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return QVariant();
}

void Manager::refreshManager(ty_descriptor desc)
{
    Q_UNUSED(desc);

    tyb_monitor_refresh(manager_);
}

int Manager::handleEvent(tyb_board *board, tyb_monitor_event event)
{
    switch (event) {
    case TYB_MONITOR_EVENT_ADDED:
        handleAddedEvent(board);
        break;

    case TYB_MONITOR_EVENT_CHANGED:
    case TYB_MONITOR_EVENT_DISAPPEARED:
        handleChangedEvent(board);
        break;

    case TYB_MONITOR_EVENT_DROPPED:
        handleDroppedEvent(board);
        break;
    }

    return 0;
}

void Manager::handleAddedEvent(tyb_board *board)
{
    auto proxy_ptr = Board::createBoard(board);
    Board *proxy = proxy_ptr.get();

    proxy->serial_notifier_.moveToThread(&serial_thread_);

    connect(proxy, &Board::taskChanged, this, [=]() {
        refreshBoardItem(proxy);
    });

    beginInsertRows(QModelIndex(), boards_.size(), boards_.size());
    boards_.push_back(proxy_ptr);
    endInsertRows();

    emit boardAdded(proxy);
}

void Manager::handleChangedEvent(tyb_board *board)
{
    auto it = find_if(boards_.begin(), boards_.end(), [=](std::shared_ptr<Board> &ptr) { return ptr->board() == board; });
    if (it == boards_.end())
        return;

    auto proxy = *it;
    proxy->refreshBoard();

    QModelIndex index = createIndex(it - boards_.begin(), 0);
    emit dataChanged(index, index);

    emit proxy->boardChanged();
}

void Manager::handleDroppedEvent(tyb_board *board)
{
    auto it = find_if(boards_.begin(), boards_.end(), [=](std::shared_ptr<Board> &ptr) { return ptr->board() == board; });
    if (it == boards_.end())
        return;

    auto proxy = *it;
    proxy->refreshBoard();

    beginRemoveRows(QModelIndex(), it - boards_.begin(), it - boards_.begin());
    boards_.erase(it);
    endRemoveRows();

    emit proxy->boardDropped();
}

void Manager::refreshBoardItem(Board *board)
{
    auto it = find_if(boards_.begin(), boards_.end(), [&](std::shared_ptr<Board> &ptr) { return ptr.get() == board; });
    if (it == boards_.end())
        return;

    QModelIndex index = createIndex(it - boards_.begin(), 0);
    dataChanged(index, index);
}
