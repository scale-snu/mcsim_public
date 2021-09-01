#include "O3Core_test.h"
#include "gtest/gtest.h"

namespace PinPthread {
namespace O3CoreTest {

// static variable of O3CoreTest
std::unique_ptr<PthreadTimingSimulator> O3CoreTest::test_pts;
TLBL1* O3CoreTest::test_tlbl1i;
TLBL1* O3CoreTest::test_tlbl1d;
O3CoreForTest* O3CoreTest::test_o3core;
CacheL1* O3CoreTest::test_cachel1i;
CacheL1* O3CoreTest::test_cachel1d;
std::vector<LocalQueueElement *> O3CoreTest::request_events;
std::vector<LocalQueueElement *> O3CoreTest::reply_events;

/* 1. START of O3Core Build && Fixture Build Testing */
TEST_F(O3CoreTest, CheckBuild) {
  EXPECT_NE(nullptr, test_pts) << "wrong PthreadTimingSimulator build";
  EXPECT_NE(nullptr, test_o3core) << "wrong McSim O3Core build";
  EXPECT_NE(nullptr, test_tlbl1i) << "wrong McSim L1ITLB build";
  EXPECT_NE(nullptr, test_tlbl1d) << "wrong McSim L1DTLB build";
  EXPECT_NE(nullptr, test_cachel1i) << "wrong McSim L1ICache build";
  EXPECT_NE(nullptr, test_cachel1d) << "wrong McSim L1DCache build";

  EXPECT_NE(nullptr, test_o3core->bp);
  EXPECT_NE(nullptr, test_o3core->get_o3queue());
  EXPECT_NE(nullptr, test_o3core->get_o3rob());

  EXPECT_LT((uint32_t)4, test_o3core->o3rob_max_size) << "as of now, it is assumed that o3rob_max_size >= 4";
}

/* 2. START of O3Core Testing */
TEST_F(O3CoreTest, Fetch) {
  O3Queue* test_o3queue = test_o3core->get_o3queue();

  // case 1 - Just one instruction exists in o3queue
  // When the process_event of O3Core is called, 
  // look for the oldest instruction waiting for fetch among the instructions in o3queue 
  // and then send the request to iTLB.
  // if process_event is successfully completed, 
  // the state of instructions will change to being_loaded 
  // and an event is placed on req_event of iTLB.
  test_o3queue[0].state = o3iqs_not_in_queue;
  test_o3queue[0].ip = TEST_ADDR_I;
  test_o3core->set_o3queue_size(1);
  test_o3core->set_o3queue_head(0);

  test_o3core->geq->add_event(10, test_o3core);
  test_o3core->process_event(10);

  EXPECT_EQ(o3iqs_being_loaded, test_o3queue[0].state);

  clear_geq();

  // case 2 - multiple instructions exist in o3queue and some are in the same cache block
  // those instructions in the same block become being_loaded
  test_tlbl1i->req_event.clear();  // reset
  test_o3queue[0].state = o3iqs_not_in_queue;
  test_o3queue[1].state = o3iqs_not_in_queue;
  test_o3queue[2].state = o3iqs_not_in_queue;
  test_o3queue[3].state = o3iqs_not_in_queue;
  test_o3queue[0].ip = TEST_ADDR_I;
  test_o3queue[1].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*1;
  test_o3queue[2].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*2;
  test_o3queue[3].ip = TEST_ADDR_I;
  test_o3core->set_o3queue_size(4);
  test_o3core->set_o3queue_head(0);
  
  test_o3core->geq->add_event(10, test_o3core);
  test_o3core->process_event(10);

  EXPECT_EQ(o3iqs_being_loaded, test_o3queue[0].state);
  EXPECT_EQ(o3iqs_not_in_queue, test_o3queue[1].state);
  EXPECT_EQ(o3iqs_not_in_queue, test_o3queue[2].state);
  EXPECT_EQ(o3iqs_being_loaded, test_o3queue[3].state);  // also being_loaded!
}

TEST_F(O3CoreTest, Dispatch) {
  O3Queue* test_o3queue = test_o3core->get_o3queue();
  O3ROB* test_o3rob = test_o3core->get_o3rob();
  
  // case 1 - The o3queue has only one instruction in the state of o3iqs_ready
  // The instruction is not a branch instruction and doesn’t have any branch & memory dependency.
  // But it does have register dependency on rr0 and o3rob[0].rw0
  test_o3queue[0].ip = TEST_ADDR_I;
  test_o3queue[0].state = o3iqs_ready;
  test_o3queue[0].ready_time = 5;
  test_o3queue[0].rr0 = 10;                  // true dependency on o3rob[0].rw0
  test_o3queue[0].rr1 = 0;
  test_o3queue[0].rr2 = 0;
  test_o3queue[0].rr3 = 0;
  test_o3queue[0].raddr = 0;
  test_o3queue[0].raddr2 = 0;
  test_o3queue[0].waddr = 0;

  test_o3core->set_o3queue_size(1);
  test_o3core->set_o3queue_head(0);

  test_o3rob[0].branch_miss = false;
  test_o3rob[0].state = o3irs_executing;
  test_o3rob[0].ready_time = 15;
  test_o3rob[0].rw0 = 10;                    // true dependency on o3queue[0]
  test_o3rob[0].rw1 = -1;
  test_o3rob[0].rw2 = -1;
  test_o3rob[0].rw3 = -1;

  test_o3core->set_o3rob_head(0);
  test_o3core->set_o3rob_size(1);

  test_o3core->geq->add_event(10, test_o3core);
  test_o3core->process_event(10);

  // after process_event() call
  EXPECT_EQ((int32_t)0, test_o3rob[1].rr0);  // true dependency (pointing rob[0])
  EXPECT_EQ((int32_t)-1, test_o3rob[1].rr1);
  EXPECT_EQ((int32_t)-1, test_o3rob[1].rr2);
  EXPECT_EQ((int32_t)-1, test_o3rob[1].rr3);

  EXPECT_EQ((uint32_t)0, test_o3core->get_o3queue_size());  // 1 - 1 --> 0
  EXPECT_EQ((uint32_t)1, test_o3core->get_o3queue_head());  // 0 + 1 --> 1
  EXPECT_EQ((uint32_t)2, test_o3core->get_o3rob_size());    // 1 + 1 --> 2

  clear_geq();

  // case 2 - the queue has only one instruction that accesses the memory 3 times
  // it will occupy 3 o3rob entries
  test_o3queue[1].ip = TEST_ADDR_I;
  test_o3queue[1].state = o3iqs_ready;
  test_o3queue[1].ready_time = 5;
  test_o3queue[1].rr0 = 0;
  test_o3queue[1].rr1 = 0;
  test_o3queue[1].rr2 = 0;
  test_o3queue[1].rr3 = 0;
  test_o3queue[1].raddr = TEST_ADDR_D;
  test_o3queue[1].raddr2 = TEST_ADDR_D + (1 << test_cachel1d->set_lsb)*1;
  test_o3queue[1].waddr = TEST_ADDR_D + (1 << test_cachel1d->set_lsb)*2;
  test_o3core->set_o3queue_size(1);
  test_o3core->set_o3queue_head(1);

  test_o3core->geq->add_event(10, test_o3core);
  test_o3core->process_event(10);

  EXPECT_EQ((uint32_t)0, test_o3core->get_o3queue_size());   // 1 - 1 --> 0
  EXPECT_EQ((uint32_t)2, test_o3core->get_o3queue_head());   // 1 + 1 --> 2
  EXPECT_EQ((uint32_t)5, test_o3core->get_o3rob_size());     // 2 + 3 --> 5
}

TEST_F(O3CoreTest, Execute) {
  O3ROB* test_o3rob = test_o3core->get_o3rob();

// case 1 - 6 non-memory operation
  for (int i = 0; i < 6; i++) {
    test_o3rob[i].type = ins_x87;
    test_o3rob[i].state = o3irs_issued;
    test_o3rob[i].ready_time = 5;
    test_o3rob[i].memaddr = 0;                 // non-memory op
    test_o3rob[i].rr0 = -1;
    test_o3rob[i].rr1 = -1;
    test_o3rob[i].rr2 = -1;
    test_o3rob[i].rr3 = -1;
    test_o3rob[i].mem_dep = -1;
    test_o3rob[i].instr_dep = -1;
    test_o3rob[i].branch_dep = -1;
    test_o3rob[i].branch_miss = false;
  }
  test_o3rob[3].type = no_mem;
  test_o3rob[3].branch_miss = true;            // This will get branch_miss penalty
  test_o3core->set_o3rob_head(0);
  test_o3core->set_o3rob_size(6);

  uint64_t curr_time = 0;
  uint64_t process_interval = test_o3core->process_interval;

  curr_time += process_interval;
  // call o3core->process_event instead of geq->process_event
  test_o3core->process_event(curr_time);

  EXPECT_EQ(o3irs_completed, test_o3rob[0].state);
  EXPECT_EQ(o3irs_completed, test_o3rob[1].state);
  EXPECT_EQ(o3irs_completed, test_o3rob[2].state);
  EXPECT_EQ(o3irs_completed, test_o3rob[3].state);  // max_alu == 4
  EXPECT_EQ(o3irs_issued, test_o3rob[4].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[5].state);

  EXPECT_EQ((uint64_t)curr_time + test_o3core->get_sse_t(), test_o3rob[0].ready_time);
  EXPECT_EQ((uint64_t)curr_time + test_o3core->get_sse_t(), test_o3rob[1].ready_time);
  EXPECT_EQ((uint64_t)curr_time + test_o3core->get_sse_t(), test_o3rob[2].ready_time);
  EXPECT_EQ((uint64_t)curr_time + test_o3core->get_branch_miss_penalty() + test_o3core->process_interval, \
            test_o3rob[3].ready_time);              // branch miss penalty
  
  curr_time += process_interval;
  test_o3core->process_event(curr_time);

  EXPECT_EQ(o3irs_completed, test_o3rob[4].state);  // assume that SSE unit is fully pipelined
  EXPECT_EQ(o3irs_completed, test_o3rob[5].state);

  // case 2 - memory operation
  for (int i = 0; i < 6; i++) {
    test_o3rob[i].isread = false;
    test_o3rob[i].state = o3irs_issued;
    test_o3rob[i].ready_time = 5;
    test_o3rob[i].memaddr = TEST_ADDR_D;    // mem op
    test_o3rob[i].rr0 = -1;
    test_o3rob[i].rr1 = -1;
    test_o3rob[i].rr2 = -1;
    test_o3rob[i].rr3 = -1;
    test_o3rob[i].mem_dep = -1;
    test_o3rob[i].instr_dep = -1;
    test_o3rob[i].branch_dep = -1;
    test_o3rob[i].branch_miss = false;
  }
  test_o3core->set_o3rob_head(0);
  test_o3core->set_o3rob_size(6);

  curr_time = process_interval;
  test_o3core->process_event(curr_time);

  EXPECT_EQ(o3irs_executing, test_o3rob[0].state);
  EXPECT_EQ(o3irs_executing, test_o3rob[1].state);
  EXPECT_EQ(o3irs_executing, test_o3rob[2].state);
  EXPECT_EQ(o3irs_executing, test_o3rob[3].state);  // max_ldst == 4
  EXPECT_EQ(o3irs_issued, test_o3rob[4].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[5].state);
}

TEST_F(O3CoreTest, Commit) {
  O3ROB* test_o3rob = test_o3core->get_o3rob();

  // state: completed 6, executing 1
  test_o3rob[0].state = o3irs_completed;
  test_o3rob[1].state = o3irs_completed;
  test_o3rob[2].state = o3irs_executing;  // executing!
  test_o3rob[3].state = o3irs_completed;
  test_o3rob[4].state = o3irs_completed;
  test_o3rob[5].state = o3irs_completed;
  test_o3rob[6].state = o3irs_completed;

  // ready_time: ready 6, not ready 1
  test_o3rob[0].ready_time = 0;
  test_o3rob[1].ready_time = 0;
  test_o3rob[2].ready_time = 0;
  test_o3rob[3].ready_time = 0;
  test_o3rob[4].ready_time = 0;
  test_o3rob[5].ready_time = 0;
  test_o3rob[6].ready_time = 40;

  test_o3core->set_o3rob_size(7);
  test_o3core->set_o3rob_head(0);

  // CHECK in-order commit
  test_o3core->geq->add_event(10, test_o3core);
  test_o3core->process_event(10);

  EXPECT_EQ(o3irs_invalid, test_o3rob[0].state);    // commit
  EXPECT_EQ(o3irs_invalid, test_o3rob[1].state);    // commit
  EXPECT_EQ(o3irs_executing, test_o3rob[2].state);  // executing
  EXPECT_EQ(o3irs_completed, test_o3rob[3].state);  // Wait for the previous command to commit
  EXPECT_EQ(o3irs_completed, test_o3rob[4].state);

  EXPECT_EQ((uint32_t)5, test_o3core->get_o3rob_size());
  EXPECT_EQ((uint32_t)2, test_o3core->get_o3rob_head());  // only 2 instructions are committed

  // CHECK time constraints
  test_o3rob[2].state = o3irs_completed;
  test_o3core->geq->add_event(20, test_o3core);
  test_o3core->process_event(20);

  EXPECT_EQ(o3irs_invalid, test_o3rob[2].state);    // commit
  EXPECT_EQ(o3irs_invalid, test_o3rob[3].state);    // commit
  EXPECT_EQ(o3irs_invalid, test_o3rob[4].state);    // commit
  EXPECT_EQ(o3irs_invalid, test_o3rob[5].state);    // commit
  EXPECT_EQ(o3irs_completed, test_o3rob[6].state);  // Not ready yet

  EXPECT_EQ((uint32_t)1, test_o3core->get_o3rob_size());
  EXPECT_EQ((uint32_t)6, test_o3core->get_o3rob_head());  // 6 instructions are committed and one instruction left

  // CHECK all instructions completed well
  test_o3core->geq->add_event(50, test_o3core);
  test_o3core->process_event(50);

  EXPECT_EQ(o3irs_invalid, test_o3rob[6].state);
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3rob_size());
  EXPECT_EQ((uint32_t)7, test_o3core->get_o3rob_head());  // all instructions are committed
}

// reply event from ITLB, L1I cache
TEST_F(O3CoreTest, ReqEvent) {  

  // case 1 - event from ITLB
  request_events.push_back(new LocalQueueElement(test_tlbl1i, et_tlb_rd, TEST_ADDR_I, 0));
  EXPECT_TRUE(test_cachel1i->req_event.empty());

  test_o3core->add_req_event(10, request_events[0], test_tlbl1i);

  EXPECT_EQ(et_read, request_events[0]->type);  // event type changed from TLB read(et_tlb_rd) to cache read(et_read)
  EXPECT_EQ((uint32_t)1, test_cachel1i->req_event.size());

  // case 2 - read fail from L1I cache (nack)
  request_events.push_back(new LocalQueueElement(test_cachel1i, et_nack, TEST_ADDR_I, 0));
  EXPECT_EQ((uint64_t)0, test_o3core->get_num_consecutive_nacks());
  EXPECT_EQ((uint64_t)0, test_o3core->get_num_nacks());

  test_o3core->add_req_event(10, request_events[1], test_cachel1i);

  EXPECT_EQ((uint64_t)1, test_o3core->get_num_consecutive_nacks());
  EXPECT_EQ((uint64_t)1, test_o3core->get_num_nacks());
  EXPECT_EQ(et_read, request_events[1]->type);  // event type changed from nack to cache read
  EXPECT_EQ((uint32_t)2, test_cachel1i->req_event.size());
  
  // case 3 - successfully read from L1I cache
  request_events.push_back(new LocalQueueElement(test_cachel1i, et_read, TEST_ADDR_I, 0));
  O3Queue* test_o3queue = test_o3core->get_o3queue();
  test_o3queue[0].state = o3iqs_being_loaded;
  test_o3queue[1].state = o3iqs_not_in_queue;  // not in queue
  test_o3queue[2].state = o3iqs_being_loaded;
  test_o3queue[3].state = o3iqs_being_loaded;

  test_o3queue[0].ip = TEST_ADDR_I;
  test_o3queue[1].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*1;
  test_o3queue[2].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*2;
  test_o3queue[3].ip = TEST_ADDR_I;
  test_o3core->set_o3queue_size(4);

  test_o3core->add_req_event(10, request_events[2], test_cachel1i);

  EXPECT_EQ((uint64_t)0, test_o3core->get_num_consecutive_nacks());
  EXPECT_EQ((uint64_t)1, test_o3core->get_num_nacks());
  EXPECT_EQ(o3iqs_ready, test_o3queue[0].state);  // loaded!
  EXPECT_EQ(o3iqs_not_in_queue, test_o3queue[1].state);
  EXPECT_EQ(o3iqs_being_loaded, test_o3queue[2].state);
  EXPECT_EQ(o3iqs_ready, test_o3queue[3].state);  // also loaded
}

// reply event from DTLB, L1D cache
TEST_F(O3CoreTest, RepEvent) {

  O3ROB* test_o3rob = test_o3core->get_o3rob();

  // case 1 - event from DTLB
  reply_events.push_back(new LocalQueueElement(test_tlbl1d, et_tlb_rd, TEST_ADDR_D, 0));
  reply_events.push_back(new LocalQueueElement(test_tlbl1d, et_tlb_rd, TEST_ADDR_D, 0));
  reply_events[0]->rob_entry = 0;
  reply_events[1]->rob_entry = 1;
  test_o3rob[0].isread = true;   // read
  test_o3rob[1].isread = false;  // write

  test_o3core->set_o3rob_size(2);
  test_o3core->set_o3rob_head(0);
  EXPECT_TRUE(test_cachel1d->req_event.empty());
  
  test_o3core->add_rep_event(10, reply_events[0], test_tlbl1d);  // read event
  test_o3core->add_rep_event(10, reply_events[1], test_tlbl1d);  // write event
  
  EXPECT_EQ(et_read,  reply_events[0]->type);  // event type changed from TLB read(et_tlb_rd) to cache read(et_read)
  EXPECT_EQ(et_write, reply_events[1]->type);  // event type changed from TLB read(et_tlb_rd) to cache write(et_write)
  EXPECT_EQ((uint32_t)2, test_cachel1d->req_event.size());  // now, L1D cache has 2 request events

  // case 2 - read/write fail from L1D cache (nack)
  reply_events.push_back(new LocalQueueElement(test_cachel1d, et_nack, TEST_ADDR_D, 0));
  reply_events[2]->rob_entry = 2;
  test_o3rob[2].isread = true;   // read
  
  test_o3core->set_o3rob_size(3);
  test_o3core->set_o3rob_head(0);

  test_o3core->add_rep_event(10, reply_events[2], test_cachel1d);
  EXPECT_EQ((uint64_t)1, test_o3core->get_num_consecutive_nacks());
  EXPECT_EQ((uint64_t)2, test_o3core->get_num_nacks());  // one for above req event test and one for now
  EXPECT_EQ(et_read, reply_events[2]->type);  // event type changed from nack(et_nack) to cache read(et_read)
  EXPECT_EQ((uint32_t)3, test_cachel1d->req_event.size());

  // case 3 - successfully read from L1D cache
  reply_events.push_back(new LocalQueueElement(test_cachel1d, et_read, TEST_ADDR_D, 0));
  reply_events[3]->rob_entry = 3;
  test_o3rob[3].isread = true;   // read
  test_o3rob[3].state = o3irs_executing;
  test_o3rob[3].branch_miss = false;
  uint64_t temp_tot_mem_rd_time = test_o3core->get_total_mem_rd_time();
  uint64_t temp_tot_mem_wr_time = test_o3core->get_total_mem_wr_time();

  test_o3core->add_rep_event(20, reply_events[3], test_cachel1d);
  EXPECT_LT(temp_tot_mem_rd_time, test_o3core->get_total_mem_rd_time());  // updated
  EXPECT_EQ(temp_tot_mem_wr_time, test_o3core->get_total_mem_wr_time());  // same
  EXPECT_EQ(o3irs_completed, test_o3rob[3].state);
  EXPECT_EQ((uint64_t)0, test_o3core->get_num_consecutive_nacks());
  EXPECT_EQ((uint64_t)2, test_o3core->get_num_nacks());
}

TEST_F(O3CoreTest, Scenario1) {
  O3Queue* test_o3queue = test_o3core->get_o3queue();
  O3ROB* test_o3rob = test_o3core->get_o3rob();
  
  /*
  LOAD F2 (memaddr)
  ADD  F6 F2 F4  // true dep. with LOAD(F2)
  SUB  F8 F4 F8
  DIV  F8 F6 F2  // true dep. with LOAD(F2) and ADD(F6)
  */
  
  test_o3queue[0].ip = TEST_ADDR_I;
  test_o3queue[0].type = mem_rd;   // LOAD F2 memaddr
  test_o3queue[0].state = o3iqs_ready;
  test_o3queue[0].ready_time = 5;
  test_o3queue[0].rr0 = 0;  test_o3queue[0].rr1 = 0;  test_o3queue[0].rr2 = 0;  test_o3queue[0].rr3 = 0;
  test_o3queue[0].rw0 = 2;  test_o3queue[0].rw1 = 0;  test_o3queue[0].rw2 = 0;  test_o3queue[0].rw3 = 0;
  test_o3queue[0].raddr = 0;  test_o3queue[0].raddr2 = 0;  test_o3queue[0].waddr = 140737488348888;
  
  test_o3queue[1].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*1;
  test_o3queue[1].type = no_mem;   // ADD F6 F2 F4
  test_o3queue[1].state = o3iqs_ready;
  test_o3queue[1].ready_time = 5;
  test_o3queue[1].rr0 = 2;  test_o3queue[1].rr1 = 4;  test_o3queue[1].rr2 = 0;  test_o3queue[1].rr3 = 0;
  test_o3queue[1].rw0 = 6;  test_o3queue[1].rw1 = 0;  test_o3queue[1].rw2 = 0;  test_o3queue[1].rw3 = 0;
  test_o3queue[1].raddr = 0;  test_o3queue[1].raddr2 = 0;  test_o3queue[1].waddr = 0;

  test_o3queue[2].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*2;
  test_o3queue[2].type = no_mem;   // SUB F8 F4 F8
  test_o3queue[2].state = o3iqs_ready;
  test_o3queue[2].ready_time = 5;
  test_o3queue[2].rr0 = 4;  test_o3queue[2].rr1 = 8;  test_o3queue[2].rr2 = 0;  test_o3queue[2].rr3 = 0;
  test_o3queue[2].rw0 = 8;  test_o3queue[2].rw1 = 0;  test_o3queue[2].rw2 = 0;  test_o3queue[2].rw3 = 0;
  test_o3queue[2].raddr = 0;  test_o3queue[2].raddr2 = 0;  test_o3queue[2].waddr = 0;

  test_o3queue[3].ip = TEST_ADDR_I + (1 << test_cachel1i->set_lsb)*3;
  test_o3queue[3].type = no_mem;    // DIV F8 F6 F2
  test_o3queue[3].state = o3iqs_ready;
  test_o3queue[3].ready_time = 5;
  test_o3queue[3].rr0 = 6;  test_o3queue[3].rr1 = 2;  test_o3queue[3].rr2 = 0;  test_o3queue[3].rr3 = 0;
  test_o3queue[3].rw0 = 8;  test_o3queue[3].rw1 = 0;  test_o3queue[3].rw2 = 0;  test_o3queue[3].rw3 = 0;
  test_o3queue[3].raddr = 0;  test_o3queue[3].raddr2 = 0;  test_o3queue[3].waddr = 0;

  test_o3core->set_o3queue_size(4);
  test_o3core->set_o3queue_head(0);

  EXPECT_EQ((uint32_t)4, test_o3core->get_o3queue_size());
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3queue_head());
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3rob_size());
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3rob_head());

  test_o3core->process_event(10);
  // three instructions are issued
  EXPECT_EQ(o3irs_issued, test_o3rob[0].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[1].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[2].state); 
  EXPECT_EQ(o3irs_issued, test_o3rob[3].state); 
  
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3queue_size());
  EXPECT_EQ((uint32_t)4, test_o3core->get_o3queue_head());
  EXPECT_EQ((uint32_t)4, test_o3core->get_o3rob_size());  // all instructions are issued
  EXPECT_EQ((uint32_t)0, test_o3core->get_o3rob_head());

  EXPECT_EQ((int32_t)0, test_o3rob[1].rr0);   // true dep.: rob[0]
  EXPECT_EQ((int32_t)-1, test_o3rob[1].rr1);  // no dep
  EXPECT_EQ((int32_t)-1, test_o3rob[2].rr0);
  EXPECT_EQ((int32_t)-1, test_o3rob[2].rr1);
  EXPECT_EQ((int32_t)1, test_o3rob[3].rr0);   // true dep.: rob[1]
  EXPECT_EQ((int32_t)0, test_o3rob[3].rr1);   // true dep.: rob[0]

  test_o3core->process_event(20);
  // The first instruction is in the state of executing and the third instruction is in the state of completed (the other two instructions are waiting)
  EXPECT_EQ(o3irs_executing, test_o3rob[0].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[1].state);  // can't execute (data dependency)
  EXPECT_EQ(o3irs_completed, test_o3rob[2].state);  // doesn't have any dependency. out-of-order execute
  EXPECT_EQ(o3irs_issued, test_o3rob[3].state);  // same as above

  test_o3rob[0].state = o3irs_completed;
  test_o3rob[0].ready_time = 30;
  test_o3core->process_event(30);
  // make rob[0] completed and run process_event()
  EXPECT_EQ(o3irs_invalid, test_o3rob[0].state);  // commited
  EXPECT_EQ(o3irs_issued, test_o3rob[1].state);
  EXPECT_EQ(o3irs_completed, test_o3rob[2].state);  // in-order commit
  EXPECT_EQ(o3irs_issued, test_o3rob[3].state);
  EXPECT_EQ((int32_t)-1, test_o3rob[1].rr0);  // dep. resolved
  EXPECT_EQ((int32_t)-1, test_o3rob[3].rr1);  // dep. resolved

  test_o3core->process_event(40);
  // rob[1] completed
  EXPECT_EQ(o3irs_completed, test_o3rob[1].state);  // non-mem op (no executing state)
  EXPECT_EQ(o3irs_completed, test_o3rob[2].state);
  EXPECT_EQ(o3irs_issued, test_o3rob[3].state);

  test_o3core->process_event(50);
  // rob[1] commited
  EXPECT_EQ(o3irs_invalid, test_o3rob[1].state);
  EXPECT_EQ(o3irs_invalid, test_o3rob[2].state); 
  EXPECT_EQ(o3irs_issued, test_o3rob[3].state);
  EXPECT_EQ((int32_t)-1, test_o3rob[3].rr0);  // dep. resolved

  test_o3core->process_event(60);
  // rob[3] completed
  EXPECT_EQ(o3irs_completed, test_o3rob[3].state);

  test_o3core->process_event(70);
  // rob[3] commited
  EXPECT_EQ(o3irs_invalid, test_o3rob[3].state);

  EXPECT_EQ((uint32_t)0, test_o3core->get_o3rob_size());
  EXPECT_EQ((uint32_t)4, test_o3core->get_o3rob_head());
}

}
}
