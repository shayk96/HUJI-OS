#include "MapReduceFramework.h"
#include <pthread.h>
#include <algorithm>
#include <iostream>
#include "JobContext.cpp"
#include "MapReduceClient.h"

#define BAD_CREATION "system error: cannot create thread"
#define MAP_STATE (1ul << 62)
#define SHUFFLE_STATE (1ul << 63)
#define REDUCE_STATE (3ul << 62)
#define INDEX 0x7FFFFFF
#define INC_PROCESSED 0x80000000
#define MAIN_THREAD 0
#define PROCESSED 0x3FFFFFFF80000000
#define PERCENTAGE 100

using namespace std;

/**
 * a struct containing the thread id and the JobContext
 */
typedef struct threadContext {
    int thread_id;
    JobContext *job;
} threadContext;

/**
 * a compare function that compares between IntermediatePairs keys
 * @param right an IntermediatePair pair
 * @param left another IntermediatePair pair
 * @return True if left bigger than right else False
 */
bool compare(IntermediatePair &right, IntermediatePair &left) {
  return *right.first < *left.first;
}

/**
 * checks if two IntermediatePair are equal in the keys
 * @param right an IntermediatePair pair
 * @param left another IntermediatePair pair
 * @return True if equal else False
 */
bool equal(IntermediatePair &right, IntermediatePair &left) {
  return !(*right.first < *left.first) && !(*left.first < *right.first);
}

/**
 *  handles the map phase. each thread reads pairs of (k1, v1) from the input vector and calls the map function
    on each of them.
 * @param tc the threadContext of each thread
 * @param counter the atomic counter of the program
 */
void map_phase(threadContext* tc, atomic<uint64_t>* counter){
  *counter |= MAP_STATE;
  uint64_t pair_index = ((*(counter))++) & INDEX;
  while (pair_index < tc->job->input_vec_size) {
      InputPair cur_pair = tc->job->input_vec.at(pair_index);
      tc->job->client.map(cur_pair.first, cur_pair.second, tc);
      *counter += INC_PROCESSED;
      pair_index = ((*counter)++) & (INDEX);
    }
}

/**
 * handles the sort phase. each thread sorts its intermediate vector according to the keys
    within
 * @param tc the threadContext of each thread
 */
void sort_phase(threadContext* tc){
  IntermediateVec *cur_vec = tc->job->threads_vectors->at(tc->thread_id);
  sort(cur_vec->begin(), cur_vec->end(), compare);
}

/**
 * removes empty vectors from the threads_vectors array. also counts the number of pairs produced
 * @param tc the threadContext of each thread
 * @return the number of pairs produced
 */
int remove_empty_vectors(threadContext *tc) {
  int total_pairs = 0;
  for (int pos = tc->job->threads_vectors->size() - 1; pos >= 0; pos--) {
      if (tc->job->threads_vectors->at(pos)->empty()) {
          delete tc->job->threads_vectors->at(pos);
          tc->job->threads_vectors->erase(tc->job->threads_vectors->begin() + pos);
          continue;
        }
      total_pairs += tc->job->threads_vectors->at(pos)->size();
    }
  return total_pairs;
}

/**
 * searches for the max key from the last element in each thread vector
  * @param tc the threadContext of each thread
 * @return the max pair
 */
IntermediatePair get_max_pair(threadContext *tc) {
  int max_vec = 0;
  for (int pair = 1; pair <= tc->job->threads_vectors->size() - 1; pair++) {
      if (compare(tc->job->threads_vectors->at(max_vec)->back(), tc->job->threads_vectors->at(pair)->back())) {
          max_vec = pair;
        }
    }
  IntermediatePair max_pair = tc->job->threads_vectors->at(max_vec)->back();
  tc->job->threads_vectors->at(max_vec)->pop_back();
  if (tc->job->threads_vectors->at(max_vec)->empty()) {
      delete tc->job->threads_vectors->at(max_vec);
      tc->job->threads_vectors->erase(tc->job->threads_vectors->begin() + max_vec);
    }
  return max_pair;
}

/**
 * creates a new vector of (k2, v2) where in each sequence all keys are identical and all elements with a given key
 * are in a single sequence
 *
 * @param tc the threadContext of each thread
 * @param counter the atomic counter of the program
 */
void shuffle_phase(threadContext *tc, atomic<uint64_t> *counter) {
  *counter = SHUFFLE_STATE;
  tc->job->pairs_after_map = remove_empty_vectors(tc);            //remove empty vectors and count the number of pairs
  while (!tc->job->threads_vectors->empty()) {
      IntermediatePair max_pair = get_max_pair(tc);
      *counter += (INC_PROCESSED);
      auto *val_vec = new(nothrow) vector<IntermediatePair>;
      if (val_vec == nullptr){
          cerr << BAD_ALLOC << endl;
          exit(EXIT_FAILURE);
        }
      val_vec->push_back(max_pair);
      for (int pair = tc->job->threads_vectors->size() - 1 ; pair >= 0; pair--) {
          while((equal(max_pair,tc->job->threads_vectors->at(pair)->back()))) {
              val_vec->push_back(tc->job->threads_vectors->at(pair)->back());
              tc->job->threads_vectors->at(pair)->pop_back();
              *counter += (INC_PROCESSED);
              if (tc->job->threads_vectors->at(pair)->empty()) {
                  delete tc->job->threads_vectors->at(pair);
                  tc->job->threads_vectors->erase(tc->job->threads_vectors->begin() + pair);
                  break;
                }
            }
        }
      tc->job->shuffle_vec->push_back(make_pair(max_pair.first,val_vec));
    }
}

/**
 * handles the reduce phase. each thread reads pairs of (k2, vector<k2,v2>) from the shuffle vector and calls the
 * reduce function on each of them.
 * @param tc the threadContext of each thread
 * @param counter the atomic counter of the program
 */
void reduce_phase(threadContext *tc, atomic<uint64_t> *counter) {
  uint64_t pair_index = ((*(counter))++) & (INDEX);
  while (pair_index < tc->job->shuffle_vec_size) {
      auto cur_pair = tc->job->shuffle_vec->at(pair_index);
      tc->job->client.reduce(cur_pair.second, tc);
      *counter += (INC_PROCESSED);
      pair_index = ((*counter)++) & (INDEX);
    }
}

/**
 * the main function that each thread runs
 *
 * @param arg a JobContext
 * @return nullptr
 */
void *thread_job(void *arg) {
  auto *tc = (threadContext *) arg;
  atomic<uint64_t> *counter = tc->job->counter;
  //// MAP phase
  map_phase(tc, counter);
  ////SORT phase
  sort_phase(tc);
  tc->job->barrier->barrier();
  ////SHUFFLE phase
  if (tc->thread_id == MAIN_THREAD) {
      shuffle_phase(tc, counter);
      tc->job->shuffle_vec_size = tc->job->shuffle_vec->size();
      *counter = REDUCE_STATE;
    }
  tc->job->barrier->barrier();
  //// REDUCE PHASE
  reduce_phase(tc, counter);
  delete tc;
  return nullptr;
}


/**
 * The function receives as input output element (K3, V3) and context which contains data
   structure of the thread that created the output element. The function saves the output
   element in the context data structures (output vector).
 */
void emit3(K3 *key, V3 *value, void *context) {
  auto *tc = (threadContext *) context;
  pthread_mutex_lock(tc->job->out_mutex);
  tc->job->outputVec.push_back(make_pair(key,value));
  pthread_mutex_unlock(tc->job->out_mutex);
}

/**
 * this function gets a JobHandle and updates the state of the job into the given
   JobState struct.
 *
 * @param job a JobHandle
 * @param state a JobState
 */
void getJobState(JobHandle job, JobState *state) {
  auto *cur_job = (JobContext *) job;
  pthread_mutex_lock(cur_job->state_mutex);
  unsigned long counter = (cur_job->counter->load());
  int processed = (counter & PROCESSED) >> 31; // taking the 31 in thr middle
  stage_t stage = static_cast<stage_t>(counter >> 62);
  unsigned long total = 0;
  if (stage == UNDEFINED_STAGE){
      state->percentage = cur_job->state->percentage = 0;
      state->stage = cur_job->state->stage = stage;
      pthread_mutex_unlock(cur_job->state_mutex);
      return;
    }
  else if (stage == MAP_STAGE) {
      total = cur_job->input_vec_size;
    }
  else if (stage == SHUFFLE_STAGE) {
      total = cur_job->pairs_after_map;
    }
  else if (stage == REDUCE_STAGE) {
      total = cur_job->shuffle_vec_size;
    }
  cur_job->state->stage = state->stage = stage;
  if (total == 0 ){
      cur_job->state->percentage = state->percentage = 0;
    } else {
      cur_job->state->percentage = state->percentage = ((float) processed / (float) total) * PERCENTAGE;
    }
  pthread_mutex_unlock(cur_job->state_mutex);
}

/**
 * the function calls the waitForJob function and than releasing all resources of the job.
 *
 * @param job the job which converted to JobContext
 */
void closeJobHandle(JobHandle job) {
  auto *cur_job = (JobContext *) job;
  waitForJob(job);
  delete cur_job;
}

/**
 * a function gets JobHandle returned by startMapReduceFramework and waits
   until it is finished.
 * @param job the job which converted to JobContext
 */
void waitForJob(JobHandle job){
  auto *cur_job = (JobContext *) job;
  if (!cur_job->joined) {
      for (int thread = 0 ; thread < cur_job->multi_thread_level; thread++){
          pthread_join(cur_job->threads[thread], nullptr);
        }
      cur_job->joined = true;
    }
}

/**
 * saves the intermediary element in the thread vector
 * @param key the key of the pair to insert
 * @param value the value of the pair to insert
 * @param context the context which converted to threadContext
 */
void emit2(K2 *key, V2 *value, void *context) {
  auto *tc = (threadContext *) context;
  IntermediatePair pair = make_pair(key, value);
  (tc->job->threads_vectors->at(tc->thread_id))->push_back(pair);
}

/**
 * starts running the MapReduce algorithm
 * @param client containing the reduce and map functions
 * @param inputVec vector that holds the pairs to process
 * @param outputVec vector that the program fills with the results
 * @param multiThreadLevel number of threads created in the program
 * @return a JobHandle
 */
JobHandle startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec,
                            int multiThreadLevel) {
  auto *state = new JobState{UNDEFINED_STAGE, 0};
  auto *threads = new(nothrow) pthread_t[multiThreadLevel];
  if(threads == nullptr){
      cerr << BAD_ALLOC <<endl;
      exit(EXIT_FAILURE);
    }
  auto *job = new JobContext(multiThreadLevel, client, state, outputVec, inputVec, threads);
  for (int thread = 0; thread < multiThreadLevel; ++thread) {
      int success = pthread_create(&threads[thread], nullptr, thread_job, new threadContext{thread, job});
      if (success != 0) {
          cerr << BAD_CREATION <<endl;
          exit(EXIT_FAILURE);
        }
    }
  return job;
}

