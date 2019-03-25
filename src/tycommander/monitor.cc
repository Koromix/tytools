/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QBrush>
#include <QIcon>

#include "board.hpp"
#include "database.hpp"
#include "descriptor_notifier.hpp"
#include "monitor.hpp"
#include "../libhs/platform.h"
#include "../libty/task.h"

using namespace std;

Monitor::Monitor(QObject *parent)
    : QAbstractListModel(parent)
{
    int r = ty_pool_new(&pool_);
    if (r < 0)
        throw bad_alloc();

    loadSettings();
}

Monitor::~Monitor()
{
    stop();

    ty_pool_free(pool_);
    ty_monitor_free(monitor_);
}

void Monitor::loadSettings()
{
    auto max_tasks = db_.get("maxTasks").toUInt();
    if (!max_tasks) {
#ifdef _WIN32
        if (hs_win32_version() >= HS_WIN32_VERSION_10) {
            /* Windows 10 is much faster to load drivers and make the device available, we
               can probably afford that. */
            max_tasks = 2;
        } else {
            max_tasks = 1;
        }
#else
        max_tasks = 4;
#endif
    }
    ty_pool_set_max_threads(pool_, max_tasks);
    ignore_generic_ = db_.get("ignoreGeneric", false).toBool();
    default_serial_ = db_.get("serialByDefault", true).toBool();
    serial_log_size_ = db_.get("serialLogSize", 20000000ull).toULongLong();
    serial_log_dir_ = db_.get("serialLogDir", "").toString();

    emit settingsChanged();

    if (started_) {
        stop();
        start();
    }
}

void Monitor::setMaxTasks(unsigned int max_tasks)
{
    if (max_tasks == ty_pool_get_max_threads(pool_))
        return;

    ty_pool_set_max_threads(pool_, max_tasks);

    db_.put("maxTasks", max_tasks);
    emit settingsChanged();
}

void Monitor::setIgnoreGeneric(bool ignore_generic)
{
    if (ignore_generic == ignore_generic_)
        return;

    ignore_generic_ = ignore_generic;

    if (ignore_generic) {
        for (size_t i = 0; i < boards_.size(); i++) {
            auto &board = boards_[i];
            if (board->model() == TY_MODEL_GENERIC) {
                beginRemoveRows(QModelIndex(), static_cast<int>(i), static_cast<int>(i));
                boards_.erase(boards_.begin() + static_cast<int>(i));
                endRemoveRows();
                i--;
            }
        }
    } else {
        ty_monitor_list(monitor_, handleEvent, this);
    }

    db_.put("ignoreGeneric", ignore_generic);
    emit settingsChanged();
}

unsigned int Monitor::maxTasks() const
{
    return ty_pool_get_max_threads(pool_);
}

void Monitor::setSerialByDefault(bool default_serial)
{
    if (default_serial == default_serial_)
        return;

    default_serial_ = default_serial;

    for (auto &board: boards_) {
        auto db = board->database();

        if (!db.get("enableSerial").isValid()) {
            board->setEnableSerial(default_serial);
            db.remove("enableSerial");
        }
    }

    db_.put("serialByDefault", default_serial);
    emit settingsChanged();
}

void Monitor::setSerialLogSize(size_t default_size)
{
    if (default_size == serial_log_size_)
        return;

    serial_log_size_ = default_size;

    for (auto &board: boards_) {
        auto db = board->database();

        if (!db.get("serialLogSize").isValid()) {
            board->setSerialLogSize(default_size);
            board->updateSerialLogState(false);
            db.remove("serialLogSize");
        }
    }

    db_.put("serialLogSize", static_cast<qulonglong>(default_size));
    emit settingsChanged();
}

void Monitor::setSerialLogDir(const QString &dir)
{
    if (dir == serial_log_dir_)
        return;

    serial_log_dir_ = dir;

    for (auto &board: boards_) {
        board->serial_log_dir_ = dir;
        board->updateSerialLogState(true);
        emit board->settingsChanged();
    }

    db_.put("serialLogDir", dir);
    emit settingsChanged();
}

bool Monitor::start()
{
    if (started_)
        return true;

    int r;

    if (!monitor_) {
        ty_monitor *monitor;

        r = ty_monitor_new(&monitor);
        if (r < 0)
            return false;
        unique_ptr<ty_monitor, decltype(&ty_monitor_free)> monitor_ptr(monitor, ty_monitor_free);

        r = ty_monitor_register_callback(monitor, handleEvent, this);
        if (r < 0)
            return false;

        ty_descriptor_set set = {};
        ty_monitor_get_descriptors(monitor, &set, 1);
        monitor_notifier_.setDescriptorSet(&set);
        connect(&monitor_notifier_, &DescriptorNotifier::activated, this, &Monitor::refresh);

        monitor_ = monitor_ptr.release();
    }

    serial_thread_.start();

    r = ty_monitor_start(monitor_);
    if (r < 0)
        return false;
    monitor_notifier_.setEnabled(true);

    started_ = true;
    return true;
}

void Monitor::stop()
{
    if (!started_)
        return;

    serial_thread_.quit();
    serial_thread_.wait();

    if (!boards_.empty()) {
        beginRemoveRows(QModelIndex(), 0, static_cast<int>(boards_.size()));
        boards_.clear();
        endRemoveRows();
    }

    monitor_notifier_.setEnabled(false);
    ty_monitor_stop(monitor_);

    started_ = false;
}

vector<shared_ptr<Board>> Monitor::boards()
{
    return boards_;
}

shared_ptr<Board> Monitor::board(unsigned int i)
{
    if (i >= boards_.size())
        return nullptr;

    return boards_[i];
}

unsigned int Monitor::boardCount() const
{
    return static_cast<unsigned int>(boards_.size());
}

shared_ptr<Board> Monitor::boardFromModel(const QAbstractItemModel *model,
                                          const QModelIndex &index)
{
    auto board = model->data(index, Monitor::ROLE_BOARD).value<Board *>();
    return board ? board->shared_from_this() : nullptr;
}

vector<shared_ptr<Board>> Monitor::find(function<bool(Board &board)> filter)
{
    auto boards = boards_;

    vector<shared_ptr<Board>> matches;
    matches.reserve(boards_.size());
    for (auto &board: boards_) {
        if (filter(*board))
            matches.push_back(board);
    }

    return matches;
}

int Monitor::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(boards_.size());
}

int Monitor::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return COLUMN_COUNT;
}

QVariant Monitor::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case COLUMN_BOARD:
        return tr("Board");
    case COLUMN_STATUS:
        return tr("Status");
    case COLUMN_IDENTITY:
        return tr("Identity");
    case COLUMN_LOCATION:
        return tr("Location");
    case COLUMN_SERIAL_NUMBER:
        return tr("Serial Number");
    case COLUMN_DESCRIPTION:
        return tr("Description");
    }

    return QVariant();
}

QVariant Monitor::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(boards_.size()))
        return QVariant();

    auto board = boards_[index.row()];
    if (role == ROLE_BOARD)
        return QVariant::fromValue(board.get());

    if (index.column() == 0) {
        switch (role) {
            case Qt::ToolTipRole:
                return tr("%1\n+ Location: %2\n+ Serial Number: %3\n+ Status: %4\n+ Capabilities: %5")
                       .arg(board->modelName())
                       .arg(board->location())
                       .arg(board->serialNumber())
                       .arg(board->statusText())
                       .arg(Board::makeCapabilityString(board->capabilities(), tr("(none)")));
            case Qt::DecorationRole:
                return board->statusIcon();
            case Qt::EditRole:
                return board->tag();
        }
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case COLUMN_BOARD:
            return board->tag();
        case COLUMN_MODEL:
            return board->modelName();
        case COLUMN_STATUS:
            return board->statusText();
        case COLUMN_IDENTITY:
            return board->id();
        case COLUMN_LOCATION:
            return board->location();
        case COLUMN_SERIAL_NUMBER:
            return board->serialNumber();
        case COLUMN_DESCRIPTION:
            return board->description();
        }
    }

    return QVariant();
}

Qt::ItemFlags Monitor::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool Monitor::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= static_cast<int>(boards_.size()))
        return false;

    auto board = boards_[index.row()];
    board->setTag(value.toString());

    return true;
}

void Monitor::refresh(ty_descriptor desc)
{
    Q_UNUSED(desc);
    ty_monitor_refresh(monitor_);
}

int Monitor::handleEvent(ty_board *board, ty_monitor_event event, void *udata)
{
    auto self = static_cast<Monitor *>(udata);

    switch (event) {
    case TY_MONITOR_EVENT_ADDED:
        self->handleAddedEvent(board);
        break;

    case TY_MONITOR_EVENT_CHANGED:
    case TY_MONITOR_EVENT_DISAPPEARED:
    case TY_MONITOR_EVENT_DROPPED:
        self->handleChangedEvent(board);
        break;
    }

    return 0;
}

Monitor::iterator Monitor::findBoardIterator(ty_board *board)
{
    return find_if(boards_.begin(), boards_.end(),
                   [=](std::shared_ptr<Board> &ptr) { return ptr->board() == board; });
}

void Monitor::handleAddedEvent(ty_board *board)
{
    if (ignore_generic_ && ty_board_get_model(board) == TY_MODEL_GENERIC)
        return;
    if (findBoardIterator(board) != boards_.end())
        return;

    // Work around the private constructor for make_shared()
    struct BoardSharedEnabler : public Board {
        BoardSharedEnabler(ty_board *board)
            : Board(board) {}
    };
    auto board_wrapper_ptr = make_shared<BoardSharedEnabler>(board);
    auto board_wrapper = board_wrapper_ptr.get();

    if (board_wrapper->hasCapability(TY_BOARD_CAPABILITY_UNIQUE))
        configureBoardDatabase(*board_wrapper);
    board_wrapper->serial_log_dir_ = serial_log_dir_;
    board_wrapper->loadSettings(this);

    board_wrapper->setThreadPool(pool_);
    board_wrapper->serial_notifier_.moveToThread(&serial_thread_);

    connect(board_wrapper, &Board::infoChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    // Don't capture board_wrapper_ptr, this should be obvious but I made the mistake once
    connect(board_wrapper, &Board::interfacesChanged, this, [=]() {
        if (db_.isValid() && !board_wrapper->database().isValid() &&
                board_wrapper->hasCapability(TY_BOARD_CAPABILITY_UNIQUE)) {
            configureBoardDatabase(*board_wrapper);
            board_wrapper->loadSettings(this);
        }
        refreshBoardItem(findBoardIterator(board));
    });
    connect(board_wrapper, &Board::statusChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    connect(board_wrapper, &Board::progressChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    connect(board_wrapper, &Board::dropped, this, [=]() {
        removeBoardItem(findBoardIterator(board));
    });

    beginInsertRows(QModelIndex(), static_cast<int>(boards_.size()),
                    static_cast<int>(boards_.size()));
    boards_.push_back(board_wrapper_ptr);
    endInsertRows();

    emit boardAdded(board_wrapper);
}

void Monitor::handleChangedEvent(ty_board *board)
{
    auto it = findBoardIterator(board);
    if (it == boards_.end())
        return;
    auto ptr = *it;

    ptr->refreshBoard();
}

void Monitor::refreshBoardItem(iterator it)
{
    auto index = createIndex(it - boards_.begin(), 0);
    dataChanged(index, index);
}

void Monitor::removeBoardItem(iterator it)
{
    beginRemoveRows(QModelIndex(), it - boards_.begin(), it - boards_.begin());
    boards_.erase(it);
    endRemoveRows();
}

void Monitor::configureBoardDatabase(Board &board)
{
    board.setDatabase(db_.subDatabase(board.id()));
    board.setCache(cache_.subDatabase(board.id()));
}
