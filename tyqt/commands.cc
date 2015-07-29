/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QFutureWatcher>
#include <QThreadPool>

#include "commands.hh"
#include "selector_dialog.hh"
#include "tyqt.hh"

using namespace std;

class BoardSelectorCommand {
    QFutureInterface<QString> intf_;

    function<QFuture<QString>(Board &)> f_;
    QFutureWatcher<QString> watcher_;

    shared_ptr<Board> board_;

public:
    BoardSelectorCommand(function<QFuture<QString>(Board &)> f);

    QFuture<QString> start();
};

BoardSelectorCommand::BoardSelectorCommand(function<QFuture<QString>(Board &)> f)
    : f_(f)
{
}

QFuture<QString> BoardSelectorCommand::start()
{
    intf_.reportStarted();

    QObject::connect(&watcher_, &QFutureWatcher<QString>::resultReadyAt, [=](int index) {
        assert(!index);
        intf_.reportResult(watcher_.result());
    });
    QObject::connect(&watcher_, &QFutureWatcher<QString>::finished, [=]() {
        intf_.reportFinished();
    });
    QObject::connect(&watcher_, &QFutureWatcher<QString>::progressRangeChanged, [=](int min, int max) {
        intf_.setProgressRange(min, max);
    });
    QObject::connect(&watcher_, &QFutureWatcher<QString>::progressValueChanged, [=](int value) {
        intf_.setProgressValue(value);
    });

    auto dialog = tyQt->openSelector();
    if (!dialog) {
        intf_.reportFinished();
        return intf_.future();
    }
    QObject::connect(dialog, &SelectorDialog::boardSelected, [=](Board *board) {
        if (!board) {
            intf_.reportFinished();
            return;
        }

        board_ = board->getSharedPtr();
        watcher_.setFuture(f_(*board_));
    });
    dialog->show();

    return intf_.future();
}

static QFuture<QString> immediate_future(const QString &s = QString())
{
    QFutureInterface<QString> intf;
    intf.reportStarted();
    intf.reportResult(s);
    intf.reportFinished();

    return intf.future();
}

QFuture<QString> Commands::execute(const QString &cmd, const QStringList &parameters)
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

    return immediate_future(QString("Unknown command '%1'").arg(cmd));
}

QFuture<QString> Commands::openMainWindow()
{
    tyQt->openMainWindow();
    return immediate_future();
}

QFuture<QString> Commands::activateMainWindow()
{
    tyQt->activateMainWindow();
    return immediate_future();
}

QFuture<QString> Commands::upload(const QString &tag, const QString &firmware)
{
    auto manager = tyQt->manager();

    if (!manager->boardCount())
        return immediate_future(TyQt::tr("No board available"));

    shared_ptr<Board> board;
    if (!tag.isEmpty()) {
        board = manager->find([=](Board &board) { return board.matchesTag(tag); });
    } else {
        if (manager->boardCount() == 1) {
            board = manager->board(0);
        } else {
            board = manager->find([=](Board &board) { return board.property("firmware") == firmware; });
            if (!board) {
                return (new BoardSelectorCommand([=](Board &board) {
                    return upload(board, firmware);
                }))->start();
            }
        }
    }
    if (!board)
        return immediate_future(TyQt::tr("Cannot find board '%1'").arg(tag));

    return upload(*board, firmware);
}

QFuture<QString> Commands::upload(Board &board, const QString &firmware)
{
    if (!firmware.isEmpty())
        board.setProperty("firmware", firmware);

    return board.upload(board.property("firmware").toString(), board.property("resetAfter").toBool());
}
