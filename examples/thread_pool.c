/*
 * thread_pool.c — submit parallel tasks and wait for completion
 *
 * Build: cd .. && make && cd examples && make thread_pool && ./thread_pool
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "clib.h"

typedef struct {
    int id;
    int delay_ms;
} task_arg_t;

static void worker(void *arg)
{
    task_arg_t *t = (task_arg_t *)arg;
    printf("  task %d: start (delay %d ms)\n", t->id, t->delay_ms);
    usleep(t->delay_ms * 1000);
    printf("  task %d: done\n", t->id);
}

int main(void)
{
    ml_pool_t         pool;
    ml_thread_pool_t *tp;
    task_arg_t        args[6];

    ml_pool_create(&pool, 0);

    /* Create a thread pool: 4 workers, max 32 queued tasks */
    tp = ml_thread_pool_create(&pool, 4, 32);
    if (!tp) {
        fprintf(stderr, "thread pool create failed\n");
        return 1;
    }
    printf("[1] thread pool created: 4 workers, max 32 queued\n");

    /* Submit 6 tasks with varying delays */
    for (int i = 0; i < 6; i++) {
        args[i].id = i;
        args[i].delay_ms = (6 - i) * 50; /* later tasks finish faster */
        if (ml_thread_pool_submit(tp, worker, &args[i]) != 0) {
            fprintf(stderr, "submit failed for task %d\n", i);
        }
    }
    printf("[2] 6 tasks submitted\n");

    /* Wait for all tasks to complete */
    ml_thread_pool_wait(tp);
    printf("[3] all tasks completed\n");

    ml_thread_pool_destroy(tp);
    ml_pool_destroy(&pool);
    printf("[4] thread pool destroyed\n");

    return 0;
}
