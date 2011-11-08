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


#ifndef PODIFY_HPP
#define PODIFY_HPP
#include <graphlab/serialization/iarchive.hpp>
#include <graphlab/serialization/oarchive.hpp>
namespace graphlab {

 /**
This is a macro which allows you to temporarily define
an object as a POD. 
for instance if you have a struct that has no save/load
function defined, but you want to treat it as a POD.
Now you can write:

SOmePODStruct s;
oarc << PODIFY(s);
and
iarc >> PODIFY(s);
*/

#define PODIFY(X) (__podify__<typeof(X)>(X))

/**
This wraps and adds temporary support for serialization
of a POD object
*/
template <typename T>
class __podify__{
 public:
   
  T alt;
  const T* ref;
  // if created without constructor, assign 
  // the pointer to the built-in version
  __podify__():ref(&alt) { }
  
  __podify__(const T &a):ref(&a) { }
  
  operator const T&() const{
    return *ref;
  }
  
  operator T&() {
    return *(const_cast<T*>(ref));
  }
  
  void save(oarchive &a) const{
    serialize(a, ref, sizeof(T));
  }
  // unfortunately load will not work 
  // because we this class is only created as a temporary
  // we are unable to get a reference to it
  // so we make the pass by value version below
};


template <typename T>
iarchive& operator>>(iarchive& a, __podify__<T> i) {
    T* refptr = (const_cast<T*>(i.ref));
    deserialize(a, refptr, sizeof(T));
    return a;
}

}
#endif

