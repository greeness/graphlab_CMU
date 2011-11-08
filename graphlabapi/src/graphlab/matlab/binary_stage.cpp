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


#include <cstdio>
#include <fstream>
#include <iostream>
#include <graphlab.hpp>
#include "rtwtypes.h"
#include "updates_types.h"
#include "updates_initialize.h"
#include "gl_emx_graphtypes.hpp"
#include "mex_options_struct.h"
#include "update_function_generator.hpp"
#include "mx_emx_converters.hpp"

int main(int argc, char** argv) {
  // parse the command line
  graphlab::command_line_options clopts("Binary stage for a mex file");
  std::string graphfile;
  std::string outgraphfile;
  clopts.attach_option("ingraphfile",
                       &graphfile, graphfile,
                       "Mex serialized graph file");
  clopts.attach_option("outgraphfile",
                        &outgraphfile, outgraphfile,
                        "Output graph file");
                        
  if(!clopts.parse(argc, argv)) {
    std::cerr << "Error in parsing input." << std::endl;
    return EXIT_FAILURE;
  }

  // create the core and set the engine options
  gl_types::core core;
  core.set_engine_options(clopts);

  if (graphfile.length() == 0) {
    std::cerr << "Input Graph file parameter not set!" << std::endl;
    return EXIT_FAILURE;
  }
  if (outgraphfile.length() == 0) {
    std::cerr << "Output Graph file parameter not set!" << std::endl;
    return EXIT_FAILURE;
  }
  // read the graphfile
  std::ifstream fin(graphfile.c_str(), std::ios_base::binary);
  if (!fin.good()) {
    std::cerr << "Unable to open graph file for reading!" << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<parsed_initial_schedule> schedule;
  // deserialize the graph and the initial schedule
  std::cout<< "Deserializing Graph data... " << std::endl;
  graphlab::iarchive iarc(fin);
  iarc >> core.graph();
  iarc >> schedule;
  fin.close();

  std::cout<< "Initializing EMX functions... " << std::endl;
  updates_initialize();
  register_all_matlab_update_functions();

  bool printed = false;
  // perfornm add tasks
  std::cout<< "Initializing Schedule... " << std::endl;
  // loop through the schedule data structure
  for (size_t i = 0;i < schedule.size(); ++i) {
    // search for the update function in the update function map
    update_function_map_type::iterator iter =
              update_function_map.find("__gl__" + schedule[i].update_function);

    // if the update function is not found. print a warning but continue
    if (iter == update_function_map.end()) {
      if (!printed) {
        std::cerr << "Update function "
                 << schedule[i].update_function << " not found!" << std::endl;
      }
      printed = true;
    }
    for (size_t j = 0; j < schedule[i].vertices.size(); ++j) {
      uint32_t vertexid = schedule[i].vertices[j] - 1;
      // look for the update function
      double priority = 1;
      if (j < schedule[i].priorities.size()) priority = schedule[i].priorities[j];
      core.add_task(vertexid, iter->second, priority);
    }
  }

  std::cout<< "Running" << std::endl;
  double time = core.start();
  std::cout << "Running time: " << time << " seconds." << std::endl;
  std::cout << "Update Counts: " << core.last_update_count() << std::endl;

  // write the graph back to the temp file
  std::ofstream fout(outgraphfile.c_str(), std::ios_base::binary);
  if (!fout.good()) {
    std::cerr << "Unable to open graph file for writing!" << std::endl;
    return EXIT_FAILURE;
  }
  graphlab::oarchive oarc(fout);
  oarc << core.graph();
  fout.close();
  std::cout << "Done!" << std::endl;
}

