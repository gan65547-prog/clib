/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 *
 * Base64 encoding and decoding.
 */

#ifndef __ML_BASE64_H__
#define __ML_BASE64_H__

#define ML_BASE64_ENC_SIZE(x)  (((x)+2) / 3 * 4 + 1)
#define ML_BASE64_DEC_SIZE(x)  ((x) / 4 * 3)

/* Encode binary data to base64 text. Returns encoded length, or 0 if output buffer is too small. */
extern int ml_base64_encode(const unsigned char *inbuf, int ilen, char *outbuf, int olen);

/* Decode base64 text to binary data. Returns decoded length, or -1 on error. */
extern int ml_base64_decode(const char *inbuf, int ilen, unsigned char *outbuf, int olen);

#endif
