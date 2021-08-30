#include "MC_scheduler_test.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace PinPthread {

extern std::ostream & operator << (std::ostream & output, mc_scheduling_policy policy);
extern std::ostream & operator << (std::ostream & output, mc_bank_action action);

// static variable of MCSchedTest
std::shared_ptr<PinPthread::PthreadTimingSimulator> MCSchedTest::test_pts;
MemoryControllerForTest* MCSchedTest::test_mc;
std::vector<uint64_t> MCSchedTest::row_A_addresses;
std::vector<uint64_t> MCSchedTest::row_B_addresses;

/* 1. START of MC Build && Fixture Build Testing */
TEST_F(MCSchedTest, CheckBuild) {
  EXPECT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator Build ";
  EXPECT_NE(nullptr, test_mc) << "wrong McSim Build ";
  EXPECT_EQ(true, test_mc->get_parbs());
  EXPECT_EQ(mc_scheduling_open, test_mc->get_policy());
}
/* END of 1. */

/* 2. START of MC Testing */
// 2.1) MC add requests to the [rank][bank], [0][0], but different rows
TEST_F(MCSchedTest, MCRequests) {
  //
  EXPECT_EQ(true,test_mc->req_l.empty());
  // 
  PinPthread::LocalQueueElement * event_A = new PinPthread::LocalQueueElement();
  PinPthread::LocalQueueElement * event_B = new PinPthread::LocalQueueElement();
  event_A->type = PinPthread::event_type::et_read;
  event_B->type = PinPthread::event_type::et_read;

  event_A->address = row_A_addresses[0];
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_A->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_A->address));
  EXPECT_EQ((uint64_t)0x10, test_mc->page_num(event_A->address)); 

  event_B->address = row_B_addresses[0];
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_B->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_B->address));
  EXPECT_EQ((uint64_t)0x20, test_mc->page_num(event_B->address)); 

  //
  /* clearing geq is necessary, because other module's constructor may have pushed to geq. */
  clear_geq();
  test_mc->add_req_event(0, event_A, NULL);
  test_mc->add_req_event(0, event_B, NULL);

  delete event_A;
  delete event_B;
}

// 2.2) Process two requests
TEST_F(MCSchedTest, MCProcessEvent) {
  // uint64_t end_time = test_mc->geq->process_event_isolateTEST(ct_memory_controller);
  test_mc->geq->process_event_isolateTEST(ct_memory_controller);
  
  // whole MC data
  EXPECT_EQ((uint64_t)2, test_mc->get_num_read());
  EXPECT_EQ((uint64_t)0, test_mc->get_num_write());
  EXPECT_EQ((uint64_t)2, test_mc->get_num_activate());
  EXPECT_EQ((uint64_t)1, test_mc->get_num_precharge()); // since it is assumed to be "open" policy

  // single bank[0][0] data
  EXPECT_EQ((uint64_t)0x20, test_mc->get_bank_status(0, 0).page_num);
  EXPECT_EQ(mc_bank_read, test_mc->get_bank_status(0, 0).action_type);
}

// 2.3) Verify open-page policy (FR-FCFS)
TEST_F(MCSchedTest, MCSchedulingOpen) {
  PinPthread::LocalQueueElement * event_A_1 = create_read_event(row_A_addresses[0]);
  PinPthread::LocalQueueElement * event_A_2 = create_read_event(row_A_addresses[1]);
  PinPthread::LocalQueueElement * event_A_3 = create_read_event(row_A_addresses[2]);
  EXPECT_EQ((uint64_t)0x10, test_mc->page_num(event_A_1->address));
  EXPECT_EQ((uint64_t)0x10, test_mc->page_num(event_A_2->address));
  EXPECT_EQ((uint64_t)0x10, test_mc->page_num(event_A_3->address));

  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_A_1->address));
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_A_2->address));
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_A_3->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_A_1->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_A_2->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_A_3->address));

  PinPthread::LocalQueueElement * event_B_1 = create_read_event(row_B_addresses[0]);
  PinPthread::LocalQueueElement * event_B_2 = create_read_event(row_B_addresses[1]);
  PinPthread::LocalQueueElement * event_B_3 = create_read_event(row_B_addresses[2]);
  EXPECT_EQ((uint64_t)0x20, test_mc->page_num(event_B_1->address));
  EXPECT_EQ((uint64_t)0x20, test_mc->page_num(event_B_2->address));
  EXPECT_EQ((uint64_t)0x20, test_mc->page_num(event_B_3->address));

  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_B_1->address));
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_B_2->address));
  EXPECT_EQ((uint32_t)0, test_mc->rank_num(event_B_3->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_B_1->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_B_2->address));
  EXPECT_EQ((uint32_t)0, test_mc->bank_num(event_B_3->address));

  clear_geq();
  test_mc->add_req_event(0, event_A_1, NULL);
  test_mc->add_req_event(0, event_B_1, NULL);
  test_mc->add_req_event(0, event_A_2, NULL);
  test_mc->add_req_event(0, event_B_2, NULL);
  test_mc->add_req_event(0, event_A_3, NULL);
  test_mc->add_req_event(0, event_B_3, NULL);

  test_mc->geq->process_event_isolateTEST(ct_memory_controller);
  //
  EXPECT_EQ((uint64_t)8, test_mc->get_num_read()); // +6
  EXPECT_EQ((uint64_t)0, test_mc->get_num_write());
  EXPECT_EQ((uint64_t)3, test_mc->get_num_activate());  // +1, at transition
  EXPECT_EQ((uint64_t)2, test_mc->get_num_precharge()); // +1, at transition
  //
  EXPECT_EQ(mc_bank_read, test_mc->get_bank_status(0, 0).action_type);

  delete event_A_1;
  delete event_A_2;
  delete event_A_3;
  delete event_B_1;
  delete event_B_2;
  delete event_B_3;
}

PinPthread::LocalQueueElement * MCSchedTest::create_read_event(uint64_t _address) {
  PinPthread::LocalQueueElement * event_return = new PinPthread::LocalQueueElement();
  event_return->type = PinPthread::event_type::et_read;
  event_return->address = _address;
  return event_return;
}

}
