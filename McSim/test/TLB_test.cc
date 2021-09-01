#include "TLB_test.h"
#include "gtest/gtest.h"

namespace PinPthread {
namespace TLBTest {

// static variable of TLBTest
std::unique_ptr<PthreadTimingSimulator> TLBTest::test_pts;
O3CoreForTest* TLBTest::test_o3core;
TLBL1ForTest* TLBTest::test_tlbl1i;
TLBL1ForTest* TLBTest::test_tlbl1d;
O3ROB* TLBTest::test_o3rob;
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
  EXPECT_EQ((uint)0, test_tlbl1i->get_size_of_LRU());
  EXPECT_EQ((uint)0, test_tlbl1i->get_size_of_entries());

  EXPECT_TRUE(test_tlbl1d->req_event.empty());
  EXPECT_EQ((uint)0, test_tlbl1d->get_size_of_LRU());
  EXPECT_EQ((uint)0, test_tlbl1d->get_size_of_entries());

  clear_geq();
  EXPECT_TRUE(test_tlbl1i->geq->event_queue.empty());
}

/* 2-1. ITLB */
TEST_F(TLBTest, ITLBReqEvent) {
  // insert the same local event at different times
  events.push_back(create_tlb_read_event(TEST_ADDR_I, test_o3core));
  test_tlbl1i->add_req_event(10, events.back());
  events.push_back(create_tlb_read_event(TEST_ADDR_I, test_o3core));
  test_tlbl1i->add_req_event(20, events.back());

  EXPECT_EQ((uint64_t)2, test_tlbl1i->geq->event_queue.size());
  EXPECT_EQ((uint64_t)2, test_tlbl1i->req_event.size());
}

TEST_F(TLBTest, ITLBProcessEvent) {
  test_tlbl1i->process_event(10);
  test_tlbl1i->process_event(20);

  // the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((uint64_t)1, test_tlbl1i->get_num_miss());
  EXPECT_EQ((uint64_t)2, test_tlbl1i->get_num_access());
  EXPECT_EQ((uint64_t)1, test_tlbl1i->get_size_of_entries());
  EXPECT_EQ((uint64_t)1, test_tlbl1i->get_size_of_LRU());

  // check whether the LRU entry is updated properly
  EXPECT_EQ((uint64_t)20, test_tlbl1i->get_LRU_time());

  // make TLB full
  uint64_t address;
  for (uint i = 1; i <= test_tlbl1i->num_entries; i++) {
    address = TEST_ADDR_I + ((1 << test_tlbl1i->page_sz_log2)*i);
    events.push_back(create_tlb_read_event(address, test_o3core));
    test_tlbl1i->add_req_event(20 + i*10, events.back());
    test_tlbl1i->process_event(20 + i*10);
  }
  
  EXPECT_EQ((uint64_t)65, test_tlbl1i->get_num_miss());   // 1 + 64
  EXPECT_EQ((uint64_t)66, test_tlbl1i->get_num_access()); // 2 + 64
  EXPECT_EQ(test_tlbl1i->num_entries, test_tlbl1i->get_size_of_entries()); // TLB full
  EXPECT_EQ(test_tlbl1i->num_entries, test_tlbl1i->get_size_of_LRU());

  // one entry (initially accessed one) is evicted, now LRU access time is 30
  EXPECT_EQ((uint64_t)30, test_tlbl1i->get_LRU_time());

  for (auto && el : events) delete el;
  events.clear();
  clear_geq();
}

/* 2-2. DTLB */
/* DTLB는 ROB entry를 먼저 설정해줘야 함 */
TEST_F(TLBTest, DTLBReqEvent) {
  set_rob_entry(test_o3rob[0], TEST_ADDR_I, TEST_ADDR_D);
  events.push_back(create_tlb_read_event(test_o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;
  test_tlbl1d->add_req_event(10, events.back());
  
  events.push_back(create_tlb_read_event(test_o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;
  test_tlbl1d->add_req_event(20, events.back());

  EXPECT_EQ((uint64_t)2, test_tlbl1d->geq->event_queue.size());
  EXPECT_EQ((uint64_t)2, test_tlbl1d->req_event.size());
}

TEST_F(TLBTest, DTLBProcessEvent) {
  test_tlbl1d->process_event(10);
  test_tlbl1d->process_event(20);

  // the # of misses is 1 because the accesses have occured at the same address
  EXPECT_EQ((uint64_t)1, test_tlbl1d->get_num_miss());
  EXPECT_EQ((uint64_t)2, test_tlbl1d->get_num_access());
  EXPECT_EQ((uint64_t)1, test_tlbl1d->get_size_of_entries());
  EXPECT_EQ((uint64_t)1, test_tlbl1d->get_size_of_LRU()); 

  // check whether the LRU entry is updated properly
  EXPECT_EQ((uint64_t)20, test_tlbl1d->get_LRU_time());

  // make TLB full
  for (uint i = 1; i <= test_tlbl1d->num_entries; i++) {
    uint64_t address = TEST_ADDR_D + ((1 << test_tlbl1d->page_sz_log2)*i);
    uint32_t rob_entry_num = i % test_o3core->o3rob_max_size;

    set_rob_entry(test_o3rob[rob_entry_num], TEST_ADDR_I, address);
    events.push_back(create_tlb_read_event(test_o3rob[rob_entry_num].memaddr, test_o3core));
    events.back()->rob_entry = rob_entry_num;

    test_tlbl1d->add_req_event(20 + 10*i, events.back());
    test_tlbl1d->process_event(20 + 10*i);
  }

  EXPECT_EQ((uint64_t)65, test_tlbl1d->get_num_miss());   // 1 + 64
  EXPECT_EQ((uint64_t)66, test_tlbl1d->get_num_access()); // 2 + 64
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->get_size_of_entries());
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->get_size_of_LRU());
  EXPECT_EQ((uint64_t)30,  test_tlbl1d->get_LRU_time());

  clear_geq();

  // create one more TLB miss event
  set_rob_entry(test_o3rob[0], TEST_ADDR_I, TEST_ADDR_D - (1 << test_tlbl1d->page_sz_log2));
  events.push_back(create_tlb_read_event(test_o3rob[0].memaddr, test_o3core));
  events.back()->rob_entry = 0;

  test_tlbl1d->add_req_event(1000, events.back());
  test_tlbl1d->process_event(1000);

  EXPECT_EQ((uint64_t)66, test_tlbl1d->get_num_miss());   // 65 + 1
  EXPECT_EQ((uint64_t)67, test_tlbl1d->get_num_access()); // 66 + 1
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->get_size_of_entries());
  EXPECT_EQ(test_tlbl1d->num_entries, test_tlbl1d->get_size_of_LRU());
  EXPECT_EQ((uint64_t)40, test_tlbl1d->get_LRU_time());

  for (auto && el : events) delete el;
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
}
