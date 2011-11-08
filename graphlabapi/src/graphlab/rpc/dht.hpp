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


#ifndef DHT_HPP
#define DHT_HPP
#include <boost/functional/hash.hpp>
#include <boost/unordered_map.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/rpc/dc_dist_object.hpp>
namespace graphlab {

/**
 * \ingroup rpc
 * Implements a very rudimentary distributed key value store.
  */
template <typename KeyType, typename ValueType>
class dht { 
 public:
  typedef boost::unordered_map<size_t, ValueType> storage_type;
  
  dht(distributed_control &dc): rpc(dc, this) { }
  
  /**
   * gets the value associated with a key.
   * Returns (true, Value) if the entry is available.
   * Returns (false, undefined) otherwise.
   */
  std::pair<bool, ValueType> get(const KeyType &key) const {
    // who owns the data?
    size_t hashvalue = hasher(key);
    size_t owningmachine = hashvalue % rpc.dc().numprocs();

    std::pair<bool, ValueType> retval;
    // if it is me, we can return it
    if (owningmachine == rpc.dc().procid()) {
      lock.lock();
      typename storage_type::const_iterator iter = storage.find(hashvalue);
      retval.first = iter != storage.end();
      if (retval.first) retval.second = iter->second;
      lock.unlock();
    }
    else {
      retval = rpc.fast_remote_request(owningmachine, 
                                       &dht<KeyType,ValueType>::get, 
                                       key);
    }
    return retval;
  }
  
  /**
   * Sets the newval to be the value associated with the key
   */
  void set(const KeyType &key, const ValueType &newval) {  
    // who owns the data?
    size_t hashvalue = hasher(key);
    size_t owningmachine = hashvalue % rpc.dc().numprocs();
    // if it is me, set it
    if (owningmachine == rpc.dc().procid()) {
      lock.lock();
      storage[hashvalue] = newval;
      lock.unlock();
    }
    else {
      rpc.fast_remote_call(owningmachine, 
                           &dht<KeyType,ValueType>::set, 
                           key, newval);
    }
  }
  
  void print_stats() const {
    std::cerr << rpc.calls_sent() << " calls sent\n";
    std::cerr << rpc.calls_received() << " calls received\n";
  }
  
  /**
  Must be called by all machines simultaneously
  */
  void clear() {
    rpc.barrier();
    storage.clear();
  }
 private:
  mutable dc_dist_object<dht<KeyType, ValueType> > rpc;
  
  boost::hash<KeyType> hasher;
  mutex lock;
  storage_type storage;


};

};
#endif

