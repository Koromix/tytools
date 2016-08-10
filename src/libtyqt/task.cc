/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QMutexLocker>

#include "tyqt/task.hpp"

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

void Task::reportProgress(const QString &action, unsigned int value, unsigned int max)
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
    ty_task_set_callback(task, [](struct ty_task *task, ty_message_type type, const void *data, void *udata) {
        Q_UNUSED(task);

        auto task2 = static_cast<TyTask *>(udata);
        task2->notifyMessage(type, data);
    }, this);
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

void TyTask::notifyMessage(ty_message_type type, const void *data)
{
    /* The task is doing something, we don't need to keep it alive anymore... it'll keep this
       object alive instead. */
    if (task_ && type == TY_MESSAGE_STATUS) {
        ty_task_set_cleanup(task_, [](void *ptr) {
            auto task_ptr = static_cast<shared_ptr<Task> *>(ptr);
            delete task_ptr;
        }, new shared_ptr<Task>(shared_from_this()));

        ty_task_unref(task_);
        task_ = NULL;
    }

    switch (type) {
    case TY_MESSAGE_LOG:
        notifyLog(data);
        break;
    case TY_MESSAGE_STATUS:
        notifyStatus(data);
        break;
    case TY_MESSAGE_PROGRESS:
        notifyProgress(data);
        break;
    }
}

void TyTask::notifyLog(const void *data)
{
    auto msg = static_cast<const ty_log_message *>(data);
    reportLog(msg->level, msg->msg);
}

void TyTask::notifyStatus(const void *data)
{
    auto msg = static_cast<const ty_status_message *>(data);

    switch (msg->status) {
    case TY_TASK_STATUS_PENDING:
        reportPending();
        break;
    case TY_TASK_STATUS_RUNNING:
        reportStarted();
        break;
    case TY_TASK_STATUS_FINISHED: {
        ty_task_cleanup_func *f;
        void *result = ty_task_steal_result(msg->task, &f);
        if (!f)
            f = [](void *ptr) { Q_UNUSED(ptr); };
        reportFinished(ty_task_get_return_value(msg->task) >= 0, shared_ptr<void>(result, f));
        break;
    }

    default:
        break;
    }
}

void TyTask::notifyProgress(const void *data)
{
    auto msg = static_cast<const ty_progress_message *>(data);
    reportProgress(msg->action, msg->value, msg->max);
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

ty_task_status TaskInterface::status() const
{
    return task_->status();
}

unsigned int TaskInterface::progress() const
{
    return task_->progress();
}

unsigned int TaskInterface::progressMaximum() const
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

void TaskListener::notifyProgress(const QString &action, unsigned int value, unsigned int max)
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

void TaskWatcher::notifyProgress(const QString &action, unsigned int value, unsigned int max)
{
    emit progress(action, value, max);
}
