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

#include "McSim.h"
#include "PTSCache.h"
#include "PTSO3Core.h"
#include "PTSTLB.h"

#include <glog/logging.h>
#include <algorithm>
#include <iomanip>
#include <sstream>


namespace PinPthread {

BranchPredictor::BranchPredictor(uint32_t num_entries_, uint32_t gp_size_):
  num_entries(num_entries_), gp_size(gp_size_), bimodal_entry(num_entries, 1),
  global_history(0) {
}


bool BranchPredictor::miss(uint64_t addr, bool taken) {
  bool miss;
  global_history      = (gp_size == 0) ? 0 : ((global_history << 1) + (taken == true ? 1 : 0));
  addr                = addr ^ (global_history << (64 - gp_size));
  uint32_t curr_entry = bimodal_entry[addr%num_entries];

  miss = (curr_entry > 1 && taken == false) || (curr_entry < 2 && taken == true);
  bimodal_entry[addr%num_entries] = (curr_entry == 0 && taken == false) ? 0 :
    (curr_entry == 3 && taken == true)  ? 3 :
    (taken == true) ? (curr_entry + 1) : (curr_entry - 1);
  return miss;
}

static const int32_t word_log = 3;

std::ostream& operator<<(std::ostream & output, o3_instr_queue_state iqs) {
  switch (iqs) {
    case o3iqs_not_in_queue: output << "iqs_niq"; break;
    case o3iqs_being_loaded: output << "iqs_bld"; break;
    case o3iqs_ready:        output << "iqs_rdy"; break;
    default:                 output << "iqs_inv"; break;
  }
  return output;
}


std::ostream & operator<<(std::ostream & output, o3_instr_rob_state irs) {
  switch (irs) {
    case o3irs_issued:    output << "irs_iss"; break;
    case o3irs_executing: output << "irs_exe"; break;
    case o3irs_completed: output << "irs_cmp"; break;
    default:              output << "irs_inv"; break;
  }
  return output;
}


extern std::ostream & operator << (std::ostream & output, ins_type it);

O3Core::O3Core(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Component(type_, num_, mcsim_),
  num_hthreads(get_param_uint64("num_hthreads", "pts.", max_hthreads)),
  is_active(),
  last_time_no_mem_served(0),
  last_time_mem_served(0),
  num_bubbled_slots(0),
  active(false), stack(0), stacksize(0),
  resume_time(0),
  num_instrs(0), num_branch(0), num_branch_miss(0), num_nacks(0),
  num_consecutive_nacks(0),
  num_x87_ops(0), num_call_ops(0), total_mem_wr_time(0),
  total_mem_rd_time(0),
  lsu_to_l1i_t(get_param_uint64("to_l1i_t", 10)),
  lsu_to_l1d_t(get_param_uint64("to_l1d_t", 10)),
  lsu_to_l1i_t_for_x87_op(get_param_uint64("to_l1i_t_for_x87_op", lsu_to_l1i_t)),
  branch_miss_penalty(get_param_uint64("branch_miss_penalty", 100)),
  spinning_slowdown(get_param_uint64("spinning_slowdown", 10)),
  lock_t(get_param_uint64("lock_t", 100)),
  unlock_t(get_param_uint64("unlock_t", 100)),
  barrier_t(get_param_uint64("barrier_t", 100)),
  sse_t(get_param_uint64("sse_t", 40)),
  consecutive_nack_threshold(get_param_uint64("consecutive_nack_threshold", 1000)),
  was_nack(false),
  latest_ip(0), latest_bmp_time(0) {
  process_interval    = get_param_uint64("process_interval", 80);
  branch_miss_penalty = ceil_by_y(branch_miss_penalty, process_interval);
  lock_t              = ceil_by_y(lock_t,              process_interval);
  unlock_t            = ceil_by_y(unlock_t,            process_interval);
  barrier_t           = ceil_by_y(barrier_t,           process_interval);

  bp = new BranchPredictor(
      get_param_uint64("num_bp_entries", 256),
      get_param_uint64("gp_size", 0));
  CHECK(mem_acc.empty());

  bypass_tlb       = get_param_bool("bypass_tlb", false);
  display_barrier  = get_param_bool("display_barrier", false);
  mimick_inorder   = get_param_bool("mimick_inorder", false);

  o3queue_max_size = get_param_uint64("o3queue_max_size", 64) + 4;
  o3rob_max_size   = get_param_uint64("o3rob_max_size",   16);
  max_issue_width  = get_param_uint64("max_issue_width",   4);
  max_commit_width = get_param_uint64("max_commit_width",  4);
  max_alu          = get_param_uint64("max_alu",  max_commit_width);
  max_ldst         = get_param_uint64("max_ldst", max_commit_width);
  max_ld           = get_param_uint64("max_ld",   max_commit_width);
  max_st           = get_param_uint64("max_st",   max_commit_width);
  max_sse          = get_param_uint64("max_sse",  max_commit_width);
  o3queue = new O3Queue[o3queue_max_size];
  o3rob   = new O3ROB[o3rob_max_size];
  o3queue_head = 0;
  o3queue_size = 0;
  o3rob_head   = 0;
  o3rob_size   = 0;

  for (unsigned int i = 0; i < o3queue_max_size; i++) {
    o3queue[i].state = o3iqs_invalid;
  }

  for (unsigned int i = 0; i < o3rob_max_size; i++) {
    o3rob[i].state = o3irs_invalid;
  }

  CHECK_GE(o3rob_max_size, 4) << "as of now, it is assumed that o3rob_max_size >= 4" << std::endl;
}


O3Core::~O3Core() {
  if (num_instrs > 0) {
    std::cout << "  -- OOO [" << std::setw(3) << num << "] : fetched " << std::setw(10) << num_instrs
         << " instrs, branch (miss, access)=( "
         << std::setw(8) << num_branch_miss << ", " << std::setw(10) << num_branch << ")= ";
    if (num_branch > 0) {
      std::cout << std::setw(6) << std::setiosflags(std::ios::fixed) << std::setprecision(2) << 100.00*num_branch_miss/num_branch << "%,";
    } else {
      std::cout << std::setw(6) << 0 << "%,";
    }
    std::cout << " nacks= " << num_nacks
         << ", x87_ops= " << num_x87_ops
         << ", call_ops= " << num_call_ops
         << ", latest_ip= 0x" << std::hex << latest_ip << std::dec
         << ", tot_mem_wr_time= " << total_mem_wr_time
         << ", tot_mem_rd_time= " << total_mem_rd_time << std::endl;
  }

  delete bp;
}


uint32_t O3Core::process_event(uint64_t curr_time) {
  if (mcsim->skip_all_instrs == true) {
    o3queue_size = 0;
    resume_time  = curr_time;
  }

  if (o3queue_size <= (o3queue_max_size >> 1)) {
    if (active == false) {
      // return num_hthreads;
    } else {
      geq->add_event(curr_time, this);
      return num;
    }
    // geq->add_event(curr_time, this);
    // return (active == false ? num_hthreads : num);
  }

  // process o3queue

  // check o3queue and send a request to iTLB
  // -- TODO(gajh): can we send multiple requests to an iTLB simultaneously?
  uint64_t addr_to_read = 0;
  for (unsigned int i = 0; i < o3queue_size; i++) {
    unsigned int idx = (i + o3queue_head) % o3queue_max_size;

    if (o3queue[idx].state != o3iqs_not_in_queue) continue;
    if (addr_to_read == 0) {
      addr_to_read = (o3queue[idx].ip >> cachel1i->set_lsb) << cachel1i->set_lsb;
      o3queue[idx].state = o3iqs_being_loaded;
      auto lqe = new LocalQueueElement(this, et_tlb_rd, addr_to_read, num);
      if (bypass_tlb == true) {
        lqe->type = et_read;
        cachel1i->add_req_event(curr_time + lsu_to_l1i_t, lqe);
      } else {
        tlbl1i->add_req_event(curr_time + lsu_to_l1i_t, lqe);
      }
    } else if ((addr_to_read >> cachel1i->set_lsb) == (o3queue[idx].ip >> cachel1i->set_lsb)) {
      o3queue[idx].state = o3iqs_being_loaded;
    }
  }

  // check queue and send the instructions to the reorder buffer
  // "-3" is a number considering that an instruction might
  // occupy up to three ROB entries (when it has 3 memory accesses).
  for (unsigned int i = 0; i < max_issue_width && o3queue_size > 0 && o3rob_size < o3rob_max_size - 3; i++) {
    O3Queue & o3q_entry      = o3queue[o3queue_head];

    if (o3q_entry.state == o3iqs_ready && o3q_entry.ready_time <= curr_time) {
      unsigned int rob_idx = (o3rob_head + o3rob_size) % o3rob_max_size;
      bool branch_miss     = false;

      if (o3q_entry.type == ins_branch_taken || o3q_entry.type == ins_branch_not_taken) {
        num_branch++;
        if (bp->miss(o3q_entry.ip, o3q_entry.type == ins_branch_taken)) {
          num_branch_miss++;
          branch_miss = true;
        }
      }

      int32_t instr_dep  = -1;
      int32_t branch_dep = -1;
      for (unsigned int j = 0; j < o3rob_size; j++) {
        int rob_idx_br = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
        const O3ROB & o3rob_br = o3rob[rob_idx_br];
        if (o3rob_br.state == o3irs_completed && o3rob_br.ready_time <= curr_time) continue;
        if (o3rob_br.branch_miss == true) {
          branch_dep = rob_idx_br;
          break;
        }
      }
      // register dependency resolution:
      // We assume that false dependencies are resolved by register renaming.
      int32_t rr0 = -1;
      int32_t rr1 = -1;
      int32_t rr2 = -1;
      int32_t rr3 = -1;
      for (unsigned int j = 0; j < o3rob_size; j++) {
        int rob_idx_r = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
        const O3ROB & o3rob_r = o3rob[rob_idx_r];
        if (o3rob_r.state == o3irs_completed && o3rob_r.ready_time <= curr_time) continue;
        if (rr0 == -1 && IsRegDep(o3q_entry.rr0, o3rob_r)) {
          rr0 = rob_idx_r;
        }
        if (rr1 == -1 && IsRegDep(o3q_entry.rr1, o3rob_r)) {
          rr1 = rob_idx_r;
        }
        if (rr2 == -1 && IsRegDep(o3q_entry.rr2, o3rob_r)) {
          rr2 = rob_idx_r;
        }
        if (rr3 == -1 && IsRegDep(o3q_entry.rr3, o3rob_r)) {
          rr3 = rob_idx_r;
        }
      }

      // fill ROB
      geq->add_event(curr_time + process_interval, this);
      for (unsigned int k = 0; k < 4; ++k) {  // 0: raddr, 1: raddr2, 2: waddr, 3: no-mem
        O3ROB & o3rob_entry    = o3rob[rob_idx];
        o3rob_entry.state      = o3irs_issued;
        o3rob_entry.ready_time = curr_time + process_interval;
        o3rob_entry.ip         = o3q_entry.ip;
        int32_t mem_dep        = -1;
        uint64_t memaddr       = 0;

        if (k == 0) {
          if (o3q_entry.raddr == 0) {
            continue;
          } else {
            memaddr = o3q_entry.raddr;
          }
        } else if (k == 1) {
          if (o3q_entry.raddr2 == 0) {
            continue;
          } else {
            memaddr = o3q_entry.raddr2;
          }
        } else if (k == 2) {
          if (o3q_entry.waddr == 0) {
            continue;
          } else {
            memaddr = o3q_entry.waddr;
          }
        } else if (o3q_entry.raddr != 0 || o3q_entry.raddr2 != 0 || o3q_entry.waddr != 0) {
          break;
        }

        if (k < 3) {
          for (unsigned int j = 0; j < o3rob_size; j++) {
            int rob_idx_dep = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
            const O3ROB & o3rob_dep = o3rob[rob_idx_dep];
            if ((o3rob_dep.state != o3irs_completed || o3rob_dep.ready_time > curr_time) &&
                (o3rob_dep.memaddr >> word_log) == (memaddr >> word_log)) {
              mem_dep = rob_idx_dep;
              break;
            }
          }
        }
        o3rob_entry.memaddr    = memaddr;
        o3rob_entry.branch_miss = branch_miss;
        o3rob_entry.isread     = (k < 2);
        o3rob_entry.mem_dep    = mem_dep;
        o3rob_entry.instr_dep  = instr_dep;
        o3rob_entry.branch_dep = branch_dep;
        o3rob_entry.type       = o3q_entry.type;
        o3rob_entry.rr0        = (int32_t)rr0;
        o3rob_entry.rr1        = (int32_t)rr1;
        o3rob_entry.rr2        = (int32_t)rr2;
        o3rob_entry.rr3        = (int32_t)rr3;
        o3rob_entry.rw0        = o3q_entry.rw0;
        o3rob_entry.rw1        = o3q_entry.rw1;
        o3rob_entry.rw2        = o3q_entry.rw2;
        o3rob_entry.rw3        = o3q_entry.rw3;
        instr_dep              = rob_idx;
        rob_idx = (rob_idx + 1) % o3rob_max_size;
        o3rob_size++;
      }

      o3queue_size--;
      o3q_entry.state = o3iqs_invalid;
      o3queue_head    = (o3queue_head + 1) % o3queue_max_size;
    }
  }

  // process o3rob

  // check if ready to execute
  int32_t num_alu  = 0;
  int32_t num_ldst = 0;
  int32_t num_ld   = 0;
  int32_t num_st   = 0;
  int32_t num_sse  = 0;
  for (unsigned int i = 0; i < o3rob_size; i++) {
    int rob_idx = (o3rob_head + i) % o3rob_max_size;
    O3ROB & o3rob_entry = o3rob[rob_idx];

    if (o3rob_entry.state == o3irs_issued && o3rob_entry.ready_time <= curr_time) {
      if (o3rob_entry.type == ins_notify) {
        if (i == 0) {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time + barrier_t;
          geq->add_event(o3rob_entry.ready_time, this);
          mcsim->is_migrate_ready[o3rob_entry.rw0] = true;
        } else {
          geq->add_event(curr_time + process_interval, this);
          break;
        }
      } else if (o3rob_entry.type == ins_waitfor) {
        if (i == 0 && mcsim->is_migrate_ready[o3rob_entry.rw0] == true) {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time + process_interval;
          geq->add_event(o3rob_entry.ready_time, this);
          mcsim->is_migrate_ready[o3rob_entry.rw0] = false;
        } else {
          geq->add_event(curr_time + process_interval, this);
          break;
        }
      } else if (o3rob_entry.mem_dep == -1    && o3rob_entry.instr_dep == -1 &&
                 o3rob_entry.branch_dep == -1 &&
                 o3rob_entry.rr0 == -1        && o3rob_entry.rr1 == -1 &&
                 o3rob_entry.rr2 == -1        && o3rob_entry.rr3 == -1) {
        if (o3rob_entry.memaddr == 0 && num_alu < max_alu &&
            (o3rob_entry.type != ins_x87 || num_sse < max_sse)) {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time +
              ((o3rob_entry.type == ins_lock) ? lock_t :
               (o3rob_entry.type == ins_unlock) ? unlock_t :
               (o3rob_entry.type == ins_barrier) ? barrier_t :
               (o3rob_entry.type == ins_x87) ? sse_t :
               (o3rob_entry.branch_miss == true) ? (branch_miss_penalty + process_interval) :
               process_interval);
          geq->add_event(o3rob_entry.ready_time, this);
          num_alu++;
          num_sse += ((o3rob_entry.type == ins_x87) ? 1 : 0);
        } else if (o3rob_entry.memaddr != 0 && num_ldst < max_ldst &&
                   ((o3rob_entry.isread == true && num_ld < max_ld) ||
                    (o3rob_entry.isread == false && num_st < max_st))) {
          o3rob_entry.state = o3irs_executing;
          auto lqe = new LocalQueueElement(this, et_tlb_rd, o3rob_entry.memaddr, num);
          lqe->rob_entry = rob_idx;
          if (bypass_tlb == true) {
            lqe->type = (o3rob_entry.isread == true) ? et_read : et_write;
            cachel1d->add_req_event(curr_time + lsu_to_l1d_t, lqe);
          } else {
            tlbl1d->add_req_event(curr_time + lsu_to_l1d_t, lqe);
          }
          num_ldst++;
          (o3rob_entry.isread == true) ? num_ld++ : num_st++;
        }
      } else {
        if (mimick_inorder == true) {
          geq->add_event(curr_time + process_interval, this);
          break;
        }
      }
    } else {
      if (mimick_inorder == true) {
        geq->add_event(curr_time + process_interval, this);
        break;
      }
    }
  }

  for (unsigned int i = 0; i < o3rob_size; i++) {
    int rob_idx = (o3rob_head + i) % o3rob_max_size;
    O3ROB & o3rob_entry = o3rob[rob_idx];
    bool already_add_event = false;

    if (o3rob_entry.state == o3irs_completed && o3rob_entry.ready_time == curr_time) {
      if (already_add_event == false) {
        already_add_event = true;
        geq->add_event(curr_time + process_interval, this);
      }
      for (unsigned int j = i+1; j < o3rob_size; j++) {
        int next_idx = (o3rob_head + j) % o3rob_max_size;
        O3ROB & o3rob_next = o3rob[next_idx];
        if (o3rob_next.state != o3irs_issued) continue;
        if (o3rob_next.mem_dep    == rob_idx) o3rob_next.mem_dep    = -1;
        if (o3rob_next.instr_dep  == rob_idx) o3rob_next.instr_dep  = -1;
        if (o3rob_next.branch_dep == rob_idx) o3rob_next.branch_dep = -1;
        if (o3rob_next.rr0        == rob_idx) o3rob_next.rr0        = -1;
        if (o3rob_next.rr1        == rob_idx) o3rob_next.rr1        = -1;
        if (o3rob_next.rr2        == rob_idx) o3rob_next.rr2        = -1;
        if (o3rob_next.rr3        == rob_idx) o3rob_next.rr3        = -1;
      }
    }
  }

  // check if ready to commit
  for (unsigned int i = 0; i < max_commit_width && o3rob_size > 0; i++) {
    O3ROB & o3rob_entry = o3rob[o3rob_head];

    if (o3rob_entry.state == o3irs_completed && o3rob_entry.ready_time <= curr_time) {
      o3rob_entry.state = o3irs_invalid;
      o3rob_size--;
      o3rob_head = (o3rob_head + 1) % o3rob_max_size;
    }
  }
  if (o3rob_size > 0 && o3rob[o3rob_head].state == o3irs_completed && o3rob[o3rob_head].ready_time <= curr_time) {
    geq->add_event(curr_time + process_interval, this);
  }

  /* if (o3rob_size != 0) {
    geq->add_event(curr_time + process_interval, this);
  } */
  return num_hthreads;
}


// a hack to differentiate I$ and D$ accesses -- NACK is supported
void O3Core::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  if (local_event->type == et_tlb_rd) {
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    latest_ip         = local_event->address;
    local_event->type = et_read;
    cachel1i->add_req_event(event_time + lsu_to_l1i_t, local_event);
  } else if (local_event->type == et_nack) {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type = et_read;
    cachel1i->add_req_event(event_time + lsu_to_l1i_t + spinning_slowdown*process_interval, local_event);

    if (num_consecutive_nacks > consecutive_nack_threshold) {
      displayO3Queue();
      displayO3ROB();
      local_event->display();
      geq->display();
      LOG(FATAL) << " " << num << ", latest_ip = 0x" << std::hex << latest_ip << std::dec << std::endl;
    }
  } else {
    uint64_t aligned_event_time = ceil_by_y(event_time, process_interval);
    geq->add_event(aligned_event_time, this);
    num_consecutive_nacks = 0;

    // move the state of O3Queue entries from being_loaded to ready
    for (unsigned int i = 0; i < o3queue_size; i++) {
      unsigned int idx = (i + o3queue_head) % o3queue_max_size;
      if (o3queue[idx].state == o3iqs_being_loaded &&
          local_event->address >> cachel1i->set_lsb == o3queue[idx].ip >> cachel1i->set_lsb) {
        o3queue[idx].state      = o3iqs_ready;
        o3queue[idx].ready_time = aligned_event_time;
      }
    }
    mcsim->update_os_page_req_dist(local_event->address);
    delete local_event;
  }
}


void O3Core::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  if (local_event->type == et_tlb_rd) {
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type = (o3rob[local_event->rob_entry].isread == true) ? et_read : et_write;
    cachel1d->add_req_event(event_time + lsu_to_l1d_t, local_event);
  } else if (local_event->type == et_nack) {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type = (o3rob[local_event->rob_entry].isread == true) ? et_read : et_write;
    cachel1d->add_req_event(event_time + lsu_to_l1d_t + spinning_slowdown*process_interval, local_event);

    if (num_consecutive_nacks > consecutive_nack_threshold) {
      displayO3Queue();
      displayO3ROB();
      local_event->display();
      geq->display();
      LOG(FATAL) << " " << num << ", latest_ip = 0x" << std::hex << latest_ip << std::dec << std::endl;
    }
  } else {
    uint64_t aligned_event_time = ceil_by_y(event_time, process_interval);
    geq->add_event(aligned_event_time, this);
    num_consecutive_nacks = 0;

    // move the state of O3ROB entries from execute to complete
    CHECK(local_event->rob_entry >= 0 && local_event->rob_entry < (int32_t)o3rob_max_size);
    O3ROB & o3rob_entry    = o3rob[local_event->rob_entry];
    if (o3rob_entry.isread) {
      total_mem_rd_time += (aligned_event_time - o3rob_entry.ready_time);
    } else {
      total_mem_wr_time += (aligned_event_time - o3rob_entry.ready_time);
    }
    o3rob_entry.state      = o3irs_completed;
    o3rob_entry.ready_time = aligned_event_time + ((o3rob_entry.branch_miss == true) ? branch_miss_penalty : 0);
    geq->add_event(o3rob_entry.ready_time, this);
    mcsim->update_os_page_req_dist(local_event->address);
    delete local_event;
  }
}


bool O3Core::is_private(ADDRINT addr) {
  // currently only memory accesses in a stack are treated as a private access
  return (addr >= stack && addr < stack + stacksize);
}


void O3Core::displayO3Queue() {
  std::cout << "  OOO[" << std::setw(3) << num << "]Q: head=" << o3queue_head << ", size=" << o3queue_size << std::endl;
  for (unsigned int i = 0; i < o3queue_max_size; i++) {
    O3Queue & o3q_entry = o3queue[i];
    std::cout << "  - " << std::setw(3) << i << ": " << o3q_entry.state << ", "
         << std::dec << o3q_entry.ready_time << ", 0x"
         << std::hex << o3q_entry.waddr << ", 0x" << o3q_entry.wlen << ", 0x" << o3q_entry.raddr << ", 0x" << o3q_entry.raddr2 << ", 0x" << o3q_entry.rlen << ", 0x"
         << o3q_entry.ip << ", " << o3q_entry.type << ": " << std::dec
         << o3q_entry.rr0 << ", " << o3q_entry.rr1 << ", " << o3q_entry.rr2 << ", " << o3q_entry.rr3 << ", "
         << o3q_entry.rw0 << ", " << o3q_entry.rw1 << ", " << o3q_entry.rw2 << ", " << o3q_entry.rw3 << std::dec << std::endl;
  }
}


void O3Core::displayO3ROB() {
  std::cout << "  OOO[" << std::setw(3) << num << "]R: head=" << o3rob_head << ", size=" << o3rob_size << std::endl;
  for (unsigned int i = 0; i < o3rob_max_size; i++) {
    O3ROB & o3rob_entry = o3rob[i];
    std::cout << "  - " << std::setw(3) << i << ": " << o3rob_entry.state << ", "
         << std::dec << o3rob_entry.ready_time << ", 0x"
         << std::hex << o3rob_entry.ip << ", 0x" << o3rob_entry.memaddr << ", " << o3rob_entry.isread << ", "
         << o3rob_entry.branch_miss << ": 0x" << std::dec << o3rob_entry.mem_dep << ", 0x"
         << o3rob_entry.instr_dep << ", 0x" << o3rob_entry.branch_dep << ", " << o3rob_entry.type << ": 0x"
         << o3rob_entry.rr0 << ", 0x" << o3rob_entry.rr1 << ", 0x" << o3rob_entry.rr2 << ", 0x" << o3rob_entry.rr3 << ", 0x"
         << o3rob_entry.rw0 << ", 0x" << o3rob_entry.rw1 << ", 0x" << o3rob_entry.rw2 << ", 0x" << o3rob_entry.rw3 << std::dec << std::endl;
  }
}


bool O3Core::IsRegDep(uint32_t rr, const O3ROB & o3rob_) {
  return (rr != 0) && (rr == o3rob_.rw0 || rr == o3rob_.rw1 || rr == o3rob_.rw2 || rr == o3rob_.rw3);
}

}  // namespace PinPthread
