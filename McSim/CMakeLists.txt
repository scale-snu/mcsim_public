cmake_minimum_required(VERSION 3.10)

project(mcsim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(gflags REQUIRED)
find_package(glog REQUIRED)

# set(CMAKE_BUILD_TYPE Release)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# variable_watch(CMAKE_BUILD_TYPE)

if (NOT INCL_XED_DIR)
  get_filename_component(INCL_XED_DIR ../pin/extras/xed-intel64/include/xed ABSOLUTE )
endif()

if (NOT EXISTS ${INCL_XED_DIR}/xed-category-enum.h)
  message(SEND_ERROR "Can't find xed-category-enum.h in ${INCL_XED_DIR}")
endif()

if (NOT INCL_TOML_DIR)
  get_filename_component(INCL_TOML_DIR ../toml11 ABSOLUTE)
endif()

if (NOT EXISTS ${INCL_TOML_DIR})
  message(SEND_ERROR "Can't find ${INCL_TOML_DIR}")
endif()

set(SRC_FILES
  PTSCache.cc
  PTSComponent.cc
  PTSO3Core.cc
  PTSDirectory.cc
  PTSMemoryController.cc
  PTSTLB.cc
  PTSXbar.cc
  McSim.cc
  PTS.cc
  PTSProcessDescription.cc
  main.cc)

include_directories( ${INCL_XED_DIR} ${INCL_TOML_DIR} )
#add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall -Weffc++ -Wextra -Wsign-conversion )
add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall )
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -g -O0 -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++fs")

add_executable(mcsim ${SRC_FILES})
target_link_libraries(mcsim gflags)
target_link_libraries(mcsim glog::glog)
target_link_libraries(mcsim stdc++fs)
# if (NOT GLOG_PATH)
#   target_link_libraries(mcsim glog::glog)
# else()
#   target_link_libraries(mcsim unwind)
#   target_link_libraries(mcsim ${GLOG_PATH}/libglog.a)
# endif()
