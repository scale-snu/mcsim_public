#include "coherence_test.h"
#include "gtest/gtest.h"

#include <vector>
#include <iostream>

namespace PinPthread {

// static variable of CacheTest
std::unique_ptr<PinPthread::PthreadTimingSimulator> CoherenceTest::test_pts;
std::vector<O3CoreForTest *> CoherenceTest::test_cores;
std::vector<CacheL1ForTest *> CoherenceTest::test_l1ds;
std::vector<CacheL2ForTest *> CoherenceTest::test_l2s;
DirectoryForTest* CoherenceTest::test_dir;
uint32_t CoherenceTest::l1_set;
uint64_t CoherenceTest::l1_tag;
uint32_t CoherenceTest::l2_set;
uint64_t CoherenceTest::l2_tag;

/* 1. START of Cache Build && Fixture Build Testing */
TEST_F(CoherenceTest, CheckBuild) {
  ASSERT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator Build ";
  ASSERT_NE(nullptr, test_pts->mcsim) << "wrong McSim Build ";

  ASSERT_NE(nullptr, test_pts->mcsim->o3cores[0]);
  ASSERT_NE(nullptr, test_pts->mcsim->l1ds[0]);
  ASSERT_NE(nullptr, test_pts->mcsim->o3cores[1]);
  ASSERT_NE(nullptr, test_pts->mcsim->l1ds[1]);
  ASSERT_NE(nullptr, test_pts->mcsim->l2s[0]);
  ASSERT_NE(nullptr, test_pts->mcsim->dirs[0]);

  ASSERT_EQ(test_pts->mcsim->l2s[0],  test_pts->mcsim->dirs[0]->cachel2);
  ASSERT_EQ(test_pts->mcsim->dirs[0], test_pts->mcsim->l2s[0]->directory);
  ASSERT_EQ(test_pts->mcsim->l2s[0]->cachel1d[0], test_pts->mcsim->l1ds[0]);
  ASSERT_EQ(test_pts->mcsim->l2s[0]->cachel1d[1], test_pts->mcsim->l1ds[1]);
  ASSERT_EQ(test_pts->mcsim->l1ds[0]->cachel2, test_pts->mcsim->l2s[0]);
  ASSERT_EQ(test_pts->mcsim->l1ds[1]->cachel2, test_pts->mcsim->l2s[0]);

  ASSERT_NE(nullptr, test_pts->mcsim->o3cores[2]);
  ASSERT_NE(nullptr, test_pts->mcsim->l1ds[2]);
  ASSERT_NE(nullptr, test_pts->mcsim->o3cores[3]);
  ASSERT_NE(nullptr, test_pts->mcsim->l1ds[3]);
  ASSERT_NE(nullptr, test_pts->mcsim->l2s[1]);
  ASSERT_NE(nullptr, test_pts->mcsim->dirs[1]);

  ASSERT_EQ(test_pts->mcsim->l2s[1],  test_pts->mcsim->dirs[1]->cachel2);
  ASSERT_EQ(test_pts->mcsim->dirs[1], test_pts->mcsim->l2s[1]->directory);
  ASSERT_EQ(test_pts->mcsim->l2s[1]->cachel1d[0], test_pts->mcsim->l1ds[2]);
  ASSERT_EQ(test_pts->mcsim->l2s[1]->cachel1d[1], test_pts->mcsim->l1ds[3]);
  ASSERT_EQ(test_pts->mcsim->l1ds[2]->cachel2, test_pts->mcsim->l2s[1]);
  ASSERT_EQ(test_pts->mcsim->l1ds[3]->cachel2, test_pts->mcsim->l2s[1]);
}

TEST_F(CoherenceTest, Case1) {
  // $: I to E, Dir: I to E
  l1_set = (TEST_ADDR_D >> test_l1ds[0]->set_lsb) % test_l1ds[0]->get_num_sets();
  l1_tag = (TEST_ADDR_D >> test_l1ds[0]->set_lsb) / test_l1ds[0]->get_num_sets();
  l2_set = (TEST_ADDR_D >> test_l2s[0]->set_lsb) % test_l2s[0]->get_num_sets();
  l2_tag = (TEST_ADDR_D >> test_l2s[0]->set_lsb) / test_l2s[0]->get_num_sets();

  auto l1_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l2_tags_set = test_l2s[0]->get_tags(l2_set);

  EXPECT_EQ(l1_tags_set[0]->second, cs_invalid);
  EXPECT_EQ(l2_tags_set[0]->type_l1l2, cs_invalid);
  EXPECT_EQ(l2_tags_set[0]->type, cs_invalid);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_EQ(it, test_dir->get_dir_end());  // not in directory

  set_rob_entry((test_cores[0]->get_o3rob())[0], TEST_ADDR_D, 0);
  test_cores[0]->set_o3rob_head(0);
  test_cores[0]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(0, test_cores[0]);
  test_pts->mcsim->global_q->process_event();
  // std::cout << test_mcsim->global_q->curr_time << std::endl;  // 1110
  // std::cout << l2_tags_set[0]->last_access_time << std::endl; // 1050

  EXPECT_EQ(l1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l2_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_tags_set[0]->type, cs_exclusive);

  it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_exclusive);
}

TEST_F(CoherenceTest, Case2) {
  // $: I|E to S, Dir: -
  auto l1_0_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l2_tags_set   = test_l2s[0]->get_tags(l2_set);

  EXPECT_EQ(l1_1_tags_set[0]->second, cs_invalid);

  set_rob_entry((test_cores[1]->get_o3rob())[0], TEST_ADDR_D, 1200);
  test_cores[1]->set_o3rob_head(0);
  test_cores[1]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(1200, test_cores[1]);
  test_pts->mcsim->global_q->process_event();

  EXPECT_EQ(l1_0_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_0_tags_set[0]->second, cs_exclusive);
  EXPECT_EQ(l1_1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_1_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l2_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_tags_set[0]->type_l1l2, cs_shared);
  EXPECT_EQ(l2_tags_set[0]->type, cs_exclusive);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_exclusive);
}

TEST_F(CoherenceTest, Case3) {
  // $: I to E, Dir: E to S
  auto l1_0_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l1_2_tags_set = test_l1ds[2]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  EXPECT_EQ(l1_2_tags_set[0]->second, cs_invalid);

  EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_invalid);
  EXPECT_EQ(l2_1_tags_set[0]->type, cs_invalid);

  set_rob_entry((test_cores[2]->get_o3rob())[0], TEST_ADDR_D, 2400);
  test_cores[2]->set_o3rob_head(0);
  test_cores[2]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(2400, test_cores[2]);
  test_pts->mcsim->global_q->process_event();

  EXPECT_EQ(l1_0_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_0_tags_set[0]->second, cs_exclusive);
  EXPECT_EQ(l1_1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_1_tags_set[0]->second, cs_exclusive);
  EXPECT_EQ(l1_2_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_2_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l2_0_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_0_tags_set[0]->type_l1l2, cs_shared);
  EXPECT_EQ(l2_0_tags_set[0]->type, cs_shared);

  EXPECT_EQ(l2_1_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_1_tags_set[0]->type, cs_shared);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_shared);
}

TEST_F(CoherenceTest, Case4) {
  // $: I to M, S|E to I, Dir: S to M
  auto l1_0_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l1_2_tags_set = test_l1ds[2]->get_tags(l1_set);
  auto l1_3_tags_set = test_l1ds[3]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  EXPECT_EQ(l1_3_tags_set[0]->second, cs_invalid);
  
  set_rob_entry((test_cores[3]->get_o3rob())[0], TEST_ADDR_D, 3600, false); // write event
  test_cores[3]->set_o3rob_head(0);
  test_cores[3]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(3600, test_cores[3]);
  test_pts->mcsim->global_q->process_event();
  if (l1_0_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_0_tags_set[0]->second, cs_invalid);
  }
  if (l1_1_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_1_tags_set[0]->second, cs_invalid);
  }
  if (l1_2_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_2_tags_set[0]->second, cs_invalid);
  }
  EXPECT_EQ(l1_3_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_3_tags_set[0]->second, cs_modified);

  if (l2_0_tags_set[0]->tag == l2_tag) {
    EXPECT_EQ(l2_0_tags_set[0]->type_l1l2, cs_invalid);
    EXPECT_EQ(l2_0_tags_set[0]->type, cs_invalid);
  }
  EXPECT_EQ(l2_1_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_modified);
  EXPECT_EQ(l2_1_tags_set[0]->type, cs_modified);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_modified);
}

TEST_F(CoherenceTest, Case5) {
  // $: I to E, M to E, Dir: M to S
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l1_3_tags_set = test_l1ds[3]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  set_rob_entry((test_cores[1]->get_o3rob())[0], TEST_ADDR_D, 4800);
  test_cores[1]->set_o3rob_head(0);
  test_cores[1]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(4800, test_cores[1]);
  test_pts->mcsim->global_q->process_event();

  EXPECT_EQ(l1_1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_1_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l1_3_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_3_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l2_0_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_0_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_0_tags_set[0]->type, cs_shared);

  EXPECT_EQ(l2_1_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_1_tags_set[0]->type, cs_shared);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_shared);
}

TEST_F(CoherenceTest, Case6) {
  // $: E to I, Dir: S to E
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  auto temp = 1 << test_l2s[0]->set_lsb;
  temp *= test_l2s[0]->get_num_sets();
  uint64_t const test_address2 = TEST_ADDR_D + temp; // same index, different tag

  set_rob_entry((test_cores[1]->get_o3rob())[0], test_address2, 6000);
  test_cores[1]->set_o3rob_head(0);
  test_cores[1]->set_o3rob_size(1);
  
  test_pts->mcsim->global_q->add_event(6000, test_cores[1]);
  test_pts->mcsim->global_q->process_event();

  EXPECT_NE(l1_1_tags_set[0]->first, l1_tag);  // evicted
  EXPECT_NE(l2_0_tags_set[0]->tag, l2_tag);    // evicted

  EXPECT_EQ(l2_1_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_1_tags_set[0]->type, cs_shared);

  auto it = test_dir->search_dir(TEST_ADDR_D >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_shared);

/*
  auto test_l1_3 = test_mcsim->l1ds[3];
  auto l1_3_tags_set = test_l1_3->tags[l1_set];

  set_rob_entry(test_mcsim->o3cores[1]->o3rob[0], test_address, 6000, false);
  test_mcsim->o3cores[1]->o3rob_head = 0;
  test_mcsim->o3cores[1]->o3rob_size = 1;

  test_mcsim->global_q->add_event(6000, test_mcsim->o3cores[1]);
  test_mcsim->global_q->process_event();

  EXPECT_EQ(l1_1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_1_tags_set[0]->second, cs_modified);

  if (l1_3_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_3_tags_set[0]->second, cs_invalid);
  }

  EXPECT_EQ(l2_0_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_0_tags_set[0]->type_l1l2, cs_modified);
  EXPECT_EQ(l2_0_tags_set[0]->type, cs_modified);

  if (l2_1_tags_set[0]->tag == l2_tag) {
    EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_invalid);
    EXPECT_EQ(l2_1_tags_set[0]->type, cs_invalid);
  }

  auto it = test_dir->dir.find(test_address >> test_dir->set_lsb);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_modified);
*/
}

std::map<uint64_t, PinPthread::Directory::DirEntry>::iterator DirectoryForTest::search_dir(uint64_t addr) {
  return dir.find(addr);
}
std::map<uint64_t, PinPthread::Directory::DirEntry>::iterator DirectoryForTest::get_dir_end() {
  return dir.end();
}


void CoherenceTest::set_rob_entry(O3ROB & o3rob_entry, uint64_t _memaddr, uint64_t ready_time, bool isread) {
  o3rob_entry.memaddr = _memaddr;
  o3rob_entry.isread = isread;
  o3rob_entry.ready_time = ready_time;

  o3rob_entry.state = o3irs_issued;
  o3rob_entry.branch_miss = false;
  o3rob_entry.mem_dep = -1;
  o3rob_entry.instr_dep = -1;
  o3rob_entry.branch_dep = -1;
  o3rob_entry.type = mem_rd;
  o3rob_entry.rr0 = -1; o3rob_entry.rr1 = -1;
  o3rob_entry.rr2 = -1; o3rob_entry.rr3 = -1;
  o3rob_entry.rw0 = -1; o3rob_entry.rw1 = -1;
  o3rob_entry.rw2 = -1; o3rob_entry.rw3 = -1;
}

}
