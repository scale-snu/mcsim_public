// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTSCACHE_H_
#define PTSCACHE_H_

#include <list>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "McSim.h"

namespace PinPthread {

class Cache : public Component {
 public:
  Cache(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~Cache() { }
  const uint32_t set_lsb;
  const uint32_t num_banks;
  const uint32_t num_sets;
  const uint32_t num_ways;
  const uint32_t num_sets_per_subarray;
  friend class McSim;

 protected:
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
  void display_event(uint64_t curr_time, LocalQueueElement *, const std::string &);
};


using l1_tag_pair = std::pair<uint64_t, coherence_state_type>;

class CacheL1 : public Cache {
 public:
  explicit CacheL1(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~CacheL1();

  void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  uint32_t process_event(uint64_t curr_time);
  void show_state(uint64_t);

  std::vector<Component *> lsus;  // uplink
  CacheL2 * cachel2;              // downlink

  class PrefetchEntry {
   public:
    PrefetchEntry(): addr(0), hit(false) { }
    uint64_t addr;
    bool     hit;
  };

  const uint32_t l1_to_lsu_t;
  const uint32_t l1_to_l2_t;

  const bool       always_hit;
  const uint32_t   l2_set_lsb;  // TODO(gajh): as of now, we only support the case when L1$ line size <= L2$ line size
  const bool       use_prefetch;
  const uint32_t   num_pre_entries;

 protected:
  l1_tag_pair  *** tags;       // address + coherence state of a set-associative cache
  PrefetchEntry ** pres;       // prefetch history info
  uint64_t         num_prefetch_requests;
  uint64_t         num_prefetch_hits;
  uint32_t         oldest_pre_entry_idx;

  void add_event_to_lsu(uint64_t curr_time, LocalQueueElement *);
  void do_prefetch(uint64_t curr_time, const LocalQueueElement &);
  inline void update_LRU(uint32_t idx, l1_tag_pair ** tags_set, l1_tag_pair * const set_it);
};


class CacheL2 : public Cache {
 public:
  CacheL2(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~CacheL2();

  void add_req_event(uint64_t, LocalQueueElement *, Component * from = nullptr);
  void add_rep_event(uint64_t, LocalQueueElement *, Component * from = nullptr);
  virtual uint32_t process_event(uint64_t curr_time);
  void show_state(uint64_t);

  std::vector<CacheL1 *> cachel1d;   // uplink
  std::vector<CacheL1 *> cachel1i;   // uplink
  Directory * directory;  // downlink
  NoC  * crossbar;        // downlink

  class L2Entry {
   public:
    L2Entry() : tag(0), type(cs_invalid), type_l1l2(cs_invalid),  sharedl1(),
      pending(nullptr), first_access_time(0), last_access_time(0) { }

    uint64_t tag;
    coherence_state_type  type;  // cs_type between L2 and DIR
    coherence_state_type  type_l1l2;  // cs_type between L1 and L2
    std::set<Component *> sharedl1;
    LocalQueueElement *   pending;
    uint64_t first_access_time;
    uint64_t last_access_time;

    friend std::ostream & operator<<(std::ostream & out, L2Entry & l2);
  };

  const uint32_t l2_to_l1_t;
  const uint32_t l2_to_dir_t;
  const uint32_t l2_to_xbar_t;
  const uint32_t num_flits_per_packet;
  const uint32_t num_banks_log2;
  const bool     always_hit;
  const bool     display_life_time;

 protected:
  L2Entry ***    tags;  // address + coherence state of a set-associative cache
  uint64_t       num_ev_from_l1;
  uint64_t       num_ev_from_l1_miss;
  uint64_t       num_destroyed_cache_lines;
  uint64_t       cache_line_life_time;
  uint64_t       time_between_last_access_and_cache_destroy;

  void add_event_to_LL(uint64_t curr_time, LocalQueueElement *, bool check_top, bool is_data = false);
  void test_tags(uint32_t set);
  inline void update_LRU(uint32_t idx, L2Entry ** tags_set, L2Entry * const set_it);
  inline void req_L1_evict(uint64_t curr_time, L2Entry * const set_it, uint64_t addr, LocalQueueElement * lqe, bool always);
};

}  // namespace PinPthread

#endif  // PTSCACHE_H_
