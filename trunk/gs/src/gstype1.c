/* Copyright (C) 1990, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Adobe Type 1 charstring interpreter */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gxhintn.h"

#if NEW_TYPE1_HINTER
#   define OLD(a) DO_NOTHING
#   define NEW(a) a
#else
#   define OLD(a) a
#   define NEW(a) DO_NOTHING
#endif

/*
 * Define whether to always do Flex segments as curves.
 * This is only an issue because some old Adobe DPS fonts
 * seem to violate the Flex specification in a way that requires this.
 * We changed this from 1 to 0 in release 5.02: if it causes any
 * problems, we'll implement a more sophisticated test.
 */
#define ALWAYS_DO_FLEX_AS_CURVE 0

/* ------ Main interpreter ------ */

/*
 * Continue interpreting a Type 1 charstring.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is where the othersubr # is stored for callothersubr.
 */
int
gs_type1_interpret(gs_type1_state * pcis, const gs_glyph_data_t *pgd,
		   int *pindex)
{
    gs_font_type1 *pfont = pcis->pfont;
    gs_type1_data *pdata = &pfont->data;
#   if NEW_TYPE1_HINTER
    t1_hinter *h = &pcis->h;
#   endif
    bool encrypted = pdata->lenIV >= 0;
    gs_op1_state s;
    fixed cstack[ostack_size];

#define cs0 cstack[0]
#define ics0 fixed2int_var(cs0)
#define cs1 cstack[1]
#define ics1 fixed2int_var(cs1)
#define cs2 cstack[2]
#define ics2 fixed2int_var(cs2)
#define cs3 cstack[3]
#define ics3 fixed2int_var(cs3)
#define cs4 cstack[4]
#define ics4 fixed2int_var(cs4)
#define cs5 cstack[5]
#define ics5 fixed2int_var(cs5)
    cs_ptr csp;
#define clear CLEAR_CSTACK(cstack, csp)
    ip_state_t *ipsp = &pcis->ipstack[pcis->ips_count - 1];
    register const byte *cip;
    register crypt_state state;
    register int c;
    int code = 0;
    fixed ftx = pcis->origin.x, fty = pcis->origin.y;

    switch (pcis->init_done) {
	case -1:
	    NEW(t1_hinter__init(h, pcis->path));
	    break;
	case 0:
	    gs_type1_finish_init(pcis, &s);	/* sets sfc, ptx, pty, origin */
	    ftx = pcis->origin.x, fty = pcis->origin.y;
#	    if NEW_TYPE1_HINTER
            code = t1_hinter__set_mapping(h, &pcis->pis->ctm, &pfont->FontBBox, 
			    &pfont->FontMatrix, &pfont->base->FontMatrix,
			    pcis->scale.x.unit, pcis->scale.y.unit, 
			    pcis->origin.x, pcis->origin.y, 
			    gs_currentaligntopixels(pfont->dir));
	    if (code < 0)
	    	return code;
	    code = t1_hinter__set_font_data(h, 1, pdata, pcis->charpath_flag);
	    if (code < 0)
	    	return code;
#	    endif
	    break;
	default /*case 1 */ :
	    ptx = pcis->position.x;
	    pty = pcis->position.y;
	    sfc = pcis->fc;
    }
    sppath = pcis->path;
    s.pcis = pcis;
    INIT_CSTACK(cstack, csp, pcis);

    if (pgd == 0)
	goto cont;
    ipsp->cs_data = *pgd;
    cip = pgd->bits.data;
  call:state = crypt_charstring_seed;
    if (encrypted) {
	int skip = pdata->lenIV;

	/* Skip initial random bytes */
	for (; skip > 0; ++cip, --skip)
	    decrypt_skip_next(*cip, state);
    }
    goto top;
  cont:cip = ipsp->ip;
    state = ipsp->dstate;
  top:for (;;) {
	uint c0 = *cip++;

	charstring_next(c0, state, c, encrypted);
	if (c >= c_num1) {
	    /* This is a number, decode it and push it on the stack. */

	    if (c < c_pos2_0) {	/* 1-byte number */
		decode_push_num1(csp, cstack, c);
	    } else if (c < cx_num4) {	/* 2-byte number */
		decode_push_num2(csp, cstack, c, cip, state, encrypted);
	    } else if (c == cx_num4) {	/* 4-byte number */
		long lw;

		decode_num4(lw, cip, state, encrypted);
		CS_CHECK_PUSH(csp, cstack);
		*++csp = int2fixed(lw);
		if (lw != fixed2long(*csp)) {
		    /*
		     * We handle the only case we've ever seen that
		     * actually uses such large numbers specially.
		     */
		    long denom;

		    c0 = *cip++;
		    charstring_next(c0, state, c, encrypted);
		    if (c < c_num1)
			return_error(gs_error_rangecheck);
		    if (c < c_pos2_0)
			decode_num1(denom, c);
		    else if (c < cx_num4)
			decode_num2(denom, c, cip, state, encrypted);
		    else if (c == cx_num4)
			decode_num4(denom, cip, state, encrypted);
		    else
			return_error(gs_error_invalidfont);
		    c0 = *cip++;
		    charstring_next(c0, state, c, encrypted);
		    if (c != cx_escape)
			return_error(gs_error_rangecheck);
		    c0 = *cip++;
		    charstring_next(c0, state, c, encrypted);
		    if (c != ce1_div)
			return_error(gs_error_rangecheck);
		    *csp = float2fixed((double)lw / denom);
		}
	    } else		/* not possible */
		return_error(gs_error_invalidfont);
	  pushed:if_debug3('1', "[1]%d: (%d) %f\n",
		      (int)(csp - cstack), c, fixed2float(*csp));
	    continue;
	}
#ifdef DEBUG
	if (gs_debug['1']) {
	    static const char *const c1names[] =
	    {char1_command_names};

	    if (c1names[c] == 0)
		dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
	    else
		dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
			  c1names[c]);
	}
#endif
	switch ((char_command) c) {
#define cnext clear; goto top
#define inext goto top

		/* Commands with identical functions in Type 1 and Type 2, */
		/* except for 'escape'. */

	    case c_undef0:
	    case c_undef2:
	    case c_undef17:
		return_error(gs_error_invalidfont);
	    case c_callsubr:
		c = fixed2int_var(*csp) + pdata->subroutineNumberBias;
		code = pdata->procs.subr_data
		    (pfont, c, false, &ipsp[1].cs_data);
		if (code < 0)
		    return_error(code);
		--csp;
		ipsp->ip = cip, ipsp->dstate = state;
		++ipsp;
		cip = ipsp->cs_data.bits.data;
		goto call;
	    case c_return:
		gs_glyph_data_free(&ipsp->cs_data, "gs_type1_interpret");
		--ipsp;
		goto cont;
	    case c_undoc15:
		/* See gstype1.h for information on this opcode. */
		cnext;

		/* Commands with similar but not identical functions */
		/* in Type 1 and Type 2 charstrings. */

	    case cx_hstem:
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__hstem(h, cs0, cs1);
		if (code < 0)
		    return code;
#	    endif
		OLD(apply_path_hints(pcis, false));
		OLD(type1_hstem(pcis, cs0, cs1, true));
		cnext;
	    case cx_vstem:
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__vstem(h, cs0, cs1);
		if (code < 0)
		    return code;
#	    endif
		OLD(apply_path_hints(pcis, false));
		OLD(type1_vstem(pcis, cs0, cs1, true));
		cnext;
	    case cx_vmoveto:
		cs1 = cs0;
		cs0 = 0;
		accum_y(cs1);
	      move:		/* cs0 = dx, cs1 = dy for hint checking. */
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__rmoveto(h, cs0, cs1);
#	    else
		if ((pcis->hint_next != 0 || gx_path_is_drawing(sppath)) &&
		    pcis->flex_count == flex_max
		    )
		    apply_path_hints(pcis, true);
		code = gx_path_add_point(sppath, ptx, pty);
#	    endif
		goto cc;
	    case cx_rlineto:
		accum_xy(cs0, cs1);
	      line:		/* cs0 = dx, cs1 = dy for hint checking. */
                NEW(code = t1_hinter__rlineto(h, cs0, cs1));
		OLD(code = gx_path_add_line(sppath, ptx, pty));
	      cc:if (code < 0)
		    return code;
	      pp:if_debug2('1', "[1]pt=(%g,%g)\n",
			  fixed2float(ptx), fixed2float(pty));
		cnext;
	    case cx_hlineto:
		accum_x(cs0);
		cs1 = 0;
		goto line;
	    case cx_vlineto:
		cs1 = cs0;
		cs0 = 0;
		accum_y(cs1);
		goto line;
	    case cx_rrcurveto:
                NEW(code = t1_hinter__rcurveto(h, cs0, cs1, cs2, cs3, cs4, cs5));
		OLD(code = gs_op1_rrcurveto(&s, cs0, cs1, cs2, cs3, cs4, cs5));
		goto cc;
	    case cx_endchar:
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__endchar(h, (pcis->seac_accent >= 0));
		if (code < 0)
		    return code;
                if (pcis->seac_accent < 0)
                    code = t1_hinter__endglyph(h);
#	    endif
		code = gs_type1_endchar(pcis);
		if (code == 1) {
		    /* do accent of seac */
		    spt = pcis->position;
		    ipsp = &pcis->ipstack[pcis->ips_count - 1];
		    cip = ipsp->cs_data.bits.data;
		    goto call;
		}
		return code;
	    case cx_rmoveto:
		accum_xy(cs0, cs1);
		goto move;
	    case cx_hmoveto:
		accum_x(cs0);
		cs1 = 0;
		goto move;
	    case cx_vhcurveto:
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__rcurveto(h, 0, cs0, cs1, cs2, cs3, 0);
#	    else
		{
		    gs_fixed_point pt1, pt2, p;
		    fixed ax0, ay0;

		    code = gx_path_current_point(sppath, &p);
		    if (code < 0)
			return code;
		    ax0 = p.x - ptx;
		    ay0 = p.y - pty;
		    accum_y(cs0);
		    pt1.x = ptx + ax0, pt1.y = pty + ay0;
		    accum_xy(cs1, cs2);
		    pt2.x = ptx, pt2.y = pty;
		    accum_x(cs3);
		    code = gx_path_add_curve(sppath, pt1.x, pt1.y, pt2.x, pt2.y, ptx, pty);
		}
#	    endif
		goto cc;
	    case cx_hvcurveto:
#	    if NEW_TYPE1_HINTER
                code = t1_hinter__rcurveto(h, cs0, 0, cs1, cs2, 0, cs3);
#	    else
		{
		    gs_fixed_point pt1, pt2, p;
		    fixed ax0, ay0;

		    code = gx_path_current_point(sppath, &p);
		    if (code < 0)
			return code;
		    ax0 = p.x - ptx;
		    ay0 = p.y - pty;
		    accum_x(cs0);
		    pt1.x = ptx + ax0, pt1.y = pty + ay0;
		    accum_xy(cs1, cs2);
		    pt2.x = ptx, pt2.y = pty;
		    accum_y(cs3);
		    code = gx_path_add_curve(sppath, pt1.x, pt1.y, pt2.x, pt2.y, ptx, pty);
		}
#	    endif
		goto cc;

		/* Commands only recognized in Type 1 charstrings, */
		/* plus 'escape'. */

	    case c1_closepath:
                NEW(code = t1_hinter__closepath(h));
		OLD(code = gs_op1_closepath(&s));
		OLD(apply_path_hints(pcis, true));
		goto cc;
	    case c1_hsbw:
#	    if NEW_TYPE1_HINTER
                if (!h->seac_flag)
                    code = t1_hinter__sbw(h, cs0, fixed_0, cs1, fixed_0);
                else
                    code = t1_hinter__sbw_seac(h, pcis->adxy.x, pcis->adxy.y);
		if (code < 0)
		    return code;
#	    endif
		gs_type1_sbw(pcis, cs0, fixed_0, cs1, fixed_0);
		cs1 = fixed_0;
rsbw:		/* Give the caller the opportunity to intervene. */
		pcis->os_count = 0;	/* clear */
		ipsp->ip = cip, ipsp->dstate = state;
		pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
		/* If we aren't in a seac, do nothing else now; */
		/* finish_init will take care of the rest. */
		if (pcis->init_done < 0) {
		    /* Finish init when we return. */
		    pcis->init_done = 0;
		} else {
		    /*
		     * Accumulate the side bearing now, but don't do it
		     * a second time for the base character of a seac.
		     */
		    if (pcis->seac_accent >= 0) {
			/*
			 * As a special hack to work around a bug in
			 * Fontographer, we deal with the (illegal)
			 * situation in which the side bearing of the
			 * accented character (save_lsbx) is different from
			 * the side bearing of the base character (cs0/cs1).
			 */
			fixed dsbx = cs0 - pcis->save_lsb.x;
			fixed dsby = cs1 - pcis->save_lsb.y;

			if (dsbx | dsby) {
			    accum_xy(dsbx, dsby);
			    pcis->lsb.x += dsbx;
			    pcis->lsb.y += dsby;
			    pcis->save_adxy.x -= dsbx;
			    pcis->save_adxy.y -= dsby;
			}
		    } else
			accum_xy(pcis->lsb.x, pcis->lsb.y);
		    pcis->position.x = ptx;
		    pcis->position.y = pty;
		}
		return type1_result_sbw;
	    case cx_escape:
		charstring_next(*cip, state, c, encrypted);
		++cip;
#ifdef DEBUG
		if (gs_debug['1'] && c < char1_extended_command_count) {
		    static const char *const ce1names[] =
		    {char1_extended_command_names};

		    if (ce1names[c] == 0)
			dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
		    else
			dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
				  ce1names[c]);
		}
#endif
		switch ((char1_extended_command) c) {
		    case ce1_dotsection:
#		    if NEW_TYPE1_HINTER
                        code = t1_hinter__dotsection(h);
			if (code < 0)
			    return code;
#		    endif
			pcis->dotsection_flag ^=
			    (dotsection_in ^ dotsection_out);
			cnext;
		    case ce1_vstem3:
#		    if NEW_TYPE1_HINTER
                        code = t1_hinter__vstem3(h, cs0, cs1, cs2, cs3, cs4, cs5);
			if (code < 0)
			    return code;
#		    endif
			OLD(apply_path_hints(pcis, false));
			if (!pcis->vstem3_set && pcis->fh.use_x_hints) {
			    type1_center_vstem(pcis, pcis->lsb.x + cs2, cs3);
			    /* Adjust the current point */
			    /* (center_vstem handles everything else). */
			    ptx += pcis->vs_offset.x;
			    pty += pcis->vs_offset.y;
			    pcis->vstem3_set = true;
			}
			OLD(type1_vstem(pcis, cs0, cs1, true));
			OLD(type1_vstem(pcis, cs2, cs3, true));
			OLD(type1_vstem(pcis, cs4, cs5, true));
			cnext;
		    case ce1_hstem3:
#		    if NEW_TYPE1_HINTER
                        code = t1_hinter__hstem3(h, cs0, cs1, cs2, cs3, cs4, cs5);
			if (code < 0)
			    return code;
#		    else
			apply_path_hints(pcis, false);
			type1_hstem(pcis, cs0, cs1, true);
			type1_hstem(pcis, cs2, cs3, true);
			type1_hstem(pcis, cs4, cs5, true);
#		    endif
			cnext;
		    case ce1_seac:
			code = gs_type1_seac(pcis, cstack + 1, cstack[0],
					     ipsp);
			if (code != 0) {
			    *pindex = ics3;
			    return code;
			}
			clear;
			cip = ipsp->cs_data.bits.data;
			goto call;
		    case ce1_sbw:
#		    if NEW_TYPE1_HINTER
                        if (!h->seac_flag)
                            code = t1_hinter__sbw(h, cs0, cs1, cs2, cs3);
                        else
                            code = t1_hinter__sbw_seac(h, cs0 + pcis->adxy.x , cs1 + pcis->adxy.y);
			if (code < 0)
			    return code;
#		    endif
			gs_type1_sbw(pcis, cs0, cs1, cs2, cs3);
			goto rsbw;
		    case ce1_div:
			csp[-1] = float2fixed((double)csp[-1] / (double)*csp);
			--csp;
			goto pushed;
		    case ce1_undoc15:
			/* See gstype1.h for information on this opcode. */
			cnext;
		    case ce1_callothersubr:
			{
			    int num_results;

#define fpts pcis->flex_points
			    /* We must remember to pop both the othersubr # */
			    /* and the argument count off the stack. */
			    switch (*pindex = fixed2int_var(*csp)) {
				case 0:
				    {	
#					if !NEW_TYPE1_HINTER
					/* We have to do something really sleazy */
					/* here, namely, make it look as though */
					/* the rmovetos never really happened, */
					/* because we don't want to interrupt */
					/* the current subpath. */
					gs_fixed_point ept;
#					endif

#if defined(DEBUG) || !ALWAYS_DO_FLEX_AS_CURVE || NEW_TYPE1_HINTER
					fixed fheight = csp[-4];
#endif
#if (defined(DEBUG) || !ALWAYS_DO_FLEX_AS_CURVE) && !NEW_TYPE1_HINTER
					gs_fixed_point hpt;
#endif
#					if !NEW_TYPE1_HINTER
					if (pcis->flex_count != 8)
					    return_error(gs_error_invalidfont);
#					endif
					/* Assume the next two opcodes */
					/* are `pop' `pop'.  Unfortunately, some */
					/* Monotype fonts put these in a Subr, */
					/* so we can't just look ahead in the */
					/* opcode stream. */
					pcis->ignore_pops = 2;
					csp[-4] = csp[-3] - pcis->asb_diff;
					csp[-3] = csp[-2];
					csp -= 3;
#					if !NEW_TYPE1_HINTER
					code = gx_path_current_point(sppath, &ept);
					if (code < 0)
					    return code;
					gx_path_add_point(sppath, fpts[0].x, fpts[0].y);
					gx_path_set_state_flags(sppath, 
					    (byte)pcis->flex_path_state_flags); /* <--- sleaze */
#if defined(DEBUG) || !ALWAYS_DO_FLEX_AS_CURVE
					/* Decide whether to do the flex as a curve. */
					hpt.x = fpts[1].x - fpts[4].x;
					hpt.y = fpts[1].y - fpts[4].y;
					if_debug3('1',
					  "[1]flex: d=(%g,%g), height=%g\n",
						  fixed2float(hpt.x), fixed2float(hpt.y),
						fixed2float(fheight) / 100);
#endif
#if !ALWAYS_DO_FLEX_AS_CURVE	/* See beginning of file. */
					if (any_abs(hpt.x) + any_abs(hpt.y) <
					    fheight / 100
					    ) {		/* Do the flex as a line. */
					    code = gx_path_add_line(sppath,
							      ept.x, ept.y);
					} else
#endif
					{	/* Do the flex as a curve. */
					    code = gx_path_add_curve(sppath,
						       fpts[2].x, fpts[2].y,
						       fpts[3].x, fpts[3].y,
						      fpts[4].x, fpts[4].y);
					    if (code < 0)
						return code;
					    code = gx_path_add_curve(sppath,
						       fpts[5].x, fpts[5].y,
						       fpts[6].x, fpts[6].y,
						      fpts[7].x, fpts[7].y);
					}
#					else
					code = t1_hinter__flex_end(h, fheight);
#					endif
				    }
				    if (code < 0)
					return code;
				    pcis->flex_count = flex_max;	/* not inside flex */
				    inext;
				case 1:
#				    if !NEW_TYPE1_HINTER
				    code = gx_path_current_point(sppath, &fpts[0]);
				    if (code < 0)
					return code;
				    pcis->flex_path_state_flags =	/* <--- more sleaze */
					gx_path_get_state_flags(sppath);
#				    else
				    code = t1_hinter__flex_beg(h);
				    if (code < 0)
					return code;
#				    endif
				    pcis->flex_count = 1;
				    csp -= 2;
				    inext;
				case 2:
				    if (pcis->flex_count >= flex_max)
					return_error(gs_error_invalidfont);
				    OLD(code = gx_path_current_point(sppath,
						 &fpts[pcis->flex_count++]));
				    NEW(code = t1_hinter__flex_point(h));
				    if (code < 0)
					return code;
				    csp -= 2;
				    inext;
				case 3:
				    /* Assume the next opcode is a `pop'. */
				    /* See above as to why we don't just */
				    /* look ahead in the opcode stream. */
				    pcis->ignore_pops = 1;
#				    if NEW_TYPE1_HINTER
                                    code = t1_hinter__drop_hints(h);
				    if (code < 0)
					return code;
#				    endif
				    OLD(replace_stem_hints(pcis));
				    csp -= 2;
				    inext;
				case 12:
				case 13:
				    /* Counter control isn't implemented. */
				    cnext;
				case 14:
				    num_results = 1;
				  blend:
				    code = gs_type1_blend(pcis, csp,
							  num_results);
				    if (code < 0)
					return code;
				    csp -= code;
				    inext;
				case 15:
				    num_results = 2;
				    goto blend;
				case 16:
				    num_results = 3;
				    goto blend;
				case 17:
				    num_results = 4;
				    goto blend;
				case 18:
				    num_results = 6;
				    goto blend;
			    }
			}
#undef fpts
			/* Not a recognized othersubr, */
			/* let the client handle it. */
			{
			    int scount = csp - cstack;
			    int n;

			    /* Copy the arguments to the caller's stack. */
			    if (scount < 1 || csp[-1] < 0 ||
				csp[-1] > int2fixed(scount - 1)
				)
				return_error(gs_error_invalidfont);
			    n = fixed2int_var(csp[-1]);
			    code = (*pdata->procs.push_values)
				(pcis->callback_data, csp - (n + 1), n);
			    if (code < 0)
				return_error(code);
			    scount -= n + 1;
			    pcis->position.x = ptx;
			    pcis->position.y = pty;
			    apply_path_hints(pcis, false);
			    /* Exit to caller */
			    ipsp->ip = cip, ipsp->dstate = state;
			    pcis->os_count = scount;
			    pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
			    if (scount)
				memcpy(pcis->ostack, cstack, scount * sizeof(fixed));
			    return type1_result_callothersubr;
			}
		    case ce1_pop:
			/* Check whether we're ignoring the pops after */
			/* a known othersubr. */
			if (pcis->ignore_pops != 0) {
			    pcis->ignore_pops--;
			    inext;
			}
			CS_CHECK_PUSH(csp, cstack);
			++csp;
			code = (*pdata->procs.pop_value)
			    (pcis->callback_data, csp);
			if (code < 0)
			    return_error(code);
			goto pushed;
		    case ce1_setcurrentpoint:
			ptx = ftx + pcis->vs_offset.x; 
			pty = fty + pcis->vs_offset.y;
			cs0 += pcis->adxy.x;
			cs1 += pcis->adxy.y;
			accum_xy(cs0, cs1);
			goto pp;
		    default:
			return_error(gs_error_invalidfont);
		}
		/*NOTREACHED */

		/* Fill up the dispatch up to 32. */

	      case_c1_undefs:
	    default:		/* pacify compiler */
		return_error(gs_error_invalidfont);
	}
    }
}
