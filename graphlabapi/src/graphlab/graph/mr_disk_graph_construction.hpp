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


#ifndef GRAPHLAB_MR_DISK_GRAPH_CONSTRUCTION_HPP
#define GRAPHLAB_MR_DISK_GRAPH_CONSTRUCTION_HPP
#include <boost/bind.hpp>
#include <graphlab/graph/graph.hpp>
#include <graphlab/util/stl_util.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/graph/disk_graph.hpp>
#include <graphlab/rpc/dc.hpp>
#include <graphlab/graph/mr_disk_graph_construction_impl.hpp>

namespace graphlab {

/**
  * Standard interface for full parallel/distributed construction of a 
  * disk_graph. User first subclasses the igraph_constructor, implementing
  * the begin() and vertex_to_atomid() methods; as well as either the iterate()
  * function or the generate_callback() function.
  * See \ref mr_disk_graph_construction for details.
  * 
  * The subclass must be copy-constructable.
  * 
  */
template <typename VertexData, typename EdgeData>
class igraph_constructor {
 public:

  typedef disk_graph<VertexData, EdgeData> disk_graph_type;
  typedef typename disk_graph_type::vertex_id_type vertex_id_type;
  typedef typename disk_graph_type::vertex_color_type vertex_color_type;

  
  
  enum iterate_return_type {
    Vertex,
    Edge,
    NoMoreData = -1,
  };
  
  enum iteration_method {
    ExternalIteration,
    CallBack,
  };

  /// constructor
  igraph_constructor():currentdg(NULL) { };
  
  /// destructor
  virtual ~igraph_constructor() { };
  
  /**
   * 'max' (possibly distributed instances) of the user subclass is created 
   * using the copy constructor. (max is defined by the arguments 
   * into \ref mr_disk_graph_construction )
   * begin() is then called on each instance with a unique 'i' from 0 to 
   * max - 1.
   *
   * If begin() returns ExternalIteration, the iterate() function is used.
   * to generate the graph data. \see iterate()
   *
   * If begin() returns CallBack, the generate_callback() function is used
   * to generate the graph data. \see generate_callback()
   */
  virtual iteration_method begin(size_t i, size_t max) = 0;
  
  /**
   * if begin() returns ExternalIteration, this function is used to generate
   * the graph data. The iterate() function is called repeatedly, and
   * each call to iterate() must return either a vertex or an edge through
   * its arguments which are passed by reference.
   *  \param irt 
   *  \param vtx If return_value == Vertex, this is the vertex ID returned. This parameter
   *             is ignored if return_value == Edge.
   *  \param vdata If return_value == Vertex, the vertexdata to be created on vertex 'vtx'
   *              is returned here.
   *  \param color If return_value == Vertex, this should contain the vertex color  of 'vtx'
   *  \param edge If return_value == Edge, this is the edge to be inserted. This parameter
   *             is ignored if return_value == Vertex.
   *  \param edata if return_value == Edge, the edgedata to be created on the edge 
   *              'edge' is returned here.
   * \return if return value is Vertex or Edge, the actual data will be read
   *         from the arguments. If return value is NoMoreData, no data is read,
   *         and the construction loop is terminated.
   */
  virtual iterate_return_type iterate(vertex_id_type& vtx, 
                                      VertexData& vdata,
                                      vertex_color_type & color,
                                      std::pair<vertex_id_type, vertex_id_type>& edge, 
                                      EdgeData & edata)  { return NoMoreData; }


  /**
   * if begin() returns CallBack, this function is used to generate
   * the graph data. The generate_callback() function is called once
   * and the user must use the add_vertex() and add_edge() functions to
   * insert edges and vertices into the graph.
  */
  virtual void generate_callback()  { };
                       
  /**
   * This function provides the mapping from vertex to atom ID.
   * The behavior of this function must be consistent across all (distributed)
   * instances.
   */
  virtual uint16_t vertex_to_atomid(vertex_id_type vtx, uint16_t numatoms) = 0;

  /**
    Adds a vertex. Used by the generate_callback() function
  */
  void add_vertex(vertex_id_type vtx, const VertexData& vdata, vertex_color_type color) {
    uint16_t location = vertex_to_atomid(vtx, numatoms);
    ASSERT_LT(location, numatoms);
    currentdg->add_vertex_unsafe(vtx, vdata, location);
    currentdg->set_color_unsafe(vtx, color, location);
  }
  
  /**
    Adds an edge. Used by the generate_callback() function
  */
  void add_edge(std::pair<vertex_id_type, vertex_id_type> edge, 
                const EdgeData & edata) {

    uint16_t locationsrc = vertex_to_atomid(edge.first, numatoms);
    uint16_t locationdest = vertex_to_atomid(edge.second, numatoms);
    ASSERT_LT(locationsrc, numatoms);
    ASSERT_LT(locationdest, numatoms);
    currentdg->add_edge_explicit(edge.first, locationsrc,
                                 edge.second, locationdest,
                                 edata); 
  }
  /**
   * Used by mr_disk_graph_construction. Creates a section of the disk graph
   * using the information generated by this class.
   */
  void mr_disk_graph_construction_map(disk_graph<VertexData, EdgeData> &dg,
                                      size_t i,
                                      size_t max) {
    iteration_method method = begin(i, max);
    numatoms = (uint16_t) dg.num_atoms();
    currentdg = &dg;
    if (method == ExternalIteration) {
      iterate_return_type irt;
      vertex_id_type vtx;
      std::pair<vertex_id_type, vertex_id_type> edge;
      VertexData vdata;
      EdgeData edata;
      // loop over iterate until it returns an NoMoreData
      vertex_color_type color = 0;
      while((irt = iterate(vtx, vdata, color, edge, edata)) != NoMoreData){   
        if (irt == Vertex) {
          uint16_t location = vertex_to_atomid(vtx, numatoms);
          ASSERT_LT(location, numatoms);
          dg.add_vertex_unsafe(vtx, vdata, location);
          dg.set_color(vtx, color);
        }
        else if (irt == Edge) {
          uint16_t locationsrc = vertex_to_atomid(edge.first, numatoms);
          uint16_t locationdest = vertex_to_atomid(edge.second, numatoms);
          ASSERT_LT(locationsrc, numatoms);
          ASSERT_LT(locationdest, numatoms);
          dg.add_edge_explicit(edge.first, locationsrc,
                               edge.second, locationdest,
                               edata);
        }
        else {
          ASSERT_MSG(false, "Graph Constructor returned invalid value");
        }
        color = 0;
      }
    }
    else {
      generate_callback();
    }
  }
  
 private:
  disk_graph_type* currentdg;
  uint16_t numatoms;
};

/**
  * \tparam GraphConstructor A subclass of igraph_constructor.
  * 
  * Each process must construct a single instance of the graph constructor.
  * This constructor is then replicated 'max_per_node' times on each machine
  * using the GraphConstructor's copy constructor. In total, 
  * max = max_per_node * dc.numprocs() instances are constructed. 
  * begin() on each instance is called using this value.
  * 
  * \note If run in the distributed setting, all processes must have access to
  * a common distributed file system (such as NFS) 
  * 
  * This function must be called with the same arguments across all the machines.
  * 
  * The constraint is that every edge and every vertex must be added at most 
  * once across all (distributed) GraphConstructors. Arbitrary joining 
  * may result if the same edge/vertex is added more than once.
  * 
  * \param dc The distributed control object
  * \param gc An instance of the GraphConstructor. Must be copy constructable
  * \param max_per_node Number of times 'gc' will be replicated on each machine
  * \param outputbasename The output atom files will be stored as outputbasename.0
  *                       outputbasename.1, etc. With atom index in outputbasename.idx
  *                       In addition, a series of temporary files named 
  *                       [outputbasename]_t... will be created. They will be
  *                       erased at the end of the execution of mr_disk_graph_construction.
  * \param numatoms The number of atoms to create.
  */
template <typename GraphConstructor, typename VertexData, typename EdgeData>
void mr_disk_graph_construction(distributed_control &dc,
                                GraphConstructor &gc, 
                                size_t max_per_node,
                                std::string outputbasename,
                                size_t numatoms,
                                disk_graph_atom_type::atom_type atomtype,
                                std::string localworkingdir = "./",
                                std::string remoteworkingdir = "./") {
  // make sure directory names end with "/"
  if (localworkingdir.length() > 0) {
    if (*(localworkingdir.rbegin()) != '/') localworkingdir += "/";
  }
  // make sure directory names end with "/"
  if (remoteworkingdir.length() > 0) {
    if (*(remoteworkingdir.rbegin()) != '/') remoteworkingdir += "/";
  }
  
  
  // lets get all the machines here first.
  dc.full_barrier();
  if (dc.procid() == 0) {
    logstream(LOG_INFO) << "Mapping over Graph Constructors..." << std::endl;
  }
  
  std::string atombase = outputbasename + "_" + tostr(dc.procid());    
  std::string localatombase = localworkingdir + atombase;
  std::string remoteatombase = remoteworkingdir + atombase;
 
  {
    // create the local disk graph
    // each machine saves to the same disk graph
    
    disk_graph<VertexData, EdgeData> dg(localatombase, numatoms, disk_graph_atom_type::WRITE_ONLY_ATOM);
    dg.clear();
    thread_group thrgrp;
    std::vector<GraphConstructor*> gcs(max_per_node);
    for (size_t i = 0;i < max_per_node; ++i) {
      gcs[i] = new GraphConstructor(gc);
      size_t gcid = dc.procid() * max_per_node + i;
      thrgrp.launch(
          boost::bind(
            &igraph_constructor<VertexData, EdgeData>::mr_disk_graph_construction_map,
            gcs[i],
            boost::ref(dg),
            gcid,
            max_per_node * dc.numprocs()));
    }
    thrgrp.join();
    for (size_t i = 0;i < max_per_node; ++i) {
      delete gcs[i];
    }
    dg.finalize();
  }
  if (localworkingdir != remoteworkingdir) {
    logstream(LOG_INFO) << dc.procid() << ": " << "Uploading stripe..." << std::endl;
    std::string command = std::string("mv ") + localatombase + ".* " + remoteworkingdir;
    logstream(LOG_INFO) << dc.procid() << ": " << "SHELL: " << command << std::endl;
    const int error = system(command.c_str());
  } 

  dc.barrier();
  if (dc.procid() == 0) {
    logstream(LOG_INFO) << dc.procid() << ": " << "Joining Atoms..." << std::endl;
  }
  std::map<size_t, mr_disk_graph_construction_impl::atom_properties> atomprops;
  // split the atoms among the machines
  if (1) {
  for (size_t i = dc.procid(); i < numatoms; i += dc.numprocs()) {
    // build a vector of all the parallel atoms
    std::vector<std::string> atomfiles, localatomfiles, remoteatomfiles;
    for (size_t j = 0;j < dc.numprocs(); ++j) {
      std::string f = outputbasename + "_" + tostr(j) + "." + tostr(i) + ".dump";
      atomfiles.push_back(f);
      localatomfiles.push_back(localworkingdir + f);
      remoteatomfiles.push_back(remoteworkingdir + f);
    }
    std::string finaloutput = outputbasename + "." + tostr(i);
    std::cout << dc.procid() << ": " << "Joining to " << finaloutput << std::endl;

    
    std::string localfinaloutput = localworkingdir + finaloutput;
    std::string remotefinaloutput = remoteworkingdir + finaloutput;
    // collect the atoms I need to the local working directory
    if (localworkingdir != remoteworkingdir) {
      logstream(LOG_INFO) << dc.procid() << ": " << "Downloading partial atoms " << i << std::endl;
      for (size_t j = 0; j < atomfiles.size(); ++j) {
        std::string command = std::string("mv ") + remoteatomfiles[j] + " " + localatomfiles[j];
//        logstream(LOG_INFO) << dc.procid() << ": " << "SHELL: " << command << std::endl;
       const int error = system(command.c_str());
      }
    }
    atomprops[i] = 
          mr_disk_graph_construction_impl::merge_parallel_disk_atom<VertexData, EdgeData>(localatomfiles, localfinaloutput, i, atomtype);
          
    // move final output back
    if (localworkingdir != remoteworkingdir) {
      logstream(LOG_INFO) << dc.procid() << ": " << "Uploading combined atom " << finaloutput << std::endl;
      std::string command = std::string("mv ") + atomprops[i].filename + " " + remoteworkingdir;
      logstream(LOG_INFO) << dc.procid() << ": " << "SHELL: " << command << std::endl;
      atomprops[i].base_atom_filename = remotefinaloutput;
      const int error = system(command.c_str());
    }
  }
  }
  dc.barrier();
  
  // processor 0 joins and built the atom index
  // other processors just send
  if (dc.procid() > 0) {
    dc.send_to(0, atomprops);
  }
  else {
    // receive all the atom_to_adjacent_atoms
    for (procid_t i = 1;i < dc.numprocs(); ++i) {
      std::map<size_t, mr_disk_graph_construction_impl::atom_properties> temp;
      dc.recv_from(i, temp);
      // merge into atom_to_adjacent_atoms
      std::map<size_t, mr_disk_graph_construction_impl::atom_properties>::iterator iter = temp.begin();
      while(iter != temp.end()) {
        ASSERT_TRUE(atomprops.find(iter->first) == atomprops.end());
        atomprops[iter->first] = iter->second;
        ++iter;
      }  
    }
    // done! Now build the atom index
    ASSERT_EQ(atomprops.size(), numatoms);
    atom_index_file idxfile = mr_disk_graph_construction_impl::atom_index_from_properties(atomprops);
    idxfile.write_to_file(remoteworkingdir + outputbasename+".idx");
  }
  dc.barrier();
  
};


}
#endif

