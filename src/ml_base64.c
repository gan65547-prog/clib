/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include "ml_base64.h"


static const unsigned char base64_map[] =
{
    0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
};

static const char base64_string[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/* Base64 encode. Returns encoded length, or 0 if output buffer is too small. */
int ml_base64_encode(const unsigned char *inbuf, int ilen, char *outbuf, int olen)
{
    char *ret, *dst;
    unsigned i_bits = 0;
    int i_shift = 0;
    int bytes_remaining = ilen;

    if (olen < ML_BASE64_ENC_SIZE(ilen))
        return 0;

    ret = dst = outbuf;
    while (bytes_remaining) {
        i_bits = (i_bits << 8) + *inbuf++;
        bytes_remaining--;
        i_shift += 8;

        do {
            *dst++ = base64_string[(i_bits << 6 >> i_shift) & 0x3f];
            i_shift -= 6;
        } while (i_shift > 6 || (bytes_remaining == 0 && i_shift > 0));
    }
    while ((dst - ret) & 3)
        *dst++ = '=';
    *dst = '\0';

    return dst-ret;
}


/* Base64 decode. Returns decoded length, or -1 on error. */
int ml_base64_decode(const char *inbuf, int ilen, unsigned char *outbuf, int olen)
{
    int i, v;
    unsigned char *dst = outbuf;

    v = 0;
    for (i = 0; inbuf[i] && inbuf[i] != '=' && i < ilen; i++) {
        unsigned int index= inbuf[i]-43;
        if (index >= sizeof(base64_map) || base64_map[index] == 0xff)
            return -1;
        v = (v << 6) + base64_map[index];
        if (i & 3) {
            if (dst - outbuf < olen) {
                *dst++ = v >> (6 - 2 * (i & 3));
            } else {
                return -1;
            }
        }
    }

    return dst - outbuf;
}


