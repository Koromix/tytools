/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDir>
#include <QFileInfo>

#include "tyqt/board.hpp"
#include "commands.hpp"
#include "tyqt/firmware.hpp"
#include "main_window.hpp"
#include "selector_dialog.hpp"
#include "tyqt/task.hpp"
#include "tyqt.hpp"

using namespace std;

class BoardSelectorTask : public Task, private TaskListener {
    QString action_;
    QString desc_;

    function<TaskInterface(Board &)> f_;

public:
    BoardSelectorTask(const QString &action = QString(),
                      function<TaskInterface(Board &)> f = function<TaskInterface(Board &)>())
        : action_(action), f_(f) {}
    BoardSelectorTask(function<TaskInterface(Board &)> f)
        : f_(f) {}

    void setAction(const QString &action) { action_ = action; }
    QString action() const { return action_; }

    void setDescription(const QString &desc) { desc_ = desc; }
    QString description() const { return desc_; }

    void setFunction(function<TaskInterface(Board &)> f) { f_ = f; }

    bool start() override;

private:
    void notifyLog(ty_log_level level, const QString &msg) override;
    void notifyFinished(bool success, shared_ptr<void> result) override;
    void notifyProgress(const QString &action, uint64_t value, uint64_t max) override;
};

bool BoardSelectorTask::start()
{
    reportLog(TY_LOG_INFO, "Waiting for user selection");
    reportStarted();

    auto dialog = new SelectorDialog();
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setAction(action_);
    dialog->setDescription(desc_);

    auto ptr = shared_from_this();
    QObject::connect(dialog, &SelectorDialog::boardSelected, [this, ptr](Board *board) {
        if (!board) {
            reportLog(TY_LOG_INFO, QString("%1 was canceled").arg(action_));
            reportFinished(false, nullptr);
            return;
        }

        auto task = f_(*board);
        setTask(&task);
        task.start();
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    return true;
}

void BoardSelectorTask::notifyLog(ty_log_level level, const QString &msg)
{
    reportLog(level, msg);
}

void BoardSelectorTask::notifyFinished(bool success, shared_ptr<void> result)
{
    reportFinished(success, result);
}

void BoardSelectorTask::notifyProgress(const QString &action, uint64_t value, uint64_t max)
{
    reportProgress(action, value, max);
}

TaskInterface Commands::execute(const QString &cmd, const QStringList &parameters)
{
    if (parameters.count() < 2)
        return make_task<FailedTask>(TyQt::tr("Command '%1' needs more parameters").arg(cmd));

    auto arguments = parameters;
    auto working_dir = arguments.takeFirst();
    auto tag = arguments.takeFirst();

    if (cmd == "open") {
        return openMainWindow();
    } else if (cmd == "reset") {
        return reset(tag);
    } else if (cmd == "reboot") {
        return reboot(tag);
    } else if (cmd == "upload") {
        for (auto &filename: arguments)
           filename = QFileInfo(working_dir, filename).filePath();

        return upload(tag, arguments);
    }

    return make_task<FailedTask>(TyQt::tr("Unknown command '%1'").arg(cmd));
}

TaskInterface Commands::openMainWindow()
{
    return make_task<ImmediateTask>([]() {
        auto win = new MainWindow();
        win->setAttribute(Qt::WA_DeleteOnClose, true);
        win->show();

        return true;
    });
}

TaskInterface Commands::reset(const QString &tag)
{
    auto monitor = tyQt->monitor();

    if (!monitor->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = monitor->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else {
        board = monitor->board(0);
    }

    return board->reset();
}

TaskInterface Commands::reboot(const QString &tag)
{
    auto monitor = tyQt->monitor();

    if (!monitor->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = monitor->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else {
        board = monitor->board(0);
    }

    return board->reboot();
}

TaskInterface Commands::upload(const QString &tag, const QStringList &filenames)
{
    auto monitor = tyQt->monitor();

    if (!monitor->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = monitor->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else if (monitor->boardCount() == 1) {
        board = monitor->board(0);
    } else {
        if (filenames.count() == 1) {
            board = monitor->find([&](Board &board) {
                return ty_compare_paths(board.firmware().toLocal8Bit().constData(),
                                        filenames[0].toLocal8Bit().constData());
            });
        }

        if (!board) {
            auto task = make_shared<BoardSelectorTask>([=](Board &board) {
                return upload(board, filenames);
            });
            if (filenames.count() == 1) {
                task->setAction(TyQt::tr("Upload '%1'").arg(QFileInfo(filenames[0]).fileName()));
                task->setDescription(TyQt::tr("Upload '%1' to:").arg(QFileInfo(filenames[0]).fileName()));
            } else {
                task->setAction(TyQt::tr("Upload firmwares"));
                task->setDescription(TyQt::tr("Upload firmwares to:"));
            }

            return TaskInterface(task);
        }
    }

    return upload(*board, filenames);
}

TaskInterface Commands::upload(Board &board, const QStringList &filenames)
{
    vector<shared_ptr<Firmware>> fws;

    if (!filenames.isEmpty()) {
        fws.reserve(filenames.count());
        for (auto filename: filenames) {
            auto fw = Firmware::load(filename);
            if (fw)
                fws.push_back(fw);
        }
    } else if (!board.firmware().isEmpty()) {
        auto fw = Firmware::load(board.firmware());
        if (fw)
            fws.push_back(fw);
    } else {
        return make_task<FailedTask>(TyQt::tr("No firmware to upload to '%1'").arg(board.tag()));
    }
    // FIXME: forward all error messages
    if (fws.empty())
        return make_task<FailedTask>(ty_error_last_message());

    return board.upload(fws);
}
