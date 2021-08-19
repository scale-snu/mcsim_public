#ifndef MC_SCHED_H_
#define MC_SCHED_H_

#include "gtest/gtest.h"
#include "gflags/gflags.h"

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSMemoryController.h"
#include "AddressGen.h"

DECLARE_string(mdfile); // defined in /test/main.cc

namespace PinPthread {

class MCSchedTest : public ::testing::Test {

  protected:
    static PthreadTimingSimulator* test_pts;
    static MemoryController* test_mc;
    static std::vector<uint64_t> row_A_addresses;
    static std::vector<uint64_t> row_B_addresses;

    static void SetUpTestSuite() {
      // Called once per TEST Suite
      test_pts = new PthreadTimingSimulator(FLAGS_mdfile);
      test_mc = test_pts->mcsim->mcs[0];

      AddressGen* addrgen = new AddressGen();
      // addrgen->generate([MC #], [bank], [row])
      // row_A_addresses: MC(0), bank(0), row(0x10), different column #
      row_A_addresses.push_back(addrgen->generate(0, 0, 0x10) + 0xa);
      row_A_addresses.push_back(addrgen->generate(0, 0, 0x10) + 0xb);
      row_A_addresses.push_back(addrgen->generate(0, 0, 0x10) + 0xc);
      // row_B_addresses: MC(0), bank(0), row(0x20), different column #
      row_B_addresses.push_back(addrgen->generate(0, 0, 0x20) + 0xa);
      row_B_addresses.push_back(addrgen->generate(0, 0, 0x20) + 0xb);
      row_B_addresses.push_back(addrgen->generate(0, 0, 0x20) + 0xc);
      delete addrgen;
    }

    static void TearDownTestSuite() {
      delete test_pts;
    }
    
    void clear_geq() { test_mc->geq->event_queue.clear(); }
    LocalQueueElement * create_read_event(uint64_t _address);
};

}

#endif // MC_SCHED_H_
