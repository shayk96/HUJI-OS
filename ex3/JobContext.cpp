#include "MapReduceFramework.h"
#include <list>
#include <vector>
#include <atomic>
#include "Barrier.h"
#include <semaphore.h>
#include <iostream>

#define BAD_ALLOC "system error: bad memory allocation"
#define DEFAULT 0

using namespace std;

/**
 * a class that includes all the parameters that are relevant for the job.
 */
class JobContext{

 public:

    /**
     * an atomic 64-bit counter : 2 bits for the stage, 31 bits for the processed and 31 bits for the index
     */
    atomic<uint64_t>* counter;

    /**
     * a struct that holds the state of the program and the percentages done
     */
    JobState *state;

    /**
     * a class containing the reduce and map functions
     */
    const MapReduceClient& client;

    /**
     * a vector that holds the pairs to process
     */
    const InputVec &input_vec;

    /**
     * a vector that the program fills with the results
     */
    OutputVec& outputVec;

    /**
     * an array that holds pointers to the threads created in the program
     */
    pthread_t* threads;

    /**
     * a vector that holes each threads result vector from the MAP phase
     */
    vector<IntermediateVec*> *threads_vectors;   // before shuffle

    /**
     * a mutex that is used than updating the stage and percentage of the program before returning it to the user
     */
    pthread_mutex_t* state_mutex;

    /**
     * a mutex that is used than emitting the result of the REDUCE stage to the output vector
     */
    pthread_mutex_t* out_mutex;

    /**
     * the size of the input vector (number of elements to send to the MAP function)
     */
    unsigned long int input_vec_size;

    /**
     * a barrier used in the program
     */
    Barrier* barrier;

    /**
     * a vector that is filled in the SHUFFLE stage
     */
    vector<pair<K2*,vector<IntermediatePair >*>> *shuffle_vec;

    /**
     * number of threads created in the program
     */
    int multi_thread_level;

    /**
     * total number of pairs to process in the SHUFFLE stage
     */
    int pairs_after_map;

    /**
     * the size fo the shuffle vector
     */
    int shuffle_vec_size;

    /**
     * a boolean that indicates if a join has been called on the threads
     */
    bool joined;



    /**
     * a constructor for the class
     *
     * @param multiThreadLevel number of threads created in the program
     * @param client a struct containing the reduce and map functions, input vector and output vector
     * @param stage a struct that holds the state of the program and the percentages done
     * @param outputVec a vector that the program fills with the results
     * @param input_vec a vector that holds the pairs to process
     * @param threads an array that holds pointers to the threads created in the program
     */
    JobContext(int multiThreadLevel, const MapReduceClient& client, JobState *stage, OutputVec& outputVec,
               const InputVec& input_vec, pthread_t* threads):
    state(stage),client(client), input_vec(input_vec), outputVec(outputVec), threads(threads),
    multi_thread_level(multiThreadLevel){

      state_mutex = new(nothrow) pthread_mutex_t;
      out_mutex = new(nothrow) pthread_mutex_t;
      init_mutexes();
      input_vec_size = input_vec.size();
      counter = new atomic<uint64_t>((unsigned long) DEFAULT);
      barrier = new Barrier(multiThreadLevel);
      shuffle_vec = new(nothrow) vector<pair<K2*,vector<IntermediatePair >*>>;
      if (shuffle_vec == nullptr){
        cerr << BAD_ALLOC <<endl;
        exit(EXIT_FAILURE);
      }
      pairs_after_map = DEFAULT;
      shuffle_vec_size = DEFAULT;
      joined = false;
      threads_vectors= new(nothrow) vector<IntermediateVec*>;   // before shuffle
      for (int thread_id = DEFAULT; thread_id < multi_thread_level; ++thread_id) {
        threads_vectors->push_back(new(nothrow) IntermediateVec);
        if (threads_vectors->at(thread_id) == nullptr){
          cerr << BAD_ALLOC <<endl;
          exit(EXIT_FAILURE);
        }
      }
    }


    /**
     * initializes the mutexes and checks for successful allocation
     */
  void init_mutexes() const {
    pthread_mutex_init(state_mutex, nullptr);
    if (state_mutex == nullptr){
      cerr << BAD_ALLOC << endl;
      exit(EXIT_FAILURE);
    }
    pthread_mutex_init(out_mutex, nullptr);
    if (out_mutex == nullptr){
      cerr << BAD_ALLOC << endl;
      exit(EXIT_FAILURE);
    }
  }

  /**
   * a destructor for the class. releases all the resources
   */
  ~JobContext(){
      delete state;
      delete[] threads;
      delete counter;
      delete barrier;
      pthread_mutex_destroy(out_mutex);
      delete out_mutex;
      pthread_mutex_destroy(state_mutex);
      delete state_mutex;
      delete threads_vectors;
      for (auto v : *shuffle_vec ){
        delete v.second;
      }
      delete shuffle_vec;
    }
};