#!/bin/bash

pushd .

cd $DEPS_DIR

function download_file {
  # detect wget
  echo "Downloading $2 from $1 ..."
  wgetcmd=`which wget`
  if [ -z $wgetcmd ] ; then
    curlcmd=`which curl`
    if [ -z $curlcmd ] ; then
      echo "Unable to find either curl or wget! Cannot proceed with automatic install."
      exit 1
    fi
    curl $1 -o $2
  else
    wget $1 -O $2
  fi
}

if [ -z $IN_BOOTSTRAP ]; then
  echo "This script should not be run directly."
else 
  cmakecmd="cmake"
  # use which to look for a cmake in the path
  cmake_pos=`which $cmakecmd`
  if [ ! -z $force_cmake_install ] ; then
    echo "Forcing cmake install..."
  fi
  if [ ! -z $force_cmake_install ] || [ -z "$cmake_pos" ] ; then
    # get the current path and remember it. This will be the installation directory
    installprefix=$PWD

    # check for cmake in the current directory

    if [ ! -z $force_cmake_install ] || [ ! -f "cmake" ] ; then
      echo " ==================== CMake Not Found ! ==================== "
      echo
      echo "This script will now proceed to download CMake and set it up in"
      echo "the local graphlabapi/deps directory. The GraphLab compilation "
      echo "process will be directed to use graphlabapi/deps/cmake."
      echo
      echo "Press Enter to continue, "
      echo "or press Ctrl-C to break and install CMake yourself."
      if [ -z $always_yes ] ; then
        read
      fi
      echo "Looking for latest version of CMake..."

      # get the cmake software page
      rm -f software.html
      download_file "http://www.cmake.org/cmake/resources/software.html" software.html
      # look for the first tar.gz I can download
      cmakedownload=`grep -m 1 -o -e "href=\"http://www\\.cmake.*\\.tar\\.gz\"" software.html | grep -o -e "http.*\\.tar\\.gz"`
      if [ -z "$cmakedownload" ] ; then
        echo "Unable to locate CMake package. You will have to install it yourself."
        exit 1
      fi
      rm -f cmake.tar.gz
      set -e
      download_file $cmakedownload cmake.tar.gz
      tar -xzvf cmake.tar.gz
      # cd into the extracted directory and install
      cd cmake-*
      ./configure --prefix=$installprefix
      make -j2
      make install
      set +e
      cd ..

      # make a link back to the deps directory
      ln -s $installprefix/bin/cmake cmake
      cmakecmd=$installprefix/bin/cmake
    else
      echo " cmake detected in the graphlabapi/deps directory."
      cmakecmd=$installprefix/bin/cmake
    fi
  else
    echo "cmake detected in $cmake_pos. Skipping cmake installation."
  fi
fi


popd




