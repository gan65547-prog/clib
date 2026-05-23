/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_bitmap.h"

#define BITMAP_MASK 1
#define BITMAP_BYTE 8



void ml_bitmap_init(ml_bitmap_t * btmp)
{
    memset(btmp->bits, 0, btmp->btmp_bytes_len);   
}


int ml_bitmap_test(ml_bitmap_t * btmp, unsigned  bit_idx)
{
    unsigned byte_idx = bit_idx / BITMAP_BYTE;
    unsigned bit_odd  = bit_idx & (BITMAP_BYTE-1);
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}


int ml_bitmap_get(ml_bitmap_t * btmp, unsigned * bit_idx)
{
    unsigned idx_byte = 0, idx_bit = 0;

    while ((idx_byte < btmp->btmp_bytes_len) && (0xff == btmp->bits[idx_byte])) {
        idx_byte++;
    }

    if (idx_byte == btmp->btmp_bytes_len) {
        return -1;
    }

    while ((unsigned char )(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) { 
        idx_bit++;
    }
    *bit_idx = idx_byte * BITMAP_BYTE + idx_bit;

    return 0;
}


void ml_bitmap_set(ml_bitmap_t * btmp, unsigned bit_idx, unsigned char value)
{
    unsigned byte_idx = bit_idx / 8;
    unsigned bit_odd  = bit_idx & (BITMAP_BYTE-1);

    if (value) {
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } 
    else {
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}

