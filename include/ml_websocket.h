/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * ml_websocket — RFC 6455 WebSocket protocol implementation.
 *
 * Pure protocol layer: frame encode/decode, HTTP upgrade handshake,
 * masking.  Stateless API — no connection tracking or I/O.
 * Suitable for embedding in mlfw plugin modules or standalone use.
 */

#ifndef __ML_WEBSOCKET_H__
#define __ML_WEBSOCKET_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── opcodes (RFC 6455 §5.2) ────────────────────────────────────────── */

#define ML_WS_OPCODE_CONT   0x0   /* continuation frame */
#define ML_WS_OPCODE_TEXT   0x1   /* text (UTF-8) */
#define ML_WS_OPCODE_BINARY 0x2   /* binary */
#define ML_WS_OPCODE_CLOSE  0x8   /* connection close */
#define ML_WS_OPCODE_PING   0x9   /* ping */
#define ML_WS_OPCODE_PONG   0xA   /* pong */

/* ── close status codes (RFC 6455 §7.4) ─────────────────────────────── */

#define ML_WS_CLOSE_NORMAL      1000
#define ML_WS_CLOSE_GOING_AWAY  1001
#define ML_WS_CLOSE_PROTO_ERR   1002
#define ML_WS_CLOSE_BAD_DATA    1003
#define ML_WS_CLOSE_NO_STATUS   1005
#define ML_WS_CLOSE_POLICY      1008
#define ML_WS_CLOSE_TOO_BIG     1009
#define ML_WS_CLOSE_SERVER_ERR  1011

/* ── limits ─────────────────────────────────────────────────────────── */

#define ML_WS_MAX_FRAME_SIZE    (64 * 1024)   /* 64 KiB per frame */
#define ML_WS_HANDSHAKE_SIZE    512           /* max upgrade req/resp */

/* GUID for WebSocket accept key (RFC 6455 §4.2.2) */
#define ML_WS_GUID              "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* ── frame descriptor ───────────────────────────────────────────────── */

typedef struct {
    int      fin;           /* 1 = final fragment */
    int      opcode;        /* text/binary/close/ping/pong */
    int      masked;        /* 1 = payload is masked (client→server) */
    uint8_t  mask[4];       /* masking key (valid only when masked=1) */
    char    *payload;       /* pointer into input buffer, NOT owned */
    uint64_t  payload_len;   /* payload byte length */
} ml_ws_frame_t;

/* ── handshake ──────────────────────────────────────────────────────── */

/*
 * Build a client HTTP upgrade request.
 * Writes into `out` (max `out_len`), returns bytes written or -1 on overflow.
 */
int ml_ws_client_handshake(const char *host, const char *path,
                           const char *key, char *out, int out_len);

/*
 * Build a server HTTP upgrade response (101 Switching Protocols).
 * `key` is the Sec-WebSocket-Key from the client request.
 * Returns bytes written or -1 on overflow.
 */
int ml_ws_server_handshake(const char *key, char *out, int out_len);

/*
 * Compute the Sec-WebSocket-Accept value from a client key.
 * key: client's Sec-WebSocket-Key (24-char base64).
 * accept_out: at least 29 bytes (28 + NUL).
 * Returns accept_out on success, NULL on bad input.
 */
char *ml_ws_accept_key(const char *key, char *accept_out);

/*
 * Generate a random 16-byte value and base64-encode it into a 24-char key.
 * `key_out` must be at least 25 bytes (24 + NUL).
 */
void ml_ws_generate_key(char *key_out);

/* ── frame parsing ──────────────────────────────────────────────────── */

/*
 * Parse one WebSocket frame from a buffer.
 * Returns number of bytes consumed (>= 0), 0 if need more data, -1 on error.
 * The frame's `payload` pointer points into `data` — do not free separately.
 *
 * Use for server-side frames (client→server: masked) and client-side
 * frames (server→client: unmasked).
 */
int ml_ws_parse_frame(const char *data, int len, ml_ws_frame_t *frame);

/*
 * Encode a WebSocket frame for sending.
 *   opcode: one of ML_WS_OPCODE_*
 *   payload, payload_len: frame body (NULL/0 is valid for close/ping/pong)
 *   masked: non-zero to apply client→server masking
 *   out, out_len: destination buffer
 * Returns bytes written, or -1 on overflow.
 *
 * Use masked=1 for client→server frames, masked=0 for server→client.
 */
int ml_ws_encode_frame(int opcode, const char *payload, int payload_len,
                       int masked, char *out, int out_len);

/*
 * Encode a close frame with a status code and optional reason.
 * Status code is sent in network byte order per RFC 6455 §5.5.1.
 */
int ml_ws_encode_close(int status_code, const char *reason,
                       int masked, char *out, int out_len);

/* Apply / remove XOR masking in-place. idempotent (masking twice = original). */
void ml_ws_mask(char *data, size_t len, const uint8_t mask[4]);

/* Human-readable opcode name for debugging/logging. */
const char *ml_ws_opcode_name(int opcode);

#ifdef __cplusplus
}
#endif

#endif /* __ML_WEBSOCKET_H__ */
