/*
    jbig2dec
    
    Copyright (C) 2002-2003 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id$
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_arith_iaid.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

typedef enum {
    JBIG2_CORNER_BOTTOMLEFT = 0,
    JBIG2_CORNER_TOPLEFT = 1,
    JBIG2_CORNER_BOTTOMRIGHT = 2,
    JBIG2_CORNER_TOPRIGHT = 3
} Jbig2RefCorner;

typedef struct {
    bool SBHUFF;
    bool SBREFINE;
    bool SBDEFPIXEL;
    Jbig2ComposeOp SBCOMBOP;
    bool TRANSPOSED;
    Jbig2RefCorner REFCORNER;
    int SBDSOFFSET;
    /* SBW */
    /* SBH */
    uint32_t SBNUMINSTANCES;
    int SBSTRIPS;
    /* SBNUMSYMS */
    int *SBSYMCODES;
    /* SBSYMCODELEN */
    /* SBSYMS */
    int SBHUFFFS;
    int SBHUFFDS;
    int SBHUFFDT;
    int SBHUFFRDW;
    int SBHUFFRDH;
    int SBHUFFRDX;
    int SBHUFFRDY;
    bool SBHUFFRSIZE;
    bool SBRTEMPLATE;
    int8_t sbrat[4];
} Jbig2TextRegionParams;


/**
 * jbig2_decode_text_region: decode a text region segment
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 segment (header) structure
 * @params: parameters from the text region header
 * @dicts: an array of referenced symbol dictionaries
 * @n_dicts: the number of referenced symbol dictionaries
 * @image: image structure in which to store the decoded region bitmap
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 *
 * Implements the text region decoding proceedure
 * described in section 6.4 of the JBIG2 spec.
 *
 * returns: 0 on success
 **/
static int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             const Jbig2TextRegionParams *params,
                             const Jbig2SymbolDict * const *dicts, const int n_dicts,
                             Jbig2Image *image,
                             const byte *data, const size_t size,
			     Jbig2ArithCx *GR_stats)
{
    /* relevent bits of 6.4.4 */
    uint32_t NINSTANCES;
    uint32_t ID;
    int32_t STRIPT;
    int32_t FIRSTS;
    int32_t DT;
    int32_t DFS;
    int32_t IDS;
    int32_t CURS;
    int32_t CURT;
    int S,T;
    int x,y;
    bool first_symbol;
    uint32_t index, max_id;
    Jbig2Image *IB;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    Jbig2ArithIntCtx *IADT = NULL;
    Jbig2ArithIntCtx *IAFS = NULL;
    Jbig2ArithIntCtx *IADS = NULL;
    Jbig2ArithIntCtx *IAIT = NULL;
    Jbig2ArithIaidCtx *IAID = NULL;
    Jbig2ArithIntCtx *IARI = NULL;
    Jbig2ArithIntCtx *IARDW = NULL;
    Jbig2ArithIntCtx *IARDH = NULL;
    Jbig2ArithIntCtx *IARDX = NULL;
    Jbig2ArithIntCtx *IARDY = NULL;
    int code = 0;
    int RI;
    
    max_id = 0;
    for (index = 0; index < n_dicts; index++) {
        max_id += dicts[index]->n_symbols;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "symbol list contains %d glyphs in %d dictionaries", max_id, n_dicts);
    
    if (!params->SBHUFF) {
	int SBSYMCODELEN;

	ws = jbig2_word_stream_buf_new(ctx, data, size);
        as = jbig2_arith_new(ctx, ws);
        IADT = jbig2_arith_int_ctx_new(ctx);
        IAFS = jbig2_arith_int_ctx_new(ctx);
        IADS = jbig2_arith_int_ctx_new(ctx);
        IAIT = jbig2_arith_int_ctx_new(ctx);
	/* Table 31 */
	for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < max_id; SBSYMCODELEN++);
        IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
	IARI = jbig2_arith_int_ctx_new(ctx);
	IARDW = jbig2_arith_int_ctx_new(ctx);
	IARDH = jbig2_arith_int_ctx_new(ctx);
	IARDX = jbig2_arith_int_ctx_new(ctx);
	IARDY = jbig2_arith_int_ctx_new(ctx);
    }
    
    /* 6.4.5 (1) */
    jbig2_image_clear(ctx, image, params->SBDEFPIXEL);
    
    /* 6.4.6 */
    if (params->SBHUFF) {
        /* todo */
    } else {
        code = jbig2_arith_int_decode(IADT, as, &STRIPT);
    }

    /* 6.4.5 (2) */
    STRIPT *= -(params->SBSTRIPS);
    FIRSTS = 0;
    NINSTANCES = 0;
    
    /* 6.4.5 (3) */
    while (NINSTANCES < params->SBNUMINSTANCES) {
        /* (3b) */
        if (params->SBHUFF) {
            /* todo */
        } else {
            code = jbig2_arith_int_decode(IADT, as, &DT);
        }
        DT *= params->SBSTRIPS;
        STRIPT += DT;
       
	first_symbol = TRUE;
	/* 6.4.5 (3c) - decode symbols in strip */
	for (;;) {
	    /* (3c.i) */
	    if (first_symbol) {
		/* 6.4.7 */
		if (params->SBHUFF) {
		    /* todo */
		} else {
		    code = jbig2_arith_int_decode(IAFS, as, &DFS);
		}
		FIRSTS += DFS;
		CURS = FIRSTS;
		first_symbol = FALSE;

	    } else {
		/* (3c.ii / 6.4.8) */
		if (params->SBHUFF) {
		    /* todo */
		} else {
		    code = jbig2_arith_int_decode(IADS, as, &IDS);
		}
		if (code) {
		    break;
		}
		CURS += IDS + params->SBDSOFFSET;
	    }

	    /* (3c.iii / 6.4.9) */
	    if (params->SBSTRIPS == 1) {
		CURT = 0;
	    } else if (params->SBHUFF) {
		/* todo */
	    } else {
		code = jbig2_arith_int_decode(IAIT, as, &CURT);
	    }
	    T = STRIPT + CURT;

	    /* (3b.iv / 6.4.10) decode the symbol id */
	    if (params->SBHUFF) {
		/* todo */
	    } else {
		code = jbig2_arith_iaid_decode(IAID, as, (int *)&ID);
	    }
	    if (ID >= max_id) {
		return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "symbol id out of range! (%d/%d)", ID, max_id);
	    }

	    /* (3c.v / 6.4.11) look up the symbol bitmap IB */
	    {
		uint32_t id = ID;

		index = 0;
		while (id >= dicts[index]->n_symbols)
		    id -= dicts[index++]->n_symbols;
		IB = jbig2_image_clone(ctx, dicts[index]->glyphs[id]);
	    }
	    if (params->SBREFINE) {
		code = jbig2_arith_int_decode(IARI, as, &RI);
	    } else {
		RI = 0;
	    }
	    if (RI) {
		Jbig2RefinementRegionParams rparams;
		Jbig2Image *IBO;
		int32_t RDW, RDH, RDX, RDY;
		Jbig2Image *image;

		/* (6.4.11 (1, 2, 3, 4)) */
		code = jbig2_arith_int_decode(IARDW, as, &RDW);
		code = jbig2_arith_int_decode(IARDH, as, &RDH);
		code = jbig2_arith_int_decode(IARDX, as, &RDX);
		code = jbig2_arith_int_decode(IARDY, as, &RDY);

		/* (6.4.11 (6)) */
		IBO = IB;
		image = jbig2_image_new(ctx, IBO->width + RDW,
					     IBO->height + RDH);

		/* Table 12 */
		rparams.GRTEMPLATE = params->SBRTEMPLATE;
		rparams.reference = IBO;
		rparams.DX = (RDW >> 1) + RDX;
		rparams.DY = (RDH >> 1) + RDY;
		rparams.TPGRON = 0;
		memcpy(rparams.grat, params->sbrat, 4);
		jbig2_decode_refinement_region(ctx, segment,
		    &rparams, as, image, GR_stats);
		IB = image;

		jbig2_image_release(ctx, IBO);
	    }
        
	    /* (3c.vi) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER > 1)) {
		CURS += IB->width - 1;
	    } else if ((params->TRANSPOSED) && !(params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }
        
	    /* (3c.vii) */
	    S = CURS;
        
	    /* (3c.viii) */
	    if (!params->TRANSPOSED) {
		switch (params->REFCORNER) {  /* FIXME: double check offsets */
		case JBIG2_CORNER_TOPLEFT: x = S; y = T; break;
		case JBIG2_CORNER_TOPRIGHT: x = S - IB->width + 1; y = T; break;
		case JBIG2_CORNER_BOTTOMLEFT: x = S; y = T - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: x = S - IB->width + 1; y = T - IB->height + 1; break;
		}
	    } else { /* TRANSPOSED */
		switch (params->REFCORNER) {
		case JBIG2_CORNER_TOPLEFT: y = T; x = T; break;
		case JBIG2_CORNER_TOPRIGHT: y = S - IB->width + 1; x = T; break;
		case JBIG2_CORNER_BOTTOMLEFT: y = S; x = T - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: y = S - IB->width + 1; x = T - IB->height + 1; break;
		}
	    }
        
	    /* (3c.ix) */
#ifdef JBIG2_DEBUG
	    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"composing glyph id %d: %dx%d @ (%d,%d) symbol %d/%d", 
			ID, IB->width, IB->height, x, y, NINSTANCES + 1,
			params->SBNUMINSTANCES);
#endif
	    jbig2_image_compose(ctx, image, IB, x, y, params->SBCOMBOP);
        
	    /* (3c.x) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER < 2)) {
		CURS += IB->width -1 ;
	    } else if ((params->TRANSPOSED) && (params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }
        
	    /* (3c.xi) */
	    NINSTANCES++;

	    jbig2_image_release(ctx, IB);
	}
        /* end strip */
    }
    /* 6.4.5 (4) */

    if (!params->SBHUFF) {
	jbig2_arith_int_ctx_free(ctx, IADT);
	jbig2_arith_int_ctx_free(ctx, IAFS);
	jbig2_arith_int_ctx_free(ctx, IADS);
	jbig2_arith_int_ctx_free(ctx, IAIT);
	jbig2_arith_iaid_ctx_free(ctx, IAID);
	jbig2_arith_int_ctx_free(ctx, IARI);
	jbig2_arith_int_ctx_free(ctx, IARDW);
	jbig2_arith_int_ctx_free(ctx, IARDH);
	jbig2_arith_int_ctx_free(ctx, IARDX);
	jbig2_arith_int_ctx_free(ctx, IARDY);
	jbig2_free(ctx->allocator, as);
	jbig2_word_stream_buf_free(ctx, ws);
    }
    
    return 0;
}

/**
 * jbig2_read_text_info: read a text region segment header
 **/
int
jbig2_parse_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2TextRegionParams params;
    Jbig2Image *image;
    Jbig2SymbolDict **dicts;
    int n_dicts;
    uint16_t flags;
    uint16_t huffman_flags = 0;
    Jbig2ArithCx *GR_stats = NULL;
    int code = 0;
    
    /* 7.4.1 */
    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;
    
    /* 7.4.3.1.1 */
    flags = jbig2_get_int16(segment_data + offset);
    offset += 2;
    
    params.SBHUFF = flags & 0x0001;
    params.SBREFINE = flags & 0x0002;
    params.SBSTRIPS = 1 << ((flags & 0x000c) >> 2);
    params.REFCORNER = (flags & 0x0030) >> 4;
    params.TRANSPOSED = flags & 0x0040;
    params.SBCOMBOP = (flags & 0x00e0) >> 7;
    params.SBDEFPIXEL = flags & 0x0100;
    params.SBDSOFFSET = (flags & 0x7C00) >> 10;
    params.SBRTEMPLATE = flags & 0x8000;
    
    if (params.SBHUFF)	/* Huffman coding */
      {
        /* 7.4.3.1.2 */
        huffman_flags = jbig2_get_int16(segment_data + offset);
        offset += 2;
        
        if (huffman_flags & 0x8000)
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "reserved bit 15 of text region huffman flags is not zero");
      }
    else	/* arithmetic coding */
      {
        /* 7.4.3.1.3 */
        if ((params.SBREFINE) && !(params.SBRTEMPLATE))
          {
            params.sbrat[0] = segment_data[offset];
            params.sbrat[1] = segment_data[offset + 1];
            params.sbrat[2] = segment_data[offset + 2];
            params.sbrat[3] = segment_data[offset + 3];
            offset += 4;
	  }
      }
    
    /* 7.4.3.1.4 */
    params.SBNUMINSTANCES = jbig2_get_int32(segment_data + offset);
    offset += 4;
    
    if (params.SBHUFF) {
        /* 7.4.3.1.5 */
        /* todo: symbol ID huffman decoding table decoding */
        
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "symbol id huffman table decoding NYI");
            
        /* 7.4.3.1.6 */
        params.SBHUFFFS = (huffman_flags & 0x0003);
        params.SBHUFFDS = (huffman_flags & 0x000c) >> 2;
        params.SBHUFFDT = (huffman_flags & 0x0030) >> 4;
        params.SBHUFFRDW = (huffman_flags & 0x00c0) >> 6;
        params.SBHUFFRDH = (huffman_flags & 0x0300) >> 8;
        params.SBHUFFRDX = (huffman_flags & 0x0c00) >> 10;
        params.SBHUFFRDY = (huffman_flags & 0x3000) >> 12;
        /* todo: conformance */
        params.SBHUFFRSIZE = huffman_flags & 0x8000;
        
        /* 7.4.3.1.7 */
        /* todo: symbol ID huffman table decoding */
    }

    /* 7.4.3.2 (3) */
    if (!params.SBHUFF && params.SBREFINE) {
	int stats_size = params.SBRTEMPLATE ? 1 << 10 : 1 << 13;
	GR_stats = jbig2_alloc(ctx->allocator, stats_size);
	memset(GR_stats, 0, stats_size);
    }

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "text region: %d x %d @ (%d,%d) %d symbols",
        region_info.width, region_info.height,
        region_info.x, region_info.y, params.SBNUMINSTANCES);
    
    /* compose the list of symbol dictionaries */
    n_dicts = jbig2_sd_count_referred(ctx, segment);
    if (n_dicts != 0) {
        dicts = jbig2_sd_list_referred(ctx, segment);
    } else {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "text region refers to no symbol dictionaries!");
    }
    if (dicts == NULL) {
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		"unable to retrive symbol dictionaries!"
		" previous parsing error?");
    } else {
	int index;
	if (dicts[0] == NULL) {
	    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, 
			segment->number,
                        "unable to find first referenced symbol dictionary!");
	}
	for (index = 1; index < n_dicts; index++)
	    if (dicts[index] == NULL) {
		jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
			"unable to find all referenced symbol dictionaries!");
	    n_dicts = index;
	}
    }

    image = jbig2_image_new(ctx, region_info.width, region_info.height);

    code = jbig2_decode_text_region(ctx, segment, &params,
                (const Jbig2SymbolDict * const *)dicts, n_dicts, image,
                segment_data + offset, segment->data_length - offset,
		GR_stats);

    if (!params.SBHUFF && params.SBREFINE) {
	jbig2_free(ctx->allocator, GR_stats);
    }

    jbig2_free(ctx->allocator, dicts);

    /* todo: check errors */

    if ((segment->flags & 63) == 4) {
        /* we have an intermediate region here. save it for later */
        segment->result = image;
    } else {
        /* otherwise composite onto the page */
	Jbig2Image *page_image = ctx->pages[ctx->current_page].image;
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, 
            "composing %dx%d decoded text region onto page at (%d, %d)",
            region_info.width, region_info.height, region_info.x, region_info.y);
        jbig2_image_compose(ctx, page_image, image, region_info.x, region_info.y, JBIG2_COMPOSE_OR);
        jbig2_image_release(ctx, image);
    }
    
    /* success */            
    return 0;
    
    too_short:
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Segment too short");
}
