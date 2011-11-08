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
 *  Code written by Danny Bickson, CMU
 *  Any changes to the code must include this original license notice in full.
 */


#ifndef _KMEANS_HPP
#define _KMEANS_HPP

#include "clustering.h"
#include "../gabp/advanced_config.h"
#include "distance.h"
#include <graphlab/macros_def.hpp>

extern advanced_config ac;
extern problem_setup ps;
extern const char * runmodesname[];

void last_iter();
double calc_cost();
void update_kmeans_clusters();
int calc_cluster_centers();



void init_fuzzy_kmeans(){
  graph_type * g= ps.g<graph_type>();
  for (int i=0; i< ps.M; i++){
     vertex_data & vdata = g->vertex_data(i);
     vdata.distances = _randu(ac.K);
     vdata.distances /= sum(vdata.distances);
     if (ac.debug)
       std::cout<<"Initial assignment of " << i << " is: " << vdata.distances << std::endl;
     vdata.distances = pow(vdata.distances, 2);
  }

}
 /***
 * UPDATE FUNCTION
 */
void kmeans_update_function(gl_types::iscope &scope, 
			 gl_types::icallback &scheduler) {
    

  /* GET current vertex data */
  vertex_data& vdata = scope.vertex_data();

  int id = scope.vertex();
  bool toprint = ac.debug /*&& (id == 0 || (id == ps.M-1))*/; 
  
 /* print statistics */
  if (toprint){
    printf("entering data point %u, current cluster %d\n",  id, vdata.current_cluster);  
    print(vdata.datapoint); 
  }

  if (!vdata.reported) //this matrix row have no non-zero entries, and thus ignored
     return;

  double min_dist = 1e100;
  int pos = -1;
  graphlab::timer t; t.start();

  int end_cluster;
  switch(ps.algorithm){
    case K_MEANS: 
    case K_MEANS_FUZZY:
	end_cluster = ps.K; break; //regular k-means, calculate distance to all cluster heads
    case K_MEANS_PLUS_PLUS: end_cluster = 1; break; //calculate distance of all point to current cluster
    default: assert(false); 
  }

  for (int i=0; i< end_cluster; i++){
     flt_dbl_vec & row = ps.clusts.cluster_vec[i].location;
     if (toprint)
        std::cout<<" cluster " << i << " location " << mid(row,0,std::min(row.size(),20)) << " sum sqr " << ps.clusts.cluster_vec[i].sum_sqr << std::endl;
     double dist = calc_distance(vdata.datapoint, row, ps.clusts.cluster_vec[i].sum_sqr, sum_sqr(vdata.datapoint));
     if (toprint)
        std::cout<<" distance: " << dist << std::endl;
     assert(dist >= 0 && !std::isnan(dist));
     if (ps.algorithm == K_MEANS_PLUS_PLUS || ps.algorithm == K_MEANS){
       if (min_dist > dist){
         min_dist = dist;
          pos = i;
       }
     }
     else if (ps.algorithm == K_MEANS_FUZZY){
	vdata.distances[i] = dist;
        assert(!std::isnan(dist));
     }
  }  
  ps.counter[DISTANCE_CALCULATION] += t.current_time();


  if (ps.algorithm == K_MEANS || ps.algorithm == K_MEANS_PLUS_PLUS){
    assert(pos != -1);
    if (pos != vdata.current_cluster){
      vdata.hot=true;
      if (toprint && (ps.algorithm == K_MEANS))
        std::cout <<id<<" assigned to cluster: " << pos << std::endl;
      else if (toprint && ps.algorithm == K_MEANS_PLUS_PLUS)
        std::cout <<id<<" distance to current cluster is : " << min_dist << std::endl;
    }

    vdata.min_distance = min_dist;
    vdata.prev_cluster = vdata.current_cluster; 
    vdata.current_cluster = pos;
  }
  /**
 * See algo description in: http://www.cs.princeton.edu/courses/archive/fall08/cos436/Duda/C/fk_means.htm
 * min_distance = \sum_j a(j,i)
 * distance(j) = u(i,j)
 * */
  else if (ps.algorithm == K_MEANS_FUZZY){
     flt_dbl_vec old_distance = vdata.distances;
     flt_dbl factor = sum(pow(vdata.distances,-2));
     assert(!std::isnan(factor) && factor > 0);
     flt_dbl_vec normalized = pow(vdata.distances,-2) / factor;
     assert(!std::isnan(sum(normalized)));
    
     vdata.distances = pow(normalized, 2);
     if (toprint)
         std::cout<<id<<" distances (uphi) are: " << vdata.distances << std::endl << " normalized (U) " << normalized << std::endl;
     if (id == 4246&& toprint){
         std::cout<<"something wrong";
     }
     if (toprint)
         std::cout<<" contribution to cost function is : " << elem_mult(vdata.distances, pow(old_distance,2))<<std::endl;
     vdata.min_distance = dot(vdata.distances, pow(old_distance,2));
   }
}


/**
 * printout cost after each iteration
 */
void last_iter(){
  printf("Entering last iter with %d\n", ps.iiter);
  if (ps.algorithm == K_MEANS_PLUS_PLUS || ps.algorithm == K_MEANS)
    update_kmeans_clusters();
  else if (ps.algorithm == K_MEANS_FUZZY)
    calc_cluster_centers();
 
  double cost = calc_cost();
  printf("%g) Iter %s %d  Cost=%g Normalized cost=%g\n",
        ps.gt.current_time(), 
	runmodesname[ps.algorithm], 
	ps.iiter,
        cost, 
	cost/ps.M);
  ps.iiter++;

}



void update_kmeans_clusters(){



   for (int i=0; i< ps.M; i++){
       vertex_data & data = ps.g<graph_type>()->vertex_data(i);
       if (!data.hot)
         continue;
       else {
         assert(data.current_cluster >=0 && data.current_cluster < ps.K);
         if ((ps.init_type == INIT_KMEANS_PLUS_PLUS && ps.iiter >= 1) || (ps.algorithm == K_MEANS && ps.init_type != INIT_KMEANS_PLUS_PLUS)){
           assert(data.prev_cluster >=0 && data.prev_cluster < ps.K);
           assert(data.prev_cluster != data.current_cluster);
         }
         //add point mass into new cluster
         if (ac.debug)
           std::cout<<" adding point " << i << " into cluster " << data.current_cluster << std::endl;
         plus(ps.clusts.cluster_vec[data.current_cluster].cur_sum_of_points , data.datapoint);    
         ps.clusts.cluster_vec[data.current_cluster].num_assigned_points++;
         
         if (ps.init_type == INIT_KMEANS_PLUS_PLUS && ps.iiter < 2 && data.prev_cluster == -1){
         }
         else{ //remove point from old cluster
           if (ac.debug)
           std::cout<<" removing point " << i << " from old cluster " << data.prev_cluster << std::endl;
           minus(ps.clusts.cluster_vec[data.prev_cluster].cur_sum_of_points , data.datapoint);    
           ps.clusts.cluster_vec[data.prev_cluster].num_assigned_points--;
         }
     
         data.hot = false;  
         if (ac.debug)
           std::cout<<"in hot node: " << i << std::endl;
       }
   }
   calc_cluster_centers();
}
 


#include <graphlab/macros_undef.hpp>
#endif //_ALS_HPP
