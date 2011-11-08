#ifndef GRAPHLAB_THREAD_POOL_HPP
#define GRAPHLAB_THREAD_POOL_HPP

#include <boost/bind.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/util/blocking_queue.hpp>

namespace graphlab {

  /**
   * \ingroup util
   * Manages a pool of threads.
   * 
   * The interface is nearly identical to the \ref thread_group. 
   * The key difference is internal behavior. The thread pool preallocates a
   * collection of threads which it keeps asleep. When tasks are issued 
   * through the "launch" function, threads are woken up to perform the
   * tasks. 
   *
   * The thread_pool object performs limited exception forwarding.
   * exception throws within a thread of type const char* will be caught
   * and forwarded to the join() function.
   * If the call to join() is wrapped by a try-catch block, the exception
   * will be caught safely and thread cleanup will be completed properly.
   *
   * If multiple threads are running in the thread-group, the master should
   * test if running_threads() is > 0, and retry the join().
   *
   */
  class thread_pool {
    private:

      thread_group threads;
      blocking_queue<boost::function<void (void)> > spawn_queue;
      size_t pool_size;
      
      // protects the exception queue, and the task counters
      mutex mut;
      conditional event_condition;  // to wake up the joining thread
      std::queue<const char*> exception_queue;
      size_t tasks_inserted;
      size_t tasks_completed;
      bool waiting_on_join; // true if a thread is waiting in join

      bool cpu_affinity;
      // not implemented
      thread_pool& operator=(const thread_pool &thrgrp);
      thread_pool(const thread_pool&);
      
      /**
        Called by each thread. Loops around a queue of tasks.
      */
      void wait_for_task();

      /**
        Creates all the threads in the thread pool.
        Resets the task and exception queue
      */
      void spawn_thread_group();
      
      /**
        Destroys the thread pool.
        Also destroys the task queue
      */
      void destroy_all_threads();
    public:
      
       /* Initializes a thread pool with nthreads. 
        * If affinity is set, the nthreads will by default stripe across 
        * the available cores on the system. 
        */
      thread_pool(size_t nthreads, bool affinity = false);
  
      /**
        Changes the CPU affinity. Note that pthread does not provide
        a way to change CPU affinity on a currently started thread.
        This function therefore waits for all threads in the pool
        to finish their current task, and destroy all the threads. Then
        new threads are created with the new affinity setting.
        */
      void set_cpu_affinity(bool affinity);
      
      /**
        Gets the CPU affinity.
      */
      bool get_cpu_affinity() { return cpu_affinity; };
  
      /** 
       * Launch a single thread which calls spawn_function. If affinity
       * is set on construction of the thread_pool, the thread handling the
       * function will be locked on to one particular CPU.
       */
      void launch(const boost::function<void (void)> &spawn_function);
  
  
      /** Waits for all threads to become free. const char* exceptions
      thrown by threads are forwarded to the join() function.
      Once this function returns normally, the queue is empty.
      
      Note that this function may not return if producers continually insert
      tasks through launch. 
      */
      void join();
      
      //! Destructor. Cleans up all threads
      ~thread_pool();
  };
  
}
#endif
