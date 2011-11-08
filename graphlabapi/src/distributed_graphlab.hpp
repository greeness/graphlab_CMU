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


#ifndef GRAPHLAB_DISTRIBUTED_MASTER_INCLUDES
#define GRAPHLAB_DISTRIBUTED_MASTER_INCLUDES



//
#include <graphlab/distributed2/distributed2_includes.hpp>
#include <graphlab/engine/engine_includes.hpp>
#include <graphlab/factors/factor_includes.hpp>
#include <graphlab/graph/graph_includes.hpp>
#include <graphlab/logger/logger_includes.hpp>
#include <graphlab/monitoring/monitoring_includes.hpp>
#include <graphlab/parallel/parallel_includes.hpp>
#include <graphlab/schedulers/scheduler_includes.hpp>
#include <graphlab/scope/scope_includes.hpp>
#include <graphlab/serialization/serialization_includes.hpp>
#include <graphlab/shared_data/shared_data_includes.hpp>
#include <graphlab/tasks/task_includes.hpp>
#include <graphlab/util/util_includes.hpp>


#include <graphlab/distributed_core.hpp>




/**
   \namespace graphlab
   \brief The namespace containing all graphlab objects and functions.

   All objects and functions used in graphlab are contained within the
   graphlab namespace.  Forexample to access the graph type a user
   must therefore either include the graphlab namespace or use:

   <code>
     graphlab::graph<VertexType, EdgeType>
   </code>

   Because most of the graphlab types depend on the graph type we have
   created a templated struct called graphlab::types. \todo finish
   explanation.
                           
*/
namespace graphlab {
 
  /**
  \brief A types datastructure which provides convenient specializations of all
  user-facing GraphLab types.
  
  GraphLab is heavily templatized. The graphlab::types object provides a 
  convenient way to access the GraphLab classes without requiring excessive 
  angle brackets (< , >). The GraphLab types object is located in <graphlab.hpp>. 
  To define a graphlab type object:
  
  \code
  typedef graphlab::graph<vertex_data, edge_data> graph_type;
  typedef graphlab::types<graph_type> gl;
  \endcode
  
  Now we can use gl::... to access all the available graphlab types. 
  */
  template<typename Graph>
  struct distributed_types {
   
    /**
       The type of the graph
     */
    typedef Graph graph;

    /**
     The type of the shared memory graph
    */
    typedef graphlab::graph<typename Graph::vertex_data_type,
                            typename Graph::edge_data_type> memory_graph;

    /**
     The type of the disk graph
    */
    typedef graphlab::disk_graph<typename Graph::vertex_data_type,
                                  typename Graph::edge_data_type> disk_graph;

    /**
     The type of the distributed graph
    */
    typedef graphlab::distributed_graph<typename Graph::vertex_data_type,
                                        typename Graph::edge_data_type> distributed_graph;

    /** \brief A convenient wrapper object around the commonly used
    portions of GraphLab.  This is useful for most GraphLab
    applications. See the \ref graphlab::distributed_core object for more details.
    */
    typedef graphlab::distributed_core<typename distributed_graph::vertex_data_type,
                                       typename distributed_graph::edge_data_type> distributed_core;
    typedef distributed_core core;

    typedef graphlab::command_line_options command_line_options;
    typedef graphlab::engine_options engine_options;
    
    /// \brief The type of the data stored on each vertex of the Graph. 
    typedef typename distributed_graph::vertex_data_type vertex_data_type;
    
    /// \brief The type of the data stored on each edge of the Graph.   
    typedef typename distributed_graph::edge_data_type   edge_data_type;
    
    typedef graphlab::update_task<distributed_graph>        update_task;
    typedef typename update_task::update_function_type update_function;
    
    typedef graphlab::iscope<distributed_graph>              iscope;
    typedef graphlab::ischeduler<distributed_graph>          ischeduler;
    typedef graphlab::icallback<distributed_graph>           icallback;
    typedef graphlab::iengine<distributed_graph>             iengine;
    typedef graphlab::imonitor<distributed_graph>            imonitor;

    typedef graphlab::glshared_sync_ops<Graph> glshared_sync_ops;
    typedef graphlab::glshared_apply_ops glshared_apply_ops;
    typedef graphlab::glshared_merge_ops glshared_merge_ops;


    template <typename Scheduler>
    class distributed_locking_engine: public graphlab::distributed_locking_engine<distributed_graph, Scheduler> { };
    class distributed_chromatic_engine: public graphlab::distributed_chromatic_engine<distributed_graph> { };


    typedef graphlab::fifo_scheduler<distributed_graph> fifo_scheduler;
    typedef graphlab::priority_scheduler<distributed_graph> priority_scheduler;
    typedef graphlab::sampling_scheduler<distributed_graph> sampling_scheduler;
    typedef graphlab::sweep_scheduler<distributed_graph> sweep_scheduler;
    typedef graphlab::multiqueue_fifo_scheduler<distributed_graph> multiqueue_fifo_scheduler;
    typedef graphlab::multiqueue_priority_scheduler<distributed_graph> multiqueue_priority_scheduler;
    typedef graphlab::clustered_priority_scheduler<distributed_graph> clustered_priority_scheduler;
    typedef graphlab::round_robin_scheduler<distributed_graph> round_robin_scheduler;
    typedef graphlab::chromatic_scheduler<distributed_graph> chromatic_scheduler;
    
    
    
    
    

    /// \brief The type of id assigned to each vertex. Equivalent to graphlab::vertex_id_t
    typedef typename graph::vertex_id_type vertex_id;
    __attribute__((__deprecated__)) typedef vertex_id vertex_id_t;
    

    /// \brief The type of id assigned to each vertex. Equivalent to graphlab::edge_id_t
    typedef typename graph::edge_id_type edge_id;
    __attribute__((__deprecated__)) typedef edge_id edge_id_t;
 

    typedef typename graph::edge_list_type edge_list;

       
    typedef graphlab::scheduler_options          scheduler_options;
    typedef graphlab::sched_status               sched_status;
    typedef graphlab::graph_partitioner          graph_partitioner;
    typedef graphlab::scope_range scope_range;

    template <typename T>
    class glshared:public graphlab::glshared<T> { };

    template <typename T>
    class distributed_glshared:public graphlab::distributed_glshared<T> { };
  };

}


#endif

