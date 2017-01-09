/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_THREAD_H
#define TY_THREAD_H

#include "common.h"
#ifndef _WIN32
    #include <pthread.h>
#endif

TY_C_BEGIN

typedef struct ty_thread {
#ifdef _WIN32
    void *h; // HANDLE
#else
    pthread_t thread;
    bool init;
#endif
} ty_thread;

/* Define correctly-sized dummy types to get CRITICAL_SECTION and CONDITION_VARIABLE
   without including windows.h. */
#if defined(_MSC_VER) && defined(_WIN64)
typedef __declspec(align(8)) struct { char dummy[40]; } TY_WIN32_CRITICAL_SECTION;
typedef __declspec(align(8)) struct { char dummy[8]; } TY_WIN32_CONDITION_VARIABLE;
#elif defined(_MSC_VER)
typedef __declspec(align(4)) struct { char dummy[24]; } TY_WIN32_CRITICAL_SECTION;
typedef __declspec(align(4)) struct { char dummy[4]; } TY_WIN32_CONDITION_VARIABLE;
#elif defined(_WIN64)
typedef struct { char dummy[40]; } __attribute__((__aligned__(8))) TY_WIN32_CRITICAL_SECTION;
typedef struct { char dummy[8]; } __attribute__((__aligned__(8))) TY_WIN32_CONDITION_VARIABLE;
#elif defined(_WIN32)
typedef struct { char dummy[24]; } __attribute__((__aligned__(4))) TY_WIN32_CRITICAL_SECTION;
typedef struct { char dummy[4]; } __attribute__((__aligned__(4))) TY_WIN32_CONDITION_VARIABLE;
#endif

typedef struct ty_mutex {
#ifdef _WIN32
    TY_WIN32_CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    bool init;
} ty_mutex;

typedef struct ty_cond {
#ifdef _WIN32
    union {
        TY_WIN32_CONDITION_VARIABLE cv;
        struct {
            void *ev; // HANDLE

            TY_WIN32_CRITICAL_SECTION mutex;
            unsigned int waiting;
            unsigned int wakeup;
        } xp;
    };
#else
    pthread_cond_t cond;
#endif
    bool init;
} ty_cond;

typedef int ty_thread_func(void *udata);

TY_PUBLIC int ty_thread_create(ty_thread *thread, ty_thread_func *f, void *udata);
TY_PUBLIC int ty_thread_join(ty_thread *thread);
TY_PUBLIC void ty_thread_detach(ty_thread *thread);

TY_PUBLIC int ty_mutex_init(ty_mutex *mutex);
TY_PUBLIC void ty_mutex_release(ty_mutex *mutex);

TY_PUBLIC void ty_mutex_lock(ty_mutex *mutex);
TY_PUBLIC void ty_mutex_unlock(ty_mutex *mutex);

TY_PUBLIC int ty_cond_init(ty_cond *cond);
TY_PUBLIC void ty_cond_release(ty_cond *cond);

TY_PUBLIC void ty_cond_signal(ty_cond *cond);
TY_PUBLIC void ty_cond_broadcast(ty_cond *cond);

TY_PUBLIC bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout);

TY_C_END

#endif
