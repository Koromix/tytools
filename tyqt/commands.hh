/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
