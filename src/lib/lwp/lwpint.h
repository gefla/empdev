/*
 * lwpint.h -- lwp internal structures
 *
 * Copyright (C) 1991-3 Stephen Crane.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * author: Stephen Crane, (jsc@doc.ic.ac.uk), Department of Computing,
 * Imperial College of Science, Technology and Medicine, 180 Queen's
 * Gate, London SW7 2BZ, England.
 */
#ifndef _LWPINT_H
#define _LWPINT_H

#ifdef UCONTEXT
#include <ucontext.h>
#else  /* !UCONTEXT */
#include <setjmp.h>
#endif /* !UCONTEXT */

/* `liveness' counter: check signals every `n' visits to the scheduler */
/* note: the lower this value, the more responsive the system but the */
/* more inefficient the context switch time */
#define LCOUNT	-1

/* process control block.  do *not* change the position of context */
struct lwpProc {
#ifdef UCONTEXT
    ucontext_t context;		/* context structure */
#else  /* !UCONTEXT */
    jmp_buf context;		/* processor context area */
#endif /* !UCONTEXT */
    void *sbtm;			/* bottom of stack attached to it */
    int size;			/* size of stack */
    void (*entry)(void *);	/* entry point */
    int dead;			/* whether the process can be rescheduled */
    int pri;			/* which scheduling queue we're on */
    long runtime;		/* time at which process is restarted */
    int fd;			/* fd we're blocking on */
    int argc;			/* initial arguments */
    char **argv;
    void *ud;			/* user data */
    void *lowmark;		/* start of low buffer around stack */
    void *himark;		/* start of upper buffer around stack */
    char *name;			/* process name and description */
    char *desc;
    int flags;
    struct lwpProc *next;
};

/* queue */
struct lwpQueue {
    struct lwpProc *head;
    struct lwpProc *tail;
};

/* semaphore */
struct lwpSem {
    int count;
    struct lwpQueue q;
    char *name;
};

#ifdef UCONTEXT
void lwpInitContext(struct lwpProc *, stack_t *);
#define lwpSave(x)    getcontext(&(x))
#define lwpRestore(x) setcontext(&(x))
#else  /* !UCONTEXT */
#if defined(hpux) && !defined(hpc)
void lwpInitContext(volatile struct lwpProc * volatile, void *);
#else
void lwpInitContext(struct lwpProc *, void *);
#endif
#if defined(hpc)
int lwpSave(jmp_buf);
#define lwpRestore(x)	longjmp(x, 1)
#elif defined(hpux) || defined(AIX32) || defined(ALPHA)
int lwpSave(jmp_buf);
void lwpRestore(jmp_buf);
#elif defined(SUN4)
#define	lwpSave(x)	_setjmp(x)
#define lwpRestore(x)	_longjmp(x, 1)
#else
#define	lwpSave(x)	setjmp(x)
#define lwpRestore(x)	longjmp(x, 1)
#endif
#endif /* !UCONTEXT */

#ifdef AIX32
/* AIX needs 12 extra bytes above the stack; we add it here */
#define	LWP_EXTRASTACK	3*sizeof(long)
#else
#define LWP_EXTRASTACK	0
#endif

#define LWP_REDZONE	1024	/* make this a multiple of 1024 */

/* XXX Note that this assumes sizeof(long) == 4 */
#define LWP_CHECKMARK	0x5a5a5a5aL

#ifdef hpux
#define STKALIGN 64
#else
#define STKALIGN sizeof(double)
#endif

/* internal routines */
void lwpAddTail(struct lwpQueue *, struct lwpProc *);
struct lwpProc *lwpGetFirst(struct lwpQueue *);
void lwpReady(struct lwpProc *);
void lwpReschedule(void);
void lwpEntryPoint(void);
void lwpInitSelect(struct lwpProc * self);
void lwpDestroy(struct lwpProc * proc);

#endif /* _LWP_H */
