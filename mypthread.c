// File:	mypthread.c

// List all group members' names:
// Sanchay Kanade (sk2656)
// Tanya Sharma (tds104)
// iLab machine tested on: ilab4.cs.rutgers.edu

#include "mypthread.h"
#include <errno.h>
#include<ucontext.h>

/***************************************   Data Structures & Global Variables    ***************************************/

/* Multilevel running queue that stores threads as node and where each index indicates a priority level
Higher the index value, lower the priority of the thread. For scheduling algorithms not using multilevel queues, 
we use the 0th index to use just a single queue for scheduling*/
plevels* runningQueue = NULL;  

/*wait queue storing nodes waiting to acquire a mutex*/
queueType* waitingQueue = NULL; 

/*current node that is being scheduled*/
contextNode* current = NULL; 

/*join queue storing context of threads that have used join() on aother thread and are waiting for them to finish and return*/
queueType* joinQueue = NULL; 

/*storing details about the thread that is exiting*/
exitNode* exitList = NULL; 

/*status of the current thread, to handle different scheduling states, set to the initialthread which starts scheduling 
with the first thread being added to the running queue*/
states status = INITIALTHREAD; 

/*flag to indicate whether the current thread is the first thread of not*/
int firstThread = 1; 

/*we are periodically counting number of scheduling cycles to provide feedback and increase priority of low priority 
threads which have a tendency to be starved*/
int cycles = 0; 

/*timer variable used to set timer off and call scheduler after a fixed duration*/
struct itimerval itimer; 

/*stores no of running threads*/
uint threadCount = 1; 

/*stores no of mutexes currently engaged by threads*/
int mutexCount = 0; 

/*handling which scheduler is used for a particular program*/
enum SCHED_TYPE{RR, MLFQ, PSJF};
int TYPE = PSJF;

/***************************************   Thread Functions   ***************************************/

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{
	void (*exitFunction)(void*) = &mypthread_exit;
	ucontext_t* exitContext = (ucontext_t*)malloc(sizeof(ucontext_t)); 
	getcontext(exitContext);

	/*New Stack allocation*/
	exitContext->uc_link = 0;
	exitContext->uc_stack.ss_sp = malloc(MEM); 
	exitContext->uc_stack.ss_size = MEM; 
	exitContext->uc_stack.ss_flags = 0; 
	makecontext(exitContext, (void*)exitFunction, 1, NULL);

	/*storing the context of new and old thread and creating a new tcb*/
	ucontext_t* newContext, *oldContext;
	newContext = (ucontext_t*)malloc(sizeof(ucontext_t));
	getcontext(newContext); 
	tcb* newThreadBlock = (tcb*)malloc(sizeof(tcb));  
	contextNode* newNode = (contextNode*)malloc(sizeof(contextNode));

	/*handling the creation of the first thread or any subsequent threads*/
	if (firstThread) {

		createFirstThread( newContext, oldContext, exitContext, function, arg, newThreadBlock,newNode);
	}
	else {

		createNewThread(newContext, exitContext, function, arg, newThreadBlock,newNode);
	}
	
	/*increase thread count and call scheduler*/
	*thread = threadCount; 
	threadCount++;
	schedule();
	

	return 0;
};

/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield()
{	
	/*if there are no subsequent threads in the queue after this thread*/
	if (runningQueue->rqs[current->threadBlock->tPriority]->front->next == NULL) {

		return 0;
	}

	/*if there are other threads in the running queue, set status to YIELD and call scheduler to yield to the next thread*/
	else {

		status = YIELD;
		schedule();
	}

	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr)
{
	int i;
	/*initializing the exit node*/
	exitNode* eNode = (exitNode*)malloc(sizeof(exitNode));
	eNode->next = NULL;
	eNode->tID = current->threadBlock->tID;
	eNode->value_ptr = value_ptr;
	contextNode* temp = NULL;

    /*Set the status flag to exit when a thread is exiting*/
	status = EXITING;
    

	for (i = 0; i < getThreadCount(joinQueue); i++) {

		/*if thread id of the thread waiting to be joined is the same as the current thread id*/
		temp = dequeue(joinQueue);
		if (temp->threadBlock->join_id == current->threadBlock->tID) {
			temp->threadBlock->value_ptr = value_ptr;
			temp->threadBlock->tPriority = 0;
			enqueue(temp, runningQueue->rqs[0]);
		}

		/*Exit and add the thread into join queue becasue it is waiting for some thread to finish */
		else {
			enqueue(temp, joinQueue);
		}
	}

    /*Saving the value of exiting node in exitList which of type exitNode*/
	if (exitList == NULL) {
		exitList = eNode;
	}

	else {
		eNode->next = exitList;
		exitList = eNode;
	}

	/*call scheduler to handle exit thread*/
	schedule();

	return;
};


/* Wait for thread termination 
 The calling thread waits for another thread to exit*/
int mypthread_join(mypthread_t thread, void **value_ptr)
{
	exitNode* temp = exitList;

	/*if thread does not exist*/
	if ((uint)thread > threadCount - 1) {
		return -1;
	}

	/*preserve value pointer*/
	while (temp != NULL) {

		if (temp->tID == thread) {

			if (value_ptr != NULL) {
				*value_ptr = temp->value_ptr;
			}
			return 0;
		}

		else {
			temp = temp->next;
		}
	}

	current->threadBlock->join_id = thread;

	status = JOINING;

	/*call scheduler to handle join*/
	schedule();

	if (value_ptr != NULL) {

		*value_ptr = current->threadBlock->value_ptr;
	}

	return 0;
};

void createFirstThread(ucontext_t* newContext, ucontext_t *oldContext, ucontext_t *exitContext, void *(*function)(void*),void * arg, tcb* newThreadBlock,contextNode* newNode ){

		/*setting first thread flag to false because we are handling the first thread*/
		firstThread = 0; 

		/*initializes running queue, waiting queue and join queue for the scheduler*/
		prepareQueuesForScheduler(); 

		/*creating a new thread using old context*/
		oldContext = (ucontext_t*)malloc(sizeof(ucontext_t));
		contextNode* oldNode = (contextNode*)malloc(sizeof(contextNode));
		tcb* oldThreadBlock = (tcb*)malloc(sizeof(tcb));
		getcontext(oldContext);

		/*creating new thread context and node*/
		newContext->uc_link = exitContext;  
		newContext->uc_stack.ss_sp = malloc(MEM);
		newContext->uc_stack.ss_size = MEM;
		newContext->uc_stack.ss_flags = 0;
		makecontext(newContext, (void*)function, 1, arg);

		newThreadBlock->tContext = newContext;
		newThreadBlock->tID = 1;  
		newThreadBlock->tPriority = 0;
		newThreadBlock->join_id = -1;
		newThreadBlock->value_ptr = NULL;
		newThreadBlock->mutexID = -1;

		newNode->threadBlock = newThreadBlock;
		newNode->next = NULL;

		enqueue(newNode, runningQueue->rqs[0]);

		/*Creating old thread context and old thread node*/
		oldThreadBlock->tContext = oldContext;
		oldThreadBlock->tID = 0;
		oldThreadBlock->tPriority = 0;
		oldThreadBlock->join_id = -1;
		oldThreadBlock->value_ptr = NULL;
		oldThreadBlock->mutexID = -1;

		oldNode->threadBlock = oldThreadBlock;
		oldNode->next = NULL;

		current = oldNode;

		/*pushing thread into the running/ready queue*/
		enqueue(oldNode, runningQueue->rqs[0]); 
}

void createNewThread(ucontext_t *newContext,ucontext_t *exitContext, void *(*function)(void *),void *arg,tcb *newThreadBlock, contextNode *newNode){

		/*creating a new thread and new context for the thread*/
		newContext->uc_link = exitContext;  
		newContext->uc_stack.ss_sp = malloc(MEM);
		newContext->uc_stack.ss_size = MEM;
		newContext->uc_stack.ss_flags = 0;
		makecontext(newContext, (void*)function, 1, arg);
		newThreadBlock->tContext = newContext;
		newThreadBlock->tID = threadCount;  //thread ID= thread count at the time the thread was created
		newThreadBlock->tPriority = 0;
		newThreadBlock->join_id = -1;
		newThreadBlock->value_ptr = NULL;
		newThreadBlock->mutexID = -1;

		newNode->threadBlock = newThreadBlock;
		newNode->next = NULL;

		/*pushinhg created thread to the running/ready queue*/
		enqueue(newNode, runningQueue->rqs[0]);
		newNode->threadBlock->timeQuantum = 1;
}

/***************************************   Methods for Mutex   ***************************************/

/* initialize the mutex lock */
/* receives a pointer to the mutex and a configuration mutex, returns 0 if it is possible to create the mutex*/
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	/*if the argument is invalid*/ 
	if(mutex == NULL)
		return EINVAL;
	
	/*initialising a new mutex*/
	mutex->lockedState=0; //set to unlocked
	mutex->tID=-1;
	mutex->isWaiting=0;
	mutex->mutexID=mutexCount++;

	/*return 0 on success*/
	return 0;
};

/* aquire a mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex)
{
		/*if argument is invalid*/
		if(mutex==NULL)
			return EINVAL;

		/*using atomic built in functions to test the mutex*/		
		while(__sync_lock_test_and_set(&(mutex->lockedState),1)){

			/*if mutex is already locked, the thread needs to be put on the waiting list, we set status as WAITING and call scheduler to update queue*/
			if(mutex->lockedState){
				status=WAITING;
				current->threadBlock->mutexID=mutex->mutexID;
				schedule();
			}	
		}

		/*once the thread acquires the lock*/
		mutex->lockedState=1; //already done 
		mutex->tID=current->threadBlock->tID;
		mutex->isWaiting=0;

		/*return 0 on success*/
		return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex)
{
	/*if argument is invalid*/
	if(mutex == NULL)
		return EINVAL;
	
	/*if mutex is already unlocked then we can't unlock it again*/
	if(mutex->lockedState==0)
		return EINVAL;

	/*if there is an access error on the mutex and the incorrect threads call unlock*/
	if(current->threadBlock->tID!=mutex->tID)
		return EINVAL;

	/*updating mutex metadata to indicate it is unlocked*/
	mutex->tID=-1; 
	mutex->lockedState=0;//unlocking the mutex
	contextNode *newNode=waitingQueue->front; 
	
	/*if there is another thread in the same queue, we switch to that and see if it is not null*/
	while(newNode!=NULL){

		/*if the correct thread has called unlock*/
		if(mutex->mutexID == newNode->threadBlock->mutexID){

			/*unlocking the mutex*/
			newNode->threadBlock->mutexID=-1;
			newNode->threadBlock->tPriority=0;

			/*mutex set to ready waiting true because we have a thread that wants to acquire the mutex*/
			mutex->isWaiting=1;

			/*putting the thread at the front of the mutex's waiting queue, to the running/ready queue*/
			enqueue(dequeue(waitingQueue),runningQueue->rqs[0]);

			/*return 0 on success*/
			return 0;
		}

		newNode=newNode->next;
	}
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex)
{
	/*if argument is invalid*/
	if(mutex == NULL)
		return EINVAL;

	/*cannot destroy a locked mutex*/
	if(mutex->lockedState==1)
		return EINVAL;
	
	/*if there is another thread waiting to acquire the mutex, we cannot destroy it*/
	if(mutex->isWaiting==1)
		return EINVAL;
	
	contextNode *destroyNode = waitingQueue->front;
	while(destroyNode!=NULL){

		/*if mutex is acquired by a thread*/
		if(mutex->mutexID==destroyNode->threadBlock->mutexID){
			return EINVAL;
		}

		destroyNode=destroyNode->next;
	}

	mutex=NULL;

	return 0;
};

/***************************************   Scheduler Functions   ***************************************/

/* scheduler */
void prepareQueuesForScheduler() {

	/*initialize runnning queue*/
	runningQueue = (plevels*)malloc(sizeof(plevels));
	for (int i = 0; i < LEVELS; i++) {
		runningQueue->rqs[i] = (queueType*)malloc(sizeof(queueType));
		runningQueue->rqs[i]->front = NULL;
		runningQueue->rqs[i]->back = NULL;
	}

	/*Initializing waiting queue for all threads waiting for mutex*/
	waitingQueue= (queueType*)malloc(sizeof(queueType));
	waitingQueue->front = NULL;
	waitingQueue->back = NULL;

	/*Initializing join queue for threads waiting for other threads to complete*/
	joinQueue = (queueType*)malloc(sizeof(queueType));
	joinQueue->front = NULL;
	joinQueue->back = NULL;

	/* Specifies the signal handler where to jump when SIGALARM is raised on the epiration of ITIMER_REAL*/
	signal(SIGALRM, timer_triggered);
}

/*Whenver ITIMER_REAL timer finishes, SIGALARM will be generated and the control will reach here and call schedule*/
void timer_triggered(int signum) {

	status = TIMER;  
	schedule();
}

 void schedule()
{   
	/*deploys scheduling algorithm based on the SCHED definition*/
	if(TYPE == MLFQ){
		sched_MLFQ();
	}

	if(TYPE == RR){
		sched_RR();
	}

	if(TYPE == PSJF){
		sched_PSJF();
	}

	sched_RR();
	return;
}

/* Round Robin scheduling algorithm */
void sched_RR()
{
	if(handleThreadStates()){

		/*do nothing because the only case that returns 1 is "NONE' (current thread)*/
	}

	else{

		contextNode* newContext;
		newContext = runningQueue->rqs[0]->front;

		/*if queueu is empty*/
		if(newContext==NULL){
			return;
		}
	
		if (status == EXITING) {

				status = NONE;
				current = newContext;
				setcontext(newContext->threadBlock->tContext);
		}

		else {

				contextNode* old = current;
				current = newContext;
				swapcontext(old->threadBlock->tContext, newContext->threadBlock->tContext);
		}

		/*time quantum for RR*/
		itimer.it_value.tv_usec = (6); 
		itimer.it_value.tv_sec = 0;
		itimer.it_interval = itimer.it_value;

		/*if there is a problem setting the timer*/
		if (setitimer(ITIMER_REAL, &itimer, NULL) == -1) {
			return;
		}
	}	
	return;
}

/* Preemptive PSJF (STCF) scheduling algorithm */
void sched_PSJF()
{

	if(handleThreadStates()){
		/*do nothing because the only case that returns 1 is "NONE' (current thread)*/
	}

	else{

		/*this thread got run time so reset waiting time to 0 and increase time quantum for it*/
		contextNode* newContext;
		newContext = runningQueue->rqs[0]->front;
		newContext->threadBlock->timeQuantum++;
		newContext->threadBlock->timeWaiting = 0 ;

		/*if queue is empty*/
		if(newContext==NULL){
			return;
		}

		/*All the other threads which are not running are waiting so increase thier waiting time*/
		contextNode* temp = newContext->next;
		while(temp->next != NULL) {

					temp->threadBlock->timeWaiting++;
					temp = temp->next;
				}

		if (status == EXITING) {

			status = NONE;
			current = newContext;
			
			/*dequeue the thread with minimum time quantum and enqueue to the front of the queue*/
			contextNode* minTimeQuantumThread;
			contextNode* temp = newContext->next;
			int minmumTQ = temp->threadBlock->timeQuantum;
			int maximumWT = temp->threadBlock->timeWaiting;

			/*finding thread to be put into the front*/
			while(temp->next != NULL){

				if(temp->threadBlock->timeQuantum<=minmumTQ){

					if(temp->threadBlock->timeWaiting <= maximumWT){
						minTimeQuantumThread = temp;
					}
				}

				temp = temp->next;
			}

			/*adding this to the next of exiting thread so that it runs next*/
			minTimeQuantumThread->next = newContext->next;
			newContext->next = minTimeQuantumThread;
			setcontext(minTimeQuantumThread->threadBlock->tContext);

		}
		else{

			contextNode* old = current;
			contextNode* minTimeQuantumThread;
			contextNode* temp = newContext->next;
			int minmumTQ = temp->threadBlock->timeQuantum;
			int maximumWT = temp->threadBlock->timeWaiting;

			/*finding thread to be put into the front of the wait queue*/
			while(temp->next != NULL){

				if(temp->threadBlock->timeQuantum<=minmumTQ){

					if(temp->threadBlock->timeWaiting <= maximumWT){
						minTimeQuantumThread = temp;
					}
				}

				temp = temp->next;
			}

			current = minTimeQuantumThread;
			swapcontext(old->threadBlock->tContext, current->threadBlock->tContext);
		}
		
	}	

	return;
}

/* Preemptive MLFQ scheduling algorithm */
/* Graduate Students Only */
void sched_MLFQ() {


	if (handleThreadStates()) {
		/*do nothing because the only case that returns 1 is "NONE' (current thread)*/
	}

	else {

		/*each time MLFQ is invoked, we increase the counter for cycles to track aging threads*/
		cycles++;
		int i;
		contextNode* newContext;

		/*checking for threads to schedule in the order of priority*/
		for (i = 0; i <LEVELS ; i++) {

			newContext = runningQueue->rqs[i]->front;

			if (newContext != NULL) {
				break;
			}
		}

		/*If the loop exits naturally witgout braking all the queues have null values which means there is nothing to schedule*/
		if (newContext == NULL) {
			return;
		}

		if (current != NULL) {

			/*The node with the highest priority is the same as the current running thread*/
			if (newContext->threadBlock->tID == current->threadBlock->tID) {

				/*Both are same task so move to next task*/
				if (newContext->next != NULL) {
					newContext = newContext->next;
				}
				
				/*check all the priority queue to find thread to run*/
				else {

					for (i = i + 1; i < LEVELS; i++) {

						newContext = runningQueue->rqs[i]->front;

						if (newContext != NULL) {
							break;
						}
					}
				}

				/*Move the front node of the running queue which is also a current node to the back of the queue but */
				enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority]), runningQueue->rqs[current->threadBlock->tPriority]);
				if (newContext == NULL) {
					newContext = current;
				}
			}
		}

		/*each level of the running queue is assigned a time quantum based on the priority and level*/
		int t = newContext->threadBlock->tPriority;
		itimer.it_value.tv_usec = (6 + 6 * t)*100; 
		itimer.it_value.tv_sec = 0;
		itimer.it_interval = itimer.it_value;

		/*if there is an error in setting the timer*/
		if (setitimer(ITIMER_REAL, &itimer, NULL) == -1) {
			return;
		}

		/*if the current thread is exiting, set context of the next thread*/
		if (status == EXITING) {
			status = NONE;
			current = newContext;
			setcontext(newContext->threadBlock->tContext);
		}

		/*if thread has not finished execution, swap out its context*/
		else {
			contextNode* old = current;
			current = newContext;
			swapcontext(old->threadBlock->tContext, newContext->threadBlock->tContext);
		}

	}

	return;
}

/***************************************   Helper Functions   ***************************************/

/*Function to handle queue enqueue logic*/
void enqueue(contextNode* enterThread, queueType* Q) {

	/*if the thread is the first node in the queue*/
	if (Q->front == NULL) {

		Q->front = enterThread;
		Q->back = enterThread;
		enterThread->next = NULL;
	}

	/*every subsequent thread*/
	else {

		Q->back->next = enterThread;
		Q->back = enterThread;
		Q->back->next = NULL;
	}
}

/*function to handle queue dequeue logic*/
contextNode* dequeue(queueType* Q) {

	contextNode* temp;

	/*if the queue is empty*/
	if (Q->front == NULL) {

		return NULL;
	}

	else {

		temp = Q->front;
		Q->front = temp->next;
	}

	return temp;
}

/*returns the numbers of threads waiting in the queue*/
int getThreadCount(queueType* Q) {

	int count = 0;
	contextNode* temp = Q->front;

	while (temp != NULL) {

		count++;
		temp = temp->next;
	}
	return count;

}

/*Freeing up used up context so that memory frees up*/
void freeContext(contextNode* freeable) {

	free(freeable->threadBlock->tContext->uc_stack.ss_sp);
	free(freeable->threadBlock->tContext);
	free(freeable->threadBlock);
	free(freeable);

}

/*This function handles thread states*/
int handleThreadStates() {

	if(status==NONE)
		return 1;

	if(status==INITIALTHREAD)
		return 0;

	if(status==TIMER){

		if (current->threadBlock->tPriority < LEVELS - 1 && TYPE != RR && TYPE != PSJF) {

			current->threadBlock->tPriority++;
			enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority - 1]), runningQueue->rqs[current->threadBlock->tPriority]);
		}

		else {
			enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority]), runningQueue->rqs[current->threadBlock->tPriority]);
		}

		status = NONE;
	}

	if(status==YIELD){

		enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority]), runningQueue->rqs[current->threadBlock->tPriority]);
		status = NONE;
	}

	if(status==EXITING){

		dequeue(runningQueue->rqs[current->threadBlock->tPriority]);

		if (current->threadBlock->tID > 0) {
			freeContext(current);
		}

		current = NULL;
	}

	if(status==JOINING){

		enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority]), joinQueue);
		status = NONE;
	}

	if(status==WAITING){

		enqueue(dequeue(runningQueue->rqs[current->threadBlock->tPriority]), waitingQueue);
		status = NONE;
	}

	preventThreadStarvation();

	return 0;
}


//move oldest (lowest priority) threads to the end of the highest priority queueType
void preventThreadStarvation(){

	/*Count of cycles  */
	if (cycles > CYCLE_COUNT) {

		cycles = 0;
		int flag=0;

		queueType* top = runningQueue->rqs[0];
		queueType* bottom = runningQueue->rqs[LEVELS - 1];
		contextNode* temp;

		if(bottom->front!=NULL && top->front!=NULL){

				top->back->next = bottom->front;
				top->back = bottom->back;
				top->front = NULL;
				top->back = NULL;
				flag=1;
		}

		if(bottom->front!=NULL && top->front==NULL){
				
				top->front = bottom->front;
				top->back = bottom->back;
				top->front = NULL;
				top->back = NULL;
				flag=1;
		}

		if(flag){

			temp = top->front;
			while (temp != NULL) {

				temp->threadBlock->tPriority = 0;
				temp = temp->next;
			}

		}
	}
}

			
	
