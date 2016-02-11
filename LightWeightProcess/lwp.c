/* Light Weight Process controlling threads
 author: Anh Nguyen and Jun Trinh
 CPE453 - Winter 2016 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "lwp.h"

#define FAILURE -1

void initStructure();
void shutdownStructure();
void admitThreadToPool(thread new);
void removeThreadFromPool(thread victim);
void addThreadToAllThreadList(thread new);
thread nextThread();

/* count total number of thread */
static int countThread = NO_THREAD;
/* For tracking the list of thread, and the current running thread */
static thread allThreadList = NULL;
/*Current running thread*/
static thread currentRunningThread = NULL;
/* To store original registers */
rfile originalRegisters;

/* The schedule to run */
static struct scheduler mySchedule =
{NULL, NULL, admitThreadToPool, removeThreadFromPool, nextThread};

thread scheduledThreadList = NULL;

/**Process**/
tid_t lwp_create(lwpfun function,void *arguments,size_t stacksize) {
	unsigned long *tempPtr;
	
	/* allocate space for new thread, return failure in unsuccess malloc */
	thread newthread = calloc(sizeof(context), 1);
	if (newthread == NULL) {
		return FAILURE;
	}
	
	/* Allocate a stack for this new thread */
	tempPtr = calloc(stacksize, sizeof(long));
	if (tempPtr == NULL) {
		return FAILURE;
	}
	/* Set new thread's fields */
	newthread->tid = ++countThread;
	newthread->stacksize = stacksize * sizeof(long);
	newthread->state.fxsave = FPU_INIT;
	newthread->stack = tempPtr;
	
	/* Set up the stack */
	tempPtr = newthread->stack + stacksize;
	*(--tempPtr) = (unsigned long)lwp_exit;
	*(--tempPtr) = (unsigned long)function;
	--tempPtr;
	newthread->state.rbp = (unsigned long)tempPtr;
	newthread->state.rdi = (unsigned long)arguments;
	
	/* Add this thread to global list of all thread */
	addThreadToAllThreadList(<#thread new#>);
	
	/* Add this thread to schedule */
	mySchedule.admit(newthread);
	
	return newthread->tid;
}

void  lwp_start() {
	/* Save context */
	save_context(&originalRegisters);
	/* Pick a thread to run */
	currentRunningThread = mySchedule.next();
	
	if (currentRunningThread) {
		/* Load context to run */
		load_context(&currentRunningThread->state);
	}
}

tid_t lwp_gettid() {
	return currentRunningThread == NULL ?
	NO_THREAD : currentRunningThread->tid;
}

void  lwp_yield() {
	thread oldThread = currentRunningThread;
	thread toYield;
	if ((toYield = mySchedule.next()))//there are nextThread
	{
		//pick the thread
		currentRunningThread = toYield;
		swap_rfiles(&oldThread->state, &currentRunningThread->state);
	}
	else {
		lwp_stop();
		//load_context(&originalRegisters);
	}
	
}

void  lwp_exit() {
	/*thread temp = currentRunningThread;
	mySchedule.remove(currentRunningThread);
	thread temp2;
	currentRunningThread = NULL;
	
	if((temp2 = mySchedule.next()))
	{
		currentRunningThread = temp2;
		swap_rfiles(&temp->state, &currentRunningThread->state);
		//free
		free(temp->stack);
		free(temp);
	}
	// Get nextThread thread
	else {
		lwp_stop();
	}*/
	thread temp = currentRunningThread;
	thread temp2;
	
	mySchedule.remove(currentRunningThread);
	currentRunningThread = NULL;
	
	if((temp2 = mySchedule.next())) {
		//free
		free(temp->stack);
		free(temp);
		
		currentRunningThread = temp2;
		load_context(&currentRunningThread->state);
		
	}
	// Get nextThread thread
	else {
		lwp_stop();
	}
	
	
}

void  lwp_stop() {
	if(currentRunningThread)
	{
		save_context(&currentRunningThread->state);
	}
	load_context((&originalRegisters));
}

void  lwp_set_scheduler(scheduler fun) {
	thread temp;
	
	if (fun) {
		/* call init if the passed in schedule has an init function */
		if(fun->init) {
			fun->init();
		}
		/* moving all existing thread on current
		 schedule to the end of the new schedule */
		temp = scheduledThreadList = mySchedule.next();
		while (temp) {
			mySchedule.remove(temp);
			fun->admit(temp);
			temp = mySchedule.next();
		}
		
		/* call shutdown if the passed in schedule has an shutdown function */
		if (mySchedule.shutdown) {
			mySchedule.shutdown();
		}
		
		mySchedule = *fun;
	}
}

scheduler lwp_get_scheduler() {
	return &mySchedule;
}

thread tid2thread(tid_t tid) {
	thread temp = allThreadList;
	/* obvious case of invalid thread ID */
	if(!tid || tid > countThread) {
		return NULL;
	}
	
	/* traverse the list of thread */
	while (temp != NULL) {
		/* found the thread with given tid */
		if (temp->tid == tid) {
			return temp;
		}
		temp = temp->lib_two;
	}
	/* not found, return null */
	return NULL;
}

void initStructure() {
	/* no implementation needed */
}

void shutdownStructure() {
	/* taken care from exit */
}

/* add the new thread to the pool of scheduled thread, 
 which has structure of a double circular linked list */
void admitThreadToPool(thread new) {
	/* general case */
	if (scheduledThreadList != NULL)
	{
		new->sched_two = scheduledThreadList;
		new->sched_one = scheduledThreadList->sched_one;
		scheduledThreadList->sched_one->sched_two = new;
		scheduledThreadList->sched_one = new;
	}
	/* this is the first thread */
	else {
		scheduledThreadList = new;
		scheduledThreadList->sched_two = new;
		scheduledThreadList->sched_one = new;
	}
}

/* remove the thread victim from the pool of scheduled thread, 
 which has structure of a double circular linked list */
void removeThreadFromPool(thread victim) {
	thread temp = scheduledThreadList;
	/* when the victim is in front of the scheduled list */
	if (victim == scheduledThreadList) {
		/* There still some thread left in the pool */
		if (victim != scheduledThreadList->sched_two) {
			scheduledThreadList->sched_two->sched_one = victim->sched_one;
			scheduledThreadList->sched_one->sched_two = victim->sched_two;
			scheduledThreadList = scheduledThreadList->sched_two;
		}
		/* victim was the only thread on the list */
		else {
			scheduledThreadList = NULL;
		}
	}
	/* general case, victim is in middle of list */
	else {
		/* look for victim incase victim was invalid thread in my schedule */
		while (victim != temp) {
			temp = temp->sched_two;
		}
		temp->sched_two->sched_one = temp->sched_one;
		temp->sched_one->sched_two = temp->sched_two;
	}
}

thread nextThread(){
	thread toReturn;
	
	if (scheduledThreadList == NULL)
		toReturn = NO_THREAD;
	else if (currentRunningThread == NULL)
		toReturn = scheduledThreadList;
	else
		toReturn = currentRunningThread->sched_two;
	
	return toReturn;
}

void addThreadToAllThreadList(thread new){
	if (allThreadList) {
		new->lib_two = allThreadList;
		allThreadList->lib_one = new;
		new->lib_one = NULL;
	}
	else
	{
		allThreadList = new;
	}
}


