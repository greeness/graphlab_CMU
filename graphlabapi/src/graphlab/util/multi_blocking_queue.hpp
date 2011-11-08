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


#ifndef GRAPHLAB_MULTI_multi_blocking_queue_HPP
#define GRAPHLAB_MULTI_multi_blocking_queue_HPP


#include <vector>
#include <deque>
#include <iostream>
#include <graphlab/util/random.hpp>
#include <graphlab/parallel/pthread_tools.hpp>

#include <graphlab/macros_def.hpp>

namespace graphlab {

   /** 
    * \ingroup util_internal
    * \brief Implements a blocking queue useful for producer/consumer models
    */
    
  template<typename T>
  class multi_blocking_queue {
  private:
    
    typedef typename std::deque<T> queue_type;
    
    struct single_queue {
      queue_type m_queue;
      mutex m_mutex;
      conditional m_conditional;
      bool handler_sleeping;
      size_t numelem;
    };
    size_t num_queues;
    std::vector<single_queue> allqueues;
    bool m_alive;
    
  public:
    
    //! creates a blocking queue
    multi_blocking_queue(size_t num_queues = 1) : 
          num_queues(num_queues), m_alive(true) { 
      init(num_queues);
    }
   
    size_t get_num_queues() {
      return num_queues;
    } 
    /**
      an alternate initialization which can be called after construction.
      This is not safe once the queue is being used.
    */
    void init(size_t nqueues) {
      num_queues = nqueues;
      allqueues.resize(num_queues);
      for (size_t i = 0;i < allqueues.size(); ++i) {
        allqueues[i].handler_sleeping = false;
        allqueues[i].numelem = 0;
      }
    }
    
    //! Add an element to the blocking queue
    inline void enqueue(const T& elem) {
      // TODO: this can easily be done with some bit operations on the 
      const size_t prod = 
        random::fast_uniform<size_t>(0, num_queues * num_queues - 1);
      size_t r1 = prod / num_queues;
      size_t r2 = prod % num_queues;
      size_t qidx = 
        (allqueues[r1].numelem < allqueues[r2].numelem) ? r1 : r2;
      single_queue& queueref = allqueues[qidx];
      queueref.m_mutex.lock();
      queueref.m_queue.push_back(elem);
      queueref.numelem++;
      if (queueref.handler_sleeping) queueref.m_conditional.signal();
      queueref.m_mutex.unlock();
    }

    //! Add an element to the blocking queue
    inline void enqueue_to_head(const T& elem) {
      // TODO: this can easily be done with some bit operations on the 
      const size_t prod = 
        random::fast_uniform<size_t>(0, num_queues * num_queues - 1);
      size_t r1 = prod / num_queues;
      size_t r2 = prod % num_queues;
      size_t qidx = 
        (allqueues[r1].numelem < allqueues[r2].numelem) ? r1 : r2;
      single_queue& queueref = allqueues[qidx];
      queueref.m_mutex.lock();
      queueref.m_queue.push_front(elem);
      queueref.numelem++;
      if (queueref.handler_sleeping) queueref.m_conditional.signal();
      queueref.m_mutex.unlock();
    }


    //! Add an element to the blocking queue
    inline void enqueue_specific(const T& elem, size_t qidx_) {
      size_t qidx = qidx_ % num_queues;
      single_queue& queueref = allqueues[qidx];
      queueref.m_mutex.lock();
      queueref.m_queue.push_back(elem);
      queueref.numelem++;
      if (queueref.handler_sleeping) queueref.m_conditional.signal();
      queueref.m_mutex.unlock();
    }
    /**
     * Blocks until an element is available in the queue or an
     * interrupt is invoked on the queue.
     */
    inline std::pair<T, bool> dequeue(size_t id) {
      std::pair<T, bool> ret;
      ret.second = false;
      
      single_queue& queueref = allqueues[id];
      queueref.m_mutex.lock();
      while(queueref.m_queue.empty() && m_alive) {
        queueref.handler_sleeping = true;
        queueref.m_conditional.wait(queueref.m_mutex);
        queueref.handler_sleeping = false;
      }
      
      if(!queueref.m_queue.empty()) {
        ret.second = true;  // success
        ret.first = queueref.m_queue.front();
        queueref.m_queue.pop_front();
        queueref.numelem--;
      } 
      queueref.m_mutex.unlock();
      return ret;
    }

    inline std::pair<T, bool> try_dequeue(size_t id) {
      std::pair<T, bool> ret;
      ret.second = false;
      
      for (size_t i = 0; i < allqueues.size(); ++i) {
        single_queue& queueref = allqueues[id];
        if (queueref.numelem > 0) {
          queueref.m_mutex.lock();
          
          if(!queueref.m_queue.empty()) {
            ret.second = true;  // success
            ret.first = queueref.m_queue.front();
            queueref.m_queue.pop_front();
            queueref.numelem--;
            queueref.m_mutex.unlock();
            break;
          } 
          queueref.m_mutex.unlock();
        }
        id++;
        if (id == allqueues.size()) id = 0;
      }
      return ret;
    }

    //! Return true if queue 'id' is empty
    inline bool empty(size_t id) { 
      single_queue& queueref = allqueues[id];
      return queueref.numelem == 0;
    }

    /** Wakes up all threads waiting on the queue whether 
        or not an element is available. Once this function is called,
        the blocking queue is essentially destroyed and can no longer be used.
    */
    inline void stop_blocking() {

      for (size_t i = 0; i < allqueues.size(); ++i) {
        allqueues[i].m_mutex.lock();
      }
      m_alive = false;
      for (size_t i = 0; i < allqueues.size(); ++i) {
        allqueues[i].m_conditional.broadcast();
        allqueues[i].m_mutex.unlock();
      }

    }
    
    //! get the size of the queue
    inline size_t size(size_t id) {
      single_queue& queueref = allqueues[id];
      return queueref.numelem;
    }



    ~multi_blocking_queue() {
      stop_blocking();
    }    
  }; // end of multi_blocking_queue class
  

} // end of namespace graphlab

#include <graphlab/macros_undef.hpp>

#endif

