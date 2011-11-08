/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */


#ifndef GRAPHLAB_SHARED_TERMINATION_HPP
#define GRAPHLAB_SHARED_TERMINATION_HPP

#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/parallel/atomic.hpp>

namespace graphlab {
  /**
   \ingroup util_internal
     simple condition variable based shared termination checker.
     When a processor finds that it is out of work, it should call
     - begin_critical_section(cpuid),
     - check the state of the queue.
     - If the queue has jobs, call cancel_critical_section().
     - If the queue has no jobs, then call end_critical_section(cpuid)
     - If (end_critical_section() returns true, the scheduler can terminate.
     Otherwise it must loop again.
  */
  class shared_termination {
  public:
    shared_termination(size_t ncpus) {
      numactive = ncpus;
      numcpus = ncpus;
      done = false;
      trying_to_sleep.value = 0;
      sleeping.resize(ncpus);
      for (size_t i = 0; i < ncpus; ++i) sleeping[i] = 0;
    }
  
    ~shared_termination(){ }
  
    void begin_critical_section(size_t cpuid) {
      trying_to_sleep.inc();
      sleeping[cpuid] = true;
      m.lock();
    }

    void cancel_critical_section(size_t cpuid) {
      m.unlock();
      sleeping[cpuid] = false;
      trying_to_sleep.dec();
    }

    bool end_critical_section(size_t cpuid) {
      // if done flag is set, quit immediately
      if (done) {
        m.unlock();
        trying_to_sleep.dec();
        sleeping[cpuid] = false;
        return true;
      }
      /*
        Assertion: Since numactive is decremented only within 
        a critical section, and is incremented only within the same critical 
        section. Therefore numactive is a valid counter of the number of threads 
        outside of this critical section. 
      */
      numactive--;
    
      /*
        Assertion: If numactive is ever 0 at this point, the algorithm is done.
        WLOG, let the current thread which just decremented numactive be thread 0
      
        Since there is only 1 active thread (0), there must be no threads 
        performing insertions. Since only 1 thread can be in the critical section 
        at any time, and the critical section checks the status of the task queue, 
        the task queue must be empty.
      */
      if (numactive == 0) {
        done = true;
        cond.broadcast();
      }
      else {
        cond.wait(m);
        // here we are protected by the mutex again.
        if (!done) numactive++;
      }
      m.unlock();
      trying_to_sleep.dec();
      sleeping[cpuid] = false;
      return done;
    }
  
    void new_job() {
      /*
        Assertion: numactive > 0 if there is work to do.
        This is relatively trivial. Even if no threads wake up in time to 
        pick up any jobs, the thread which created the job must see it in the 
        critical section.
      */
      if (trying_to_sleep.value > 0) {
        m.lock();
        if (numactive < numcpus) cond.broadcast();
        m.unlock();
      }
    }

    void new_job(size_t cpuhint) {
      if (sleeping[cpuhint]) {
        m.lock();
        if (numactive < numcpus) cond.broadcast();
        m.unlock();
      }
    }

    void completed_job() { }
    
    size_t num_active() {
      return numactive;
    }
  private:
    conditional cond;
    mutex m;
    size_t numactive;
    size_t numcpus;
    atomic<size_t> trying_to_sleep;
    std::vector<char> sleeping;
    bool done;
  };

}
#endif

