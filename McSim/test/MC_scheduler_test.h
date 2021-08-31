#ifndef MC_SCHED_H_
#define MC_SCHED_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTSProcessDescription.h"

#include "../PTSMemoryController.h"
#include "../PTSDirectory.h"
#include "AddressGen.h"

namespace PinPthread {

class MemoryControllerForTest : public MemoryController {
 public:
  explicit MemoryControllerForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    MemoryController(type_, num_, mcsim_) { }
  ~MemoryControllerForTest() { }
  bool get_policy() { return policy; }
  uint32_t rank_num(uint64_t addr) { return get_rank_num(addr); }
  uint32_t bank_num(uint64_t addr) { return get_bank_num(addr); }
  uint64_t page_num(uint64_t addr) { return get_page_num(addr); }
  uint64_t get_num_read() { return num_read; };
  uint64_t get_num_write() { return num_write; };
  uint64_t get_num_activate() { return num_activate; };
  uint64_t get_num_precharge() { return num_precharge; };
  BankStatus get_bank_status(uint rank, uint bank) { return bank_status[rank][bank]; }
};

class MCSchedTest : public ::testing::Test {
  protected:
    static std::shared_ptr<PinPthread::PthreadTimingSimulator> test_pts;
    static MemoryControllerForTest* test_mc;
    static std::vector<uint64_t> row_A_addresses;
    static std::vector<uint64_t> row_B_addresses;

    static void SetUpTestSuite() {
      // Called once per TEST Suite
      test_pts = std::make_unique<PthreadTimingSimulator>("../Apps/md/test/test-md.toml");
      
      test_mc = new MemoryControllerForTest(ct_memory_controller, 0, test_pts->mcsim);
      auto temp_mc = test_pts->mcsim->mcs[0];
      test_pts->mcsim->mcs[0] = test_mc;

      test_pts->mcsim->connect_comps();
      delete temp_mc;
      
      AddressGen* addrgen = new AddressGen(test_pts);
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

    void clear_geq() { test_mc->geq->event_queue.clear(); }
    LocalQueueElement * create_read_event(uint64_t _address);
    void geq_process_event();
};

}

#endif // MC_SCHED_H_
