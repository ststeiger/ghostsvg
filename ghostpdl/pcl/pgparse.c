/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pgparse.c */
/* HP-GL/2 parser */
#include "math_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "scommon.h"
#include "pgmand.h"

/* ---------------- Command definition ---------------- */

/* Register a command.  Return true if this is a redefinition. */
static bool
hpgl_register_command(hpgl_parser_state_t *pgl_parser_state,
		      byte *pindex, 
		      const hpgl_command_definition_t *pcmd)
{
    int index = pgl_parser_state->hpgl_command_next_index;
    byte prev = *pindex;

    if ( prev != 0 && prev <= index && 
	 pgl_parser_state->hpgl_command_list[prev] == pcmd )
	index = prev;
    else if ( index != 0 && pgl_parser_state->hpgl_command_list[index] == pcmd )
	    ;
    else
	pgl_parser_state->hpgl_command_list[pgl_parser_state->hpgl_command_next_index = ++index] = (hpgl_command_definition_t *)pcmd;
    *pindex = index;
    return (prev != 0 && prev != index);
}

/* Define a list of commands. */
void
hpgl_define_commands(const gs_memory_t *mem, const hpgl_named_command_t *pcmds,
		     hpgl_parser_state_t *pgl_parser_state
)
{	const hpgl_named_command_t *pcmd = pcmds;

	for ( ; pcmd->char1; ++pcmd )
#ifdef DEBUG
	  if (
#endif
	  hpgl_register_command(pgl_parser_state,
				&pgl_parser_state->hpgl_command_indices
				[pcmd->char1 - 'A'][pcmd->char2 - 'A'],
				&pcmd->defn)
#ifdef DEBUG
	  )
	    dprintf2("Redefining command %c%c\n", pcmd->char1, pcmd->char2);
#endif
	  ;
}

/* ---------------- Parsing ---------------- */

/* Initialize the HP-GL/2 parser state. */
void
hpgl_process_init(hpgl_parser_state_t *pst)
{	pst->first_letter = -1;
	pst->command = 0;
}

/* Process a buffer of HP-GL/2 commands. */
/* Return 0 if more input needed, 1 if ESC seen, or an error code. */
int
hpgl_process(hpgl_parser_state_t *pst, hpgl_state_t *pgls,
  stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	int code = 0;

	pst->source.limit = rlimit;
	/* Prepare to catch a longjmp indicating the argument scanner */
	/* needs more data, or encountered an error. */
	code = setjmp(pst->exit_to_parser);
	if ( code )
	  { /* The command detected an error, or we need to ask */
	    /* the caller for more data.  pst->command != 0. */
	    pr->ptr = pst->source.ptr;
	    if ( code < 0 && code != e_NeedData )
	      { pst->command = 0; /* cancel command */
	        if_debug0('i', "\n");
	        return code;
	      }
	    return 0;
	  }
	/* Check whether we're still feeding data to a command. */
call:	if ( pst->command )
	  { pst->source.ptr = p;
	    pst->arg.next = 0;
	    code = (*pst->command->proc)(pst, pgls);
	    p = pst->source.ptr;
	    if ( code < 0 )
	      goto x;
	    pst->command = 0;
	    if_debug0('i', "\n");
	  }
	while ( p < rlimit )
	  {	byte next = *++p;

		if ( next >= 'A' && next <= 'Z' )
		  next -= 'A';
		else if ( next >= 'a' && next <= 'z' )
		  next -= 'a';
		else if ( next == ESC )
		  { --p;
		    pst->first_letter = -1;
		    code = 1;
		    break;
		  }
		else		/* ignore everything else */
		  { /* Apparently this is what H-P plotters do.... */
		    if ( next > ' ' && next != ',' )
		      pst->first_letter = -1;
		    continue;
		  }
		if ( pst->first_letter < 0 )
		  { pst->first_letter = next;
		    continue;
		  }
		{ int index = pst->hpgl_command_indices[pst->first_letter][next];

#ifdef DEBUG
		  if ( gs_debug_c('i') )
		    { char c = (index ? '-' : '?');
		      dprintf4("--%c%c%c%c", pst->first_letter + 'A',
			       next + 'A', c, c);
		    }
#endif
		  if ( index == 0 )	/* anomalous, drop 1st letter */
		    { pst->first_letter = next;
		      continue;
		    }
		  pst->first_letter = -1;
		  pst->command = pst->hpgl_command_list[index];
		  pst->phase = 0;
		  pst->done = false;
		  hpgl_args_init(pst);
		  /*
		   * Only a few commands should be executed while we're in
		   * polygon mode: check for this here.  Note that we rely
		   * on the garbage-skipping property of the parser to skip
		   * over any following arguments.  This doesn't work for
		   * the few commands with special syntax that should be
		   * ignored in polygon mode (CO, DT, LB, SM); they must be
		   * flagged as executable even in polygon mode, and check
		   * the render_mode themselves.
		   */
		  {
		      bool ignore_command = false;
		      if (( pgls->g.polygon_mode ) &&
			  !(pst->command->flags & hpgl_cdf_polygon)
			  )
			  ignore_command = true;
		      else
			  { /* similarly if we are in lost mode we do not
			       execute the commands that are only defined to
			       be used when lost mode is cleared. */
			      if (( pgls->g.lost_mode == hpgl_lost_mode_entered ) &&
				  (pst->command->flags & hpgl_cdf_lost_mode_cleared)
				  )
				  ignore_command = true;
			  }
		      /* Also, check that we have a command that can be executed
			 with the current personality.  NB reorganize me. */
		      if ( pgls->personality == rtl )
			  if ( !(pst->command->flags & hpgl_cdf_rtl) ) /* not rtl pcl only */
			      ignore_command = true;
		      if ( (pgls->personality == pcl5c) || (pgls->personality == pcl5e) )
			  if ( !(pst->command->flags & hpgl_cdf_pcl) ) /* not pcl rtl only */
			      ignore_command = true;
		      if ( ignore_command )
			  pst->command = 0;
		  }
		  goto call;
		}
	  }
x:	pr->ptr = p;
	return (code == e_NeedData ? 0 : code);
}

/*
 * Get a numeric HP-GL/2 argument from the input stream.  Return 0 if no
 * argument, a pointer to the value if an argument is present, or longjmp if
 * need more data.  Note that no errors are possible.
 */
static const hpgl_value_t *
hpgl_arg(const gs_memory_t *mem, 
	 hpgl_parser_state_t *pst)
{	const byte *p;
        const byte *rlimit;
	hpgl_value_t *pvalue;

#define parg (&pst->arg)
	if ( parg->next < parg->count )
	  { /* We're still replaying already-scanned arguments. */
	    return &parg->scanned[parg->next++];
	  }
	if ( pst->done )
	  return 0;
        p = pst->source.ptr;
        rlimit = pst->source.limit;
	pvalue = &parg->scanned[parg->count];
#define check_value()\
  if ( parg->have_value ) goto done

	for ( ; p < rlimit; ++p )
	  { byte ch = p[1];
	    switch ( ch )
	      {
	      case '+':
		check_value();
		parg->have_value = 1;
		parg->sign = 1;
		pvalue->v_n.i = 0;
		break;
	      case '-':
		check_value();
		parg->have_value = 1;
		parg->sign = -1;
		pvalue->v_n.i = 0;
		break;
	      case '.':
		switch ( parg->have_value )
		  {
		  default:	/* > 1 */
		    goto out;
		  case 0:
		    pvalue->v_n.r = 0;
		    break;
		  case 1:
		    pvalue->v_n.r = pvalue->v_n.i;
		  }
		parg->have_value = 2;
		parg->frac_scale = 1.0;
		break;
	      case ';':
		pst->done = true;
		check_value();
		goto out;
	      case HT: case LF: case FF: case CR: 
		/* control charachers are ignored during parsing hpgl
		 */
		continue;
	      case SP: case ',':
		/*
		 * The specification doesn't say what to do with extra
		 * separators; we just ignore them.
		 */
		if ( !parg->have_value )
		  break;
		++p;
done:		if ( parg->sign < 0 )
		  { if ( parg->have_value > 1 )
		      pvalue->v_n.r = -pvalue->v_n.r;
		    else
		      pvalue->v_n.i = -pvalue->v_n.i;
		  }
		goto out;
	      case '0': case '1': case '2': case '3': case '4':
	      case '5': case '6': case '7': case '8': case '9':
		ch -= '0';
#define max_i 0x7fffffff
		switch ( parg->have_value )
		  {
		  default:	/* case 2 */
		    pvalue->v_n.r += ch / (parg->frac_scale *= 10);
		    break;
		  case 0:
		    parg->have_value = 1;
		    pvalue->v_n.i = ch;
		    break;
		  case 1:
		    if ( pvalue->v_n.i >= max_i/10 &&
			 (pvalue->v_n.i > max_i/10 || ch > max_i%10)
		       )
                        return NULL;
		    else
		      pvalue->v_n.i = pvalue->v_n.i * 10 + ch;
		  }
		break;
	      default:
		pst->done = true;
		check_value();
		goto out;
	      }
	  }
	/* We ran out of data before reaching a terminator. */
	pst->source.ptr = p;
	longjmp(pst->exit_to_parser, e_NeedData);
	/* NOTREACHED */
out:	pst->source.ptr = p;
	switch ( parg->have_value )
	  {
	  case 0:		/* no argument */
	    return NULL;
	  case 1:		/* integer */
	    if_debug1('I', "  %ld", (long)pvalue->v_n.i);
	    pvalue->is_real = false;
	    break;
	  default /* case 2 */:	/* real */
	    if_debug1('I', "  %g", pvalue->v_n.r);
	    pvalue->is_real = true;
	  }
	hpgl_arg_init(pst);
	parg->next = ++(parg->count);
	return pvalue;
#undef parg
}

/* Get a real argument. */
bool
hpgl_arg_real(const gs_memory_t *mem,
	      hpgl_args_t *pargs, hpgl_real_t *pr)
{	const hpgl_value_t *pvalue = hpgl_arg(mem, pargs);

	if ( !pvalue )
	  return false;
	*pr = (pvalue->is_real ? pvalue->v_n.r : pvalue->v_n.i);
	return true;
}

/* Get a clamped real argument. */
bool
hpgl_arg_c_real(const gs_memory_t *mem,
		hpgl_args_t *pargs, hpgl_real_t *pr)
{	const hpgl_value_t *pvalue = hpgl_arg(mem, pargs);
	hpgl_real_t r;

	if ( !pvalue )
	  return false;
	r = (pvalue->is_real ? pvalue->v_n.r : pvalue->v_n.i);
	*pr = (r < -32768 ? -32768 : r > 32767 ? 32767 : r);
	return true;

}

/* Get an integer argument. */
bool
hpgl_arg_int(const gs_memory_t *mem,
	     hpgl_args_t *pargs, int32 *pi)
{	const hpgl_value_t *pvalue = hpgl_arg(mem, pargs);

	if ( !pvalue )
	  return false;
	*pi = (pvalue->is_real ? (int32)pvalue->v_n.r : pvalue->v_n.i);
	return true;
}

/* Get a clamped integer argument. */
bool
hpgl_arg_c_int(const gs_memory_t *mem, 
	       hpgl_args_t *pargs, int *pi)
{	const hpgl_value_t *pvalue = hpgl_arg(mem, pargs);
	int32 i;

	if ( !pvalue )
	  return false;
	i = (pvalue->is_real ? (int32)pvalue->v_n.r : pvalue->v_n.i);
	*pi = (i < -32768 ? -32768 : i > 32767 ? 32767 : i);
	return true;
}

/* Get a "current units" argument. */
bool
hpgl_arg_units(const gs_memory_t *mem, 
	       hpgl_args_t *pargs, hpgl_real_t *pu)
{	/****** PROBABLY WRONG ******/
	return hpgl_arg_real(mem, pargs, pu);
}

/* initialize the HPGL command counter */
 int
hpgl_init_command_index(hpgl_parser_state_t **pgl_parser_state, gs_memory_t *mem)
{
    hpgl_parser_state_t *pgst = 
	(hpgl_parser_state_t *)gs_alloc_bytes(mem, sizeof(hpgl_parser_state_t),
					      "hpgl_init_command_index");
    /* fatal */
    if ( pgst == 0 )
	return -1;
	
    pgst->hpgl_command_next_index = 0;
    /* NB fix me the parser should not depend on this behavior the
       previous design had these in bss which was automatically
       cleared to zero. */
    memset(pgst->hpgl_command_indices, 0, sizeof(pgst->hpgl_command_indices));
    hpgl_process_init(pgst);
    *pgl_parser_state = pgst;
    return 0;
}
