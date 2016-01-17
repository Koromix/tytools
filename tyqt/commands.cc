/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QDir>
#include <QFileInfo>

#include "board.hh"
#include "commands.hh"
#include "firmware.hh"
#include "selector_dialog.hh"
#include "task.hh"
#include "tyqt.hh"

using namespace std;

class BoardSelectorTask : public Task, private TaskListener {
    QString title_;
    function<TaskInterface(Board &)> f_;

public:
    BoardSelectorTask(const QString &title, function<TaskInterface(Board &)> f)
        : title_(title), f_(f) {}

    bool start() override;

private:
    void notifyLog(ty_log_level level, const QString &msg) override;
    void notifyFinished(bool success, shared_ptr<void> result) override;
    void notifyProgress(const QString &action, unsigned int value, unsigned int max) override;
};

bool BoardSelectorTask::start()
{
    reportLog(TY_LOG_INFO, "Waiting for user selection");
    reportStarted();

    auto dialog = tyQt->openSelector();
    if (!dialog) {
        reportFinished(false, nullptr);
        return true;
    }

    auto ptr = shared_from_this();
    QObject::connect(dialog, &SelectorDialog::boardSelected, [this, ptr](Board *board) {
        if (!board) {
            reportLog(TY_LOG_INFO, QString("%1 was canceled").arg(title_));
            reportFinished(false, nullptr);
            return;
        }

        auto task = f_(*board);
        setTask(&task);
        task.start();
    });
    dialog->show();

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

void BoardSelectorTask::notifyProgress(const QString &action, unsigned int value, unsigned int max)
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
    } else if (cmd == "activate") {
        return activateMainWindow();
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
        tyQt->openMainWindow();
        return true;
    });
}

TaskInterface Commands::activateMainWindow()
{
    return make_task<ImmediateTask>([]() {
        tyQt->activateMainWindow();
        return true;
    });
}

TaskInterface Commands::reset(const QString &tag)
{
    auto manager = tyQt->manager();

    if (!manager->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = manager->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else {
        board = manager->board(0);
    }

    return board->reset();
}

TaskInterface Commands::reboot(const QString &tag)
{
    auto manager = tyQt->manager();

    if (!manager->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = manager->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else {
        board = manager->board(0);
    }

    return board->reboot();
}

TaskInterface Commands::upload(const QString &tag, const QStringList &filenames)
{
    auto manager = tyQt->manager();

    if (!manager->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = manager->find([&](Board &board) { return board.matchesTag(tag); });
        if (!board)
            return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));
    } else if (manager->boardCount() == 1) {
        board = manager->board(0);
    } else {
        if (filenames.count() == 1) {
            board = manager->find([&](Board &board) {
                return ty_compare_paths(board.firmware().toLocal8Bit().constData(),
                                        filenames[0].toLocal8Bit().constData());
            });
        }

        if (!board) {
            return make_task<BoardSelectorTask>("Upload", [=](Board &board) {
                return upload(board, filenames);
            });
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
