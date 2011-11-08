#include <graphlab/parallel/thread_pool.hpp>
#include <graphlab/logger/assertions.hpp>

namespace graphlab {


thread_pool::thread_pool(size_t nthreads, bool affinity) {
  waiting_on_join = false;
  tasks_inserted = 0;
  tasks_completed = 0;
  cpu_affinity = affinity;
  pool_size = nthreads;
  spawn_thread_group();
}

/**
  Creates the thread group
*/
void thread_pool::spawn_thread_group() {
  size_t ncpus = thread::cpu_count();
  // start all the threads if CPU affinity is set
  for (size_t i = 0;i < pool_size; ++i) {
    if (cpu_affinity) {
      threads.launch(boost::bind(&thread_pool::wait_for_task, this), i % ncpus);
    }
    else {
      threads.launch(boost::bind(&thread_pool::wait_for_task, this));
    }
  }
}


void thread_pool::destroy_all_threads() {
  // wait for all execution to complete
  spawn_queue.wait_until_empty();
  // kill the queue
  spawn_queue.stop_blocking();
  
  // join the threads in the thread group
  while(1) {
    try {
      threads.join();
      break;
    }
    catch (const char* c) {
      // this should not be possible!
      logstream(LOG_FATAL) 
        << "Unexpected exception caught in thread pool destructor: " 
        << c << std::endl;
      ASSERT_TRUE(false);
    }
  }
}

void thread_pool::set_cpu_affinity(bool affinity) {
  if (affinity != cpu_affinity) {
    cpu_affinity = affinity;
    // stop the queue from blocking
    spawn_queue.stop_blocking();
    
    // join the threads in the thread group
    while(1) {
      try {
        threads.join();
        break;
      }
      catch (const char* c) {
        // this should not be possible!
        logstream(LOG_FATAL) 
          << "Unexpected exception caught in thread pool destructor: " 
          << c << std::endl;
        ASSERT_TRUE(false);
      }
    }
    spawn_queue.start_blocking();
    spawn_thread_group();
  }
}
      
void thread_pool::wait_for_task() {
  while(1) {
    std::pair<boost::function<void (void)>, bool> queue_entry;
    // pop from the queue
    queue_entry = spawn_queue.dequeue();
    if (queue_entry.second) {
      // try to run the function. remember to put it in a try catch
      try {
        queue_entry.first();
      }
      catch(const char* ex) {
        // if an exception was raised, put it in the exception queue
        mut.lock();
        exception_queue.push(ex);
        event_condition.signal();
        mut.unlock();
      }
      
      mut.lock();
      tasks_completed++;
      // the waiting on join flag just prevents me from 
      // signaling every time completed == inserted. Which could be very
      // very often
      if (waiting_on_join && 
          tasks_completed == tasks_inserted) event_condition.signal();
      mut.unlock();
    }
    else {
      // quit if the queue is dead
      break;
    }
  }
  
}

void thread_pool::launch(const boost::function<void (void)> &spawn_function) {
  mut.lock();
  tasks_inserted++;
  spawn_queue.enqueue(spawn_function);
  mut.unlock();
}

void thread_pool::join() {
  std::pair<bool, bool> eventret;
  // first we wait for the queue to empty
  spawn_queue.wait_until_empty();
  
  mut.lock();
  waiting_on_join = true;
  while(1) {
    // check the exception queue. 
    if (!exception_queue.empty()) {
      // pop an exception
      const char* ex = exception_queue.front();
      exception_queue.pop();
      // unlock and throw the event
      waiting_on_join = false;
      mut.unlock();
      throw(ex);
    }
    // nothing to throw, check if all tasks were completed
    if (tasks_completed == tasks_inserted) {
      // yup
      break;
    }
    event_condition.wait(mut);
  }
  waiting_on_join = false;
  mut.unlock();
}


thread_pool::~thread_pool() {
  destroy_all_threads();
}

}
