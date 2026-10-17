/*
 * ringbuf.c — lock-free SPSC ring buffer backed by shared memory
 *
 * Build: cd .. && make && cd examples && make ringbuf && ./ringbuf
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clib.h"

int main(void)
{
    const char *path = "/tmp/clib_example_ringbuf";
    ml_kring_t  ring;

    /* Create: 1 MiB ring, must be power of 2 */
    if (ml_kring_create((char *)path, 1048576, &ring) != 0) {
        fprintf(stderr, "ring create failed (may need /tmp write access)\n");
        return 1;
    }
    printf("[1] ring created at %s (length=%u)\n", path, ring.m_kbuff->m_length);

    /* Producer: write some messages */
    const char *msgs[] = {"hello", "world", "lock-free", "spsc"};
    for (int i = 0; i < 4; i++) {
        ml_uint_t n = ml_kring_input(&ring, (void *)msgs[i], strlen(msgs[i]) + 1);
        printf("[2] wrote '%s' (%u bytes)\n", msgs[i], n);
    }

    /* Consumer: read them back in order */
    char buf[256];
    for (int i = 0; i < 4; i++) {
        ml_uint_t n = ml_kring_output(&ring, buf, sizeof(buf));
        if (n > 0) {
            printf("[3] read '%s' (%u bytes), %u remaining\n",
                   buf, n, ml_kring_touse(&ring));
        }
    }

    printf("[4] empty=%d vacant=%u\n", ml_kring_empty(&ring), ml_kring_vacant(&ring));

    ml_kring_destroy(&ring);
    printf("[5] ring destroyed\n");

    return 0;
}
