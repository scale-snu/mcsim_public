#ifndef TLB_TEST_H_
#define TLB_TEST_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"

#include <vector>

namespace PinPthread {

class TLBTest : public ::testing::Test {
  protected:
    static std::unique_ptr<PthreadTimingSimulator> test_pts;
    static O3CoreForTest* test_o3core;
    static TLBL1ForTest* test_tlbl1i;
    static TLBL1ForTest* test_tlbl1d;
    static O3ROB* test_o3rob;
    static std::vector<LocalQueueElement *> events;
    static const uint64_t TEST_ADDR_I = 0x401640;       // instruction
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8; // data

    static void SetUpTestSuite() {
      test_pts = std::make_unique<PthreadTimingSimulator>("../Apps/md/test-md.toml");

      test_o3core = dynamic_cast<O3CoreForTest *>(test_pts->mcsim->o3cores[0]);
      test_o3rob = test_o3core->get_o3rob();
      test_tlbl1i = dynamic_cast<TLBL1ForTest *>(test_pts->mcsim->tlbl1is[0]);
      test_tlbl1d = dynamic_cast<TLBL1ForTest *>(test_pts->mcsim->tlbl1ds[0]);
    }

    void clear_geq() { test_tlbl1i->geq->event_queue.clear(); }
    LocalQueueElement * create_tlb_read_event(uint64_t _address, Component * from); 
    void set_rob_entry(O3ROB & o3rob_entry, uint64_t _ip, uint64_t _memaddr);
};

}

#endif // TLB_TEST_H
