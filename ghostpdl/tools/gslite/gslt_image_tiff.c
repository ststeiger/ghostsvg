/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gslt_image_tiff.c 2991 2007-12-18 23:05:58Z giles $ */
/* gslt image loading implementation for TIFF images */

#include "std.h"
#include "gsmemory.h"
#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"
#include "jpeglib_.h"
#include "sdct.h"
#include "sjpeg.h"
#include "srlx.h"
#include "slzwx.h"
#include "szlibx.h"
#include "scfx.h"
#include "memory_.h"
#include "gslt_image.h"
#include "gserror.h"

/* import the gs structure descriptor for gslt_image_t */
extern const gs_memory_struct_type_t st_gslt_image;

/*
 * TIFF image loader. Should be enough to support TIFF files in XPS.
 * Baseline TIFF 6.0 plus CMYK, LZW, Flate and JPEG support.
 * Limited bit depths (1,2,4,8).
 * Limited planar configurations (1=chunky).
 * No tiles (easy fix if necessary).
 * TODO: RGBPal images
 */

typedef struct gslt_tiff_s gslt_tiff_t;

struct gslt_tiff_s
{
    /* "file" */
    byte *bp, *rp, *ep;

    /* byte order */
    unsigned order;

    /* where we can find the strips of image data */
    unsigned rowsperstrip;
    unsigned *stripoffsets;
    unsigned *stripbytecounts;

    /* colormap */
    unsigned *colormap;

    /* assorted tags */
    unsigned subfiletype;
    unsigned photometric;
    unsigned compression;
    unsigned imagewidth;
    unsigned imagelength;
    unsigned samplesperpixel;
    unsigned bitspersample;
    unsigned planar;
    unsigned extrasamples;
    unsigned xresolution;
    unsigned yresolution;
    unsigned resolutionunit;
    unsigned fillorder;
    unsigned g3opts;
    unsigned g4opts;
    unsigned predictor;

    unsigned ycbcrsubsamp[2];

    byte *jpegtables;       /* point into "file" buffer */
    unsigned jpegtableslen;
};

enum
{
    TII = 0x4949, /* 'II' */
    TMM = 0x4d4d, /* 'MM' */
    TBYTE = 1,
    TASCII = 2,
    TSHORT = 3,
    TLONG = 4,
    TRATIONAL = 5
};

#define NewSubfileType                          254
#define ImageWidth                              256
#define ImageLength                             257
#define BitsPerSample                           258
#define Compression                             259
#define PhotometricInterpretation               262
#define FillOrder                               266
#define StripOffsets                            273
#define SamplesPerPixel                         277
#define RowsPerStrip                            278
#define StripByteCounts                         279
#define XResolution                             282
#define YResolution                             283
#define PlanarConfiguration                     284
#define T4Options                               292
#define T6Options                               293
#define ResolutionUnit                          296
#define Predictor                               317
#define ColorMap                                320
#define TileWidth                               322
#define TileLength                              323
#define TileOffsets                             324
#define TileByteCounts                          325
#define ExtraSamples                            338
#define JPEGTables                              347
#define YCbCrSubSampling                        520

static const byte bitrev[256] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#ifdef GSLT_DEBUG_TIFF
static void
gslt_debug_tiff(gs_memory_t *mem, gslt_tiff_t *tiff);
#endif

static int
gslt_report_error(stream_state * st, const char *str)
{
    (void) gs_throw1(-1, "%s", str);
    return 0;
}

static inline int
readbyte(gslt_tiff_t *tiff)
{
    if (tiff->rp < tiff->ep)
        return *tiff->rp++;
    return EOF;
}

static inline unsigned
readshort(gslt_tiff_t *tiff)
{
    unsigned a = readbyte(tiff);
    unsigned b = readbyte(tiff);
    if (tiff->order == TII)
        return (b << 8) | a;
    return (a << 8) | b;
}

static inline unsigned
readlong(gslt_tiff_t *tiff)
{
    unsigned a = readbyte(tiff);
    unsigned b = readbyte(tiff);
    unsigned c = readbyte(tiff);
    unsigned d = readbyte(tiff);
    if (tiff->order == TII)
        return (d << 24) | (c << 16) | (b << 8) | a;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static int
gslt_decode_tiff_uncompressed(gs_memory_t *mem, gslt_tiff_t *tiff, byte *rp, byte *rl, byte *wp, byte *wl)
{
    memcpy(wp, tiff->rp, wl - wp);
    return gs_okay;
}

static int
gslt_decode_tiff_packbits(gs_memory_t *mem, gslt_tiff_t *tiff, byte *rp, byte *rl, byte *wp, byte *wl)
{
    stream_RLD_state state;
    stream_cursor_read scr;
    stream_cursor_write scw;
    int code;

    s_init_state((stream_state*)&state, &s_RLD_template, mem);
    state.report_error = gslt_report_error;

    s_RLD_template.set_defaults((stream_state*)&state);
    s_RLD_template.init((stream_state*)&state);

    scr.ptr = rp - 1;
    scr.limit = rl - 1;
    scw.ptr = wp - 1;
    scw.limit = wl - 1;

    code = s_RLD_template.process((stream_state*)&state, &scr, &scw, true);
    if (code == ERRC)
        return gs_throw1(-1, "error in packbits data (code = %d)", code);

    return gs_okay;
}

static int
gslt_decode_tiff_lzw(gs_memory_t *mem, gslt_tiff_t *tiff, byte *rp, byte *rl, byte *wp, byte *wl)
{
    stream_LZW_state state;
    stream_cursor_read scr;
    stream_cursor_write scw;
    int code;

    s_init_state((stream_state*)&state, &s_LZWD_template, mem);
    state.report_error = gslt_report_error;

    s_LZWD_template.set_defaults((stream_state*)&state);

    /* old-style TIFF 5.0 reversed bit order, late change */
    if (rp[0] == 0 && rp[1] & 0x01)
    {
        state.EarlyChange = 0;
        state.FirstBitLowOrder = 1;
    }

    /* new-style TIFF 6.0 normal bit order, early change */
    else
    {
        state.EarlyChange = 1;
        state.FirstBitLowOrder = 0;
    }

    s_LZWD_template.init((stream_state*)&state);

    scr.ptr = rp - 1;
    scr.limit = rl - 1;
    scw.ptr = wp - 1;
    scw.limit = wl - 1;

    code = s_LZWD_template.process((stream_state*)&state, &scr, &scw, true);
    if (code == ERRC)
    {
        s_LZWD_template.release((stream_state*)&state);
        return gs_throw1(-1, "error in lzw data (code = %d)", code);
    }

    s_LZWD_template.release((stream_state*)&state);

    return gs_okay;
}

static int
gslt_decode_tiff_flate(gs_memory_t *mem, gslt_tiff_t *tiff, byte *rp, byte *rl, byte *wp, byte *wl)
{
    stream_zlib_state state;
    stream_cursor_read scr;
    stream_cursor_write scw;
    int code;

    s_init_state((stream_state*)&state, &s_zlibD_template, mem);
    state.report_error = gslt_report_error;

    s_zlibD_template.set_defaults((stream_state*)&state);

    s_zlibD_template.init((stream_state*)&state);

    scr.ptr = rp - 1;
    scr.limit = rl - 1;
    scw.ptr = wp - 1;
    scw.limit = wl - 1;

    code = s_zlibD_template.process((stream_state*)&state, &scr, &scw, true);
    if (code == ERRC)
    {
        s_zlibD_template.release((stream_state*)&state);
        return gs_throw1(-1, "error in flate data (code = %d)", code);
    }

    s_zlibD_template.release((stream_state*)&state);
    return gs_okay;
}

static int
gslt_decode_tiff_fax(gs_memory_t *mem, gslt_tiff_t *tiff, int comp, byte *rp, byte *rl, byte *wp, byte *wl)
{
    stream_CFD_state state;
    stream_cursor_read scr;
    stream_cursor_write scw;
    int code;

    s_init_state((stream_state*)&state, &s_CFD_template, mem);
    state.report_error = gslt_report_error;

    s_CFD_template.set_defaults((stream_state*)&state);

    state.EndOfLine = false;
    state.EndOfBlock = false;
    state.Columns = tiff->imagewidth;
    state.Rows = tiff->imagelength;
    state.BlackIs1 = tiff->photometric == 0;

    state.K = 0;
    if (comp == 4)
        state.K = -1;
    if (comp == 2)
        state.EncodedByteAlign = true;

    s_CFD_template.init((stream_state*)&state);

    scr.ptr = rp - 1;
    scr.limit = rl - 1;
    scw.ptr = wp - 1;
    scw.limit = wl - 1;

    code = s_CFD_template.process((stream_state*)&state, &scr, &scw, true);
    if (code == ERRC)
    {
        s_CFD_template.release((stream_state*)&state);
        return gs_throw1(-1, "error in fax data (code = %d)", code);
    }

    s_CFD_template.release((stream_state*)&state);
    return gs_okay;
}

/*
 * We need more find control over JPEG decoding parameters than
 * the s_DCTD_template filter will give us. So we abuse the
 * filter, and take control after the filter setup (which sets up
 * the memory manager and error handling) and call the gs_jpeg
 * wrappers directly for doing the actual decoding.
 */

static int
gslt_decode_tiff_jpeg(gs_memory_t *mem, gslt_tiff_t *tiff, byte *rp, byte *rl, byte *wp, byte *wl)
{
    stream_DCT_state state; /* used by gs_jpeg_* wrappers */
    jpeg_decompress_data jddp;
    struct jpeg_source_mgr *srcmgr;
    JSAMPROW scanlines[1];
    int stride;
    int code;

    /*
     * Set up the JPEG and DCT filter voodoo.
     */

    s_init_state((stream_state*)&state, &s_DCTD_template, mem);
    state.report_error = gslt_report_error;
    s_DCTD_template.set_defaults((stream_state*)&state);

    state.jpeg_memory = mem;
    state.data.decompress = &jddp;

    jddp.template = s_DCTD_template;
    jddp.memory = mem;
    jddp.scanline_buffer = NULL;

    if ((code = gs_jpeg_create_decompress(&state)) < 0)
        return gs_throw(-1, "error in gs_jpeg_create_decompress");

    s_DCTD_template.init((stream_state*)&state);

    srcmgr = jddp.dinfo.src;

    /*
     * Read the abbreviated table file.
     */

    if (tiff->jpegtables)
    {
        srcmgr->next_input_byte = tiff->jpegtables;
        srcmgr->bytes_in_buffer = tiff->jpegtableslen;

        code = gs_jpeg_read_header(&state, FALSE);
        if (code != JPEG_HEADER_TABLES_ONLY)
            return gs_throw(-1, "error in jpeg table data");
    }

    /*
     * Read the image jpeg header.
     */

    srcmgr->next_input_byte = rp;
    srcmgr->bytes_in_buffer = rl - rp;

    if ((code = gs_jpeg_read_header(&state, TRUE)) < 0)
        return gs_throw(-1, "error in jpeg_read_header");

    /* when TIFF says RGB and libjpeg says YCbCr, libjpeg is wrong */
    if (tiff->photometric == 2 && jddp.dinfo.jpeg_color_space == JCS_YCbCr)
    {
        jddp.dinfo.jpeg_color_space = JCS_RGB;
    }

    /*
     * Decode the strip image data.
     */

    if ((code = gs_jpeg_start_decompress(&state)) < 0)
        return gs_throw(-1, "error in jpeg_start_decompress");

    stride = jddp.dinfo.output_width * jddp.dinfo.output_components;

    while (wp + stride <= wl && jddp.dinfo.output_scanline < jddp.dinfo.output_height)
    {
        scanlines[0] = wp;
        code = gs_jpeg_read_scanlines(&state, scanlines, 1);
        if (code < 0)
            return gs_throw(01, "error in jpeg_read_scanlines");
        wp += stride;
    }

    /*
     * Clean up.
     */

    if ((code = gs_jpeg_finish_decompress(&state)) < 0)
        return gs_throw(-1, "error in jpeg_finish_decompress");

    gs_jpeg_destroy(&state);

    return gs_okay;
}

static inline int
getcomp(byte *line, int x, int bpc)
{
    switch (bpc)
    {
    case 1: return line[x / 8] >> (7 - (x % 8)) & 0x01;
    case 2: return line[x / 4] >> ((3 - (x % 4)) * 2) & 0x03;
    case 4: return line[x / 2] >> ((1 - (x % 2)) * 4) & 0x0f;
    case 8: return line[x];
    }
    return 0;
}

static inline void
putcomp(byte *line, int x, int bpc, int value)
{
    int maxval = (1 << bpc) - 1;

    // clear bits first
    switch (bpc)
    {
    case 1: line[x / 8] &= ~(maxval << (7 - (x % 8))); break;
    case 2: line[x / 4] &= ~(maxval << ((3 - (x % 4)) * 2)); break;
    case 4: line[x / 2] &= ~(maxval << ((1 - (x % 2)) * 4)); break;
    }

    switch (bpc)
    {
    case 1: line[x / 8] |= value << (7 - (x % 8)); break;
    case 2: line[x / 4] |= value << ((3 - (x % 4)) * 2); break;
    case 4: line[x / 2] |= value << ((1 - (x % 2)) * 4); break;
    case 8: line[x] = value; break;
    }
}

static void
gslt_unpredict_tiff(byte *line, int width, int components, int bits)
{
    byte left[32];
    int i, k, v;

    for (k = 0; k < components; k++)
        left[k] = 0;

    for (i = 0; i < width; i++)
    {
        for (k = 0; k < components; k++)
        {
            v = getcomp(line, i * components + k, bits);
            v = v + left[k];
            v = v % (1 << bits);
            putcomp(line, i * components + k, bits, v);
            left[k] = v;
        }
    }
}

static void
gslt_invert_tiff(byte *line, int width, int components, int bits)
{
    int i, k, v;
    int m = (1 << bits) - 1;

    for (i = 0; i < width; i++)
    {
        for (k = 0; k < components; k++)
        {
            v = getcomp(line, i * components + k, bits);
            v = m - v;
            putcomp(line, i * components + k, bits, v);
        }
    }
}

static int
gslt_expand_colormap(gs_memory_t *mem, gslt_tiff_t *tiff, gslt_image_t *image)
{
    int maxval = 1 << image->bits;
    byte *samples;
    byte *src, *dst;
    int stride;
    int x, y;

    /* colormap has first all red, then all green, then all blue values */
    /* colormap values are 0..65535, bits is 4 or 8 */
    /* image can be with or without extrasamples: components is 1 or 2 */

    if (image->components != 1 && image->components != 2)
        return gs_throw(-1, "invalid number of samples for RGBPal");

    if (image->bits != 4 && image->bits != 8)
        return gs_throw(-1, "invalid number of bits for RGBPal");

    stride = image->width * (image->components + 2);

    samples = gs_alloc_bytes(mem, stride * image->height, "samples");
    if (!samples)
        return gs_throw(-1, "out of memory: samples");

    for (y = 0; y < image->height; y++)
    {
        src = image->samples + (image->stride * y);
        dst = samples + (stride * y);

        for (x = 0; x < image->width; x++)
        {
            if (tiff->extrasamples)
            {
                int c = getcomp(src, x * 2, image->bits);
                int a = getcomp(src, x * 2 + 1, image->bits);
                *dst++ = tiff->colormap[c + 0] >> 8;
                *dst++ = tiff->colormap[c + maxval] >> 8;
                *dst++ = tiff->colormap[c + maxval * 2] >> 8;
                *dst++ = a << (8 - image->bits);
            }
            else
            {
                int c = getcomp(src, x, image->bits);
                *dst++ = tiff->colormap[c + 0] >> 8;
                *dst++ = tiff->colormap[c + maxval] >> 8;
                *dst++ = tiff->colormap[c + maxval * 2] >> 8;
            }
        }
    }

    gs_free_object(mem, image->samples, "samples");

    image->bits = 8;
    image->stride = stride;
    image->samples = samples;

    return gs_okay;
}

static int
gslt_decode_tiff_strips(gs_memory_t *mem, gslt_tiff_t *tiff, gslt_image_t *image)
{
    int error;

    /* switch on compression to create a filter */
    /* feed each strip to the filter */
    /* read out the data and pack the samples into an gslt_image */

    /* type 32773 / packbits -- nothing special (same row-padding as PDF) */
    /* type 2 / ccitt rle -- no EOL, no RTC, rows are byte-aligned */
    /* type 3 and 4 / g3 and g4 -- each strip starts new section */
    /* type 5 / lzw -- each strip is handled separately */

    byte *wp;
    unsigned row;
    unsigned strip;
    unsigned i;

    if (!tiff->rowsperstrip || !tiff->stripoffsets || !tiff->rowsperstrip)
        return gs_throw(-1, "no image data in tiff; maybe it is tiled");

    if (tiff->planar != 1)
        return gs_throw(-1, "image data is not in chunky format");

    image->width = tiff->imagewidth;
    image->height = tiff->imagelength;
    image->components = tiff->samplesperpixel;
    image->bits = tiff->bitspersample;
    image->stride = (image->width * image->components * image->bits + 7) / 8;

    switch (tiff->photometric)
    {
    case 0: /* WhiteIsZero -- inverted */
        image->colorspace = GSLT_GRAY;
        break;
    case 1: /* BlackIsZero */
        image->colorspace = GSLT_GRAY;
        break;
    case 2: /* RGB */
        image->colorspace = GSLT_RGB;
        break;
    case 3: /* RGBPal */
        image->colorspace = GSLT_RGB;
        break;
    case 5: /* CMYK */
        image->colorspace = GSLT_CMYK;
        break;
    case 6: /* YCbCr */
        /* it's probably a jpeg ... we let jpeg convert to rgb */
        image->colorspace = GSLT_RGB;
        break;
    default:
        return gs_throw1(-1, "unknown photometric: %d", tiff->photometric);
    }

    switch (tiff->resolutionunit)
    {
    case 2:
        image->xres = tiff->xresolution;
        image->yres = tiff->yresolution;
    case 3:
        image->xres = tiff->xresolution * 2.54 + 0.5;
        image->yres = tiff->yresolution * 2.54 + 0.5;
    default:
        image->xres = 96;
        image->yres = 96;
    }

    image->samples = gs_alloc_bytes(mem, image->stride * image->height, "samples");
    if (!image->samples)
        return gs_throw(-1, "could not allocate image samples");

    memset(image->samples, 0x55, image->stride * image->height);

    wp = image->samples;

    strip = 0;
    for (row = 0; row < tiff->imagelength; row += tiff->rowsperstrip)
    {
        unsigned offset = tiff->stripoffsets[strip];
        unsigned rlen = tiff->stripbytecounts[strip];
        unsigned wlen = image->stride * tiff->rowsperstrip;
        byte *rp = tiff->bp + offset;

        if (wp + wlen > image->samples + image->stride * image->height)
            wlen = image->samples + image->stride * image->height - wp;

        if (rp + rlen > tiff->ep)
            return gs_throw(-1, "strip extends beyond the end of the file");

        /* the bits are in un-natural order */
        if (tiff->fillorder == 2)
            for (i = 0; i < rlen; i++)
                rp[i] = bitrev[rp[i]];

        switch (tiff->compression)
        {
        case 1:
            error = gslt_decode_tiff_uncompressed(mem, tiff, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 2:
            error = gslt_decode_tiff_fax(mem, tiff, 2, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 3:
            error = gslt_decode_tiff_fax(mem, tiff, 3, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 4:
            error = gslt_decode_tiff_fax(mem, tiff, 4, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 5:
            error = gslt_decode_tiff_lzw(mem, tiff, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 6:
            error = gs_throw(-1, "deprecated JPEG in TIFF compression not supported"); 
            break;
        case 7:
            error = gslt_decode_tiff_jpeg(mem, tiff, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 8:
            error = gslt_decode_tiff_flate(mem, tiff, rp, rp + rlen, wp, wp + wlen); 
            break;
        case 32773:
            error = gslt_decode_tiff_packbits(mem, tiff, rp, rp + rlen, wp, wp + wlen); 
            break;
        default:
            error = gs_throw1(-1, "unknown TIFF compression: %d", tiff->compression);
        }

        if (error)
            return gs_rethrow1(error, "could not decode strip %d", row / tiff->rowsperstrip);

        /* scramble the bits back into original order */
        if (tiff->fillorder == 2)
            for (i = 0; i < rlen; i++)
                rp[i] = bitrev[rp[i]];

        wp += image->stride * tiff->rowsperstrip;
        strip ++;
    }

    /* Predictor (only for LZW and Flate) */
    if ((tiff->compression == 5 || tiff->compression == 8) && tiff->predictor == 2)
    {
        byte *p = image->samples;
        for (i = 0; i < image->height; i++)
        {
            gslt_unpredict_tiff(p, image->width, tiff->samplesperpixel, image->bits);
            p += image->stride;
        }
    }

    /* RGBPal */
    if (tiff->photometric == 3 && tiff->colormap)
    {
        error = gslt_expand_colormap(mem, tiff, image);
        if (error)
            return gs_rethrow(error, "could not expand colormap");
    }

    /* WhiteIsZero .. invert */
    if (tiff->photometric == 0)
    {
        byte *p = image->samples;
        for (i = 0; i < image->height; i++)
        {
            gslt_invert_tiff(p, image->width, image->components, image->bits);
            p += image->stride;
        }
    }

    /* Premultiplied transparency */
    if (tiff->extrasamples == 1)
    {
        image->colorspace ++;
    }

    /* Non-pre-multiplied transparency */
    if (tiff->extrasamples == 2)
    {
        image->colorspace ++;
    }

    return gs_okay;
}

static void
gslt_read_tiff_tag_value(unsigned *p, gslt_tiff_t *tiff, unsigned type, unsigned ofs, unsigned n)
{
    tiff->rp = tiff->bp + ofs;
    if (tiff->rp > tiff->ep)
        tiff->rp = tiff->bp;

    while (n--)
    {
        switch (type)
        {
        case TRATIONAL:
            *p = readlong(tiff);
            *p = *p / readlong(tiff);
            p ++;
            break;
        case TBYTE: *p++ = readbyte(tiff); break;
        case TSHORT: *p++ = readshort(tiff); break;
        case TLONG: *p++ = readlong(tiff); break;
        default: *p++ = 0; break;
        }
    }
}

static int 
gslt_read_tiff_tag(gs_memory_t *mem, gslt_tiff_t *tiff, unsigned offset)
{
    unsigned tag;
    unsigned type;
    unsigned count;
    unsigned value;

    tiff->rp = tiff->bp + offset;

    tag = readshort(tiff);
    type = readshort(tiff);
    count = readlong(tiff);

    if ((type == TBYTE && count <= 4) ||
            (type == TSHORT && count <= 2) ||
            (type == TLONG && count <= 1))
        value = tiff->rp - tiff->bp;
    else
        value = readlong(tiff);

    switch (tag)
    {
    case NewSubfileType:
        gslt_read_tiff_tag_value(&tiff->subfiletype, tiff, type, value, 1);
        break;
    case ImageWidth:
        gslt_read_tiff_tag_value(&tiff->imagewidth, tiff, type, value, 1);
        break;
    case ImageLength:
        gslt_read_tiff_tag_value(&tiff->imagelength, tiff, type, value, 1);
        break;
    case BitsPerSample:
        gslt_read_tiff_tag_value(&tiff->bitspersample, tiff, type, value, 1);
        break;
    case Compression:
        gslt_read_tiff_tag_value(&tiff->compression, tiff, type, value, 1);
        break;
    case PhotometricInterpretation:
        gslt_read_tiff_tag_value(&tiff->photometric, tiff, type, value, 1);
        break;
    case FillOrder:
        gslt_read_tiff_tag_value(&tiff->fillorder, tiff, type, value, 1);
        break;
    case SamplesPerPixel:
        gslt_read_tiff_tag_value(&tiff->samplesperpixel, tiff, type, value, 1);
        break;
    case RowsPerStrip:
        gslt_read_tiff_tag_value(&tiff->rowsperstrip, tiff, type, value, 1);
        break;
    case XResolution:
        gslt_read_tiff_tag_value(&tiff->xresolution, tiff, type, value, 1);
        break;
    case YResolution:
        gslt_read_tiff_tag_value(&tiff->yresolution, tiff, type, value, 1);
        break;
    case PlanarConfiguration:
        gslt_read_tiff_tag_value(&tiff->planar, tiff, type, value, 1);
        break;
    case T4Options:
        gslt_read_tiff_tag_value(&tiff->g3opts, tiff, type, value, 1);
        break;
    case T6Options:
        gslt_read_tiff_tag_value(&tiff->g4opts, tiff, type, value, 1);
        break;
    case Predictor:
        gslt_read_tiff_tag_value(&tiff->predictor, tiff, type, value, 1);
        break;
    case ResolutionUnit:
        gslt_read_tiff_tag_value(&tiff->resolutionunit, tiff, type, value, 1);
        break;
    case YCbCrSubSampling:
        gslt_read_tiff_tag_value(tiff->ycbcrsubsamp, tiff, type, value, 2);
        break;
    case ExtraSamples:
        gslt_read_tiff_tag_value(&tiff->extrasamples, tiff, type, value, 1);
        break;

    case JPEGTables:
        tiff->jpegtables = tiff->bp + value;
        tiff->jpegtableslen = count;
        break;

    case StripOffsets:
        tiff->stripoffsets = (unsigned*) gs_alloc_bytes(mem, count * sizeof(unsigned), "StripOffsets");
        if (!tiff->stripoffsets)
            return gs_throw(-1, "could not allocate strip offsets");
        gslt_read_tiff_tag_value(tiff->stripoffsets, tiff, type, value, count);
        break;

    case StripByteCounts:
        tiff->stripbytecounts = (unsigned*) gs_alloc_bytes(mem, count * sizeof(unsigned), "StripByteCounts");
        if (!tiff->stripbytecounts)
            return gs_throw(-1, "could not allocate strip byte counts");
        gslt_read_tiff_tag_value(tiff->stripbytecounts, tiff, type, value, count);
        break;

    case ColorMap:
        tiff->colormap = (unsigned*) gs_alloc_bytes(mem, count * sizeof(unsigned), "ColorMap");
        if (!tiff->colormap)
            return gs_throw(-1, "could not allocate color map");
        gslt_read_tiff_tag_value(tiff->colormap, tiff, type, value, count);
        break;

    case TileWidth:
    case TileLength:
    case TileOffsets:
    case TileByteCounts:
        return gs_throw(-1, "tiled tiffs not supported");

    default:
        /*
           printf("unknown tag: %d t=%d n=%d\n", tag, type, count);
         */
        break;
    }

    return gs_okay;
}

gslt_image_t *
gslt_image_decode_tiff(gs_memory_t *mem, byte *buf, int len)
{
    gslt_image_t *image;
    int error;
    gslt_tiff_t tiffst;
    gslt_tiff_t *tiff = &tiffst;

    unsigned version;
    unsigned offset;
    unsigned count;
    unsigned i;

    memset(tiff, 0, sizeof(gslt_tiff_t));

    tiff->bp = buf;
    tiff->rp = buf;
    tiff->ep = buf + len;

    /* tag defaults, where applicable */
    tiff->bitspersample = 1;
    tiff->compression = 1;
    tiff->samplesperpixel = 1;
    tiff->resolutionunit = 2;
    tiff->rowsperstrip = 0xFFFFFFFF;
    tiff->fillorder = 1;
    tiff->planar = 1;
    tiff->subfiletype = 0;
    tiff->predictor = 1;
    tiff->ycbcrsubsamp[0] = 2;
    tiff->ycbcrsubsamp[1] = 2;

    /*
     * Read IFH
     */

    /* get byte order marker */
    tiff->order = TII;
    tiff->order = readshort(tiff);
    if (tiff->order != TII && tiff->order != TMM)
        gs_throw(-1, "not a TIFF file, wrong magic marker");

    /* check version */
    version = readshort(tiff);
    if (version != 42)
        gs_throw(-1, "not a TIFF file, wrong version marker");

    /* get offset of IFD */
    offset = readlong(tiff);

    /*
     * Read IFD
     */

    tiff->rp = tiff->bp + offset;

    count = readshort(tiff);

    offset += 2;
    for (i = 0; i < count; i++)
    {
        error = gslt_read_tiff_tag(mem, tiff, offset);
        if (error) {
            error = gs_rethrow(error, "could not read TIFF header tag");
	    return NULL;
	}
        offset += 12;
    }
#ifdef GSLT_DEBUG_TIFF
    gslt_debug_tiff(mem, tiff);
#endif
    /* new image */
    image = gs_alloc_struct_immovable(mem, gslt_image_t,
	&st_gslt_image, "new tiff gslt_image");
    if (image == NULL) {
	error = gs_throw(-1, "unable to allocate tiff gslt_image");
	return NULL;
    }
    image->samples = gs_alloc_bytes(mem, image->stride * image->height,
	"decoded tiff data");
    if (image->samples == NULL) {
	error = gs_throw(-1, "unable to allocate tiff gslt_image pixel data");
	gs_free_object(mem, image, "free tiff gslt_image");
	return NULL;
    }

    /*
     * Decode the image strips
     */

    if (tiff->rowsperstrip > tiff->imagelength)
        tiff->rowsperstrip = tiff->imagelength;

    error = gslt_decode_tiff_strips(mem, tiff, image);
    if (error) {
	error = gs_rethrow(error, "could not decode image data");
	return NULL;
    }

    /*
     * Clean up scratch memory
     */
    /* todo: we need to handle this on error conditions too */
    if (tiff->colormap) gs_free_object(mem, tiff->colormap, "ColorMap");
    if (tiff->stripoffsets) gs_free_object(mem, tiff->stripoffsets, "StripOffsets");
    if (tiff->stripbytecounts) gs_free_object(mem, tiff->stripbytecounts, "StripByteCounts");

    return image;
}

#ifdef GSLT_DEBUG_TIFF
static void
gslt_debug_tiff(gs_memory_t *mem, gslt_tiff_t *tiff)
{
    int n;

    dputs("TIFF <<\n");
    dprintf1("\t/NewSubfileType %u\n", tiff->subfiletype);
    dprintf1("\t/PhotometricInterpretation %u\n", tiff->photometric);
    dprintf1("\t/Compression %u\n", tiff->compression);
    dprintf1("\t/ImageWidth %u\n", tiff->imagewidth);
    dprintf1("\t/ImageLength %u\n", tiff->imagelength);
    dprintf1("\t/BitsPerSample %u\n", tiff->bitspersample);
    dprintf1("\t/SamplesPerPixel %u\n", tiff->samplesperpixel);
    dprintf1("\t/PlanarConfiguration %u\n", tiff->planar);
    dprintf1("\t/ExtraSamples %u\n", tiff->extrasamples);
    dprintf1("\t/ColorMap $%p\n", tiff->colormap);
    dprintf1("\t/XResolution %u\n", tiff->xresolution);
    dprintf1("\t/YResolution %u\n", tiff->yresolution);
    dprintf1("\t/ResolutionUnit %u\n", tiff->resolutionunit);
    dprintf1("\t/FillOrder %u\n", tiff->fillorder);
    dprintf1("\t/T4Options %u\n", tiff->g3opts);
    dprintf1("\t/T6Options %u\n", tiff->g4opts);
    dprintf1("\t/Predictor %u\n", tiff->predictor);
    dprintf1("\t/JPEGTables %u\n", tiff->jpegtableslen);
    dprintf2("\t/YCbCrSubSampling %u %u\n", tiff->ycbcrsubsamp[0], tiff->ycbcrsubsamp[1]);

    n = (tiff->imagelength + tiff->rowsperstrip - 1) / tiff->rowsperstrip;

    dprintf1("\t/RowsPerStrip %u\n", tiff->rowsperstrip);

    if (tiff->stripoffsets)
        dprintf1("\t/StripOffsets %u\n", n);

    if (tiff->stripbytecounts)
        dprintf1("\t/StripByteCounts %u\n", n);

    dputs(">>\n");
}
#endif
