/*
 * ml_bitmap — Bit array with O(1) first-free-bit lookup.
 *
 * Maintains a byte-array bitmask and a cursor that tracks the first free
 * bit index.  Useful for fast resource allocation (connection slots, etc.).
 */
#ifndef __ML_BITMAP_H__
#define __ML_BITMAP_H__

typedef struct ml_bitmap_s             ml_bitmap_t;

struct ml_bitmap_s {
   unsigned   btmp_bytes_len;
   unsigned char * bits;
};

/* Initialise a bitmap. Must be called before any other operation. */
void ml_bitmap_init(ml_bitmap_t * btmp);

/* Test whether a specific bit is set (returns non-zero if set). */
int  ml_bitmap_test(ml_bitmap_t * btmp, unsigned bit_idx);

/* Find the first free (0) bit and return its index via bit_idx. Returns 0 on success. */
int  ml_bitmap_get(ml_bitmap_t * btmp, unsigned * bit_idx);

/* Set a bit to the given value (0 or 1). */
void ml_bitmap_set(ml_bitmap_t * btmp, unsigned bit_idx, unsigned char value);

#endif
