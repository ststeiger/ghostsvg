/* Copyright (C) 1994, 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Token reading operators */
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "gsstruct.h"		/* for iscan.h */
#include "stream.h"
#include "files.h"
#include "store.h"
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "iname.h"
#include "iscan.h"

/* <file> token <obj> -true- */
/* <string> token <post> <obj> -true- */
/* <string|file> token -false- */
private int ztoken_continue(P1(i_ctx_t *));
private int token_continue(P4(i_ctx_t *, stream *, scanner_state *, bool));
int
ztoken(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_file: {
	    stream *s;
	    scanner_state state;

	    check_read_file(s, op);
	    check_ostack(1);
	    scanner_state_init(&state, false);
	    return token_continue(i_ctx_p, s, &state, true);
	}
	case t_string: {
	    ref token;
	    int code = scan_string_token(i_ctx_p, op, &token);

	    switch (code) {
	    case scan_EOF:	/* no tokens */
		make_false(op);
		return 0;
	    default:
		if (code < 0)
		    return code;
	    }
	    push(2);
	    op[-1] = token;
	    make_true(op);
	    return 0;
	}
    }
}
/* Continue reading a token after an interrupt or callout. */
/* *op is the scanner state; op[-1] is the file. */
private int
ztoken_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream *s;
    scanner_state *pstate;

    check_read_file(s, op - 1);
    check_stype(*op, st_scanner_state);
    pstate = r_ptr(op, scanner_state);
    pop(1);
    return token_continue(i_ctx_p, s, pstate, false);
}
/* Common code for token reading. */
private int
token_continue(i_ctx_t *i_ctx_p, stream * s, scanner_state * pstate,
	       bool save)
{
    os_ptr op = osp;
    int code;
    ref token;

    /* Note that scan_token may change osp! */
    /* Also, we must temporarily remove the file from the o-stack */
    /* when calling scan_token, in case we are scanning a procedure. */
    ref fref;

    ref_assign(&fref, op);
again:
    pop(1);
    code = scan_token(i_ctx_p, s, &token, pstate);
    op = osp;
    switch (code) {
	default:		/* error */
	    if (code > 0)	/* comment, not possible */
		code = gs_note_error(e_syntaxerror);
	    push(1);
	    ref_assign(op, &fref);
	    break;
	case scan_BOS:
	    code = 0;
	case 0:		/* read a token */
	    push(2);
	    ref_assign(op - 1, &token);
	    make_true(op);
	    break;
	case scan_EOF:		/* no tokens */
	    push(1);
	    make_false(op);
	    code = 0;
	    break;
	case scan_Refill:	/* need more data */
	    push(1);
	    ref_assign(op, &fref);
	    code = scan_handle_refill(i_ctx_p, op, pstate, save, false,
				      ztoken_continue);
	    switch (code) {
		case 0:	/* state is not copied to the heap */
		    goto again;
		case o_push_estack:
		    return code;
	    }
	    break;		/* error */
    }
    if (code <= 0 && !save) {	/* Deallocate the scanner state record. */
	ifree_object(pstate, "token_continue");
    }
    return code;
}

/* <file> .tokenexec - */
/* Read a token and do what the interpreter would do with it. */
/* This is different from token + exec because literal procedures */
/* are not executed (although binary object sequences ARE executed). */
int ztokenexec_continue(P1(i_ctx_t *));	/* export for interpreter */
private int tokenexec_continue(P4(i_ctx_t *, stream *, scanner_state *, bool));
int
ztokenexec(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream *s;
    scanner_state state;

    check_read_file(s, op);
    check_estack(1);
    scanner_state_init(&state, false);
    return tokenexec_continue(i_ctx_p, s, &state, true);
}
/* Continue reading a token for execution after an interrupt or callout. */
/* *op is the scanner state; op[-1] is the file. */
/* We export this because this is how the interpreter handles a */
/* scan_Refill for an executable file. */
int
ztokenexec_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream *s;
    scanner_state *pstate;

    check_read_file(s, op - 1);
    check_stype(*op, st_scanner_state);
    pstate = r_ptr(op, scanner_state);
    pop(1);
    return tokenexec_continue(i_ctx_p, s, pstate, false);
}
/* Common code for token reading + execution. */
private int
tokenexec_continue(i_ctx_t *i_ctx_p, stream * s, scanner_state * pstate,
		   bool save)
{
    os_ptr op = osp;
    int code;
    /* Note that scan_token may change osp! */
    /* Also, we must temporarily remove the file from the o-stack */
    /* when calling scan_token, in case we are scanning a procedure. */
    ref fref;

    ref_assign(&fref, op);
    pop(1);
again:
    check_estack(1);
    code = scan_token(i_ctx_p, s, (ref *) (esp + 1), pstate);
    op = osp;
    switch (code) {
	case 0:
	    if (r_is_proc(esp + 1)) {	/* Treat procedure as a literal. */
		push(1);
		ref_assign(op, esp + 1);
		code = 0;
		break;
	    }
	    /* falls through */
	case scan_BOS:
	    ++esp;
	    code = o_push_estack;
	    break;
	case scan_EOF:		/* no tokens */
	    code = 0;
	    break;
	case scan_Refill:	/* need more data */
	    code = scan_handle_refill(i_ctx_p, &fref, pstate, save, true,
				      ztokenexec_continue);
	    switch (code) {
		case 0:	/* state is not copied to the heap */
		    goto again;
		case o_push_estack:
		    return code;
	    }
	    /* falls through */
	default:		/* error */
	    break;
    }
    if (code < 0) {		/* Push the operand back on the stack. */
	push(1);
	ref_assign(op, &fref);
    }
    if (!save) {		/* Deallocate the scanner state record. */
	ifree_object(pstate, "token_continue");
    }
    return code;
}

/*
 * Handle a scan_Comment or scan_DSC_Comment return from scan_token
 * (scan_code) by calling out to %Process[DSC]Comment.  The continuation
 * procedure expects the file and scanner state on the o-stack.
 */
int
ztoken_handle_comment(i_ctx_t *i_ctx_p, const ref *fop, scanner_state *sstate,
		      const ref *ptoken, int scan_code,
		      bool save, bool push_file, op_proc_t cont)
{
    const char *proc_name;
    scanner_state *pstate;
    os_ptr op;
    int code;

    switch (scan_code) {
    case scan_Comment:
	proc_name = "%ProcessComment";
	break;
    case scan_DSC_Comment:
	proc_name = "%ProcessDSCComment";
	break;
    default:
	return_error(e_Fatal);	/* can't happen */
    }
    check_ostack(2);		/* ****** WRONG, RETURNS ON OVERFLOW ****** */
    check_estack(4);
    code = name_enter_string(proc_name, esp + 4);
    if (code < 0)
	return code;
    if (save) {
	pstate = ialloc_struct(scanner_state, &st_scanner_state,
			       "ztoken_handle_comment");
	if (pstate == 0)
	    return_error(e_VMerror);
	*pstate = *sstate;
    } else
	pstate = sstate;
    /* Push the file and comment string on the o-stack. */
    op = osp += 2;
    osp[-1] = *fop;
    *osp = *ptoken;
    /*
     * Push the continuation, scanner state, file, and callout name on the
     * e-stack.
     */
    make_op_estack(esp + 1, cont);
    make_istruct(esp + 2, 0, pstate);
    esp[3] = *fop;
    r_clear_attrs(esp + 3, a_executable);
    r_set_attrs(esp + 4, a_executable);	/* see name_ref above */
    esp += 4;
    return o_push_estack;
}

/* ------ Initialization procedure ------ */

const op_def ztoken_op_defs[] =
{
    {"1token", ztoken},
    {"1.tokenexec", ztokenexec},
		/* Internal operators */
    {"2%ztokenexec_continue", ztokenexec_continue},
    op_def_end(0)
};
