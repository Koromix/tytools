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

#include <functional>

#include "ty.h"
#include "board.hh"

using namespace std;

static const int manual_reboot_delay = 5000;

class BoardCommand : public QEvent {
    QString msg_;

    tyb_board *board_;
    const function<void(BoardWorker *, tyb_board *)> f_;

public:
    BoardCommand(tyb_board *board, function<void(BoardWorker *, tyb_board *)> f, QString msg = QString());
    ~BoardCommand();

    QString msg() const;

    void execute(BoardWorker *worker);
};

BoardCommand::BoardCommand(tyb_board *board, function<void(BoardWorker *, tyb_board *)> f, QString msg)
    : QEvent(QEvent::User), msg_(msg), board_(tyb_board_ref(board)), f_(f)
{
}

BoardCommand::~BoardCommand()
{
    tyb_board_unref(board_);
}

QString BoardCommand::msg() const
{
    return msg_;
}

void BoardCommand::execute(BoardWorker *worker)
{
    f_(worker, board_);
    emit worker->taskProgress("", 0, 0);
}

void BoardWorker::customEvent(QEvent *ev)
{
    if (ev->type() != QEvent::User)
        return;

    running_task_ = static_cast<BoardCommand *>(ev);
    running_task_->execute(this);

    running_task_ = nullptr;
}

void BoardWorker::reportTaskProgress(unsigned int progress, unsigned int total)
{
    if (!running_task_)
        return;

    emit taskProgress(running_task_->msg(), progress, total);
}

Board::Board(tyb_board *board, QObject *parent)
    : QObject(parent), board_(tyb_board_ref(board))
{
    thread_ = new QThread(parent);
    thread_->start();

    worker_ = new BoardWorker();
    worker_->moveToThread(thread_);

    // This construct has been valid since Qt 4.8
    connect(thread_, &QThread::finished, worker_, &QThread::deleteLater);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);

    connect(worker_, &BoardWorker::taskProgress, this, &Board::reportTaskProgress);

    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setMaximumBlockCount(100000);

    serial_notifier_.setMinInterval(5);
    connect(&serial_notifier_, &DescriptorSetNotifier::activated, this, &Board::serialReceived);

    refreshBoard();
}

Board::~Board()
{
    tyb_board_unref(board_);
    thread_->quit();
}

tyb_board *Board::board() const
{
    return board_;
}

bool Board::matchesIdentity(const QString &id)
{
    return tyb_board_matches_identity(board_, id.toLocal8Bit().constData()) == 1;
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

QString Board::modelDesc() const
{
    auto model = tyb_board_get_model(board_);
    if (!model)
        return tr("(unknown)");

    return tyb_board_model_get_desc(model);
}

QString Board::identity() const
{
    return tyb_board_get_identity(board_);
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

QString Board::runningTask(unsigned int *progress, unsigned int *total) const
{
    if (progress)
        *progress = task_progress_;
    if (total)
        *total = task_total_;

    return task_msg_;
}

bool Board::event(QEvent *e)
{
    if (e->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *ce = static_cast<QDynamicPropertyChangeEvent *>(e);
        const char *name = ce->propertyName().constData();

        emit propertyChanged(name, property(name));
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

void Board::upload(const QString &filename, bool reset_after)
{
    BoardCommand *cmd = new BoardCommand(board_, [filename, reset_after](BoardWorker *worker, tyb_board *board) {
        tyb_firmware *firmware;

        emit worker->reportTaskProgress();

        if (!tyb_board_has_capability(board, TYB_BOARD_CAPABILITY_UPLOAD)) {
            tyb_board_reboot(board);

            int r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_UPLOAD, true, manual_reboot_delay);
            if (r < 0)
                return;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Reboot does not seem to work, trigger manually");
                return;
            }
        }

        int r = tyb_firmware_load(filename.toLocal8Bit().constData(), nullptr, &firmware);
        if (r < 0)
            return;
        unique_ptr<tyb_firmware, decltype(tyb_firmware_free) *> firmware_ptr(firmware, tyb_firmware_free);

        r = tyb_board_upload(board, firmware, 0, [](const tyb_board *board, const tyb_firmware *f, size_t uploaded, void *udata) {
            TY_UNUSED(board);

            BoardWorker *worker = static_cast<BoardWorker *>(udata);
            worker->reportTaskProgress(uploaded, f->size);

            return 0;
        }, worker);
        if (r < 0)
            return;
        if (reset_after) {
            tyb_board_reset(board);
            QThread::msleep(400);
        }
    }, tr("Uploading"));
    QCoreApplication::postEvent(worker_, cmd);
}

void Board::reset()
{
    // this can be deleted while the worker thread is working, don't capture it!
    BoardCommand *cmd = new BoardCommand(board_, [](BoardWorker *worker, tyb_board *board) {
        worker->reportTaskProgress();

        if (!tyb_board_has_capability(board, TYB_BOARD_CAPABILITY_RESET)) {
            tyb_board_reboot(board);

            int r = tyb_board_wait_for(board, TYB_BOARD_CAPABILITY_RESET, true, manual_reboot_delay);
            if (r < 0)
                return;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Cannot reset board");
                return;
            }
        }

        tyb_board_reset(board);
        QThread::msleep(800);
    }, tr("Resetting"));
    QCoreApplication::postEvent(worker_, cmd);
}

void Board::reboot()
{
    BoardCommand *cmd = new BoardCommand(board_, [](BoardWorker *worker, tyb_board *board) {
        TY_UNUSED(worker);

        worker->reportTaskProgress();

        tyb_board_reboot(board);
        QThread::msleep(800);
    }, tr("Rebooting"));
    QCoreApplication::postEvent(worker_, cmd);
}

void Board::sendSerial(const QByteArray &buf)
{
    BoardCommand *cmd = new BoardCommand(board_, [buf](BoardWorker *worker, tyb_board *board) {
        TY_UNUSED(worker);

        tyb_board_serial_write(board, buf.data(), buf.size());
    });
    QCoreApplication::postEvent(worker_, cmd);
}

void Board::refreshBoard()
{
    if (tyb_board_has_capability(board_, TYB_BOARD_CAPABILITY_SERIAL)) {
        if (!serial_available_) {
            if (clear_on_reset_)
                serial_document_.clear();

            ty_descriptor_set set = {0};
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

void Board::reportTaskProgress(const QString &msg, unsigned int progress, unsigned int total)
{
    task_msg_ = msg;
    task_progress_ = progress;
    task_total_ = total;

    emit taskProgress(*this, msg, progress, total);
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

    int r = tyb_monitor_new(&manager_);
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

    ty_descriptor_set set = {0};
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
            return board->modelDesc();
        case Qt::DecorationRole:
            return QIcon(":/board");
        case Qt::ToolTipRole:
            return QString(tr("%1\n\nCapabilities: %2\nLocation: %3\nSerial Number: %4"))
                           .arg(board->modelDesc())
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
            return board->identity();
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

void Manager::updateTaskProgress(const Board &board, const QString &msg, size_t progress, size_t total)
{
    TY_UNUSED(msg);
    TY_UNUSED(progress);
    TY_UNUSED(total);

    auto it = find_if(boards_.begin(), boards_.end(), [&](auto &ptr) { return ptr.get() == &board; });

    QModelIndex index = createIndex(it - boards_.begin(), 0);
    dataChanged(index, index);
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
    auto board_proxy = make_shared<Board>(board);

    connect(board_proxy.get(), &Board::taskProgress, this, &Manager::updateTaskProgress);

    beginInsertRows(QModelIndex(), boards_.size(), boards_.size());
    boards_.push_back(board_proxy);
    endInsertRows();

    emit boardAdded(board_proxy);
}

void Manager::handleChangedEvent(tyb_board *board)
{
    auto it = find_if(boards_.begin(), boards_.end(), [=](auto &ptr) { return ptr->board() == board; });
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
    auto it = find_if(boards_.begin(), boards_.end(), [=](auto &ptr) { return ptr->board() == board; });
    if (it == boards_.end())
        return;

    auto proxy = *it;
    proxy->refreshBoard();

    beginRemoveRows(QModelIndex(), it - boards_.begin(), it - boards_.begin());
    boards_.erase(it);
    endRemoveRows();

    emit proxy->boardDropped();
}
