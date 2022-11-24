#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>                                                                              
#include "YAUThreads.h"

threaddesc threadarr[MAX_THREADS];
int numthreads, curthread;
ucontext_t parent;


void initYAUThreads()
{
	numthreads = 0;
	curthread = 0;
}


int YAUSpawn( void (threadfunc)() )
{
	threaddesc *tdescptr;

	if (numthreads >= MAX_THREADS) 
	{
		printf("FATAL: Maximum thread limit reached... creation failed! \n");
		return -1;
	}

	tdescptr = &(threadarr[numthreads]);
	getcontext(&(tdescptr->threadcontext));
	tdescptr->threadid = numthreads;
	tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
	tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
	tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
	tdescptr->threadcontext.uc_link = 0;
	tdescptr->threadcontext.uc_stack.ss_flags = 0;
	tdescptr->threadfunc = threadfunc;

	makecontext(&(tdescptr->threadcontext), threadfunc, 1, tdescptr);

	numthreads++;

	return 0;
}


void handle_timerexpiry() 
{
    struct sigaction handler;
	int nxtthread, curthreadsave;
	
    handler.sa_handler = handle_timerexpiry;
    sigaction(SIGALRM, &handler, NULL);
    alarm(RR_QUANTUM);

	nxtthread = (curthread +1) % numthreads;

	curthreadsave = curthread;
	curthread = nxtthread;
	swapcontext(&(threadarr[curthreadsave].threadcontext),
		    &(threadarr[nxtthread].threadcontext));
}
 

void startYAUThreads(int sched)
{
	struct sigaction handler;

	if (sched == RR && numthreads > 0)
	{
		handler.sa_handler = handle_timerexpiry;
		sigaction(SIGALRM, &handler, NULL);
		alarm(RR_QUANTUM);

		swapcontext(&parent, &(threadarr[curthread].threadcontext));
	}	
}


int getYAUThreadid(threaddesc *th)
{
	return th->threadid;
}

		
	
