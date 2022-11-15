// File:	mypthread_t.h

// List all group members' names:
// Sanchay Kanade (sk2656)
// Tanya Sharma (tds104)
// iLab machine tested on: ilab4.cs.rutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* in order to use the built-in Linux pthread library as a control for benchmarking, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1
#define MAX_THREADS 100
#define CYCLE_COUNT 200
#define MEM 64000
#define LEVELS 10

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

/*using for u_context to store user context*/
#include <ucontext.h>

/*for implementing timer so that our schduler can be called after a duration, and for using ITIMER_VIRTUAL which counts down against the user-mode CPU time
consumed by the process.  (The measurement includes CPU time consumed by all threads in the process.)*/
#include <sys/time.h> 

/*for using SIG signals and handler*/
#include <signal.h>

/*****************************Data Structures*****************************/

//unsigned integer type for storing tID
typedef uint mypthread_t; 

/* add important states in a thread control block */
typedef struct threadControlBlock
{
	/*thread Id*/
	mypthread_t tID;  

	/*thread status*/
	//states tState; //stores the status of the thread

	/*thread contex:struct type suitable for holding the context for a user thread of execution*/
	ucontext_t* tContext; 

	/*thread priority*/
	int tPriority; 

	/* thread join ID:contains id of the thread this thread is waiting for to finish so that this can start executing*/
	mypthread_t join_id; 

	void* value_ptr; 

	/*mutex ID held by the thread*/
	int mutexID; 

	/*for shortest job first scheduling*/
	int timeQuantum;

	/*Time passed since the thread arrived. It will reset when it gets runtime*/ 
	int timeWaiting;

} tcb;

/*structure to store context of the thread*/
typedef struct contextNode {
	
	/*block of thread whose context need to be stored*/
	tcb* threadBlock; 

	/*stores address of next thread*/
	struct contextNode* next; 

}contextNode;

/* States that the threa dmay encounter, to be used to update the thread's position in the wait, join or running queue*/
typedef enum { NONE, TIMER, YIELD, EXITING, JOINING, INITIALTHREAD, WAITING } states;

/* mutex struct definition */
typedef struct mypthread_mutex_t
{

	/*ID of the thread that holds the mutex*/
	mypthread_t tID;

	/*to store status of the lock 0(unlocked) or 1(locked)*/
	int lockedState; 

	//states tStates; rateher than using int we can use our enum

	/*flag to check if a thread is waiting for the mutex to be freed*/
	int isWaiting;  

	/*ID of the current mutex*/
	int mutexID;  

} mypthread_mutex_t;



typedef struct exitNode {

	/*thread ID of the exit node*/
	mypthread_t tID;

	void *value_ptr; 

	/*pointer to next node*/
	struct exitNode* next; 

}exitNode;

/*Queue data structure to store threads*/
typedef struct queueType {

	/*store head of the queue*/
	contextNode* front;

	/*stored tail of the queue*/
	contextNode* back; 
	
}queueType;


/*array of queues, 10 levels of priority*/
typedef struct plevels {

	queueType* rqs[100];  

}plevels;




/************************ Function Declarations: *************************/

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initialize a mutex */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire a mutex (lock) */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release a mutex (unlock) */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy a mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

void timer_triggered(int signum);

contextNode* dequeue(queueType* Q);

void schedule();

void enqueue(contextNode* enterThread, queueType* Q);

void prepareQueuesForScheduler();

void preventThreadStarvation();


#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
