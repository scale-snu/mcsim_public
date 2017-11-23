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

#ifndef PTS_CACHE_H
#define PTS_CACHE_H

#include "McSim.h"
#include <list>
#include <queue>
#include <set>
#include <stack>

using namespace std;

namespace PinPthread
{

  class Cache : public Component
  {
    public:
      Cache(component_type type_, uint32_t num_, McSim * mcsim_);
      virtual ~Cache() { };

      uint32_t set_lsb;

    protected:
      uint32_t num_banks;
      uint32_t num_sets;
      uint32_t num_ways;

      uint32_t num_sets_per_subarray;
      std::vector< std::queue<LocalQueueElement * > > req_qs;

      // stats
      uint64_t num_rd_access;
      uint64_t num_rd_miss;
      uint64_t num_wr_access;
      uint64_t num_wr_miss;
      uint64_t num_ev_coherency;
      uint64_t num_ev_capacity;
      uint64_t num_coherency_access;  // number of received coherency accesses
      uint64_t num_upgrade_req;       // S/E -> M
      uint64_t num_bypass;
      uint64_t num_nack;

      uint64_t tot_awake_time;

      virtual void show_state(uint64_t) = 0;
      void display_event(uint64_t curr_time, LocalQueueElement *, const string &);


    public:
      friend class McSim;
  };



  class CacheL1 : public Cache
  {
    public:
      CacheL1(component_type type_, uint32_t num_, McSim * mcsim_);
      virtual ~CacheL1();

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);
      void show_state(uint64_t);

      CacheL2 * cachel2;       // downlink
      vector<Component *> lsus;  // uplink
      
      class PrefetchEntry
      {
        public:
          uint64_t addr;
          bool     hit;

          PrefetchEntry() : addr(0), hit(false) { }
      };

      bool             use_prefetch;
      PrefetchEntry ** pres;  // prefetch history info
      uint32_t         num_pre_entries;
      uint32_t         oldest_pre_entry_idx;

      uint64_t         num_prefetch_requests;
      uint64_t         num_prefetch_hits;

    private:
      pair< uint64_t, coherence_state_type > *** tags;  // address + coherence state of a set-associative cache
      //vector< list< pair< uint64_t, coherence_state_type > > > tags;  // address + coherence state of a set-associative cache

      const uint32_t l1_to_lsu_t;
      const uint32_t l1_to_l2_t;
      bool           always_hit;
      uint32_t       l2_set_lsb;  // gajh: as of now, we only support the case when L1$ line size <= L2$ line size

      void add_event_to_lsu(uint64_t curr_time, LocalQueueElement *);
  };


  class CacheL2 : public Cache
  {
    public:
      class L2Entry
      {
        public:
          uint64_t tag;
          coherence_state_type  type;  // cs_type between L2 and DIR
          coherence_state_type  type_l1l2;  // cs_type between L1 and L2
          std::set<Component *> sharedl1;
          LocalQueueElement *   pending;
          uint64_t first_access_time;
          uint64_t last_access_time;

          L2Entry() : tag(0), type(cs_invalid), type_l1l2(cs_invalid),  sharedl1(),
                      pending(NULL), first_access_time(0), last_access_time(0) { }
      };

      CacheL2(component_type type_, uint32_t num_, McSim * mcsim_);
      virtual ~CacheL2();

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);
      void show_state(uint64_t);

      Directory * directory;  // downlink
      NoC  * crossbar;        // downlink
      std::vector<CacheL1  *> cachel1d;   // uplink
      std::vector<CacheL1  *> cachel1i;   // uplink
      //std::vector< std::list< L2Entry > > tags;  // address + coherence state of a set-associative cache
      L2Entry *** tags;  // address + coherence state of a set-associative cache

    //private:
      const uint32_t l2_to_l1_t;
      const uint32_t l2_to_dir_t;
      const uint32_t l2_to_xbar_t;
      const uint32_t num_flits_per_packet;
      uint32_t       num_banks_log2;

      uint64_t       num_destroyed_cache_lines;
      uint64_t       cache_line_life_time;
      uint64_t       time_between_last_access_and_cache_destroy;
      uint64_t       num_ev_from_l1;
      uint64_t       num_ev_from_l1_miss;
      bool           always_hit;
      bool           display_life_time;

      void add_event_to_LL(uint64_t curr_time, LocalQueueElement *, bool check_top, bool is_data = false);
      void test_tags(uint32_t set);
  };

}

#endif
