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

#ifndef MCSIM_H_
#define MCSIM_H_

#include <stdlib.h>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <utility>
#include <vector>

#include "PTS.h"
#include "PTSComponent.h"

static const ADDRINT search_addr = 0x800e7fffde77a040;
static const UINT32 max_hthreads = 1024;

namespace PinPthread {
class O3Core;
class CacheL1;
class CacheL2;
class Directory;
class Crossbar;
class MemoryController;
class BranchPredictor;
class TLBL1;
class NoC;
class McSim;


enum ins_type {
  mem_rd,
  mem_2nd_rd,
  mem_wr,
  no_mem,
  ins_branch_taken,
  ins_branch_not_taken,
  ins_lock,
  ins_unlock,
  ins_barrier,
  ins_x87,
  ins_notify,   // for thread migration
  ins_waitfor,  // for thread migration
  ins_invalid
};

enum coherence_state_type {
  cs_invalid,
  cs_shared,
  cs_exclusive,
  cs_modified,
  cs_tr_to_i,  // will be the invalid state
  cs_tr_to_s,  // will be the shared state
  cs_tr_to_m,  // will be the modified state
  cs_tr_to_e,
  cs_m_to_s    // modified -> shared
};



class McSim {
 public:
  explicit McSim(PthreadTimingSimulator * pts_);
  ~McSim();

  std::pair<UINT32, UINT64> resume_simulation(bool must_switch);
  // return value -- whether we have to resume simulation
  
  UINT32 add_instruction(
    UINT32 hthreadid_,
    UINT64 curr_time_,
    UINT64 waddr,
    UINT32 wlen,
    UINT64 raddr,
    UINT64 raddr2,
    UINT32 rlen,
    UINT64 ip,
    UINT32 category,
    bool   isbranch,
    bool   isbranchtaken,
    bool   islock,
    bool   isunlock,
    bool   isbarrier,
    UINT32 rr0, UINT32 rr1, UINT32 rr2, UINT32 rr3,
    UINT32 rw0, UINT32 rw1, UINT32 rw2, UINT32 rw3);

  void link_thread(INT32 pth_id, bool * active_, INT32 * spinning_,
    ADDRINT * stack_, ADDRINT * stacksize_);

  void set_stack_n_size(INT32 pth_id, ADDRINT stack, ADDRINT stacksize);
  void set_active(INT32 pth_id, bool is_active);

  PthreadTimingSimulator * pts;
  bool   skip_all_instrs;
  bool   simulate_only_data_caches;
  bool   show_l2_stat_per_interval;
  bool   is_race_free_application;
  UINT32 max_acc_queue_size;
  UINT32 num_hthreads;
  UINT64 print_interval;

  std::vector<bool>               is_migrate_ready;  // for thread migration

  std::vector<O3Core *>           o3cores;
  std::vector<CacheL1 *>          l1ds;
  std::vector<CacheL1 *>          l1is;
  std::vector<CacheL2 *>          l2s;
  NoC *                           noc;
  std::vector<Directory *>        dirs;
  std::vector<MemoryController *> mcs;
  GlobalEventQueue *              global_q;
  std::vector<TLBL1 *>            tlbl1ds;
  std::vector<TLBL1 *>            tlbl1is;
  std::list<Component *>          comps;

  UINT32 get_num_hthreads() const { return num_hthreads; }
  UINT64 get_curr_time()    const { return global_q->curr_time; }
  void   show_state(UINT64);
  void   show_l2_cache_summary();

  std::map<UINT64, UINT64>  os_page_req_dist;
  void update_os_page_req_dist(UINT64 addr);
  UINT64 num_fetched_instrs;

  // some stat info
 private:
  UINT64 num_instrs_printed_last_time;

  UINT64 num_destroyed_cache_lines_last_time;
  UINT64 cache_line_life_time_last_time;
  UINT64 time_between_last_access_and_cache_destroy_last_time;

  UINT64 lsu_process_interval;
  UINT64 curr_time_last;
  UINT64 num_fetched_instrs_last;
  UINT64 num_mem_acc_last;
  UINT64 num_used_pages_last;
  UINT64 num_l1_acc_last;
  UINT64 num_l1_miss_last;
  UINT64 num_l2_acc_last;
  UINT64 num_l2_miss_last;
};

}  // namespace PinPthread

#endif  // MCSIM_H_

