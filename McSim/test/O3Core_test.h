#ifndef O3CORE_TEST_H_
#define O3CORE_TEST_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"

#include <vector>

namespace PinPthread {

class O3CoreTest : public ::testing::Test {
  protected:
    static std::unique_ptr<PinPthread::PthreadTimingSimulator> test_pts;
    static O3CoreForTest* test_o3core;
    static TLBL1ForTest* test_tlbl1i;
    static TLBL1ForTest* test_tlbl1d;
    static CacheL1* test_cachel1i;
    static CacheL1* test_cachel1d;
    static const uint64_t TEST_ADDR_I = 0x401640;
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8;
    static std::vector<LocalQueueElement *> request_events;
    static std::vector<LocalQueueElement *> reply_events;

    static void SetUpTestSuite() {
      test_pts = std::make_unique<PthreadTimingSimulator>("../Apps/md/test-md.toml");

      test_o3core = dynamic_cast<O3CoreForTest *>(test_pts->mcsim->o3cores[0]);
      test_tlbl1i = dynamic_cast<TLBL1ForTest *>(test_pts->mcsim->tlbl1is[0]);
      test_tlbl1d = dynamic_cast<TLBL1ForTest *>(test_pts->mcsim->tlbl1ds[0]);
      test_cachel1i = test_pts->mcsim->l1is[0];
      test_cachel1d = test_pts->mcsim->l1ds[0];
    }

    virtual void TearDown() override {
      clear_geq();
      O3Queue * test_o3queue = test_o3core->get_o3queue();
      for (unsigned int i = 0; i < test_o3core->get_o3queue_max_size(); i++) {
        test_o3queue[i].state = o3iqs_invalid;
      }
      test_o3core->set_o3queue_head(0);
      test_o3core->set_o3queue_size(0);
      O3ROB * test_o3rob = test_o3core->get_o3rob();
      for (unsigned int i = 0; i < test_o3core->get_o3rob_max_size(); i++) {
        test_o3rob[i].state = o3irs_invalid;
      }
      test_o3core->set_o3rob_head(0);
      test_o3core->set_o3rob_size(0);
    }
    
    void clear_geq() { test_o3core->geq->event_queue.clear(); }
};

}

#endif // O3CORE_TEST_H_
