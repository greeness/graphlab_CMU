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


#ifndef UPDATE_FUNCTION_GENERATOR_HPP
#define UPDATE_FUNCTION_GENERATOR_HPP
#include <boost/unordered_map.hpp>
#include <boost/preprocessor.hpp>
#include "gl_emx_graphtypes.hpp"
#include "update_function_array.hpp"
#include "graphlab/macros_def.hpp"
/*********************************************************************
 * The update_function_array.hpp defines an array UPDATE_FUNCTIONS 
 * which lists all the matlab update functions available.
 * However, I need to generate my own graphlab update functions 
 * which call the matlab update functions. So, for each matlab update 
 * function, I generate a graphlab update function with the same name but
 * prefixed with __gl__
 *********************************************************************/
#define GET_NUM_UPDATE_FUNCTIONS BOOST_PP_ARRAY_SIZE(__UPDATE_FUNCTIONS__)
#define GET_GL_UPDATE_FUNCTION_N(N) BOOST_PP_CAT( __gl__ , BOOST_PP_ARRAY_ELEM(N, __UPDATE_FUNCTIONS__))


// Some useful macros to extract elements from the UPDATE_FUNCTIONS array 


/*********************************************************************
 * Generate all the update functions
 * To simplify the amount of code I need to generate using boost PP,
 * I simply use the boost preprocessor create a function which calls
 * (and instantiates) a generic templateized update function caller.
 *********************************************************************/
/**
 * The handle passed to the matlab update function is a pointer to this struct.
 */
struct gl_update_function_params{
  gl_types::iscope* scope;
  gl_types::icallback* scheduler;
};

template <gl_emx_updatefn_type emx_update_fn>
void exec_update_function(gl_types::iscope& scope, 
                                 gl_types::icallback& scheduler) {
  // create the graph structure arrays I need to pass to the update function
  // allocate them all on the stack
  int32_t inesize = scope.in_edge_ids().size();
  uint32_t inedges[inesize];
  uint32_t inv[inesize];
  
  int32_t outesize = scope.out_edge_ids().size();
  uint32_t outedges[outesize];
  uint32_t outv[outesize];
  // read the inedges and out edges and fill the graph structure
  size_t ctr = 0;
  foreach (graphlab::edge_id_t eid, scope.in_edge_ids()) {
    inedges[ctr] = eid + 1;           // add one to everything to match matlab
    inv[ctr] = scope.source(eid) + 1; // add one to everything to match matlab
    ++ctr;
  }
  
  ctr = 0;
  foreach (graphlab::edge_id_t eid, scope.out_edge_ids()) {
    outedges[ctr] = eid + 1;           // add one to everything to match matlab
    outv[ctr] = scope.target(eid) + 1; // add one to everything to match matlab
    ++ctr;
  }
  
  // construct the emxArray types
  emxArray_uint32_T eml_inedges, eml_inv, eml_outedges, eml_outv;
  eml_inedges.data = inedges;
  eml_inedges.size = &inesize;
  eml_inedges.allocatedSize = inesize;
  eml_inedges.numDimensions = 1;
  eml_inedges.canFreeData = false;

  eml_inv.data = inv;
  eml_inv.size = &inesize;
  eml_inv.allocatedSize = inesize;
  eml_inv.numDimensions = 1;
  eml_inv.canFreeData = false;  

  
  eml_outedges.data = outedges;
  eml_outedges.size = &outesize;
  eml_outedges.allocatedSize = outesize;
  eml_outedges.numDimensions = 1;
  eml_outedges.canFreeData = false;

  eml_outv.data = outv;
  eml_outv.size = &outesize;
  eml_outv.allocatedSize = outesize;
  eml_outv.numDimensions = 1;
  eml_outv.canFreeData = false;  
  
  // build the handle. The handle is basically a pointer to a 
  // gl_update_function_params
  gl_update_function_params params;
  params.scope = &scope;
  params.scheduler = &scheduler;
  // store the pointer in an integer
  size_t paramsptr = (size_t)(&params);  
  HANDLE_TYPE handle;
  // force cast the contents of the integer to a double
  handle = *reinterpret_cast<HANDLE_TYPE*>(&paramsptr);
  emx_update_fn(scope.vertex() + 1, &eml_inedges, &eml_inv, &eml_outedges, &eml_outv, handle);

  // nothing to free since everything is on the stack
}

#define GEN_UPDATE_FUNCTION_DECL(Z,N,_) void GET_GL_UPDATE_FUNCTION_N(N)     \
                                  (gl_types::iscope& scope,               \
                                   gl_types::icallback& scheduler);
  
BOOST_PP_REPEAT(GET_NUM_UPDATE_FUNCTIONS, GEN_UPDATE_FUNCTION_DECL, _)
  
/*********************************************************************
 * Registers all the graphlab update functions created in a map
 * so I get a string->update_function mapping.
 *********************************************************************/
typedef boost::unordered_map<std::string, 
                graphlab::update_task<emx_graph>::update_function_type> update_function_map_type;

extern update_function_map_type update_function_map;
void register_all_matlab_update_functions();


#include <graphlab/macros_undef.hpp>

//#undef UPDATE_FUNCTIONS
#undef GET_NUM_UPDATE_FUNCTIONS
#undef GET_GL_UPDATE_FUNCTION_N
#undef GEN_UPDATE_FUNCTION_DECL

#endif

