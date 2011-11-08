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


// requires mex.h and rtwtypes.h to be included before this
#ifndef __RTWTYPES_H__
#error "rtwtypes.h must be included before this file"
#endif

#ifndef mex_h
// if there is no mex, we do not use this file
#define MEX_CLASSID_TRAITS_HPP
#endif

#ifndef MEX_CLASSID_TRAITS_HPP
#define MEX_CLASSID_TRAITS_HPP

/**
 * Checks if the classID is storage compatible with the type T
 */
template <typename T> inline bool compatible_classid(mxClassID cid) { return false; };

template <> inline bool compatible_classid<char>(mxClassID cid) {
  return cid == mxCHAR_CLASS || cid == mxINT8_CLASS || cid == mxUINT8_CLASS; 
};
template <> inline bool compatible_classid<int8_T>(mxClassID cid) {
  return cid == mxINT8_CLASS || cid == mxUINT8_CLASS; 
};
template <> inline bool compatible_classid<uint8_T>(mxClassID cid) {
  return cid == mxINT8_CLASS || cid == mxUINT8_CLASS; 
};
template <> inline bool compatible_classid<int16_T>(mxClassID cid) {
  return cid == mxINT16_CLASS || cid == mxUINT16_CLASS; 
};
template <> inline bool compatible_classid<uint16_T>(mxClassID cid) {
  return cid == mxINT16_CLASS || cid == mxUINT16_CLASS; 
};
template <> inline bool compatible_classid<int32_T>(mxClassID cid) {
  return cid == mxINT32_CLASS || cid == mxUINT32_CLASS; 
};
template <> inline bool compatible_classid<uint32_T>(mxClassID cid) {
  return cid == mxINT32_CLASS || cid == mxUINT32_CLASS; 
};
template <> inline bool compatible_classid<int64_T>(mxClassID cid) {
  return cid == mxINT64_CLASS || cid == mxUINT64_CLASS; 
};
template <> inline bool compatible_classid<uint64_T>(mxClassID cid) {
  return cid == mxINT64_CLASS || cid == mxUINT64_CLASS; 
};
template <> inline bool compatible_classid<real32_T>(mxClassID cid) {
  return cid == mxSINGLE_CLASS; 
};
template <> inline bool compatible_classid<real64_T>(mxClassID cid) {
  return cid == mxDOUBLE_CLASS; 
};
#ifdef CREAL_T
template <> inline bool compatible_classid<cint8_T>(mxClassID cid) {
  return cid == mxINT8_CLASS || cid == mxUINT8_CLASS; 
};
template <> inline bool compatible_classid<cuint8_T>(mxClassID cid) {
  return cid == mxINT8_CLASS || cid == mxUINT8_CLASS; 
};
template <> inline bool compatible_classid<cint16_T>(mxClassID cid) {
  return cid == mxINT16_CLASS || cid == mxUINT16_CLASS; 
};
template <> inline bool compatible_classid<cuint16_T>(mxClassID cid) {
  return cid == mxINT16_CLASS || cid == mxUINT16_CLASS; 
};
template <> inline bool compatible_classid<cint32_T>(mxClassID cid) {
  return cid == mxINT32_CLASS || cid == mxUINT32_CLASS; 
};
template <> inline bool compatible_classid<cuint32_T>(mxClassID cid) {
  return cid == mxINT32_CLASS || cid == mxUINT32_CLASS; 
};
template <> inline bool compatible_classid<cint64_T>(mxClassID cid) {
  return cid == mxINT64_CLASS || cid == mxUINT64_CLASS; 
};
template <> inline bool compatible_classid<cuint64_T>(mxClassID cid) {
  return cid == mxINT64_CLASS || cid == mxUINT64_CLASS; 
};
template <> inline bool compatible_classid<creal_T>(mxClassID cid) {
  return cid == mxSINGLE_CLASS; 
};
template <> inline bool compatible_classid<creal32_T>(mxClassID cid) {
  return cid == mxSINGLE_CLASS; 
};
template <> inline bool compatible_classid<creal64_T>(mxClassID cid) {
  return cid == mxDOUBLE_CLASS; 
};
#endif



/**
 * Converts a emx type to matlab traits information
 */
template <typename T> 
struct prefered_classid {
  static mxClassID cid() { return mxUNKNOWN_CLASS; };
  static bool complex() {return false; };
};

template <> 
struct prefered_classid<char> {
  static mxClassID cid() { return mxCHAR_CLASS; };
  static bool complex() {return false; };
};

template <> 
struct prefered_classid<int8_T> {
  static mxClassID cid() { return mxINT8_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<uint8_T> {
  static mxClassID cid() { return mxUINT8_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<int16_T> {
  static mxClassID cid() { return mxINT16_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<uint16_T> {
  static mxClassID cid() { return mxUINT16_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<int32_T> {
  static mxClassID cid() { return mxINT32_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<uint32_T> {
  static mxClassID cid() { return mxUINT32_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<int64_T> {
  static mxClassID cid() { return mxINT64_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<uint64_T> {
  static mxClassID cid() { return mxUINT64_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<real32_T> {
  static mxClassID cid() { return mxSINGLE_CLASS; };
  static bool complex() {return false; };
};
template <> 
struct prefered_classid<real64_T> {
  static mxClassID cid() { return mxDOUBLE_CLASS; };
  static bool complex() {return false; };
};

#ifdef CREAL_T
template <> 
struct prefered_classid<creal_T> {
  static mxClassID cid() { return mxSINGLE_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<creal32_T> {
  static mxClassID cid() { return mxSINGLE_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<creal64_T> {
  static mxClassID cid() { return mxDOUBLE_CLASS; };
  static bool complex() {return true; };
};

template <> 
struct prefered_classid<cint8_T> {
  static mxClassID cid() { return mxINT8_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cuint8_T> {
  static mxClassID cid() { return mxUINT8_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cint16_T> {
  static mxClassID cid() { return mxINT16_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cuint16_T> {
  static mxClassID cid() { return mxUINT16_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cint32_T> {
  static mxClassID cid() { return mxINT32_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cuint32_T> {
  static mxClassID cid() { return mxUINT32_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cint64_T> {
  static mxClassID cid() { return mxINT64_CLASS; };
  static bool complex() {return true; };
};
template <> 
struct prefered_classid<cuint64_T> {
  static mxClassID cid() { return mxUINT64_CLASS; };
  static bool complex() {return true; };
};
#endif

#endif

