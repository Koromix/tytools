/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCoreApplication>
#include <QDynamicPropertyChangeEvent>
#include <QIcon>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
#include <QThreadPool>

#include <functional>

#include "ty.h"
#include "board.hh"
#include "tyqt.hh"

using namespace std;

class BoardTask : public QRunnable {
    QFutureInterface<QString> intf_;

    shared_ptr<Board> board_;

    QMetaObject::Connection conn_;
    QThread *thread_ = nullptr;
    function<bool(BoardTask &)> f_;

    unsigned int progress_ = 0, total_ = 0;
    QString error_message_;

public:
    BoardTask(Board &board, function<bool(BoardTask &)> f);
    ~BoardTask();

    QFuture<QString> start();

    void setProgress(unsigned int progress, unsigned int total);

private:
    void run();
};

static const int manual_reboot_delay = 5000;

BoardTask::BoardTask(Board &board, function<bool(BoardTask &)> f)
    : board_(board.getSharedPtr()), f_(f)
{
    conn_ = QObject::connect(tyQt, &TyQt::errorMessage, [=](const QString &msg) {
        if (QThread::currentThread() == thread_)
            error_message_ = msg;
    });
}

BoardTask::~BoardTask()
{
    QObject::disconnect(conn_);
}

QFuture<QString> BoardTask::start()
{
    intf_.reportStarted();
    QThreadPool::globalInstance()->start(this);

    return intf_.future();
}

void BoardTask::setProgress(unsigned int progress, unsigned int total)
{
    if (progress < progress_)
        return;

    if (total != total_) {
        total_ = total;
        intf_.setProgressRange(0, total);
    }

    progress_ = progress;
    intf_.setProgressValue(progress);
    emit board_->taskProgress(progress, total);
}

void BoardTask::run()
{
    thread_ = QThread::currentThread();

    bool ret = f_(*this);
    if (!ret) {
        if (!error_message_.isEmpty()) {
            intf_.reportResult(error_message_);
        } else {
            intf_.reportResult("Task failed");
        }
    }

    intf_.reportFinished();
    if (progress_ < total_)
        emit board_->taskProgress(0, 0);
}

Board::Board(tyb_board *board, QObject *parent)
    : QObject(parent), board_(tyb_board_ref(board))
{
    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setMaximumBlockCount(100000);

    serial_notifier_.setMinInterval(5);
    connect(&serial_notifier_, &DescriptorSetNotifier::activated, this, &Board::serialReceived);

    refreshBoard();
}

shared_ptr<Board> Board::createBoard(tyb_board *board)
{
    // Workaround the private constructor for make_shared()
    struct BoardSharedEnabler : public Board
    {
        BoardSharedEnabler(tyb_board *board)
            : Board(board) {}
    };

    return make_shared<BoardSharedEnabler>(board);
}

Board::~Board()
{
    tyb_board_unref(board_);
}

shared_ptr<Board> Board::getSharedPtr()
{
    return shared_from_this();
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
        info.desc = tyb_board_interface_get_desc(iface);
        info.path = tyb_board_interface_get_path(iface);
        info.capabilities = tyb_board_interface_get_capabilities(iface);
        info.number = tyb_board_interface_get_interface_number(iface);

        auto vec = reinterpret_cast<std::vector<BoardInterfaceInfo> *>(udata);
        vec->push_back(info);

        return 0;
    }, &vec);

    return vec;
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
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL);
}

bool Board::isSerialAvailable() const
{
    return tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL);
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

QFuture<QString> Board::runningTask() const
{
    return running_task_;
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
    } else {
        if (serial_available_) {
            serial_available_ = false;
            serial_notifier_.clear();
        }
    }
}

QFuture<QString> Board::upload(const QString &filename, bool reset_after)
{
    return startAsync([=](BoardTask &task) {
        tyb_firmware *firmware;

        if (!tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_UPLOAD)) {
            tyb_board_reboot(board_);

            int r = tyb_board_wait_for(board_, TYB_BOARD_CAPABILITY_UPLOAD, manual_reboot_delay);
            if (r < 0)
                return false;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Reboot does not seem to work, trigger manually");
                return false;
            }
        }

        int r = tyb_firmware_load(filename.toLocal8Bit().constData(), nullptr, &firmware);
        if (r < 0)
            return false;
        unique_ptr<tyb_firmware, decltype(tyb_firmware_free) *> firmware_ptr(firmware, tyb_firmware_free);

        r = tyb_board_upload(board_, firmware, 0, [](const tyb_board *board, const tyb_firmware *f, size_t uploaded, void *udata) {
            TY_UNUSED(board);

            BoardTask *task = static_cast<BoardTask *>(udata);
            task->setProgress(uploaded, f->size);

            return 0;
        }, &task);
        if (r < 0)
            return false;

        if (reset_after) {
            tyb_board_reset(board_);
            QThread::msleep(400);
        }

        return true;
    });
}

QFuture<QString> Board::reset()
{
    return startAsync([=](BoardTask &task) {
        TY_UNUSED(task);

        if (!tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_RESET)) {
            tyb_board_reboot(board_);

            int r = tyb_board_wait_for(board_, TYB_BOARD_CAPABILITY_RESET, manual_reboot_delay);
            if (r < 0)
                return false;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Cannot reset board");
                return false;
            }
        }

        tyb_board_reset(board_);
        QThread::msleep(800);

        return true;
    });
}

QFuture<QString> Board::reboot()
{
    return startAsync([=](BoardTask &task) {
        TY_UNUSED(task);

        tyb_board_reboot(board_);
        QThread::msleep(800);

        return true;
    });
}

QFuture<QString> Board::sendSerial(const QByteArray &buf)
{
    return startAsync([=](BoardTask &task) {
        TY_UNUSED(task);

        tyb_board_serial_write(board_, buf.data(), buf.size());
        return true;
    });
}

void Board::serialReceived(ty_descriptor desc)
{
    TY_UNUSED(desc);

    char buf[1024];

    ssize_t r = tyb_board_serial_read(board_, buf, sizeof(buf), 0);
    if (r < 0) {
        serial_notifier_.clear();
        return;
    }
    if (!r)
        return;

    appendToSerialDocument(QString::fromLocal8Bit(buf, r));
}

/* startAsync/BoardTask make sure Board will exist for as long as the task is running,
   you can capture 'this' safely. */
QFuture<QString> Board::startAsync(function<bool(BoardTask &)> f)
{
    if (running_task_.isRunning()) {
        ty_error(TY_ERROR_BUSY, "A task is already running for board '%s'",
                 tag().toLocal8Bit().constData());
        return QFuture<QString>();
    }

    running_task_ = (new BoardTask(*this, f))->start();
    return running_task_;
}

Manager::~Manager()
{
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
    connect(&manager_notifier_, &DescriptorSetNotifier::activated, this, &Manager::refreshManager);

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
    TY_UNUSED(parent);

    return boards_.size();
}

int Manager::columnCount(const QModelIndex &parent) const
{
    TY_UNUSED(parent);

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
            return QIcon(":/board");
        case Qt::ToolTipRole:
            return QString(tr("%1\n\nCapabilities: %2\nLocation: %3\nSerial Number: %4"))
                           .arg(board->modelName())
                           .arg(Board::makeCapabilityString(board->capabilities(), tr("(none)")))
                           .arg(board->location())
                           .arg(QString::number(board->serialNumber()));
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
            return QBrush(Qt::lightGray);
        case Qt::TextAlignmentRole:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return QVariant();
}

Qt::ItemFlags Manager::flags(const QModelIndex &index) const
{
    if (index.row() >= static_cast<int>(boards_.size()))
        return 0;

    if (boards_[index.row()]->state() == TYB_BOARD_STATE_ONLINE) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    } else {
        return Qt::ItemIsSelectable;
    }
}

void Manager::refreshManager(ty_descriptor desc)
{
    TY_UNUSED(desc);

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

    connect(proxy, &Board::taskProgress, this, [=](unsigned int progress, unsigned int total) {
        TY_UNUSED(progress);
        TY_UNUSED(total);

        refreshBoardItem(proxy);
    });

    beginInsertRows(QModelIndex(), boards_.size(), boards_.size());
    boards_.push_back(proxy_ptr);
    endInsertRows();

    emit boardAdded(proxy_ptr);
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
