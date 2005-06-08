/* Copyright (C) 1997, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Sample unpacking procedures */
#include "gx.h"
#include "gxsample.h"
#include "gxfixed.h"
#include "gximage.h"

/* ---------------- Lookup tables ---------------- */

/*
 * Define standard tables for spreading 1-bit input data.
 * Note that these depend on the end-orientation of the CPU.
 * We can't simply define them as byte arrays, because
 * they might not wind up properly long- or short-aligned.
 */
#define map4tox(z,a,b,c,d)\
    z, z^a, z^b, z^(a+b),\
    z^c, z^(a+c), z^(b+c), z^(a+b+c),\
    z^d, z^(a+d), z^(b+d), z^(a+b+d),\
    z^(c+d), z^(a+c+d), z^(b+c+d), z^(a+b+c+d)
/* Work around warnings from really picky compilers. */
#ifdef __STDC__
#  define n0L 0xffffffffU
#  define ffL8 0x0000ff00U
#  define ffL16 0x00ff0000U
#  define ffL24 0xff000000U
#else
#if arch_sizeof_long == 4
/*
 * The compiler evaluates long expressions mod 2^32.  Even very picky
 * compilers allow assigning signed longs to unsigned longs, so we use
 * signed constants.
 */
#  define n0L (-1)
#  define ffL8 0x0000ff00
#  define ffL16 0x00ff0000
#  define ffL24 (-0x01000000)
#else
/*
 * The compiler evaluates long expressions mod 2^64.
 */
#  define n0L 0xffffffffL
#  define ffL8 0x0000ff00L
#  define ffL16 0x00ff0000L
#  define ffL24 0xff000000L
#endif
#endif
#if arch_is_big_endian
const bits32 lookup4x1to32_identity[16] = {
    map4tox(0, 0xff, ffL8, ffL16, ffL24)
};
const bits32 lookup4x1to32_inverted[16] = {
    map4tox(n0L, 0xff, ffL8, ffL16, ffL24)
};
#else /* !arch_is_big_endian */
const bits32 lookup4x1to32_identity[16] = {
    map4tox(0, ffL24, ffL16, ffL8, 0xff)
};
const bits32 lookup4x1to32_inverted[16] = {
    map4tox(n0L, ffL24, ffL16, ffL8, 0xff)
};
#endif
#undef n0L
#undef ffL8
#undef ffL16
#undef ffL24

/* ---------------- Unpacking procedures ---------------- */

const byte *
sample_unpack_copy(byte * bptr, int *pdata_x, const byte * data, int data_x,
		uint dsize, const sample_map *ignore_smap, int spread,
		int ignore_num_components_per_plane)
{				/* We're going to use the data right away, so no copying is needed. */
    *pdata_x = data_x;
    return data;
}

const byte *
sample_unpack_1(byte * bptr, int *pdata_x, const byte * data, int data_x,
		uint dsize, const sample_map *smap, int spread,
		int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    const byte *psrc = data + (data_x >> 3);
    int left = dsize - (data_x >> 3);

    if (spread == 1) {
	bits32 *bufp = (bits32 *) bptr;
	const bits32 *map = &ptab->lookup4x1to32[0];
	uint b;

	if (left & 1) {
	    b = psrc[0];
	    bufp[0] = map[b >> 4];
	    bufp[1] = map[b & 0xf];
	    psrc++, bufp += 2;
	}
	left >>= 1;
	while (left--) {
	    b = psrc[0];
	    bufp[0] = map[b >> 4];
	    bufp[1] = map[b & 0xf];
	    b = psrc[1];
	    bufp[2] = map[b >> 4];
	    bufp[3] = map[b & 0xf];
	    psrc += 2, bufp += 4;
	}
    } else {
	byte *bufp = bptr;
	const byte *map = &ptab->lookup8[0];

	while (left--) {
	    uint b = *psrc++;

	    *bufp = map[b >> 7];
	    bufp += spread;
	    *bufp = map[(b >> 6) & 1];
	    bufp += spread;
	    *bufp = map[(b >> 5) & 1];
	    bufp += spread;
	    *bufp = map[(b >> 4) & 1];
	    bufp += spread;
	    *bufp = map[(b >> 3) & 1];
	    bufp += spread;
	    *bufp = map[(b >> 2) & 1];
	    bufp += spread;
	    *bufp = map[(b >> 1) & 1];
	    bufp += spread;
	    *bufp = map[b & 1];
	    bufp += spread;
	}
    }
    *pdata_x = data_x & 7;
    return bptr;
}

const byte *
sample_unpack_2(byte * bptr, int *pdata_x, const byte * data, int data_x,
		uint dsize, const sample_map *smap, int spread,
		int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    const byte *psrc = data + (data_x >> 2);
    int left = dsize - (data_x >> 2);

    if (spread == 1) {
	bits16 *bufp = (bits16 *) bptr;
	const bits16 *map = &ptab->lookup2x2to16[0];

	while (left--) {
	    uint b = *psrc++;

	    *bufp++ = map[b >> 4];
	    *bufp++ = map[b & 0xf];
	}
    } else {
	byte *bufp = bptr;
	const byte *map = &ptab->lookup8[0];

	while (left--) {
	    unsigned b = *psrc++;

	    *bufp = map[b >> 6];
	    bufp += spread;
	    *bufp = map[(b >> 4) & 3];
	    bufp += spread;
	    *bufp = map[(b >> 2) & 3];
	    bufp += spread;
	    *bufp = map[b & 3];
	    bufp += spread;
	}
    }
    *pdata_x = data_x & 3;
    return bptr;
}

const byte *
sample_unpack_4(byte * bptr, int *pdata_x, const byte * data, int data_x,
		uint dsize, const sample_map *smap, int spread,
		int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    byte *bufp = bptr;
    const byte *psrc = data + (data_x >> 1);
    int left = dsize - (data_x >> 1);
    const byte *map = &ptab->lookup8[0];

    while (left--) {
	uint b = *psrc++;

	*bufp = map[b >> 4];
	bufp += spread;
	*bufp = map[b & 0xf];
	bufp += spread;
    }
    *pdata_x = data_x & 1;
    return bptr;
}

const byte *
sample_unpack_8(byte * bptr, int *pdata_x, const byte * data, int data_x,
		uint dsize, const sample_map *smap, int spread,
		int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    byte *bufp = bptr;
    const byte *psrc = data + data_x;

    *pdata_x = 0;
    if (spread == 1) {
	if (ptab->lookup8[0] != 0 ||
	    ptab->lookup8[255] != 255
	    ) {
	    uint left = dsize - data_x;
	    const byte *map = &ptab->lookup8[0];

	    while (left--)
		*bufp++ = map[*psrc++];
	} else {		/* No copying needed, and we'll use the data right away. */
	    return psrc;
	}
    } else {
	int left = dsize - data_x;
	const byte *map = &ptab->lookup8[0];

	for (; left--; psrc++, bufp += spread)
	    *bufp = map[*psrc];
    }
    return bptr;
}
