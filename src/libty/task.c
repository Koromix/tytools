/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "../libhs/array.h"
#include "system.h"
#include "task.h"

struct ty_pool {
    int unused_timeout;
    unsigned int max_threads;

    ty_mutex mutex;

    _HS_ARRAY(ty_thread) worker_threads;
    size_t busy_workers;

    _HS_ARRAY(ty_task *) pending_tasks;
    ty_cond pending_cond;

    bool init;
};

static ty_pool *default_pool;
static TY_THREAD_LOCAL ty_task *current_task;

int ty_pool_new(ty_pool **rpool)
{
    assert(rpool);

    ty_pool *pool;
    int r;

    pool = calloc(1, sizeof(*pool));
    if (!pool) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    pool->max_threads = 16;
    pool->unused_timeout = 10000;

    r = ty_mutex_init(&pool->mutex);
    if (r < 0)
        goto error;
    r = ty_cond_init(&pool->pending_cond);
    if (r < 0)
        goto error;

    pool->init = true;

    *rpool = pool;
    return 0;

error:
    ty_pool_free(pool);
    return r;
}

void ty_pool_free(ty_pool *pool)
{
    if (pool) {
        if (pool->init) {
            ty_mutex_lock(&pool->mutex);

            for (size_t i = 0; i < pool->pending_tasks.count; i++) {
                ty_task *task = pool->pending_tasks.values[i];
                ty_task_unref(task);
            }
            _hs_array_release(&pool->pending_tasks);
            pool->max_threads = 0;
            ty_cond_broadcast(&pool->pending_cond);

            /* This is a signal for worker threads to stop detaching themselves and
               freeing their own structures, because we need to join with them. */
            pool->init = false;

            ty_mutex_unlock(&pool->mutex);

            for (size_t i = 0; i < pool->worker_threads.count; i++) {
                ty_thread *thread = &pool->worker_threads.values[i];
                ty_thread_join(thread);
            }
            _hs_array_release(&pool->worker_threads);
        }

        ty_cond_release(&pool->pending_cond);
        ty_mutex_release(&pool->mutex);
    }

    free(pool);
}

static int start_worker_thread(ty_pool *pool);
int ty_pool_set_max_threads(ty_pool *pool, unsigned int max)
{
    assert(pool);

    int r;

    ty_mutex_lock(&pool->mutex);

    if (max > pool->max_threads) {
        size_t need_threads = pool->pending_tasks.count;
        if (need_threads > (size_t)pool->max_threads - pool->worker_threads.count)
            need_threads = (size_t)pool->max_threads - pool->worker_threads.count;
        for (size_t i = 0; i < need_threads; i++) {
            r = start_worker_thread(pool);
            if (r < 0) {
                if (pool->worker_threads.count)
                    r = 0;
                goto cleanup;
            }
        }
    } else {
        ty_cond_broadcast(&pool->pending_cond);
    }
    pool->max_threads = max;

    r = 0;
cleanup:
    ty_mutex_unlock(&pool->mutex);
    return r;
}

unsigned int ty_pool_get_max_threads(ty_pool *pool)
{
    assert(pool);
    return pool->max_threads;
}

void ty_pool_set_idle_timeout(ty_pool *pool, int timeout)
{
    assert(pool);

    ty_mutex_lock(&pool->mutex);
    pool->unused_timeout = timeout;
    ty_cond_broadcast(&pool->pending_cond);
    ty_mutex_unlock(&pool->mutex);
}

int ty_pool_get_idle_timeout(ty_pool *pool)
{
    assert(pool);
    return pool->unused_timeout;
}

static void cleanup_default_pool(void)
{
    ty_pool_free(default_pool);
}

int ty_pool_get_default(ty_pool **rpool)
{
    assert(rpool);

    if (!default_pool) {
        int r = ty_pool_new(&default_pool);
        if (r < 0)
            return r;

        atexit(cleanup_default_pool);
    }

    *rpool = default_pool;
    return 0;
}

int ty_task_new(const char *name, int (*run)(ty_task *task), ty_task **rtask)
{
    assert(name);
    assert(run);
    assert(rtask);

    ty_task * task;
    int r;

    task = calloc(1, sizeof(*task));
    if (!task) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }
    task->refcount = 1;

    task->task_run = run;
    task->name = strdup(name);
    if (!task->name) {
        r = ty_error(TY_ERROR_MEMORY, NULL);
        goto error;
    }

    r = ty_mutex_init(&task->mutex);
    if (r < 0)
        goto error;
    r = ty_cond_init(&task->cond);
    if (r < 0)
        goto error;

    *rtask = task;
    return 0;

error:
    ty_task_unref(task);
    return r;
}

ty_task *ty_task_ref(ty_task *task)
{
    assert(task);

    _ty_refcount_increase(&task->refcount);
    return task;
}

void ty_task_unref(ty_task *task)
{
    if (task) {
        if (_ty_refcount_decrease(&task->refcount))
            return;

        if (task->result_cleanup)
            (*task->result_cleanup)(task->result);

        if (task->user_cleanup)
            (*task->user_cleanup)(task->user_cleanup_udata);
        if (task->task_finalize)
            (*task->task_finalize)(task);

        free(task->name);
        ty_cond_release(&task->cond);
        ty_mutex_release(&task->mutex);
    }

    free(task);
}

static void change_task_status(ty_task *task, ty_task_status status)
{
    ty_message_data msg = {0};

    task->status = status;

    ty_mutex_lock(&task->mutex);
    ty_cond_broadcast(&task->cond);
    ty_mutex_unlock(&task->mutex);

    msg.task = task;
    msg.type = TY_MESSAGE_STATUS;
    msg.u.task.status = status;

    ty_message(&msg);
}

static void run_task(ty_task *task)
{
    assert(task->status <= TY_TASK_STATUS_PENDING);

    ty_task *previous_task;

    previous_task = current_task;
    current_task = task;

    change_task_status(task, TY_TASK_STATUS_RUNNING);
    task->ret = (*task->task_run)(task);
    if (task->task_finalize) {
        (*task->task_finalize)(task);
        task->task_finalize = NULL;
    }
    change_task_status(task, TY_TASK_STATUS_FINISHED);

    current_task = previous_task;
}

static int worker_thread_main(void *udata)
{
    ty_pool *pool = udata;

    while (true) {
        uint64_t start;
        bool run;
        ty_task *task;

        ty_mutex_lock(&pool->mutex);
        pool->busy_workers--;

        run = true;
        start = ty_millis();
        while (true) {
            if (pool->worker_threads.count > pool->max_threads)
                goto timeout;
            if (pool->pending_tasks.count) {
                task = pool->pending_tasks.values[0];
                _hs_array_remove(&pool->pending_tasks, 0, 1);
                break;
            }
            if (!run)
                goto timeout;

            run = ty_cond_wait(&pool->pending_cond, &pool->mutex,
                               ty_adjust_timeout(pool->unused_timeout, start));
        }

        pool->busy_workers++;
        ty_mutex_unlock(&pool->mutex);

        run_task(task);
        ty_task_unref(task);
    }

timeout:
    if (pool->init) {
        for (size_t i = 0; i < pool->worker_threads.count; i++) {
            ty_thread *thread = &pool->worker_threads.values[i];
            if (thread->thread_id == ty_thread_get_self_id()) {
                ty_thread_detach(thread);
                pool->worker_threads.values[i] =
                    pool->worker_threads.values[pool->worker_threads.count - 1];
                _hs_array_pop(&pool->worker_threads, 1);
                break;
            }
        }
    }
    ty_mutex_unlock(&pool->mutex);

    return 0;
}

// Call with pool->mutex locked
static int start_worker_thread(ty_pool *pool)
{
    ty_thread *thread;
    int r;

    // Can't handle failure after ty_thread_create() so grow the array first
    r = _hs_array_grow(&pool->worker_threads, 1);
    if (r < 0)
        return ty_libhs_translate_error(r);
    thread = &pool->worker_threads.values[pool->worker_threads.count];

    r = ty_thread_create(thread, worker_thread_main, pool);
    if (r < 0)
        return r;

    pool->worker_threads.count++;
    pool->busy_workers++;

    return 0;
}

int ty_task_start(ty_task *task)
{
    assert(task);
    assert(task->status == TY_TASK_STATUS_READY);

    ty_pool *pool;
    int r;

    if (!task->pool) {
        r = ty_pool_get_default(&task->pool);
        if (r < 0)
            return r;
    }
    pool = task->pool;

    ty_mutex_lock(&pool->mutex);

    if (pool->busy_workers == pool->worker_threads.count &&
            pool->worker_threads.count < pool->max_threads) {
        r = start_worker_thread(pool);
        if (r < 0)
            goto cleanup;
    }

    r = _hs_array_push(&pool->pending_tasks, task);
    if (r < 0) {
        r = ty_libhs_translate_error(r);
        goto cleanup;
    }
    ty_task_ref(task);
    ty_cond_signal(&pool->pending_cond);

    change_task_status(task, TY_TASK_STATUS_PENDING);

    r = 0;
cleanup:
    ty_mutex_unlock(&pool->mutex);
    return r;
}

int ty_task_wait(ty_task *task, ty_task_status status, int timeout)
{
    assert(task);
    assert(status > TY_TASK_STATUS_READY);

    uint64_t start;
    int r;

    /* If the caller wants to wait until the task has finished without timing out, try
       to execute the task in this thread if it's not running already. */
    if (status == TY_TASK_STATUS_FINISHED && timeout < 0) {
        if (task->status == TY_TASK_STATUS_PENDING) {
            ty_pool *pool = task->pool;

            ty_mutex_lock(&pool->mutex);
            if (task->status == TY_TASK_STATUS_PENDING) {
                for (size_t i = 0; i < pool->pending_tasks.count; i++) {
                    if (pool->pending_tasks.values[i] == task) {
                        _hs_array_remove(&pool->pending_tasks, i, 1);
                        break;
                    }
                }
                ty_task_unref(task);

                task->status = TY_TASK_STATUS_READY;
            }
            ty_mutex_unlock(&pool->mutex);
        }

        if (task->status == TY_TASK_STATUS_READY) {
            run_task(task);
            return 1;
        }
    } else if (task->status == TY_TASK_STATUS_READY) {
        r = ty_task_start(task);
        if (r < 0)
            return r;
    }

    ty_mutex_lock(&task->mutex);
    start = ty_millis();
    while (task->status < status) {
        if (!ty_cond_wait(&task->cond, &task->mutex, ty_adjust_timeout(timeout, start)))
            break;
    }
    r = task->status >= status;
    ty_mutex_unlock(&task->mutex);

    return r;
}

int ty_task_join(ty_task *task)
{
    assert(task);

    int r = ty_task_wait(task, TY_TASK_STATUS_FINISHED, -1);
    if (r < 0)
        return r;

    return task->ret;
}

ty_task *ty_task_get_current(void)
{
    return current_task;
}
