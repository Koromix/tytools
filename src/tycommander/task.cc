/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QMutexLocker>

#include "task.hpp"

using namespace std;

void Task::reportLog(ty_log_level level, const QString &msg)
{
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyLog(level, msg);
}

void Task::reportPending()
{
    status_ = TY_TASK_STATUS_PENDING;

    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyPending();
}

void Task::reportStarted()
{
    status_ = TY_TASK_STATUS_RUNNING;

    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyStarted();
}

void Task::reportFinished(bool success, shared_ptr<void> result)
{
    status_ = TY_TASK_STATUS_FINISHED;
    success_ = success;
    result_ = result;
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyFinished(success, result);
}

void Task::reportProgress(const QString &action, uint64_t value, uint64_t max)
{
    progress_ = value;
    progress_max_ = max;

    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyProgress(action, value, max);
}

void Task::addListener(TaskListener *listener)
{
    QMutexLocker locker(&listeners_lock_);
    listeners_.push_back(listener);
}

void Task::removeListener(TaskListener *listener)
{
    QMutexLocker locker(&listeners_lock_);

    auto it = find(listeners_.begin(), listeners_.end(), listener);
    if (it != listeners_.end())
        listeners_.erase(it);
}

TyTask::TyTask(ty_task *task)
    : task_(task)
{
    name_ = task->name;

    task->user_callback = [](const ty_message_data *msg, void *udata) {
        auto task = static_cast<TyTask *>(udata);
        task->notifyMessage(msg);
    };
    task->user_callback_udata = this;
}

TyTask::~TyTask()
{
    ty_task_unref(task_);
}

bool TyTask::start()
{
    if (status() == TY_TASK_STATUS_READY)
        ty_task_start(task_);

    return status() >= TY_TASK_STATUS_PENDING;
}

void TyTask::notifyMessage(const ty_message_data *msg)
{
    /* The task is doing something, we don't need to keep it alive anymore... it'll keep this
       object alive instead. */
    if (task_ && msg->type == TY_MESSAGE_STATUS) {
        task_->user_cleanup = [](void *ptr) {
            auto task_ptr = static_cast<shared_ptr<Task> *>(ptr);
            delete task_ptr;
        };
        task_->user_cleanup_udata = new shared_ptr<Task>(shared_from_this());

        ty_task_unref(task_);
        task_ = NULL;
    }

    switch (msg->type) {
    case TY_MESSAGE_LOG:
        notifyLog(msg);
        break;
    case TY_MESSAGE_STATUS:
        notifyStatus(msg);
        break;
    case TY_MESSAGE_PROGRESS:
        notifyProgress(msg);
        break;
    }
}

void TyTask::notifyLog(const ty_message_data *msg)
{
    reportLog(msg->u.log.level, msg->u.log.msg);
}

void TyTask::notifyStatus(const ty_message_data *msg)
{
    switch (msg->u.task.status) {
    case TY_TASK_STATUS_PENDING:
        reportPending();
        break;
    case TY_TASK_STATUS_RUNNING:
        reportStarted();
        break;
    case TY_TASK_STATUS_FINISHED: {
        void *result = msg->task->result;
        void (*result_cleanup_func)(void *result) = msg->task->result_cleanup;
        msg->task->result_cleanup = NULL;

        if (!result_cleanup_func)
            result_cleanup_func = [](void *result) { Q_UNUSED(result); };
        reportFinished(msg->task->ret >= 0, shared_ptr<void>(result, result_cleanup_func));

        break;
    }

    default:
        break;
    }
}

void TyTask::notifyProgress(const ty_message_data *msg)
{
    reportProgress(msg->u.progress.action, msg->u.progress.value, msg->u.progress.max);
}

bool ImmediateTask::start()
{
    if (status() >= TY_TASK_STATUS_PENDING)
        return true;

    reportStarted();
    bool ret = f_();
    reportFinished(ret, nullptr);

    return true;
}

bool FailedTask::start()
{
    if (status() >= TY_TASK_STATUS_PENDING)
        return true;

    if (!msg_.isEmpty()) {
        ty_log(TY_LOG_ERROR, "%s", msg_.toUtf8().constData());
        reportLog(TY_LOG_ERROR, msg_);
    }
    reportFinished(false, nullptr);

    return true;
}

TaskInterface::TaskInterface(std::shared_ptr<Task> task)
    : task_(task)
{
}

bool TaskInterface::start()
{
    return task_->start();
}

QString TaskInterface::name() const
{
    return task_->name();
}

ty_task_status TaskInterface::status() const
{
    return task_->status();
}

uint64_t TaskInterface::progress() const
{
    return task_->progress();
}

uint64_t TaskInterface::progressMaximum() const
{
    return task_->progressMaximum();
}

bool TaskInterface::success() const
{
    return task_->success();
}

shared_ptr<void> TaskInterface::result() const
{
    return task_->result();
}

TaskListener::~TaskListener()
{
    task_->removeListener(this);
}

void TaskListener::setTask(TaskInterface *task)
{
    task_->removeListener(this);

    if (task) {
        task_ = task->task_;
        task_->addListener(this);
    } else {
        task_ = make_shared<FailedTask>();
    }
}

void TaskListener::notifyLog(ty_log_level level, const QString &msg)
{
    Q_UNUSED(level);
    Q_UNUSED(msg);
}

void TaskListener::notifyPending()
{
}

void TaskListener::notifyStarted()
{
}

void TaskListener::notifyFinished(bool success, shared_ptr<void> result)
{
    Q_UNUSED(success);
    Q_UNUSED(result);
}

void TaskListener::notifyProgress(const QString &action, uint64_t value, uint64_t max)
{
    Q_UNUSED(action);
    Q_UNUSED(value);
    Q_UNUSED(max);
}

void TaskWatcher::notifyLog(ty_log_level level, const QString &msg)
{
    emit log(level, msg);
}

void TaskWatcher::notifyPending()
{
    emit pending();
}

void TaskWatcher::notifyStarted()
{
    emit started();
}

void TaskWatcher::notifyFinished(bool success, shared_ptr<void> result)
{
    emit finished(success, result);
}

void TaskWatcher::notifyProgress(const QString &action, uint64_t value, uint64_t max)
{
    emit progress(action, value, max);
}
