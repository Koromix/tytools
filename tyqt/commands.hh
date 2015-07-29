/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef COMMANDS_HH
#define COMMANDS_HH

#include <QFuture>
#include <QStringList>

#include "board.hh"

namespace Commands {
    QFuture<QString> execute(const QString &cmd, const QStringList &parameters);

    QFuture<QString> openMainWindow();
    QFuture<QString> activateMainWindow();

    QFuture<QString> upload(const QString &tag, const QString &firmware);
    QFuture<QString> upload(Board &board, const QString &firmware);
}

#endif
