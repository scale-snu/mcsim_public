#ifndef TLB_TEST_H_
#define TLB_TEST_H_

#include "gtest/gtest.h"
#include "gflags/gflags.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include <vector>

DECLARE_string(mdfile); // defined in /test/main.cc

namespace PinPthread {

class TLBTest : public ::testing::Test {

  protected:
    static PthreadTimingSimulator* test_pts;
    static O3Core* test_o3core;
    static TLBL1* test_tlbl1i;
    static TLBL1* test_tlbl1d;
    static std::vector<LocalQueueElement *> events;

    static void SetUpTestSuite() {
      test_pts = new PthreadTimingSimulator(FLAGS_mdfile);
      test_o3core = test_pts->mcsim->o3cores[0];
      test_tlbl1i = test_pts->mcsim->tlbl1is[0];
      test_tlbl1d = test_pts->mcsim->tlbl1ds[0];
    }

    void clear_geq() { test_tlbl1i->geq->event_queue.clear(); }
    LocalQueueElement * create_tlb_read_event(uint64_t _address, Component * from); 
    void set_rob_entry(O3ROB & o3rob_entry, uint64_t _ip, uint64_t _memaddr);
}; 

}

#endif // TLB_TEST_H
