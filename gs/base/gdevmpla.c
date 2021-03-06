/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$ */
/* Any-depth planar "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gxdevice.h"
#include "gxdevmem.h"           /* semi-public definitions */
#include "gxgetbit.h"
#include "gdevmem.h"            /* private definitions */
#include "gdevmpla.h"           /* interface */

/* procedures */
static dev_proc_open_device(mem_planar_open);
declare_mem_procs(mem_planar_copy_mono, mem_planar_copy_color, mem_planar_fill_rectangle);
static dev_proc_copy_color(mem_planar_copy_color_24to8);
static dev_proc_copy_color(mem_planar_copy_color_4to1);
static dev_proc_copy_plane(mem_planar_copy_plane);
static dev_proc_strip_tile_rectangle(mem_planar_strip_tile_rectangle);
static dev_proc_get_bits_rectangle(mem_planar_get_bits_rectangle);

/*
 * Set up a planar memory device, after calling gs_make_mem_device but
 * before opening the device.  The pre-existing device provides the color
 * mapping procedures, but not the drawing procedures.  Requires: num_planes
 * > 0, plane_depths[0 ..  num_planes - 1] > 0, sum of plane depths =
 * mdev->color_info.depth.
 *
 * Note that this is the only public procedure in this file, and the only
 * sanctioned way to set up a planar memory device.
 */
int
gdev_mem_set_planar(gx_device_memory * mdev, int num_planes,
                    const gx_render_plane_t *planes /*[num_planes]*/)
{
    int total_depth;
    int same_depth = planes[0].depth;
    gx_color_index covered = 0;
    int pi;
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(mdev->color_info.depth);

    if (num_planes < 1 || num_planes > GX_DEVICE_COLOR_MAX_COMPONENTS)
        return_error(gs_error_rangecheck);
    for (pi = 0, total_depth = 0; pi < num_planes; ++pi) {
        int shift = planes[pi].shift;
        int plane_depth = planes[pi].depth;
        gx_color_index mask;

        if (shift < 0 || plane_depth > 16 ||
            !gdev_mem_device_for_bits(plane_depth))
            return_error(gs_error_rangecheck);
        mask = (((gx_color_index)1 << plane_depth) - 1) << shift;
        if (covered & mask)
            return_error(gs_error_rangecheck);
        covered |= mask;
        if (plane_depth != same_depth)
            same_depth = 0;
        total_depth += plane_depth;
    }
    if (total_depth > mdev->color_info.depth)
        return_error(gs_error_rangecheck);
    mdev->num_planes = num_planes;
    memcpy(mdev->planes, planes, num_planes * sizeof(planes[0]));
    mdev->plane_depth = same_depth;
    /* Change the drawing procedures. */
    set_dev_proc(mdev, open_device, mem_planar_open);
    if (num_planes == 1) {
        /* For 1 plane, just use a normal device */
        set_dev_proc(mdev, fill_rectangle, dev_proc(mdproto, fill_rectangle));
        set_dev_proc(mdev, copy_mono,  dev_proc(mdproto, copy_mono));
        set_dev_proc(mdev, copy_color, dev_proc(mdproto, copy_color));
        set_dev_proc(mdev, copy_alpha, dev_proc(mdproto, copy_alpha));
        set_dev_proc(mdev, strip_tile_rectangle, dev_proc(mdproto, strip_tile_rectangle));
        set_dev_proc(mdev, get_bits_rectangle, dev_proc(mdproto, get_bits_rectangle));
    } else {
        set_dev_proc(mdev, fill_rectangle, mem_planar_fill_rectangle);
        set_dev_proc(mdev, copy_mono, mem_planar_copy_mono);
        if ((mdev->color_info.depth == 24) &&
            (mdev->num_planes == 3) &&
            (mdev->planes[0].depth == 8) && (mdev->planes[0].shift == 16) &&
            (mdev->planes[1].depth == 8) && (mdev->planes[1].shift == 8) &&
            (mdev->planes[2].depth == 8) && (mdev->planes[2].shift == 0))
            set_dev_proc(mdev, copy_color, mem_planar_copy_color_24to8);
        else if ((mdev->color_info.depth == 4) &&
                 (mdev->num_planes == 4) &&
                 (mdev->planes[0].depth == 1) && (mdev->planes[0].shift == 3) &&
                 (mdev->planes[1].depth == 1) && (mdev->planes[1].shift == 2) &&
                 (mdev->planes[2].depth == 1) && (mdev->planes[2].shift == 1) &&
                 (mdev->planes[3].depth == 1) && (mdev->planes[3].shift == 0))
            set_dev_proc(mdev, copy_color, mem_planar_copy_color_4to1);
        else
            set_dev_proc(mdev, copy_color, mem_planar_copy_color);
        set_dev_proc(mdev, copy_alpha, gx_default_copy_alpha);
        set_dev_proc(mdev, copy_plane, mem_planar_copy_plane);
        set_dev_proc(mdev, strip_tile_rectangle, mem_planar_strip_tile_rectangle);
        set_dev_proc(mdev, get_bits_rectangle, mem_planar_get_bits_rectangle);
    }
    set_dev_proc(mdev, strip_copy_rop, dev_proc(mdproto, strip_copy_rop));
    return 0;
}

/* Open a planar memory device. */
static int
mem_planar_open(gx_device * dev)
{
    gx_device_memory *const mdev = (gx_device_memory *)dev;

    /* Check that we aren't trying to open a chunky device as planar. */
    if (mdev->num_planes == 0)
        return_error(gs_error_rangecheck);
    return gdev_mem_open_scan_lines(mdev, dev->height);
}

/*
 * We execute drawing operations by patching a few parameters in the
 * device structure and then calling the procedure appropriate to the
 * plane depth.
 */
typedef struct mem_save_params_s {
    int depth;                  /* color_info.depth */
    byte *base;
    byte **line_ptrs;
} mem_save_params_t;
#define MEM_SAVE_PARAMS(mdev, msp)\
  (msp.depth = mdev->color_info.depth,\
   msp.base = mdev->base,\
   msp.line_ptrs = mdev->line_ptrs)
/* Previous versions of MEM_SET_PARAMS calculated raster as
 * bitmap_raster(mdev->width * plane_depth), but this restricts us to
 * non interleaved frame buffers. */
#define MEM_SET_PARAMS(mdev, plane_depth)\
  (mdev->color_info.depth = plane_depth, /* maybe not needed */\
   mdev->base = mdev->line_ptrs[0],\
   mdev->raster = mdev->line_ptrs[1]-mdev->line_ptrs[0])
#define MEM_RESTORE_PARAMS(mdev, msp)\
  (mdev->color_info.depth = msp.depth,\
   mdev->base = msp.base,\
   mdev->line_ptrs = msp.line_ptrs)

/* Fill a rectangle with a color. */
static int
mem_planar_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                          gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    mem_save_params_t save;
    int pi;

    MEM_SAVE_PARAMS(mdev, save);
    for (pi = 0; pi < mdev->num_planes; ++pi) {
        int plane_depth = mdev->planes[pi].depth;
        gx_color_index mask = ((gx_color_index)1 << plane_depth) - 1;
        const gx_device_memory *mdproto =
            gdev_mem_device_for_bits(plane_depth);

        MEM_SET_PARAMS(mdev, plane_depth);
        dev_proc(mdproto, fill_rectangle)(dev, x, y, w, h,
                                          (color >> mdev->planes[pi].shift) &
                                          mask);
        mdev->line_ptrs += mdev->height;
    }
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
}

/* Copy a bitmap. */
static int
mem_planar_copy_mono(gx_device * dev, const byte * base, int sourcex,
                     int sraster, gx_bitmap_id id, int x, int y, int w, int h,
                     gx_color_index color0, gx_color_index color1)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    mem_save_params_t save;
    int pi;

    MEM_SAVE_PARAMS(mdev, save);
    for (pi = 0; pi < mdev->num_planes; ++pi) {
        int plane_depth = mdev->planes[pi].depth;
        int shift = mdev->planes[pi].shift;
        gx_color_index mask = ((gx_color_index)1 << plane_depth) - 1;
        const gx_device_memory *mdproto =
            gdev_mem_device_for_bits(plane_depth);
        gx_color_index c0 =
            (color0 == gx_no_color_index ? gx_no_color_index :
             (color0 >> shift) & mask);
        gx_color_index c1 =
            (color1 == gx_no_color_index ? gx_no_color_index :
             (color1 >> shift) & mask);

        MEM_SET_PARAMS(mdev, plane_depth);
        if (c0 == c1)
            dev_proc(mdproto, fill_rectangle)(dev, x, y, w, h, c0);
        else
            dev_proc(mdproto, copy_mono)
                (dev, base, sourcex, sraster, id, x, y, w, h, c0, c1);
        mdev->line_ptrs += mdev->height;
    }
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
}

/* Copy color: Special case the 24 -> 8+8+8 case. */
static int
mem_planar_copy_color_24to8(gx_device * dev, const byte * base, int sourcex,
                            int sraster, gx_bitmap_id id,
                            int x, int y, int w, int h)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
#define BUF_LONGS 100   /* arbitrary, >= 1 */
#define BUF_BYTES (BUF_LONGS * ARCH_SIZEOF_LONG)
    union b_ {
        ulong l[BUF_LONGS];
        byte b[BUF_BYTES];
    } buf, buf1, buf2;
    mem_save_params_t save;
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(8);
    uint plane_raster = bitmap_raster(w<<3);
    int br, bw, bh, cx, cy, cw, ch, ix, iy;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    MEM_SAVE_PARAMS(mdev, save);
    MEM_SET_PARAMS(mdev, 8);
    if (plane_raster > BUF_BYTES) {
        br = BUF_BYTES;
        bw = BUF_BYTES;
        bh = 1;
    } else {
        br = plane_raster;
        bw = w;
        bh = BUF_BYTES / plane_raster;
    }
    for (cy = y; cy < y + h; cy += ch) {
        ch = min(bh, y + h - cy);
        for (cx = x; cx < x + w; cx += cw) {
            int sx = sourcex + cx - x;
            const byte *source_base = base + sraster * (cy - y);

            cw = min(bw, x + w - cx);
            source_base += sx * 3;
            for (iy = 0; iy < ch; ++iy) {
                const byte *sptr = source_base;
                byte *dptr0 = buf.b  + br * iy;
                byte *dptr1 = buf1.b + br * iy;
                byte *dptr2 = buf2.b + br * iy;
                ix = cw;
                do {
                    /* Use the temporary variables below to free the C compiler
                     * to interleave load/stores for latencies sake despite the
                     * pointer aliasing rules. */
                    byte r = *sptr++;
                    byte g = *sptr++;
                    byte b = *sptr++;
                    *dptr0++ = r;
                    *dptr1++ = g;
                    *dptr2++ = b;
                } while (--ix);
                source_base += sraster;
            }
            dev_proc(mdproto, copy_color)
                        (dev, buf.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch);
            mdev->line_ptrs += mdev->height;
            dev_proc(mdproto, copy_color)
                    (dev, buf1.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch);
            mdev->line_ptrs += mdev->height;
            dev_proc(mdproto, copy_color)
                    (dev, buf2.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch);
            mdev->line_ptrs -= 2*mdev->height;
        }
    }
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
}

/* Copy color: Special case the 4 -> 1+1+1+1 case. */
static int
mem_planar_copy_color_4to1(gx_device * dev, const byte * base, int sourcex,
                            int sraster, gx_bitmap_id id,
                            int x, int y, int w, int h)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
#define BUF_LONGS 100   /* arbitrary, >= 1 */
#define BUF_BYTES (BUF_LONGS * ARCH_SIZEOF_LONG)
    union b_ {
        ulong l[BUF_LONGS];
        byte b[BUF_BYTES];
    } buf0, buf1, buf2, buf3;
    mem_save_params_t save;
    const gx_device_memory *mdproto = gdev_mem_device_for_bits(1);
    uint plane_raster = bitmap_raster(w);
    int br, bw, bh, cx, cy, cw, ch, ix, iy;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    MEM_SAVE_PARAMS(mdev, save);
    MEM_SET_PARAMS(mdev, 1);
    if (plane_raster > BUF_BYTES) {
        br = BUF_BYTES;
        bw = BUF_BYTES<<3;
        bh = 1;
    } else {
        br = plane_raster;
        bw = w;
        bh = BUF_BYTES / plane_raster;
    }
    for (cy = y; cy < y + h; cy += ch) {
        ch = min(bh, y + h - cy);
        for (cx = x; cx < x + w; cx += cw) {
            int sx = sourcex + cx - x;
            const byte *source_base = base + sraster * (cy - y) + (sx>>1);

            cw = min(bw, x + w - cx);
            if ((sx & 1) == 0) {
                for (iy = 0; iy < ch; ++iy) {
                    const byte *sptr = source_base;
                    byte *dptr0 = buf0.b + br * iy;
                    byte *dptr1 = buf1.b + br * iy;
                    byte *dptr2 = buf2.b + br * iy;
                    byte *dptr3 = buf3.b + br * iy;
                    byte roll = 0x80;
                    byte bc = 0;
                    byte bm = 0;
                    byte by = 0;
                    byte bk = 0;
                    ix = cw;
                    do {
                        byte b = *sptr++;
                        if (b & 0x80)
                            bc |= roll;
                        if (b & 0x40)
                            bm |= roll;
                        if (b & 0x20)
                            by |= roll;
                        if (b & 0x10)
                            bk |= roll;
                        roll >>= 1;
                        if (b & 0x08)
                            bc |= roll;
                        if (b & 0x04)
                            bm |= roll;
                        if (b & 0x02)
                            by |= roll;
                        if (b & 0x01)
                            bk |= roll;
                        roll >>= 1;
                        if (roll == 0) {
                            *dptr0++ = bc;
                            *dptr1++ = bm;
                            *dptr2++ = by;
                            *dptr3++ = bk;
                            bc = 0;
                            bm = 0;
                            by = 0;
                            bk = 0;
                            roll = 0x80;
                        }
                        ix -= 2;
                    } while (ix > 0);
                    if (roll != 0x80) {
                        *dptr0++ = bc;
                        *dptr1++ = bm;
                        *dptr2++ = by;
                        *dptr3++ = bk;
                    }
                    source_base += sraster;
                }
            } else {
                for (iy = 0; iy < ch; ++iy) {
                    const byte *sptr = source_base;
                    byte *dptr0 = buf0.b + br * iy;
                    byte *dptr1 = buf1.b + br * iy;
                    byte *dptr2 = buf2.b + br * iy;
                    byte *dptr3 = buf3.b + br * iy;
                    byte roll = 0x80;
                    byte bc = 0;
                    byte bm = 0;
                    byte by = 0;
                    byte bk = 0;
                    byte b = *sptr++;
                    ix = cw;
                    goto loop_entry;
                    do {
                        b = *sptr++;
                        if (b & 0x80)
                            bc |= roll;
                        if (b & 0x40)
                            bm |= roll;
                        if (b & 0x20)
                            by |= roll;
                        if (b & 0x10)
                            bk |= roll;
                        roll >>= 1;
                        if (roll == 0) {
                            *dptr0++ = bc;
                            *dptr1++ = bm;
                            *dptr2++ = by;
                            *dptr3++ = bk;
                            bc = 0;
                            bm = 0;
                            by = 0;
                            bk = 0;
                            roll = 0x80;
                        }
loop_entry:
                        if (b & 0x08)
                            bc |= roll;
                        if (b & 0x04)
                            bm |= roll;
                        if (b & 0x02)
                            by |= roll;
                        if (b & 0x01)
                            bk |= roll;
                        roll >>= 1;
                        ix -= 2;
                    } while (ix >= 0); /* ix == -2 means 1 extra done */
                    if ((ix == -2) && (roll == 0x40)) {
                        /* We did an extra one, and it was the last thing
                         * we did. Nothing to store. */
                    } else {
                        /* Flush the stored bytes */
                        *dptr0++ = bc;
                        *dptr1++ = bm;
                        *dptr2++ = by;
                        *dptr3++ = bk;
                    }
                    source_base += sraster;
                }
            }
            dev_proc(mdproto, copy_mono)
                        (dev, buf0.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch,
                         (gx_color_index)0, (gx_color_index)1);
            mdev->line_ptrs += mdev->height;
            dev_proc(mdproto, copy_mono)
                        (dev, buf1.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch,
                         (gx_color_index)0, (gx_color_index)1);
            mdev->line_ptrs += mdev->height;
            dev_proc(mdproto, copy_mono)
                        (dev, buf2.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch,
                         (gx_color_index)0, (gx_color_index)1);
            mdev->line_ptrs += mdev->height;
            dev_proc(mdproto, copy_mono)
                        (dev, buf3.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch,
                         (gx_color_index)0, (gx_color_index)1);
            mdev->line_ptrs -= 3*mdev->height;
        }
    }
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
}

/* Copy a color bitmap. */
/* This is slow and messy. */
static int
mem_planar_copy_color(gx_device * dev, const byte * base, int sourcex,
                      int sraster, gx_bitmap_id id,
                      int x, int y, int w, int h)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
#define BUF_LONGS 100   /* arbitrary, >= 1 */
#define BUF_BYTES (BUF_LONGS * ARCH_SIZEOF_LONG)
    union b_ {
        ulong l[BUF_LONGS];
        byte b[BUF_BYTES];
    } buf;
    int source_depth = dev->color_info.depth;
    mem_save_params_t save;
    int pi;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    MEM_SAVE_PARAMS(mdev, save);
    for (pi = 0; pi < mdev->num_planes; ++pi) {
        int plane_depth = mdev->planes[pi].depth;
        int shift = mdev->planes[pi].shift;
        gx_color_index mask = ((gx_color_index)1 << plane_depth) - 1;
        const gx_device_memory *mdproto =
            gdev_mem_device_for_bits(plane_depth);
        /*
         * Divide up the transfer into chunks that can be assembled
         * within the fixed-size buffer.  This code can be simplified
         * a lot if all planes have the same depth, by simply using
         * copy_color to transfer one column at a time, but it might
         * be very inefficient.
         */
        uint plane_raster = bitmap_raster(plane_depth * w);
        int br, bw, bh, cx, cy, cw, ch, ix, iy;

        MEM_SET_PARAMS(mdev, plane_depth);
        if (plane_raster > BUF_BYTES) {
            br = BUF_BYTES;
            bw = BUF_BYTES * 8 / plane_depth;
            bh = 1;
        } else {
            br = plane_raster;
            bw = w;
            bh = BUF_BYTES / plane_raster;
        }
        /*
         * We could do the extraction with get_bits_rectangle
         * selecting a single plane, but this is critical enough
         * code that we more or less replicate it here.
         */
        for (cy = y; cy < y + h; cy += ch) {
            ch = min(bh, y + h - cy);
            for (cx = x; cx < x + w; cx += cw) {
                int sx = sourcex + cx - x;
                const byte *source_base = base + sraster * (cy - y);
                int source_bit = 0;

                cw = min(bw, x + w - cx);
                if (sx) {
                    int xbit = sx * source_depth;

                    source_base += xbit >> 3;
                    source_bit = xbit & 7;
                }
                for (iy = 0; iy < ch; ++iy) {
                    sample_load_declare_setup(sptr, sbit, source_base,
                                              source_bit, source_depth);
                    sample_store_declare_setup(dptr, dbit, dbbyte,
                                               buf.b + br * iy,
                                               0, plane_depth);

                    for (ix = 0; ix < cw; ++ix) {
                        gx_color_index value;

                        sample_load_next_any(value, sptr, sbit, source_depth);
                        value = (value >> shift) & mask;
                        sample_store_next16(value, dptr, dbit, plane_depth,
                                            dbbyte);
                    }
                    sample_store_flush(dptr, dbit, plane_depth, dbbyte);
                    source_base += sraster;
                }
                /*
                 * Detect and bypass the possibility that copy_color is
                 * defined in terms of copy_mono.
                 */
                if (plane_depth == 1)
                    dev_proc(mdproto, copy_mono)
                        (dev, buf.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch,
                         (gx_color_index)0, (gx_color_index)1);
                else
                    dev_proc(mdproto, copy_color)
                        (dev, buf.b, 0, br, gx_no_bitmap_id, cx, cy, cw, ch);
            }
        }
        mdev->line_ptrs += mdev->height;
    }
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
#undef BUF_BYTES
#undef BUF_LONGS
}

/* Copy a given bitmap into a bitmap. */
static int
mem_planar_copy_plane(gx_device * dev, const byte * base, int sourcex,
                      int sraster, gx_bitmap_id id,
                      int x, int y, int w, int h, int plane)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int plane_depth;
    mem_save_params_t save;
    const gx_device_memory *mdproto;

    if ((plane < 0) || (plane >= mdev->num_planes))
        return gs_error_rangecheck;
    MEM_SAVE_PARAMS(mdev, save);
    plane_depth = mdev->planes[plane].depth;
    mdproto = gdev_mem_device_for_bits(plane_depth);
    if (plane_depth == 1)
        dev_proc(mdproto, copy_mono)(dev, base, sourcex, sraster, id,
                                     x, y, w, h,
                                     (gx_color_index)0, (gx_color_index)1);
    else
        dev_proc(mdproto, copy_color)(dev, base, sourcex, sraster,
                                      id, x, y, w, h);
    MEM_RESTORE_PARAMS(mdev, save);
    return 0;
}

static int
mem_planar_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
                                int x, int y, int w, int h,
                                gx_color_index color0, gx_color_index color1,
                                int px, int py)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    mem_save_params_t save;
    int pi;

    /* We can't split up the transfer if the tile is colored. */
    if (color0 == gx_no_color_index && color1 == gx_no_color_index)
        return gx_default_strip_tile_rectangle
            (dev, tiles, x, y, w, h, color0, color1, px, py);
    MEM_SAVE_PARAMS(mdev, save);
    for (pi = 0; pi < mdev->num_planes; ++pi) {
        int plane_depth = mdev->planes[pi].depth;
        int shift = mdev->planes[pi].shift;
        gx_color_index mask = ((gx_color_index)1 << plane_depth) - 1;
        const gx_device_memory *mdproto =
            gdev_mem_device_for_bits(plane_depth);
        gx_color_index c0 =
            (color0 == gx_no_color_index ? gx_no_color_index :
             (color0 >> shift) & mask);
        gx_color_index c1 =
            (color1 == gx_no_color_index ? gx_no_color_index :
             (color1 >> shift) & mask);

        MEM_SET_PARAMS(mdev, plane_depth);
        if (c0 == c1)
            dev_proc(mdproto, fill_rectangle)(dev, x, y, w, h, c0);
        else {
            /*
             * Temporarily replace copy_mono in case strip_tile_rectangle is
             * defined in terms of it.
             */
            set_dev_proc(dev, copy_mono, dev_proc(mdproto, copy_mono));
            dev_proc(mdproto, strip_tile_rectangle)
                (dev, tiles, x, y, w, h, c0, c1, px, py);
        }
        mdev->line_ptrs += mdev->height;
    }
    MEM_RESTORE_PARAMS(mdev, save);
    set_dev_proc(dev, copy_mono, mem_planar_copy_mono);
    return 0;
}

/*
 * Repack planar into chunky format.  This is an internal procedure that
 * implements the straightforward chunky case of get_bits_rectangle, and
 * is also used for the general cases.
 */
static int
planar_to_chunky(gx_device_memory *mdev, int x, int y, int w, int h,
                 int offset, uint draster, byte *dest)
{
    int num_planes = mdev->num_planes;
    sample_load_declare(sptr[GX_DEVICE_COLOR_MAX_COMPONENTS],
                        sbit[GX_DEVICE_COLOR_MAX_COMPONENTS]);
    sample_store_declare(dptr, dbit, dbbyte);
    int ddepth = mdev->color_info.depth;
    int direct =
        (mdev->color_info.depth != num_planes * mdev->plane_depth ? 0 :
         mdev->planes[0].shift == 0 ? -mdev->plane_depth : mdev->plane_depth);
    int pi, ix, iy;

    /* Check whether the planes are of equal size and sequential. */
    /* If direct != 0, we already know they exactly fill the depth. */
    if (direct < 0) {
        for (pi = 0; pi < num_planes; ++pi)
            if (mdev->planes[pi].shift != pi * -direct) {
                direct = 0; break;
            }
    } else if (direct > 0) {
        for (pi = 0; pi < num_planes; ++pi)
            if (mdev->planes[num_planes - 1 - pi].shift != pi * direct) {
                direct = 0; break;
            }
    }
    for (iy = y; iy < y + h; ++iy) {
        byte **line_ptr = mdev->line_ptrs + iy;

        for (pi = 0; pi < num_planes; ++pi, line_ptr += mdev->height) {
            int plane_depth = mdev->planes[pi].depth;
            int xbit = x * plane_depth;

            sptr[pi] = *line_ptr + (xbit >> 3);
            sample_load_setup(sbit[pi], xbit & 7, plane_depth);
        }
        {
            int xbit = offset * ddepth;

            dptr = dest + (iy - y) * draster + (xbit >> 3);
            sample_store_setup(dbit, xbit & 7, ddepth);
        }
        if (direct == -8) {
            /* 1 byte per component, lsb first. */
            switch (num_planes) {
            case 3: {
                const byte *p0 = sptr[2];
                const byte *p1 = sptr[1];
                const byte *p2 = sptr[0];

                for (ix = w; ix > 0; --ix, dptr += 3) {
                    dptr[0] = *p0++;
                    dptr[1] = *p1++;
                    dptr[2] = *p2++;
                }
            }
            continue;
            case 4:
                for (ix = w; ix > 0; --ix, dptr += 4) {
                    dptr[0] = *sptr[3]++;
                    dptr[1] = *sptr[2]++;
                    dptr[2] = *sptr[1]++;
                    dptr[3] = *sptr[0]++;
                }
                continue;
            default:
                break;
            }
        }
        sample_store_preload(dbbyte, dptr, dbit, ddepth);
        for (ix = w; ix > 0; --ix) {
            gx_color_index color = 0;

            for (pi = 0; pi < num_planes; ++pi) {
                int plane_depth = mdev->planes[pi].depth;
                uint value;

                sample_load_next16(value, sptr[pi], sbit[pi], plane_depth);
                color |= (gx_color_index)value << mdev->planes[pi].shift;
            }
            sample_store_next_any(color, dptr, dbit, ddepth, dbbyte);
        }
        sample_store_flush(dptr, dbit, ddepth, dbbyte);
    }
    return 0;
}

/* Copy bits back from a planar memory device. */
static int
mem_planar_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
                              gs_get_bits_params_t * params,
                              gs_int_rect ** unread)
{
    /* This duplicates most of mem_get_bits_rectangle.  Tant pis. */
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gs_get_bits_options_t options = params->options;
    int x = prect->p.x, w = prect->q.x - x, y = prect->p.y, h = prect->q.y - y;
    int num_planes = mdev->num_planes;
    gs_get_bits_params_t copy_params;
    int code;

    if (options == 0) {
        /*
         * Unfortunately, as things stand, we have to support
         * GB_PACKING_CHUNKY.  In fact, we can't even claim to support
         * GB_PACKING_PLANAR, because there is currently no way to
         * describe the particular planar packing format that the device
         * actually stores.
         */
        params->options =
            (GB_ALIGN_STANDARD | GB_ALIGN_ANY) |
            (GB_RETURN_COPY | GB_RETURN_POINTER) |
            (GB_OFFSET_0 | GB_OFFSET_SPECIFIED | GB_OFFSET_ANY) |
            (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED | GB_RASTER_ANY) |
            /*
            (mdev->num_planes == mdev->color_info.depth ?
             GB_PACKING_CHUNKY | GB_PACKING_PLANAR | GB_PACKING_BIT_PLANAR :
             GB_PACKING_CHUNKY | GB_PACKING_PLANAR)
            */
            GB_PACKING_CHUNKY |
            GB_COLORS_NATIVE | GB_ALPHA_NONE;
        return_error(gs_error_rangecheck);
    }
    if ((w <= 0) | (h <= 0)) {
        if ((w | h) < 0)
            return_error(gs_error_rangecheck);
        return 0;
    }
    if (x < 0 || w > dev->width - x ||
        y < 0 || h > dev->height - y
        )
        return_error(gs_error_rangecheck);

    /* First off, see if we can satisfy get_bits_rectangle with just returning
     * pointers to the existing data. */
    {
        gs_get_bits_params_t copy_params;
        byte *base = scan_line_base(mdev, y);
        int code;

        copy_params.options =
            GB_COLORS_NATIVE | GB_PACKING_PLANAR | GB_ALPHA_NONE |
            (mdev->raster ==
             bitmap_raster(mdev->width * mdev->color_info.depth) ?
             GB_RASTER_STANDARD : GB_RASTER_SPECIFIED);
        copy_params.raster = mdev->raster;
        code = gx_get_bits_return_pointer(dev, x, h, params,
                                          &copy_params, base);
        if (code >= 0)
            return code;
    }

    /*
     * If the request is for exactly one plane, hand it off to a device
     * temporarily tweaked to return just that plane.
     */
    if (!(~options & (GB_PACKING_PLANAR | GB_SELECT_PLANES))) {
        /* Check that only a single plane is being requested. */
        int pi;

        for (pi = 0; pi < num_planes; ++pi)
            if (params->data[pi] != 0)
                break;
        if (pi < num_planes) {
            int plane = pi++;

            for (; pi < num_planes; ++pi)
                if (params->data[pi] != 0)
                    break;
            if (pi == num_planes) {
                mem_save_params_t save;

                copy_params = *params;
                copy_params.options =
                    (options & ~(GB_PACKING_ALL | GB_SELECT_PLANES)) |
                    GB_PACKING_CHUNKY;
                copy_params.data[0] = copy_params.data[plane];
                MEM_SAVE_PARAMS(mdev, save);
                mdev->line_ptrs += mdev->height * plane;
                MEM_SET_PARAMS(mdev, mdev->planes[plane].depth);
                code = mem_get_bits_rectangle(dev, prect, &copy_params,
                                              unread);
                MEM_RESTORE_PARAMS(mdev, save);
                if (code >= 0) {
                    params->data[plane] = copy_params.data[0];
                    return code;
                }
            }
        }
    }
    /*
     * We can't return the requested plane by itself.  Fall back to
     * chunky format.  This is somewhat painful.
     *
     * The code here knows how to produce just one chunky format:
     * GB_COLORS_NATIVE, GB_ALPHA_NONE, GB_RETURN_COPY.
     * For any other format, we generate this one in a buffer and
     * hand it off to gx_get_bits_copy.  This is *really* painful.
     */
    if (!(~options & (GB_COLORS_NATIVE | GB_ALPHA_NONE |
                      GB_PACKING_CHUNKY | GB_RETURN_COPY))) {
        int offset = (options & GB_OFFSET_SPECIFIED ? params->x_offset : 0);
        uint draster =
            (options & GB_RASTER_SPECIFIED ? params->raster :
             bitmap_raster((offset + w) * mdev->color_info.depth));

        planar_to_chunky(mdev, x, y, w, h, offset, draster, params->data[0]);
    } else {
        /*
         * Do the transfer through an intermediate buffer.
         * The buffer must be large enough to hold at least one pixel,
         * i.e., GX_DEVICE_COLOR_MAX_COMPONENTS 16-bit values.
         * The algorithms are very similar to those in copy_color.
         */
#define BUF_LONGS\
  max(100, (GX_DEVICE_COLOR_MAX_COMPONENTS * 2 + sizeof(long) - 1) /\
      sizeof(long))
#define BUF_BYTES (BUF_LONGS * ARCH_SIZEOF_LONG)
        union b_ {
            ulong l[BUF_LONGS];
            byte b[BUF_BYTES];
        } buf;
        int br, bw, bh, cx, cy, cw, ch;
        int ddepth = mdev->color_info.depth;
        uint raster = bitmap_raster(ddepth * mdev->width);
        gs_get_bits_params_t dest_params;
        int dest_bytes;

        if (raster > BUF_BYTES) {
            br = BUF_BYTES;
            bw = BUF_BYTES * 8 / ddepth;
            bh = 1;
        } else {
            br = raster;
            bw = w;
            bh = BUF_BYTES / raster;
        }
        copy_params.options =
            GB_COLORS_NATIVE | GB_PACKING_CHUNKY | GB_ALPHA_NONE |
            GB_RASTER_STANDARD;
        copy_params.raster = raster;
        /* The options passed in from above may have GB_OFFSET_0, and what's
         * more, the code below may insist on GB_OFFSET_0 being set. Hence we
         * can't rely on x_offset to allow for the block size we are using.
         * We'll have to adjust the pointer by steam. */
        dest_params = *params;
        dest_params.x_offset = params->x_offset;
        if (options & GB_COLORS_RGB)
            dest_bytes = 3;
        else if (options & GB_COLORS_CMYK)
            dest_bytes = 4;
        else if (options & GB_COLORS_GRAY)
            dest_bytes = 1;
        else
            dest_bytes = mdev->color_info.depth / mdev->plane_depth;
        /* We assume options & GB_DEPTH_8 */
        for (cy = y; cy < y + h; cy += ch) {
            ch = min(bh, y + h - cy);
            for (cx = x; cx < x + w; cx += cw) {
                cw = min(bw, x + w - cx);
                planar_to_chunky(mdev, cx, cy, cw, ch, 0, br, buf.b);
                code = gx_get_bits_copy(dev, 0, cw, ch, &dest_params,
                                        &copy_params, buf.b, br);
                if (code < 0)
                    return code;
                dest_params.data[0] += cw * dest_bytes;
            }
            dest_params.data[0] += ch * dest_params.raster - (w*dest_bytes);
        }
#undef BUF_BYTES
#undef BUF_LONGS
    }
    return 0;
}
