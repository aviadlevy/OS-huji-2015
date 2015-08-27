/*
 *
 */

#include "Thread.h"
#define JUMP_VAL 1

#ifdef __x86_64__


typedef unsigned long address_t;
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
			"rol    $0x11,%0\n"
			: "=g" (ret)
			  : "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%gs:0x18,%0\n"
			"rol    $0x9,%0\n"
			: "=g" (ret)
			  : "0" (addr));
	return ret;
}

#endif


sigjmp_buf env[3];

Thread::Thread(int tid, Priority pr, void (*f)(void), State state): _tid(tid), _pr(pr), _f(f), _state(state)
{
	_quantom = 0;
	_stack = new char[STACK_SIZE];
	address_t sp, pc;
	sp = (address_t)_stack + STACK_SIZE - sizeof(address_t);
	pc = (address_t)*f;
	sigsetjmp(_jmp_buf, JUMP_VAL);
	(_jmp_buf->__jmpbuf)[JB_SP] = translate_address(sp);
	(_jmp_buf->__jmpbuf)[JB_PC] = translate_address(pc);
	sigemptyset(&_jmp_buf->__saved_mask);
}

Thread::~Thread()
{
	delete[] _stack;
}

void Thread::setState(State state)
{
	_state = state;
}

State Thread::getState() const
{
	return _state;
}

int Thread::getId() const
{
	return _tid;
}

Priority Thread::getPriority() const
{
	return _pr;
}

void Thread::increaseQuantom()
{
	_quantom++;
}

int Thread::getQuantom() const
{
	return _quantom;
}

sigjmp_buf& Thread::getJmpBuf()
{
	return _jmp_buf;
}

int Thread::saveBuf()
{
	int ret_val = sigsetjmp(_jmp_buf, JUMP_VAL);
	return ret_val;
}

void Thread::loadBuf()
{
	siglongjmp(_jmp_buf, JUMP_VAL);
}
