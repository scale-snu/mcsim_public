#ifndef TLB_TEST_H_
#define TLB_TEST_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"

#include <vector>

namespace PinPthread {

class O3CoreForTest : public O3Core {
 public:
  explicit O3CoreForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    O3Core(type_, num_, mcsim_) { }
  ~O3CoreForTest() { }
  O3ROB * get_o3rob() { return o3rob; };
  uint32_t get_o3rob_max_size() { return o3rob_max_size; }
};

class TLBL1ForTest : public TLBL1 {
 public:
  explicit TLBL1ForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    TLBL1(type_, num_, mcsim_) { }
  ~TLBL1ForTest() { }
  uint64_t get_num_access() { return num_access; }
  uint64_t get_num_miss()   { return num_miss; }
  uint64_t get_size_of_LRU()   { return entries.size(); }
  uint64_t get_size_of_entries()   { return LRU.size(); }
  uint64_t get_LRU_time()   { return LRU.begin()->first; }
};

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
      test_pts = std::make_unique<PthreadTimingSimulator>("../Apps/md/test/test-md.toml");
      test_o3core = new O3CoreForTest(ct_o3core, 0, test_pts->mcsim);
      test_tlbl1i = new TLBL1ForTest(ct_tlbl1i, 0, test_pts->mcsim);
      test_tlbl1d = new TLBL1ForTest(ct_tlbl1d, 0, test_pts->mcsim);

      auto temp_core = test_pts->mcsim->o3cores[0];
      auto temp_tlbl1i = test_pts->mcsim->tlbl1is[0];
      auto temp_tlbl1d = test_pts->mcsim->tlbl1ds[0];

      test_pts->mcsim->o3cores[0] = test_o3core;
      test_pts->mcsim->tlbl1is[0] = test_tlbl1i;
      test_pts->mcsim->tlbl1ds[0] = test_tlbl1d;
      test_o3rob = test_o3core->get_o3rob();

      test_pts->mcsim->connect_comps();
      delete temp_core;
      delete temp_tlbl1i;
      delete temp_tlbl1d;
    }
    void clear_geq() { test_tlbl1i->geq->event_queue.clear(); }
    LocalQueueElement * create_tlb_read_event(uint64_t _address, Component * from); 
    void set_rob_entry(O3ROB & o3rob_entry, uint64_t _ip, uint64_t _memaddr);
};

}

#endif // TLB_TEST_H
