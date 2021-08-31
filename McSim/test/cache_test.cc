#include "cache_test.h"
#include "gtest/gtest.h"
#include <vector>
#include <iostream>

namespace PinPthread {

// static variable of CacheTest
std::unique_ptr<PinPthread::PthreadTimingSimulator> CacheTest::test_pts;
O3Core* CacheTest::test_o3core;
CacheL1ForTest* CacheTest::test_l1i;
CacheL1ForTest* CacheTest::test_l1d;
CacheL2ForTest* CacheTest::test_l2;
std::vector<LocalQueueElement *> CacheTest::events;

/* 1. START of Cache Build && Fixture Build Testing */
TEST_F(CacheTest, CheckBuild) {
  ASSERT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator Build ";
  ASSERT_NE(nullptr, test_o3core) << "wrong McSim O3Core Build ";
  ASSERT_NE(nullptr, test_l1i) << "wrong McSim L1ICache Build ";
  ASSERT_NE(nullptr, test_l1d) << "wrong McSim L1DCache Build ";
  ASSERT_NE(nullptr, test_l2) << "wrong McSim L2Cache Build ";

  EXPECT_EQ((uint)0, test_l1i->num_sets & (test_l1i->num_sets - 1)) << "L1i: the number of sets should be 2^N";
  EXPECT_EQ((uint)0, test_l1d->num_sets & (test_l1d->num_sets - 1)) << "L1d: the number of sets should be 2^N";
  EXPECT_EQ((uint)0, test_l2->num_sets  & (test_l2->num_sets - 1 )) << "L2: the number of sets should be 2^N";
}

/* 2. START of Cache Testing */
TEST_F(CacheTest, IsEmptyInitially) {
  EXPECT_TRUE(test_l1i->req_event.empty());
  EXPECT_TRUE(test_l1i->rep_event.empty());
  EXPECT_EQ((uint)0, test_l1i->get_num_rd_access() + test_l1i->get_num_rd_miss()
                   + test_l1i->get_num_wr_access() + test_l1i->get_num_wr_miss());
  EXPECT_EQ((uint)0, test_l1i->get_num_ev_coherency() + test_l1i->get_num_ev_capacity()
                   + test_l1i->get_num_coherency_access());

  EXPECT_TRUE(test_l1d->req_event.empty());
  EXPECT_TRUE(test_l1d->rep_event.empty());
  EXPECT_EQ((uint)0, test_l1d->get_num_rd_access() + test_l1d->get_num_rd_miss()
                   + test_l1d->get_num_wr_access() + test_l1d->get_num_wr_miss());
  EXPECT_EQ((uint)0, test_l1d->get_num_ev_coherency() + test_l1d->get_num_ev_capacity()
                   + test_l1d->get_num_coherency_access());
}

TEST_F(CacheTest, BasicCase) {
  uint64_t test_address = TEST_ADDR_D;
  uint64_t curr_time = 0;
  LocalQueueElement* test_event = new LocalQueueElement(test_o3core, et_read, test_address, test_o3core->num);
  test_l1d->add_req_event(curr_time, test_event);

  EXPECT_EQ((unsigned int)1, test_l1d->req_event.size());
  EXPECT_EQ((unsigned int)0, test_l1d->get_num_rd_miss());
  EXPECT_EQ((unsigned int)0, test_l2->req_event.size());

  test_l1d->process_event(curr_time);

  EXPECT_EQ((unsigned int)0, test_l1d->req_event.size());
  EXPECT_EQ((unsigned int)1, test_l1d->get_num_rd_miss());
  EXPECT_EQ((unsigned int)1, test_l2->req_event.size());

  EXPECT_EQ((unsigned int)0, test_l2->get_num_rd_access());
  EXPECT_EQ((unsigned int)0, test_l2->get_num_rd_miss());

  curr_time += 40;  // l1_to_l2_t == 40
  test_l2->process_event(curr_time);
  
  EXPECT_EQ((unsigned int)1, test_l2->get_num_rd_access());
  EXPECT_EQ((unsigned int)1, test_l2->get_num_rd_miss());

  EXPECT_EQ((unsigned int)1, test_l2->crossbar->num_req);

  curr_time += 40;  // l2_to_xbar_t == 40 
  test_l2->crossbar->process_event(curr_time);
  // 다른 directory에 event 넘어감 (et_read)
  curr_time += 40;  // xbar_to_dir_t == 40
 
  test_l2->geq->process_event_isolateTEST(ct_directory);
  
  delete test_event;
}

}
