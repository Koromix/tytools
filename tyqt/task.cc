/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QMutexLocker>

#include "ty.h"
#include "task.hh"
#include "tyqt.hh"

using namespace std;

void Task::reportLog(ty_log_level level, const QString &msg)
{
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyLog(level, msg);
}

void Task::reportStarted()
{
    status_ = TY_TASK_STATUS_RUNNING;

    intf_.reportStarted();
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyStarted();
}

void Task::reportFinished(bool success)
{
    status_ = TY_TASK_STATUS_FINISHED;

    intf_.reportFinished(&success);
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyFinished(success);
}

void Task::reportProgress(const QString &action, unsigned int value, unsigned int max)
{
    progress_ = value;
    progress_max_ = max;

    intf_.setProgressRange(0, max);
    intf_.setProgressValue(value);
    QMutexLocker locker(&listeners_lock_);
    for (auto &l: listeners_)
        l->notifyProgress(action, value, max);
}

ty_task_status Task::status() const
{
    return status_;
}

unsigned int Task::progress() const
{
    return progress_;
}

unsigned int Task::progressMaximum() const
{
    return progress_max_;
}

QFuture<bool> Task::future() const
{
    return intf_.future();
}

TyTask::TyTask(ty_task *task)
    : task_(task)
{
    ty_task_set_callback(task, [](struct ty_task *task, ty_message_type type, const void *data, void *udata) {
        TY_UNUSED(task);

        auto task2 = static_cast<TyTask *>(udata);
        task2->reportMessage(type, data);
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

void TyTask::reportMessage(ty_message_type type, const void *data)
{
    /* The task is doing something, we don't need to keep it alive anymore... it'll keep this
       object alive instead. */
    if (task_ && type == TY_MESSAGE_STATUS) {
        ty_task_set_cleanup(task_, [](ty_task *task, void *udata) {
            TY_UNUSED(task);

            auto task_ptr = static_cast<shared_ptr<Task> *>(udata);
            delete task_ptr;
        }, new shared_ptr<Task>(shared_from_this()));

        ty_task_unref(task_);
        task_ = NULL;
    }

    switch (type) {
    case TY_MESSAGE_LOG:
        reportLog(data);
        break;
    case TY_MESSAGE_STATUS:
        reportStatus(data);
        break;
    case TY_MESSAGE_PROGRESS:
        reportProgress(data);
        break;
    }
}

void TyTask::reportLog(const void *data)
{
    auto msg = static_cast<const ty_log_message *>(data);
    Task::reportLog(msg->level, msg->msg);
}

void TyTask::reportStatus(const void *data)
{
    auto msg = static_cast<const ty_status_message *>(data);

    switch (msg->status) {
    case TY_TASK_STATUS_RUNNING:
        reportStarted();
        break;
    case TY_TASK_STATUS_FINISHED:
        reportFinished(!ty_task_get_return_value(msg->task));
        break;

    default:
        break;
    }
}

void TyTask::reportProgress(const void *data)
{
    auto msg = static_cast<const ty_progress_message *>(data);
    Task::reportProgress(msg->action, msg->value, msg->max);
}

bool ImmediateTask::start()
{
    if (status() >= TY_TASK_STATUS_PENDING)
        return true;

    reportStarted();
    bool ret = f_();
    reportFinished(ret);

    return true;
}

bool FailedTask::start()
{
    if (status() >= TY_TASK_STATUS_PENDING)
        return true;

    reportStarted();
    if (!msg_.isEmpty()) {
        tyQt->reportError(msg_);
        reportLog(TY_LOG_ERROR, msg_);
    }
    reportFinished(false);

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

QFuture<bool> TaskInterface::future() const
{
    return task_->future();
}

TaskListener::~TaskListener()
{
    if (task_) {
        QMutexLocker locker(&task_->listeners_lock_);
        task_->listeners_.removeOne(this);
    }
}

void TaskListener::setTask(TaskInterface *task)
{
    task_->listeners_lock_.lock();
    task_->listeners_.removeOne(this);
    task_->listeners_lock_.unlock();

    if (task) {
        task_ = task->task_;

        QMutexLocker locker(&task_->listeners_lock_);
        task_->listeners_.append(this);
    } else {
        task_ = make_shared<FailedTask>();
    }
}

TaskInterface TaskListener::task() const
{
    return TaskInterface(task_);
}

void TaskListener::notifyLog(ty_log_level level, const QString &msg)
{
    TY_UNUSED(level);
    TY_UNUSED(msg);
}

void TaskListener::notifyStarted()
{
}

void TaskListener::notifyFinished(bool success)
{
    TY_UNUSED(success);
}

void TaskListener::notifyProgress(const QString &action, unsigned int value, unsigned int max)
{
    TY_UNUSED(action);
    TY_UNUSED(value);
    TY_UNUSED(max);
}

void TaskWatcher::notifyLog(ty_log_level level, const QString &msg)
{
    emit log(level, msg);
}

void TaskWatcher::notifyStarted()
{
    emit started();
}

void TaskWatcher::notifyFinished(bool success)
{
    emit finished(success);
}

void TaskWatcher::notifyProgress(const QString &action, unsigned int value, unsigned int max)
{
    emit progress(action, value, max);
}
