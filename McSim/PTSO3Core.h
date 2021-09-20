// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTSO3CORE_H_
#define PTSO3CORE_H_

#include "PTS.h"
#include "PTSComponent.h"

#include <queue>
#include <utility>
#include <vector>

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
  virtual ~O3Core();
  uint32_t process_event(uint64_t curr_time);
  void     add_req_event(uint64_t, LocalQueueElement *, Component * from);
  void     add_rep_event(uint64_t, LocalQueueElement *, Component * from);

  CacheL1 * cachel1d;
  CacheL1 * cachel1i;
  TLBL1   * tlbl1d;
  TLBL1   * tlbl1i;
  BranchPredictor * bp;

  const uint32_t num_hthreads;

  const uint32_t lsu_to_l1i_t;
  const uint32_t lsu_to_l1d_t;
  const uint32_t lsu_to_l1i_t_for_x87_op;
  const uint32_t spinning_slowdown;

  const bool     bypass_tlb;
  const uint32_t consecutive_nack_threshold;
  const bool     display_barrier;
  const bool     was_nack;
  const bool     mimick_inorder;

  const uint32_t o3queue_max_size;
  const uint32_t o3rob_max_size;
  const uint32_t max_issue_width;
  const uint32_t max_commit_width;
  const int32_t  max_alu;
  const int32_t  max_ldst;
  const int32_t  max_ld;
  const int32_t  max_st;
  const int32_t  max_sse;

  friend class McSim;

 protected:
  bool     is_active;
  bool     active;

  uint64_t last_time_no_mem_served;
  uint64_t last_time_mem_served;
  uint64_t num_bubbled_slots;

  // pointer to the member variables in the corresponding Pthread object
  ADDRINT stack;
  ADDRINT stacksize;
  uint64_t resume_time;
  // uint64_t mem_time;

  uint64_t num_instrs;  // # of fetched instrs
  uint64_t num_branch;
  uint64_t num_branch_miss;
  uint64_t num_nacks;
  uint64_t num_consecutive_nacks;
  uint64_t num_x87_ops;
  uint64_t num_call_ops;
  uint64_t total_mem_wr_time;
  uint64_t total_mem_rd_time;

  uint32_t branch_miss_penalty;
  uint32_t lock_t;
  uint32_t unlock_t;
  uint32_t barrier_t;
  uint32_t sse_t;

  std::queue<std::pair<ins_type, uint64_t>> mem_acc;
  O3Queue * o3queue;
  uint32_t  o3queue_head;
  uint32_t  o3queue_size;
  O3ROB   * o3rob;
  uint32_t  o3rob_head;
  uint32_t  o3rob_size;
  uint64_t  latest_ip;
  uint64_t  latest_bmp_time;  // latest branch miss prediction time

  bool      is_private(ADDRINT);

  void displayO3Queue();
  void displayO3ROB();
  bool IsRegDep(uint32_t rr, const O3ROB & o3rob_);
};

}  // namespace PinPthread

#endif  // PTSO3CORE_H_

