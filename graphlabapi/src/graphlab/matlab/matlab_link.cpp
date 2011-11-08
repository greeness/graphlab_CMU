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


#include "gl_emx_graphtypes.hpp"
#include "matlab_link.h"
#include "update_function_generator.hpp"
#include "mx_emx_converters.hpp"

gl_update_function_params* get_params(HANDLE_TYPE handle) {
  // depun the pointer
  // first force cast it back to a uint64_t
  HANDLE_TYPE *handleptr = &handle;
  size_t paramsptr = *reinterpret_cast<size_t*>(handleptr);
  gl_update_function_params* params;
#ifdef __LP64__
  params = *reinterpret_cast<gl_update_function_params**>(&paramsptr);
  return params;
#else
  uint32_t truncated_paramsptr = paramsptr;
  params = *reinterpret_cast<gl_update_function_params**>(&truncated_paramsptr);
  return params;
#endif
}

void emx_get_edge_data(HANDLE_TYPE handle, uint32_T eid, gl_emx_edgetype *edge) {
  gl_update_function_params *paramsptr = get_params(handle);
  eid--;
  // get the data and copy it out
  const gl_emx_edgetype &e = paramsptr->scope->const_edge_data(eid);
  emxcopy(*edge, e);
}

void emx_get_vertex_data(HANDLE_TYPE handle, uint32_T vid, gl_emx_vertextype *vertex) {
  gl_update_function_params *paramsptr = get_params(handle);
  vid--;
  // get the data and copy it out
  // if vid is current vertex, we use vertex_data. Otherwise we use neighbor_vertex_data
  if (vid == paramsptr->scope->vertex()) {
    const gl_emx_vertextype &v = paramsptr->scope->vertex_data();
    emxcopy(*vertex, v);
  }
  else {
    const gl_emx_vertextype &v = paramsptr->scope->const_neighbor_vertex_data(vid);
    emxcopy(*vertex, v);
  }
}

void emx_set_edge_data(HANDLE_TYPE handle, uint32_T eid, gl_emx_edgetype *edge) {
  gl_update_function_params *paramsptr = get_params(handle);
  eid--;
  // write the data
  gl_emx_edgetype &e = paramsptr->scope->edge_data(eid);
  emxcopy(e, *edge);
}

void emx_set_vertex_data(HANDLE_TYPE handle, uint32_T vid, gl_emx_vertextype *vertex) {
  gl_update_function_params *paramsptr = get_params(handle);
  vid--;
  // write the data
  // if vid is current vertex, we use vertex_data. Otherwise we use neighbor_vertex_data
  if (vid == paramsptr->scope->vertex()) {
    gl_emx_vertextype &v = paramsptr->scope->vertex_data();
    emxcopy(v, *vertex);
  }
  else {
    gl_emx_vertextype &v = paramsptr->scope->neighbor_vertex_data(vid);
    emxcopy(v, *vertex);
  }
}


void emx_add_task(HANDLE_TYPE handle, uint32_T vid, const char* fnname, double priority) {
  //std::cout << "add_task: " << vid << " " << fnname << " " << priority << std::endl;
  static bool printed = false;
  gl_update_function_params *paramsptr = get_params(handle);
  vid--;
  if (fnname == NULL) return;
  // figure out the function..
  std::string f(fnname);
  f = "__gl__" + f;
  update_function_map_type::iterator i = update_function_map.find(f);
  if (i != update_function_map.end()) {
    paramsptr->scheduler->add_task(vid, i->second, priority);
  }
  else {
    if (!printed) std::cerr << "Update function " << fnname << " not found." << std::endl;
    printed = true;
  }
}


uint32_t emx_rand_int() {
  return graphlab::random::rand();
}

double emx_rand_double() {
  return graphlab::random::rand01();
}

double emx_rand_gamma(double alpha) {
  return graphlab::random::gamma(alpha);
}


bool emx_rand_bernoulli(double p) {
  return graphlab::random::bernoulli(p);
}


bool emx_rand_bernoulli_fast(double p) {
  return graphlab::random::fast_bernoulli(p);
}


double emx_rand_gaussian(double mean, double var) {
  return graphlab::random::gaussian(mean, var);
}


uint32_t emx_rand_int_uniform(const uint32_t high_inclusive) {
  return graphlab::random::uniform<uint32_t>(1, high_inclusive);
}

uint32_t emx_rand_int_uniform_fast(const uint32_t high_inclusive) {
  return graphlab::random::fast_uniform<uint32_t>(1, high_inclusive);
}

uint32_t emx_rand_multinomial(double* prob, uint32_t plength) {
  std::vector<double> p(prob, prob + plength);
  return graphlab::random::multinomial(p) + 1;
}
