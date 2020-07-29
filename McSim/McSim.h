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

static const ADDRINT search_addr   = 0x800e7fffde77a040;
static const uint32_t max_hthreads = 1024;

namespace PinPthread {
class Core;
class O3Core;
class Hthread;
class CacheL1;
class CacheL2;
class Directory;
class Crossbar;
class MemoryController;
class BranchPredictor;
class TLBL1;
class TLBL2;
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

  std::pair<uint32_t, uint64_t> resume_simulation(bool must_switch);
  // return value -- whether we have to resume simulation
  uint32_t add_instruction(
      uint32_t hthreadid_,
      uint64_t curr_time_,
      uint64_t waddr,
      UINT32   wlen,
      uint64_t raddr,
      uint64_t raddr2,
      UINT32   rlen,
      uint64_t ip,
      uint32_t category,
      bool     isbranch,
      bool     isbranchtaken,
      bool     islock,
      bool     isunlock,
      bool     isbarrier,
      uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
      uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3);
  void link_thread(int32_t pth_id, bool * active_, int32_t * spinning_,
      ADDRINT * stack_, ADDRINT * stacksize_);
  void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
  void set_active(int32_t pth_id, bool is_active);

  PthreadTimingSimulator   * pts;
  bool     skip_all_instrs;
  bool     simulate_only_data_caches;
  bool     show_l2_stat_per_interval;
  bool     is_race_free_application;
  uint32_t max_acc_queue_size;
  uint32_t num_hthreads;
  uint64_t print_interval;
  bool     use_o3core;

  std::vector<Core *>             cores;
  std::vector<Hthread *>          hthreads;
  std::vector<O3Core *>           o3cores;
  std::vector<bool>               is_migrate_ready;  // TODO(gajh): what is this?
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

  uint32_t get_num_hthreads() const { return num_hthreads; }
  uint64_t get_curr_time() const    { return global_q->curr_time; }
  void     show_state(uint64_t);
  void     show_l2_cache_summary();

  std::map<uint64_t, uint64_t>   os_page_req_dist;
  void update_os_page_req_dist(uint64_t addr);
  uint64_t num_fetched_instrs;

  // some stat info
 private:
  uint64_t num_instrs_printed_last_time;

  uint64_t num_destroyed_cache_lines_last_time;
  uint64_t cache_line_life_time_last_time;
  uint64_t time_between_last_access_and_cache_destroy_last_time;

  uint64_t lsu_process_interval;
  uint64_t curr_time_last;
  uint64_t num_fetched_instrs_last;
  uint64_t num_mem_acc_last;
  uint64_t num_used_pages_last;
  uint64_t num_l1_acc_last;
  uint64_t num_l1_miss_last;
  uint64_t num_l2_acc_last;
  uint64_t num_l2_miss_last;
};

}  // namespace PinPthread

#endif  // MCSIM_H_

