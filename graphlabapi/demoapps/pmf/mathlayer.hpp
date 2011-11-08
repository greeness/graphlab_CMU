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
 */

#ifndef MATH_LAYER_GRAPHLAB
#define MATH_LAYER_GRAPHLAB




/**
 *
 * SET OF WRAPPER FUNCTIONS TO ALLOW USING EIGEN
 */
#ifdef HAS_EIGEN

#include <iostream>
#include <fstream>
#include <ostream>

#include "Eigen/Dense"
#define EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#include "Eigen/Sparse"
#include "Eigen/Cholesky"
#include "Eigen/Eigenvalues"


using namespace Eigen;

typedef MatrixXd mat;
typedef VectorXd vec;
typedef VectorXi ivec;
typedef SparseVector<double> sparse_vec;

mat randn1(int dx, int dy, int col);

inline mat eye(int size){
  return mat::Identity(size, size);
}
inline vec ones(int size){
  return vec::Ones(size);
}
inline vec zeros(int size){
  return vec::Zero(size);
}
inline mat zeros(int rows, int cols){
  return mat::Zero(rows, cols);
}
inline void debug_print_vec(const char * name,const vec& _vec, int len){
  printf("%s ) ", name);
  for (int i=0; i< len; i++)
    if (_vec(i) == 0)
      printf("      0    ");
    else printf("%12.4g    ", _vec(i));
  printf("\n");
}
inline vec init_vec(const double * array, int size){
  vec ret(size);
  memcpy(ret.data(), array, size*sizeof(double));
  return ret;
}
inline void dot2(const vec&  x1, const vec& x3, mat & Q, int j, int len){
	for (int i=0; i< len; i++){
		Q(i,j) = (x1(i) * x3(i));
	}
}

inline bool ls_solve_chol(const mat &A, const vec &b, vec &result){
    //result = A.jacobiSvd(ComputeThinU | ComputeThinV).solve(b);
    result = A.ldlt().solve(b);
    return true;
}
inline bool ls_solve(const mat &A, const vec &b, vec &result){
    //result = A.jacobiSvd(ComputeThinU | ComputeThinV).solve(b);
    result = A.ldlt().solve(b);
    return true;
}
inline bool chol(mat& sigma, mat& out){
   out = sigma.llt().matrixLLT();
   return true;
}
inline bool backslash(const mat& A, const vec & b, vec & x){
   x = A.jacobiSvd(ComputeThinU | ComputeThinV).solve(b);
   return true;
} 
inline mat transpose(mat & A){
   return A.transpose();
}
inline void set_val(mat &A, int row, int col, double val){
  A(row, col) = val;
}
inline double get_val(const mat &A, int row, int col){
  return A(row, col);
}
inline vec get_col(const mat& A, int col){
  return A.col(col);
}
inline vec get_row(const mat& A, int row){
  return A.row(row);
}
inline void set_col(mat& A, int col, const vec & val){
  A.col(col) = val;
}
inline void set_row(mat& A, int row, const vec & val){
  A.row(row) = val;
}
inline mat randn(int dx, int dy){
  return randn1(dx,dy,-1);
}
inline void set_diag(mat &A, vec & v){
   A.diagonal()=v;
}
inline double sumsum(const mat & A){
   return A.sum();
}
inline mat init_mat(const char * string, int row, int col){
  mat out(row, col);
  char buf[2056];
  strcpy(buf, string);
  char *pch = strtok(buf," \r\n\t;");
  for (int i=0; i< row; i++){
    for (int j=0; j< col; j++){
     out(i,j) = atof(pch);
     pch = strtok (NULL, " \r\n\t;");
    }
  }
  return out;
}
inline vec init_vec(const char * string, int size){
  vec out(size);
  char buf[2056];
  strcpy(buf, string);
  char *pch = strtok (buf," \r\n\t;");
  int i=0;
  while (pch != NULL)
  {
     out(i) =atof(pch);
     pch = strtok (NULL, " \r\n\t;");
     i++;
  }
  assert(i == size);
  return out;
}
inline vec init_dbl_vec(const char * string, int size){
  return init_vec(string, size);
}

inline double norm(const mat &A, int pow=2){
     return A.squaredNorm();
}
inline mat inv(const mat&A){
   return A.inverse();
}
inline bool inv(const mat&A, mat &out){
   out = A.inverse();
   return true;
}
inline mat outer_product(const vec&a, const vec&b){
   return a*b.transpose();
}
inline void sort(ivec &a){
   std::sort(a.data(), a.data()+a.size());
}
inline void sort(vec & a){
   std::sort(a.data(), a.data()+a.size());
}
inline ivec sort_index(const vec&a){
  ivec ret(a.size()); 
  std::vector<std::pair<double,int> > D;
  // 	
  D.reserve(a.size());
  for (int i=0;i<a.size();i++)
    D.push_back(std::make_pair<double,int>(a.coeff(i),i));
  std::sort(D.begin(),D.end());
  for (int i=0;i<a.size();i++)
  { 
    ret[i]=D[i].second;
  } 
  return ret;
}

//Eigen does not sort eigenvalues, as done in matlab
inline bool eig_sym(const mat & T, vec & eigenvalues, mat & eigenvectors){
   //
   //Column  of the returned matrix is an eigenvector corresponding to eigenvalue number  as returned by eigenvalues(). The eigenvectors are normalized to have (Euclidean) norm equal to one.
   SelfAdjointEigenSolver<mat> solver(T);
   eigenvectors = solver.eigenvectors();
   eigenvalues = solver.eigenvalues(); 
   ivec index = sort_index(eigenvalues);
   sort(eigenvalues);
   vec eigenvalues2 = eigenvalues.reverse();
   mat T2 = zeros(eigenvectors.rows(), eigenvectors.cols());
   for (int i=0; i< eigenvectors.cols(); i++){
      set_col(T2, index[i], get_col(eigenvectors, i));
   }   
   eigenvectors = T2;
   eigenvalues = eigenvalues2;
   return true;
}
inline vec head(const vec& v, int num){
   return v.head(num);
}
inline vec mid(const vec&v, int start, int num){
   return v.segment(start, std::min(num, (int)(v.size()-start)));
}
inline vec tail(const vec&v,  int num){
   return v.segment(v.size() - num, num);
}
inline ivec head(const ivec& v, int num){
   return v.head(num);
}
inline vec elem_mult(const vec&a, const vec&b){
   vec ret = a;
   for (int i=0; i<b.size(); i++)
      ret(i) *= b(i);
   return ret;
}
inline sparse_vec elem_mult(const sparse_vec&a, const sparse_vec&b){
   return a.cwiseProduct(b);
}
inline double sum(const vec & a){
  return a.sum();
}
template<typename T>
double sum_sqr(const T& a);

template<>
inline double sum_sqr<vec>(const vec & a){
  vec ret = a.array().pow(2);
  return ret.sum();
}
inline double trace(const mat & a){
  return a.trace();
}
inline double min(const vec &a){
  return a.minCoeff();
}
inline double max(const vec & a){
  return a.maxCoeff();
}
inline vec randu(int size){
  return vec::Random(size);
}
inline double randu(){
  return vec::Random(1)(0);
}
inline ivec randi(int size, int from, int to){
  ivec ret(size);
  for (int i=0; i<size; i++)
    ret[i]= internal::random<int>(from,to);
  return ret;
}
inline int randi(int from, int to){
  return internal::random<int>(from,to);
}
inline ivec concat(const ivec&a, const ivec&b){ 
   ivec ret(a.size()+b.size());
   ret << a,b;
   return ret;
}
inline void del(ivec&a, int i){
   memcpy(a.data()+i, a.data() + i+1, (a.size() - i - 1)*sizeof(int)); 
   a.conservativeResize(a.size() - 1); //resize without deleting values!
}
inline mat get_cols(const mat&A, ivec & cols){
  mat a(A.rows(), cols.size());
  for (int i=0; i< cols.size(); i++)
    set_col(a, i, get_col(A, cols[i]));
  return a;
}
inline void set_val(vec & v, int pos, double val){
  v(pos) = val;
}
inline double dot(const vec&a, const vec& b){
   return a.dot(b);
}
inline vec reverse(vec& a){
   return a.reverse();
}
inline ivec reverse(ivec& a){
   return a.reverse();
}
inline const double * data(const mat &A){
  return A.data();
}
inline const double * data(const vec &v){
  return v.data();
}

class it_file{
  std::fstream fb;

public:
  it_file(const char * name){
  fb.open(name, std::fstream::in);
  fb.close();

  if (fb.fail()){
     fb.clear(std::fstream::failbit);
     fb.open(name, std::fstream::out | std::fstream::trunc );
  }
  else {
     fb.open(name, std::fstream::in);
  }
   
   if (!fb.is_open()){
     perror("Failed opening file ");
     printf("filename is: %s\n", name);
     assert(false);
   }
  
  };
  std::fstream & operator<<(const std::string str){
   int size = str.size();
   fb.write((char*)&size, sizeof(int));
   assert(!fb.fail());
   fb.write(str.c_str(), size);
   return fb;
  }
  std::fstream &operator<<(mat & A){
   int rows = A.rows(), cols = A.cols();
   fb.write( (const char*)&rows, sizeof(int));
   fb.write( (const char *)&cols, sizeof(int));
   for (int i=0; i< A.rows(); i++)
      for (int j=0; j< A. cols(); j++){
         double val = A(i,j);
         fb.write( (const char *)&val, sizeof(double));
         assert(!fb.fail());
      }
   return fb;
  }
   std::fstream &operator<<(const vec & v){
   int size = v.size();
   fb.write( (const char*)&size, sizeof(int));
   assert(!fb.fail());
   for (int i=0; i< v.size(); i++){
      double val = v(i);
      fb.write( (const char *)&val, sizeof(double));
      assert(!fb.fail());
   }
   return fb;
  }
  std::fstream & operator>>(std::string  str){
    int size = -1;
    fb.read((char*)&size, sizeof(int));
       if (fb.fail() || fb.eof()){
       perror("Failed reading file");
       assert(false);
    }
     
    char buf[256];
    fb.read(buf, std::min(256,size));
    assert(!fb.fail());
    assert(!strncmp(str.c_str(), buf, std::min(256,size)));
    return fb;
  }

  std::fstream &operator>>(mat & A){
   int rows, cols;
   fb.read( (char *)&rows, sizeof(int));
   assert(!fb.fail());
   fb.read( (char *)&cols, sizeof(int));
   assert(!fb.fail());
   A = mat(rows, cols);
   double val;
   for (int i=0; i< A.rows(); i++)
      for (int j=0; j< A. cols(); j++){
        fb.read((char*)&val, sizeof(double));
        assert(!fb.fail());
        A(i,j) = val;
      }
   return fb;
  }
   std::fstream &operator>>(vec & v){
   int size;
   fb.read((char*)&size, sizeof(int));
   assert(!fb.fail());
   assert(size >0);
   v = vec(size);
   double val;
   for (int i=0; i< v.size(); i++){
      fb.read((char*)& val, sizeof(double));
     assert(!fb.fail());
      v(i) = val;
   }
   return fb;
  }



  void close(){
     fb.close();
  }
};

#define Name(a) std::string(a)
inline void set_size(sparse_vec &v, int size){
  //did not find a way to declare vector dimension, yet
}
inline void set_new(sparse_vec&v, int ind, double val){
  v.insert(ind) = val;
} 
inline int nnz(sparse_vec& v){
  return v.nonZeros();
}
inline int get_nz_index(sparse_vec &v, sparse_vec::InnerIterator& i){
  return i.index();
}
inline double get_nz_data(sparse_vec &v, sparse_vec::InnerIterator& i){
  return i.value();
}
#define FOR_ITERATOR(i,v) \
  for (sparse_vec::InnerIterator i(v); i; ++i)

template<>
inline double sum_sqr<sparse_vec>(const sparse_vec & a){
  double sum=0;
  FOR_ITERATOR(i,a){
    sum+= powf(i.value(),2);
  }
  return sum;
}



inline double get_nz_data(sparse_vec &v, int i){
  assert(nnz(v) > i);
  int cnt=0;
  FOR_ITERATOR(j, v){
    if (cnt == i){
      return j.value();
    }
    cnt++;
  }
  return 0.0;
}
inline void print(sparse_vec & vec){
  int cnt = 0;
  FOR_ITERATOR(i, vec){
    std::cout<<get_nz_index(vec, i)<<":"<< get_nz_data(vec, i) << " ";
    cnt++;
    if (cnt >= 20)
       break;
  }
  std::cout<<std::endl;
}
inline vec pow(const vec&v, int exponent){
  vec ret = vec(v.size());
  for (int i=0; i< v.size(); i++)
    ret[i] = powf(v[i], exponent);
  return ret;
}
inline double dot_prod(sparse_vec &v1, sparse_vec & v2){
  return v1.dot(v2);
}
inline double dot_prod(const vec &v1, const vec & v2){
  return v1.dot(v2);
}
inline double dot_prod(sparse_vec &v1, const vec & v2){
  double sum = 0;
  for (int i=0; i< v2.size(); i++){
    sum+= v2[i] * v1.coeffRef(i);
  }
  return sum;
}
inline vec cumsum(vec& v){
  vec ret = v;
  for (int i=1; i< v.size(); i++)
     for (int j=0; j< i; j++)
       ret(i) += v(j);
  return ret;
}
inline double get_val(sparse_vec & v1, int i){ //TODO optimize performance
  for (sparse_vec::InnerIterator it(v1); it; ++it)
    if (it.index() == i)
       return it.value();

  return 0;
} 
inline double get_val(vec & v1, int i){
  return v1(i);
}
inline void set_div(sparse_vec&v, sparse_vec::InnerIterator i, double val){
   v.coeffRef(i.index()) /= val;
}
inline sparse_vec minus(sparse_vec &v1,sparse_vec &v2){
   return v1-v2;
}
inline vec minus( sparse_vec &v1,  vec &v2){
   return v1-sparse_vec(v2);
}
inline void plus( vec &v1,  sparse_vec &v2){
   FOR_ITERATOR(i, v2){
     v1[i.index()] += i.value();
   }
}
inline void minus( vec &v1, sparse_vec &v2){
   FOR_ITERATOR(i, v2){
      v1[i.index()] -= i.value();
   }
}
inline sparse_vec fabs( sparse_vec & dvec1){
   sparse_vec ret = dvec1;
   FOR_ITERATOR(i, ret){
      ret.coeffRef(i.index()) = fabs(i.value()); 
   }	
   return ret;
};

inline vec fabs( const vec & dvec1){
   vec ret(dvec1.size());
   for (int i=0; i< dvec1.size(); i++){
      ret(i) = fabs(dvec1(i));
   }	
   return ret;
};
inline double abs_sum(const mat& A){
  double sum =0;
  for (int i=0; i< A.rows(); i++)
    for (int j=0; j< A.cols(); j++)
      sum += fabs(A(i,j));
  return sum;
}
inline double abs_sum(const vec &v){
  double sum =0;
  for (int i=0; i< v.size(); i++)
      sum += fabs(v(i));
  return sum;
}
inline double sum(const sparse_vec &v){
  double sum =0;
  FOR_ITERATOR(i, v){
      sum += i.value();
  }
  return sum;
}
inline vec sqrt(vec & v){
   vec ret(v.size());
   for (int i=0; i< v.size(); i++){
      ret[i] = sqrt(v(i));
   }
   return ret;
}
#else //eigen is not found
/***
 *
 *  SET OF WRAPPER FUNCTIONS TO BE USED WITH IT++
 *
 * */
#if defined(HAS_ITPP)

#include <itpp/itbase.h>
#include <itpp/itstat.h>
#include <itpp/stat/misc_stat.h>
#include "itppvecutils.hpp"
using namespace itpp;

//#undef sparse_vec
//typedef Sparse_Vec<float> sparse_vec;

inline void compact(sparse_vec & a){
  //TODO
}

inline void set_val(mat& A, int row, int col, double val){
  A.set(row, col, val);
}
inline double get_val(const mat& A, int row, int col){
  return A.get(row, col);
}
inline vec get_col(mat& A, int col){
  return A.get_col(col);
}
inline vec get_row(mat& A, int row){
  return A.get_row(row);
}
inline void set_col(mat& A, int col, const vec & val){
  A.set_col(col, val);
}
inline void set_row(mat& A, int row, const vec & val){
  A.set_row(row, val);
}
inline void set_diag(mat &A, vec &v){
  A = diag(v);
}
inline mat init_mat(const char * string, int row, int col){
  return mat(string);
}
inline vec init_vec(const char * string, int size){
  return vec(string);
}
inline vec init_dbl_vec(const char * string, int size){
  return vec(string);
}
inline vec head(const vec &v, int num){
  return v.mid(0,num);
}
inline vec mid(const vec&v, int start, int num){
  return v.mid(start, num);
}
inline vec tail(const vec&v, int num){
  return v.mid(v.size()-num, num);
}
inline ivec head(const ivec &v, int num){
  return v.mid(0,num);
}
inline ivec sort(ivec &a){
   Sort<int> sorter;
   sorter.sort(0, a.size()-1, a);
   return a;
}
inline void del(ivec&a, int i){
  a.del(i);
}
inline ivec sort_index(vec& a){
  Sort<double> sorter;
  return sorter.sort_index(0, a.size()-1, a);
}
inline void set_val(vec & v, int pos, double val){
  v.set(pos, val);
}
inline mat get_cols(const mat &A, const ivec & ind){
  return A.get_cols(ind);
}
inline const double * data(const mat &A){
  return A._data();
}
inline const double * data(const vec &v){
  return v._data();
}
inline void set_size(sparse_vec &v, int size){
  v.set_size(size);
}
inline void set_size(mat &a, int row, int col){
  a.set_size(row, col);
}
inline void set_new(sparse_vec&v, int ind, double val){
  v.set_new(ind, val);
} 
#define FOR_ITERATOR(i, v) \
    for (int i = 0; i < v.nnz(); i++)
inline int get_nz_index(sparse_vec &v, int i){
  return v.get_nz_index(i);
}
inline double get_nz_data(sparse_vec &v, int i){
  return v.get_nz_data(i);
}
inline int nnz(sparse_vec & v){
  return v.nnz();
}
inline double dot_prod(sparse_vec &v1, sparse_vec & v2){
  return v1*v2;
}
inline double dot_prod(vec &v1, vec & v2){
  return v1*v2;
}
inline double dot_prod(const sparse_vec &v1, const vec & v2){
  return v1*v2;
}
inline double dot_prod(vec &v1, sparse_vec & v2){
  return v1*v2;
}
inline double get_val(sparse_vec & v1, int i){
   FOR_ITERATOR(j, v1){
      if (v1.get_nz_index(j) == i)
         return v1.get_nz_data(j);
   }
   return 0;
}
inline double get_val(vec & v1, int i){
  return v1[i];
}
inline void set_div(sparse_vec&v, int i, double val){
  v.set(v.get_nz_index(i) ,v.get_nz_data(i) / val);
}
inline sparse_vec minus(sparse_vec &v1,sparse_vec &v2){
/*  sparse_vec ret; 
  for (int i=0; i< v1.nnz(); i++){
      ret.set_new(v1.get_nz_index(i), v1.get_nz_data(i) - get_val(v2, v1.get_nz_index(i)));
  }
  for (int i=0; i< v2.nnz(); i++){
      ret.set_new(v2.get_nz_index(i), get_val(v1, v2.get_nz_index(i)) - v2.get_nz_data(i));
  }
  return ret;*/
  return v1+(-v2);
}
inline vec minus( sparse_vec &v1,  vec &v2){
  vec ret = -v2;;
  FOR_ITERATOR(i, v1){  
    ret.set(v1.get_nz_index(i), ret.get(v1.get_nz_index(i)) + v1.get_nz_data(i));
  }
  return ret;
}
inline void plus( vec &v1,  sparse_vec &v2){
  FOR_ITERATOR(i, v2){ 
     v1[get_nz_index(v2, i)] += get_nz_data(v2, i);
  }
}
inline void minus( vec &v1, sparse_vec &v2){
  FOR_ITERATOR(i, v2){ 
     v1[get_nz_index(v2, i)] -= get_nz_data(v2, i);
  }
}
inline sparse_vec fabs( sparse_vec & dvec1){
   sparse_vec ret(dvec1.size(), dvec1.nnz());
   FOR_ITERATOR(i, dvec1){
       set_new(ret,get_nz_index(dvec1, i), fabs(get_nz_data(dvec1, i)));
   }
   return ret;
	
};

inline vec fabs(const vec & a){
   return abs(a);
}
inline double abs_sum(const mat& A){
   return sumsum(abs(A));
}
inline double abs_sum(const vec&v){
  return sum(fabs(v));
}
inline bool eig_sym(const mat & T, vec & eigenvalues, mat & eigenvectors){
  itpp::eig_sym(T,eigenvalues, eigenvectors);
  eigenvalues = reverse(eigenvalues);
  mat reverse_cols = zeros(eigenvectors.rows(), eigenvectors.cols());
  for (int i=0; i< eigenvectors.cols(); i++){
    reverse_cols.set_col(i, eigenvectors.get_col(eigenvectors.rows() - i - 1));
  }
  eigenvectors = reverse_cols;
  return true;
}

inline double sum_sqr(sparse_vec & v){
  double sum = 0;
  FOR_ITERATOR(i, v){
     sum+= powf(v.get_nz_data(i),2);
  }
  return sum;
}
inline void debug_print_vec(const char * name,const vec& _vec, int len){
  printf("%s ) ", name);
  for (int i=0; i< len; i++)
    if (_vec[i] == 0)
      printf("      0    ");
    else printf("%12.4g    ", _vec[i]);
  printf("\n");
}

inline void dot2(const vec&  x1, const vec& x3, mat & Q, int j, int len){
	for (int i=0; i< len; i++){
		Q.set(i,j,(x1[i] * x3[i]));
	}
}
inline void assign(vec & v1, sparse_vec & v2, int N){
  v1 = zeros(N);
  FOR_ITERATOR(i, v2){
     v1[get_nz_index(v2, i)] = get_nz_data(v2, i);
  }

}

inline double get(sparse_vec & v1, int pos){
  FOR_ITERATOR(i, v1){
     if (get_nz_index(v1, i) < pos)
	continue;
     else if (get_nz_index(v1, i) > pos)
	break;
     else if (get_nz_index(v1, i) == pos)
	return get_nz_data(v1, i);
  }
  return 0;
}


inline double min( sparse_vec & dvec){
 
  double dmin = 1e100;
  FOR_ITERATOR(i, dvec){
     dmin = std::min(dmin, get_nz_data(dvec, i));
  }
  return dmin;
}

inline double max( sparse_vec & dvec){
 
  double dmax = -1e100;
  FOR_ITERATOR(i, dvec){
     dmax = std::max(dmax, get_nz_data(dvec, i));
  }
  return dmax;
}
inline void plus_mul( vec &v1,  sparse_vec &v2, double factor){
  FOR_ITERATOR(i, v2){  
    v1[get_nz_index(v2, i)] += factor*get_nz_data(v2, i);
  }
}

inline double sum( sparse_vec & dvec){
  double sum = 0;
  FOR_ITERATOR(i, dvec){
     sum += get_nz_data(dvec, i);
  }
  return sum;
}
inline vec dbl_fzeros(int size){ 
  return itpp::zeros(size);
}
inline mat dbl_fzeros(int rows, int cols){
  return itpp::zeros(rows, cols);
}
inline void print(sparse_vec & vec){
  int cnt = 0;
  FOR_ITERATOR(i, vec){
    std::cout<<get_nz_index(vec, i)<<":"<< get_nz_data(vec, i) << " ";
    cnt++;
    if (cnt >= 20)
       break;
  }
  std::cout<<std::endl;
}
inline vec init_vec(double * array, int size){
  return vec(array, size);
}
/**
 * It seems that it++ random number generator is not thread safe so
 * we are using graphlab's
 */
inline ivec randi(int size, int from, int to){
  ivec ret(size);
  for (int i=0; i<size; i++)
    ret[i]= graphlab::random::uniform<int>(from,to);
  return ret;
}
inline int randi(int from, int to){
  return graphlab::random::uniform<int>(from,to);
}

#endif

#endif //eigen
#endif //MATH_LAYER_GRAPHLAB
