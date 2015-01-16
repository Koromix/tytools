/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MAIN_H
#define MAIN_H

#include "ty.h"

TY_C_BEGIN

struct ty_board_manager;
struct ty_board;

void print_supported_models(void);

int get_manager(ty_board_manager **rmanager);
int get_board(ty_board **rboard);

TY_C_END

#endif
