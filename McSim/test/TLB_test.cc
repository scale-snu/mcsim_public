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
  EXPECT_TRUE(test_tlbl1i->LRU.empty());
  EXPECT_TRUE(test_tlbl1i->entries.empty());

  EXPECT_TRUE(test_tlbl1d->req_event.empty());
  EXPECT_TRUE(test_tlbl1d->LRU.empty());
  EXPECT_TRUE(test_tlbl1d->entries.empty());

  clear_geq();
  EXPECT_TRUE(test_tlbl1i->geq->event_queue.empty());
}

/* 2-1. ITLB */
TEST_F(TLBTest, ITLBReqEvent) {
  // insert the same local event at different times
  events.push_back(create_tlb_read_event(TEST_ADDR_I, test_o3core));
  test_tlbl1i->add_req_event(10, events.back());
  test_tlbl1i->add_req_event(20, events.back());

  EXPECT_EQ((uint64_t)2, test_tlbl1i->geq->event_queue.size());
  EXPECT_EQ((uint64_t)2, test_tlbl1i->req_event.size());
}

TEST_F(TLBTest, ITLBProcessEvent) {
  test_tlbl1i->geq->process_event_isolateTEST(ct_tlbl1i);

  // the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((uint64_t)1, test_tlbl1i->num_miss);
  EXPECT_EQ((uint64_t)2, test_tlbl1i->num_access);
  EXPECT_EQ((uint64_t)1, test_tlbl1i->entries.size());
  EXPECT_EQ((uint64_t)1, test_tlbl1i->LRU.size());

  // check whether the LRU entry is updated properly
  EXPECT_EQ((uint64_t)20, test_tlbl1i->LRU.begin()->first);

  // make TLB full
  uint64_t address;
  for (uint i = 1; i <= test_tlbl1i->num_entries; i++) {
    address = TEST_ADDR_I + ((1 << test_tlbl1i->page_sz_log2)*i);
    events.push_back(create_tlb_read_event(address, test_o3core));
    test_tlbl1i->add_req_event(20 + i*10, events.back());
  }
  test_tlbl1i->geq->process_event_isolateTEST(ct_tlbl1i);
  
  EXPECT_EQ((uint64_t)65, test_tlbl1i->num_miss);   // 1 + 64
  EXPECT_EQ((uint64_t)66, test_tlbl1i->num_access); // 2 + 64
  EXPECT_EQ(test_tlbl1i->num_entries, test_tlbl1i->entries.size()); // TLB full
  EXPECT_EQ(test_tlbl1i->num_entries, test_tlbl1i->LRU.size());

  // one entry (initially accessed one) is evicted, now LRU access time is 30
  EXPECT_EQ((uint64_t)30, test_tlbl1i->LRU.begin()->first);

  events.clear();
  clear_geq();
}

/* 2-2. DTLB */
/* DTLB는 ROB entry를 먼저 설정해줘야 함 */
TEST_F(TLBTest, DTLBReqEvent) {
  set_rob_entry(test_o3core->o3rob[0], TEST_ADDR_I, TEST_ADDR_D);
  events.push_back(create_tlb_read_event(test_o3core->o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;

  test_tlbl1d->add_req_event(10, events.back());
  test_tlbl1d->add_req_event(20, events.back());

  EXPECT_EQ((uint64_t)2, test_tlbl1d->geq->event_queue.size());
  EXPECT_EQ((uint64_t)2, test_tlbl1d->req_event.size());
}

TEST_F(TLBTest, DTLBProcessEvent) {
  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);
  
  // the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((uint64_t)1, test_tlbl1d->num_miss);
  EXPECT_EQ((uint64_t)2, test_tlbl1d->num_access);
  EXPECT_EQ((uint64_t)1, test_tlbl1d->entries.size());
  EXPECT_EQ((uint64_t)1, test_tlbl1d->LRU.size()); 

  // check whether the LRU entry is updated properly
  EXPECT_EQ((uint64_t)20, test_tlbl1d->LRU.begin()->first);

  // make TLB full
  for (uint i = 1; i <= test_tlbl1d->num_entries; i++) {
    uint64_t address = TEST_ADDR_D + ((1 << test_tlbl1d->page_sz_log2)*i);
    uint32_t rob_entry_num = i % test_o3core->o3rob_max_size;

    set_rob_entry(test_o3core->o3rob[rob_entry_num], TEST_ADDR_I, address);
    events.push_back(create_tlb_read_event(test_o3core->o3rob[rob_entry_num].memaddr, test_o3core));
    events.back()->rob_entry = rob_entry_num;

    test_tlbl1d->add_req_event(20 + 10*i, events.back());
  }
  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);

  EXPECT_EQ((uint64_t)65, test_tlbl1d->num_miss);   // 1 + 64
  EXPECT_EQ((uint64_t)66, test_tlbl1d->num_access); // 2 + 64
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->entries.size());
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->LRU.size());
  EXPECT_EQ((uint64_t)30,  test_tlbl1d->LRU.begin()->first);

  clear_geq();

  // create one more TLB miss event
  set_rob_entry(test_o3core->o3rob[0], TEST_ADDR_I, TEST_ADDR_D - (1 << test_tlbl1d->page_sz_log2));
  events.push_back(create_tlb_read_event(test_o3core->o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;

  test_tlbl1d->add_req_event(1000, events.back());

  test_tlbl1d->geq->process_event_isolateTEST(ct_tlbl1d);

  EXPECT_EQ((uint64_t)66, test_tlbl1d->num_miss);   // 65 + 1
  EXPECT_EQ((uint64_t)67, test_tlbl1d->num_access); // 66 + 1
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->entries.size());
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->LRU.size());
  EXPECT_EQ((uint64_t)40, test_tlbl1d->LRU.begin()->first);

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
