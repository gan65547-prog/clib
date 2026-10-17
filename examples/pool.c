/*
 * pool.c — memory pool lifecycle: create, alloc, free, reuse, destroy
 *
 * Build: cd .. && make && cd examples && make pool && ./pool
 * Standalone: gcc -I../include -L.. -lclib pool.c -o pool && ./pool
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clib.h"

int main(void)
{
    ml_pool_t pool;

    /* Create a pool (0 = default page size, typically 4096) */
    if (ml_pool_create(&pool, 0) != 0) {
        fprintf(stderr, "pool create failed\n");
        return 1;
    }
    printf("[1] pool created\n");

    /* Allocate small blocks from the pool page */
    char *a = pool.f_alloc(&pool, 64);
    snprintf(a, 64, "hello from pool-allocated memory");
    printf("[2] allocated 64 bytes: '%s'\n", a);

    int *b = pool.f_alloc(&pool, 4 * sizeof(int));
    for (int i = 0; i < 4; i++) b[i] = i * 100;
    printf("[3] allocated int[4]: %d %d %d %d\n", b[0], b[1], b[2], b[3]);

    /* Free and re-allocate — same-size block should be reused */
    pool.f_free(&pool, a);
    char *c = pool.f_alloc(&pool, 64);
    snprintf(c, 64, "reused slot");
    printf("[4] freed and re-allocated 64 bytes: '%s' (reused=%d)\n", c, c == a);

    /* Large allocation (> page size) goes to heap, not pool page */
    void *big = pool.f_alloc(&pool, 8192);
    printf("[5] large alloc (8192 bytes): %s\n", big ? "ok" : "FAIL");

    /* All pool memory is freed in one call */
    ml_pool_destroy(&pool);
    printf("[6] pool destroyed — all memory released at once\n");

    return 0;
}
