/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "commands.hh"
#include "selector_dialog.hh"
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
    if (cmd == "open") {
        return openMainWindow();
    } else if (cmd == "activate") {
        return activateMainWindow();
    } else if (cmd == "upload") {
        auto tag = parameters.value(0, QString());
        auto firmware = parameters.value(1, QString());

        return upload(tag, firmware);
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

TaskInterface Commands::upload(const QString &tag, const QString &firmware)
{
    auto manager = tyQt->manager();

    if (!manager->boardCount())
        return make_task<FailedTask>(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = manager->find([=](Board &board) { return board.matchesTag(tag); });
    } else {
        if (manager->boardCount() == 1) {
            board = manager->board(0);
        } else {
            board = manager->find([=](Board &board) { return board.firmware() == firmware; });
            if (!board) {
                return make_task<BoardSelectorTask>("Upload", [=](Board &board) {
                    return upload(board, firmware);
                });
            }
        }
    }
    if (!board)
        return make_task<FailedTask>(TyQt::tr("Cannot find board '%1'").arg(tag));

    return upload(*board, firmware);
}

TaskInterface Commands::upload(Board &board, const QString &firmware)
{
    QString firmware2 = firmware;
    if (firmware2.isEmpty()) {
        if (board.firmware().isEmpty())
            return make_task<FailedTask>(TyQt::tr("No firmware to upload"));

        firmware2 = board.firmware();
    }

    return board.upload(firmware2, board.property("resetAfter").toBool());
}
