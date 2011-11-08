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
 * Also contains code that is Copyright 2011 Yahoo! Inc.  All rights
 * reserved.  
 *
 * Contributed under the iCLA for:
 *    Joseph Gonzalez (jegonzal@yahoo-inc.com) 
 *
 */


#ifndef GRAPHLAB_DISK_ATOM_HPP
#define GRAPHLAB_DISK_ATOM_HPP


#include <sstream>
#include <map>
#include <boost/unordered_map.hpp>
#include <graphlab/serialization/serialization_includes.hpp>
#include <graphlab/graph/graph.hpp>
#include <graphlab/graph/graph_atom.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <boost/iostreams/stream.hpp>
#include <graphlab/logger/logger.hpp>
#include <kchashdb.h>

namespace graphlab {
  
  /**
   * Interface for reading and writing to an atom on disk
   * 
   * The atom serves 2 purposes.
   * First it provides a partition of the graph, including its ghost vertices.
   * Next, it provides an auxiliary hashtable distributed across all atom files, 
   *        identifying the owner of a vertex.
   * 
   * The atom file is a Kyoto Cabinet data store and it contains the following keys:
   * 
   * "_vidlist" ==> uint64_t : the vertex after vertex 'vid' in a linked list of vertices \n
   * "numv" ==> uint64_t : the number of vertices in the atom \n
   * "nume" ==> uint64_t : the number of edges in the atom \n
   * "numlocalv" ==> uint64_t : the number of local vertices in the atom. \n
   * "numlocale" ==> uint64_t : the number of local edges in the atom. \n
   * v[vid] ==> archive of owner, vdata : The vertex data of vertex 'vid' \n
   * e[srcv][destv] ==> archive of edata : The edge on the edge srcv --> destv \n
   * i[vid] ==> uint64_t* : An array of in-vertices of vertex 'vid' \n
   * o[vid] ==> uint64_t* : An array of out-vertices of vertex 'vid' \n
   * c[vid] ==> uint32_t : Color of vertex 'vid' \n
   * 
   * Next the DHT entries are: \n
   * h[vid] ==> uint16_t : The atom file owning vertex vid.  \n
   * These are entirely independent of the previous keys.
   */
  class disk_atom: public graph_atom {
  public:
    typedef kyotocabinet::TreeDB storage_type;
  private:

    //! Todo: Fix ugly hack
    typedef graph<bool,bool>::vertex_id_type    vertex_id_type;
    typedef graph<bool,bool>::vertex_color_type vertex_color_type;

    storage_type db;
    // with only one global invalidate flag
    //kyotocabinet::HashDB db;
  
    atomic<uint64_t> numv;
    atomic<uint64_t> nume;
    atomic<uint64_t> numlocalv;
    atomic<uint64_t> numlocale;
    uint16_t atomid;
  
    std::string filename;
    
    inline std::string id_to_str(uint64_t i) {
      char c[10]; 
      unsigned char len = compress_int(i, c);
      return std::string(c + 10 - len, (std::streamsize)len); 
    }

    void open_db();
    
    vertex_id_type vertex_key_to_id(std::string s);
  public:
   
    /// constructor. Accesses an atom stored at the filename provided
    disk_atom(std::string filename, uint16_t atomid);

  
    ~disk_atom();
      
    /// Gets the atom ID of this atom
    inline uint16_t atom_id() const {
      return atomid;
    }
  
    inline std::string get_filename() const {
      return filename;
    }
  
    /**
     * \brief Inserts vertex 'vid' into the file without data.
     * If the vertex already exists, it will be overwritten.
     */
    void add_vertex(vertex_id_type vid, uint16_t owner);
  
  
    /**
     * \brief Inserts vertex 'vid' into the file without data.
     * If the vertex already exists, nothing will be done.
     * Returns true if vertex was added.
     */
    bool add_vertex_skip(vertex_id_type vid, uint16_t owner);
  
  
    /**
     * \brief Inserts vertex 'vid' into the file. If the vertex already exists,
     * it will be overwritten.
     */
    void add_vertex_with_data(vertex_id_type vid, uint16_t owner, const std::string &vdata);
    
  
    /**
     * \brief Inserts edge src->target into the file. If the edge already exists,
     * it will be overwritten.
     */
    void add_edge_with_data(vertex_id_type src, vertex_id_type target, const std::string& edata);
  
    /**
     * \brief Inserts edge src->target into the file. If the edge already exists,
     * it will be overwritten.
     */
    void add_edge_with_data(vertex_id_type src, uint16_t srcowner,
                            vertex_id_type target, uint16_t targetowner, const std::string& edata);
  
    /**
     * \brief Modifies an existing vertex in the file where no data is assigned to the 
     * vertex. User must ensure that the file
     * already contains this vertex. If user is unsure, add_vertex should be used.
     */
    void set_vertex(vertex_id_type vid, uint16_t owner);
  


    /**
     * \brief Modifies an existing vertex in the file. User must ensure that the file
     * already contains this vertex. If user is unsure, add_vertex should be used.
     */
    void set_vertex_with_data(vertex_id_type vid, uint16_t owner, const std::string &vdata);
  
    
    /**
     * \brief Modifies an existing edge in the file. User must ensure that the file
     * already contains this edge. If user is unsure, add_edge should be used.
     */
    void set_edge_with_data(vertex_id_type src, vertex_id_type target, const std::string &edata);
  
  
    /**
     * \brief Reads a vertex from the file returning only the 'owner' of the vertex
     * and not the data.
     * Returns true if vertex exists and false otherwise.
     */
    bool get_vertex(vertex_id_type vid, uint16_t &owner);
  

    /**
     * \brief Reads a vertex from the file returning results in 'owner' and 'vdata'.
     * Returns true if vertex exists and false otherwise.
     * If there is no vertex data stored, vdata will not be modified.
     */
    bool get_vertex_data(vertex_id_type vid, uint16_t &owner, std::string &vdata);
  
    /**
     * \brief Reads a edge from the file returning results in 'owner' and 'vdata'.
     * Returns true if edge exists and false otherwise.
     * If there is no edge data stored, edata will not be modified.
     */
    bool get_edge_data(vertex_id_type src, vertex_id_type target, std::string &edata);


    /**
     * \brief Returns a list of all the vertices in the file
     */
    std::vector<vertex_id_type> enumerate_vertices();
  
    /**
     * \brief Returns a list of all the adjacent atoms in the file
     * and the number of ghost vertices in this atom belonging to the
     * adjacent atom
     */
    std::map<uint16_t, uint32_t> enumerate_adjacent_atoms();
  
    /**
     * \brief Returns the set of incoming vertices of vertex 'vid'
     */
    std::vector<vertex_id_type> get_in_vertices(vertex_id_type vid);
   
   
    /**
     * \brief Returns the set of outgoing vertices of vertex 'vid'
     */
    std::vector<vertex_id_type> get_out_vertices(vertex_id_type vid);


    /**
     * \brief Get the color of the vertex 'vid'.
     * Returns vertex_color_type(-1) if the entry does not exist
     */
    vertex_color_type get_color(vertex_id_type vid);

    /**
     * \brief Sets the color of vertex 'vid'
     */
    void set_color(vertex_id_type vid, vertex_color_type color);
  
    /// Returns the largest color number
    vertex_color_type max_color();
  
    /**
     * \brief Reads from the auxiliary hash table mapping vid ==> owner.
     * Returns (uint16_t)(-1) if the entry does not exist
     */
    uint16_t get_owner(vertex_id_type vid);

    /**
     * \brief Writes to the auxiliary hash table mapping vid ==> owner.
     */
    void set_owner(vertex_id_type vid, uint16_t owner);

    /// \brief empties the atom file
    void clear();
  
    /// \brief Ensures the disk storage is up to date
    void synchronize();
    
    /** \brief Return the total number of vertices stored in this atom, 
     * whether or not the this atom actually owns the vertex.
     */
    inline uint64_t num_vertices() const {
      return numv.value;
    }
  
    /** \brief  Return the total number of edges stored in this atom, whether or 
     * not the this atom actually owns the edge.
     */
    inline uint64_t num_edges() const {
      return nume.value;
    }
  
  
    /// Returns a reference to the underlying Kyoto Cabinet
    inline storage_type& get_db() {
      return db;
    }
    
    void build_memory_atom(std::string fname);

  };

}

#endif

