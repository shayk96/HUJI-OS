#include "uthreads.h"
#include <queue>
#include <set>
#include <map>
#include <iostream>
#include <csetjmp>
#include <csignal>
#include <sys/time.h>
#include <unistd.h>
#include "Thread.h"

using namespace std;
#define SUCCESS 0
#define FAILURE (-1)
#define MAIN_THREAD_ID 0
#define LONG_RETURN_VALUE 1
#define MIN_QUANTUM 0
#define SIGNAL_NUM 26
#define OFFSET 1
#define SET_JUMP 0


//---------------thread states-----------------------------------------
#define READY  0
#define RUNNING 1
#define BLOCKED 2
#define SLEEPING 3
#define SLEEPING_AND_BLOCKED 4
#define SUICIDE 5

//---------------------error messages---------------------------------
#define TIMER_ERROR "system error: setitimer error."
#define SIGACTION_ERROR "system error: sigaction error."
#define BAD_ALLOC "system error: bad memory allocation"
#define SIGNAL_FAILURE "system error: signal masking or unmasking failure"
#define WRONG_QUANTUM_ERROR "thread library error: quantum must bo positive integer"
#define MAX_THREADS_ERROR "thread library error: maximum number of threads reached"
#define ID_ERROR "thread library error: thread id does not exist"
#define MAIN_OR_ID_BLOCK_ERROR "thread library error: thread id does not exists or trying to block main thread"
#define SLEEP_ERROR "thread library error: main thread cant sleep or sleeping time must be positive"
#define SLEEP_ID_ERROR "thread library error: cant resume a thread that is sleeping"
#define NULL_ERROR "thread library error: cant create a thread with NULL entry point"

//------------------------globals-------------------------------------

/**
 * the length between signals. starts with -1 in order to be initialized. will be changed by the user in the
 * uthread_init function.
 */
int QUANTUM_LENGTH = -1;

/**
 * a map holding all the threads in the program.
 * the key is the thread id and the value is a pointer to the thread
 */
map<int, Thread *> thread_map = map<int,Thread*>();

/**
 * a map holding all the threads that are is SLEEPING state.
 * the key is the quantum number to wake up at and the value is a vector
 * of pointers to thread
 */
map<int, set<Thread *>> sleeping_thread_map = map<int,set<Thread *>>();

/**
 * a set holding the id's of the blocked threads
 */
set<int> blocked_threads = set<int> ();

/**
 * a set holding the id's of the sleeping threads
 */
set<int> sleeping_threads = set<int> ();

/**
 * a set holding all the available id's
 */
set<int> available_num = set<int> ();

/**
 * a queue that holds the id's of threads that are in READY state in order of
 * FIFO
 */
queue<int> ready_queue = queue<int>();

/**
 * a pointer to the RUNNING thread
 */
Thread *current_thread;

/**
 * a set that holds the signals to mask
 */
sigset_t set2mask;

/**
 * the total number of quantum that passed
 */
int total_quantum = 1;

/**
 * a timer for the signal
 */
struct itimerval timer;


//---------------------------------------------------------------------


/***
 * an empty function for the main thread to be initialized
 */
void empty_func()
{}

/***
 * erases the thread from the sleeping_thread_map so it wont be resumed
 * after termination
 * @param tid the thread's id that's needs to be erased
 */
void erase_from_sleeping_map(int tid)
{
  for (auto threads : sleeping_thread_map)
    {
      for (auto thread : threads.second)
        {
          if (thread->get_id() == tid)
            {
              threads.second.erase(thread);
              break;
            }
        }
    }
}

/***
 * deletes the thread and removes its id from every data container in the program.
 * @param tid of the thread to terminate.
 */
void terminate_thread_helper(int tid)
{
  Thread *thread_to_remove = thread_map[tid];
  thread_map.erase(tid);
  sleeping_threads.erase(tid);
  blocked_threads.erase(tid);
  erase_from_sleeping_map(tid);
  available_num.insert(tid);
  delete thread_to_remove;
}

/***
 * Initializing the main thread in the program.
 */
void init_main_thread()
{
  uthread_spawn(&empty_func);
  current_thread = thread_map.find(MAIN_THREAD_ID)->second;
  current_thread->increase_run_time();
  current_thread->change_state(RUNNING);
  ready_queue.pop();
}

/***
 * blocks signals
 */
void block_signal()
{
  if (sigprocmask(SIG_BLOCK, &set2mask, nullptr) < SUCCESS)
    {
      cerr << SIGNAL_FAILURE << endl;
      exit(EXIT_FAILURE);
    }
}

/***
 * unblocks signals
 */
void unblock_signal()
{
  if (sigprocmask(SIG_UNBLOCK, &set2mask, nullptr) < SUCCESS)
    {
      cerr << SIGNAL_FAILURE << endl;
      exit(EXIT_FAILURE);
    }
}

/***
 * Called by every increasing of the total quantum number.
 * Checks if any sleeping thread needs to wake up, by this time.
 * If there is, changing its state according the last state
 * (for SLEEPING_AND_BLOCKED - changes for BLOCKED, for SLEEPING changes to READY).
 */
void handle_sleepers()
{
  if (sleeping_thread_map.find(total_quantum) == sleeping_thread_map.end())
    {
      return;
    }
  for (auto thread : sleeping_thread_map.find(total_quantum)->second)
    {
      if (thread->get_state() == SLEEPING_AND_BLOCKED)
        {
          thread->change_state(BLOCKED);
          sleeping_threads.erase(thread->get_id());
        }
      else
        {
          thread->change_state(READY);
          sleeping_threads.erase(thread->get_id());
          ready_queue.push(thread->get_id());
        }
    }
  sleeping_thread_map.erase(total_quantum);
}

/***
 * finds the next id in the queue
 * @return the next thread's id
 */
int find_next_id()
{
  int next_thread_id = ready_queue.front();
  ready_queue.pop();
  while (thread_map.find(next_thread_id) == thread_map.end() ||
         blocked_threads.find(next_thread_id) != blocked_threads.end() ||
         sleeping_threads.find(next_thread_id) != sleeping_threads.end())
    {
      next_thread_id = ready_queue.front();
      ready_queue.pop();
    }
  return next_thread_id;
}

/***
 * Turns the current thread to READY or deletes it, if its terminate itself,
 * and turns the next ready thread to RUNNING.
 * @param signal
 */
void switch_threads()
{
  if (current_thread->get_state() == RUNNING)
    {
      current_thread->change_state(READY);
      ready_queue.push(current_thread->get_id());
    }
  if (current_thread->get_state() == SUICIDE)
    {
      terminate_thread_helper(current_thread->get_id());
    }
  int next_thread_id = find_next_id();
  current_thread = thread_map.find(next_thread_id)->second;
  current_thread->change_state(RUNNING);
  current_thread->increase_run_time();
  unblock_signal();
  siglongjmp(current_thread->env, LONG_RETURN_VALUE);
}

/***
 * Called by every quantum, switch between the threads and checks if any
 * thread needs to wake up.
 * @param signal
 */
void scheduler(int signal)
{
  block_signal();
  int ret_val = sigsetjmp(current_thread->env, LONG_RETURN_VALUE);
  if (ret_val != SET_JUMP)
    {
      unblock_signal();
      return;
    }
  total_quantum++;
  handle_sleepers();
  switch_threads();
}

/**
 * Sets the timer for the library.
 */
void set_timer()
{
  struct sigaction sa = {0};
  // Install timer_handler as the signal handler for SIGVTALRM.
  sa.sa_handler = &scheduler;
  if (sigaction(SIGVTALRM, &sa, NULL) < SUCCESS)
    {
      cerr << SIGACTION_ERROR << endl;
    }
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = QUANTUM_LENGTH;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = QUANTUM_LENGTH;
  // Configure the timer to expire after QUANTUM... */
  timer.it_value.tv_sec = 0;            // first time interval, seconds part
  timer.it_value.tv_usec = QUANTUM_LENGTH;        // first time interval, microseconds part
  // configure the timer to expire every QUANTUM after that.
  timer.it_interval.tv_sec = 0;;                   // following time intervals, seconds part
  timer.it_interval.tv_usec = QUANTUM_LENGTH;    // following time intervals, microseconds part
  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
      cerr << TIMER_ERROR << endl;
      exit(EXIT_FAILURE);
    }
}

/**
 * @brief initializes the thread library.
 *
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
  if (quantum_usecs <= MIN_QUANTUM)
    {
      cerr << WRONG_QUANTUM_ERROR << endl;
      return FAILURE;
    }
  QUANTUM_LENGTH = quantum_usecs;
  for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
      available_num.insert(i);
    }
  sigemptyset(&set2mask);
  sigaddset(&set2mask, SIGVTALRM);
  init_main_thread();
  set_timer();
  return EXIT_SUCCESS;
}

/***
 * Finds the minimal id that available.
 * @return The minimal id that available
 */
int pop_min()
{
  int min_num = *available_num.begin();
  available_num.erase(*available_num.begin());
  return min_num;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack_ptr of size STACK_SIZE bytes.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point)
{
  block_signal();
  if (available_num.empty())
    {
      cerr << MAX_THREADS_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  if (entry_point == nullptr)
    {
      cerr << NULL_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  auto *thread = new(std::nothrow)Thread(pop_min(), STACK_SIZE, entry_point, READY);
  if (thread == nullptr)
    {
      cerr << BAD_ALLOC << endl;
      unblock_signal();
      return FAILURE;
    }
  ready_queue.push(thread->get_id());
  thread_map.insert(pair<int, Thread *>(thread->get_id(), thread));
  unblock_signal();
  return thread->get_id();
}

/***
 * Handle thread doing an action on itself.
 * @param state
 */
void self_action(int state)
{
  current_thread->change_state(state);
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
      cerr << TIMER_ERROR << endl;
      exit(EXIT_FAILURE);
    }
  scheduler(SIGNAL_NUM);
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
  block_signal();
  if (tid == MAIN_THREAD_ID)
    {
      exit(EXIT_SUCCESS);
    }
  if (thread_map.find(tid) == thread_map.end())
    {
      cerr << ID_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  if (tid == current_thread->get_id())
    {
      self_action(SUICIDE);
    }
  else
    {
      terminate_thread_helper(tid);
    }
  unblock_signal();
  return EXIT_SUCCESS;
}

/***
 * Block a thread.
 * @param tid The id of the thread to block.
 */
void block_thread_helper(int tid)
{
  Thread *thread_to_block = thread_map.find(tid)->second;
  if (thread_to_block->get_state() == SLEEPING)
    {
      thread_to_block->change_state(SLEEPING_AND_BLOCKED);
    }
  else
    {
      thread_to_block->change_state(BLOCKED);
    }
  blocked_threads.insert(tid);
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
  block_signal();
  if (tid == MAIN_THREAD_ID || (thread_map.find(tid) == thread_map.end()))
    {
      cerr << MAIN_OR_ID_BLOCK_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  if (blocked_threads.find(tid) != blocked_threads.end())
    {
      unblock_signal();
      return EXIT_SUCCESS;
    }
  if (tid == current_thread->get_id())
    {
      blocked_threads.insert(tid);
      self_action(BLOCKED);
      unblock_signal();
      return EXIT_SUCCESS;
    }
  block_thread_helper(tid);  //sleeping thread gets blocked
  unblock_signal();
  return EXIT_SUCCESS;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error.
 * If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
  block_signal();
  if (thread_map.find(tid) == thread_map.end())
    {
      cerr << ID_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  if (thread_map.find(tid)->second->get_state() == SLEEPING)
    {
      cerr << SLEEP_ID_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  Thread *thread_to_unblock = thread_map.find(tid)->second;
  if (thread_to_unblock->get_state() == RUNNING || thread_to_unblock->get_state() == READY)
    {
      unblock_signal();
      return EXIT_SUCCESS;
    }
  if (thread_to_unblock->get_state() == SLEEPING_AND_BLOCKED)
    {
      thread_to_unblock->change_state(SLEEPING);
      blocked_threads.erase(tid);
    }
  else
    {
      thread_to_unblock->change_state(READY);
      blocked_threads.erase(thread_to_unblock->get_id());
      ready_queue.push(tid);
    }
  unblock_signal();
  return EXIT_SUCCESS;
}

/**
 * Returns the thread ID of the calling thread.
 * @return The ID of the calling thread.
*/
int uthread_get_tid()
{
  return current_thread->get_id();
}

/**
 * Returns the total number of quantums since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 * @return The total number of quantums.
*/
int uthread_get_total_quantums()
{
  return total_quantum;
}

/**
 * Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts
 * should increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called,
 * include also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
  block_signal();
  if (thread_map.find(tid) == thread_map.end())
    {
      cerr << ID_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  int quantum = thread_map.find(tid)->second->get_run_time();
  unblock_signal();
  return quantum;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums)
{
  block_signal();
  if (current_thread->get_id() == MAIN_THREAD_ID || num_quantums <= MIN_QUANTUM)
    {
      cerr << SLEEP_ERROR << endl;
      unblock_signal();
      return FAILURE;
    }
  int time_to_wake_up = total_quantum + num_quantums + OFFSET;
  if (sleeping_thread_map.find(time_to_wake_up) == sleeping_thread_map.end())
    {
      set<Thread *> temp_sleeping_threads = set<Thread *>();
      temp_sleeping_threads.insert(current_thread);
      sleeping_thread_map.insert(pair<int, set<Thread *>>(time_to_wake_up, temp_sleeping_threads));
    }
  else
    {
      set<Thread *> threads_to_wake_up = sleeping_thread_map.find(time_to_wake_up)->second;
      threads_to_wake_up.insert(current_thread);
    }
  sleeping_threads.insert(current_thread->get_id());
  self_action(SLEEPING);
  return EXIT_SUCCESS;
}
