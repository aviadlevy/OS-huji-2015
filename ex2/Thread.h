/*
 *
 */
#ifndef THREAD_H
#define THREAD_H

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <iostream>
#include "uthreads.h"

enum State {READY, RUNNING, BLOCK};

/*
 * The thread
 */
class Thread
{

public:
	/**
	 * constructor for Thread.
	 * @param tid id for the new thread
	 * @param pr priority for the new thread
	 * @param f the thread function that get no args and return nothing
	 * @param state current state for the new thread
	 */
	Thread(int tid, Priority pr, void (*f)(void), State state);
	
	/**
	 * thread d-tor
	 */
	~Thread();

	/**
	 * function to set the thread state
	 * @param state the new state for our thread
	 */
	void setState(State state);
	
	/**
	 * function that get the state of the thread
	 * @return the current state of the thread
	 */
	State getState() const;
	
	/**
	 * function that get the ID of the thread
	 * @return the ID of the thread 
	 */
	int getId() const;
	
	/**
	 * function that get the priority of the thread
	 * @return the priority of the thread
	 */
	Priority getPriority() const;
	
	/**
	 * function to increase the quantom of the thread
	 */
	void increaseQuantom();
	
	/**
	 * function that get the quantom of the thread
	 * @return the current quantom of the thread
	 */
	 int getQuantom() const;

	/**
	 * function that get the jump buffer of the thread
	 * @return the current jump buffer of the thread
	 */
	sigjmp_buf& getJmpBuf();

	/**
	 *
	 */
	int saveBuf();

	/**
	 *
	 */
	void loadBuf();

private:
	int _tid;  // The thread id number.
	Priority _pr;  // The priority of the thread (Red, orange or green)
	int _quantom;  // The time interval, as explained in the pdf.
	void (*_f)(void);  // function that connected to the thread
	State _state;   // The state of the thread
	char* _stack;
	sigjmp_buf _jmp_buf;  //save and load system status
};


#endif /* THREAD_H */
