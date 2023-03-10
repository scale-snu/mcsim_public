// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "coherence_test.h"
#include "gtest/gtest.h"

#include <vector>
#include <iostream>

namespace PinPthread {
extern std::ostream & operator << (std::ostream & output, component_type ct);
extern std::ostream & operator << (std::ostream & output, coherence_state_type cs);

namespace CoherenceTest {

// static variable of CacheTest
std::unique_ptr<PinPthread::PthreadTimingSimulator> CoherenceTest::test_pts;
std::vector<O3CoreForTest *> CoherenceTest::test_cores;
std::vector<CacheL1ForTest *> CoherenceTest::test_l1ds;
std::vector<CacheL2ForTest *> CoherenceTest::test_l2s;
DirectoryForTest* CoherenceTest::test_dir;
UINT32 CoherenceTest::l1_set;
UINT64 CoherenceTest::l1_tag;
UINT32 CoherenceTest::l2_set;
UINT64 CoherenceTest::l2_tag;

TEST_F(CoherenceTest, CheckBuild) {
  ASSERT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator Build ";
  ASSERT_NE(nullptr, test_pts->mcsim) << "wrong McSim Build ";

  for (auto && core : test_cores) ASSERT_NE(nullptr, core);
  for (auto && l1d : test_l1ds)   ASSERT_NE(nullptr, l1d);
  for (auto && l2 : test_l2s)     ASSERT_NE(nullptr, l2);
  ASSERT_NE(nullptr, test_dir);

  ASSERT_EQ(test_l2s[0]->cachel1d[0], test_l1ds[0]);
  ASSERT_EQ(test_l2s[0]->cachel1d[1], test_l1ds[1]);
  ASSERT_EQ(test_l1ds[0]->cachel2, test_l2s[0]);
  ASSERT_EQ(test_l1ds[1]->cachel2, test_l2s[0]);
  ASSERT_EQ(test_l2s[0],  test_dir->cachel2);
  ASSERT_EQ(test_dir, test_l2s[0]->directory);

  ASSERT_EQ(test_l2s[1]->cachel1d[0], test_l1ds[2]);
  ASSERT_EQ(test_l2s[1]->cachel1d[1], test_l1ds[3]);
  ASSERT_EQ(test_l1ds[2]->cachel2, test_l2s[1]);
  ASSERT_EQ(test_l1ds[3]->cachel2, test_l2s[1]);
}

TEST_F(CoherenceTest, Case1) {
  // $: I to E, Dir: I to E
  l1_set = (TEST_ADDR_D >> test_l1ds[0]->set_lsb) % test_l1ds[0]->num_sets;
  l1_tag = (TEST_ADDR_D >> test_l1ds[0]->set_lsb) / test_l1ds[0]->num_sets;
  l2_set = (TEST_ADDR_D >> test_l2s[0]->set_lsb) % test_l2s[0]->num_sets;
  l2_tag = (TEST_ADDR_D >> test_l2s[0]->set_lsb) / test_l2s[0]->num_sets;

  // save the address that you want to check the coherence state
  for (auto && l2 : test_l2s) {
    l2->set_address(TEST_ADDR_D);
  }
  test_dir->set_address(TEST_ADDR_D);

  auto l1_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l2_tags_set = test_l2s[0]->get_tags(l2_set);

  if (l1_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_tags_set[0]->second, cs_invalid);
  }
  if (l2_tags_set[0]->tag == l2_tag) {
    EXPECT_EQ(l2_tags_set[0]->type, cs_invalid);
    EXPECT_EQ(l2_tags_set[0]->type_l1l2, cs_invalid);
  }

  auto it = test_dir->search_dir(TEST_ADDR_D);
  EXPECT_EQ(it, test_dir->get_dir_end());  // not in directory

  
  set_rob_entry((test_cores[0]->get_o3rob())[0], TEST_ADDR_D, 0);
  test_cores[0]->set_o3rob_head(0);
  test_cores[0]->set_o3rob_size(1);
  test_pts->mcsim->global_q->add_event(0, test_cores[0]);
  test_pts->mcsim->global_q->process_event();


  EXPECT_EQ(l1_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l1_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(l2_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_tags_set[0]->type_l1l2, cs_exclusive);
  EXPECT_EQ(l2_tags_set[0]->type, cs_exclusive);

  it = test_dir->search_dir(TEST_ADDR_D);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_exclusive);

  // check transient state
  EXPECT_EQ(test_dir->cs_type[0], cs_tr_to_e);
  EXPECT_EQ(test_dir->cs_type[1], cs_exclusive);  // (I) -> tr_to_e -> E
}

TEST_F(CoherenceTest, Case2) {
  // $: I to E, E to S, Dir: -
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

  auto it = test_dir->search_dir(TEST_ADDR_D);
  EXPECT_NE(it, test_dir->get_dir_end());
  EXPECT_EQ(it->second.type, cs_exclusive);
}

TEST_F(CoherenceTest, Case3) {
  // $: I to E|S, E to S, Dir: E to S
  auto l1_0_tags_set = test_l1ds[0]->get_tags(l1_set);
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l1_2_tags_set = test_l1ds[2]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  if (l1_2_tags_set[0]->first == l1_tag) {
    EXPECT_EQ(l1_2_tags_set[0]->second, cs_invalid);
  }
  if (l2_1_tags_set[0]->tag == l2_tag) {
    EXPECT_EQ(l2_1_tags_set[0]->type, cs_invalid);
    EXPECT_EQ(l2_1_tags_set[0]->type_l1l2, cs_invalid);
  }

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

  EXPECT_EQ(test_dir->cs_type[0], cs_tr_to_s);
  EXPECT_EQ(test_dir->cs_type[1], cs_shared);    // (E) -> tr_to_s -> S
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

  EXPECT_EQ(test_dir->cs_type[0], cs_tr_to_m);
  EXPECT_EQ(test_dir->cs_type[1], cs_modified);    // (S) -> tr_to_m -> M
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
  EXPECT_EQ(l1_3_tags_set[0]->first, l1_tag);
  EXPECT_EQ(l2_0_tags_set[0]->tag, l2_tag);
  EXPECT_EQ(l2_1_tags_set[0]->tag, l2_tag);

  EXPECT_EQ(l1_1_tags_set[0]->second, cs_exclusive);
  EXPECT_EQ(l1_3_tags_set[0]->second, cs_exclusive);

  EXPECT_EQ(test_l2s[0]->cs_type[0], cs_invalid);
  EXPECT_EQ(test_l2s[0]->cs_type[1], cs_shared);          // (I) -> I -> S
  EXPECT_EQ(test_l2s[0]->cs_type_l1l2[0], cs_invalid);
  EXPECT_EQ(test_l2s[0]->cs_type_l1l2[1], cs_exclusive);  // (I) -> I -> E

  EXPECT_EQ(test_l2s[1]->cs_type[0], cs_tr_to_s);
  EXPECT_EQ(test_l2s[1]->cs_type[1], cs_shared);          // (M) -> tr_to_s -> S
  EXPECT_EQ(test_l2s[1]->cs_type_l1l2[0], cs_exclusive);  // (M) -> E

  EXPECT_EQ(test_dir->cs_type[0], cs_m_to_s);
  EXPECT_EQ(test_dir->cs_type[1], cs_shared);             // (M) -> m_to_s -> S
}

TEST_F(CoherenceTest, Case6) {
  // $: E to I, Dir: S to E
  auto l1_1_tags_set = test_l1ds[1]->get_tags(l1_set);
  auto l2_0_tags_set = test_l2s[0]->get_tags(l2_set);
  auto l2_1_tags_set = test_l2s[1]->get_tags(l2_set);

  auto temp = 1 << test_l2s[0]->set_lsb;
  temp *= test_l2s[0]->num_sets;
  UINT64 const test_address2 = TEST_ADDR_D + temp;  // same index, different tag
  test_dir->set_address(test_address2);

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

  EXPECT_EQ(test_dir->cs_type[0], cs_tr_to_e);
  EXPECT_EQ(test_dir->cs_type[1], cs_exclusive);  // (I) -> tr_to_e -> E

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

UINT32 CacheL2ForTest::process_event(UINT64 curr_time) {
  auto res = CacheL2::process_event(curr_time);

  for (uint i = 0; i < num_ways; ++i) {
    if (tags[address.first][i]->tag == address.second) {
      if (cs_type.empty() ||
          cs_type.back() != tags[address.first][i]->type) {
        cs_type.push_back(tags[address.first][i]->type);
      }
      if (cs_type_l1l2.empty() ||
          cs_type_l1l2.back() != tags[address.first][i]->type_l1l2) {
        cs_type_l1l2.push_back(tags[address.first][i]->type_l1l2);
      }
      break;
    } else if (i == num_ways - 1) {  // not in L2 cache
      cs_type.push_back(cs_invalid);
      cs_type_l1l2.push_back(cs_invalid);
    }
  }
  return res;
}

void CacheL2ForTest::set_address(UINT64 address_) {
  UINT32 set = (address_ >> set_lsb) % num_sets;
  UINT32 tag = (address_ >> set_lsb) / num_sets;
  address = std::pair<UINT32, UINT32>(set, tag);
  cs_type.clear();
  cs_type_l1l2.clear();
}

UINT32 DirectoryForTest::process_event(UINT64 curr_time) {
  auto res = Directory::process_event(curr_time);

  if (dir.find(address) != dir.end()) {
    // if there's no recorded coherence state yet,
    // the last recorded coherence state had changed,
    if (cs_type.empty() ||
        cs_type.back() != dir.find(address)->second.type) {  
      cs_type.push_back(dir.find(address)->second.type);  // save the state
    }
  } else {  // not in directory
    cs_type.push_back(cs_invalid);
  }
  return res;
}

void DirectoryForTest::set_address(UINT64 address_) {
  address = address_ >> set_lsb;
  cs_type.clear();
}

std::map<UINT64, PinPthread::Directory::DirEntry>::iterator DirectoryForTest::search_dir(UINT64 addr) {
  return dir.find(addr >> set_lsb);
}
std::map<UINT64, PinPthread::Directory::DirEntry>::iterator DirectoryForTest::get_dir_end() {
  return dir.end();
}

void CoherenceTest::set_rob_entry(O3ROB & o3rob_entry, UINT64 _memaddr, UINT64 ready_time, bool isread) {
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
}
