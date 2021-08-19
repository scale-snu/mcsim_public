#ifndef O3CORE_TEST_H_
#define O3CORE_TEST_H_

#include "gtest/gtest.h"
#include "gflags/gflags.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"
#include <vector>

DECLARE_string(mdfile); // defined in /test/main.cc

namespace PinPthread {

class O3CoreTest : public ::testing::Test {

  protected:
    static PthreadTimingSimulator* test_pts;
    static O3Core* test_o3core;
    static TLBL1* test_tlbl1i;
    static TLBL1* test_tlbl1d;
    static CacheL1* test_cachel1i;
    static CacheL1* test_cachel1d;
    static const uint64_t TEST_ADDR_I = 0x401640;
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8;

    static void SetUpTestSuite() {
      // Called once per TEST Suite
      test_pts = new PthreadTimingSimulator(FLAGS_mdfile);
      test_o3core = test_pts->mcsim->o3cores[0];
      test_tlbl1i = test_pts->mcsim->tlbl1is[0];
      test_tlbl1d = test_pts->mcsim->tlbl1ds[0];
      test_cachel1i = test_pts->mcsim->l1is[0];
      test_cachel1d = test_pts->mcsim->l1ds[0];
    }

    virtual void TearDown() override {
      clear_geq();
      for (unsigned int i = 0; i < test_o3core->o3queue_max_size; i++) {
        test_o3core->o3queue[i].state = o3iqs_invalid;
      }
      test_o3core->o3queue_head = 0;
      test_o3core->o3queue_size = 0;
      for (unsigned int i = 0; i < test_o3core->o3rob_max_size; i++) {
        test_o3core->o3rob[i].state = o3irs_invalid;
      }
      test_o3core->o3rob_head = 0;
      test_o3core->o3rob_size = 0;
    }
    
    void clear_geq() { test_o3core->geq->event_queue.clear(); }
};

}

#endif 
