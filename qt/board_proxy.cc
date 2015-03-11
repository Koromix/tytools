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
#include "board_proxy.hh"

using namespace std;

static const int manual_reboot_delay = 5000;

class BoardCommand : public QEvent {
    QString msg_;

    ty_board *board_;
    const function<void(BoardProxyWorker *, ty_board *)> f_;

public:
    BoardCommand(ty_board *board, function<void(BoardProxyWorker *, ty_board *)> f, QString msg = QString());
    ~BoardCommand();

    QString msg() const;

    void execute(BoardProxyWorker *worker);
};

BoardCommand::BoardCommand(ty_board *board, function<void(BoardProxyWorker *, ty_board *)> f, QString msg)
    : QEvent(QEvent::User), msg_(msg), board_(ty_board_ref(board)), f_(f)
{
}

BoardCommand::~BoardCommand()
{
    ty_board_unref(board_);
}

QString BoardCommand::msg() const
{
    return msg_;
}

void BoardCommand::execute(BoardProxyWorker *worker)
{
    f_(worker, board_);
    emit worker->taskProgress("", 0, 0);
}

void BoardProxyWorker::customEvent(QEvent *ev)
{
    if (ev->type() != QEvent::User)
        return;

    running_task_ = static_cast<BoardCommand *>(ev);
    running_task_->execute(this);

    running_task_ = nullptr;
}

void BoardProxyWorker::reportTaskProgress(unsigned int progress, unsigned int total)
{
    if (!running_task_)
        return;

    emit taskProgress(running_task_->msg(), progress, total);
}

BoardProxy::BoardProxy(ty_board *board, QObject *parent)
    : QObject(parent), board_(ty_board_ref(board))
{
    thread_ = new QThread(parent);
    thread_->start();

    worker_ = new BoardProxyWorker();
    worker_->moveToThread(thread_);

    // This construct has been valid since Qt 4.8
    connect(thread_, &QThread::finished, worker_, &QThread::deleteLater);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);

    connect(worker_, &BoardProxyWorker::taskProgress, this, &BoardProxy::reportTaskProgress);

    serial_document_.setDocumentLayout(new QPlainTextDocumentLayout(&serial_document_));
    serial_document_.setMaximumBlockCount(10000);

    serial_notifier_.setMinInterval(5);
    connect(&serial_notifier_, &DescriptorSetNotifier::activated, this, &BoardProxy::serialReceived);

    refreshBoard();
}

BoardProxy::~BoardProxy()
{
    ty_board_unref(board_);
    thread_->quit();
}

ty_board *BoardProxy::board() const
{
    return board_;
}

bool BoardProxy::matchesIdentity(const QString &id)
{
    return ty_board_matches_identity(board_, id.toLocal8Bit().constData()) == 1;
}

ty_board_state BoardProxy::state() const
{
    return ty_board_get_state(board_);
}

uint16_t BoardProxy::capabilities() const
{
    return ty_board_get_capabilities(board_);
}

const ty_board_model *BoardProxy::model() const
{
    return ty_board_get_model(board_);
}

QString BoardProxy::modelName() const
{
    auto model = ty_board_get_model(board_);
    if (!model)
        return tr("(unknown)");

    return ty_board_model_get_name(model);
}

QString BoardProxy::modelDesc() const
{
    auto model = ty_board_get_model(board_);
    if (!model)
        return tr("(unknown)");

    return ty_board_model_get_desc(model);
}

QString BoardProxy::identity() const
{
    return ty_board_get_identity(board_);
}

QString BoardProxy::location() const
{
    return ty_board_get_location(board_);
}

uint64_t BoardProxy::serialNumber() const
{
    return ty_board_get_serial_number(board_);
}

std::vector<BoardInterfaceInfo> BoardProxy::interfaces() const
{
    std::vector<BoardInterfaceInfo> vec;

    ty_board_list_interfaces(board_, [](ty_board_interface *iface, void *udata) {
        BoardInterfaceInfo info;
        info.desc = ty_board_interface_get_desc(iface);
        info.path = ty_board_interface_get_path(iface);
        info.capabilities = ty_board_interface_get_capabilities(iface);
        info.number = ty_board_interface_get_interface_number(iface);

        auto vec = reinterpret_cast<std::vector<BoardInterfaceInfo> *>(udata);
        vec->push_back(info);

        return 0;
    }, &vec);

    return vec;
}

bool BoardProxy::isUploadAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_UPLOAD) || isRebootAvailable();
}

bool BoardProxy::isResetAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_RESET) || isRebootAvailable();
}

bool BoardProxy::isRebootAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_SERIAL);
}

bool BoardProxy::isSerialAvailable() const
{
    return ty_board_has_capability(board_, TY_BOARD_CAPABILITY_SERIAL);
}

QTextDocument &BoardProxy::serialDocument()
{
    return serial_document_;
}

void BoardProxy::appendToSerialDocument(const QString &s)
{
    QTextCursor cursor(&serial_document_);
    cursor.movePosition(QTextCursor::End);

    cursor.insertText(s);
}

QString BoardProxy::runningTask(unsigned int *progress, unsigned int *total) const
{
    if (progress)
        *progress = task_progress_;
    if (total)
        *total = task_total_;

    return task_msg_;
}

bool BoardProxy::event(QEvent *e)
{
    if (e->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *ce = static_cast<QDynamicPropertyChangeEvent *>(e);
        const char *name = ce->propertyName().constData();

        emit propertyChanged(name, property(name));
    }

    return QObject::event(e);
}

QStringList BoardProxy::makeCapabilityList(uint16_t capabilities)
{
    QStringList list;

    for (unsigned int i = 0; i < TY_BOARD_CAPABILITY_COUNT; i++) {
        if (capabilities & (1 << i))
            list.append(ty_board_get_capability_name(static_cast<ty_board_capability>(i)));
    }

    return list;
}

QString BoardProxy::makeCapabilityString(uint16_t capabilities, QString empty_str)
{
    QStringList list = makeCapabilityList(capabilities);

    if (list.isEmpty()) {
        return empty_str;
    } else {
        return list.join(", ");
    }
}

void BoardProxy::upload(const QString &filename, bool reset_after)
{
    BoardCommand *cmd = new BoardCommand(board_, [filename, reset_after](BoardProxyWorker *worker, ty_board *board) {
        ty_firmware *firmware;

        emit worker->reportTaskProgress();

        if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD)) {
            ty_board_reboot(board);

            int r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_UPLOAD, true, manual_reboot_delay);
            if (r < 0)
                return;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Reboot does not seem to work, trigger manually");
                return;
            }
        }

        int r = ty_firmware_load(filename.toLocal8Bit().constData(), nullptr, &firmware);
        if (r < 0)
            return;
        unique_ptr<ty_firmware, decltype(ty_firmware_free) *> firmware_ptr(firmware, ty_firmware_free);

        r = ty_board_upload(board, firmware, 0, [](const ty_board *board, const ty_firmware *f, size_t uploaded, void *udata) {
            TY_UNUSED(board);

            BoardProxyWorker *worker = static_cast<BoardProxyWorker *>(udata);
            worker->reportTaskProgress(uploaded, f->size);

            return 0;
        }, worker);
        if (r < 0)
            return;
        if (reset_after) {
            ty_board_reset(board);
            QThread::msleep(400);
        }
    }, tr("Uploading"));
    QCoreApplication::postEvent(worker_, cmd);
}

void BoardProxy::reset()
{
    // this can be deleted while the worker thread is working, don't capture it!
    BoardCommand *cmd = new BoardCommand(board_, [](BoardProxyWorker *worker, ty_board *board) {
        worker->reportTaskProgress();

        if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET)) {
            ty_board_reboot(board);

            int r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RESET, true, manual_reboot_delay);
            if (r < 0)
                return;
            if (!r) {
                ty_error(TY_ERROR_TIMEOUT, "Cannot reset board");
                return;
            }
        }

        ty_board_reset(board);
        QThread::msleep(800);
    }, tr("Resetting"));
    QCoreApplication::postEvent(worker_, cmd);
}

void BoardProxy::reboot()
{
    BoardCommand *cmd = new BoardCommand(board_, [](BoardProxyWorker *worker, ty_board *board) {
        TY_UNUSED(worker);

        worker->reportTaskProgress();

        ty_board_reboot(board);
        QThread::msleep(800);
    }, tr("Rebooting"));
    QCoreApplication::postEvent(worker_, cmd);
}

void BoardProxy::sendSerial(const QByteArray &buf)
{
    BoardCommand *cmd = new BoardCommand(board_, [buf](BoardProxyWorker *worker, ty_board *board) {
        TY_UNUSED(worker);

        ty_board_serial_write(board, buf.data(), buf.size());
    });
    QCoreApplication::postEvent(worker_, cmd);
}

void BoardProxy::refreshBoard()
{
    if (ty_board_has_capability(board_, TY_BOARD_CAPABILITY_SERIAL)) {
        if (!serial_available_) {
            ty_descriptor_set set = {0};
            ty_board_get_descriptors(board_, TY_BOARD_CAPABILITY_SERIAL, &set, 1);

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

void BoardProxy::serialReceived(ty_descriptor desc)
{
    TY_UNUSED(desc);

    char buf[128];

    ssize_t r = ty_board_serial_read(board_, buf, sizeof(buf), 0);
    if (r < 0) {
        serial_notifier_.clear();
        return;
    }
    if (!r)
        return;

    appendToSerialDocument(QString::fromLocal8Bit(buf, r));
}

void BoardProxy::reportTaskProgress(const QString &msg, unsigned int progress, unsigned int total)
{
    task_msg_ = msg;
    task_progress_ = progress;
    task_total_ = total;

    emit taskProgress(*this, msg, progress, total);
}

BoardManagerProxy::~BoardManagerProxy()
{
    // Just making sure nothing depends on the manager when it's destroyed
    manager_notifier_.clear();
    boards_.clear();

    ty_board_manager_free(manager_);
}

bool BoardManagerProxy::start()
{
    if (manager_)
        return true;

    int r = ty_board_manager_new(&manager_);
    if (r < 0)
        return false;
    r = ty_board_manager_register_callback(manager_, [](ty_board *board, ty_board_event event, void *udata) {
        BoardManagerProxy *model = static_cast<BoardManagerProxy *>(udata);
        return model->handleEvent(board, event);
    }, this);
    if (r < 0) {
        ty_board_manager_free(manager_);
        manager_ = nullptr;

        return false;
    }

    ty_descriptor_set set = {0};
    ty_board_manager_get_descriptors(manager_, &set, 1);

    manager_notifier_.setDescriptorSet(&set);
    connect(&manager_notifier_, &DescriptorSetNotifier::activated, this, &BoardManagerProxy::refreshManager);

    ty_board_manager_refresh(manager_);

    return true;
}

vector<shared_ptr<BoardProxy>> BoardManagerProxy::boards()
{
    return boards_;
}

shared_ptr<BoardProxy> BoardManagerProxy::board(unsigned int i)
{
    if (i >= boards_.size())
        return nullptr;

    return boards_[i];
}

unsigned int BoardManagerProxy::boardCount() const
{
    return boards_.size();
}

int BoardManagerProxy::rowCount(const QModelIndex &parent) const
{
    TY_UNUSED(parent);

    return boards_.size();
}

int BoardManagerProxy::columnCount(const QModelIndex &parent) const
{
    TY_UNUSED(parent);

    return 2;
}

QVariant BoardManagerProxy::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant BoardManagerProxy::data(const QModelIndex &index, int role) const
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
                           .arg(BoardProxy::makeCapabilityString(board->capabilities(), tr("(none)")))
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

Qt::ItemFlags BoardManagerProxy::flags(const QModelIndex &index) const
{
    if (index.row() >= static_cast<int>(boards_.size()))
        return 0;

    if (boards_[index.row()]->state() == TY_BOARD_STATE_ONLINE) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    } else {
        return Qt::ItemIsSelectable;
    }
}

void BoardManagerProxy::refreshManager(ty_descriptor desc)
{
    TY_UNUSED(desc);

    ty_board_manager_refresh(manager_);
}

void BoardManagerProxy::updateTaskProgress(const BoardProxy &board, const QString &msg, size_t progress, size_t total)
{
    TY_UNUSED(msg);
    TY_UNUSED(progress);
    TY_UNUSED(total);

    auto it = find_if(boards_.begin(), boards_.end(), [&](auto &ptr) { return ptr.get() == &board; });

    QModelIndex index = createIndex(it - boards_.begin(), 0);
    dataChanged(index, index);
}

int BoardManagerProxy::handleEvent(ty_board *board, ty_board_event event)
{
    switch (event) {
    case TY_BOARD_EVENT_ADDED:
        handleAddedEvent(board);
        break;

    case TY_BOARD_EVENT_CHANGED:
    case TY_BOARD_EVENT_DISAPPEARED:
        handleChangedEvent(board);
        break;

    case TY_BOARD_EVENT_DROPPED:
        handleDroppedEvent(board);
        break;
    }

    return 0;
}

void BoardManagerProxy::handleAddedEvent(ty_board *board)
{
    auto board_proxy = make_shared<BoardProxy>(board);

    connect(board_proxy.get(), &BoardProxy::taskProgress, this, &BoardManagerProxy::updateTaskProgress);

    beginInsertRows(QModelIndex(), boards_.size(), boards_.size());
    boards_.push_back(board_proxy);
    endInsertRows();

    emit boardAdded(board_proxy);
}

void BoardManagerProxy::handleChangedEvent(ty_board *board)
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

void BoardManagerProxy::handleDroppedEvent(ty_board *board)
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
