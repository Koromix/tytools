/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_THREAD_H
#define TY_THREAD_H

#include "common.h"
#ifndef _WIN32
    #include <pthread.h>
#endif

TY_C_BEGIN

#ifdef _WIN32
typedef unsigned long ty_thread_id; // DWORD
#else
typedef pthread_t ty_thread_id;
#endif

typedef struct ty_thread {
    ty_thread_id thread_id;
#ifdef _WIN32
    void *h; // HANDLE
#else
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

int ty_thread_create(ty_thread *thread, ty_thread_func *f, void *udata);
int ty_thread_join(ty_thread *thread);
void ty_thread_detach(ty_thread *thread);

ty_thread_id ty_thread_get_self_id(void);

int ty_mutex_init(ty_mutex *mutex);
void ty_mutex_release(ty_mutex *mutex);

void ty_mutex_lock(ty_mutex *mutex);
void ty_mutex_unlock(ty_mutex *mutex);

int ty_cond_init(ty_cond *cond);
void ty_cond_release(ty_cond *cond);

void ty_cond_signal(ty_cond *cond);
void ty_cond_broadcast(ty_cond *cond);

bool ty_cond_wait(ty_cond *cond, ty_mutex *mutex, int timeout);

TY_C_END

#endif
