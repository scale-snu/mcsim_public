cmake_minimum_required(VERSION 3.10)

project(mcsim)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(gflags REQUIRED)

# set(CMAKE_BUILD_TYPE Release)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# variable_watch(CMAKE_BUILD_TYPE)

if(NOT INCL_DIR)
  get_filename_component(INCL_DIR ../pin/extras/xed-intel64/include/xed ABSOLUTE )
endif()

if (NOT EXISTS ${INCL_DIR}/xed-category-enum.h)
  message(SEND_ERROR "Can't find xed-category-enum.h in ${INCL_DIR}")
endif()

set(SRC_FILES
  PTSCache.cc
  PTSComponent.cc
  PTSCore.cc
  PTSO3Core.cc
  PTSDirectory.cc
  PTSMemoryController.cc
  PTSTLB.cc
  PTSXbar.cc
  McSim.cc
  PTS.cc
  main.cc)

include_directories( ${INCL_DIR} )
#add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall -Weffc++ -Wextra -Wsign-conversion )
add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall )
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -g -O0 -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

add_executable(mcsim ${SRC_FILES})
target_link_libraries(mcsim gflags)
