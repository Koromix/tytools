/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QDir>
#include <QFileInfo>
#include <QPointer>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include "board.hpp"
#include "client_handler.hpp"
#include "main_window.hpp"
#include "selector_dialog.hpp"
#include "monitor.hpp"
#include "tycommander.hpp"

using namespace std;

const QHash<QString, void (ClientHandler::*)(const QStringList &)> ClientHandler::commands_ = {
    {"workdir", &ClientHandler::setWorkingDirectory},
    {"multi",   &ClientHandler::setMultiSelection},
    {"persist", &ClientHandler::setPersistOption},
    {"select",  &ClientHandler::selectBoard},
    {"open",    &ClientHandler::openMainWindow},
    {"reset",   &ClientHandler::reset},
    {"reboot",  &ClientHandler::reboot},
    {"upload",  &ClientHandler::upload},
    {"attach",  &ClientHandler::attach},
    {"detach",  &ClientHandler::detach}
};

ClientHandler::ClientHandler(unique_ptr<SessionPeer> peer, QObject *parent)
    : QObject(parent), peer_(move(peer))
{
    connect(peer_.get(), &SessionPeer::closed, this, &ClientHandler::closed);
    connect(peer_.get(), &SessionPeer::received, this, &ClientHandler::execute);

#ifdef _WIN32
    peer_->send({"allowsetforegroundwindow", QString::number(GetCurrentProcessId())});
#endif
}

void ClientHandler::execute(const QStringList &arguments)
{
    if (arguments.isEmpty()) {
        notifyLog(TY_LOG_ERROR, tr("Command not specified"));
        notifyFinished(false);
        return;
    }

    auto parameters = arguments;
    auto cmd_name = parameters.takeFirst();

    auto cmd_it = commands_.find(cmd_name);
    if (cmd_it == commands_.end()) {
        notifyLog(TY_LOG_ERROR, tr("Unknown command '%1'").arg(cmd_name));
        notifyFinished(false);
        return;
    }

    (this->**cmd_it)(parameters);
}

void ClientHandler::setWorkingDirectory(const QStringList &parameters)
{
    if (parameters.isEmpty()) {
        notifyLog(TY_LOG_ERROR, "Missing argument for 'workdir' command");
        notifyFinished(false);
        return;
    }

    working_directory_ = parameters[0];
}

void ClientHandler::setMultiSelection(const QStringList &parameters)
{
    multi_ = QVariant(parameters.value(0, "1")).toBool();
}

void ClientHandler::setPersistOption(const QStringList &parameters)
{
    persist_ = QVariant(parameters.value(0, "1")).toBool();
}

void ClientHandler::selectBoard(const QStringList &filters)
{
    if (filters.empty()) {
        notifyLog(TY_LOG_ERROR, "Missing argument for 'select' command");
        notifyFinished(false);
        return;
    }

    filters_.append(filters);
}

void ClientHandler::openMainWindow(const QStringList &parameters)
{
    Q_UNUSED(parameters);

    auto win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->show();

    notifyFinished(true);
}

void ClientHandler::reset(const QStringList &)
{
    auto boards = selectedBoards();
    if (boards.empty())
        return;

    for (auto &board: boards) {
        auto task = board->reset();
        addTask(task);
    }
    executeTasks();
}

void ClientHandler::reboot(const QStringList &)
{
    auto boards = selectedBoards();
    if (boards.empty())
        return;

    for (auto &board: boards) {
        auto task = board->reboot();
        addTask(task);
    }
    executeTasks();
}

void ClientHandler::upload(const QStringList &filenames)
{
    auto monitor = tyCommander->monitor();

    QStringList filenames2;
    filenames2.reserve(filenames.count());
    for (auto filename: filenames) {
        QFileInfo info(working_directory_, filename);
        if (!info.exists()) {
            notifyLog(TY_LOG_ERROR, tr("File '%1' does not exist").arg(filename));
            continue;
        }
        filename = QDir::toNativeSeparators(info.filePath());
        filenames2.append(filename);
    }
    if (filenames2.isEmpty()) {
        notifyFinished(false);
        return;
    }

    if (!monitor->boardCount()) {
        notifyLog(TY_LOG_ERROR, tr("No board available"));
        notifyFinished(false);
        return;
    }

    vector<shared_ptr<Board>> boards;
    if (filters_.isEmpty() && !filenames2.isEmpty()) {
        if (filenames2.count() == 1) {
            boards = monitor->find([&](Board &board) {
                return ty_compare_paths(board.firmware().toLocal8Bit().constData(),
                                        filenames2[0].toLocal8Bit().constData());
            });
        }

        if (boards.empty()) {
            notifyLog(TY_LOG_INFO, "Waiting for user selection");
            notifyStarted();

            auto dialog = new SelectorDialog();
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setExtendedSelection(multi_);
            if (filenames2.count() == 1) {
                dialog->setAction(tr("Upload '%1'").arg(QFileInfo(filenames2[0]).fileName()));
                dialog->setDescription(tr("Upload '%1' to:")
                                       .arg(QFileInfo(filenames2[0]).fileName()));
            } else {
                dialog->setAction(tr("Upload firmwares"));
                dialog->setDescription(tr("Upload firmwares to:"));
            }

            /* This object can be destroyed before the dialog is closed or validated, if the
               client disconnects. We want to complete the task even if that happens, so use
               QPointer to detect it. */
            QPointer<ClientHandler> this_ptr = this;
            connect(dialog, &SelectorDialog::accepted, [=]() {
                if (this_ptr) {
                    auto tasks = makeUploadTasks(dialog->selectedBoards(), filenames2);
                    for (auto &task: tasks)
                        addTask(task);
                    executeTasks();
                } else {
                    auto tasks = makeUploadTasks(dialog->selectedBoards(), filenames2);
                    for (auto &task: tasks)
                        task.start();
                }
            });
            connect(dialog, &SelectorDialog::rejected, this, [=]() {
                notifyLog(TY_LOG_ERROR, "Upload was canceled");
                notifyFinished(false);
            });

            dialog->show();
            dialog->raise();
            dialog->activateWindow();

            return;
        }
    } else {
        boards = selectedBoards();
    }
    if (boards.empty())
        return;

    auto tasks = makeUploadTasks(boards, filenames2);
    for (auto &task: tasks)
        addTask(task);
    executeTasks();
}

void ClientHandler::attach(const QStringList &)
{
    auto boards = selectedBoards();
    if (boards.empty())
        return;

    bool ret = true;
    for (auto &board: boards) {
        board->setEnableSerial(true, persist_);
        if (board->hasCapability(TY_BOARD_CAPABILITY_SERIAL) && !board->serialOpen())
            ret = false;
    }
    notifyFinished(ret);
}

void ClientHandler::detach(const QStringList &)
{
    auto boards = selectedBoards();
    if (boards.empty())
        return;

    for (auto &board: boards)
        board->setEnableSerial(false, persist_);
    notifyFinished(true);
}

/* This function is static because it can be called after the client is gone (and the
   handler destroyed), such as if the user does not wait for the board selection dialog.
   This means we cannot use notify*() methods in there, hence the use of pseudo-tasks
   such as FailedTask and so on. */
vector<TaskInterface> ClientHandler::makeUploadTasks(const vector<shared_ptr<Board>> &boards,
                                                     const QStringList &filenames)
{
    vector<TaskInterface> tasks;

    if (filenames.isEmpty()) {
        unsigned int fws_count = 0;
        for (auto &board: boards) {
            if (!board->firmware().isEmpty()) {
                fws_count++;

                auto fw = Firmware::load(board->firmware());
                if (!fw) {
                    tasks.push_back(make_task<FailedTask>(ty_error_last_message()));
                    continue;
                }

                tasks.push_back(board->upload({fw}));
            }
        }
        if (!fws_count) {
            QString msg;
            if (boards.size() == 1) {
                msg = tr("Board '%1' is not associated to a firmware").arg(boards[0]->tag());
            } else {
                msg = tr("No board has an associated firmware");
            }
            tasks.push_back(make_task<FailedTask>(msg));
        }
    } else {
        vector<shared_ptr<Firmware>> fws;
        fws.reserve(filenames.count());
        for (auto &filename: filenames) {
            auto fw = Firmware::load(filename);
            if (!fw) {
                tasks.push_back(make_task<FailedTask>(ty_error_last_message()));
                continue;
            }
            fws.push_back(fw);
        }

        if (!fws.empty()) {
            for (auto &board: boards)
                tasks.push_back(board->upload(fws));
        }
    }

    return tasks;
}

vector<shared_ptr<Board>> ClientHandler::selectedBoards()
{
    auto monitor = tyCommander->monitor();

    if (!monitor->boardCount()) {
        notifyLog(TY_LOG_ERROR, tr("No board available"));
        notifyFinished(false);
        return {};
    }

    vector<shared_ptr<Board>> boards;
    if (filters_.isEmpty()) {
        boards = monitor->boards();
    } else {
        auto filters = multi_ ? filters_ : QStringList{filters_.last()};
        boards = monitor->find([&](Board &board) {
            for (auto &filter: filters) {
                if (board.matchesTag(filter))
                    return true;
            }
            return false;
        });

        if (boards.empty()) {
            if (filters_.count() == 1) {
                notifyLog(TY_LOG_ERROR, tr("Cannot find any board matching '%1'").arg(filters_[0]));
            } else {
                notifyLog(TY_LOG_ERROR, tr("Cannot find any matching board"));
            }
            notifyFinished(false);
            return {};
        }
    }

    if (!multi_)
        boards.resize(1);
    return boards;
}

void ClientHandler::notifyLog(ty_log_level level, const QString &msg)
{
    QString ctx;
    if (auto watcher = qobject_cast<TaskWatcher *>(sender()))
        ctx = watcher->task().name();
    peer_->send({"log", ctx, QString::number(level), msg});
}

void ClientHandler::notifyStarted()
{
    peer_->send("start");
}

void ClientHandler::notifyFinished(bool success)
{
    finished_tasks_++;
    if (!success)
        error_count_++;

    if (finished_tasks_ >= tasks_.size())
        peer_->send({"exit", error_count_ ? "1" : "0"});
}

void ClientHandler::notifyProgress(const QString &action, unsigned int value, unsigned int max)
{
    if (tasks_.size() > 1) {
        if (!value)
            notifyLog(TY_LOG_INFO, QString("%1...").arg(action));
    } else {
        QString ctx;
        if (auto watcher = qobject_cast<TaskWatcher *>(sender()))
            ctx = watcher->task().name();
        peer_->send({"progress", ctx, action, QString::number(value), QString::number(max)});
    }
}

void ClientHandler::addTask(TaskInterface task)
{
    tasks_.push_back(task);

    auto watcher = new TaskWatcher(this);
    connect(watcher, &TaskWatcher::log, this, &ClientHandler::notifyLog);
    connect(watcher, &TaskWatcher::started, this, &ClientHandler::notifyStarted);
    connect(watcher, &TaskWatcher::finished, this, &ClientHandler::notifyFinished);
    connect(watcher, &TaskWatcher::progress, this, &ClientHandler::notifyProgress);
    watcher->setTask(&task);
}

void ClientHandler::executeTasks()
{
    if (tasks_.empty()) {
        notifyFinished(true);
        return;
    }

    for (auto &task: tasks_)
        task.start();
}
