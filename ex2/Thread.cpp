#include "Thread.h"
#include <iostream>
#include <signal.h>


#define BAD_ALLOC "system error: bad memory allocation"
#define DEFAULT_RUN_TIME 0
#define MAIN_THREAD_ID 0
#define JB_SP 6
#define JB_PC 7
using namespace std;

//-------------------black box-------------------------------------
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}
//-----------------------------------------------------------------


/**
 * a constructor for the thread
 * @param t_id the thread's unique id
 * @param stack_size the size of the stack
 * @param thread_func the function that the thread is doing
 * @param t_state the thread's state
 */
Thread::Thread(int t_id, int stack_size, thread_entry_point thread_func, int t_state) {
  stack = new(nothrow) char[stack_size];
  if (stack == nullptr) {
    cerr << BAD_ALLOC << endl;
    exit(EXIT_FAILURE);
  }
  id = t_id;
  state = t_state;
  total_run_time = DEFAULT_RUN_TIME;
  if (t_id != MAIN_THREAD_ID) {
    pc = (address_t) thread_func;
    sp = (address_t) stack + stack_size - sizeof(address_t);
  }
  sigsetjmp(env, 1);
  env->__jmpbuf[JB_SP] = translate_address(sp);
  env->__jmpbuf[JB_PC] = translate_address(pc);
  sigemptyset(&env->__saved_mask);
}

/**
 *  returns the state od the thread
 * @return the state of the thread
 */
int Thread::get_state() const {
  return state;
}

/**
 * changes the state of the thread
 * @param t_state the state of the thread
 */
void Thread::change_state(int t_state) {
  state = t_state;
}

/**
 * returns the number quantums the read ran so far
 * @return the number quantums the read ran so far
 */
int Thread::get_run_time() const {
  return total_run_time;
}

/**
 * a destructor for the class
 */
Thread::~Thread() {
  delete[] stack;
}

/**
 * returns the thread's id
 * @return thread's id
 */
int Thread::get_id() const {
  return id;
}

/***
 * increasing the thread's run time by one
 */
void Thread::increase_run_time() {
  total_run_time++;
}
