#include "TLB_test.h"
#include "gtest/gtest.h"

namespace PinPthread {

// static variable of TLBTest
PthreadTimingSimulator* TLBTest::test_pts;
TLBL1* TLBTest::test_tlbl1i;
TLBL1* TLBTest::test_tlbl1d;
O3Core* TLBTest::test_o3core;
std::vector<LocalQueueElement *> TLBTest::events;

/* 1. START of TLB Build && Fixture Build Testing */
TEST_F(TLBTest, CheckBuild) {
  EXPECT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator Build ";
  EXPECT_NE(nullptr, test_o3core) << "wrong McSim O3Core Build ";
  EXPECT_NE(nullptr, test_tlbl1i) << "wrong McSim TLBL1I Build ";
  EXPECT_NE(nullptr, test_tlbl1d) << "wrong McSim TLBL1D Build ";
}

/* 2. START of TLB Testing */
TEST_F(TLBTest, IsEmptyInitially) {
  EXPECT_TRUE(test_tlbl1i->req_event.empty());
  EXPECT_TRUE(test_tlbl1i->rep_event.empty());
  EXPECT_TRUE(test_tlbl1i->LRU.empty());
  EXPECT_TRUE(test_tlbl1i->entries.empty());

  EXPECT_TRUE(test_tlbl1d->req_event.empty());
  EXPECT_TRUE(test_tlbl1d->rep_event.empty());
  EXPECT_TRUE(test_tlbl1d->LRU.empty());
  EXPECT_TRUE(test_tlbl1d->entries.empty());
}

TEST_F(TLBTest, ITLBReqEvent) {
  events.push_back(create_tlb_read_event(0x401640, test_o3core));

  clear_geq();
  EXPECT_TRUE(test_tlbl1i->geq->event_queue.empty());

  // insert the same local event at different times
  test_tlbl1i->add_req_event(10, events.back());
  test_tlbl1i->add_req_event(20, events.back());

  EXPECT_EQ((long unsigned int)2, test_tlbl1i->geq->event_queue.size());
  EXPECT_EQ((long unsigned int)2, test_tlbl1i->req_event.size());
}

TEST_F(TLBTest, ITLBProcessEvent) {
  test_tlbl1i->geq->process_event_isolateTEST(ct_tlbl1i);

  // the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((long unsigned int)1, test_tlbl1i->num_miss);
  EXPECT_EQ((long unsigned int)2, test_tlbl1i->num_access);
  EXPECT_EQ((long unsigned int)1, test_tlbl1i->entries.size());
  EXPECT_EQ((long unsigned int)1, test_tlbl1i->LRU.size());

  // check whether the LRU entry is updated properly
  EXPECT_EQ((long unsigned int)20, test_tlbl1i->LRU.begin()->first);

  uint64_t address;
  for (int i = 1; i < 65; i++) {
    address = 0x547DC0 + ((1 << test_tlbl1i->page_sz_log2)*i);
    events.push_back(create_tlb_read_event(address, test_o3core));
    test_tlbl1i->add_req_event(20 + i*10, events.back());
  }
  test_tlbl1i->geq->process_event_isolateTEST(ct_tlbl1i);
  
  EXPECT_EQ((long unsigned int)66, test_tlbl1i->num_access);
  EXPECT_EQ((long unsigned int)65, test_tlbl1i->num_miss);
  EXPECT_EQ((long unsigned int)64, test_tlbl1i->entries.size());
  EXPECT_EQ((long unsigned int)64, test_tlbl1i->LRU.size());
  EXPECT_LT((long unsigned int)20, test_tlbl1i->LRU.begin()->first);

  events.clear();
  clear_geq();
}

TEST_F(TLBTest, DTLBReqEvent) {
  set_rob_entry(test_o3core->o3rob[0], 0x547DC0, 0x7FFFFFFFE6D8);

  events.push_back(create_tlb_read_event(test_o3core->o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;

  test_tlbl1d->add_req_event(10, events.back());
  test_tlbl1d->add_req_event(20, events.back());

  EXPECT_EQ((long unsigned int)2, test_tlbl1d->geq->event_queue.size());
  EXPECT_EQ((long unsigned int)2, test_tlbl1d->req_event.size());
}

TEST_F(TLBTest, DTLBProcessEvent) {
  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);
  
  EXPECT_EQ((long unsigned int)2, test_tlbl1d->num_access);
  EXPECT_EQ((long unsigned int)1, test_tlbl1d->num_miss); // wjdoh) the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((long unsigned int)1, test_tlbl1d->entries.size());
  EXPECT_EQ((long unsigned int)1, test_tlbl1d->LRU.size()); 
  EXPECT_EQ((long unsigned int)20, test_tlbl1d->LRU.begin()->first); // wjdoh) check whether the LRU entry is updated properly

  clear_geq();
  events.clear();
  test_tlbl1d->num_access = 0;
  for (uint i = 0; i < test_o3core->o3rob_max_size; i++) {
    uint64_t address = 0x7FFFFFFFE6D8 + ((1 << test_tlbl1d->page_sz_log2)*i);
    set_rob_entry(test_o3core->o3rob[i], 0x547DC0, address);
    events.push_back(create_tlb_read_event(test_o3core->o3rob[i].memaddr, test_o3core));
    events.back()->rob_entry = i;
    test_tlbl1d->add_req_event(10*i, events.back());
  }
  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);

  EXPECT_EQ((long unsigned int)64, test_tlbl1d->num_access);
  EXPECT_EQ((long unsigned int)64, test_tlbl1d->num_miss);
  EXPECT_EQ((long unsigned int)64, test_tlbl1d->entries.size());
  EXPECT_EQ((long unsigned int)64, test_tlbl1d->LRU.size());
  EXPECT_EQ(0, (int)test_tlbl1d->LRU.begin()->first);

  clear_geq();
  // wjdoh) create one more TLB miss event
  set_rob_entry(test_o3core->o3rob[0], 0x547DC0, 0x7FFFFFFFE6D8 - (1<<test_tlbl1d->page_sz_log2));
  events.push_back(create_tlb_read_event(test_o3core->o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;
  test_tlbl1d->add_req_event(1000, events.back());
  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);

  EXPECT_EQ((long unsigned int)65, test_tlbl1d->num_access);
  EXPECT_EQ((long unsigned int)65, test_tlbl1d->num_miss);
  EXPECT_EQ((long unsigned int)64, test_tlbl1d->entries.size());
  EXPECT_EQ((long unsigned int)64, test_tlbl1d->LRU.size());
  EXPECT_LT(0, (int)test_tlbl1d->LRU.begin()->first);

}

LocalQueueElement * TLBTest::create_tlb_read_event(uint64_t _address, Component * from) {
  LocalQueueElement * event_return = new LocalQueueElement(from, et_tlb_rd, _address, from->num);
  return event_return;
}

void TLBTest::set_rob_entry(O3ROB & o3rob_entry, uint64_t _ip, uint64_t _memaddr) {
  o3rob_entry.ip = _ip;
  o3rob_entry.memaddr = _memaddr;

  o3rob_entry.state = o3irs_executing;
  o3rob_entry.branch_miss = false;
  o3rob_entry.isread = true;
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
