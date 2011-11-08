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


#ifndef GRAPHLAB_SYNCHRONIZED_CIRCULAR_QUEUE_HPP
#define GRAPHLAB_SYNCHRONIZED_CIRCULAR_QUEUE_HPP

#include <queue>
#include <cassert>
#include <cstring>


#include <graphlab/parallel/pthread_tools.hpp>


namespace graphlab {

  
  /**
   \ingroup util_internal
     Implementation of a self-resizing circular queue
  */
  template <typename T>
  class synchronized_circular_queue {

  public:
    synchronized_circular_queue(size_t sizehint = 128) { 
      queue = (T*)malloc(sizeof(T) * sizehint);
      head = 0;
      tail = 0;
      length = 0;
      queuesize = sizehint;
    }
  
    ~synchronized_circular_queue() {
      free(queue);
    }
  

    void push(const T &item) {
      queuelock.lock();
      if (length == queuesize) {
        unsync_doublesize();
      }
      queue[tail] = item;
      ++tail;     
      tail = (tail < queuesize)?tail:0;
      ++length;
      queuelock.unlock();
    }
  
    bool safepop(T * ret) {
      queuelock.lock();
      if (length == 0) {
        queuelock.unlock();
        return false;
      }
      (*ret) = queue[head];
      ++head;
      head = (head < queuesize)?head:0;
      --length;
      queuelock.unlock();
      return true;
    }
  
    T pop() {
      queuelock.lock();
      assert(length > 0);
      T t = queue[head];
      ++head;
      head = (head < queuesize)?head:0;
      --length;
      queuelock.unlock();
      return t;
    }
  
    size_t size() const{
      return length;
    }
  private:
    T* queue;
    spinlock queuelock;
    size_t head;      /// points to the first element in the queue
    size_t tail;      /// points to one past the last element in the queue
    size_t length;    /// number of elements in the queue
    size_t queuesize; /// the size of the queue array
  
    void unsync_doublesize() {
      queue = (T*)realloc(queue, sizeof(T) * (queuesize*2));
      //queue.resize(queue.size() * 2);
      // now I need to move elements around
      // if head < tail, that means that the array
      // is in the right position. I do not need to do anything
      // otherwise.I need to move (0...tail-1) to (size:size+tail)
      if (head >= tail && length > 0) {
        memcpy(queue+queuesize, queue, sizeof(T) * tail);
        tail = queuesize + tail;
      }
      queuesize *= 2;
    }
  };

}
#endif

