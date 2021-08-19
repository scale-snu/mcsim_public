#ifndef COHERENCE_TEST_H_
#define COHERENCE_TEST_H_

#include "gtest/gtest.h"
#include "gflags/gflags.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"
#include "../PTSDirectory.h"
#include "../PTSXbar.h"

#include "AddressGen.h"
#include <vector>

namespace PinPthread {

class CoherenceTest : public ::testing::Test {
  protected:
    static PthreadTimingSimulator* test_pts;
    static McSim* test_mcsim;
    static uint32_t l1_set;
    static uint64_t l1_tag;
    static uint32_t l2_set;
    static uint64_t l2_tag;
    // 26C8(hex) = 9928(dec) = 10 011011 001000(bin)
    static const uint64_t TEST_ADDR_D = 0x26C8;

    static void SetUpTestSuite() {
      test_pts = new PthreadTimingSimulator("../Apps/md/test-coherence.toml");
      test_mcsim = test_pts->mcsim;
    }

    virtual void TearDown() override {
      clear_geq();
    }

    static void TearDownTestSuite() {
      delete test_pts;
    }

    void clear_geq() { test_mcsim->global_q->event_queue.clear(); }
    void set_rob_entry(O3ROB & o3rob_entry, uint64_t _memaddr, uint64_t ready_time, bool isread = true);
};

}

#endif // COHERENCE_TEST_H
