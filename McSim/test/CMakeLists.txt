#file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cc)
set(TEST_SOURCES
#  MC_scheduler_test.cc
#  TLB_test.cc
  O3Core_test.cc
#  Cache_test.cc
#  Coherence_test.cc
  AddressGen.cc
  main.cc)

#set(SOURCES ${TEST_SOURCES})
set(MCSIM_SRCS
  ../McSim.cc
  ../PTS.cc
  ../PTSCache.cc
  ../PTSComponent.cc
  ../PTSDirectory.cc
  ../PTSMemoryController.cc
  ../PTSO3Core.cc
  ../PTSProcessDescription.cc
  ../PTSTLB.cc
  ../PTSXbar.cc
  )

add_executable(mcsim-unittest ${TEST_SOURCES} ${MCSIM_SRCS})

target_link_libraries(mcsim-unittest gtest gflags glog stdc++fs)

#add_test(NAME mcsim-unittest COMMAND mcsim-unittest)
#add_test(NAME mcsim-unittest COMMAND setarch x86_64 -R mcsim-unittest -mdfile md-test.toml -runfile app-test.toml -logtostderr=true)
