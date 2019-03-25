/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TASK_HH
#define TASK_HH

#include <QMutex>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

#include "../libty/task.h"

class Task : public std::enable_shared_from_this<Task> {
    ty_task_status status_ = TY_TASK_STATUS_READY;
    uint64_t progress_ = 0, progress_max_ = 0;
    bool success_ = false;
    std::shared_ptr<void> result_;

    QMutex listeners_lock_{QMutex::Recursive};
    std::vector<class TaskListener *> listeners_;

protected:
    QString name_;

public:
    Task() {}
    virtual ~Task() {}

    Task& operator=(const Task &other) = delete;
    Task(const Task &other) = delete;
    Task& operator=(const Task &&other) = delete;
    Task(const Task &&other) = delete;

    virtual bool start() = 0;

    QString name() const { return name_; }
    ty_task_status status() const { return status_; }
    uint64_t progress() const { return progress_; }
    uint64_t progressMaximum() const { return progress_max_; }
    bool success() const { return success_; }
    std::shared_ptr<void> result() const { return result_; }

    void reportLog(ty_log_level level, const QString &msg);
    void reportPending();
    void reportStarted();
    void reportFinished(bool success, std::shared_ptr<void> result);
    void reportProgress(const QString &action, uint64_t value, uint64_t max);

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
    void notifyMessage(const ty_message_data *msg);
    void notifyLog(const ty_message_data *msg);
    void notifyStatus(const ty_message_data *msg);
    void notifyProgress(const ty_message_data *msg);
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

    QString name() const;
    ty_task_status status() const;
    uint64_t progress() const;
    uint64_t progressMaximum() const;
    bool success() const;
    std::shared_ptr<void> result() const;

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
    virtual void notifyPending();
    virtual void notifyStarted();
    virtual void notifyFinished(bool success, std::shared_ptr<void> result);
    virtual void notifyProgress(const QString &action, uint64_t value, uint64_t max);

    friend class Task;
};

class TaskWatcher : public QObject, public TaskListener {
    Q_OBJECT

public:
    TaskWatcher(QObject *parent = nullptr)
        : QObject(parent) {}

signals:
    void log(ty_log_level level, const QString &msg);
    void pending();
    void started();
    void finished(bool success, std::shared_ptr<void> result);
    void progress(const QString &action, uint64_t value, uint64_t max);

protected:
    void notifyLog(ty_log_level level, const QString &msg) override;
    void notifyPending() override;
    void notifyStarted() override;
    void notifyFinished(bool success, std::shared_ptr<void> result) override;
    void notifyProgress(const QString &action, uint64_t value, uint64_t max) override;
};

#endif
