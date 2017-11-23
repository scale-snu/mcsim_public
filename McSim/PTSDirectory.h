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

#ifndef PTS_DIRECTORY_H
#define PTS_DIRECTORY_H

#include "McSim.h"
#include <list>
#include <stack>
#include <queue>
#include <set>

namespace PinPthread
{
  class Directory : public Component
  {
    public:
      class DirEntry
      {
        public:
          coherence_state_type type;
          std::set<Component *> sharedl2;
          LocalQueueElement * pending;
          bool got_cl;  // whether the entry got a cache line during an invalidation
          bool not_in_dc;   // true if the entry is not in the directory cache
          uint32_t num_sharer;

          DirEntry() : type(cs_invalid), sharedl2(), pending(NULL), got_cl(false), not_in_dc(false), num_sharer(0) { }
      };

      Directory(component_type type_, uint32_t num_, McSim * mcsim_);
      ~Directory();

      //MemoryController * memorycontroller;  // downlink
      Component * memorycontroller;  // downlink
      CacheL2          * cachel2;           // uplink
      NoC              * crossbar;          // uplink

      const uint32_t set_lsb;
      const uint32_t num_sets;
      const uint32_t num_ways;
      const uint32_t dir_to_mc_t;
      const uint32_t dir_to_l2_t;
      const uint32_t dir_to_xbar_t;
      const uint32_t num_flits_per_packet;

      std::map<uint64_t, DirEntry> dir;
      std::vector< std::list<uint64_t> > dir_cache;
      std::vector<uint64_t> num_sharer_histogram;

      bool has_directory_cache;
      bool use_limitless;
      uint32_t limitless_broadcast_threshold;

      // stats
      uint64_t num_nack;
      uint64_t num_bypass;  // miss after miss
      uint64_t num_i_to_tr;
      uint64_t num_e_to_tr;
      uint64_t num_s_to_tr;
      uint64_t num_m_to_tr;
      uint64_t num_m_to_i;
      uint64_t num_tr_to_i;
      uint64_t num_tr_to_e;
      uint64_t num_tr_to_s;
      uint64_t num_tr_to_m;
      uint64_t num_evict;   // requests from L2 since a cache is evicted
      uint64_t num_invalidate;
      uint64_t num_from_mc;
      uint64_t num_dir_cache_access;
      uint64_t num_dir_cache_miss;
      uint64_t num_dir_cache_retry;
      uint64_t num_dir_evict;

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);
      void show_state(uint64_t);

      inline void remove_directory_cache_entry(uint32_t set, uint64_t dir_entry);
      void add_event_to_UL(uint64_t curr_time, LocalQueueElement *, bool is_data);
      void add_event_to_ULpp(uint64_t curr_time, LocalQueueElement *, bool is_data);
      void add_event_to_UL(uint64_t curr_time,
                           Component * comp, 
                           LocalQueueElement * lqe);
  };


}

#endif

