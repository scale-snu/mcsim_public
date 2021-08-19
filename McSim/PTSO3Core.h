/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#ifndef PTSO3CORE_H_
#define PTSO3CORE_H_

#include "PTS.h"
#include "PTSComponent.h"

#include <queue>
#include <utility>
#include <gtest/gtest_prod.h>

namespace PinPthread {

class BranchPredictor {
 public:
  BranchPredictor(uint32_t num_entries_, uint32_t gp_size_);
  ~BranchPredictor() { }

  bool miss(uint64_t addr, bool taken);

 private:
  uint32_t num_entries;
  uint32_t gp_size;
  std::vector<uint32_t> bimodal_entry;  // 0 -- strongly not taken
  uint64_t global_history;
};


enum o3_instr_queue_state {
  o3iqs_not_in_queue,  // just arrived
  o3iqs_being_loaded,  // contacting i$
  o3iqs_ready,         // returned from i$, wait to be issued
  o3iqs_invalid
};

enum o3_instr_rob_state {
  o3irs_issued,  // wait until all depending instructions to be executed
  o3irs_executing,  // instruction being executed now -- complete time is set
  o3irs_completed,  // instruction was completed
  o3irs_invalid
};

class O3Queue {
 public:
  o3_instr_queue_state state;
  uint64_t ready_time;
  uint64_t waddr;
  uint32_t wlen;
  uint64_t raddr;
  uint64_t raddr2;
  uint32_t rlen;
  uint64_t ip;
  ins_type type;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};

class O3ROB {
 public:
  o3_instr_rob_state state;
  uint64_t ready_time;
  uint64_t ip;  // just for debugging
  uint64_t memaddr;  // 0 means no_mem
  bool     isread;
  bool     branch_miss;
  int32_t  mem_dep;
  int32_t  instr_dep;
  int32_t  branch_dep;
  ins_type type;
  int32_t  rr0;
  int32_t  rr1;
  int32_t  rr2;
  int32_t  rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};

class McSim;

class O3Core : public Component {
 public:
  explicit O3Core(component_type type_, uint32_t num_, McSim * mcsim_);
  ~O3Core();
  uint32_t process_event(uint64_t curr_time);
  void     add_req_event(uint64_t, LocalQueueElement *, Component * from);
  void     add_rep_event(uint64_t, LocalQueueElement *, Component * from);

 private:
  uint32_t num_hthreads;
  bool     is_active;

  uint64_t last_time_no_mem_served;
  uint64_t last_time_mem_served;
  uint64_t num_bubbled_slots;

  CacheL1 * cachel1d;
  CacheL1 * cachel1i;
  TLBL1   * tlbl1d;
  TLBL1   * tlbl1i;
  BranchPredictor * bp;

  // pointer to the member variables in the corresponding Pthread object
  bool active;
  ADDRINT stack;
  ADDRINT stacksize;

  uint64_t resume_time;
  uint64_t mem_time;

  uint64_t num_instrs;  // # of fetched instrs
  uint64_t num_branch;
  uint64_t num_branch_miss;
  uint64_t num_nacks;
  uint64_t num_consecutive_nacks;
  uint64_t num_x87_ops;
  uint64_t num_call_ops;
  uint64_t total_mem_wr_time;
  uint64_t total_mem_rd_time;

  const uint32_t lsu_to_l1i_t;
  const uint32_t lsu_to_l1d_t;
  const uint32_t lsu_to_l1i_t_for_x87_op;
  uint32_t branch_miss_penalty;
  const uint32_t spinning_slowdown;
  bool           bypass_tlb;
  uint32_t lock_t;
  uint32_t unlock_t;
  uint32_t barrier_t;
  uint32_t sse_t;
  const uint32_t consecutive_nack_threshold;
  bool           display_barrier;
  bool           was_nack;
  bool           mimick_inorder;

  std::queue<std::pair<ins_type, uint64_t>> mem_acc;
  O3Queue * o3queue;
  O3ROB   * o3rob;
  uint32_t  o3queue_max_size;
  uint32_t  o3queue_head;
  uint32_t  o3queue_size;
  uint32_t  max_issue_width;
  uint32_t  max_commit_width;
  uint32_t  o3rob_max_size;
  uint32_t  o3rob_head;
  uint32_t  o3rob_size;
  uint64_t  latest_ip;
  uint64_t  latest_bmp_time;  // latest branch miss prediction time
  int32_t   max_alu;
  int32_t   max_ldst;
  int32_t   max_ld;
  int32_t   max_st;
  int32_t   max_sse;

  bool      is_private(ADDRINT);

  void displayO3Queue();
  void displayO3ROB();

  bool IsRegDep(uint32_t rr, const O3ROB & o3rob_);

 public:
  friend class McSim;

  FRIEND_TEST(TLBTest, DTLBReqEvent);
  FRIEND_TEST(TLBTest, DTLBProcessEvent);

  friend class O3CoreTest;   // for O3CoreTest::TearDown()
  FRIEND_TEST(O3CoreTest, CheckBuild);
  FRIEND_TEST(O3CoreTest, Fetch);
  FRIEND_TEST(O3CoreTest, Dispatch);
  FRIEND_TEST(O3CoreTest, Execute);
  FRIEND_TEST(O3CoreTest, Commit);
  FRIEND_TEST(O3CoreTest, RepEvent);
  FRIEND_TEST(O3CoreTest, ReqEvent);
  FRIEND_TEST(O3CoreTest, Scenario1);
};

}  // namespace PinPthread

#endif  // PTSO3CORE_H_

