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


#ifndef GRAPHLAB_GLSHARED_HPP
#define GRAPHLAB_GLSHARED_HPP
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/type_traits/function_traits.hpp>
#include <boost/type_traits/remove_reference.hpp>

#include <graphlab/parallel/atomic.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/util/generics/any.hpp>
#include <graphlab/logger/assertions.hpp>

namespace graphlab {

  /**
   * Common base class for all glshared<T> objects.  Exposes a common
   *  interface allowing all glshared<T> objects to be manipulated in
   *  the same way.  
   */
  class glshared_base{
  public:
    /**
     * The type of an apply function. The apply function performs an
     *  atomic operation on the contents of a shared object. The apply
     *  function takes a reference to the current value of the object
     *  (current_value: wrapped inside an any), an additional
     *  parameter (param), and makes modifications to the current
     *  value.
     */
    typedef void(*apply_function_type)(any& current_data, const any& param);

    /**
     * Gets the value of the shared variable wrapped in an any.
     */
    virtual any get_any() const = 0;
  
    /**
     * Sets the value of the shared variable using an any. The type of
     * the any must match the type of the shared object.
     */
    virtual void set_any(const any&) = 0;
  
    /**
     * Performs an atomic modification on the value of the shared
     * object.  essentially calls fun(current_value, srcd) where
     * current_value is the value of this variable wrapped inside an
     * any.
     */
    virtual void apply(apply_function_type fun,
                       const any& srcd) = 0;
                         
    /**
     * Returns true if there are no other active references to this
     * variable.
     */
    virtual bool is_unique() const = 0;
  };



  namespace glshared_impl {
    template <typename T>
    struct empty_deleter {
      void operator()(T* d) { }
    };
  };




  /**
   * \brief A shared data entry.  
   *
   * glshared<datatype> variable; 
   * will create a shared variable of the defined datatype.
   * The accessor functions get_val(), get_ptr() and set() can be used
   * to access the data in parallel. An RCU mechanism is used to ensure
   * consistency of the stored data. 
   * The variable can be registered with a GraphLab
   * engine to provide global aggregate information of a graph during
   * GraphLab execution. \see asynchronous_engine::set_sync
   *
   * This is implemented using an RCU
   * scheme where two shared pointers are created to 
   * two instances of the data. The two shared pointers
   * are called the "head" and the "buffer".  All reads are performed
   * using the head, and all writes are made to the buffer. Since
   * readers can hold references to the data (using a shared pointer),
   * writes are only performed when all readers release their
   * references to the buffers.  When writes complete, the head and
   * the buffer pointers are exchanged atomically.
   *
   * The two-pointer scheme allows a limited amount of simultaneous
   * read/write. Specifically, while readers are accessing data, a
   * single write could be completed successfully. After which, all
   * readers must release their references to the data before a second
   * write can complete. Readers should therefore release their
   * references to the shared data as soon as possible to avoid
   * dead-locks.
   */
  template <typename T>
  class glshared : public glshared_base{

  public:
    //! Type of the apply function inhereted from the gl_shared base
    typedef glshared_base::apply_function_type apply_function_type;
    //! Type of the boost shared pointer to a constant 
    typedef boost::shared_ptr<const T> const_ptr_type;
    //! Type of the boost shared pointer
    typedef boost::shared_ptr<T> ptr_type;


  private:
    // two instances of the data
    T buffer_and_head[2];

    // shared pointers to the data
    boost::shared_ptr<T> buffer_and_head_ptr[2];
  
    // a pointer to the shared pointer which contains the write target
    boost::shared_ptr<T>* buffer;
    // a pointer to the shared pointer which contains the read target
    boost::shared_ptr<T>* head;
  
    // A lock used to sequentialize multiple writes
    mutex set_lock;

    // Waits until all references to the buffer are released
    inline void wait_for_buffer_release() {
      while(!(buffer->unique())) sched_yield();
    }
  
    // Performs an atomic exchange of the head and buffer pointers
    inline void exchange_buffer_and_head() {
      atomic_exchange(buffer, head);
    }
  
  public:
    //! Construct initial shared pointers
    glshared() {
      buffer_and_head_ptr[0].reset(&(buffer_and_head[0]), 
                                   glshared_impl::empty_deleter<T>());
      buffer_and_head_ptr[1].reset(&(buffer_and_head[1]), 
                                   glshared_impl::empty_deleter<T>());
      buffer = &(buffer_and_head_ptr[0]);
      head = &(buffer_and_head_ptr[1]);
    }
  

    /// Returns a copy of the data
    inline T get_val() const{
      return *(*(head));
    }

    /**
     * Gets the value of the shared variable wrapped in an any.
     */
    any get_any() const {
      return *(*(head));
    }
  
    /**
     * Returns true if there are no other active references to this
     * variable. This should not be used to test for exclusive access,
     * and is meant for internal use.
     */
    bool is_unique() const {
      return buffer->unique() && head->unique();
    }
  
    /**
     * Returns a shared_ptr to the data.  When the shared pointer goes
     * out of scope, its contained pointer becomes invalid. The user
     * should not request for the underlying pointer inside the
     * shared_ptr.
     *
     *  gl_shared<int> shared_x;
     * 
     *  Ok: 
     *    boost::shared_ptr<const int> var_p = shared_x.get_ptr();
     *    const int& x = *var_p;
     *
     *  Bad:
     *    const int& x = *shared_x.get_ptr();
     *
     *
     */
    inline const_ptr_type get_ptr() const{
      return boost::const_pointer_cast<const T, T>(*head);
    }

    /**
     * changes the data to 't'. This operation is atomic This
     * operation could stall forever if there are active shared
     * pointers to this variable which are never released.
     */
    void set(const T& t) {
      set_lock.lock();
      wait_for_buffer_release();
      *(*buffer) = t;
      exchange_buffer_and_head();
      set_lock.unlock();
    }
  
  
    /**
     * Sets the value of the shared variable using an any. The type of
     * the any must match the type of the shared object.
     */
    void set_any(const any &t) {
      set(t.as<T>());
    }
  
    /** 
     * Exchanges the data with 't'. This operation performs the
     * exchange atomically and could therefore stall
     * forever if there are active shared
     * pointers to this variable which are never released.
     */
    void exchange(T& t) {
      set_lock.lock();
      wait_for_buffer_release();
      T retval = get_val();
      *(*buffer) = t;
      t = retval;
      exchange_buffer_and_head();
      set_lock.unlock();
    }


    /**
     * apply's a function to this variable passing an additional
     * parameter. This operation tries to perform the modification atomically
     * and could stall forever if there are
     * active shared pointers to this variable which are never
     * released.
     */
    void apply(apply_function_type fun,
               const any& srcd) {
      set_lock.lock();
      wait_for_buffer_release();
      any temp = *(*head);
      fun(temp, srcd);
      *(*buffer) = temp.as<T>();
      exchange_buffer_and_head();
      set_lock.unlock();
    }
  };


  /**
   * Because it is inconvenient that the apply function specification
   * takes the current value as an "any" as opposed to using the true
   * type of the current value (T), this function adapts an apply
   * function written in the more intuitive form:
   *
   *  void applyfn(T&, const any&)
   *
   * to the regular apply function type.  apply_adapter<T, applyfn> is
   * a function which matches the regular apply function type and
   * calls applyfn.
   */
  template<typename T, void (*applyfn)(T&, const any&) >  
  void apply_adapter(any &d, const any& param) {
    applyfn(d.as<T>(), param);
  }

} 
#endif

