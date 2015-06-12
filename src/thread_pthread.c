/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <time.h>
#include "ty/system.h"
#include "ty/thread.h"

int ty_mutex_init(ty_mutex *mutex, ty_mutex_type type)
{
    pthread_mutexattr_t attr;
    int ptype, r;

    mutex->init = false;

    switch (type) {
    case TY_MUTEX_FAST:
        ptype = PTHREAD_MUTEX_NORMAL;
        break;
    case TY_MUTEX_RECURSIVE:
        ptype = PTHREAD_MUTEX_RECURSIVE;
        break;

    default:
        assert(false);
    }

    r = pthread_mutexattr_init(&attr);
    assert(!r);
    r = pthread_mutexattr_settype(&attr, ptype);
    assert(!r);

    r = pthread_mutex_init(&mutex->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (r)
        return ty_error(TY_ERROR_SYSTEM, "pthread_mutex_init() failed: %s", strerror(r));
    mutex->init = true;

    return 0;
}

void ty_mutex_release(ty_mutex *mutex)
{
    if (!mutex->init)
        return;

    pthread_mutex_destroy(&mutex->mutex);
    mutex->init = false;
}

void ty_mutex_lock(ty_mutex *mutex)
{
    pthread_mutex_lock(&mutex->mutex);
}

void ty_mutex_unlock(ty_mutex *mutex)
{
    pthread_mutex_unlock(&mutex->mutex);
}

int ty_cond_init(ty_cond *cond)
{
#ifndef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP
    pthread_condattr_t attr;
#endif
    int r;

    cond->init = false;

#ifdef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP
    r = pthread_cond_init(&cond->cond, NULL);
#else
    r = pthread_condattr_init(&attr);
    assert(!r);
    r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    assert(!r);

    r = pthread_cond_init(&cond->cond, &attr);
    pthread_condattr_destroy(&attr);
#endif
    if (r)
        return ty_error(TY_ERROR_SYSTEM, "pthread_cond_init() failed: %s", strerror(r));
    cond->init = true;

    return 0;
}

void ty_cond_release(ty_cond *cond)
{
    if (!cond->init)
        return;

    pthread_cond_destroy(&cond->cond);
    cond->init = false;
}

void ty_cond_signal(ty_cond *cond)
{
    pthread_cond_signal(&cond->cond);
}

void ty_cond_broadcast(ty_cond *cond)
{
    pthread_cond_broadcast(&cond->cond);
}

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout)
{
    int r;

    if (timeout >= 0) {
#ifdef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP
        struct timespec ts;

        ts.tv_sec = (time_t)(timeout / 1000);
        ts.tv_nsec = (long)(timeout % 1000 * 1000000);

        r = pthread_cond_timedwait_relative_np(&cond->cond, &mutex->mutex, &ts);
#else
        struct timespec ts;
        uint64_t end;

        end = ty_millis() + (uint64_t)timeout;
        ts.tv_sec = (time_t)(end / 1000);
        ts.tv_nsec = (long)(end % 1000 * 1000000);

        r = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
#endif
    } else {
        r = pthread_cond_wait(&cond->cond, &mutex->mutex);
    }
    assert(!r || r == ETIMEDOUT);

    return !r;
}
