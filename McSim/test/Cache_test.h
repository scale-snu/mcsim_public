#ifndef CACHE_TEST_H_
#define CACHE_TEST_H_

#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include <vector>

#include "../McSim.h"
#include "../PTS.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"
#include "../PTSDirectory.h"
#include "../PTSXbar.h"

namespace PinPthread {

class CacheTest : public ::testing::Test {

  protected:
    static PthreadTimingSimulator* test_pts;
    static O3Core* test_o3core;
    static CacheL1* test_l1i;
    static CacheL1* test_l1d;
    static CacheL2* test_l2;
    static std::vector<LocalQueueElement *> events;
    static const uint64_t TEST_ADDR_I = 0x401640;
    static const uint64_t TEST_ADDR_D = 0x7FFFFFFFE6D8;

    static void SetUpTestSuite() {
      test_pts = new PthreadTimingSimulator("../Apps/md/test-coherence.toml");
      test_o3core = test_pts->mcsim->o3cores[0];
      test_l1i = test_pts->mcsim->l1is[0];
      test_l1d = test_pts->mcsim->l1ds[0];
      test_l2 = test_pts->mcsim->l2s[0];
    }

    static void TearDownTestSuite() {
      delete test_pts;
    }

    void clear_geq() { test_o3core->geq->event_queue.clear(); }
};

}

#endif // Cache_TEST_H
