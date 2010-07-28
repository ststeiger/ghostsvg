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
/* Interface to platform-dependent synchronization primitives */

#if !defined(gpsync_INCLUDED)
#  define gpsync_INCLUDED

/* Initial version 4/1/98 by John Desrosiers (soho@crl.com). */
/* 8/9/98 L. Peter Deutsch (ghost@aladdin.com) Changed ...sizeof to
   procedures, added some comments. */

/* -------- Synchronization primitives ------- */

/*
 * Semaphores support wait/signal semantics: a wait operation will allow
 * control to proceed iff the number of signals since semaphore creation
 * is greater than the number of waits.
 */
typedef struct {
    void *dummy_;
} gp_semaphore;

uint gp_semaphore_sizeof(void);
/*
 * Hack: gp_semaphore_open(0) succeeds iff it's OK for the memory manager
 * to move a gp_semaphore in memory.
 */
int gp_semaphore_open(gp_semaphore * sema);
int gp_semaphore_close(gp_semaphore * sema);
int gp_semaphore_wait(gp_semaphore * sema);
int gp_semaphore_signal(gp_semaphore * sema);

/*
 * Monitors support enter/leave semantics: at most one thread can have
 * entered and not yet left a given monitor.
 */
typedef struct {
    void *dummy_;
} gp_monitor;

uint gp_monitor_sizeof(void);
/*
 * Hack: gp_monitor_open(0) succeeds iff it's OK for the memory manager
 * to move a gp_monitor in memory.
 */
int gp_monitor_open(gp_monitor * mon);
int gp_monitor_close(gp_monitor * mon);
int gp_monitor_enter(gp_monitor * mon);
int gp_monitor_leave(gp_monitor * mon);

/*
 * A new thread starts by calling a procedure, passing it a void * that
 * allows it to gain access to whatever data it needs.
 *
 * NOW DEPRECATED: USE gs_create_handled_thread instead.
 */
typedef void (*gp_thread_creation_callback_t) (void *);
int gp_create_thread(gp_thread_creation_callback_t, void *);

/*
 * Start a new thread in which the given callback procedure (fun) is called,
 * passing it the given void * (arg) that allows it to gain access to whatever
 * data it needs. If thread creation succeeds, this returns 0, and a thread id
 * is placed in *thread. Otherwise a negative value is returned and *thread
 * becomes NULL.
 *
 * This thread id cannot simply be discarded, it must be gp_thread_finished to
 * avoid resource leakage or even crashes.
 */
typedef void *gp_thread_id;
int gp_thread_start(gp_thread_creation_callback_t fun, void *arg, gp_thread_id *thread);

/*
 * Given a thread id created by gp_thread_start, this causes the current
 * thread to wait until that thread has completed, and to discard
 * the thread id. No further operations on the thread id are permitted.
 */
void gp_thread_finish(gp_thread_id thread);

#endif /* !defined(gpsync_INCLUDED) */
