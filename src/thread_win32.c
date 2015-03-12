/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "thread.h"

int ty_mutex_init(ty_mutex *mutex, ty_mutex_type type)
{
    TY_UNUSED(type);

    InitializeCriticalSection(&mutex->mutex);
    mutex->init = true;

    return 0;
}

void ty_mutex_release(ty_mutex *mutex)
{
    if (!mutex->init)
        return;

    DeleteCriticalSection(&mutex->mutex);
    mutex->init = false;
}

void ty_mutex_lock(ty_mutex *mutex)
{
    EnterCriticalSection(&mutex->mutex);
}

void ty_mutex_unlock(ty_mutex *mutex)
{
    LeaveCriticalSection(&mutex->mutex);
}

int ty_cond_init(ty_cond *cond)
{
    InitializeConditionVariable(cond);

    return 0;
}

void ty_cond_release(ty_cond *cond)
{
    TY_UNUSED(cond);
    // No need for a DeleteConditionVariable() apparently
}

void ty_cond_signal(ty_cond *cond)
{
    WakeConditionVariable(cond);
}

void ty_cond_broadcast(ty_cond *cond)
{
    WakeAllConditionVariable(cond);
}

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout)
{
    return SleepConditionVariableCS(cond, &mutex->mutex, timeout >= 0 ? (DWORD)timeout : INFINITE);
}
