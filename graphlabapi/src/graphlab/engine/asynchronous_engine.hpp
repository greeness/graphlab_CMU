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
 * This file includes the experimental components under active
 * development at Yahoo Labs Research.  For more information contact:
 *
 *  Joseph Gonzalez
 *  Email:  jegonzal@yahoo-inc.com
 *
 */


#ifndef GRAPHLAB_ASYNCHRONOUS_ENGINE_HPP
#define GRAPHLAB_ASYNCHRONOUS_ENGINE_HPP

#include <cmath>
#include <cassert>
#include <algorithm>
#include <boost/bind.hpp>

#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/parallel/atomic.hpp>
#include <graphlab/util/timer.hpp>
#include <graphlab/util/random.hpp>
#include <graphlab/util/mutable_queue.hpp>
#include <graphlab/util/counting_queue.hpp>

#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>
#include <graphlab/engine/iengine.hpp>
#include <graphlab/tasks/update_task.hpp>
#include <graphlab/logger/logger.hpp>
#include <graphlab/monitoring/imonitor.hpp>
#include <graphlab/shared_data/glshared.hpp>
#include <graphlab/engine/scope_manager_and_scheduler_wrapper.hpp>
#include <graphlab/metrics/metrics.hpp>

#include <graphlab/macros_def.hpp>
namespace graphlab {

  
  /**
   * This class defines a basic asynchronous engine
   */
  template<typename Graph, typename Scheduler, typename ScopeFactory>
  class asynchronous_engine : 
    public scope_manager_and_scheduler_wrapper<Graph, Scheduler, ScopeFactory> {

  public: // Type declerations

    typedef scope_manager_and_scheduler_wrapper<Graph, Scheduler, ScopeFactory> base;
    using base::apply_scheduler_options;
    using base::release_scheduler_and_scope_manager;
    using base::get_scheduler;
    using base::get_scope_manager;
    

    typedef iengine<Graph> iengine_base;
    typedef typename iengine_base::vertex_id_type vertex_id_type;
    typedef typename iengine_base::update_task_type update_task_type;
    typedef typename iengine_base::update_function_type update_function_type;
    typedef typename iengine_base::ischeduler_type ischeduler_type;
    typedef typename iengine_base::imonitor_type imonitor_type;
    typedef typename iengine_base::termination_function_type termination_function_type;
    typedef typename iengine_base::iscope_type iscope_type;
    
    typedef typename iengine_base::sync_function_type sync_function_type;
    typedef typename iengine_base::merge_function_type merge_function_type;


    /** The internal worker thread class used for the threaded engine */
    class engine_thread {      
      asynchronous_engine* engine;
      ScopeFactory* scope_manager;
      Scheduler* scheduler;
      size_t workerid;
    public:
      engine_thread() : engine(NULL), workerid(0) {  }
      void init(asynchronous_engine* _engine, 
                Scheduler* _scheduler, 
                ScopeFactory* _scope_manager, 
                size_t _workerid) {
        engine = _engine;
        scheduler = _scheduler;
        scope_manager = _scope_manager;
        workerid = _workerid;
      } // End of init      
      void run() {
        assert(engine != NULL);
        logger(LOG_INFO, "Worker %d started.\n", workerid);        
        /* Start consuming tasks while the engine is active*/
        engine->run_to_terminate(workerid, scheduler, scope_manager);
//         while(engine->active) {
//           bool executed_task = engine->run_once(workerid, scheduler, scope_manager);
//           // If this was nothing to execute then fail
//           if (!executed_task) break;
//         }   
        logger(LOG_INFO, "Worker %d finished.\n", workerid);
      }      
    }; // end of task worker


    class engine_thread_for_sync {
      asynchronous_engine* engine;
      size_t workerid;
      public:
      engine_thread_for_sync() : engine(NULL), workerid(0) { }
      void init(asynchronous_engine* _engine,
          size_t _workerid) {
        engine = _engine;
        workerid = _workerid;
      }// End of init

      void run() {
        assert(engine != NULL);
        logger(LOG_INFO, "Worker (Sync) %d started.\n", workerid);        
        engine-> sync_loop(workerid);
        logger(LOG_INFO, "Worker (Sync) %d finished.\n", workerid);
      }
    };

    

  private: // data members

    /** The graph that this engine is executing */
    Graph& graph;
    
    /** Number of cpus to use */
    size_t ncpus; 


    /** Use processor affinities */
    bool use_cpu_affinity;

    /** Use schedule yielding when waiting on the scheduler*/
    bool use_sched_yield;
    
    /** set to 1 if the processor is in the midst of asking scheduler for stuff
     *  and running an update */
    std::vector<padded_integer> proc_in_update;

    
    /** Track the number of updates */
    std::vector<size_t> update_counts;
    
    /** track an approximation to the number of updates. This 
        is only updated every (APX_INTERVAL+1) updates per thread.
        i.e. this can be off by at most (APX_INTERVAL+1)*Nthreads */
    atomic<size_t> apx_update_counts;
    
    /// frequency where apx_update_counts is updated. Must be a power of 2 - 1`
    static const size_t APX_INTERVAL = 127;

    atomic<size_t> numsyncs;
    
    /** The monitor which tracks and records engine events */
    imonitor_type* monitor;

    
    /** The time in millis that the engine was started */
    size_t start_time_millis;

    /** The timeout time in millis */
    size_t timeout_millis;

    /** The last time a check was run */
    size_t last_check_millis;
    
    /** The total number of tasks that should be executed */
    size_t task_budget;

    /** The termination functions */
    std::vector<termination_function_type> term_functions;

    /** Boolean that determins whether the engine_update is active */
    bool active; 

    /** Boolean that determins whether the engine_sync is active */
    bool sync_active;

    
    /** The cause of the last termination condition */
    const char* exception_message;
    exec_status termination_reason;

    scope_range::scope_range_enum default_scope_range;

    barrier sync_barrier;

    std::vector<any> sync_accumulators;





    struct sync_task {
      sync_function_type sync_fun;
      merge_function_type merge_fun;
      glshared_base::apply_function_type apply_fun;
      size_t sync_interval;
      size_t next_time;
      any zero;
      mutex lock;
      vertex_id_type rangelow;
      vertex_id_type rangehigh;
      glshared_base *sharedvariable;
      sync_task() :
        sync_fun(NULL), merge_fun(NULL), apply_fun(NULL),
        sync_interval(-1),
        next_time(0), rangelow(0), 
        rangehigh(vertex_id_type(-1)), sharedvariable(NULL) { }
    };
    
    /// A list of all registered sync tasks
    std::vector<sync_task> sync_tasks;
    
    /// A map from the shared variable to the sync task
    std::map<glshared_base*, size_t> var2synctask;

    /// Sync Tasks ordered by the negative of the next update time. (it is a max-heap)
    mutable_queue<size_t, int> sync_task_queue;
    std::pair<size_t, int> sync_task_queue_head;


    // Add instant task queue
    counting_queue<size_t> task_exec_queue;

    /// the lock protecting the sync_task_queue
    mutex sync_task_queue_lock;
    /// The priority of the head of the queue
    size_t sync_task_queue_next_update;

    /// Metrics logging
    metrics engine_metrics, scheduler_metrics;
    
    thread_group syncthreads;
    std::vector<engine_thread_for_sync> syncers;
    
    mutex sync_now_lock;
    conditional sync_now_cond;
    size_t sync_now_counts;
    
  public:

    /**
     * Create an asynchronous engine.
     *
     * The execution type specifies whether the engine should simulate
     * threads or use actual threads. 
     */
    asynchronous_engine(Graph& graph,
                        size_t ncpus = 1) :
      scope_manager_and_scheduler_wrapper<Graph,
                                          Scheduler,
                                          ScopeFactory>(graph,ncpus),
      graph(graph),
      ncpus( std::max(ncpus, size_t(1)) ),
      use_cpu_affinity(false),
      use_sched_yield(true),
      proc_in_update(std::max(ncpus, size_t(1))),
      update_counts(std::max(ncpus, size_t(1)), 0),
      monitor(NULL),
      start_time_millis(lowres_time_millis()),
      timeout_millis(0),
      last_check_millis(0),
      task_budget(0),
      active(false),
      sync_active(true),
      exception_message(NULL),
      termination_reason(EXEC_UNSET),
      default_scope_range(scope_range::EDGE_CONSISTENCY),
      sync_barrier(ncpus),
      sync_accumulators(ncpus),
      task_exec_queue(ncpus),
      engine_metrics("engine"),
      sync_now_counts(0){
      
      syncers.resize(ncpus);
      for (size_t i = 0; i < ncpus; ++i) {
        // Initialize the syncers. 
        syncers[i].init(this, i);
        if(use_cpu_affinity)  {
          syncthreads.launch(boost::bind(&engine_thread_for_sync::run, &(syncers[i])), i);
        } else {
          syncthreads.launch(boost::bind(&engine_thread_for_sync::run, &(syncers[i])));
        }
      }

    }

    ~asynchronous_engine() { 
      sync_task_queue_lock.lock();
      sync_active = false;
      task_exec_queue.stop_blocking();
      sync_task_queue_lock.unlock();
      while (syncthreads.running_threads() > 0) {
        try {
          syncthreads.join();
        }
        catch(const char* c) {
          logstream(LOG_ERROR) << "Exception Caught: " << c << std::endl;
        }
      }
    }

    //! Get the number of cpus
    size_t get_ncpus() const { return ncpus; }

    //! set sched yield
    void set_sched_yield(bool value) {
      use_sched_yield = value;
    }

    void set_cpu_affinities(bool value) {
      use_cpu_affinity = value;
    }

    void set_engine_options(const scheduler_options& opts) { }
    
    static void print_options_help(std::ostream& out) { }


    /**
     * Set the default scope range.  The scope ranges are defined in
     * iscope.hpp
     */
    void set_default_scope(scope_range::scope_range_enum default_scope_range_) {
      default_scope_range = default_scope_range_;
    }
  
    using iengine<Graph>::exec_status_as_string;

    /** Execute the engine */
    void start() {
      // call the scope_manager_and_scheduler_wrapper for the scheduler
      // and scope manager
      Scheduler* scheduler = get_scheduler();
      apply_scheduler_options();
      scheduler->register_monitor(monitor);
      ScopeFactory* scope_manager = get_scope_manager();

      scope_manager->set_default_scope(default_scope_range);
      
      // std::cout << "Scheduler Options:\n";
      // std::cout << sched_options();
      
      /*
       * Prepare data structures for execution:
       * 1) finalize the graph.
       * 2) Reset engine fields
       */
      // Prepare the graph
      graph.finalize();      
      // Clear the update counts
      
      for (size_t i = 0;i < proc_in_update.size(); ++i) proc_in_update[i].val = 0;

      std::fill(update_counts.begin(), update_counts.end(), 0);
      apx_update_counts.value = 0;
      numsyncs.value = 0;
      // Reset timers
      start_time_millis = lowres_time_millis();
      last_check_millis = 0;
      // Reset active flag
      active = true;  
      // Reset the last exec status 
      exception_message = NULL;
      termination_reason = EXEC_TASK_DEPLETION;
      // Start any scheduler threads (if necessary)
      // initialize the local sync queue
      construct_sync_queue();
      ensure_all_sync_vars_are_unique();
      // evaluate all syncs
      scheduler->start();
      
      run_threaded(scheduler, scope_manager);

      // complete sync of all variables
      for (size_t i = 0;i < sync_tasks.size(); ++i) {
        sync_now(*sync_tasks[i].sharedvariable);
      }
      scheduler_metrics = scheduler->get_metrics();
      release_scheduler_and_scope_manager();
      


      // Metrics: update counts
      for(size_t i = 0; i < update_counts.size(); ++i) {
        engine_metrics.add("updatecount", 
                           (double)update_counts[i], INTEGER);
        engine_metrics.add_vector_entry("updatecount_vector", i, (double)update_counts[i]);
      }
      engine_metrics.add("runtime",
                         ((double)lowres_time_millis()-(double)start_time_millis)*0.001, TIME);
      engine_metrics.set("termination_reason", 
                         exec_status_as_string(termination_reason));

      engine_metrics.set_integer("num_vertices", graph.num_vertices());
      engine_metrics.set_integer("num_edges", graph.num_edges());
      engine_metrics.set_integer("num_syncs", numsyncs.value);
      
      // ok. if death was due to an exception, rethrow
      if (termination_reason == EXEC_EXCEPTION) {
        throw(exception_message);
      }
    } 


    /**
     * Stop the engine
     */
    void stop() {
      termination_reason = EXEC_FORCED_ABORT;
      active = false;
    }
    
    metrics get_metrics() {
      return engine_metrics;
    }


    void reset_metrics() {
      engine_metrics.clear();
      // do a deeper clear of the scheduler metrics
      // otherwise dump metrics still output a metrics block
      scheduler_metrics = metrics();
    }

    void report_metrics(imetrics_reporter &reporter) {
      engine_metrics.report(reporter);
      scheduler_metrics.report(reporter);
    }
    
    /**
     * Return the reason why the engine last terminated
     */
    exec_status last_exec_status() const {
      return termination_reason;
    }

    
    /**
     * This function computes the last update count by adding all the
     * update counts of the individual threads.  This is an underestimate
     * if the engine is currently running.
     */
    size_t last_update_count() const {
      size_t sum = 0;
      for(size_t i = 0; i < update_counts.size(); ++i)
        sum += update_counts[i];
      return sum;
    } // end of last_update_count

    /** This function provides an approximation to the last update count.
     * This is a faster version of last_update_count and may be off
     by at most (APX_INTERVAL+1)*Nthreads */
    inline size_t approximate_last_update_count() const {
      return apx_update_counts.value;
    }
    
    /**
     * Register a monitor with this engine.  Currently this engine
     * only supports a single monitor. 
     */
    void register_monitor(imonitor<Graph>* _monitor = NULL) {
      monitor = _monitor;
      if(monitor != NULL) monitor->init(this);
    } // end of register monitor


    /**
     * Add a terminator to the engine.
     */
    void add_terminator(termination_function_type term) {
      term_functions.push_back(term);
    }


    /**
     * Clear all terminators from the engine
     */
    void clear_terminators() {
      term_functions.clear();
    }



    /**
     * Timeout. Default - no timeout. 
     */
    void set_timeout(size_t timeout_seconds = 0) {
      timeout_millis = timeout_seconds * 1000;
    }
    
    /**
     * Task budget - max number of tasks to allow
     */
    virtual void set_task_budget(size_t max_tasks) {
      task_budget = max_tasks;
    }
    

    /**
     * \brief Registers a sync with the engine.
     *
     * Registers a sync with the engine.
     * The sync will be performed approximately every "interval" updates,
     * and will perform a reduction over all vertices from rangelow
     * to rangehigh inclusive.
     * The merge function may be NULL, in which it will not be used.
     * However, it is highly recommended to provide a merge function since
     * this allow the sync operation to be parallelized.
     *
      * The sync operation is guaranteed to be strictly sequentially consistent
     * with all other execution.
     *
     * \param shared The shared variable to synchronize
     * \param sync The reduction function
     * \param apply The final apply function which writes to the shared value
     * \param zero The initial zero value passed to the reduction
     * \param sync_interval Frequency at which the sync is initiated.
     *                      Corresponds approximately to the number of
     *                     update function calls before the sync is reevaluated.
     *                     If 0, the sync will only be evaluated once
     *                     at engine start,  and will never be evaluated again.
     *                     Defaults to 0.
     * \param merge Combined intermediate reduction value. defaults to NULL.
     *              in which case, it will not be used.
     * \param rangelow he lower range of vertex id to start syncing.
     *                 The range is inclusive. i.e. vertex with id 'rangelow'
     *                 and vertex with id 'rangehigh' will be included.
     *                 Defaults to 0.
     * \param rangehigh The upper range of vertex id to stop syncing.
     *                  The range is inclusive. i.e. vertex with id 'rangelow'
     *                  and vertex with id 'rangehigh' will be included.
     *                  Defaults to infinity.
     */
     void set_sync(glshared_base& shared,
                  sync_function_type sync,
                  glshared_base::apply_function_type apply,
                  const any& zero,
                  size_t sync_interval = 0,
                  merge_function_type merge = NULL,
                  vertex_id_type rangelow = 0,
                  vertex_id_type rangehigh = -1) {
      sync_task st;
      st.sync_fun = sync;
      st.merge_fun = merge;
      st.apply_fun = apply;
      st.sync_interval = sync_interval;
      st.next_time = 0;
      st.zero = zero;
      st.rangelow = rangelow;
      st.rangehigh = rangehigh;
      st.sharedvariable = &shared;
      sync_tasks.push_back(st);
      var2synctask[&shared] = sync_tasks.size() - 1;
      if (merge == NULL) {
        logger(LOG_WARNING, 
                "Syncs without a merge function defined are not "
               "parallelized and may be slow on large graphs.");
      }
    }

    /**
     * Performs a sync immediately. This function requires that the shared
     * variable already be registered with the engine.
     * and that the engine is not currently running. 
     * \todo This sync currently does not evaluate in parallel. 
     */
    void sync_now(glshared_base& shared) {
      ASSERT_FALSE(active);
      // makes sure the sync registration exists
      std::map<glshared_base*, size_t>::iterator iter = var2synctask.find(&shared);
      ASSERT_TRUE(iter != var2synctask.end());
      sync_now_lock.lock();
      size_t cur_sync_now_value = sync_now_counts;
      sync_now_lock.unlock();
      
      task_exec_queue.enqueue(iter->second);
      task_exec_queue.broadcast();
      // wait for completion
      sync_now_lock.lock();
      while (sync_now_counts == cur_sync_now_value) {
        sync_now_cond.wait(sync_now_lock);
      }
      sync_now_lock.unlock();
      
    }
    

    /**
     * Do not use. 
     *  When called during engine execution, will synchronize
     * the provided shared variable as soon as possible.
     * \todo Provide access to this from the update functions
     */
    void sync_soon(glshared_base& shared) {
      ASSERT_MSG(false, "Deprecated");
    }
    
    /**
     * Do not use.
     * when called during engine execution, will synchronize
     * all shared variable as soon as possible.
     * \todo Provide access to this from the update functions
     */
    void sync_all_soon() {
      ASSERT_MSG(false, "Deprecated");
    }
    
  protected: // internal functions

    /**
     * Execute the engine using actual threads
     */
    void run_threaded(Scheduler* scheduler, ScopeFactory* scope_manager) {
      /* Initialize a pool of threads */
      std::vector<engine_thread> workers(ncpus);

      
      thread_group threads;

      
      for(size_t i = 0; i < ncpus; ++i) {
        // Initialize the worker
        workers[i].init(this, scheduler, scope_manager, i);

        // Start the worker thread using the thread group with cpu
        // affinity attached (CPU affinity currently only supported in
        // linux) since Mac affinity is set through the NX frameworks
        if(use_cpu_affinity)  {
          threads.launch(boost::bind(&engine_thread::run, &(workers[i])), i);
        } else {
          threads.launch(boost::bind(&engine_thread::run, &(workers[i])));
        }
      }

      while (threads.running_threads() > 0) {
        try {
          threads.join();
        }
        catch(const char* c) {
          logstream(LOG_ERROR) << "Exception Caught: " << c << std::endl;
          // killall the running threads
          exception_message = c;
          termination_reason = EXEC_EXCEPTION;
          active = false;
        }
      }
    } // end of run threaded


    
    /**
     * Check all the terminators to identify if any termination
     * condition has been met. If a termination condition is met this
     * function returns true otherwise it will return false.  
     */
    bool satisfies_termination_condition() {
      /**
       * Timeout termination condition.
       *
       * If a timeout was set and the current time is greater than the
       * timeout point return true
       */
      if(timeout_millis > 0 && 
         start_time_millis + timeout_millis < lowres_time_millis()) {
        termination_reason = EXEC_TIMEOUT;
        // Deactivate the engine
        return true;
      }

      /**
       * Task budget termination condition
       *
       * If the task budget is greater than zero and the last update
       * count exceeds the task budget then terminate
       */
      if(task_budget > 0 && last_update_count() > task_budget) {
        termination_reason = EXEC_TASK_BUDGET_EXCEEDED;
        // Deactivate the engine
        return true;
      }

      /**
       * Check all the terminators.
       *
       * If a shared data manager is not provided then this will fail.
       */
      for (size_t i = 0; i < term_functions.size(); ++i) {
        if (term_functions[i]()) {
          termination_reason = EXEC_TERM_FUNCTION;
          return true;
        }
      }
      // No termination condition was met
      return false;
    } // End of satisfies termination condition



    // Do not use.
    bool run_once(size_t cpuid, 
                  Scheduler* scheduler, 
                  ScopeFactory* scope_manager) {
      // Loop until we get a task for recieve a termination signal
      while(active) {
        evaluate_sync_queue(scope_manager, 
                            cpuid, 
                            approximate_last_update_count());
        /**
         * Run any pending syncs and then test all termination
         * conditions.
         */
        if (last_check_millis < lowres_time_millis()) {
          last_check_millis = lowres_time_millis();
          // Check all termination conditions
          if(satisfies_termination_condition()) {
            /* 
            terminate_all_syncs_mutex.lock();
            terminate_all_syncs = 1;
            terminate_all_syncs_cond.broadcast();
            terminate_all_syncs_mutex.unlock();
            */
            active = false;
            return false;
          }
        }
        
        /**
         * Get and execute the next task from the scheduler.
         */
        update_task_type task;
        sched_status::status_enum stat = scheduler->get_next_task(cpuid, task);
        
        if (stat == sched_status::EMPTY) {
          // check the schedule terminator
          scheduler->get_terminator().begin_critical_section(cpuid);
          stat = scheduler->get_next_task(cpuid, task);
          if (stat == sched_status::NEWTASK) {
            scheduler->get_terminator().cancel_critical_section(cpuid);
          }
          else {
            if (scheduler->get_terminator().end_critical_section(cpuid)) {
              termination_reason = EXEC_TASK_DEPLETION;
              active = false;
              return false;
            }
            else {
              if(use_sched_yield) sched_yield();
              else return true;
            }
          }
        }
        
        if (stat == sched_status::NEWTASK) {
          // If the status is new task than we must execute the task
          const vertex_id_type vertex = task.vertex();
          assert(vertex < graph.num_vertices());
          assert(task.function() != NULL);
          // Lock the vertex to ensure that no other processor tries
          // to take it build a scope
          iscope_type* scope = scope_manager->get_scope(cpuid, vertex);
          assert(scope != NULL);                    
          // get the callback for this cpu
          typename Scheduler::callback_type& scallback = 
                                      scheduler->get_callback(cpuid);
          // execute the task
          
          task.function()(*scope, scallback);
          // Commit any changes to the scope
          scope->commit();
          // Release the scope
          scope_manager->release_scope(scope);
          // Mark the task as completed in the scheduler
          scheduler->completed_task(cpuid, task);
          // record the successful execution of the task
          if ((update_counts[cpuid] & APX_INTERVAL) == APX_INTERVAL) {
            apx_update_counts.inc(APX_INTERVAL + 1);
          }
          update_counts[cpuid]++;
          
          return true;
          break;
        } // end of switch
      } // end of while(true)
      // If this point is reached 
      return false;
    } // End of run once
    

    

    /** runs the engine to termination. 
     * \note Do not use for simulated engine
    */
    void run_to_terminate(size_t cpuid, 
                  Scheduler* scheduler, 
                  ScopeFactory* scope_manager) {
      // Loop until we get a task for recieve a termination signal
      size_t ctr = 0;
      size_t updcount = 0;
      bool isempty = false;
      while(active) {
        if (__builtin_expect(ctr == 0 || isempty, 0)) {
          if (cpuid == 0) { 
            evaluate_sync_queue(scope_manager,
                cpuid,
                approximate_last_update_count());
          }

          size_t timemillis = lowres_time_millis();
          size_t curupdatecount = approximate_last_update_count();
          if (last_check_millis < timemillis || isempty) {
            last_check_millis = timemillis;
            if (satisfies_termination_condition()) {
              active = false;
              break;
            }
          }
          // estimate the next ctrlimit
          //
          // compute average update rate per millisecond. with a prior of 16K per 1000ms
          ctr = 1 + ((curupdatecount + 16000) / (timemillis + 1000)) * 100;

          if (sync_task_queue_next_update > curupdatecount) {
            ctr = std::min(ctr, 1 + (sync_task_queue_next_update - curupdatecount) / ncpus);
          }
        }
        --ctr;

        /**
         * Get and execute the next task from the scheduler.
         */
        proc_in_update[cpuid].val = 1;
        
        update_task_type task;
        sched_status::status_enum stat = scheduler->get_next_task(cpuid, task);
        
        if (stat == sched_status::EMPTY) {
          isempty = true;
          // check the schedule terminator
          scheduler->get_terminator().begin_critical_section(cpuid);
          stat = scheduler->get_next_task(cpuid, task);
          if (stat == sched_status::NEWTASK) {
            scheduler->get_terminator().cancel_critical_section(cpuid);
          }
          else {
            if (scheduler->get_terminator().end_critical_section(cpuid)) {
              active = false;
            }
            else {
              if(use_sched_yield) sched_yield();
            }
          }
        }
        
        if (stat == sched_status::NEWTASK) {
          isempty = false;
          // If the status is new task than we must execute the task
          const vertex_id_type vertex = task.vertex();
          assert(vertex < graph.num_vertices());
          assert(task.function() != NULL);

          // Lock the vertex to ensure that no other processor tries
          // to take it build a scope
          iscope_type* scope = scope_manager->get_scope(cpuid, vertex);
          assert(scope != NULL);                    
          // get the callback for this cpu
          typename Scheduler::callback_type& scallback = 
                                      scheduler->get_callback(cpuid);
          // execute the task
          
          task.function()(*scope, scallback);
          // Commit any changes to the scope
          scope->commit();
          // Release the scope
          scope_manager->release_scope(scope);
          //logger(LOG_DEBUG  , "Worker %d released lock on %d.\n", cpuid, vertex);        

          // Mark the task as completed in the scheduler
          scheduler->completed_task(cpuid, task);
          // record the successful execution of the task
          if ((updcount & APX_INTERVAL) == APX_INTERVAL) {
            apx_update_counts.inc(APX_INTERVAL + 1);
          }
          update_counts[cpuid]++;
        } 
        
        proc_in_update[cpuid].val = 0;
      } // end of while(true)
      //update_counts[cpuid] += updcount;
      // loop until all processors are either
      // 1: here. or 
      // 2: waiting inside the evaluate_sync_queue function
      size_t numpinupdate = 1;
      while(numpinupdate > 0) {
        numpinupdate = 0;
        for (size_t i = 0;i < proc_in_update.size(); ++i) {
          numpinupdate += proc_in_update[cpuid].val;
        }
        sched_yield();
      }
    }
    
    void construct_sync_queue() {
      sync_task_queue.clear();
      size_t min_sync_interval = size_t(-1);
      for (size_t i = 0;i < sync_tasks.size(); ++i) {
        sync_task_queue.push(i, 0);
        if (sync_tasks[i].sync_interval > 0) {
          min_sync_interval = std::min(min_sync_interval, sync_tasks[i].sync_interval);
        }
      }
      if (min_sync_interval < ncpus * APX_INTERVAL) {
        logger(LOG_WARNING, 
               "Sync interval is too short."
               "Engine may not be able to achieve desired Sync frequency");
      }
      if (sync_task_queue.size() > 0) sync_task_queue_next_update = 0;
      else sync_task_queue_next_update = size_t(-1);
    }


    void sync_loop(size_t cpuid) {
      size_t syncid = 0;
      while(true) {
        // Block until the update threads signal the sync condition.
        std::pair<size_t, bool> syncid_succ = task_exec_queue.poll_till_pop();
        if (syncid_succ.second == false) return;

        ScopeFactory* scope_manager = get_scope_manager();
        // Each syncer tries to acquire its share of the graph.
        size_t numv = graph.num_vertices();
        size_t v_per_cpu = 1+ (numv-1)/(ncpus);
        size_t v_start = v_per_cpu * cpuid;
        size_t v_end = std::min(numv-1, v_start + v_per_cpu-1);
        scope_manager->acquire_range_lock(v_start, v_end);

        
        sync_barrier.wait();
        

        parallel_evaluate_sync(syncid, scope_manager, cpuid);


        scope_manager->release_range_lock(v_start, v_end);
        // engine is not active. This is a sync now
        if (cpuid == 0 && active == false) {
          sync_now_lock.lock();
          sync_now_counts++;
          sync_now_cond.signal();
          sync_now_lock.unlock();
        }
      }
    }


    // Assumptions: The all syncer threads have got the lock of the entire graph.
    void parallel_evaluate_sync(size_t syncid, 
                                 ScopeFactory* scope_manager,
                                 size_t cpuid) {
        if (sync_tasks[syncid].merge_fun != NULL) {
        // Threaded engine and we have a merge function 
        // we can do a parallel reduction
        if (cpuid == 0) {
          numsyncs.inc();
        }
        // wait for all threads to get here
        sync_task &sync = sync_tasks[syncid];

        // yes we do have a merge function
        // lets do a parallel reduction
        
        // # get the range of vertices
        const vertex_id_type vmin = sync.rangelow;
        const vertex_id_type vmax = std::min(sync.rangehigh, vertex_id_type(graph.num_vertices()));
        // slice the range into ncpus
        const vertex_id_type nverts = vmax - vmin;
        // get my true range
        const vertex_id_type v_mymin = vertex_id_type(vmin + (nverts * cpuid) / ncpus);
        const vertex_id_type v_mymax = vertex_id_type(vmin + (nverts * (cpuid + 1)) / ncpus);
        
        //accumulate through all the vertices
        any& accumulator = sync_accumulators[cpuid];
        accumulator = sync.zero;
        for (vertex_id_type i = v_mymin; i < v_mymax; ++i) {
          iscope_type* scope = scope_manager->get_scope(ncpus+cpuid, i, 
                                          scope_range::NULL_CONSISTENCY);
          sync.sync_fun(*scope, accumulator);
          scope->commit();
          scope_manager->release_scope(scope);
        }

        sync_barrier.wait();

        // merge. Currently done only on one CPU
        // we could conceivably do a tree merge.
        // TODO: Tree merge
        if (cpuid == 0) {
          any& mergeresult = sync_accumulators[0];
          for (size_t i = 1; i < sync_accumulators.size(); ++i) {
            sync.merge_fun(mergeresult, sync_accumulators[i]);
          }
          sync.sharedvariable->apply(sync.apply_fun, accumulator);
        }
      } else {
        // simulated engine, or no merge function.
        // we have to do the sync sequentially
        if (cpuid == 0) {
          numsyncs.inc();
          sync_task &sync = sync_tasks[syncid];
          // # get the range of vertices
          const vertex_id_type vmin = sync.rangelow;
          const vertex_id_type vmax = 
            vertex_id_type(std::min(sync.rangehigh, (vertex_id_type)(graph.num_vertices() - 1)));
          
          //accumulate through all the vertices
          any accumulator = sync.zero;
          for (vertex_id_type i = vmin; i <= vmax; ++i) {
            iscope_type* scope = scope_manager->get_scope(cpuid, i,
                                          scope_range::NULL_CONSISTENCY);
            scope->commit();
            scope_manager->release_scope(scope);
          }
          sync.sharedvariable->apply(sync.apply_fun, accumulator);
        }
      }
    }

    
    void ensure_all_sync_vars_are_unique() {
      for (size_t i = 0;i < sync_tasks.size(); ++i) {
        ASSERT_MSG(sync_tasks[i].sharedvariable->is_unique(), 
                   "All shared pointers to synced variables should be released "
                   "before calling engine start!");
      }
    }
    
    // evaluate the sync queue. Loop through at most max_sync times.
    // Should only be called by cpu0.
    void evaluate_sync_queue(ScopeFactory* scope_manager,
                             size_t cpuid,
                             size_t curupdatecount) {
      bool is_cpu0 = (cpuid == 0);

      if (!is_cpu0) return;

      // if the head of the queue is ready
      while (sync_task_queue_next_update <= curupdatecount) {

        // wait for all threads to reach here.
        // Become probelmatic when having 2 groups of threads: update, sync
        bool hastask = !sync_task_queue.empty() && 
                      (size_t)(-(sync_task_queue.top().second)) <= curupdatecount;
                      

        // no task to do. Return
        if (hastask == false) {
          // cpu 0 updates the head tracker
            if (active && !sync_task_queue.empty()) {
              sync_task_queue_next_update = -(sync_task_queue.top().second);
            }
            else {
              sync_task_queue_next_update = size_t(-1);
            }
          return;
        }
        
        sync_task_queue_head = sync_task_queue.pop();

        // go for it. Evaluate the extracted task
        // Put the syncid into the exec task queue, and signal waiting syncers.
        task_exec_queue.enqueue(sync_task_queue_head.first);
        task_exec_queue.broadcast();
        // put it back if the interval is postive
        if (sync_tasks[sync_task_queue_head.first].sync_interval > 0) {
          int next_time((int)(approximate_last_update_count() + 
                        sync_tasks[sync_task_queue_head.first].sync_interval));
          sync_task_queue.insert_max(sync_task_queue_head.first, -next_time);
        }
        
        // update the head tracker
        if (active && !sync_task_queue.empty()) {
          sync_task_queue_next_update = -(sync_task_queue.top().second);
        }
        else {
          sync_task_queue_next_update = size_t(-1);
        }
      }
    }
    
  }; // end of asynchronous engine
  

}; // end of namespace graphlab
#include <graphlab/macros_undef.hpp>

#endif

