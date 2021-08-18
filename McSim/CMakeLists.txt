cmake_minimum_required(VERSION 3.10)

project(mcsim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Build type: Debug")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Build type: Release")
elseif(CMAKE_BUILD_TYPE MATCHES Trace)
  message(STATUS "Build type: Trace")
else()
  message(STATUS "Invalid build type: ${CMAKE_BUILD_TYPE}")
  message(STATUS "Default to : Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()


if (NOT INCL_XED_DIR)
  get_filename_component(INCL_XED_DIR ../pin/extras/xed-intel64/include/xed ABSOLUTE )
endif()

if (NOT EXISTS ${INCL_XED_DIR}/xed-category-enum.h)
  message(SEND_ERROR "Can't find xed-category-enum.h in ${INCL_XED_DIR}")
endif()

if (NOT INCL_TOML_DIR)
  get_filename_component(INCL_TOML_DIR ../third-party/toml11 ABSOLUTE)
endif()

if (NOT EXISTS ${INCL_TOML_DIR})
  message(SEND_ERROR "Can't find ${INCL_TOML_DIR}")
endif()

if (NOT INCL_LIB)
  get_filename_component(INCL_LIB ../build/include ABSOLUTE)
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

include_directories( ${INCL_XED_DIR} ${INCL_TOML_DIR} ${INCL_LIB})
#add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall -Weffc++ -Wextra -Wsign-conversion )
add_compile_options( -Wno-unknown-pragmas -DTARGET_IA32E -Wall )

# flags for release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -g")

# flags for debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -DDEBUG -DLOG_TRACE -ggdb -Og -g3 -fPIC")

# flags for trace build
set(CMAKE_CXX_FLAGS_TRACE "${CMAKE_CXX_FLAGS} -O3 -g -DLOG_TRACE")


link_directories(../build/lib)
include_directories(../build/include)

add_executable(mcsim ${SRC_FILES})
target_link_libraries(mcsim gflags)
target_link_libraries(mcsim glog)
target_link_libraries(mcsim stdc++fs)

# Unit Test
add_subdirectory(test)
add_custom_target(check COMMAND mcsim-unittest)
