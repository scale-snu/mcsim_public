#ifndef CACHE_TEST_H_
#define CACHE_TEST_H_

#include "gtest/gtest.h"
#include <vector>

#include "../McSim.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"
#include "../PTSDirectory.h"
#include "../PTSXbar.h"

namespace PinPthread {


class CacheL1ForTest : public CacheL1 {
 public:
  explicit CacheL1ForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    CacheL1(type_, num_, mcsim_) { }
  ~CacheL1ForTest() { }
  uint64_t get_num_rd_access() { return num_rd_access; }
  uint64_t get_num_rd_miss() { return num_rd_miss; }
  uint64_t get_num_wr_access() { return num_wr_access; }
  uint64_t get_num_wr_miss() { return num_wr_miss; }
  uint64_t get_num_ev_coherency() { return num_ev_coherency; }
  uint64_t get_num_ev_capacity() { return num_ev_capacity; }
  uint64_t get_num_coherency_access() { return num_coherency_access; }
};

class CacheL2ForTest : public CacheL2 {
 public:
  explicit CacheL2ForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    CacheL2(type_, num_, mcsim_) { }
  ~CacheL2ForTest() { }
  uint64_t get_num_rd_access() { return num_rd_access; }
  uint64_t get_num_rd_miss() { return num_rd_miss; }
};

class CacheTest : public ::testing::Test {

  protected:
    static std::unique_ptr<PinPthread::PthreadTimingSimulator> test_pts;
    static O3Core* test_o3core;
    static CacheL1ForTest* test_l1i;
    static CacheL1ForTest* test_l1d;
    static CacheL2ForTest* test_l2;
    static std::vector<LocalQueueElement *> events;
    static const uint64_t TEST_ADDR_I = 0x401640;
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8;

    static void SetUpTestSuite() {
      test_pts = std::make_unique<PinPthread::PthreadTimingSimulator>("../Apps/md/test/test-coherence.toml");

      test_o3core = test_pts->mcsim->o3cores[0];
      test_l1i = new CacheL1ForTest(ct_cachel1i, 0, test_pts->mcsim);
      test_l1d = new CacheL1ForTest(ct_cachel1d, 0, test_pts->mcsim);
      test_l2 = new CacheL2ForTest(ct_cachel2, 0, test_pts->mcsim);

      auto temp_cachel1i = test_pts->mcsim->l1is[0];
      auto temp_cachel1d = test_pts->mcsim->l1ds[0];
      auto temp_cachel2 = test_pts->mcsim->l2s[0];

      test_pts->mcsim->l1is[0] = test_l1i;
      test_pts->mcsim->l1ds[0] = test_l1d;
      test_pts->mcsim->l2s[0] = test_l2;

      test_pts->mcsim->connect_comps();
      delete temp_cachel1i;
      delete temp_cachel1d;
      delete temp_cachel2;
    }

    void clear_geq() { test_o3core->geq->event_queue.clear(); }
};

}

#endif // Cache_TEST_H
