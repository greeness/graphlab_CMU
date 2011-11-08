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


/**
 * This class defines a basic round robin scheduler.
 * written by Danny Bickson
 **/
#ifndef GRAPHLAB_RR_SCHEDULER_HPP
#define GRAPHLAB_RR_SCHEDULER_HPP

#include <queue>
#include <cmath>
#include <cassert>
#include <vector>
#include <climits>


#include <graphlab/logger/logger.hpp>
#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>
#include <graphlab/tasks/update_task.hpp>
#include <graphlab/schedulers/ischeduler.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/parallel/atomic.hpp>
#include <graphlab/schedulers/support/vertex_task_set.hpp>
#include <graphlab/schedulers/support/unused_scheduler_callback.hpp>
#include <graphlab/util/synchronized_circular_queue.hpp>
#include <graphlab/util/controlled_termination.hpp>


#include <graphlab/macros_def.hpp>

namespace graphlab {



  /**
     \ingroup group_schedulers
     This class defines a simple First-In-First_Out scheduler
  */
  template<typename Graph>
  class round_robin_scheduler: public ischeduler<Graph> {
  public:
    typedef Graph graph_type;
    typedef ischeduler<Graph> base;

    typedef typename base::vertex_id_type vertex_id_type;
    typedef typename base::iengine_type iengine_type;
    typedef typename base::update_task_type update_task_type;
    typedef typename base::update_function_type update_function_type;
    typedef typename base::callback_type callback_type;
    typedef typename base::monitor_type monitor_type;
    typedef controlled_termination terminator_type;
  
  private:
    size_t numvertices; /// Remember the number of vertices in the graph
    atomic<size_t> cur_task;  /// Which vertex I am executing now.
    unused_scheduler_callback<Graph> callback;
    std::vector<update_task_type> task_set;           /// collection of tasks
    atomic<size_t> iterations;           /// Number of complete iterations so far
    size_t maxiterations;               /// maximum number of iterations
    size_t startvertex;
    size_t endtask;     /// last vertex to run
    controlled_termination terminator;
    size_t step;
    size_t blocksize;
    size_t ncpus;
    /** The local information for each thread
     * Each thread holds a block of vertices at a time
     * if block_begin == (-1) we are done
     */
    struct thread_information {
      size_t block_begin;
      size_t numv_remaining_in_block;
      char __pad__[64 - 2 * sizeof(size_t)];
    };
    std::vector<thread_information> thread_info;
  public:
    round_robin_scheduler(iengine_type* engine,
                          Graph &g, size_t ncpus):
      numvertices(g.num_vertices()),
      callback(engine),
      task_set(numvertices),
      maxiterations(0),
      startvertex(0),
      endtask(numvertices),
      step(1),
      ncpus(ncpus),
      thread_info(ncpus){
      cur_task.value = size_t(0);
      // adapt the blocksize. 
      // We want to minimize the possibilty of two cpus
      // picking up the same block, yet we want to maximize the block size
      // Lets just say 4 * ncpus blocks for now
      size_t nblocks = 4 * ncpus;
      blocksize = numvertices / nblocks;
      if (blocksize < 1) blocksize = 1;
    }

 
    ~round_robin_scheduler() { }

    callback_type& get_callback(size_t cpuid) {
      return callback;
    }

    void start() {
      cur_task.value = startvertex;
      endtask = startvertex + maxiterations * numvertices;
      std::cout << "max iterations = " << maxiterations << std::endl;
      std::cout << "step = " << step << std::endl;
      std::cout << "max_iterations = " << maxiterations << std::endl;
      
      for (size_t i = 0;i < thread_info.size(); ++i) {
        thread_info[i].numv_remaining_in_block = 0;
      }
    };
    
    void completed_task(size_t cpuid, const update_task_type &task){ };



    void set_max_iterations(size_t maxi) {
      maxiterations = maxi;
    }

    void add_task(update_task_type task, double priority) {
      if (task_set[task.vertex()].function() != NULL) {
        logger(LOG_WARNING, 
               "Adding task on vertex %d where a task already exists", 
               task.vertex());
      }
      task_set[task.vertex()] = update_task_type(task.vertex(), task.function());
    }

    void add_task_to_all(update_function_type func, double priority) {
      for (vertex_id_type vertex = 0; vertex < numvertices; ++vertex){
        add_task(update_task_type(vertex, func), priority);
      }
    }


    void add_tasks(const std::vector<vertex_id_type> &vertices,
                   update_function_type func,
                   double priority) {
      foreach(vertex_id_type vertex, vertices) {
        assert(vertex >=0 && vertex < numvertices);
        task_set[vertex] = update_task_type(vertex, func);
      } 
    }
  
    void set_start_vertex(size_t v){
      logstream(LOG_INFO) << "Round robin: Starting from " << v << std::endl;    
      startvertex = v;

    }

 
    /** Get the number of iterations that the scheduler has run */
    size_t get_iterations() {
      return iterations.value;
    }

    /** Get the next element in the queue */
    sched_status::status_enum get_next_task(size_t cpuid,
                                            update_task_type &ret_task) {
      thread_information& mythr_info = thread_info[cpuid];
      while(1) {
        if (__unlikely__(mythr_info.block_begin == (size_t)(-1))) return sched_status::EMPTY;
        while (mythr_info.numv_remaining_in_block != 0) {
          // get the task from the thread info
          size_t vid = mythr_info.block_begin;
          ret_task = task_set[vid];
          
          // update the thread info
          mythr_info.block_begin = mythr_info.block_begin + step;
          //
          //following line is equivalent to: 
          // if (block_begin >= numvertices) block_begin -= numvertices;
          //
          // if (block_begin < numvertices) is true, the right side of & evaluates to 0
          // otherwise it evaluates to (size_t)(-1)
          //
          mythr_info.block_begin -= numvertices & 
                                    ((size_t)(mythr_info.block_begin < numvertices) - 1);
          --mythr_info.numv_remaining_in_block;

          // return the task if available, otherwise try again
          if (__unlikely__(ret_task.vertex() == vertex_id_type(-1))) continue;
          else return sched_status::NEWTASK;
        }
        // we are here if the block has no vertices remaining
        // cur_task is the number of tasks issued so far.
        size_t taskid = cur_task.inc_ret_last(blocksize);
        // vertex corresponding to this task is
        size_t task_vertexid = (taskid * step) % numvertices;

        // update the iteration counter
        // we increment iteration counter if we cross the numvertices boundary
        size_t t = taskid % numvertices;
        if (t < numvertices && t + blocksize >= numvertices) {
          iterations.inc();
        }
        // set the block starting point
        mythr_info.block_begin = task_vertexid;
        
        // if maxiterations is set and we our block crosses the end task boundary
        //we may be done
        if (maxiterations != 0 && taskid + blocksize >= endtask) {
          // remaining tasks
          mythr_info.numv_remaining_in_block = endtask > taskid ? endtask - taskid : 0;
          // if there are no tasks then we are done
          if (mythr_info.numv_remaining_in_block == 0) {
            mythr_info.block_begin = (size_t)(-1);
            // ok... this is the tricky part
            // we are going to return empty until every one is done, then we set
            // the terminator.  
            bool done = true;
            for (size_t i = 0;i < thread_info.size(); ++i) {
              if (thread_info[i].block_begin != (size_t)(-1)) {
                done = false;
                break;
              }
            }
            if (done) terminator.complete();
          }
          continue;
        }
        else {
          mythr_info.numv_remaining_in_block = blocksize;
        }
      }
    }

    terminator_type& get_terminator() {
      return terminator;
    };

    void set_options(const scheduler_options &opts) {
      opts.get_int_option("max_iterations", maxiterations);
      opts.get_int_option("start_vertex", startvertex);
      opts.get_int_option("step", step);
      if (opts.get_int_option("block_size", blocksize) == false) {
        size_t nblocks = 4 * ncpus;
        blocksize = numvertices / nblocks;
      }
      if (blocksize < 1) blocksize = 1;
    }

    static void print_options_help(std::ostream &out) {
      out << "max_iterations = [integer, default = 0]\n";
      out << "start_vertex = [integer, default = 0]\n";
      out << "step = [integer which is either 1 or relatively prime to #vertices, default = 1]\n";
      out << "block_size = Scheduling block size. [integer, default = nvertices/(4*ncpus)]\n";
    }

    
    
  };

} // end of namespace graphlab
#include <graphlab/macros_undef.hpp>

#endif

