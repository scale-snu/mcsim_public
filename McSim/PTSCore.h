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

#ifndef __PTSCORE_H__
#define __PTSCORE_H__

#include "PTS.h"
#include "PTSComponent.h"


using namespace std;

namespace PinPthread
{
  class Hthread;
  class BranchPredictor;


  class Core : public Component
  {
    public:
      Core(component_type type_, uint32_t num_, McSim * mcsim_);
      ~Core();
      uint32_t process_event(uint64_t curr_time);

      const uint32_t num_hthreads;

      std::vector<Hthread *> hthreads;
      std::vector<bool>      is_active;

      uint32_t lastly_served_thread_num;
      uint64_t last_time_no_mem_served;
      uint64_t last_time_mem_served;
      uint64_t num_bubbled_slots;
  };



  // Hthread includes the functionality of load store unit
  class Hthread : public Component  // hardware thread
  {
    public:
      Hthread(component_type type_, uint32_t num_, McSim * mcsim_);
      ~Hthread();
      uint32_t process_event(uint64_t curr_time);
      void     add_req_event(uint64_t, LocalQueueElement *, Component * from);
      void     add_rep_event(uint64_t, LocalQueueElement *, Component * from);

      Core    * core;
      CacheL1 * cachel1d;
      CacheL1 * cachel1i;
      TLBL1   * tlbl1d;
      TLBL1   * tlbl1i;
      BranchPredictor * bp;

      // pointer to the member variables in the corresponding Pthread object
      bool active;
      int32_t * spinning;
      ADDRINT stack;
      ADDRINT stacksize;

      uint32_t num_hthreads;
      //pthread_queue_t::iterator current; // pointer to the current thread running
      bool     tlb_rd;
      uint64_t resume_time;
      uint64_t mem_time;

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
      const uint32_t branch_miss_penalty;
      const uint32_t spinning_slowdown;
      bool           bypass_tlb;
      const uint32_t lock_t;
      const uint32_t unlock_t;
      const uint32_t barrier_t;
      const uint32_t consecutive_nack_threshold;
      bool           display_barrier;
      bool           was_nack;

      std::queue< std::pair<ins_type, uint64_t> > mem_acc;
      uint64_t  latest_ip;
      uint64_t  latest_bmp_time;  // latest branch miss prediction time

      bool      is_private(ADDRINT);
  };



  class BranchPredictor
  {
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
}

#endif

