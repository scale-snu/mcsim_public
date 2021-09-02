#ifndef COHERENCE_TEST_H_
#define COHERENCE_TEST_H_

#include "gtest/gtest.h"

#include "../McSim.h"
#include "../PTSProcessDescription.h"

#include "../PTSTLB.h"
#include "../PTSO3Core.h"
#include "../PTSCache.h"
#include "../PTSDirectory.h"
#include "../PTSXbar.h"

#include <vector>

namespace PinPthread {
namespace CoherenceTest {

class O3CoreForTest : public O3Core {
 public:
  explicit O3CoreForTest(component_type type_, UINT32 num_, McSim * mcsim_):
    O3Core(type_, num_, mcsim_) { }
  ~O3CoreForTest() { }
  O3ROB * get_o3rob() { return o3rob; };
  void set_o3rob_head(UINT32 head) { o3rob_head = head; }
  void set_o3rob_size(UINT32 size) { o3rob_size = size; }
};

class CacheL1ForTest : public CacheL1 {
 public:
  explicit CacheL1ForTest(component_type type_, UINT32 num_, McSim * mcsim_):
    CacheL1(type_, num_, mcsim_) { }
  ~CacheL1ForTest() { }
  l1_tag_pair** get_tags(UINT32 set) { return tags[set]; }
};

class CacheL2ForTest : public CacheL2 {
 public:
  explicit CacheL2ForTest(component_type type_, UINT32 num_, McSim * mcsim_):
    CacheL2(type_, num_, mcsim_), cs_type(), cs_type_l1l2(), address(0, 0) { }
  ~CacheL2ForTest() { }
  UINT32 process_event(UINT64 curr_time) override;
  void set_address(UINT64);
  std::vector<coherence_state_type> cs_type;
  std::vector<coherence_state_type> cs_type_l1l2;
  L2Entry** get_tags(UINT32 set) { return tags[set]; }
 private:
  std::pair<UINT32, UINT32> address;           // <set, tag>
};

class DirectoryForTest : public Directory {
 public:
  explicit DirectoryForTest(component_type type_, UINT32 num_, McSim * mcsim_):
    Directory(type_, num_, mcsim_), cs_type(), address(0) { }
  ~DirectoryForTest() { }
  UINT32 process_event(UINT64 curr_time) override;
  void set_address(UINT64);
  std::vector<coherence_state_type> cs_type;
  std::map<UINT64, PinPthread::Directory::DirEntry>::iterator search_dir(UINT64);
  std::map<UINT64, PinPthread::Directory::DirEntry>::iterator get_dir_end();
 private:
  UINT64 address;
};

class CoherenceTest : public ::testing::Test {
  protected:
    static std::unique_ptr<PinPthread::PthreadTimingSimulator> test_pts;
    static std::vector<O3CoreForTest *> test_cores;
    static std::vector<CacheL1ForTest *> test_l1ds;
    static std::vector<CacheL2ForTest *> test_l2s;
    static DirectoryForTest* test_dir;
    static UINT32 l1_set;
    static UINT64 l1_tag;
    static UINT32 l2_set;
    static UINT64 l2_tag;
    // 26C8(hex) = 9928(dec) = 10 011011 001000(bin)
    static const UINT64 TEST_ADDR_D = 0x26C8;

    static void SetUpTestSuite() {
      test_pts = std::make_unique<PinPthread::PthreadTimingSimulator>("../Apps/md/test/test-coherence.toml");

      // fake cores & l1s
      for (UINT32 i = 0; i < test_pts->mcsim->get_num_hthreads(); i++) {
        test_cores.push_back(new O3CoreForTest(ct_o3core, i, test_pts->mcsim));
        test_l1ds.push_back(new CacheL1ForTest(ct_cachel1d, i, test_pts->mcsim));
        auto temp_core = test_pts->mcsim->o3cores[i];
        auto temp_l1d = test_pts->mcsim->l1ds[i];
        test_pts->mcsim->o3cores[i] = test_cores.back();
        test_pts->mcsim->l1ds[i] = test_l1ds.back();
        delete temp_core;
        delete temp_l1d;
      }

      // fake l2s & directory
      test_l2s.push_back(new CacheL2ForTest(ct_cachel2, 0, test_pts->mcsim));
      test_l2s.push_back(new CacheL2ForTest(ct_cachel2, 1, test_pts->mcsim));
      test_dir = new DirectoryForTest(ct_directory, 0, test_pts->mcsim);
      auto temp_l2_0 = test_pts->mcsim->l2s[0];
      auto temp_l2_1 = test_pts->mcsim->l2s[1];
      auto temp_dir = test_pts->mcsim->dirs[0];
      test_pts->mcsim->l2s[0] = test_l2s[0];
      test_pts->mcsim->l2s[1] = test_l2s[1];
      test_pts->mcsim->dirs[0] = test_dir;
      delete temp_l2_0;
      delete temp_l2_1;
      delete temp_dir;

      test_pts->mcsim->connect_comps();
    }

    virtual void TearDown() override {
      clear_geq();
      for (auto && l2 : test_l2s) {
        l2->cs_type.clear();
        l2->cs_type_l1l2.clear();
      }
      test_dir->cs_type.clear();
    }

    void clear_geq() { test_pts->mcsim->global_q->event_queue.clear(); }
    void set_rob_entry(O3ROB & o3rob_entry, UINT64 _memaddr, UINT64 ready_time, bool isread = true);
};

}
}

#endif // COHERENCE_TEST_H
