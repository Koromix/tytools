/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TASK_HH
#define TASK_HH

#include <QFuture>
#include <QFutureInterface>
#include <QList>
#include <QMutex>

#include <functional>
#include <memory>

#include "ty.h"

class Task : public std::enable_shared_from_this<Task> {
    ty_task_status status_ = TY_TASK_STATUS_READY;
    unsigned int progress_ = 0, progress_max_ = 0;

    mutable QFutureInterface<bool> intf_;

    QMutex listeners_lock_;
    QList<class TaskListener *> listeners_;

public:
    Task() {}
    virtual ~Task() {}

    Task& operator=(const Task &other) = delete;
    Task(const Task &other) = delete;
    Task& operator=(const Task &&other) = delete;
    Task(const Task &&other) = delete;

    virtual bool start() = 0;

    ty_task_status status() const;
    unsigned int progress() const;
    unsigned int progressMaximum() const;

    QFuture<bool> future() const;

    void reportLog(ty_log_level level, const QString &msg);
    void reportStarted();
    void reportFinished(bool success);
    void reportProgress(const QString &action, unsigned int value, unsigned int max);

    friend class TaskListener;
};

class TyTask : public Task {
    ty_task *task_;

public:
    TyTask(ty_task *task);
    ~TyTask() override;

    bool start() override;

private:
    void reportMessage(ty_message_type type, const void *data);
    void reportLog(const void *data);
    void reportStatus(const void *data);
    void reportProgress(const void *data);
};

class ImmediateTask : public Task {
    std::function<bool()> f_;

public:
    ImmediateTask(std::function<bool()> f)
         : f_(f) {}

    bool start() override;
};

class FailedTask : public Task {
    QString msg_;

public:
    FailedTask(const QString &msg = QString())
        : msg_(msg) {}

    bool start() override;
};

class TaskInterface {
    std::shared_ptr<Task> task_;

public:
    TaskInterface(std::shared_ptr<Task> task = std::make_shared<FailedTask>());

    bool start();

    ty_task_status status() const;
    unsigned int progress() const;
    unsigned int progressMaximum() const;

    QFuture<bool> future() const;

    friend class TaskListener;
};

template <typename T, typename... Args>
TaskInterface make_task(Args&&... args)
{
    return TaskInterface(std::make_shared<T>(args...));
}

class TaskListener {
    std::shared_ptr<Task> task_ = std::make_shared<FailedTask>();

public:
    TaskListener() {}
    TaskListener(TaskInterface *task)
        : task_(task->task_) {}
    virtual ~TaskListener();

    void setTask(TaskInterface *task);
    TaskInterface task() const;

protected:
    virtual void notifyLog(ty_log_level level, const QString &msg);
    virtual void notifyStarted();
    virtual void notifyFinished(bool success);
    virtual void notifyProgress(const QString &action, unsigned int value, unsigned int max);

    friend class Task;
};

class TaskWatcher : public QObject, public TaskListener {
    Q_OBJECT

public:
    TaskWatcher(QObject *parent = nullptr)
        : QObject(parent) {}

signals:
    void log(int level, const QString &msg);
    void started();
    void finished(bool success);
    void progress(const QString &action, unsigned int value, unsigned int max);

protected:
    void notifyLog(ty_log_level level, const QString &msg) override;
    void notifyStarted() override;
    void notifyFinished(bool success) override;
    void notifyProgress(const QString &action, unsigned int value, unsigned int max) override;
};

#endif
