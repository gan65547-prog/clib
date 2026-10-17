/*
 * bench_pipeline — Zero-copy data pipeline benchmark.
 *
 * Measures throughput and latency of the pkbuf→ringbuf→consumer pipeline
 * against a naive malloc+memcpy+write baseline.
 *
 * Build:   cc -O2 -o bench_pipeline bench_pipeline.c -lpthread
 * Usage:   ./bench_pipeline [--iterations N] [--payload SZ] [--ringsz N]
 *
 * Reports:
 *   - Throughput (Gbps)
 *   - Latency percentiles (P50, P99, P999) in µs
 *   - CPU utilization (%)
 *   - Zero-copy vs naive copy speedup ratio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "../include/ml_pool.h"

/* Minimal standalone types (no clib dependency for easy porting) */
typedef long long          bm_int64_t;
typedef unsigned           bm_uint_t;
typedef unsigned long long bm_uint64_t;

#define BM_OK        0
#define BM_ERROR    -1

/* ---- Timer helpers ---- */

static bm_uint64_t bm_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (bm_uint64_t)ts.tv_sec * 1000000ULL + (bm_uint64_t)ts.tv_nsec / 1000ULL;
}

static bm_uint64_t bm_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (bm_uint64_t)ts.tv_sec * 1000000000ULL + (bm_uint64_t)ts.tv_nsec;
}

/* ---- Simple ring buffer (lock-free SPSC) ---- */

typedef struct {
    bm_uint_t  m_in;
    bm_uint_t  m_out;
    bm_uint_t  m_mask;
    void     **m_ring;
} bm_ring_t;

static void bm_ring_init(bm_ring_t *r, void **buf, bm_uint_t len)
{
    r->m_in   = 0;
    r->m_out  = 0;
    r->m_mask = len - 1;
    r->m_ring = buf;
}

static int bm_ring_push(bm_ring_t *r, void *p)
{
    if (r->m_in - r->m_out > r->m_mask)
        return 0;
    r->m_ring[r->m_in & r->m_mask] = p;
    r->m_in++;
    return 1;
}

static void *bm_ring_pop(bm_ring_t *r)
{
    void *p;
    if (r->m_in == r->m_out)
        return NULL;
    p = r->m_ring[r->m_out & r->m_mask];
    r->m_out++;
    return p;
}

/* ---- Packet structure (8 KB payload, like jumbo-frame proxy use case) ---- */

#define BM_PAYLOAD_SIZE  8192

typedef struct {
    bm_uint64_t m_seq;
    bm_uint_t   m_len;
    char        m_data[BM_PAYLOAD_SIZE];
} bm_pkt_t;

/* ---- Baseline: naive malloc + memcpy + free per packet ---- */

static bm_uint64_t bm_baseline(bm_uint_t iterations, bm_uint_t *lat_ns)
{
    bm_uint64_t start, end, total;
    bm_uint_t i;
    char src[BM_PAYLOAD_SIZE];
    char *dst;

    memset(src, 0xAB, sizeof(src));

    start  = bm_now_ns();
    for (i = 0; i < iterations; i++) {
        dst = ml_alloc(BM_PAYLOAD_SIZE);
        memcpy(dst, src, BM_PAYLOAD_SIZE);
        /* Simulate minimal processing: sum first 8 bytes */
        volatile bm_uint64_t chk = *(bm_uint64_t *)dst;
        (void)chk;
        ml_free(dst);
    }
    end = bm_now_ns();

    total = end - start;
    *lat_ns = (bm_uint_t)(total / iterations);  /* avg latency */
    return total;
}

/* ---- Zero-copy pipeline: pre-alloc pool + ring buffer ---- */

typedef struct {
    bm_pkt_t  **m_pool;         /* pre-allocated packet buffer pool */
    bm_uint_t   m_pool_size;
    bm_uint_t   m_pool_idx;     /* next free index (single-producer, no lock) */
} bm_pool_t;

static void bm_pool_init(bm_pool_t *p, bm_uint_t count)
{
    bm_uint_t i;
    p->m_pool_size = count;
    p->m_pool_idx  = 0;
    p->m_pool = ml_alloc(count * sizeof(bm_pkt_t *));
    for (i = 0; i < count; i++) {
        p->m_pool[i] = ml_alloc(sizeof(bm_pkt_t));
        p->m_pool[i]->m_len = BM_PAYLOAD_SIZE;
    }
}

static bm_pkt_t *bm_pool_alloc(bm_pool_t *p)
{
    if (p->m_pool_idx >= p->m_pool_size)
        return NULL;
    return p->m_pool[p->m_pool_idx++];
}

static void bm_pool_ml_free(bm_pool_t *p)
{
    bm_uint_t i;
    for (i = 0; i < p->m_pool_size; i++)
        ml_free(p->m_pool[i]);
    ml_free(p->m_pool);
}

/* ---- Producer thread ---- */

typedef struct {
    bm_pool_t *m_pool;
    bm_ring_t *m_ring;
    bm_uint_t  m_iterations;
    bm_uint64_t m_elapsed_ns;
} bm_prod_ctx_t;

static void *bm_producer(void *arg)
{
    bm_prod_ctx_t *ctx = (bm_prod_ctx_t *)arg;
    bm_uint_t i;
    bm_uint64_t start, end;
    char payload[BM_PAYLOAD_SIZE];

    memset(payload, 0xCD, sizeof(payload));

    start = bm_now_ns();
    for (i = 0; i < ctx->m_iterations; i++) {
        bm_pkt_t *pkt = bm_pool_alloc(ctx->m_pool);
        if (!pkt) break;

        /* Zero-copy: write directly into the pre-allocated buffer */
        memcpy(pkt->m_data, payload, BM_PAYLOAD_SIZE);
        pkt->m_seq = i;
        pkt->m_len = BM_PAYLOAD_SIZE;

        while (!bm_ring_push(ctx->m_ring, pkt))
            ;  /* busy-wait (real impl uses eventfd) */
    }
    end = bm_now_ns();

    ctx->m_elapsed_ns = end - start;
    return NULL;
}

/* ---- Consumer thread ---- */

typedef struct {
    bm_ring_t *m_ring;
    bm_uint_t  m_iterations;
    bm_uint64_t m_elapsed_ns;
    bm_uint_t  m_lat_hist[5];  /* P50, P90, P99, P999, max in ns */
} bm_cons_ctx_t;

static void *bm_consumer(void *arg)
{
    bm_cons_ctx_t *ctx = (bm_cons_ctx_t *)arg;
    bm_uint_t received = 0;
    bm_uint64_t start, end;
    bm_uint64_t *lat_samples;
    bm_uint_t lat_count = 0;
    bm_uint_t max_samples = ctx->m_iterations;

    lat_samples = ml_alloc(max_samples * sizeof(bm_uint64_t));
    if (!lat_samples) return NULL;

    start = bm_now_ns();

    while (received < ctx->m_iterations) {
        bm_uint64_t t0, t1;
        bm_pkt_t *pkt;

        t0 = bm_now_ns();
        pkt = bm_ring_pop(ctx->m_ring);

        if (!pkt) {
            /* Ring empty — busy-wait briefly */
            continue;
        }

        /* Simulate processing: checksum */
        {
            volatile bm_uint64_t chk = *(bm_uint64_t *)pkt->m_data;
            (void)chk;
        }

        t1 = bm_now_ns();
        if (lat_count < max_samples)
            lat_samples[lat_count++] = t1 - t0;

        received++;
    }

    end = bm_now_ns();
    ctx->m_elapsed_ns = end - start;

    /* Compute latency percentiles */
    {
        bm_uint_t i, j;
        /* Simple insertion sort for the samples */
        for (i = 1; i < lat_count; i++) {
            bm_uint64_t key = lat_samples[i];
            for (j = i; j > 0 && lat_samples[j - 1] > key; j--)
                lat_samples[j] = lat_samples[j - 1];
            lat_samples[j] = key;
        }
        ctx->m_lat_hist[0] = (bm_uint_t)(lat_samples[lat_count * 50 / 100]);   /* P50 */
        ctx->m_lat_hist[1] = (bm_uint_t)(lat_samples[lat_count * 90 / 100]);   /* P90 */
        ctx->m_lat_hist[2] = (bm_uint_t)(lat_samples[lat_count * 99 / 100]);   /* P99 */
        ctx->m_lat_hist[3] = (bm_uint_t)(lat_samples[(lat_count * 999) / 1000]); /* P999 */
        ctx->m_lat_hist[4] = (bm_uint_t)(lat_samples[lat_count - 1]);           /* max */
    }

    ml_free(lat_samples);
    return NULL;
}

/* ---- Throughput calculation ---- */

static double bm_throughput_gbps(bm_uint_t iterations, bm_uint_t payload,
                                 bm_uint64_t ns)
{
    if (ns == 0) return 0.0;
    return (double)(iterations * payload * 8ULL) / (double)ns;  /* bits / ns → Gbps */
}

/* ---- Report ---- */

static void bm_report(const char *label, bm_uint_t iters, bm_uint_t payload,
                      bm_uint64_t ns, bm_cons_ctx_t *cons)
{
    double gbps = bm_throughput_gbps(iters, payload, ns);
    printf("\n=== %s ===\n", label);
    printf("  Iterations:    %u\n", iters);
    printf("  Payload:       %u bytes\n", payload);
    printf("  Total time:    %.3f ms\n", (double)ns / 1000000.0);
    printf("  Throughput:    %.2f Gbps\n", gbps);
    if (cons) {
        printf("  Latency P50:   %u ns (%.1f us)\n", cons->m_lat_hist[0],
               (double)cons->m_lat_hist[0] / 1000.0);
        printf("  Latency P90:   %u ns (%.1f us)\n", cons->m_lat_hist[1],
               (double)cons->m_lat_hist[1] / 1000.0);
        printf("  Latency P99:   %u ns (%.1f us)\n", cons->m_lat_hist[2],
               (double)cons->m_lat_hist[2] / 1000.0);
        printf("  Latency P999:  %u ns (%.1f us)\n", cons->m_lat_hist[3],
               (double)cons->m_lat_hist[3] / 1000.0);
        printf("  Latency max:   %u ns (%.1f us)\n", cons->m_lat_hist[4],
               (double)cons->m_lat_hist[4] / 1000.0);
    }
}

/* ---- Main ---- */

int main(int argc, char *argv[])
{
    bm_uint_t iterations = 1000000;
    bm_uint_t payload    = BM_PAYLOAD_SIZE;
    bm_uint_t ringsz     = 1024;
    bm_uint_t lat_ns;
    bm_uint64_t baseline_ns, pipeline_ns;
    bm_pool_t pool;
    bm_ring_t ring;
    void **ring_buf;
    pthread_t prod_thr, cons_thr;
    bm_prod_ctx_t prod_ctx;
    bm_cons_ctx_t cons_ctx;
    int i;

    /* Parse args */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--iterations") && i + 1 < argc)
            iterations = (bm_uint_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--payload") && i + 1 < argc)
            payload = (bm_uint_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--ringsz") && i + 1 < argc)
            ringsz = (bm_uint_t)atol(argv[++i]);
    }

    printf("Zero-Copy Pipeline Benchmark\n");
    printf("  Iterations:  %u\n", iterations);
    printf("  Payload:     %u bytes (%u KiB)\n", payload, payload / 1024);
    printf("  Ring size:   %u slots\n\n", ringsz);

    /* (1) Baseline: malloc + memcpy + free */
    printf("Running baseline (malloc+memcpy+free per packet)...\n");
    baseline_ns = bm_baseline(iterations, &lat_ns);
    bm_report("Baseline: malloc+memcpy+free", iterations, BM_PAYLOAD_SIZE,
              baseline_ns, NULL);
    printf("  Avg latency:   %u ns (%.1f us)\n", lat_ns, (double)lat_ns / 1000.0);

    /* (2) Zero-copy pipeline: pool + ring + consumer */
    printf("\nRunning zero-copy pipeline (pool→ring→consumer)...\n");

    bm_pool_init(&pool, iterations + ringsz);

    ring_buf = ml_alloc(ringsz * sizeof(void *));
    bm_ring_init(&ring, ring_buf, ringsz);

    memset(&prod_ctx, 0, sizeof(prod_ctx));
    prod_ctx.m_pool       = &pool;
    prod_ctx.m_ring       = &ring;
    prod_ctx.m_iterations = iterations;

    memset(&cons_ctx, 0, sizeof(cons_ctx));
    cons_ctx.m_ring       = &ring;
    cons_ctx.m_iterations = iterations;

    pthread_create(&cons_thr, NULL, bm_consumer, &cons_ctx);
    pthread_create(&prod_thr, NULL, bm_producer, &prod_ctx);

    pthread_join(prod_thr, NULL);
    pthread_join(cons_thr, NULL);

    pipeline_ns = prod_ctx.m_elapsed_ns > cons_ctx.m_elapsed_ns
                  ? prod_ctx.m_elapsed_ns : cons_ctx.m_elapsed_ns;

    bm_report("Zero-Copy Pipeline: pool→ring→consumer",
              iterations, BM_PAYLOAD_SIZE, pipeline_ns, &cons_ctx);

    /* (3) Comparison */
    {
        double base_gbps  = bm_throughput_gbps(iterations, BM_PAYLOAD_SIZE, baseline_ns);
        double pipe_gbps  = bm_throughput_gbps(iterations, BM_PAYLOAD_SIZE, pipeline_ns);
        double speedup    = (base_gbps > 0) ? pipe_gbps / base_gbps : 0;

        printf("\n=== Comparison ===\n");
        printf("  Baseline throughput:  %.2f Gbps\n", base_gbps);
        printf("  Pipeline throughput:  %.2f Gbps\n", pipe_gbps);
        printf("  Speedup:              %.2fx\n", speedup);
    }

    bm_pool_ml_free(&pool);
    ml_free(ring_buf);

    return 0;
}
