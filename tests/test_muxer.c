/*
 * test_muxer.c — Unit tests for ml_muxer FFmpeg muxer module.
 */
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "test_framework.h"
#include "../include/ml_muxer.h"
#include "../include/ml_pool.h"
#include "../src/ml_muxer.c"

static ml_pool_t g_pool;

/* Minimal H.264 NAL units for muxing tests.
 * These are synthesized — enough to produce a non-empty output file,
 * not decodable. */
static const uint8_t h264_sps[] = {
    0x67, 0x42, 0x00, 0x1E,  /* profile=baseline, level=3.0 */
    0x8C, 0x1E, 0xFC, 0xE3
};
static const uint8_t h264_pps[] = {
    0x68, 0xCE, 0x3C, 0x80
};
/* Minimal IDR — actually a valid tiny slice */
static const uint8_t h264_idr[] = {
    0x65, 0x88, 0x82, 0x00, 0x04, 0xFC, 0xE2
};
static const uint8_t h264_slice[] = {
    0x41, 0x9A, 0x00, 0x10, 0x80
};

/* Assemble extradata: SPS + PPS with start codes */
static int make_extradata(uint8_t *buf, int maxlen)
{
    int off = 0;
    memcpy(buf + off, "\x00\x00\x00\x01", 4); off += 4;
    memcpy(buf + off, h264_sps, sizeof(h264_sps)); off += sizeof(h264_sps);
    memcpy(buf + off, "\x00\x00\x00\x01", 4); off += 4;
    memcpy(buf + off, h264_pps, sizeof(h264_pps)); off += sizeof(h264_pps);
    return off;
}

/* ── lifecycle ──────────────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    ml_muxer_t *m = ml_muxer_create("/tmp/test_muxer_create.mp4", ML_MUXER_FMT_MP4, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create mp4");
    ml_muxer_destroy(m);

    m = ml_muxer_create("/tmp/test_muxer_create.flv", ML_MUXER_FMT_FLV, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create flv");
    ml_muxer_destroy(m);

    m = ml_muxer_create("/tmp/test_muxer_create.mkv", ML_MUXER_FMT_MKV, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create mkv");
    ml_muxer_destroy(m);
}

static void test_create_null(void)
{
    ASSERT_PTR_NULL(ml_muxer_create(NULL, ML_MUXER_FMT_MP4, &g_pool), "NULL filename");
}

static void test_create_bad_path(void)
{
    ml_muxer_t *m = ml_muxer_create("/no/such/dir/file.mp4", ML_MUXER_FMT_MP4, &g_pool);
    /* create succeeds — failure is at write_header time */
    ASSERT_PTR_NOT_NULL(m, "create works even if path doesn't exist yet");
    /* write_header should fail */
    int ret = ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 640, 480, 30, 1, NULL, 0);
    ASSERT_EQ(ret, 0, "set_video ok");
    ret = ml_muxer_write_header(m);
    ASSERT(ret < 0, "write_header fails with bad path");
    ml_muxer_destroy(m);
}

/* ── error sequencing ───────────────────────────────────────────────── */

static void test_cant_write_header_twice(void)
{
    ml_muxer_t *m = ml_muxer_create("/tmp/test_muxer_hdr2.mp4", ML_MUXER_FMT_MP4, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create");

    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 320, 240, 30, 1, NULL, 0),
              0, "set_video");
    ASSERT_EQ(ml_muxer_write_header(m), 0, "first header ok");
    ASSERT(ml_muxer_write_header(m) < 0, "second header fails");
    ml_muxer_destroy(m);
}

static void test_cant_set_video_after_header(void)
{
    ml_muxer_t *m = ml_muxer_create("/tmp/test_muxer_setafter.mp4", ML_MUXER_FMT_MP4, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create");

    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 320, 240, 30, 1, NULL, 0),
              0, "set_video ok");
    ASSERT_EQ(ml_muxer_write_header(m), 0, "header ok");
    ASSERT(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 640, 480, 30, 1, NULL, 0) < 0,
           "set_video after header fails");
    ml_muxer_destroy(m);
}

static void test_cant_write_without_header(void)
{
    ml_muxer_t *m = ml_muxer_create("/tmp/test_muxer_noheader.mp4", ML_MUXER_FMT_MP4, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create");

    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 320, 240, 30, 1, NULL, 0),
              0, "set_video");
    ASSERT(ml_muxer_write_frame(m, h264_idr, sizeof(h264_idr), 0, 1) < 0,
           "write without header fails");
    ml_muxer_destroy(m);
}

static void test_null_checks(void)
{
    ASSERT(ml_muxer_set_video(NULL, ML_MUXER_CODEC_H264, 640, 480, 30, 1, NULL, 0) < 0,
           "NULL m → set_video fails");
    ASSERT(ml_muxer_write_header(NULL) < 0, "NULL m → write_header fails");
    ASSERT(ml_muxer_write_frame(NULL, h264_idr, sizeof(h264_idr), 0, 1) < 0,
           "NULL m → write_frame fails");

    {
        ml_muxer_t *m = ml_muxer_create("/tmp/test_muxer_null.mp4", ML_MUXER_FMT_MP4, &g_pool);
        ASSERT_PTR_NOT_NULL(m, "create");
        ASSERT(ml_muxer_write_frame(m, NULL, 10, 0, 0) < 0, "NULL data fails");
        ASSERT(ml_muxer_write_frame(m, h264_idr, 0, 0, 1) < 0, "len=0 fails");
        ml_muxer_destroy(m);
    }

    /* destroy with NULL is safe */
    ml_muxer_destroy(NULL);
}

/* ── round-trip: mux to file ────────────────────────────────────────── */

static void test_muxer_mp4_with_frames(void)
{
    const char *path = "/tmp/test_muxer_output.mp4";
    uint8_t extradata[128];
    int extralen;
    ml_muxer_t *m;
    int i;

    unlink(path);

    m = ml_muxer_create(path, ML_MUXER_FMT_MP4, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create");

    extralen = make_extradata(extradata, sizeof(extradata));
    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 640, 480, 30, 1,
                                 extradata, extralen), 0, "set_video");
    ASSERT_EQ(ml_muxer_write_header(m), 0, "write_header");

    /* first frame is IDR (keyframe) */
    ASSERT_EQ(ml_muxer_write_frame(m, h264_idr, sizeof(h264_idr), 0, 1),
              0, "write IDR");
    /* then 9 more P-frames */
    for (i = 1; i < 10; i++) {
        ASSERT_EQ(ml_muxer_write_frame(m, h264_slice, sizeof(h264_slice),
                                       (int64_t)i * 33, 0), 0, "write P");
    }

    ml_muxer_destroy(m);

    /* file should exist and have non-trivial size */
    FILE *f = fopen(path, "rb");
    ASSERT_PTR_NOT_NULL(f, "output file exists");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    ASSERT(sz > 100, "output file > 100 bytes");
    unlink(path);
}

static void test_muxer_flv_with_frames(void)
{
    const char *path = "/tmp/test_muxer_output.flv";
    uint8_t extradata[128];
    ml_muxer_t *m;
    int i;

    unlink(path);

    m = ml_muxer_create(path, ML_MUXER_FMT_FLV, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create flv");

    /* FLV needs extradata */
    int extralen = make_extradata(extradata, sizeof(extradata));
    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 320, 240, 25, 1,
                                 extradata, extralen), 0, "set_video");
    ASSERT_EQ(ml_muxer_write_header(m), 0, "write_header");

    ASSERT_EQ(ml_muxer_write_frame(m, h264_idr, sizeof(h264_idr), 0, 1),
              0, "write IDR");
    for (i = 1; i < 5; i++) {
        ASSERT_EQ(ml_muxer_write_frame(m, h264_slice, sizeof(h264_slice),
                                       (int64_t)i * 40, 0), 0, "write P");
    }

    ml_muxer_destroy(m);

    FILE *f = fopen(path, "rb");
    ASSERT_PTR_NOT_NULL(f, "output file exists");
    fseek(f, 0, SEEK_END);
    ASSERT(ftell(f) > 50, "flv > 50 bytes");
    fclose(f);
    unlink(path);
}

static void test_muxer_mkv_with_frames(void)
{
    const char *path = "/tmp/test_muxer_output.mkv";
    uint8_t extradata[128];
    ml_muxer_t *m;
    int i;

    unlink(path);

    m = ml_muxer_create(path, ML_MUXER_FMT_MKV, &g_pool);
    ASSERT_PTR_NOT_NULL(m, "create mkv");

    int extralen = make_extradata(extradata, sizeof(extradata));
    ASSERT_EQ(ml_muxer_set_video(m, ML_MUXER_CODEC_H264, 1280, 720, 30, 1,
                                 extradata, extralen), 0, "set_video");
    ASSERT_EQ(ml_muxer_write_header(m), 0, "write_header");

    ASSERT_EQ(ml_muxer_write_frame(m, h264_idr, sizeof(h264_idr), 0, 1),
              0, "write IDR");
    for (i = 1; i < 10; i++) {
        ASSERT_EQ(ml_muxer_write_frame(m, h264_slice, sizeof(h264_slice),
                                       (int64_t)i * 33, 0), 0, "write P");
    }

    ml_muxer_destroy(m);

    FILE *f = fopen(path, "rb");
    ASSERT_PTR_NOT_NULL(f, "output file exists");
    fseek(f, 0, SEEK_END);
    ASSERT(ftell(f) > 100, "mkv > 100 bytes");
    fclose(f);
    unlink(path);
}

/* ── suite entry ────────────────────────────────────────────────────── */

void test_muxer_suite(void)
{
    ml_pool_create(&g_pool, 0);

    TEST_SUITE("ml_muxer — FFmpeg video muxer");

    RUN_TEST("create/destroy all formats",  { test_create_destroy(); });
    RUN_TEST("create NULL filename",        { test_create_null(); });
    RUN_TEST("create bad path",             { test_create_bad_path(); });
    RUN_TEST("can't write_header twice",    { test_cant_write_header_twice(); });
    RUN_TEST("can't set_video after hdr",   { test_cant_set_video_after_header(); });
    RUN_TEST("can't write without header",  { test_cant_write_without_header(); });
    RUN_TEST("null checks",                 { test_null_checks(); });
    RUN_TEST("mux MP4 10 frames",           { test_muxer_mp4_with_frames(); });
    RUN_TEST("mux FLV 5 frames",            { test_muxer_flv_with_frames(); });
    RUN_TEST("mux MKV 10 frames",           { test_muxer_mkv_with_frames(); });

    ml_pool_destroy(&g_pool);
}
