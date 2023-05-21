#ifndef _THREAD_H_
#define _THREAD_H_

#include <csetjmp>


typedef unsigned long address_t;
typedef void (*thread_entry_point) ();

/***
 * A class represents a thread.
 */
class Thread {

 private:

  /***
   * thread id, unique to each thread, until the thread is terminated.
   */
  int id;

  /***
   * A pointer to the stack memory of the thread.
   */
  char *stack;

  /***
   * Threads' state - can be READY, RUNNING, BLOCKED, SLEEPING or BLOCKED AND SLEEPING.
   */
  int state;

  /***
   * pc's and sp's thread addresses.
   */
  address_t pc, sp;

  /***
   * quantums number the thread is running so far.
   */
  int total_run_time;

 public:
  
/***
 * The environment if the thread
 */
  sigjmp_buf env;

  /**
   * a constructor for the thread
   * @param t_id the thread's unique id
   * @param stack_size the size of the stack
   * @param thread_func the function that the thread is doing
   * @param t_state the thread's state
   */
  Thread (int t_id, int stack_size, thread_entry_point thread_func, int t_state);

  /**
   *  returns the state od the thread
   * @return the state of the thread
   */
  int get_state () const;

  /**
   * changes the state of the thread
   * @param t_state the state of the thread
   */
  void change_state (int t_state);

  /**
   * returns the number quantums the read ran so far
   * @return the number quantums the read ran so far
   */
  int get_run_time () const;

  /**
   * a destructor for the class
   */
  ~Thread ();

  /**
   * returns the thread's id
   * @return
   */
  int get_id () const;

  /***
   * increasing the thread's run time by one
   */
  void increase_run_time ();

};

#endif //_THREAD_H_
