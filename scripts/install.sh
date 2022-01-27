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
  cmake .. -DCMAKE_INSTALL_PREFIX=$DEP_BUILD_DIR -DBUILD_SHARED_LIBS=ON
  make -j2
  make install
  ldconfig
  cd $THIRD_PARTY_DIR

  # 2. glog:
  cd glog
  mkdir -p build && cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$DEP_BUILD_DIR -DBUILD_SHARED_LIBS=ON -Dgflags_DIR=$DEP_BUILD_DIR
  make -j2
  make install
  cd $THIRD_PARTY_DIR

  # 3. snappy:
  cd snappy
  mkdir -p build && cd build
  cmake ..
  cd $THIRD_PARTY_DIR

  # 4. googletest:
  cd googletest
  mkdir -p build && cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$DEP_BUILD_DIR -DBUILD_SHARED_LIBS=ON
  make -j
  make install
  cd $THIRD_PARTY_DIR
}

# Download Pin - A Binary Instrumentation Tool
download_intel_pin() {
  cd "$THIRD_PARTY_DIR"

  if [ ! -e "pin-3.20-98437-gf02b61307-gcc-linux.tar.gz" ]
  then
    wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.20-98437-gf02b61307-gcc-linux.tar.gz
    tar -xvf pin-3.20-98437-gf02b61307-gcc-linux.tar.gz
    ln -s "$THIRD_PARTY_DIR"/pin-3.20-98437-gf02b61307-gcc-linux "$ROOT"/pin
  fi
}

install_mcsim_frontend() {
  cd "$ROOT"/Pthread

  make clean PIN_ROOT="$ROOT"/pin
  make PIN_ROOT="$ROOT"/pin obj-intel64/mypthreadtool.so -j4
  make PIN_ROOT="$ROOT"/pin obj-intel64/libmypthread.a
}

install_mcsim_frontend_debug() {
  cd "$ROOT"/Pthread

  make clean PIN_ROOT="$ROOT"/pin
  make DEBUG=1 PIN_ROOT="$ROOT"/pin obj-intel64/mypthreadtool.so -j4
  make DEBUG=1 PIN_ROOT="$ROOT"/pin obj-intel64/libmypthread.a
}

install_mcsim_backend() {
  # McSim Back-end Build
  cd "$ROOT"/McSim
  rm -rf build
   
  mkdir build && cd build
  cmake .. && make -j
}

install_mcsim_backend_debug() {
  # McSim Back-end Build
  cd "$ROOT"/McSim
  rm -rf debug

  mkdir debug && cd debug
  cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j
}

install_tracegen() {
  cd "$ROOT"/TraceGen
  make PIN_ROOT="$ROOT"/pin obj-intel64/tracegen.so -j
}

build_stream_example() {
  cd "$ROOT"/McSim/stream
  make clean && make -j
}

build_dummy_binary() {
  cd "$ROOT"/McSim/dummy
  make clean && make -j
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

#install_mcsim_frontend_debug
#install_mcsim_backend_debug
#build_stream_example
#build_dummy_binary
