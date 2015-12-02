/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef COMMANDS_HH
#define COMMANDS_HH

#include <QStringList>

#include "board.hh"
#include "task.hh"

namespace Commands {
    TaskInterface execute(const QString &cmd, const QStringList &parameters);

    TaskInterface openMainWindow();
    TaskInterface activateMainWindow();

    TaskInterface upload(const QString &tag, const QString &firmware);
    TaskInterface upload(Board &board, const QString &firmware);

    TaskInterface uploadAll();
}

#endif
