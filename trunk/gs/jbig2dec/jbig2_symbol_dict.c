/*
    jbig2dec
    
    Copyright (C) 2001-2005 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id$
    
    symbol dictionary segment decode and support
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

#if defined(OUTPUT_PBM) || defined(DUMP_SYMDICT)
#include <stdio.h>
#include "jbig2_image.h"
#endif

/* Table 13 */
typedef struct {
  bool SDHUFF;
  bool SDREFAGG;
  int32_t SDNUMINSYMS;
  Jbig2SymbolDict *SDINSYMS;
  uint32_t SDNUMNEWSYMS;
  uint32_t SDNUMEXSYMS;
  /* SDHUFFDH */
  /* SDHUFFDW */
  /* SDHUFFBMSIZE */
  /* SDHUFFAGGINST */
  int SDTEMPLATE;
  int8_t sdat[8];
  bool SDRTEMPLATE;
  int8_t sdrat[4];
} Jbig2SymbolDictParams;


/* Utility routines */

#ifdef DUMP_SYMDICT
void
jbig2_dump_symbol_dict(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    Jbig2SymbolDict *dict = (Jbig2SymbolDict *)segment->result;
    int index;
    char filename[24];
    
    if (dict == NULL) return;
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "dumping symbol dict as %d individual png files\n", dict->n_symbols);
    for (index = 0; index < dict->n_symbols; index++) {
        snprintf(filename, sizeof(filename), "symbol_%04d.png", index);
#ifdef HAVE_LIBPNG
        jbig2_image_write_png_file(dict->glyphs[index], filename);
#else
        jbig2_image_write_pbm_file(dict->glyphs[index], filename);
#endif
    }
}
#endif /* DUMP_SYMDICT */

/* return a new empty symbol dict */
Jbig2SymbolDict *
jbig2_sd_new(Jbig2Ctx *ctx, int n_symbols)
{
   Jbig2SymbolDict *new = NULL;
   
   new = (Jbig2SymbolDict *)jbig2_alloc(ctx->allocator,
   				sizeof(Jbig2SymbolDict));
   if (new != NULL) {
     new->glyphs = (Jbig2Image **)jbig2_alloc(ctx->allocator,
     				n_symbols*sizeof(Jbig2Image*));
     new->n_symbols = n_symbols;
   } else {
     return NULL;
   }

   if (new->glyphs != NULL) {
     memset(new->glyphs, 0, n_symbols*sizeof(Jbig2Image*));
   } else {
     jbig2_free(ctx->allocator, new);
     return NULL;
   }
   
   return new;
}

/* release the memory associated with a symbol dict */
void
jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict)
{
   int i;
   
   if (dict == NULL) return;
   for (i = 0; i < dict->n_symbols; i++)
     if (dict->glyphs[i]) jbig2_image_release(ctx, dict->glyphs[i]);
   jbig2_free(ctx->allocator, dict->glyphs);
   jbig2_free(ctx->allocator, dict);
}

/* get a particular glyph by index */
Jbig2Image *
jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id)
{
   if (dict == NULL) return NULL;
   return dict->glyphs[id];
}

/* count the number of dictionary segments referred to by the given segment */
int
jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    int n_dicts = 0;
 
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) n_dicts++;
    }
                                                                                
    return (n_dicts);
}

/* return an array of pointers to symbol dictionaries referred to by the given segment */
Jbig2SymbolDict **
jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    Jbig2SymbolDict **dicts;
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    int dindex = 0;
                                                     
    dicts = jbig2_alloc(ctx->allocator, sizeof(Jbig2SymbolDict *) * n_dicts);
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) {
            /* add this referred to symbol dictionary */
            dicts[dindex++] = (Jbig2SymbolDict *)rsegment->result;
        }
    }
                                                                                
    if (dindex != n_dicts) {
        /* should never happen */
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "counted %d symbol dictionaries but build a list with %d.\n",
            n_dicts, dindex);
    }
                                                                                
    return (dicts);
}

/* generate a new symbol dictionary by concatenating a list of 
   existing dictionaries */
Jbig2SymbolDict *
jbig2_sd_cat(Jbig2Ctx *ctx, int n_dicts, Jbig2SymbolDict **dicts)
{
  int i,j,k, symbols;
  Jbig2SymbolDict *new = NULL;
  
  /* count the imported symbols and allocate a new array */
  symbols = 0;
  for(i = 0; i < n_dicts; i++)
    symbols += dicts[i]->n_symbols;

  /* fill a new array with cloned glyph pointers */
  new = jbig2_sd_new(ctx, symbols);
  if (new != NULL) {
    k = 0;
    for (i = 0; i < n_dicts; i++)
      for (j = 0; j < dicts[i]->n_symbols; j++)
        new->glyphs[k++] = jbig2_image_clone(ctx, dicts[i]->glyphs[j]);
  }
  
  return new;
}


/* Decoding routines */

/* 6.5 */
static Jbig2SymbolDict *
jbig2_decode_symbol_dict(Jbig2Ctx *ctx,
			 Jbig2Segment *segment,
			 const Jbig2SymbolDictParams *params,
			 const byte *data, size_t size,
			 Jbig2ArithCx *GB_stats,
			 Jbig2ArithCx *GR_stats)
{
  Jbig2SymbolDict *SDNEWSYMS;
  Jbig2SymbolDict *SDEXSYMS;
  int32_t HCHEIGHT;
  uint32_t NSYMSDECODED;
  int32_t SYMWIDTH, TOTWIDTH;
  uint32_t HCFIRSTSYM;
  Jbig2WordStream *ws = NULL;
  Jbig2ArithState *as = NULL;
  Jbig2ArithIntCtx *IADH = NULL;
  Jbig2ArithIntCtx *IADW = NULL;
  Jbig2ArithIntCtx *IAEX = NULL;
  Jbig2ArithIntCtx *IAAI = NULL;
  Jbig2ArithIaidCtx *IAID = NULL;
  Jbig2ArithIntCtx *IARDX = NULL;
  Jbig2ArithIntCtx *IARDY = NULL;
  int code = 0;

  /* 6.5.5 (3) */
  HCHEIGHT = 0;
  NSYMSDECODED = 0;

  if (!params->SDHUFF) {
      ws = jbig2_word_stream_buf_new(ctx, data, size);
      as = jbig2_arith_new(ctx, ws);
      IADH = jbig2_arith_int_ctx_new(ctx);
      IADW = jbig2_arith_int_ctx_new(ctx);
      IAEX = jbig2_arith_int_ctx_new(ctx);
      IAAI = jbig2_arith_int_ctx_new(ctx);
      if (params->SDREFAGG) {
	  int tmp = params->SDINSYMS->n_symbols + params->SDNUMNEWSYMS;
	  int SBSYMCODELEN;
	  for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < tmp; SBSYMCODELEN++);
	  IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
	  IARDX = jbig2_arith_int_ctx_new(ctx);
	  IARDY = jbig2_arith_int_ctx_new(ctx);
      }
  } else {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"NYI: huffman coded symbol dictionary");
      return NULL;
  }

  SDNEWSYMS = jbig2_sd_new(ctx, params->SDNUMNEWSYMS);

  /* 6.5.5 (4a) */
  while (NSYMSDECODED < params->SDNUMNEWSYMS) {
      int32_t HCDH, DW;

      /* 6.5.6 */
      if (params->SDHUFF) {
	; /* todo */
      } else {
	  code = jbig2_arith_int_decode(IADH, as, &HCDH);
      }

      if (code != 0) {
	jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	  "error or OOB decoding height class delta (%d)\n", code);
      }

      /* 6.5.5 (4b) */
      HCHEIGHT = HCHEIGHT + HCDH;
      SYMWIDTH = 0;
      TOTWIDTH = 0;
      HCFIRSTSYM = NSYMSDECODED;

      if (HCHEIGHT < 0) {
	  /* todo: mem cleanup */
	  code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			   "Invalid HCHEIGHT value");
          return NULL;
      }
#ifdef JBIG2_DEBUG
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "HCHEIGHT = %d", HCHEIGHT);
#endif        
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "decoding height class %d with %d syms decoded", HCHEIGHT, NSYMSDECODED);

      for (;;) {
	  /* check for broken symbol table */
 	  if (NSYMSDECODED > params->SDNUMNEWSYMS)
	    {
	      /* todo: mem cleanup? */
	      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	        "No OOB signalling end of height class %d", HCHEIGHT);
	      break;
	    }
	  /* 6.5.7 */
	  if (params->SDHUFF) {
	    ; /* todo */
	  } else {
	      code = jbig2_arith_int_decode(IADW, as, &DW);
	  }

	  /* 6.5.5 (4c.i) */
	  if (code == 1) {
	    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
	    " OOB signals end of height class %d", HCHEIGHT);
	    break;
	  }
	  SYMWIDTH = SYMWIDTH + DW;
	  TOTWIDTH = TOTWIDTH + SYMWIDTH;
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "  decoded symbol %d width %d (total width now %d)", NSYMSDECODED, SYMWIDTH, TOTWIDTH); 
	  if (SYMWIDTH < 0) {
	      /* todo: mem cleanup */
              code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Invalid SYMWIDTH value (%d) at symbol %d", SYMWIDTH, NSYMSDECODED+1);
              return NULL;
          }
#ifdef JBIG2_DEBUG
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "SYMWIDTH = %d", SYMWIDTH);
#endif
	  /* 6.5.5 (4c.ii) */
	  if (!params->SDHUFF || params->SDREFAGG) {
		jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
		  "SDHUFF = %d; SDREFAGG = %d", params->SDHUFF, params->SDREFAGG);
	      /* 6.5.8 */
	      if (!params->SDREFAGG) {
		  Jbig2GenericRegionParams region_params;
		  int sdat_bytes;
		  Jbig2Image *image;

		  /* Table 16 */
		  region_params.MMR = 0;
		  region_params.GBTEMPLATE = params->SDTEMPLATE;
		  region_params.TPGDON = 0;
		  region_params.USESKIP = 0;
		  sdat_bytes = params->SDTEMPLATE == 0 ? 8 : 2;
		  memcpy(region_params.gbat, params->sdat, sdat_bytes);

		  image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);

		  code = jbig2_decode_generic_region(ctx, segment, &region_params,
						     as, image, GB_stats);
		  /* todo: handle errors */
                  
                  SDNEWSYMS->glyphs[NSYMSDECODED] = image;

	      } else {
                  /* 6.5.8.2 refinement/aggregate symbol */
                  uint32_t REFAGGNINST;

		  if (params->SDHUFF) {
		      /* todo */
		  } else {
		      code = jbig2_arith_int_decode(IAAI, as, (int32_t*)&REFAGGNINST);
		  }
		  if (code || (int32_t)REFAGGNINST <= 0)
		      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			"invalid number of symbols or OOB in aggregate glyph");

		  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
		    "aggregate symbol coding (%d instances)", REFAGGNINST);

		  if (REFAGGNINST > 1) {
		      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			"aggregate coding with REFAGGNINST=%d", REFAGGNINST);
		      /* todo: multiple symbols are like a text region */
		  } else {
		      /* 6.5.8.2.2 */
		      bool SBHUFF = params->SDHUFF;
		      Jbig2RefinementRegionParams rparams;
		      Jbig2Image *image;
		      uint32_t ID;
		      int32_t RDX, RDY;
		      int ninsyms = params->SDINSYMS->n_symbols;

		      if (params->SDHUFF) {
			  /* todo */
		      } else {
			  code = jbig2_arith_iaid_decode(IAID, as, &ID);
		          code = jbig2_arith_int_decode(IARDX, as, &RDX);
		          code = jbig2_arith_int_decode(IARDY, as, &RDY);
		      }


		      if (ID >= ninsyms+NSYMSDECODED) {
			jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			  "refinement references unknown symbol %d", ID);
			return NULL;
		      }
   
		      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"symbol is a refinement of id %d with the refinement applied at (%d,%d)",
			ID, RDX, RDY);

		      image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);

		      /* Table 18 */
		      rparams.GRTEMPLATE = params->SDRTEMPLATE;
		      rparams.reference = (ninsyms >= ID) ? 
					params->SDINSYMS->glyphs[ID] :
					SDNEWSYMS->glyphs[ID-ninsyms];
		      rparams.DX = RDX;
		      rparams.DY = RDY;
		      rparams.TPGRON = 0;
		      memcpy(rparams.grat, params->sdrat, 4);
		      jbig2_decode_refinement_region(ctx, segment, 
		          &rparams, as, image, GR_stats);

		      SDNEWSYMS->glyphs[NSYMSDECODED] = image;

		  }
               }

#ifdef OUTPUT_PBM
		  {
		    char name[64];
		    FILE *out;
		    snprintf(name, 64, "sd.%04d.%04d.pbm", 
		             segment->number, NSYMSDECODED);
		    out = fopen(name, "wb");
                    jbig2_image_write_pbm(SDNEWSYMS->glyphs[NSYMSDECODED], out);
		    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"writing out glyph as '%s' ...", name);
		    fclose(out);
		  }
#endif

	  } else {
	      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "unhandled bitmap case!!!");
          }

	  /* 6.5.5 (4c.iii) */
	  if (params->SDHUFF && !params->SDREFAGG) {
	    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
              "NYI: parsing collective bitmaps!!!");
	  }
	
	  /* 6.5.5 (4c.iv) */
	  NSYMSDECODED = NSYMSDECODED + 1;

#ifdef JBIG2_DEBUG
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "%d of %d decoded", NSYMSDECODED, params->SDNUMNEWSYMS);
#endif

	}
     }

  jbig2_free(ctx->allocator, GB_stats);
  
  /* 6.5.10 */
  SDEXSYMS = jbig2_sd_new(ctx, params->SDNUMEXSYMS);
  {
    int i = 0;
    int j = 0;
    int k, m, exrunlength, exflag = 0;
    
    if (params->SDINSYMS != NULL)
      m = params->SDINSYMS->n_symbols;
    else
      m = 0;
    while (j < params->SDNUMEXSYMS) {
      if (params->SDHUFF)
      	/* FIXME: implement reading from huff table B.1 */
        exrunlength = params->SDNUMEXSYMS;
      else
        code = jbig2_arith_int_decode(IAEX, as, &exrunlength);
      for(k = 0; k < exrunlength; k++)
        if (exflag) {
          SDEXSYMS->glyphs[j++] = (i < m) ? 
            jbig2_image_clone(ctx, params->SDINSYMS->glyphs[i]) :
            jbig2_image_clone(ctx, SDNEWSYMS->glyphs[i-m]);
          i++;
        }
        exflag = !exflag;
    }
  }
  
  jbig2_sd_release(ctx, SDNEWSYMS);
  
  if (!params->SDHUFF) {
    jbig2_arith_int_ctx_free(ctx, IADH);
    jbig2_arith_int_ctx_free(ctx, IADW);
    jbig2_arith_int_ctx_free(ctx, IAEX);
    jbig2_arith_int_ctx_free(ctx, IAAI);
    if (params->SDREFAGG) {
      jbig2_arith_iaid_ctx_free(ctx, IAID);
      jbig2_arith_int_ctx_free(ctx, IARDX);
      jbig2_arith_int_ctx_free(ctx, IARDY);
    }
    jbig2_free(ctx->allocator, as);  
    jbig2_word_stream_buf_free(ctx, ws);
  }
  
  return SDEXSYMS;
}

/* 7.4.2 */
int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
			const byte *segment_data)
{
  Jbig2SymbolDictParams params;
  uint16_t flags;
  int sdat_bytes;
  int offset;
  Jbig2ArithCx *GB_stats = NULL;
  Jbig2ArithCx *GR_stats = NULL;
  
  if (segment->data_length < 10)
    goto too_short;

  /* 7.4.2.1.1 */
  flags = jbig2_get_int16(segment_data);
  params.SDHUFF = flags & 1;
  params.SDREFAGG = (flags >> 1) & 1;
  params.SDTEMPLATE = (flags >> 10) & 3;
  params.SDRTEMPLATE = (flags >> 12) & 1;

  if (params.SDHUFF) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "symbol dictionary uses the Huffman encoding variant (NYI)");
    return 0;
  }

  /* FIXME: there are quite a few of these conditions to check */
  /* maybe #ifdef CONFORMANCE and a separate routine */
  if(!params.SDHUFF && (flags & 0x000c)) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDH is not.");
  }
  if(!params.SDHUFF && (flags & 0x0030)) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDW is not.");
  }

  if (flags & 0x0080) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "bitmap coding context is used (NYI) symbol data likely to be garbage!");
  }

  /* 7.4.2.1.2 */
  sdat_bytes = params.SDHUFF ? 0 : params.SDTEMPLATE == 0 ? 8 : 2;
  memcpy(params.sdat, segment_data + 2, sdat_bytes);
  offset = 2 + sdat_bytes;

  /* 7.4.2.1.3 */
  if (params.SDREFAGG && !params.SDRTEMPLATE) {
      if (offset + 4 > segment->data_length)
	goto too_short;
      memcpy(params.sdrat, segment_data + offset, 4);
      offset += 4;
  } else {
      /* sdrat is meaningless if SDRTEMPLATE is 1, but set a value
         to avoid confusion if anybody looks */ 
      memset(params.sdrat, 0, 4);
  }

  if (offset + 8 > segment->data_length)
    goto too_short;

  /* 7.4.2.1.4 */
  params.SDNUMEXSYMS = jbig2_get_int32(segment_data + offset);
  /* 7.4.2.1.5 */
  params.SDNUMNEWSYMS = jbig2_get_int32(segment_data + offset + 4);
  offset += 8;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "symbol dictionary, flags=%04x, %d exported syms, %d new syms",
	      flags, params.SDNUMEXSYMS, params.SDNUMNEWSYMS);

  /* 7.4.2.2 (2) */
  {
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    Jbig2SymbolDict **dicts = NULL;
  
    params.SDINSYMS = NULL;	
    if (n_dicts > 0) {
      dicts = jbig2_sd_list_referred(ctx, segment);
      params.SDINSYMS = jbig2_sd_cat(ctx, n_dicts, dicts);
    }
    if (params.SDINSYMS != NULL) {
      params.SDNUMINSYMS = params.SDINSYMS->n_symbols;
    } else {
     params.SDNUMINSYMS = 0;
    }
  }
  
  /* 7.4.2.2 (4) */
  if (!params.SDHUFF) {
      int stats_size = params.SDTEMPLATE == 0 ? 65536 :
	params.SDTEMPLATE == 1 ? 8192 : 1024;
      GB_stats = jbig2_alloc(ctx->allocator, stats_size);
      memset(GB_stats, 0, stats_size);
      if (params.SDREFAGG) {
	stats_size = params.SDRTEMPLATE ? 1 << 10 : 1 << 13;
	GR_stats = jbig2_alloc(ctx->allocator, stats_size);
	memset(GR_stats, 0, stats_size);
      }
  }

  if (flags & 0x0100) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "segment marks bitmap coding context as retained (NYI)");
  }

  segment->result = (void *)jbig2_decode_symbol_dict(ctx, segment,
				  &params,
				  segment_data + offset,
				  segment->data_length - offset,
				  GB_stats, GR_stats);
#ifdef DUMP_SYMDICT
  if (segment->result) jbig2_dump_symbol_dict(ctx, segment);
#endif

  /* todo: retain or free GB_stats, GR_stats */

  return (segment->result != NULL) ? 0 : -1;

 too_short:
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		     "Segment too short");
}
