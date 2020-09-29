#!/bin/bash
ROOT="$(pwd)"
echo $ROOT

THIRD_PARTY_DIR="$ROOT/third-party"
DEP_BUILD_DIR="$ROOT/build"


## third-party installation
install_third_party() {

  git submodule update --init

  cd $THIRD_PARTY_DIR

  if [ ! -d "$DEP_BUILD_DIR" ]; then
    mkdir $DEP_BUILD_DIR
  fi

  # 1. gflags:
  cd gflags
  mkdir -p build && cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$DEP_BUILD_DIR -DGFLAGS_NAMESPACE=gflags -DBUILD_SHARED_LIBS=ON
  make -j2
  make install
  ldconfig
  cd $THIRD_PARTY_DIR

  # 2. glog:
  cd glog
  mkdir build; cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$DEP_BUILD_DIR -DBUILD_SHARED_LIBS=ON
  make -j2
  make install
  cd $THIRD_PARTY_DIR

  # 3. snappy:
  cd snappy
  mkdir build; cd build
  cmake ..
}


# Download Pin - A Binary Instrumentation Tool
download_intel_pin()
{
  cd "$THIRD_PARTY_DIR"

  wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.15-98253-gb56e429b1-gcc-linux.tar.gz
  tar -xvf pin-3.15-98253-gb56e429b1-gcc-linux.tar.gz
  ln -s "$THIRD_PARTY_DIR"/pin-3.15-98253-gb56e429b1-gcc-linux "$ROOT"/pin 
}


install_mcsim_frontend()
{
  cd "$ROOT"/Pthread

  make clean PIN_ROOT="$ROOT"/pin
  make PIN_ROOT="$ROOT"/pin obj-intel64/mypthreadtool.so -j4
  make PIN_ROOT="$ROOT"/pin obj-intel64/libmypthread.a
}


install_mcsim_frontend_debug()
{
  cd "$ROOT"/Pthread

  make clean PIN_ROOT="$ROOT"/pin
  make DEBUG=1 PIN_ROOT="$ROOT"/pin obj-intel64/mypthreadtool.so -j4
  make DEBUG=1 PIN_ROOT="$ROOT"/pin obj-intel64/libmypthread.a
}


install_mcsim_backend()
{
  # McSim Back-end Build
  cd "$ROOT"/McSim
  rm -rf build
   
  mkdir build && cd build
  cmake .. && make -j
}


install_mcsim_backend_debug()
{
  # McSim Back-end Build
  cd "$ROOT"/McSim
  rm -rf debug
   
  mkdir debug && cd debug
  cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j
}


install_tracegen()
{
  setarch x86_64 -R ./McSim/build/mcsim -mdfile Apps/md/o3-closed.toml -runfile Apps/list/run-stream.toml -logtostderr=true
  cd "$ROOT"/TraceGen
  make PIN_ROOT="$ROOT"/pin obj-intel64/tracegen.so -j
}


build_stream_example()
{
  cd "$ROOT"/McSim/stream
  make clean && make -j
}


run_test()
{
  cd "$ROOT"
  source bash_setup
  setarch x86_64 -R ./McSim/build/mcsim -mdfile Apps/md/o3-closed.toml -runfile Apps/list/run-stream.toml -logtostderr=true
}


## main: script start
thisexec=`basename "$0"`
thisdir=`dirname "$0"`
[ -z "$HOSTNAME" ] && HOSTNAME=$(hostname);

trap "" TSTP # Disable Ctrl-Z
trap "" 6

unset LD_LIBRARY_PATH
unset CPLUS_INCLUDE_PATH 
unset C_INCLUDE_PATH
unset PKG_CONFIG_PATH

install_third_party

download_intel_pin

install_mcsim_frontend

install_mcsim_backend

install_tracegen

#build_stream_example

#run_test
