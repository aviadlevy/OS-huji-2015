#include "uthreads.h"
#include "Thread.h"
#include <vector>
#include <queue>
#include <stdlib.h>
#include <map>
#include <algorithm>

using namespace std;

#define NUM_OF_QUEUE 3
#define RED_Q 0
#define ORANGE_Q 1
#define GREEN_Q 2
#define FAIL -1
#define SUCCESS 0
#define MAIN_THREAD 0
#define MICRO_TO_SECOND 1000000
#define THREAD_ERROR "thread library error: "
#define SYS_ERROR "system error: "
#define NOT_FOUND_ID 0
#define SUSPEND_MAIN 1
#define SET_TIME_ERROR 2
#define WRONG_INPUT 3
#define SIGNAL_ACTION_ERROR 4
#define TOO_MANY_THREADS 5

static vector<Thread*> ready[NUM_OF_QUEUE]; // An array of size 3 of queues, which each represents a queue of priority.
static vector<Thread*> blocked; // The "Blocked" vector, which represents a queue of threads.
static Thread* running; // The "Running" thread.
static map<int, Thread*> _threads; // All threads together
static int _quantum_counter = 0;
struct itimerval _timer;
struct sigaction _sigAction;
int* sig;


int removeFromReady(int tid);
Thread* popReady();

/**
 * function responsable for printing each kind of error
 */
void printError(int type, string pre)
{
	switch (type)
	{
	case NOT_FOUND_ID:
	{
		cerr << pre << "the requested tid not found" << endl;
		break;
	}
	case SUSPEND_MAIN:
	{
		cerr << pre << "You are trying to suspend the main thread" << endl;
		break;
	}
	case SET_TIME_ERROR:
	{
		cerr << pre << "Unable to set timer" << endl;
		break;
	}
	case WRONG_INPUT:
	{
		cerr << pre << "wrong input" << endl;
		break;
	}
	case SIGNAL_ACTION_ERROR:
	{
		cerr << pre << "signal action error" << endl;
		break;
	}
	case TOO_MANY_THREADS:
	{
		cerr << pre << "too many threads" << endl;
		break;
	}
	default:
		break;
	}
}


/*
 * This function finds & returns the next smallest nonnegative integer not already taken by an existing thread,
 * or -1 if there are no available free ids.
 */
int getNextId()
{
	int idToReturn = 0;
	for (map<int, Thread*>::iterator iter = _threads.begin(); iter != _threads.end();
			idToReturn++, iter++)
	{
		if (iter->first != idToReturn)
		{
			break;
		}
	}
	return idToReturn;
}

/**
 * add thread to the requsted ready queue
 */
void addToReady(Thread* th)
{
	if(th == nullptr)
	{
		return;
	}
	switch (th->getPriority())
	{
	case RED:
	{
		ready[RED_Q].insert(ready[RED_Q].end(), th);
		break;
	}
	case ORANGE:
	{
		ready[ORANGE_Q].insert(ready[ORANGE_Q].end(), th);
		break;
	}
	case GREEN:
	{
		ready[GREEN_Q].insert(ready[GREEN_Q].end(), th);
		break;
	}
	}
}


/**
 * set time and check if set is done correctly
 */
static void setTime()
{
	if (setitimer(ITIMER_VIRTUAL, &_timer, NULL) == FAIL)
	{
		printError(SET_TIME_ERROR, SYS_ERROR);
		exit(1);
	}
}

/**
 * runs the given thread.
 */
void runThread()
{
	Thread* tmp = popReady();
	if(tmp != nullptr)   //that's mean that not only one thread can run now!
	{
		running = tmp;
	}
	running->setState(RUNNING);
	running->increaseQuantom();
	_quantum_counter++;
	setTime();
}

/*
 * returns and remove from Ready the first thread in the queue of the given priority.
 */
Thread* getReady(int pr)
{
	Thread* ret = ready[pr][0];
	ready[pr].erase(ready[pr].begin());
	return ret;
}

/*
 * returns the thread with the highest priority from Ready and removes it from there.
 * returns NULL in case there are no threads in Ready.
 */
Thread* popReady()
{
	if (_threads.size() > 1) // An additional thread except the thread in the Running state.
	{
		if (!ready[RED_Q].empty())
		{
			return getReady(RED_Q);
		}
		if (!ready[ORANGE_Q].empty())
		{
			return getReady(ORANGE_Q);
		}
		if (!ready[GREEN_Q].empty())
		{
			return getReady(GREEN_Q);
		}
	}
	else if(_threads.size() == 1)
	{
		return _threads.begin()->second;
	}
	return nullptr;
}

/*
 * Translates the priority of the given tid from Enum to int.
 * Returns the int value of the priority.
 */
int translatePriority(int tid)
{
	return (_threads.at(tid)->getPriority() == RED) ? 0 : ((_threads.at(tid)->getPriority() == ORANGE_Q) ? 1 : 2);
}

/*
 * removes the thread with the given tid from ready.
 */
int removeFromReady(int tid)
{
	int pr = translatePriority(tid);
	if(find(ready[pr].begin(), ready[pr].end(), _threads[tid]) != ready[pr].end())
	{
		for (vector<Thread*>::iterator iter = ready[pr].begin(); iter != ready[pr].end(); ++iter)
		{
			if (*iter == _threads.at(tid))
			{
				ready[pr].erase(iter);
				return SUCCESS;
			}
		}
	}
	return FAIL;
}


/*
 * removes the thread with the given tid from blocked.
 */
void removeFromBlock(int tid)
{
	for (vector<Thread*>::iterator iter = blocked.begin(); iter != blocked.end(); ++iter)
	{
		if (*iter == _threads.at(tid))
		{
			blocked.erase(iter);
			break;
		}
	}
}

/**
 * switch between running thread and the next in line
 */
void switchThreads()
{
	Thread *tmp = running;
	if(tmp != nullptr)   // if sent from suspend, so running is nullptr
	{
		tmp->setState(READY);
	}
	runThread();
	// if there are x threads in _threads and x + 1 threads in blocked, than it means that there is nothing
	// in ready, so the current running thread should not get into ready and keep running.
	if(_threads.size() - 1 != blocked.size())
	{
		addToReady(tmp);
	}
}

/**
 * switch between running thread and the this thread
 */
static void timeHandler(int signum)
{
	if (!running->saveBuf())
	{
		switchThreads();
		running->loadBuf();
	}
}

/*=================================================================================================
 * ======================================Library Functions=========================================
 * ================================================================================================
 */


/* Initialize the thread library */
int uthread_init(int quantum_usecs)
{
	if (quantum_usecs <= 0)
	{
		printError(WRONG_INPUT ,THREAD_ERROR);
		return FAIL;
	}

	//initialize sigaction
	_sigAction.sa_handler = timeHandler;
	if(sigemptyset (&_sigAction.sa_mask) == FAIL)
	{
		printError(SIGNAL_ACTION_ERROR, SYS_ERROR);
		exit(1);
	}
	if(sigaddset(&_sigAction.sa_mask, SIGVTALRM))
	{
		printError(SIGNAL_ACTION_ERROR, SYS_ERROR);
		exit(1);
	}
	_sigAction.sa_flags = 0;
	if(sigaction(SIGVTALRM,&_sigAction,NULL) == FAIL)
	{
		printError(SIGNAL_ACTION_ERROR, SYS_ERROR);
		exit(1);
	}

	//initialize timer
	_timer.it_value.tv_sec = (int)(quantum_usecs/MICRO_TO_SECOND);
	_timer.it_value.tv_usec = quantum_usecs % MICRO_TO_SECOND;
	_timer.it_interval.tv_sec = (int)(quantum_usecs/MICRO_TO_SECOND);
	_timer.it_interval.tv_usec = quantum_usecs % MICRO_TO_SECOND;

	//initialize main
	Thread* mainTh = new Thread(0, ORANGE, nullptr, READY);
	_threads.insert(pair<int, Thread*>(0, mainTh));
	running = mainTh;
	mainTh->setState(RUNNING);
	mainTh->increaseQuantom();
	_quantum_counter++;
	setTime();
	return SUCCESS;
}

/* Create a new thread whose entry point is f */
int uthread_spawn(void (* f)(void), Priority pr)
{
	//can't add any more!!
	if(_threads.size() == 100)
	{
		printError(TOO_MANY_THREADS, THREAD_ERROR);
		return FAIL;
	}

	sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);

	int tid = getNextId();
	Thread* th = new Thread(tid, pr, f, READY);
	_threads.insert(pair<int, Thread*>(tid, th));
	if (running == NULL)  //for the first running (the main thread)
	{
		runThread();
	}
	else
	{
		addToReady(th);
	}
	sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
	return tid;
}

/* Terminates a thread */
int uthread_terminate(int tid)
{
	//terminate main
	if (tid == MAIN_THREAD)
	{
		_threads.clear();
		exit(0);
	}

	//not found id
	if (!_threads.count(tid))
	{
		printError(NOT_FOUND_ID, THREAD_ERROR);
		return FAIL;
	}

	sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);
	//running terminate itself
	if (tid == running->getId())
	{
		Thread* temp = running;
		switchThreads();
		removeFromReady(tid);
		removeFromBlock(tid);
		_threads.erase(temp->getId());
		running->loadBuf();
	}
	// The termination is for a thread that in ready.
	if (_threads.at(tid)->getState() == READY)
	{
		removeFromReady(tid);
		_threads.erase(tid);
	}
	else // Thread is in Block.
	{
		removeFromBlock(tid);
		_threads.erase(tid);
	}
	sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
	return SUCCESS;
}

/* Suspend a thread */
int uthread_suspend(int tid)
{
	if (tid == MAIN_THREAD)
	{
		printError(SUSPEND_MAIN, THREAD_ERROR);
		return FAIL;
	}
	if (!_threads.count(tid))
	{
		printError(NOT_FOUND_ID, THREAD_ERROR);
		return FAIL;
	}
	sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);
	if (tid == running->getId())
	{
		running->saveBuf();
		running->setState(BLOCK);
		blocked.push_back(running);
		running = nullptr;
		runThread();
		running->loadBuf();
	}
	else
	{
		if(removeFromReady(_threads[tid]->getId()) == FAIL)  // Exists, not in ready and not running - in blocked.
		{
			sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
			return FAIL;
		}
		// Was in Ready
		_threads[tid]->setState(BLOCK);
		blocked.push_back(_threads[tid]);
	}
	sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
	return SUCCESS;
}

/* Resume a thread */
int uthread_resume(int tid)
{
	if(!_threads.count(tid))
	{
		printError(NOT_FOUND_ID, THREAD_ERROR);
		return FAIL;
	}
	//if not in block, don't resume
	if(find(blocked.begin(), blocked.end(), _threads[tid]) != blocked.end())
	{
		sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);
		int pos = find(blocked.begin(), blocked.end(), _threads[tid]) - blocked.begin();
		Thread* th = blocked.at((unsigned int) pos);
		th->setState(READY);
		addToReady(th);
		removeFromBlock(tid);
		sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
		return SUCCESS;
	}
	else
	{
		return FAIL;
	}
}

/* Get the id of the calling thread */
int uthread_get_tid()
{
	return running->getId();
}

/* Get the total number of library quantums */
int uthread_get_total_quantums()
{
	return _quantum_counter;
}

/* Get the number of thread quantums */
int uthread_get_quantums(int tid)
{
	sigprocmask(SIG_BLOCK,&_sigAction.sa_mask, NULL);
	int valToRet;
	if (!_threads.count(tid))
	{
		printError(NOT_FOUND_ID, THREAD_ERROR);
		valToRet = FAIL;
	}
	else
	{
		valToRet = _threads.at(tid)->getQuantom();
	}
	sigprocmask(SIG_UNBLOCK,&_sigAction.sa_mask, NULL);
	return valToRet;
}

/* ====================================================================================================
 * =========================================FOR TESTING================================================
 * ====================================================================================================*/

