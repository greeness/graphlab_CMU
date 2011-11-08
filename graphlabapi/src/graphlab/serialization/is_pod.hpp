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


#ifndef GRAPHLAB_IS_POD_HPP
#define GRAPHLAB_IS_POD_HPP
#include <boost/type_traits.hpp>

namespace graphlab {

  template <typename T>
  struct gl_is_pod{
    // it is a pod and is not an integer since we have special handlings for integers

    BOOST_STATIC_CONSTANT(bool, value =
                          (
                           boost::type_traits::ice_and<
                             boost::is_scalar<T>::value,
                             boost::type_traits::ice_not<
                               boost::type_traits::ice_and<
                                 boost::is_integral<T>::value,
                                 sizeof(T) >= 2
                                 >::value
                               >::value
                             >::value
                          ));

    // standard POD detection is no good because things which contain pointers
    // are POD, but are not serializable
    // (T is POD and  T is not an integer of size >= 2)
    /*BOOST_STATIC_CONSTANT(bool, value =
                          (
                           boost::type_traits::ice_and<
                             boost::is_pod<T>::value,
                             boost::type_traits::ice_not<
                               boost::type_traits::ice_and<
                                 boost::is_integral<T>::value,
                                 sizeof(T) >= 2
                                 >::value
                               >::value
                             >::value
                          ));*/

  };

}

#endif



