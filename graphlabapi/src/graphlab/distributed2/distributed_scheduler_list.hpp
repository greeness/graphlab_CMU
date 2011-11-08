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


#ifndef GRAPHLAB_DISTRIBUTED_SCHEDULER_LIST_HPP
#define GRAPHLAB_DISTRIBUTED_SCHEDULER_LIST_HPP
#include <string>
#include <vector>
#include <iostream>
#include <boost/preprocessor.hpp>

#define __DISTRIBUTED_SCHEDULER_LIST__                                              \
  (("sweep", sweep_scheduler,                                           \
    "very fast dynamic scheduler. Scans all vertices in sequence, "     \
    "running all update tasks on each vertex evaluated."))              \
  (("fifo", fifo_scheduler,                                             \
    "Standard FIFO task queue, poor parallelism, but task evaluation "  \
    "sequence is highly predictable. Useful for debugging and testing.")) \
  (("priority", priority_scheduler,                                     \
    "Standard Priority queue, poor parallelism, but task evaluation "   \
    "sequence is highly predictable. Useful for debugging"))            \
  (("multiqueue_fifo", multiqueue_fifo_scheduler,                       \
    "One or more FIFO task queues is assigned to each processor, "      \
    "where the queues are stochastically load balanced. Like the "      \
    "fifo scheduler, but less predictable, and much faster."))          \
  (("multiqueue_priority", multiqueue_priority_scheduler,               \
    "One or more Priority task queues is assigned to each processor, "  \
    "where the queues are stochastically load balanced. Like the "      \
    "priority scheduler, but less predictable, and much faster."))      


#include <graphlab/schedulers/fifo_scheduler.hpp>
#include <graphlab/schedulers/priority_scheduler.hpp>
#include <graphlab/schedulers/sweep_scheduler.hpp>
#include <graphlab/schedulers/multiqueue_fifo_scheduler.hpp>
#include <graphlab/schedulers/multiqueue_priority_scheduler.hpp>

namespace graphlab {
  /// get all the scheduler names
  std::vector<std::string> get_distributed_scheduler_names();

  /// get all the scheduler names concated into a string
  std::string get_distributed_scheduler_names_str();

  /// Display the scheduler options for a particular scheduler
  void print_distributed_scheduler_info(std::string s, std::ostream &out);
}

#endif

