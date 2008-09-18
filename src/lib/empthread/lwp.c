/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2008, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                           Ken Stevens, Steve McClure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  ---
 *
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  lwp.c: Interface from Empire threads to LWP threads
 *
 *  Known contributors to this file:
 *     Sasha Mikheev
 *     Markus Armbruster, 2006-2007
 */

#include <config.h>

#include <signal.h>
#include <time.h>
#include "empthread.h"
#include "misc.h"

/* Flags that were passed to empth_init() */
static int empth_flags;


int
empth_init(void **ctx, int flags)
{
    sigset_t set;

    empth_flags = flags;
    empth_init_signals();
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    lwpInitSystem(1, ctx, flags, &set);
    return 0;
}


empth_t *
empth_create(void (*entry)(void *), int size, int flags,
	     char *name, void *ud)
{
    if (!flags)
	flags = empth_flags;
    return lwpCreate(1, entry, size, flags, name, 0, 0, ud);
}

empth_t *
empth_self(void)
{
    return LwpCurrent;
}

char *
empth_name(empth_t *thread)
{
    return lwpName(thread);
}

void
empth_set_name(empth_t *thread, char *name)
{
    lwpSetName(thread, name);
}

void
empth_exit(void)
{
    lwpExit();
}

void
empth_yield(void)
{
    lwpYield();
}

void
empth_terminate(empth_t *a)
{
    lwpTerminate(a);
}

void
empth_select(int fd, int flags)
{
    lwpSleepFd(fd, flags);
}

void
empth_wakeup(empth_t *a)
{
    lwpWakeup(a);
}

int
empth_sleep(time_t until)
{
    return lwpSleepUntil(until);
}

int
empth_wait_for_signal(void)
{
    sigset_t set;
    int sig, err;
    time_t now;

    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    for (;;) {
	err = lwpSigWait(&set, &sig);
	if (CANT_HAPPEN(err)) {
	    time(&now);
	    lwpSleepUntil(now + 60);
	    continue;
	}
	return sig;
    }
}

empth_rwlock_t *
empth_rwlock_create(char *name)
{
    return lwp_rwlock_create(name);
}

void
empth_rwlock_destroy(empth_rwlock_t *rwlock)
{
    lwp_rwlock_destroy(rwlock);
}

void
empth_rwlock_wrlock(empth_rwlock_t *rwlock)
{
    lwp_rwlock_wrlock(rwlock);
}

void
empth_rwlock_rdlock(empth_rwlock_t *rwlock)
{
    lwp_rwlock_rdlock(rwlock);
}

void
empth_rwlock_unlock(empth_rwlock_t *rwlock)
{
    lwp_rwlock_unlock(rwlock);
}
