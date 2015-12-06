/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "ty/common.h"
#include "compat.h"
#include <time.h>
#include "ty/system.h"
#include "ty/thread.h"

static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_cond = PTHREAD_COND_INITIALIZER;

TY_EXIT()
{
    pthread_cond_destroy(&thread_cond);
    pthread_mutex_destroy(&thread_mutex);
}

struct thread_context {
    ty_thread *thread;

    ty_thread_func *f;
    void *udata;
};

static void *thread_proc(void *udata)
{
    struct thread_context ctx = *(struct thread_context *)udata;

    pthread_mutex_lock(&thread_mutex);
    ctx.thread->init = true;
    pthread_cond_broadcast(&thread_cond);
    pthread_mutex_unlock(&thread_mutex);

    return (void *)(intptr_t)(*ctx.f)(ctx.udata);
}

int ty_thread_create(ty_thread *thread, ty_thread_func *f, void *udata)
{
    struct thread_context ctx;
    int r;

    ctx.thread = thread;
    ctx.f = f;
    ctx.udata = udata;

    thread->init = false;
    r = pthread_create(&thread->thread, NULL, thread_proc, &ctx);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "pthread_create() failed: %s", strerror(r));

    pthread_mutex_lock(&thread_mutex);
    while (!thread->init)
        pthread_cond_wait(&thread_cond, &thread_mutex);
    pthread_mutex_unlock(&thread_mutex);

    return 0;
}

int ty_thread_join(ty_thread *thread)
{
    assert(thread->init);

    void *retval;
    int r;

    r = pthread_join(thread->thread, &retval);
    assert(!r);

    thread->init = false;

    return (int)(intptr_t)retval;
}

void ty_thread_detach(ty_thread *thread)
{
    if (!thread->init)
        return;

    pthread_detach(thread->thread);
    thread->init = false;
}

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
