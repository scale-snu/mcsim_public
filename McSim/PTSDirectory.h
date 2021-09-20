// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTSDIRECTORY_H_
#define PTSDIRECTORY_H_

#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>

#include "McSim.h"

namespace PinPthread {

class Directory : public Component {
 public:
  explicit Directory(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~Directory();

  void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  virtual uint32_t process_event(uint64_t curr_time);
  void show_state(uint64_t);

  // MemoryController * memorycontroller;  // downlink
  Component * memorycontroller;  // downlink
  CacheL2   * cachel2;    // uplink
  NoC       * crossbar;   // uplink

  class DirEntry {
   public:
    coherence_state_type type;
    std::set<Component *> sharedl2;
    LocalQueueElement * pending;
    bool got_cl;  // whether the entry got a cache line during an invalidation
    bool not_in_dc;   // true if the entry is not in the directory cache
    uint32_t num_sharer;

    DirEntry() : type(cs_invalid), sharedl2(), pending(NULL), got_cl(false), not_in_dc(false), num_sharer(0) { }
  };

  const uint32_t set_lsb;
  const uint32_t num_sets;
  const uint32_t num_ways;
  const uint32_t dir_to_mc_t;
  const uint32_t dir_to_l2_t;
  const uint32_t dir_to_xbar_t;
  const uint32_t num_flits_per_packet;
  const bool has_directory_cache;
  const bool use_limitless;
  const uint32_t limitless_broadcast_threshold;

 protected:
  std::map<uint64_t, DirEntry> dir;
  std::vector< std::list<uint64_t> > dir_cache;
  std::vector<uint64_t> num_sharer_histogram;

  // stats
  uint64_t num_acc;
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
  uint64_t num_dir_cache_miss;
  uint64_t num_dir_cache_retry;
  uint64_t num_dir_evict;

  inline void remove_directory_cache_entry(uint32_t set, uint64_t dir_entry);
  void add_event_to_UL(uint64_t curr_time, LocalQueueElement *, bool is_data);
  void add_event_to_ULpp(uint64_t curr_time, LocalQueueElement *, bool is_data);
  void add_event_to_UL(uint64_t curr_time,
      Component * comp,
      LocalQueueElement * lqe);
};

}  // namespace PinPthread

#endif  // PTSDIRECTORY_H_

