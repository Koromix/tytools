/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TASK_HH
#define TASK_HH

#include <QFuture>
#include <QFutureInterface>
#include <QMutex>

#include <functional>
#include <memory>
#include <vector>

#include "ty/task.h"

class Task : public std::enable_shared_from_this<Task> {
    ty_task_status status_ = TY_TASK_STATUS_READY;
    unsigned int progress_ = 0, progress_max_ = 0;
    bool success_ = false;
    std::shared_ptr<void> result_;

    mutable QFutureInterface<bool> intf_;

    QMutex listeners_lock_{QMutex::Recursive};
    std::vector<class TaskListener *> listeners_;

public:
    Task() {}
    virtual ~Task() {}

    Task& operator=(const Task &other) = delete;
    Task(const Task &other) = delete;
    Task& operator=(const Task &&other) = delete;
    Task(const Task &&other) = delete;

    virtual bool start() = 0;

    ty_task_status status() const { return status_; }
    unsigned int progress() const { return progress_; }
    unsigned int progressMaximum() const { return progress_max_; }
    bool success() const { return success_; }
    std::shared_ptr<void> result() const { return result_; }

    QFuture<bool> future() const { return intf_.future(); }

    void reportLog(ty_log_level level, const QString &msg);
    void reportStarted();
    void reportFinished(bool success, std::shared_ptr<void> result);
    void reportProgress(const QString &action, unsigned int value, unsigned int max);

    void addListener(TaskListener *listener);
    void removeListener(TaskListener *listener);
};

class TyTask : public Task {
    ty_task *task_;

public:
    TyTask(ty_task *task);
    ~TyTask() override;

    bool start() override;

private:
    void notifyMessage(ty_message_type type, const void *data);
    void notifyLog(const void *data);
    void notifyStatus(const void *data);
    void notifyProgress(const void *data);
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
    bool success() const;
    std::shared_ptr<void> result() const;

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
    TaskInterface task() const { return TaskInterface(task_); }

protected:
    virtual void notifyLog(ty_log_level level, const QString &msg);
    virtual void notifyStarted();
    virtual void notifyFinished(bool success, std::shared_ptr<void> result);
    virtual void notifyProgress(const QString &action, unsigned int value, unsigned int max);

    friend class Task;
};

class TaskWatcher : public QObject, public TaskListener {
    Q_OBJECT

public:
    TaskWatcher(QObject *parent = nullptr)
        : QObject(parent) {}

signals:
    void log(ty_log_level level, const QString &msg);
    void started();
    void finished(bool success, std::shared_ptr<void> result);
    void progress(const QString &action, unsigned int value, unsigned int max);

protected:
    void notifyLog(ty_log_level level, const QString &msg) override;
    void notifyStarted() override;
    void notifyFinished(bool success, std::shared_ptr<void> result) override;
    void notifyProgress(const QString &action, unsigned int value, unsigned int max) override;
};

#endif
