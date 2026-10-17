/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_websocket.c — RFC 6455 frame encode/decode + handshake.
 */

#include "ml_websocket.h"
#include "ml_base64.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <openssl/sha.h>
#include <openssl/rand.h>

/* ── frame parsing ──────────────────────────────────────────────────── */

int
ml_ws_parse_frame(const char *data, int len, ml_ws_frame_t *frame)
{
    int hdr_len, consumed;
    uint64_t payload_len;
    const unsigned char *p;
    uint8_t b0, b1;

    if (!data || len < 2 || !frame) return -1;

    p  = (const unsigned char *)data;
    b0 = p[0];
    b1 = p[1];

    memset(frame, 0, sizeof(*frame));

    frame->fin    = (b0 >> 7) & 1;
    frame->opcode = b0 & 0x0F;
    frame->masked = (b1 >> 7) & 1;

    payload_len = b1 & 0x7F;
    hdr_len     = 2;

    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = ((uint64_t)p[2] << 8) | p[3];
        hdr_len = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = ((uint64_t)p[2] << 56) | ((uint64_t)p[3] << 48) |
                      ((uint64_t)p[4] << 40) | ((uint64_t)p[5] << 32) |
                      ((uint64_t)p[6] << 24) | ((uint64_t)p[7] << 16) |
                      ((uint64_t)p[8] << 8)  | (uint64_t)p[9];
        hdr_len = 10;
    }

    if (frame->masked) {
        if (len < hdr_len + 4) return 0;
        frame->mask[0] = p[hdr_len];
        frame->mask[1] = p[hdr_len + 1];
        frame->mask[2] = p[hdr_len + 2];
        frame->mask[3] = p[hdr_len + 3];
        hdr_len += 4;
    }

    if (payload_len > ML_WS_MAX_FRAME_SIZE)
        return -1;

    consumed = hdr_len + (int)payload_len;
    if (len < consumed) return 0;

    frame->payload     = (char *)data + hdr_len;
    frame->payload_len = (int)payload_len;

    if (frame->masked)
        ml_ws_mask(frame->payload, payload_len, frame->mask);

    return consumed;
}

/* ── frame encoding ─────────────────────────────────────────────────── */

int
ml_ws_encode_frame(int opcode, const char *payload, int payload_len,
                   int masked, char *out, int out_len)
{
    int hdr_pos = 0;
    uint8_t b0, b1;

    if (!out || out_len < 2) return -1;
    if (payload_len < 0) return -1;
    if (payload_len > ML_WS_MAX_FRAME_SIZE) return -1;

    b0 = (uint8_t)(0x80 | (opcode & 0x0F));  /* FIN=1 */
    b1 = (uint8_t)(masked ? 0x80 : 0x00);
    out[hdr_pos++] = (char)b0;

    if (payload_len < 126) {
        b1 |= (uint8_t)payload_len;
        out[hdr_pos++] = (char)b1;
    } else if (payload_len <= 65535) {
        b1 |= 126;
        out[hdr_pos++] = (char)b1;
        if (out_len < hdr_pos + 2) return -1;
        out[hdr_pos++] = (char)((payload_len >> 8) & 0xFF);
        out[hdr_pos++] = (char)(payload_len & 0xFF);
    } else {
        b1 |= 127;
        out[hdr_pos++] = (char)b1;
        if (out_len < hdr_pos + 8) return -1;
        /* write 8-byte extended length in big-endian */
        int i;
        for (i = 7; i >= 0; i--)
            out[hdr_pos++] = (char)((payload_len >> (i * 8)) & 0xFF);
    }

    if (masked) {
        uint8_t mask_key[4];
        int i;

        if (out_len < hdr_pos + 4) return -1;

        {
            struct timespec ts;
            unsigned int seed;
            clock_gettime(CLOCK_REALTIME, &ts);
            seed = (unsigned int)(ts.tv_nsec ^ ts.tv_sec);
            for (i = 0; i < 4; i++) {
                seed = seed * 1103515245u + 12345u;
                mask_key[i] = (uint8_t)((seed >> 16) & 0xFF);
            }
        }

        memcpy(out + hdr_pos, mask_key, 4);
        hdr_pos += 4;

        if (payload && payload_len > 0) {
            if (out_len < hdr_pos + payload_len) return -1;
            memcpy(out + hdr_pos, payload, payload_len);
            ml_ws_mask(out + hdr_pos, payload_len, mask_key);
            hdr_pos += payload_len;
        }
    } else {
        if (payload && payload_len > 0) {
            if (out_len < hdr_pos + payload_len) return -1;
            memcpy(out + hdr_pos, payload, payload_len);
            hdr_pos += payload_len;
        }
    }

    return hdr_pos;
}

int
ml_ws_encode_close(int status_code, const char *reason,
                   int masked, char *out, int out_len)
{
    char payload[128];
    int plen = 0;

    if (status_code > 0) {
        /* 2-byte status code in network byte order */
        payload[0] = (char)((status_code >> 8) & 0xFF);
        payload[1] = (char)(status_code & 0xFF);
        plen = 2;
    }

    if (reason) {
        int rlen = (int)strlen(reason);
        int max_reason = (int)(sizeof(payload) - plen);
        if (rlen > max_reason) rlen = max_reason;
        memcpy(payload + plen, reason, rlen);
        plen += rlen;
    }

    return ml_ws_encode_frame(ML_WS_OPCODE_CLOSE, payload, plen, masked,
                              out, out_len);
}

/* ── masking ────────────────────────────────────────────────────────── */

void
ml_ws_mask(char *data, int len, const uint8_t mask[4])
{
    int i;
    for (i = 0; i < len; i++)
        data[i] ^= (char)mask[i % 4];
}

/* ── handshake ──────────────────────────────────────────────────────── */

void
ml_ws_generate_key(char *key_out)
{
    unsigned char random_bytes[16];

    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        /* fallback: LCG seeded from clock + stack */
        int i;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        unsigned int seed = (unsigned int)(ts.tv_nsec ^ ts.tv_sec ^ (uintptr_t)&key_out);
        for (i = 0; i < 16; i++) {
            seed = seed * 1103515245u + 12345u;
            random_bytes[i] = (unsigned char)((seed >> 16) & 0xFF);
        }
    }

    ml_base64_encode(random_bytes, 16, key_out, 25);
}

char *
ml_ws_accept_key(const char *key, char *accept_out)
{
    unsigned char sha1_out[SHA_DIGEST_LENGTH];
    char concat[256];
    int klen;

    if (!key || !accept_out) return NULL;
    klen = (int)strlen(key);
    if (klen < 1) return NULL;

    /* concat: key || GUID */
    snprintf(concat, sizeof(concat), "%s%s", key, ML_WS_GUID);

    SHA1((const unsigned char *)concat, strlen(concat), sha1_out);

    ml_base64_encode(sha1_out, SHA_DIGEST_LENGTH, accept_out, 29);
    return accept_out;
}

int
ml_ws_client_handshake(const char *host, const char *path,
                       const char *key, char *out, int out_len)
{
    int n;
    if (!host || !path || !key || !out || out_len < 1)
        return -1;

    n = snprintf(out, out_len,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, key);

    return (n >= out_len) ? -1 : n;
}

int
ml_ws_server_handshake(const char *key, char *out, int out_len)
{
    char accept_key[29];
    int n;

    if (!key || !out || out_len < 1)
        return -1;

    if (!ml_ws_accept_key(key, accept_key))
        return -1;

    n = snprintf(out, out_len,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    return (n >= out_len) ? -1 : n;
}

/* ── debug ──────────────────────────────────────────────────────────── */

const char *
ml_ws_opcode_name(int opcode)
{
    switch (opcode) {
    case ML_WS_OPCODE_CONT:   return "continuation";
    case ML_WS_OPCODE_TEXT:   return "text";
    case ML_WS_OPCODE_BINARY: return "binary";
    case ML_WS_OPCODE_CLOSE:  return "close";
    case ML_WS_OPCODE_PING:   return "ping";
    case ML_WS_OPCODE_PONG:   return "pong";
    default:                  return "unknown";
    }
}
