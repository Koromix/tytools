/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QBrush>
#include <QIcon>

#include "board.hh"
#include "database.hh"
#include "descriptor_notifier.hh"
#include "monitor.hh"
#include "ty/task.h"

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
    ty_pool_set_max_threads(pool_, db_.get("maxTasks", 8).toUInt());

    emit settingsChanged();
}

void Monitor::setMaxTasks(unsigned int max_tasks)
{
    ty_pool_set_max_threads(pool_, max_tasks);

    db_.put("maxTasks", max_tasks);
    emit settingsChanged();
}

unsigned int Monitor::maxTasks() const
{
    return ty_pool_get_max_threads(pool_);
}

bool Monitor::start()
{
    if (started_)
        return true;

    int r;

    if (!monitor_) {
        ty_monitor *monitor;

        r = ty_monitor_new(TY_MONITOR_PARALLEL_WAIT, &monitor);
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
        beginRemoveRows(QModelIndex(), 0, boards_.size());
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
    return boards_.size();
}

shared_ptr<Board> Monitor::find(function<bool(Board &board)> filter)
{
    auto board = find_if(boards_.begin(), boards_.end(), [&](shared_ptr<Board> &ptr) { return filter(*ptr); });

    if (board == boards_.end())
        return nullptr;

    return *board;
}

int Monitor::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return boards_.size();
}

int Monitor::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant Monitor::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return tr("Board");
        case 1:
            return tr("Status");
        }
    }

    return QVariant();
}

QVariant Monitor::data(const QModelIndex &index, int role) const
{
    if (index.row() >= static_cast<int>(boards_.size()))
        return QVariant();
    auto board = boards_[index.row()];

    if (index.column() == 0) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return board->tag();
        case Qt::DecorationRole:
            return board->statusIcon();
        case Qt::ToolTipRole:
            return tr("%1\n+ Location: %2\n+ Serial Number: %3\n+ Status: %4\n+ Capabilities: %5")
                   .arg(board->modelName())
                   .arg(board->location())
                   .arg(board->serialNumber())
                   .arg(board->statusText())
                   .arg(Board::makeCapabilityString(board->capabilities(), tr("(none)")));
        case Qt::SizeHintRole:
            return QSize(0, 24);
        }
    } else if (index.column() == 1) {
        /* I don't like putting selector stuff into the base model but we can always
           make a proxy later if there's a problem. */
        switch (role) {
        case Qt::DisplayRole:
            return board->statusText();
        case Qt::ForegroundRole:
            return QBrush(Qt::darkGray);
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
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
    if (role != Qt::EditRole || index.row() >= static_cast<int>(boards_.size()))
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
    auto ptr = Board::createBoard(board);

    if (ptr->hasCapability(TY_BOARD_CAPABILITY_UNIQUE)) {
        ptr->setDatabase(db_.subDatabase(ptr->id()));
        ptr->loadSettings();
    }

    ptr->setThreadPool(pool_);
    ptr->serial_notifier_.moveToThread(&serial_thread_);

    connect(ptr.get(), &Board::infoChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    connect(ptr.get(), &Board::interfacesChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    connect(ptr.get(), &Board::statusChanged, this, [=]() {
        refreshBoardItem(findBoardIterator(board));
    });
    connect(ptr.get(), &Board::dropped, this, [=]() {
        removeBoardItem(findBoardIterator(board));
    });

    beginInsertRows(QModelIndex(), boards_.size(), boards_.size());
    boards_.push_back(ptr);
    endInsertRows();

    emit boardAdded(ptr.get());
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
