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
#include "PTSComponent.h"
#include "PTSCore.h"
#include "PTSO3Core.h"
#include "PTSTLB.h"
#include "PTSXbar.h"
#include "PTSDirectory.h"
#include "PTSMemoryController.h"
#include <assert.h>
#include <glog/logging.h>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "xed-category-enum.h"
}

// using namespace PinPthread;

namespace PinPthread {

std::ostream& operator<<(std::ostream & output, component_type ct) {
  switch (ct) {
    case ct_core:      output << "ct_core"; break;
    case ct_lsu:       output << "ct_lsu"; break;
    case ct_o3core:    output << "ct_o3core"; break;
    case ct_cachel1d:  output << "ct_l1d$"; break;
    case ct_cachel1i:  output << "ct_l1i$"; break;
    case ct_cachel2:   output << "ct_l2$"; break;
    case ct_directory: output << "ct_dir"; break;
    case ct_crossbar:  output << "ct_xbar"; break;
    case ct_memory_controller: output << "ct_mc"; break;
    case ct_tlbl1d:    output << "ct_tlbl1d"; break;
    case ct_tlbl1i:    output << "ct_tlbl1i"; break;
    case ct_tlbl2:     output << "ct_tlbl2"; break;
    case ct_mesh:      output << "ct_mesh"; break;
    case ct_ring:      output << "ct_ring"; break;
    default: output << ct; break;
  }
  return output;
}



std::ostream & operator << (std::ostream & output, event_type et) {
  switch (et) {
    case et_read:       output << "et_rd"; break;
    case et_write:      output << "et_wr"; break;
    case et_write_nd:   output << "et_wrnd"; break;
    case et_evict:      output << "et_ev"; break;
    case et_evict_nd:   output << "et_evnd"; break;
    case et_dir_evict:  output << "et_dir_ev"; break;
    case et_e_to_m:     output << "et_e_to_m"; break;
    case et_e_to_i:     output << "et_e_to_i"; break;
    case et_e_to_s:     output << "et_es"; break;
    case et_e_to_s_nd:  output << "et_esnd"; break;
    case et_m_to_s:     output << "et_ms"; break;
    case et_m_to_m:     output << "et_mm"; break;
    case et_m_to_i:     output << "et_mi"; break;
    case et_s_to_s:     output << "et_ss"; break;
    case et_s_to_s_nd:  output << "et_ssnd"; break;
    case et_m_to_e:     output << "et_me"; break;
    case et_dir_rd:     output << "et_dr"; break;
    case et_dir_rd_nd:  output << "et_drnd"; break;
    case et_e_rd:       output << "et_er"; break;
    case et_s_rd:       output << "et_sr"; break;
    case et_nack:       output << "et_na"; break;
    case et_invalidate: output << "et_in"; break;
    case et_invalidate_nd: output << "et_innd"; break;
    case et_s_rd_wr:    output << "et_rw"; break;
    case et_rd_bypass:  output << "et_rb"; break;
    case et_tlb_rd:     output << "et_tr"; break;
    case et_i_to_e:     output << "et_ie"; break;
    case et_rd_dir_info_req: output << "et_rdiq"; break;
    case et_rd_dir_info_rep: output << "et_rdip"; break;
    case et_nop:        output << "et_nop"; break;
    default: break;
  }
  return output;
}



std::ostream & operator << (std::ostream & output, coherence_state_type cs) {
  switch (cs) {
    case cs_invalid:   output << "cs_invalid"; break;
    case cs_shared:    output << "cs_shared"; break;
    case cs_exclusive: output << "cs_exclusive"; break;
    case cs_modified:  output << "cs_modified"; break;
    case cs_tr_to_i:   output << "cs_tr_to_i"; break;
    case cs_tr_to_s:   output << "cs_tr_to_s"; break;
    case cs_tr_to_m:   output << "cs_tr_to_m"; break;
    case cs_tr_to_e:   output << "cs_tr_to_e"; break;
    case cs_m_to_s:    output << "cs_m_to_s"; break;
    default: break;
  }
  return output;
}



std::ostream & operator << (std::ostream & output, ins_type it) {
  switch (it) {
    case mem_rd:      output << "mem_rd"; break;
    case mem_2nd_rd:  output << "mem_2nd_rd"; break;
    case mem_wr:      output << "mem_wr"; break;
    case no_mem:      output << "no_mem"; break;
    case ins_branch_taken:     output << "ins_branch_taken"; break;
    case ins_branch_not_taken: output << "ins_branch_not_taken"; break;
    case ins_lock:    output << "ins_lock"; break;
    case ins_unlock:  output << "ins_unlock"; break;
    case ins_barrier: output << "ins_barrier"; break;
    case ins_x87:     output << "ins_x87"; break;
    case ins_notify:  output << "ins_notify"; break;
    case ins_waitfor: output << "ins_waitfor"; break;
    case ins_invalid: output << "ins_invalid"; break;
    default: break;
  }
  return output;
}



McSim::McSim(PthreadTimingSimulator * pts_)
  :pts(pts_),
  skip_all_instrs(pts_->get_param_bool("pts.skip_all_instrs", false)),
  simulate_only_data_caches(pts_->get_param_bool("pts.simulate_only_data_caches", false)),
  show_l2_stat_per_interval(pts_->get_param_bool("pts.show_l2_stat_per_interval", false)),
  is_race_free_application(pts_->get_param_bool("pts.is_race_free_application", true)),
  max_acc_queue_size(pts_->get_param_uint64("pts.max_acc_queue_size", 1000)),
  cores(), hthreads(), l1ds(), l1is(), l2s(), dirs(), mcs(), tlbl1ds(), tlbl1is(), comps(),
  num_fetched_instrs(0), num_instrs_printed_last_time(0),
  num_destroyed_cache_lines_last_time(0), cache_line_life_time_last_time(0),
  time_between_last_access_and_cache_destroy_last_time(0) {
  global_q      = new GlobalEventQueue(this);
  num_hthreads  = pts->get_param_uint64("pts.num_hthreads", max_hthreads);
  use_o3core    = pts->get_param_bool("pts.use_o3core", false);

  uint32_t num_threads_per_l1_cache   = pts->get_param_uint64("pts.num_hthreads_per_l1$", 4);
  assert(use_o3core == false || num_threads_per_l1_cache == 1);
  uint32_t num_l1_caches_per_l2_cache = pts->get_param_uint64("pts.num_l1$_per_l2$", 2);
  uint32_t num_mcs                    = pts->get_param_uint64("pts.num_mcs", 2);
  print_interval                      = pts->get_param_uint64("pts.print_interval", 1000000);
  std::string   noc_type(pts->get_param_str("pts.noc_type"));

  // for stats
  if (use_o3core) {
    lsu_process_interval = pts->get_param_uint64("pts.o3core.process_interval", 10);
  } else {
    lsu_process_interval = pts->get_param_uint64("pts.lsu.process_interval",    10);
  }
  curr_time_last          = 0;
  num_fetched_instrs_last = 0;
  num_mem_acc_last        = 0;
  num_used_pages_last     = 0;
  num_l1_acc_last         = 0;
  num_l1_miss_last        = 0;
  num_l2_acc_last         = 0;
  num_l2_miss_last        = 0;
  num_dependency_distance_last = 0;

  if (noc_type != "mesh" && noc_type != "ring" &&
      num_mcs * num_l1_caches_per_l2_cache * num_threads_per_l1_cache > num_hthreads) {
    LOG(FATAL) << "the # of memory controllers must not be larger than the # of L2 caches\n";
  }

  for (uint32_t i = 0; i < num_hthreads; i++) {
    if (use_o3core == true) {
      o3cores.push_back(new O3Core(ct_o3core, i, this));
      is_migrate_ready.push_back(false);
    } else {
      hthreads.push_back(new Hthread(ct_lsu, i, this));
    }
  }

  // instantiate L1 caches
  for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++) {
    if (use_o3core == false) {
      cores.push_back(new Core(ct_core, i, this));
    }
    l1is.push_back(new CacheL1(ct_cachel1i, i, this));
    l1ds.push_back(new CacheL1(ct_cachel1d, i, this));
    tlbl1ds.push_back(new TLBL1(ct_tlbl1d, i, this));
    tlbl1is.push_back(new TLBL1(ct_tlbl1i, i, this));
  }
  if (num_hthreads % num_threads_per_l1_cache != 0) {
    if (use_o3core == false) {
      cores.push_back(cores[0]);
    }
    l1is.push_back(l1is[0]);
    l1ds.push_back(l1ds[0]);
    tlbl1ds.push_back(tlbl1ds[0]);
    tlbl1is.push_back(tlbl1is[0]);
  }

  // connect hthreads and l1s
  for (uint32_t i = 0; i < num_hthreads; i++) {
    if (use_o3core == true) {
      o3cores[i]->cachel1i = (l1is[i]);
      l1is[i]->lsus.push_back(o3cores[i]);
      o3cores[i]->cachel1d = (l1ds[i]);
      l1ds[i]->lsus.push_back(o3cores[i]);
      o3cores[i]->tlbl1d   = (tlbl1ds[i]);
      tlbl1ds[i]->lsus.push_back(o3cores[i]);
      o3cores[i]->tlbl1i   = (tlbl1is[i]);
      tlbl1is[i]->lsus.push_back(o3cores[i]);
    } else {
      hthreads[i]->core     = (cores[i/num_threads_per_l1_cache]);
      cores[i/num_threads_per_l1_cache]->hthreads.push_back(hthreads[i]);
      cores[i/num_threads_per_l1_cache]->is_active.push_back(false);
      hthreads[i]->cachel1i = (l1is[i/num_threads_per_l1_cache]);
      l1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      hthreads[i]->cachel1d = (l1ds[i/num_threads_per_l1_cache]);
      l1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      hthreads[i]->tlbl1d   = (tlbl1ds[i/num_threads_per_l1_cache]);
      tlbl1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      hthreads[i]->tlbl1i   = (tlbl1is[i/num_threads_per_l1_cache]);
      tlbl1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
    }
  }

  // instantiate L2 caches
  for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++) {
    l2s.push_back(new CacheL2(ct_cachel2, i, this));
  }

  // connect l1s and l2s
  for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++) {
    l1is[i]->cachel2 = l2s[i/num_l1_caches_per_l2_cache];
    l1ds[i]->cachel2 = l2s[i/num_l1_caches_per_l2_cache];
    l2s[i/num_l1_caches_per_l2_cache]->cachel1i.push_back(l1is[i]);
    l2s[i/num_l1_caches_per_l2_cache]->cachel1d.push_back(l1ds[i]);
  }

  if (noc_type == "mesh" || noc_type == "ring") {
    if (noc_type == "mesh") {
      noc = new Mesh2D(ct_mesh, 0, this);
    } else {
      noc = new Ring(ct_ring, 0, this);
    }
    Directory * dummy_dir = new Directory(ct_directory, num_mcs, this);

    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++) {
      l2s[i]->directory = dummy_dir;
      l2s[i]->crossbar  = noc;
      noc->cachel2.push_back(l2s[i]);
    }

    for (uint32_t i = 0; i < num_mcs; i++) {
      mcs.push_back(new MemoryController(ct_memory_controller, i, this));
      dirs.push_back(new Directory(ct_directory, i, this));
      dirs[i]->memorycontroller = (mcs[i]);
      mcs[i]->directory = (dirs[i]);
      noc->directory.push_back(dirs[i]);
      dirs[i]->cachel2  = NULL;
      dirs[i]->crossbar = noc;
    }
  } else {
    noc = new Crossbar(ct_crossbar, 0, this, num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache);

    // instantiate directories
    // currently it is assumed that (# of MCs) == (# of L2$s) == (# of directories)
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++) {
      mcs.push_back(new MemoryController(ct_memory_controller, i, this));
      dirs.push_back(new Directory(ct_directory, i, this));
      dirs[i]->memorycontroller = (mcs[i]);
      mcs[i]->directory = (dirs[i]);

      l2s[i]->directory = (dirs[i]);
      l2s[i]->crossbar  = noc;
      noc->directory.push_back(dirs[i]);
      noc->cachel2.push_back(l2s[i]);
      dirs[i]->cachel2  = (l2s[i]);
      dirs[i]->crossbar = noc;
    }
  }
  }


McSim::~McSim() {
  uint64_t ipc1000 = (global_q->curr_time == 0) ? 0 : (1000 * num_fetched_instrs * lsu_process_interval / global_q->curr_time);
  std::cout << "  -- total number of fetched instructions : " << num_fetched_instrs
    << " (IPC = " << std::setw(3) << ipc1000/1000 << "." << std::setfill('0') << std::setw(3) << ipc1000%1000 << std::setfill(' ') << ")" << std::endl;

  for (auto && el : hthreads) delete el;
  for (auto && el : o3cores) delete el;
  for (auto && el : l1is) delete el;
  for (auto && el : l1ds) delete el;
  for (auto && el : l2s) delete el;
  for (auto && el : dirs) delete el;
  for (auto && el : mcs) delete el;
  delete noc;

  delete global_q;
}


void McSim::show_state(uint64_t addr) {
  for (auto && el : comps) el->show_state(addr);
}


std::pair<uint32_t, uint64_t> McSim::resume_simulation(bool must_switch) {
  std::pair<uint32_t, uint64_t> ret_val;  // <thread_id, time>

  if (/*must_switch == true &&*/ global_q->event_queue.empty()) {
    bool any_resumable_thread = false;
    if (use_o3core == true) {
      for (uint32_t i = 0; i < o3cores.size(); i++) {
        O3Core * o3core = o3cores[i];
        if (o3core->active == true) {
          ret_val.first  = o3core->num;
          ret_val.second = global_q->curr_time;
          i = o3cores.size();
          any_resumable_thread = true;
          break;
        }
      }
    } else {
      for (uint32_t i = 0; i < cores.size(); i++) {
        Core * core = cores[i];
        for (uint32_t j = 0; j < core->hthreads.size(); j++) {
          Hthread * hthread = core->hthreads[j];
          if (/*hthread->mem_acc.empty() == true && core->is_active[j] == true &&*/
              hthread != NULL && hthread->active == true) {
            ret_val.first  = hthread->num;
            ret_val.second = global_q->curr_time;
            // core->is_active[j] = false;
            i = cores.size();
            any_resumable_thread = true;
            break;
          }
        }
      }
    }

    if (any_resumable_thread == false) {
      std::cout << global_q->curr_time << std::endl;
      for (uint32_t i = 0; i < cores.size(); i++) {
        Core * core = cores[i];
        for (uint32_t j = 0; j < core->hthreads.size(); j++) {
          Hthread * hthread = core->hthreads[j];
          if (hthread == NULL) {
            continue;
          }
          if (use_o3core == false) {
            std::cout << hthread->mem_acc.empty() << ", ";
          }
          std::cout << core->is_active[j] << ", ";
          std::cout << hthread->active << ": ";
          std::cout << hthread->resume_time << ", " << hthread->latest_bmp_time << std::endl;
        }
      }

      // cout << "  -- event queue can not be empty : cycle = " << curr_time << std::endl;
      // ASSERTX(0);
    } else {
      return ret_val;
    }
  }

  ret_val.first  = global_q->process_event();
  ret_val.second = global_q->curr_time;

  if (num_fetched_instrs / print_interval != num_instrs_printed_last_time) {
    num_instrs_printed_last_time = num_fetched_instrs / print_interval;
    std::cout << "  -- [" << std::dec << std::setw(12) << global_q->curr_time << "]: "
      << std::setw(10) << num_fetched_instrs << " instrs so far,";

    if (global_q->curr_time > curr_time_last) {
      uint64_t ipc1000 = 1000 * (num_fetched_instrs - num_fetched_instrs_last) * lsu_process_interval /
        (global_q->curr_time - curr_time_last);
      std::cout << " IPC= " << std::setw(3) << ipc1000/1000 << "." << std::setfill('0') << std::setw(3) << ipc1000%1000
        << std::setfill(' ') << ", ";
    }

    uint64_t num_l1_acc  = 0;
    uint64_t num_l1_miss = 0;
    for (unsigned int i = 0; i < l1ds.size(); i++) {
      num_l1_acc += l1ds[i]->num_rd_access + l1ds[i]->num_wr_access
        + l1is[i]->num_rd_access + l1is[i]->num_wr_access
        - l1ds[i]->num_nack - l1is[i]->num_nack;
      num_l1_miss += l1ds[i]->num_rd_miss + l1ds[i]->num_wr_miss
        + l1is[i]->num_rd_miss + l1is[i]->num_wr_miss
        - l1ds[i]->num_nack - l1is[i]->num_nack;
    }

    uint64_t num_l2_acc  = 0;
    uint64_t num_l2_miss = 0;
    for (unsigned int i = 0; i < l2s.size(); i++) {
      num_l2_acc  += l2s[i]->num_rd_access + l2s[i]->num_wr_access - l2s[i]->num_nack;
      num_l2_miss += l2s[i]->num_rd_miss + l2s[i]->num_wr_miss - l2s[i]->num_nack;
    }

    std::cout << "L1 (acc, miss)=( " << std::setw(7) << num_l1_acc - num_l1_acc_last << ", "
      << std::setw(6) << num_l1_miss - num_l1_miss_last << "), ";
    std::cout << "L2 (acc, miss)=( " << std::setw(6) << num_l2_acc - num_l2_acc_last << ", "
      << std::setw(6) << num_l2_miss - num_l2_miss_last << "), ";
    num_l1_acc_last  = num_l1_acc;
    num_l1_miss_last = num_l1_miss;
    num_l2_acc_last  = num_l2_acc;
    num_l2_miss_last = num_l2_miss;

    uint64_t num_mem_acc = 0;
    uint64_t num_used_pages = 0;

    for (unsigned int i = 0; i < mcs.size(); i++) {
      num_used_pages += mcs[i]->os_page_acc_dist_curr.size();
      num_mem_acc    += mcs[i]->num_reqs;
    }

    std::cout << std::setw(6) << num_mem_acc - num_mem_acc_last << " mem accs, ( ";
    std::cout << std::setw(4) << num_used_pages << ", ";

    num_used_pages = 0;
    for (unsigned int i = 0; i < mcs.size(); i++) {
      mcs[i]->update_acc_dist();
      num_used_pages += mcs[i]->os_page_acc_dist.size();
    }

    std::cout << std::setw(4) << num_used_pages - num_used_pages_last << ") touched pages (this time, 1stly), ";
    num_mem_acc_last    = num_mem_acc;
    num_used_pages_last = num_used_pages;

    if (o3cores.size() > 0) {
      uint64_t total_dependency_distance = 0;
      for (unsigned int i = 0; i < o3cores.size(); i++) {
        total_dependency_distance += o3cores[i]->total_dependency_distance;
      }

      if (num_fetched_instrs > num_fetched_instrs_last) {
        uint64_t dd1000 = 1000 * (total_dependency_distance - num_dependency_distance_last)/(num_fetched_instrs - num_fetched_instrs_last);
        std::cout << " avg_dd= " << std::setw(2) << dd1000/1000 << "." << std::setfill('0') << std::setw(3) << dd1000%1000
          << std::setfill(' ') << ", ";
      }
      num_dependency_distance_last = total_dependency_distance;
    }

    num_fetched_instrs_last = num_fetched_instrs;
    curr_time_last = global_q->curr_time;

    if (show_l2_stat_per_interval == true) {
      show_l2_cache_summary();
    } else {
      std::cout << std::endl;
    }
  }
  return ret_val;
}


uint32_t McSim::add_instruction(
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
    uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3) {
  // push a new event to the event queue
  uint32_t num_available_slot = 0;
  num_fetched_instrs++;

  if (use_o3core == true) {
    O3Core * o3core = o3cores[hthreadid_];

    if (o3core->o3queue_size == 0) {
      if (o3core->resume_time <= curr_time_) {
        global_q->add_event(curr_time_, o3core);
        o3core->is_active = true;
      }
    }
    ins_type type = (islock == true && isunlock == true && isbarrier == false) ? ins_notify :
      (islock == true && isunlock == true && isbarrier == true) ? ins_waitfor :
      (isbranch && isbranchtaken)        ? ins_branch_taken :
      (isbranch && !isbranchtaken)       ? ins_branch_not_taken :
      (category == XED_CATEGORY_X87_ALU || category == XED_CATEGORY_SSE) ? ins_x87 :  // treat an SSE op as an X87 op
      (islock == true)                   ? ins_lock :
      (isunlock == true)                 ? ins_unlock :
      (isbarrier == true)                ? ins_barrier : no_mem;

    o3core->num_instrs++;
    o3core->num_call_ops += (category == XED_CATEGORY_CALL) ? 1 : 0;
    if (o3core->o3queue_size >= o3core->o3queue_max_size) {
      return 0;
      LOG(ERROR) << " *_* " << curr_time_;
      o3core->displayO3Queue();
      o3core->displayO3ROB();
      global_q->display();
      ASSERTX(0);
    }
    O3Queue & o3q_entry  = o3core->o3queue[(o3core->o3queue_max_size + o3core->o3queue_head + o3core->o3queue_size)%o3core->o3queue_max_size];
    o3q_entry.state      = o3iqs_not_in_queue;
    o3q_entry.ready_time = curr_time_;
    o3q_entry.waddr      = waddr;
    o3q_entry.wlen       = wlen;
    o3q_entry.raddr      = raddr;
    o3q_entry.raddr2     = raddr2;
    o3q_entry.rlen       = rlen;
    o3q_entry.ip         = ip;
    o3q_entry.type       = type;
    o3q_entry.rr0        = rr0;
    o3q_entry.rr1        = rr1;
    o3q_entry.rr2        = rr2;
    o3q_entry.rr3        = rr3;
    o3q_entry.rw0        = rw0;
    o3q_entry.rw1        = rw1;
    o3q_entry.rw2        = rw2;
    o3q_entry.rw3        = rw3;

    o3core->o3queue_size++;

    if ((raddr != 0 && !is_race_free_application && !o3core->is_private(raddr)) ||
        (raddr != 0 && !is_race_free_application && !o3core->is_private(raddr2)) ||
        (waddr != 0 && !is_race_free_application && !o3core->is_private(waddr))) {
      num_available_slot = 0;
    }

    // if (ip != 0)
    // {
    //   cout << curr_time_ << ", " << std::hex << ip << std::dec << ", " << category;
    //   cout << ", " << xed_category_enum_t2str((xed_category_enum_t) category) << std::endl;
    // }

    // if (o3core->o3queue_size + 4 >= o3core->o3queue_max_size) resume = true;
    num_available_slot = ((o3core->o3queue_size + 4 > o3core->o3queue_max_size) ? 0 :
        (o3core->o3queue_max_size - (o3core->o3queue_size + 4)));
  } else {
    Hthread * hthread = hthreads[hthreadid_];

    if (hthread->mem_acc.empty() == true) {
      if (hthread->resume_time <= curr_time_) {
        global_q->add_event(curr_time_, hthread->core);
        if (hthread->num == hthreads.size() - 1) {
          hthread->core->is_active[hthread->core->hthreads.size() - 1] = true;
        } else {
          hthread->core->is_active[hthread->num%hthread->core->hthreads.size()] = true;
        }
      }
    }
    ins_type type = (isbranch && isbranchtaken)  ? ins_branch_taken :
      (isbranch && !isbranchtaken)       ? ins_branch_not_taken :
      (category == XED_CATEGORY_X87_ALU || category == XED_CATEGORY_SSE) ? ins_x87 :  // treat an SSE op as an X87 op
      (islock == true)                   ? ins_lock :
      (isunlock == true)                 ? ins_unlock :
      (isbarrier == true)                ? ins_barrier : no_mem;

    hthread->num_call_ops += (category == XED_CATEGORY_CALL) ? 1 : 0;
    hthread->tlb_rd    = false;
    if (simulate_only_data_caches == false) {
      hthread->mem_acc.push(std::pair<ins_type, uint64_t>(type, ip));
    }

    // if (ip != 0)
    // {
    //   cout << curr_time_ << ", " << std::hex << ip << std::dec << ", " << category;
    //   cout << ", " << xed_category_enum_t2str((xed_category_enum_t) category) << std::endl;
    // }

    if (raddr) {
      if (rlen % sizeof(uint64_t) != 0) {
        rlen = rlen - (rlen % sizeof(uint64_t)) + sizeof(uint64_t);
      }

      while (rlen != 0) {
        if (!is_race_free_application && !hthread->is_private(raddr)) num_available_slot = 0;
        hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_rd, raddr));
        raddr += sizeof(uint64_t);
        if (raddr2) {
          if (!is_race_free_application && !hthread->is_private(raddr2)) num_available_slot = 0;
          hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_rd, raddr2));
          raddr2 += sizeof(uint64_t);
        }
        rlen -= sizeof(uint64_t);
      }
    }

    if (waddr) {
      if (wlen % sizeof(uint64_t) != 0) {
        wlen = wlen - (wlen % sizeof(uint64_t)) + sizeof(uint64_t);
      }

      while (wlen != 0) {
        if (!is_race_free_application && !hthread->is_private(waddr)) num_available_slot = 0;
        hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_wr, waddr));
        waddr += sizeof(uint64_t);
        wlen  -= sizeof(uint64_t);
      }
    }

    num_available_slot = ((max_acc_queue_size <= hthread->mem_acc.size()) ? 0 :
        (max_acc_queue_size - hthread->mem_acc.size()));
  }

  return num_available_slot;
}


void McSim::set_stack_n_size(
    int32_t pth_id,
    ADDRINT stack,
    ADDRINT stacksize) {
  if (use_o3core == true) {
    o3cores[pth_id]->stack     = stack;
    o3cores[pth_id]->stacksize = stacksize;
  } else {
    hthreads[pth_id]->stack     = stack;
    hthreads[pth_id]->stacksize = stacksize;
  }
}


void McSim::set_active(int32_t pth_id, bool is_active) {
  if (use_o3core == true) {
    o3cores[pth_id]->active    = is_active;
  } else {
    hthreads[pth_id]->active   = is_active;
  }
}


void McSim::show_l2_cache_summary() {
  uint32_t num_cache_lines    = 0;
  uint32_t num_i_cache_lines  = 0;
  uint32_t num_e_cache_lines  = 0;
  uint32_t num_s_cache_lines  = 0;
  uint32_t num_m_cache_lines  = 0;
  uint32_t num_tr_cache_lines = 0;

  uint64_t num_destroyed_cache_lines = 0;
  uint64_t cache_line_life_time = 0;
  uint64_t time_between_last_access_and_cache_destroy = 0;

  for (uint32_t i = 0; i < l2s.size(); i++) {
    num_destroyed_cache_lines += l2s[i]->num_destroyed_cache_lines;
    cache_line_life_time      += l2s[i]->cache_line_life_time;
    time_between_last_access_and_cache_destroy += l2s[i]->time_between_last_access_and_cache_destroy;

    for (uint32_t j = 0; j < l2s[i]->num_sets; j++) {
      for (uint32_t k = 0; k < l2s[i]->num_ways; k++) {
        CacheL2::L2Entry * iter = l2s[i]->tags[j][k];
        switch (iter->type) {
          case cs_invalid:   num_i_cache_lines++;  break;
          case cs_exclusive: num_e_cache_lines++;  break;
          case cs_shared:    num_s_cache_lines++;  break;
          case cs_modified:  num_m_cache_lines++;  break;
          default:           num_tr_cache_lines++; break;
        }
      }
    }
  }

  num_cache_lines = num_i_cache_lines + num_e_cache_lines +
    num_s_cache_lines + num_m_cache_lines + num_tr_cache_lines;

  std::cout << " L2$ (i,e,s,m,tr) ratio=("
    << std::setiosflags(std::ios::fixed) << std::setw(4) << 1000 * num_i_cache_lines  / num_cache_lines << ", "
    << std::setiosflags(std::ios::fixed) << std::setw(4) << 1000 * num_e_cache_lines  / num_cache_lines << ", "
    << std::setiosflags(std::ios::fixed) << std::setw(4) << 1000 * num_s_cache_lines  / num_cache_lines << ", "
    << std::setiosflags(std::ios::fixed) << std::setw(4) << 1000 * num_m_cache_lines  / num_cache_lines << ", "
    << std::setiosflags(std::ios::fixed) << std::setw(4) << 1000 * num_tr_cache_lines / num_cache_lines << "), "
    << std::endl;
  /*  << "avg L2$ line life = ";

      if (num_destroyed_cache_lines == num_destroyed_cache_lines_last_time)
      {
      cout << "NaN, avg time bet last acc to L2$ destroy = NaN" << std::endl;
      }
      else
      {
      cout << setiosflags(ios::fixed) 
      << (cache_line_life_time - cache_line_life_time_last_time) /
      ((num_destroyed_cache_lines - num_destroyed_cache_lines_last_time) * l2s[0]->process_interval) << ", "
      << "avg time bet last acc to $ destroy = "
      << setiosflags(ios::fixed)
      << (time_between_last_access_and_cache_destroy - time_between_last_access_and_cache_destroy_last_time) /
      ((num_destroyed_cache_lines - num_destroyed_cache_lines_last_time) * l2s[0]->process_interval) 
      << " L2$ cycles" << std::endl;
      }*/
}


void McSim::update_os_page_req_dist(uint64_t addr) {
  if (mcs[0]->display_os_page_usage == true) {
    uint64_t page_num = addr / (1 << global_q->page_sz_base_bit);
    std::map<uint64_t, uint64_t>::iterator p_iter = os_page_req_dist.find(page_num);

    if (p_iter != os_page_req_dist.end()) {
      (p_iter->second)++;
    } else {
      os_page_req_dist.insert(std::pair<uint64_t, uint64_t>(page_num, 1));
    }
  }
}

}  // namespace PinPthread
