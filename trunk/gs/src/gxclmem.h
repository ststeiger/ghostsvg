/* Copyright (C) 2001-2006 artofcode LLC.
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
/* Definitions and declarations for clist implementation in memory. */

#ifndef gxclmem_INCLUDED
#  define gxclmem_INCLUDED

#include "gxclio.h"		/* defines interface */
#include "strimpl.h"		/* stream structures      */

/*
 * The best values of MEMFILE_DATA_SIZE are slightly less than a power of 2,
 * to allow typical malloc implementations to allocate in units of a power
 * of 2 rather than having to go slightly over.
 */
#define MEMFILE_DATA_SIZE	(16384 - 160)

   /*   ============================================================    */
   /*                                                                   */
   /*   Memfile structure definitions.                                  */
   /*                                                                   */
   /*   The PHYS structures are the elements actually allocated in      */
   /*   RAM, containing the compressed data (or optionally raw data)    */
   /*                                                                   */
   /*   There can be several LOG (logical) elements per physical        */
   /*   element, depending on the compression. The MEMFILE pdata        */
   /*   item always points into a raw block of data.                    */
   /*                                                                   */
   /*   ============================================================    */

typedef struct RAW_BUFFER {
    struct RAW_BUFFER *fwd, *back;
    struct LOG_MEMFILE_BLK *log_blk;
    char data[MEMFILE_DATA_SIZE];
} RAW_BUFFER;

typedef struct PHYS_MEMFILE_BLK {
    struct PHYS_MEMFILE_BLK *link;
    char *data_limit;		/* end of data when compressed  */
    /* NULL if not compressed       */
    char data_spare[4];		/* used during de-compress      */
    char data[MEMFILE_DATA_SIZE];
} PHYS_MEMFILE_BLK;

typedef struct LOG_MEMFILE_BLK {
    struct LOG_MEMFILE_BLK *link;
    PHYS_MEMFILE_BLK *phys_blk;
    char *phys_pdata;
    RAW_BUFFER *raw_block;	/* or NULL */
} LOG_MEMFILE_BLK;

struct MEMFILE_s {
    gs_memory_t *memory;	/* storage allocator */
    gs_memory_t *data_memory;	/* storage allocator for data */
    bool ok_to_compress;	/* if true, OK to compress this file */
	/*
	 * Reserve memory blocks: these are used to guarantee that a
	 * given-sized write (or sequence of writes) will always succeed.
	 * More specifically, the guarantee is that N bytes can successfully
	 * be written after a low-memory warning is first returned by
	 * fwrite.  The reserve of N bytes for a given file is (re)allocated
	 * by a successful call to memfile_set_memory_warning(N).  Fwrite
	 * allocates memory only from the reserve when its normal allocation
	 * attempts fail; in such cases, it allocates blocks from the
	 * reserve pool as needed and completes normally, but returns a
	 * low-memory warning status. Once a low-memory warning has been
	 * returned, fwrite will continue to attempt to allocate memory from
	 * the usual allocator on subsequent fwrites, but does *not* try to
	 * "top up" the reserve if becomes available -- only an explicit
	 * memfile_set_memory_warning does so.
	 */
    PHYS_MEMFILE_BLK *reservePhysBlockChain;  /* chain of reserve phys blks */
    int reservePhysBlockCount; 	/* count of entries on reservePhysBlockChain */
    LOG_MEMFILE_BLK *reserveLogBlockChain;	/* chain of reserve log blks */
    int reserveLogBlockCount; 	/* count of entries on reserveLogBlockChain */
    /* logical file properties */
    LOG_MEMFILE_BLK *log_head;
    LOG_MEMFILE_BLK *log_curr_blk;
    int64_t log_length;		/* updated during write             */
    int64_t log_curr_pos;	/* updated during seek, close, read */
    char *pdata;		/* raw data */
    char *pdata_end;
    /* physical file properties */
    int64_t total_space;	/* so we know when to start compress */
    PHYS_MEMFILE_BLK *phys_curr;	/* NULL if not compressing      */
    RAW_BUFFER *raw_head, *raw_tail;
    int error_code;		/* used by CLIST_ferror         */
    stream_cursor_read rd;	/* use .ptr, .limit */
    stream_cursor_write wt;	/* use .ptr, .limit */
    bool compressor_initialized;
    stream_state *compress_state;
    stream_state *decompress_state;
};
#ifndef MEMFILE_DEFINED
#define MEMFILE_DEFINED
typedef struct MEMFILE_s MEMFILE;
#endif

/*
 * Only the MEMFILE and stream_state structures are GC-compatible, so we
 * allocate all the other structures on the C heap.
 */

#define private_st_MEMFILE()	/* in gxclmem.c */\
  gs_private_st_ptrs2(st_MEMFILE, MEMFILE, "MEMFILE",\
    MEMFILE_enum_ptrs, MEMFILE_reloc_ptrs, compress_state, decompress_state)

/* Declare the procedures for returning the prototype filter states */
/* for compressing and decompressing the band list. */
const stream_state *clist_compressor_state(void *);
const stream_state *clist_decompressor_state(void *);

#endif /* gxclmem_INCLUDED */
