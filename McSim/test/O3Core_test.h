// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef O3CORE_TEST_H_
#define O3CORE_TEST_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"

#include <vector>

namespace PinPthread {
namespace O3CoreTest {

class O3CoreForTest : public O3Core {
 public:
  explicit O3CoreForTest(component_type type_, uint32_t num_, McSim * mcsim_):
    O3Core(type_, num_, mcsim_) { }
  ~O3CoreForTest() { }

  O3Queue * get_o3queue() { return o3queue; };
  O3ROB * get_o3rob() { return o3rob; };

  uint32_t get_o3queue_head() { return o3queue_head; }
  uint32_t get_o3queue_size() { return o3queue_size; }
  void set_o3queue_head(uint32_t head) { o3queue_head = head; }
  void set_o3queue_size(uint32_t size) { o3queue_size = size; }

  uint32_t get_o3rob_head() { return o3rob_head; }
  uint32_t get_o3rob_size() { return o3rob_size; }
  void set_o3rob_head(uint32_t head) { o3rob_head = head; }
  void set_o3rob_size(uint32_t size) { o3rob_size = size; }
  
  uint64_t get_num_nacks() { return num_nacks; }
  uint64_t get_num_consecutive_nacks() { return num_consecutive_nacks; }

  uint32_t get_sse_t() { return sse_t; }
  uint32_t get_branch_miss_penalty() { return branch_miss_penalty; }

  uint64_t get_total_mem_rd_time() { return total_mem_rd_time; }
  uint64_t get_total_mem_wr_time() { return total_mem_wr_time; }
};

class O3CoreTest : public ::testing::Test {
  protected:
    static std::unique_ptr<PthreadTimingSimulator> test_pts;
    static O3CoreForTest* test_o3core;
    static TLBL1* test_tlbl1i;
    static TLBL1* test_tlbl1d;
    static CacheL1* test_cachel1i;
    static CacheL1* test_cachel1d;
    static const uint64_t TEST_ADDR_I = 0x401640;
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8;
    static std::vector<LocalQueueElement *> request_events;
    static std::vector<LocalQueueElement *> reply_events;

    static void SetUpTestSuite() {
      test_pts = std::make_unique<PthreadTimingSimulator>("../Apps/md/test/test-md.toml");
      test_o3core = new O3CoreForTest(ct_o3core, 0, test_pts->mcsim);
      test_tlbl1i = test_pts->mcsim->tlbl1is[0];
      test_tlbl1d = test_pts->mcsim->tlbl1ds[0];
      test_cachel1i = test_pts->mcsim->l1is[0];
      test_cachel1d = test_pts->mcsim->l1ds[0];

      auto temp_core = test_pts->mcsim->o3cores[0];
      test_pts->mcsim->o3cores[0] = test_o3core;

      test_pts->mcsim->connect_comps();
      delete temp_core;
    }

    virtual void TearDown() override {
      clear_geq();
      O3Queue * test_o3queue = test_o3core->get_o3queue();
      for (unsigned int i = 0; i < test_o3core->o3queue_max_size; i++) {
        test_o3queue[i].state = o3iqs_invalid;
      }
      test_o3core->set_o3queue_head(0);
      test_o3core->set_o3queue_size(0);
      O3ROB * test_o3rob = test_o3core->get_o3rob();
      for (unsigned int i = 0; i < test_o3core->o3rob_max_size; i++) {
        test_o3rob[i].state = o3irs_invalid;
      }
      test_o3core->set_o3rob_head(0);
      test_o3core->set_o3rob_size(0);
    }
    
    void clear_geq() { test_o3core->geq->event_queue.clear(); }
};

}
}

#endif // O3CORE_TEST_H_
