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
 *  ntthread.c: Interface from Empire threads to Windows NT threads
 * 
 *  Known contributors to this file:
 *     Doug Hay, 1998
 *     Steve McClure, 1998
 *     Ron Koenderink, 2004-2007
 */

/*
 * EMPTHREADs for Windows NT.
 *
 * Actually, threads for any Win32 platform, like Win95, Win98, WinCE,
 * and whatever other toy OSs are in our future from Microsoft.
 *
 * WIN32 has a full pre-emptive threading environment.  But Empire can
 * not handle pre-emptive threading.  Thus, we will use the threads,
 * but limit the preemption using a Mutex.
 */

#include <config.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <winsock2.h>
#undef NS_ALL
#include <windows.h>
#include <process.h>
/* Note: unistd.h(posixio.c) is not thread-safe.
 * It may be used *only* while holding hThreadMutex.
 */
#include "unistd.h"
#include "misc.h"
#include "empthread.h"
#include "prototypes.h"
#include "server.h"

#define loc_MIN_THREAD_STACK  16384

/************************
 * loc_Thread
 */
struct loc_Thread {

    /* The thread name, passed in at create time. */
    char szName[17];

    /* True if this is the main line, and not a real thread. */
    BOOL bMainThread;

    /* The user data passed in at create time. */
    void *pvUserData;

    /* True if this thread has been killed. */
    BOOL bKilled;

    /* The entry function for the thread. */
    void (*pfnEntry) (void *);

    /* The system thread ID. */
    unsigned long ulThreadID;

    /* An Mutex that the thread will wait/sleep on. */
    HANDLE hThreadEvent;
};


/************************
 * loc_RWLock
 *
 * Invariants
 *	must hold at function call, return, sleep
 *	and resume from sleep.
 *
 * any state:
 *	nwrite >= 0
 *	nread >= 0

 * if unlocked:
 *	can_read set
 *	can_write set
 *	nwrite == 0
 *	nread == 0
 *
 * if read-locked without writers contending:
 *	can_read set
 *	can_write clear
 *	nwrite == 0
 *	nread > 0
 *
 * if read-locked with writers contending:
 *	can_read clear
 *	can_write clear
 *	nwrite > 0    #writers blocked
 *	nread > 0
 *
 * if write-locked:
 *	can_read clear
 *	can_write clear
 *	nwrite > 0    #writers blocked + 1
 *	nread == 0
 *
 * To ensure consistency, state normally changes only while the
 * thread changing it holds hThreadMutex.
 *
 */
struct loc_RWLock {
    char name[17];	/* The thread name, passed in at create time. */
    HANDLE can_read;	/* Manual event -- allows read locks */
    HANDLE can_write;	/* Auto-reset event -- allows write locks */
    int nread;		/* number of active readers */
    int nwrite;		/* total number of writers (active and waiting) */
};

/* This is the thread exclusion/non-premption mutex. */
/* The running thread has this MUTEX, and all others are */
/* either blocked on it, or waiting for some OS response. */
static HANDLE hThreadMutex;

/* This is the thread startup event. */
/* We use this to lockstep when we are starting up threads. */
static HANDLE hThreadStartEvent;

/* This is an event used to wakeup the main thread */
/* to start the shutdown sequence. */
static HANDLE hShutdownEvent;

/* The Thread Local Storage index.  We store the pThread pointer */
/* for each thread at this index. */
static DWORD dwTLSIndex;

/* The current running thread. */
static empth_t *pCurThread;

/* Ticks at start */
static unsigned long ulTickAtStart;

/* Pointer out to global context.  "player". */
/* From empth_init parameter. */
static void **ppvUserData;

/* Global flags.  From empth_init parameter. */
static int global_flags;

static void loc_debug(const char *, ...) ATTRIBUTE((format(printf, 1, 2)));

/************************
 * loc_debug
 *
 * Print out the current thread's status??
 */
static void
loc_debug(const char *pszFmt, ...)
{
    va_list vaList;
    unsigned long ulCurTick;
    unsigned long ulRunTick;
    unsigned long ulMs, ulSec, ulMin, ulHr;
    empth_t *pThread = TlsGetValue(dwTLSIndex);
    char buf[1024];

    if ((global_flags & EMPTH_PRINT) != 0) {

	/* Ticks are in milliseconds */
	ulCurTick = GetTickCount();

	ulRunTick = ulCurTick - ulTickAtStart;
	ulMs = ulRunTick % 1000L;
	ulSec = (ulRunTick / 1000L) % 60L;
	ulMin = (ulRunTick / (60L * 1000L)) % 60L;
	ulHr = (ulRunTick / (60L * 60L * 1000L));

	va_start(vaList, pszFmt);
	vsprintf(buf, pszFmt, vaList);
	va_end(vaList);

	if (pThread) {
	    printf("%ld:%02ld:%02ld.%03ld %17s: %s\n",
		   ulHr, ulMin, ulSec, ulMs, pThread->szName, buf);
	} else {
	    printf("%ld:%02ld:%02ld.%03ld %17s: %s\n",
		   ulHr, ulMin, ulSec, ulMs, "UNKNOWN", buf);
	}

    }
}

/************************
 * loc_FreeThreadInfo
 */
static void
loc_FreeThreadInfo(empth_t *pThread)
{
    if (pThread) {
	if (pThread->hThreadEvent)
	    CloseHandle(pThread->hThreadEvent);
	memset(pThread, 0, sizeof(*pThread));
	free(pThread);
    }
}

/************************
 * loc_RunThisThread
 *
 * This thread wants to run.
 * When this function returns, the globals are set to this thread
 * info, and the thread owns the MUTEX.
 */
static void
loc_RunThisThread(HANDLE hWaitObject)
{
    HANDLE hWaitObjects[2];

    empth_t *pThread = TlsGetValue(dwTLSIndex);

    if (pThread->bKilled) {
	if (!pThread->bMainThread) {
	    TlsSetValue(dwTLSIndex, NULL);
	    loc_FreeThreadInfo(pThread);
	    _endthread();
	}
    }

    hWaitObjects[0] = hThreadMutex;
    hWaitObjects[1] = hWaitObject;

    WaitForMultipleObjects(hWaitObject ? 2 : 1, hWaitObjects,
			   TRUE, INFINITE);

    if (!pCurThread) {
	/* Set the globals to this thread. */
	*ppvUserData = pThread->pvUserData;

	pCurThread = pThread;
    } else {
	/* Hmm, a problem, eh? */
	logerror("RunThisThread, someone already running.");
    }
}

/************************
 * loc_BlockThisThread
 *
 * This thread was running.  It no longer wants to.
 */
static void
loc_BlockThisThread(void)
{
    empth_t *pThread = TlsGetValue(dwTLSIndex);

    if (pCurThread == pThread) {
	/* Reset the globals back to original */

	pCurThread = NULL;
	*ppvUserData = NULL;

	/* Release the MUTEX */
	ReleaseMutex(hThreadMutex);
    } else {
	/* Hmm, this thread was not the running one. */
	logerror("BlockThisThread, not running.");
    }
}

/************************
 * loc_Exit_Handler
 *
 * Ctrl-C, Ctrl-Break, Window-Closure, User-Logging-Off or
 * System-Shutdown will initiate a shutdown.
 * This is done by calling empth_request_shutdown()
 */
static BOOL WINAPI
loc_Exit_Handler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) { 
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT: 
        case CTRL_LOGOFF_EVENT: 
        case CTRL_SHUTDOWN_EVENT: 
	    empth_request_shutdown();
            return TRUE;
        default:
            return FALSE;
    }
}

/************************
 * empth_threadMain
 *
 * This is the main line of each thread.
 * This is really a static local func....
 * Note: As the POSIX compatibility layer is not thread safe
 * this function can not open or create any files or sockets until
 * loc_RunThisThread() is called
 */
static void
empth_threadMain(void *pvData)
{
    empth_t *pThread = pvData;

    /* Out of here... */
    if (!pvData)
	return;

    /* Store pThread on this thread. */
    TlsSetValue(dwTLSIndex, pvData);

    /* Get the ID of the thread. */
    pThread->ulThreadID = GetCurrentThreadId();

    /* Signal that the thread has started. */
    SetEvent(hThreadStartEvent);

    /* Switch to this thread context */
    loc_RunThisThread(NULL);

    /* Run the thread. */
    if (pThread->pfnEntry)
	pThread->pfnEntry(pThread->pvUserData);

    /* Kill the thread. */
    empth_exit();
}

/************************
 * empth_init
 *
 * Initialize the thread environment.
 *
 * This is called from the program main line.
 */
int
empth_init(void **ctx_ptr, int flags)
{
    empth_t *pThread = NULL;

    ulTickAtStart = GetTickCount();
    ppvUserData = ctx_ptr;
    global_flags = flags;
    dwTLSIndex = TlsAlloc();

    /* Create the thread mutex. */
    /* Initally unowned. */
    hThreadMutex = CreateMutex(NULL, FALSE, NULL);
    if (!hThreadMutex) {
	logerror("Failed to create mutex %lu", GetLastError());
	return 0;
    }

    /* Create the thread start event. */
    /* Automatic state reset. */
    hThreadStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hThreadStartEvent) {
	logerror("Failed to create start event %lu", GetLastError());
	return 0;
    }

    /* Create the shutdown event for the main thread. */
    /* Manual reset */
    hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!hShutdownEvent) {
        logerror("Failed to create shutdown event %lu", GetLastError());
	return 0;
    }
    SetConsoleCtrlHandler(loc_Exit_Handler, TRUE);

    /* Create the global Thread context. */
    pThread = malloc(sizeof(*pThread));
    if (!pThread) {
	logerror("not enough memory to create main thread.");
	return 0;
    }
    memset(pThread, 0, sizeof(*pThread));

    strncpy(pThread->szName, "Main", sizeof(pThread->szName) - 1);
    pThread->ulThreadID = GetCurrentThreadId();
    pThread->bMainThread = TRUE;

    TlsSetValue(dwTLSIndex, pThread);

    /* Make this the running thread. */
    loc_RunThisThread(NULL);

    logerror("NT pthreads initialized");
    return 0;
}


/************************
 * empth_create
 *
 * Create a new thread.
 *
 * entry - entry point function for thread.
 * size  - stack size.
 * flags - debug control.
 *           LWP_STACKCHECK  - not needed
 * name  - name of the thread, for debug.
 * ud    - "user data".  The "ctx_ptr" gets this value
 *         when the thread is active.
 *         It is also passed to the entry function...
 */
empth_t *
empth_create(void (*entry)(void *), int size, int flags,
	     char *name, void *ud)
{
    empth_t *pThread = NULL;

    loc_debug("creating new thread %s", name);

    pThread = malloc(sizeof(*pThread));
    if (!pThread) {
	logerror("not enough memory to create thread %s", name);
	return NULL;
    }
    memset(pThread, 0, sizeof(*pThread));

    strncpy(pThread->szName, name, sizeof(pThread->szName) - 1);
    pThread->pvUserData = ud;
    pThread->pfnEntry = entry;
    pThread->bMainThread = FALSE;

    /* Create thread event, auto reset. */
    pThread->hThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (size < loc_MIN_THREAD_STACK)
	size = loc_MIN_THREAD_STACK;

    pThread->ulThreadID = _beginthread(empth_threadMain, size, pThread);
    if (pThread->ulThreadID == 1L) {
	logerror("can not create thread: %s: %s", name, strerror(errno));
	goto bad;
    }

    loc_debug("new thread id is %ld", pThread->ulThreadID);
    empth_yield();
    return pThread;

  bad:
    if (pThread) {
	loc_FreeThreadInfo(pThread);
    }
    return NULL;
}


/************************
 * empth_self
 */
empth_t *
empth_self(void)
{
    empth_t *pThread = TlsGetValue(dwTLSIndex);

    return pThread;
}

/************************
 * empth_exit
 */
void
empth_exit(void)
{
    empth_t *pThread = TlsGetValue(dwTLSIndex);

    loc_debug("empth_exit");
    loc_BlockThisThread();

    TlsSetValue(dwTLSIndex, NULL);
    loc_FreeThreadInfo(pThread);
    _endthread();
}

/************************
 * empth_yield
 *
 * Yield processing to another thread.
 */
void
empth_yield(void)
{
    loc_BlockThisThread();
    Sleep(0);
    loc_RunThisThread(NULL);
}

/************************
 * empth_terminate
 *
 * Kill off the thread.
 */
void
empth_terminate(empth_t *pThread)
{
    loc_debug("killing thread %s", pThread->szName);
    pThread->bKilled = TRUE;

    SetEvent(pThread->hThreadEvent);
}

/************************
 * empth_select
 *
 * Do a select on the given file.
 * Wait for IO on it.
 *
 * This would be one of the main functions used within gen\io.c
 */
void
empth_select(int fd, int flags)
{
    int handle;
    WSAEVENT hEventObject[2];
    empth_t *pThread = TlsGetValue(dwTLSIndex);

    loc_debug("%s select on %d",
	      flags == EMPTH_FD_READ ? "read" : "write", fd);
    loc_BlockThisThread();

    hEventObject[0] = WSACreateEvent();
    hEventObject[1] = pThread->hThreadEvent;

    handle = posix_fd2socket(fd);
    CANT_HAPPEN(handle < 0);

    if (flags == EMPTH_FD_READ)
	WSAEventSelect(handle, hEventObject[0], FD_READ | FD_ACCEPT | FD_CLOSE);
    else if (flags == EMPTH_FD_WRITE)
	WSAEventSelect(handle, hEventObject[0], FD_WRITE | FD_CLOSE);
    else {
	logerror("bad flag %d passed to empth_select", flags);
	empth_exit();
    }

    WSAWaitForMultipleEvents(2, hEventObject, FALSE, WSA_INFINITE, FALSE);

    WSAEventSelect(handle, hEventObject[0], 0);

    WSACloseEvent(hEventObject[0]);

    loc_RunThisThread(NULL);
}

/************************
 * empth_wakeup
 *
 * Wake up the specified thread.
 */
void
empth_wakeup(empth_t *pThread)
{
    loc_debug("waking up thread %s", pThread->szName);

    /* Let it run if it is blocked... */
    SetEvent(pThread->hThreadEvent);
}

/************************
 * empth_sleep
 *
 * Put the given thread to sleep...
 */
int
empth_sleep(time_t until)
{
    long lSec;
    empth_t *pThread = TlsGetValue(dwTLSIndex);
    int iReturn = 0;

    while (!iReturn && ((lSec = until - time(0)) > 0)) {
	loc_BlockThisThread();
	loc_debug("going to sleep %ld sec", lSec);

	if (WaitForSingleObject(pThread->hThreadEvent, lSec * 1000L) !=
	    WAIT_TIMEOUT)
	    iReturn = -1;

	loc_debug("sleep done. Waiting to run.");
	loc_RunThisThread(NULL);
    }
    return iReturn;
}

/************************
 * empth_request_shutdown
 *
 * This wakes up empth_wait_for_signal() so shutdown can proceed.
 * This is done by signalling hShutdownEvent.
 */
void
empth_request_shutdown(void)
{
    SetEvent(hShutdownEvent);
}

int
empth_wait_for_signal(void)
{
    loc_BlockThisThread();
    loc_RunThisThread(hShutdownEvent);
    return SIGTERM;
}

empth_rwlock_t *
empth_rwlock_create(char *name)
{
    empth_rwlock_t *rwlock;

    rwlock = malloc(sizeof(*rwlock));
    if (!rwlock)
	return NULL;

    memset(rwlock, 0, sizeof(*rwlock));
    strncpy(rwlock->name, name, sizeof(rwlock->name) - 1);

    if ((rwlock->can_read = CreateEvent(NULL, TRUE, TRUE, NULL)) == NULL) {
	logerror("rwlock_create: failed to create reader event %s at %s:%d",
	    name, __FILE__, __LINE__);
	free(rwlock);
	return NULL;
    }

    if ((rwlock->can_write = CreateEvent(NULL, FALSE, TRUE, NULL)) == NULL) {
	logerror("rwlock_create: failed to create writer event %s at %s:%d",
	    name, __FILE__, __LINE__);
	CloseHandle(rwlock->can_read);
	free(rwlock);
	return NULL;
    }
    return rwlock;
}

void
empth_rwlock_destroy(empth_rwlock_t *rwlock)
{
    if (CANT_HAPPEN(rwlock->nread || rwlock->nwrite))
	return;
    CloseHandle(rwlock->can_read);
    CloseHandle(rwlock->can_write);
    free(rwlock);
}

void
empth_rwlock_wrlock(empth_rwlock_t *rwlock)
{
    /* block any new readers */
    ResetEvent(rwlock->can_read);
    rwlock->nwrite++;
    loc_BlockThisThread();
    loc_RunThisThread(rwlock->can_write);
    CANT_HAPPEN(rwlock->nread != 0);
}

void
empth_rwlock_rdlock(empth_rwlock_t *rwlock)
{
    loc_BlockThisThread();
    loc_RunThisThread(rwlock->can_read);
    ResetEvent(rwlock->can_write);
    rwlock->nread++;
}

void
empth_rwlock_unlock(empth_rwlock_t *rwlock)
{
    if (CANT_HAPPEN(!rwlock->nread && !rwlock->nwrite))
	return;
   if (rwlock->nread) { /* holding read lock */
	rwlock->nread--;
	if (rwlock->nread == 0)
	    SetEvent(rwlock->can_write);
    } else {
	rwlock->nwrite--;
	SetEvent(rwlock->can_write);
    }
    if (rwlock->nwrite == 0)
	SetEvent(rwlock->can_read);
}
