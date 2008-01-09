/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2006, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue 09/09/2006

 This is an AES implementation that uses only 8-bit byte operations on the
 cipher state (there are options to use 32-bit types if available).

 The combination of mix columns and byte substitution used here is based on
 that developed by Karl Malbrain. His contribution is acknowledged.
 */

/* define if you have a fast memcpy function on your system */
#if 1
#  define HAVE_MEMCPY
#  include <string.h>
#  if defined( _MSC_VER )
#    include <intrin.h>
#    pragma intrinsic( memcpy )
#  endif
#endif

/* define if you have fast 32-bit types on your system */
#if 1
#  define HAVE_UINT_32T
#endif

/* alternative versions (test for performance on your system) */
#if 1
#  define VERSION_1
#endif

#include "aes.h"

#define sb_data(w) {\
    w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), w(0xc5),\
    w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), w(0xab), w(0x76),\
    w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), w(0x59), w(0x47), w(0xf0),\
    w(0xad), w(0xd4), w(0xa2), w(0xaf), w(0x9c), w(0xa4), w(0x72), w(0xc0),\
    w(0xb7), w(0xfd), w(0x93), w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc),\
    w(0x34), w(0xa5), w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15),\
    w(0x04), w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a),\
    w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), w(0x75),\
    w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), w(0x5a), w(0xa0),\
    w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), w(0xe3), w(0x2f), w(0x84),\
    w(0x53), w(0xd1), w(0x00), w(0xed), w(0x20), w(0xfc), w(0xb1), w(0x5b),\
    w(0x6a), w(0xcb), w(0xbe), w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf),\
    w(0xd0), w(0xef), w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85),\
    w(0x45), w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8),\
    w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), w(0xf5),\
    w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), w(0xf3), w(0xd2),\
    w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), w(0x97), w(0x44), w(0x17),\
    w(0xc4), w(0xa7), w(0x7e), w(0x3d), w(0x64), w(0x5d), w(0x19), w(0x73),\
    w(0x60), w(0x81), w(0x4f), w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88),\
    w(0x46), w(0xee), w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb),\
    w(0xe0), w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c),\
    w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), w(0x79),\
    w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), w(0x4e), w(0xa9),\
    w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), w(0x7a), w(0xae), w(0x08),\
    w(0xba), w(0x78), w(0x25), w(0x2e), w(0x1c), w(0xa6), w(0xb4), w(0xc6),\
    w(0xe8), w(0xdd), w(0x74), w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a),\
    w(0x70), w(0x3e), w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e),\
    w(0x61), w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e),\
    w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), w(0x94),\
    w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), w(0x28), w(0xdf),\
    w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), w(0xe6), w(0x42), w(0x68),\
    w(0x41), w(0x99), w(0x2d), w(0x0f), w(0xb0), w(0x54), w(0xbb), w(0x16) }

#define isb_data(w) {\
    w(0x52), w(0x09), w(0x6a), w(0xd5), w(0x30), w(0x36), w(0xa5), w(0x38),\
    w(0xbf), w(0x40), w(0xa3), w(0x9e), w(0x81), w(0xf3), w(0xd7), w(0xfb),\
    w(0x7c), w(0xe3), w(0x39), w(0x82), w(0x9b), w(0x2f), w(0xff), w(0x87),\
    w(0x34), w(0x8e), w(0x43), w(0x44), w(0xc4), w(0xde), w(0xe9), w(0xcb),\
    w(0x54), w(0x7b), w(0x94), w(0x32), w(0xa6), w(0xc2), w(0x23), w(0x3d),\
    w(0xee), w(0x4c), w(0x95), w(0x0b), w(0x42), w(0xfa), w(0xc3), w(0x4e),\
    w(0x08), w(0x2e), w(0xa1), w(0x66), w(0x28), w(0xd9), w(0x24), w(0xb2),\
    w(0x76), w(0x5b), w(0xa2), w(0x49), w(0x6d), w(0x8b), w(0xd1), w(0x25),\
    w(0x72), w(0xf8), w(0xf6), w(0x64), w(0x86), w(0x68), w(0x98), w(0x16),\
    w(0xd4), w(0xa4), w(0x5c), w(0xcc), w(0x5d), w(0x65), w(0xb6), w(0x92),\
    w(0x6c), w(0x70), w(0x48), w(0x50), w(0xfd), w(0xed), w(0xb9), w(0xda),\
    w(0x5e), w(0x15), w(0x46), w(0x57), w(0xa7), w(0x8d), w(0x9d), w(0x84),\
    w(0x90), w(0xd8), w(0xab), w(0x00), w(0x8c), w(0xbc), w(0xd3), w(0x0a),\
    w(0xf7), w(0xe4), w(0x58), w(0x05), w(0xb8), w(0xb3), w(0x45), w(0x06),\
    w(0xd0), w(0x2c), w(0x1e), w(0x8f), w(0xca), w(0x3f), w(0x0f), w(0x02),\
    w(0xc1), w(0xaf), w(0xbd), w(0x03), w(0x01), w(0x13), w(0x8a), w(0x6b),\
    w(0x3a), w(0x91), w(0x11), w(0x41), w(0x4f), w(0x67), w(0xdc), w(0xea),\
    w(0x97), w(0xf2), w(0xcf), w(0xce), w(0xf0), w(0xb4), w(0xe6), w(0x73),\
    w(0x96), w(0xac), w(0x74), w(0x22), w(0xe7), w(0xad), w(0x35), w(0x85),\
    w(0xe2), w(0xf9), w(0x37), w(0xe8), w(0x1c), w(0x75), w(0xdf), w(0x6e),\
    w(0x47), w(0xf1), w(0x1a), w(0x71), w(0x1d), w(0x29), w(0xc5), w(0x89),\
    w(0x6f), w(0xb7), w(0x62), w(0x0e), w(0xaa), w(0x18), w(0xbe), w(0x1b),\
    w(0xfc), w(0x56), w(0x3e), w(0x4b), w(0xc6), w(0xd2), w(0x79), w(0x20),\
    w(0x9a), w(0xdb), w(0xc0), w(0xfe), w(0x78), w(0xcd), w(0x5a), w(0xf4),\
    w(0x1f), w(0xdd), w(0xa8), w(0x33), w(0x88), w(0x07), w(0xc7), w(0x31),\
    w(0xb1), w(0x12), w(0x10), w(0x59), w(0x27), w(0x80), w(0xec), w(0x5f),\
    w(0x60), w(0x51), w(0x7f), w(0xa9), w(0x19), w(0xb5), w(0x4a), w(0x0d),\
    w(0x2d), w(0xe5), w(0x7a), w(0x9f), w(0x93), w(0xc9), w(0x9c), w(0xef),\
    w(0xa0), w(0xe0), w(0x3b), w(0x4d), w(0xae), w(0x2a), w(0xf5), w(0xb0),\
    w(0xc8), w(0xeb), w(0xbb), w(0x3c), w(0x83), w(0x53), w(0x99), w(0x61),\
    w(0x17), w(0x2b), w(0x04), w(0x7e), w(0xba), w(0x77), w(0xd6), w(0x26),\
    w(0xe1), w(0x69), w(0x14), w(0x63), w(0x55), w(0x21), w(0x0c), w(0x7d) }

#define mm_data(w) {\
    w(0x00), w(0x01), w(0x02), w(0x03), w(0x04), w(0x05), w(0x06), w(0x07),\
    w(0x08), w(0x09), w(0x0a), w(0x0b), w(0x0c), w(0x0d), w(0x0e), w(0x0f),\
    w(0x10), w(0x11), w(0x12), w(0x13), w(0x14), w(0x15), w(0x16), w(0x17),\
    w(0x18), w(0x19), w(0x1a), w(0x1b), w(0x1c), w(0x1d), w(0x1e), w(0x1f),\
    w(0x20), w(0x21), w(0x22), w(0x23), w(0x24), w(0x25), w(0x26), w(0x27),\
    w(0x28), w(0x29), w(0x2a), w(0x2b), w(0x2c), w(0x2d), w(0x2e), w(0x2f),\
    w(0x30), w(0x31), w(0x32), w(0x33), w(0x34), w(0x35), w(0x36), w(0x37),\
    w(0x38), w(0x39), w(0x3a), w(0x3b), w(0x3c), w(0x3d), w(0x3e), w(0x3f),\
    w(0x40), w(0x41), w(0x42), w(0x43), w(0x44), w(0x45), w(0x46), w(0x47),\
    w(0x48), w(0x49), w(0x4a), w(0x4b), w(0x4c), w(0x4d), w(0x4e), w(0x4f),\
    w(0x50), w(0x51), w(0x52), w(0x53), w(0x54), w(0x55), w(0x56), w(0x57),\
    w(0x58), w(0x59), w(0x5a), w(0x5b), w(0x5c), w(0x5d), w(0x5e), w(0x5f),\
    w(0x60), w(0x61), w(0x62), w(0x63), w(0x64), w(0x65), w(0x66), w(0x67),\
    w(0x68), w(0x69), w(0x6a), w(0x6b), w(0x6c), w(0x6d), w(0x6e), w(0x6f),\
    w(0x70), w(0x71), w(0x72), w(0x73), w(0x74), w(0x75), w(0x76), w(0x77),\
    w(0x78), w(0x79), w(0x7a), w(0x7b), w(0x7c), w(0x7d), w(0x7e), w(0x7f),\
    w(0x80), w(0x81), w(0x82), w(0x83), w(0x84), w(0x85), w(0x86), w(0x87),\
    w(0x88), w(0x89), w(0x8a), w(0x8b), w(0x8c), w(0x8d), w(0x8e), w(0x8f),\
    w(0x90), w(0x91), w(0x92), w(0x93), w(0x94), w(0x95), w(0x96), w(0x97),\
    w(0x98), w(0x99), w(0x9a), w(0x9b), w(0x9c), w(0x9d), w(0x9e), w(0x9f),\
    w(0xa0), w(0xa1), w(0xa2), w(0xa3), w(0xa4), w(0xa5), w(0xa6), w(0xa7),\
    w(0xa8), w(0xa9), w(0xaa), w(0xab), w(0xac), w(0xad), w(0xae), w(0xaf),\
    w(0xb0), w(0xb1), w(0xb2), w(0xb3), w(0xb4), w(0xb5), w(0xb6), w(0xb7),\
    w(0xb8), w(0xb9), w(0xba), w(0xbb), w(0xbc), w(0xbd), w(0xbe), w(0xbf),\
    w(0xc0), w(0xc1), w(0xc2), w(0xc3), w(0xc4), w(0xc5), w(0xc6), w(0xc7),\
    w(0xc8), w(0xc9), w(0xca), w(0xcb), w(0xcc), w(0xcd), w(0xce), w(0xcf),\
    w(0xd0), w(0xd1), w(0xd2), w(0xd3), w(0xd4), w(0xd5), w(0xd6), w(0xd7),\
    w(0xd8), w(0xd9), w(0xda), w(0xdb), w(0xdc), w(0xdd), w(0xde), w(0xdf),\
    w(0xe0), w(0xe1), w(0xe2), w(0xe3), w(0xe4), w(0xe5), w(0xe6), w(0xe7),\
    w(0xe8), w(0xe9), w(0xea), w(0xeb), w(0xec), w(0xed), w(0xee), w(0xef),\
    w(0xf0), w(0xf1), w(0xf2), w(0xf3), w(0xf4), w(0xf5), w(0xf6), w(0xf7),\
    w(0xf8), w(0xf9), w(0xfa), w(0xfb), w(0xfc), w(0xfd), w(0xfe), w(0xff) }

#define WPOLY   0x011b
#define DPOLY   0x008d
#define f1(x)   (x)
#define f2(x)   ((x<<1) ^ (((x>>7) & 1) * WPOLY))
#define f4(x)   ((x<<2) ^ (((x>>6) & 1) * WPOLY) ^ (((x>>6) & 2) * WPOLY))
#define f8(x)   ((x<<3) ^ (((x>>5) & 1) * WPOLY) ^ (((x>>5) & 2) * WPOLY) \
                        ^ (((x>>5) & 4) * WPOLY))
#define d2(x)   (((x) >> 1) ^ ((x) & 1 ? DPOLY : 0))

#define f3(x)   (f2(x) ^ x)
#define f9(x)   (f8(x) ^ x)
#define fb(x)   (f8(x) ^ f2(x) ^ x)
#define fd(x)   (f8(x) ^ f4(x) ^ x)
#define fe(x)   (f8(x) ^ f4(x) ^ f2(x))

static const uint_8t s_box[256]     =  sb_data(f1);
static const uint_8t inv_s_box[256] = isb_data(f1);

static const uint_8t gfm2_s_box[256] = sb_data(f2);
static const uint_8t gfm3_s_box[256] = sb_data(f3);

static const uint_8t gfmul_9[256] = mm_data(f9);
static const uint_8t gfmul_b[256] = mm_data(fb);
static const uint_8t gfmul_d[256] = mm_data(fd);
static const uint_8t gfmul_e[256] = mm_data(fe);

#if defined( HAVE_UINT_32T )
  typedef unsigned long uint_32t;
#endif

#if defined( HAVE_MEMCPY )
#  define block_copy(d, s, l) memcpy(d, s, l)
#  define block16_copy(d, s)  memcpy(d, s, N_BLOCK)
#else
#  define block_copy(d, s, l) copy_block(d, s, l)
#  define block16_copy(d, s)  copy_block16(d, s)
#endif

/* block size 'nn' must be a multiple of four */

static void copy_block16( void *d, const void *s )
{
#if defined( HAVE_UINT_32T )
    ((uint_32t*)d)[ 0] = ((uint_32t*)s)[ 0];
    ((uint_32t*)d)[ 1] = ((uint_32t*)s)[ 1];
    ((uint_32t*)d)[ 2] = ((uint_32t*)s)[ 2];
    ((uint_32t*)d)[ 3] = ((uint_32t*)s)[ 3];
#else
    ((uint_8t*)d)[ 0] = ((uint_8t*)s)[ 0];
    ((uint_8t*)d)[ 1] = ((uint_8t*)s)[ 1];
    ((uint_8t*)d)[ 2] = ((uint_8t*)s)[ 2];
    ((uint_8t*)d)[ 3] = ((uint_8t*)s)[ 3];
    ((uint_8t*)d)[ 4] = ((uint_8t*)s)[ 4];
    ((uint_8t*)d)[ 5] = ((uint_8t*)s)[ 5];
    ((uint_8t*)d)[ 6] = ((uint_8t*)s)[ 6];
    ((uint_8t*)d)[ 7] = ((uint_8t*)s)[ 7];
    ((uint_8t*)d)[ 8] = ((uint_8t*)s)[ 8];
    ((uint_8t*)d)[ 9] = ((uint_8t*)s)[ 9];
    ((uint_8t*)d)[10] = ((uint_8t*)s)[10];
    ((uint_8t*)d)[11] = ((uint_8t*)s)[11];
    ((uint_8t*)d)[12] = ((uint_8t*)s)[12];
    ((uint_8t*)d)[13] = ((uint_8t*)s)[13];
    ((uint_8t*)d)[14] = ((uint_8t*)s)[14];
    ((uint_8t*)d)[15] = ((uint_8t*)s)[15];
#endif
}

static void copy_block( void * d, void *s, uint_8t nn )
{
    while( nn-- )
        *((uint_8t*)d)++ = *((uint_8t*)s)++;
}

static void xor_block( void *d, const void *s )
{
#if defined( HAVE_UINT_32T )
    ((uint_32t*)d)[ 0] ^= ((uint_32t*)s)[ 0];
    ((uint_32t*)d)[ 1] ^= ((uint_32t*)s)[ 1];
    ((uint_32t*)d)[ 2] ^= ((uint_32t*)s)[ 2];
    ((uint_32t*)d)[ 3] ^= ((uint_32t*)s)[ 3];
#else
    ((uint_8t*)d)[ 0] ^= ((uint_8t*)s)[ 0];
    ((uint_8t*)d)[ 1] ^= ((uint_8t*)s)[ 1];
    ((uint_8t*)d)[ 2] ^= ((uint_8t*)s)[ 2];
    ((uint_8t*)d)[ 3] ^= ((uint_8t*)s)[ 3];
    ((uint_8t*)d)[ 4] ^= ((uint_8t*)s)[ 4];
    ((uint_8t*)d)[ 5] ^= ((uint_8t*)s)[ 5];
    ((uint_8t*)d)[ 6] ^= ((uint_8t*)s)[ 6];
    ((uint_8t*)d)[ 7] ^= ((uint_8t*)s)[ 7];
    ((uint_8t*)d)[ 8] ^= ((uint_8t*)s)[ 8];
    ((uint_8t*)d)[ 9] ^= ((uint_8t*)s)[ 9];
    ((uint_8t*)d)[10] ^= ((uint_8t*)s)[10];
    ((uint_8t*)d)[11] ^= ((uint_8t*)s)[11];
    ((uint_8t*)d)[12] ^= ((uint_8t*)s)[12];
    ((uint_8t*)d)[13] ^= ((uint_8t*)s)[13];
    ((uint_8t*)d)[14] ^= ((uint_8t*)s)[14];
    ((uint_8t*)d)[15] ^= ((uint_8t*)s)[15];
#endif
}

static void copy_and_key( void *d, const void *s, const void *k )
{
#if defined( HAVE_UINT_32T )
    ((uint_32t*)d)[ 0] = ((uint_32t*)s)[ 0] ^ ((uint_32t*)k)[ 0];
    ((uint_32t*)d)[ 1] = ((uint_32t*)s)[ 1] ^ ((uint_32t*)k)[ 1];
    ((uint_32t*)d)[ 2] = ((uint_32t*)s)[ 2] ^ ((uint_32t*)k)[ 2];
    ((uint_32t*)d)[ 3] = ((uint_32t*)s)[ 3] ^ ((uint_32t*)k)[ 3];
#elif 1
    ((uint_8t*)d)[ 0] = ((uint_8t*)s)[ 0] ^ ((uint_8t*)k)[ 0];
    ((uint_8t*)d)[ 1] = ((uint_8t*)s)[ 1] ^ ((uint_8t*)k)[ 1];
    ((uint_8t*)d)[ 2] = ((uint_8t*)s)[ 2] ^ ((uint_8t*)k)[ 2];
    ((uint_8t*)d)[ 3] = ((uint_8t*)s)[ 3] ^ ((uint_8t*)k)[ 3];
    ((uint_8t*)d)[ 4] = ((uint_8t*)s)[ 4] ^ ((uint_8t*)k)[ 4];
    ((uint_8t*)d)[ 5] = ((uint_8t*)s)[ 5] ^ ((uint_8t*)k)[ 5];
    ((uint_8t*)d)[ 6] = ((uint_8t*)s)[ 6] ^ ((uint_8t*)k)[ 6];
    ((uint_8t*)d)[ 7] = ((uint_8t*)s)[ 7] ^ ((uint_8t*)k)[ 7];
    ((uint_8t*)d)[ 8] = ((uint_8t*)s)[ 8] ^ ((uint_8t*)k)[ 8];
    ((uint_8t*)d)[ 9] = ((uint_8t*)s)[ 9] ^ ((uint_8t*)k)[ 9];
    ((uint_8t*)d)[10] = ((uint_8t*)s)[10] ^ ((uint_8t*)k)[10];
    ((uint_8t*)d)[11] = ((uint_8t*)s)[11] ^ ((uint_8t*)k)[11];
    ((uint_8t*)d)[12] = ((uint_8t*)s)[12] ^ ((uint_8t*)k)[12];
    ((uint_8t*)d)[13] = ((uint_8t*)s)[13] ^ ((uint_8t*)k)[13];
    ((uint_8t*)d)[14] = ((uint_8t*)s)[14] ^ ((uint_8t*)k)[14];
    ((uint_8t*)d)[15] = ((uint_8t*)s)[15] ^ ((uint_8t*)k)[15];
#else
    block16_copy(d, s);
    xor_block(d, k);
#endif
}

static void add_round_key( uint_8t d[N_BLOCK], const uint_8t k[N_BLOCK] )
{
    xor_block(d, k);
}

static void shift_sub_rows( uint_8t st[N_BLOCK] )
{   uint_8t tt;

    st[ 0] = s_box[st[ 0]]; st[ 4] = s_box[st[ 4]];
    st[ 8] = s_box[st[ 8]]; st[12] = s_box[st[12]];

    tt = st[1]; st[ 1] = s_box[st[ 5]]; st[ 5] = s_box[st[ 9]];
    st[ 9] = s_box[st[13]]; st[13] = s_box[ tt ];

    tt = st[2]; st[ 2] = s_box[st[10]]; st[10] = s_box[ tt ];
    tt = st[6]; st[ 6] = s_box[st[14]]; st[14] = s_box[ tt ];

    tt = st[15]; st[15] = s_box[st[11]]; st[11] = s_box[st[ 7]];
    st[ 7] = s_box[st[ 3]]; st[ 3] = s_box[ tt ];
}

static void inv_shift_sub_rows( uint_8t st[N_BLOCK] )
{   uint_8t tt;

    st[ 0] = inv_s_box[st[ 0]]; st[ 4] = inv_s_box[st[ 4]];
    st[ 8] = inv_s_box[st[ 8]]; st[12] = inv_s_box[st[12]];

    tt = st[13]; st[13] = inv_s_box[st[9]]; st[ 9] = inv_s_box[st[5]];
    st[ 5] = inv_s_box[st[1]]; st[ 1] = inv_s_box[ tt ];

    tt = st[2]; st[ 2] = inv_s_box[st[10]]; st[10] = inv_s_box[ tt ];
    tt = st[6]; st[ 6] = inv_s_box[st[14]]; st[14] = inv_s_box[ tt ];

    tt = st[3]; st[ 3] = inv_s_box[st[ 7]]; st[ 7] = inv_s_box[st[11]];
    st[11] = inv_s_box[st[15]]; st[15] = inv_s_box[ tt ];
}

#if defined( VERSION_1 )
  static void mix_sub_columns( uint_8t dt[N_BLOCK] )
  { uint_8t st[N_BLOCK];
    block16_copy(st, dt);
#else
  static void mix_sub_columns( uint_8t dt[N_BLOCK], uint_8t st[N_BLOCK] )
  {
#endif
    dt[ 0] = gfm2_s_box[st[0]] ^ gfm3_s_box[st[5]] ^ s_box[st[10]] ^ s_box[st[15]];
    dt[ 1] = s_box[st[0]] ^ gfm2_s_box[st[5]] ^ gfm3_s_box[st[10]] ^ s_box[st[15]];
    dt[ 2] = s_box[st[0]] ^ s_box[st[5]] ^ gfm2_s_box[st[10]] ^ gfm3_s_box[st[15]];
    dt[ 3] = gfm3_s_box[st[0]] ^ s_box[st[5]] ^ s_box[st[10]] ^ gfm2_s_box[st[15]];

    dt[ 4] = gfm2_s_box[st[4]] ^ gfm3_s_box[st[9]] ^ s_box[st[14]] ^ s_box[st[3]];
    dt[ 5] = s_box[st[4]] ^ gfm2_s_box[st[9]] ^ gfm3_s_box[st[14]] ^ s_box[st[3]];
    dt[ 6] = s_box[st[4]] ^ s_box[st[9]] ^ gfm2_s_box[st[14]] ^ gfm3_s_box[st[3]];
    dt[ 7] = gfm3_s_box[st[4]] ^ s_box[st[9]] ^ s_box[st[14]] ^ gfm2_s_box[st[3]];

    dt[ 8] = gfm2_s_box[st[8]] ^ gfm3_s_box[st[13]] ^ s_box[st[2]] ^ s_box[st[7]];
    dt[ 9] = s_box[st[8]] ^ gfm2_s_box[st[13]] ^ gfm3_s_box[st[2]] ^ s_box[st[7]];
    dt[10] = s_box[st[8]] ^ s_box[st[13]] ^ gfm2_s_box[st[2]] ^ gfm3_s_box[st[7]];
    dt[11] = gfm3_s_box[st[8]] ^ s_box[st[13]] ^ s_box[st[2]] ^ gfm2_s_box[st[7]];

    dt[12] = gfm2_s_box[st[12]] ^ gfm3_s_box[st[1]] ^ s_box[st[6]] ^ s_box[st[11]];
    dt[13] = s_box[st[12]] ^ gfm2_s_box[st[1]] ^ gfm3_s_box[st[6]] ^ s_box[st[11]];
    dt[14] = s_box[st[12]] ^ s_box[st[1]] ^ gfm2_s_box[st[6]] ^ gfm3_s_box[st[11]];
    dt[15] = gfm3_s_box[st[12]] ^ s_box[st[1]] ^ s_box[st[6]] ^ gfm2_s_box[st[11]];
  }

#if defined( VERSION_1 )
  static void inv_mix_sub_columns( uint_8t dt[N_BLOCK] )
  { uint_8t st[N_BLOCK];
    block16_copy(st, dt);
#else
  static void inv_mix_sub_columns( uint_8t dt[N_BLOCK], uint_8t st[N_BLOCK] )
  {
#endif
    dt[ 0] = inv_s_box[gfmul_e[st[ 0]] ^ gfmul_b[st[ 1]] ^ gfmul_d[st[ 2]] ^ gfmul_9[st[ 3]]];
    dt[ 5] = inv_s_box[gfmul_9[st[ 0]] ^ gfmul_e[st[ 1]] ^ gfmul_b[st[ 2]] ^ gfmul_d[st[ 3]]];
    dt[10] = inv_s_box[gfmul_d[st[ 0]] ^ gfmul_9[st[ 1]] ^ gfmul_e[st[ 2]] ^ gfmul_b[st[ 3]]];
    dt[15] = inv_s_box[gfmul_b[st[ 0]] ^ gfmul_d[st[ 1]] ^ gfmul_9[st[ 2]] ^ gfmul_e[st[ 3]]];

    dt[ 4] = inv_s_box[gfmul_e[st[ 4]] ^ gfmul_b[st[ 5]] ^ gfmul_d[st[ 6]] ^ gfmul_9[st[ 7]]];
    dt[ 9] = inv_s_box[gfmul_9[st[ 4]] ^ gfmul_e[st[ 5]] ^ gfmul_b[st[ 6]] ^ gfmul_d[st[ 7]]];
    dt[14] = inv_s_box[gfmul_d[st[ 4]] ^ gfmul_9[st[ 5]] ^ gfmul_e[st[ 6]] ^ gfmul_b[st[ 7]]];
    dt[ 3] = inv_s_box[gfmul_b[st[ 4]] ^ gfmul_d[st[ 5]] ^ gfmul_9[st[ 6]] ^ gfmul_e[st[ 7]]];

    dt[ 8] = inv_s_box[gfmul_e[st[ 8]] ^ gfmul_b[st[ 9]] ^ gfmul_d[st[10]] ^ gfmul_9[st[11]]];
    dt[13] = inv_s_box[gfmul_9[st[ 8]] ^ gfmul_e[st[ 9]] ^ gfmul_b[st[10]] ^ gfmul_d[st[11]]];
    dt[ 2] = inv_s_box[gfmul_d[st[ 8]] ^ gfmul_9[st[ 9]] ^ gfmul_e[st[10]] ^ gfmul_b[st[11]]];
    dt[ 7] = inv_s_box[gfmul_b[st[ 8]] ^ gfmul_d[st[ 9]] ^ gfmul_9[st[10]] ^ gfmul_e[st[11]]];

    dt[12] = inv_s_box[gfmul_e[st[12]] ^ gfmul_b[st[13]] ^ gfmul_d[st[14]] ^ gfmul_9[st[15]]];
    dt[ 1] = inv_s_box[gfmul_9[st[12]] ^ gfmul_e[st[13]] ^ gfmul_b[st[14]] ^ gfmul_d[st[15]]];
    dt[ 6] = inv_s_box[gfmul_d[st[12]] ^ gfmul_9[st[13]] ^ gfmul_e[st[14]] ^ gfmul_b[st[15]]];
    dt[11] = inv_s_box[gfmul_b[st[12]] ^ gfmul_d[st[13]] ^ gfmul_9[st[14]] ^ gfmul_e[st[15]]];
  }

#if defined( AES_ENC_PREKEYED ) || defined( AES_DEC_PREKEYED )

/*  Set the cipher key for the pre-keyed version */

return_type aes_set_key( const unsigned char key[], length_type keylen, aes_context ctx[1] )
{
    uint_8t cc, rc, hi;

    switch( keylen )
    {
    case 16:
    case 128:
        keylen = 16;
        break;
    case 24:
    case 192:
        keylen = 24;
        break;
    case 32:
    case 256:
        keylen = 32;
        break;
    default:
        ctx->rnd = 0;
        return -1;
    }
    block_copy(ctx->ksch, key, keylen);
    hi = (keylen + 28) << 2;
    ctx->rnd = (hi >> 4) - 1;
    for( cc = keylen, rc = 1; cc < hi; cc += 4 )
    {   uint_8t tt, t0, t1, t2, t3;

        t0 = ctx->ksch[cc - 4];
        t1 = ctx->ksch[cc - 3];
        t2 = ctx->ksch[cc - 2];
        t3 = ctx->ksch[cc - 1];
        if( cc % keylen == 0 )
        {
            tt = t0;
            t0 = s_box[t1] ^ rc;
            t1 = s_box[t2];
            t2 = s_box[t3];
            t3 = s_box[tt];
            rc = f2(rc);
        }
        else if( keylen > 24 && cc % keylen == 16 )
        {
            t0 = s_box[t0];
            t1 = s_box[t1];
            t2 = s_box[t2];
            t3 = s_box[t3];
        }
        tt = cc - keylen;
        ctx->ksch[cc + 0] = ctx->ksch[tt + 0] ^ t0;
        ctx->ksch[cc + 1] = ctx->ksch[tt + 1] ^ t1;
        ctx->ksch[cc + 2] = ctx->ksch[tt + 2] ^ t2;
        ctx->ksch[cc + 3] = ctx->ksch[tt + 3] ^ t3;
    }
    return 0;
}

#endif

#if defined( AES_ENC_PREKEYED )

/*  Encrypt a single block of 16 bytes */

return_type aes_encrypt( const unsigned char in[N_BLOCK], unsigned char  out[N_BLOCK], const aes_context ctx[1] )
{
    if( ctx->rnd )
    {
        uint_8t s1[N_BLOCK], r;
        copy_and_key( s1, in, ctx->ksch );

        for( r = 1 ; r < ctx->rnd ; ++r )
#if defined( VERSION_1 )
        {
            mix_sub_columns( s1 );
            add_round_key( s1, ctx->ksch + r * N_BLOCK);
        }
#else
        {   uint_8t s2[N_BLOCK];
            mix_sub_columns( s2, s1 );
            copy_and_key( s1, s2, ctx->ksch + r * N_BLOCK);
        }
#endif
        shift_sub_rows( s1 );
        copy_and_key( out, s1, ctx->ksch + r * N_BLOCK );
    }
    else
        return -1;
    return 0;
}

#endif

#if defined( AES_DEC_PREKEYED )

/*  Decrypt a single block of 16 bytes */

return_type aes_decrypt( const unsigned char in[N_BLOCK], unsigned char out[N_BLOCK], const aes_context ctx[1] )
{
    if( ctx->rnd )
    {
        uint_8t s1[N_BLOCK], r;
        copy_and_key( s1, in, ctx->ksch + ctx->rnd * N_BLOCK );
        inv_shift_sub_rows( s1 );

        for( r = ctx->rnd ; --r ; )
#if defined( VERSION_1 )
        {
            add_round_key( s1, ctx->ksch + r * N_BLOCK );
            inv_mix_sub_columns( s1 );
        }
#else
        {   uint_8t s2[N_BLOCK];
            copy_and_key( s2, s1, ctx->ksch + r * N_BLOCK );
            inv_mix_sub_columns( s1, s2 );
        }
#endif
        copy_and_key( out, s1, ctx->ksch );
    }
    else
        return -1;
    return 0;
}

#endif

#if defined( AES_ENC_128_OTFK )

/*  The 'on the fly' encryption key update for for 128 bit keys */

static void update_encrypt_key_128( uint_8t k[N_BLOCK], uint_8t *rc )
{   uint_8t cc;

    k[0] ^= s_box[k[13]] ^ *rc;
    k[1] ^= s_box[k[14]];
    k[2] ^= s_box[k[15]];
    k[3] ^= s_box[k[12]];
    *rc = f2( *rc );

    for(cc = 4; cc < 16; cc += 4 )
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }
}

/*  Encrypt a single block of 16 bytes with 'on the fly' 128 bit keying */

void aes_encrypt_128( const unsigned char in[N_BLOCK], unsigned char out[N_BLOCK],
                     const unsigned char key[N_BLOCK], unsigned char o_key[N_BLOCK] )
{   uint_8t s1[N_BLOCK], r, rc = 1;

    if(o_key != key)
        block16_copy( o_key, key );
    copy_and_key( s1, in, o_key );

    for( r = 1 ; r < 10 ; ++r )
#if defined( VERSION_1 )
    {
        mix_sub_columns( s1 );
        update_encrypt_key_128( o_key, &rc );
        add_round_key( s1, o_key );
    }
#else
    {   uint_8t s2[N_BLOCK];
        mix_sub_columns( s2, s1 );
        update_encrypt_key_128( o_key, &rc );
        copy_and_key( s1, s2, o_key );
    }
#endif

    shift_sub_rows( s1 );
    update_encrypt_key_128( o_key, &rc );
    copy_and_key( out, s1, o_key );
}

#endif

#if defined( AES_DEC_128_OTFK )

/*  The 'on the fly' decryption key update for for 128 bit keys */

static void update_decrypt_key_128( uint_8t k[N_BLOCK], uint_8t *rc )
{   uint_8t cc;

    for( cc = 12; cc > 0; cc -= 4 )
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }
    *rc = d2(*rc);
    k[0] ^= s_box[k[13]] ^ *rc;
    k[1] ^= s_box[k[14]];
    k[2] ^= s_box[k[15]];
    k[3] ^= s_box[k[12]];
}

/*  Decrypt a single block of 16 bytes with 'on the fly' 128 bit keying */

void aes_decrypt_128( const unsigned char in[N_BLOCK], unsigned char out[N_BLOCK],
                      const unsigned char key[N_BLOCK], unsigned char o_key[N_BLOCK] )
{
    uint_8t s1[N_BLOCK], r, rc = 0x6c;
    if(o_key != key)
        block16_copy( o_key, key );

    copy_and_key( s1, in, o_key );
    inv_shift_sub_rows( s1 );

    for( r = 10 ; --r ; )
#if defined( VERSION_1 )
    {
        update_decrypt_key_128( o_key, &rc );
        add_round_key( s1, o_key );
        inv_mix_sub_columns( s1 );
    }
#else
    {   uint_8t s2[N_BLOCK];
        update_decrypt_key_128( o_key, &rc );
        copy_and_key( s2, s1, o_key );
        inv_mix_sub_columns( s1, s2 );
    }
#endif
    update_decrypt_key_128( o_key, &rc );
    copy_and_key( out, s1, o_key );
}

#endif

#if defined( AES_ENC_256_OTFK )

/*  The 'on the fly' encryption key update for for 256 bit keys */

static void update_encrypt_key_256( uint_8t k[2 * N_BLOCK], uint_8t *rc )
{   uint_8t cc;

    k[0] ^= s_box[k[29]] ^ *rc;
    k[1] ^= s_box[k[30]];
    k[2] ^= s_box[k[31]];
    k[3] ^= s_box[k[28]];
    *rc = f2( *rc );

    for(cc = 4; cc < 16; cc += 4)
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }

    k[16] ^= s_box[k[12]];
    k[17] ^= s_box[k[13]];
    k[18] ^= s_box[k[14]];
    k[19] ^= s_box[k[15]];

    for( cc = 20; cc < 32; cc += 4 )
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }
}

/*  Encrypt a single block of 16 bytes with 'on the fly' 256 bit keying */

void aes_encrypt_256( const unsigned char in[N_BLOCK], unsigned char out[N_BLOCK],
                      const unsigned char key[2 * N_BLOCK], unsigned char o_key[2 * N_BLOCK] )
{
    uint_8t s1[N_BLOCK], r, rc = 1;
    if(o_key != key)
    {
        block16_copy( o_key, key );
        block16_copy( o_key + 16, key + 16 );
    }
    copy_and_key( s1, in, o_key );

    for( r = 1 ; r < 14 ; ++r )
#if defined( VERSION_1 )
    {
        mix_sub_columns(s1);
        if( r & 1 )
            add_round_key( s1, o_key + 16 );
        else
        {
            update_encrypt_key_256( o_key, &rc );
            add_round_key( s1, o_key );
        }
    }
#else
    {   uint_8t s2[N_BLOCK];
        mix_sub_columns( s2, s1 );
        if( r & 1 )
            copy_and_key( s1, s2, o_key + 16 );
        else
        {
            update_encrypt_key_256( o_key, &rc );
            copy_and_key( s1, s2, o_key );
        }
    }
#endif

    shift_sub_rows( s1 );
    update_encrypt_key_256( o_key, &rc );
    copy_and_key( out, s1, o_key );
}

#endif

#if defined( AES_DEC_256_OTFK )

/*  The 'on the fly' encryption key update for for 256 bit keys */

static void update_decrypt_key_256( uint_8t k[2 * N_BLOCK], uint_8t *rc )
{   uint_8t cc;

    for(cc = 28; cc > 16; cc -= 4)
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }

    k[16] ^= s_box[k[12]];
    k[17] ^= s_box[k[13]];
    k[18] ^= s_box[k[14]];
    k[19] ^= s_box[k[15]];

    for(cc = 12; cc > 0; cc -= 4)
    {
        k[cc + 0] ^= k[cc - 4];
        k[cc + 1] ^= k[cc - 3];
        k[cc + 2] ^= k[cc - 2];
        k[cc + 3] ^= k[cc - 1];
    }

    *rc = d2(*rc);
    k[0] ^= s_box[k[29]] ^ *rc;
    k[1] ^= s_box[k[30]];
    k[2] ^= s_box[k[31]];
    k[3] ^= s_box[k[28]];
}

/*  Decrypt a single block of 16 bytes with 'on the fly'
    256 bit keying
*/
void aes_decrypt_256( const unsigned char in[N_BLOCK], unsigned char out[N_BLOCK],
                      const unsigned char key[2 * N_BLOCK], unsigned char o_key[2 * N_BLOCK] )
{
    uint_8t s1[N_BLOCK], r, rc = 0x80;

    if(o_key != key)
    {
        block16_copy( o_key, key );
        block16_copy( o_key + 16, key + 16 );
    }

    copy_and_key( s1, in, o_key );
    inv_shift_sub_rows( s1 );

    for( r = 14 ; --r ; )
#if defined( VERSION_1 )
    {
        if( ( r & 1 ) )
        {
            update_decrypt_key_256( o_key, &rc );
            add_round_key( s1, o_key + 16 );
        }
        else
            add_round_key( s1, o_key );
        inv_mix_sub_columns( s1 );
    }
#else
    {   uint_8t s2[N_BLOCK];
        if( ( r & 1 ) )
        {
            update_decrypt_key_256( o_key, &rc );
            copy_and_key( s2, s1, o_key + 16 );
        }
        else
            copy_and_key( s2, s1, o_key );
        inv_mix_sub_columns( s1, s2 );
    }
#endif
    copy_and_key( out, s1, o_key );
}

#endif
