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

#include <glog/logging.h>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <set>
#include <sstream>
#include <string>

#include "PTSCache.h"
#include "PTSDirectory.h"
#include "PTSXbar.h"


namespace PinPthread {

extern std::ostream & operator << (std::ostream & output, coherence_state_type cs);
extern std::ostream & operator << (std::ostream & output, component_type ct);
extern std::ostream & operator << (std::ostream & output, event_type et);


Cache::Cache(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Component(type_, num_, mcsim_),
  set_lsb(get_param_uint64("set_lsb", 6)),
  num_rd_access(0), num_rd_miss(0),
  num_wr_access(0), num_wr_miss(0),
  num_ev_coherency(0), num_ev_capacity(0),
  num_coherency_access(0), num_upgrade_req(0),
  num_bypass(0), num_nack(0) {
  num_banks = get_param_uint64("num_banks", 1);
  req_qs    = std::vector< std::queue<LocalQueueElement * > >(num_banks);
}

void Cache::display_event(uint64_t curr_time, LocalQueueElement * lqe, const std::string & postfix) {
  if (lqe->address >> set_lsb == search_addr >> set_lsb) {
    LOG(WARNING) << "  -- [" << std::setw(7) << curr_time << "] " << type << postfix << " [" << num << "] " << *lqe;
    show_state(lqe->address);
  }
}


// in L1, num_sets is the number of sets of all L1 banks.
// set_lsb still sets the size of a cache line.
// bank and set numbers are specified like:
// [ MSB <-----------------> LSB ]
// [ ... SETS  BANKS  CACHE_LINE ]
CacheL1::CacheL1(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Cache(type_, num_, mcsim_),
  l1_to_lsu_t(get_param_uint64("to_lsu_t", 0)),
  l1_to_l2_t(get_param_uint64("to_l2_t", 45)) {
  num_sets   = get_param_uint64("num_sets", 64);
  num_ways   = get_param_uint64("num_ways",  4);
  always_hit = get_param_bool("always_hit", false);
  process_interval = get_param_uint64("process_interval", 10);
  l2_set_lsb = get_param_uint64("set_lsb", "pts.l2$.", set_lsb);
  CHECK(l2_set_lsb >= set_lsb);

  num_sets_per_subarray = get_param_uint64("num_sets_per_subarray", 1);

  tags = new l1_tag_pair ** [num_sets];
  for (uint32_t i = 0; i < num_sets; i++) {
    tags[i] = new l1_tag_pair * [num_ways];
    // tags[i][0] = LRU, tags[i][num_ways -1] = MRU
    for (uint32_t j = 0; j < num_ways; j++) {
      tags[i][j] = new l1_tag_pair(0, cs_invalid);
    }
  }

  use_prefetch          = get_param_bool("use_prefetch", false);
  num_prefetch_requests = 0;
  num_prefetch_hits     = 0;
  num_pre_entries       = get_param_uint64("num_pre_entries", 64);
  oldest_pre_entry_idx  = 0;

  pres = new PrefetchEntry * [num_pre_entries];
  for (uint32_t i = 0; i < num_pre_entries; i++) {
    pres[i] = new PrefetchEntry();
  }
}


CacheL1::~CacheL1() {
  if (num_rd_access > 0) {
    std::cout << "  -- L1$" << ((type == ct_cachel1d) ? "D[" : "I[")
      << std::setw(3) << num << "] : RD (miss, access)=( "
      << std::setw(8) << num_rd_miss << ", " << std::setw(8) << num_rd_access << ")= "
      << std::setw(6) << std::setiosflags(std::ios::fixed) << std::setprecision(2) << 100.00*num_rd_miss/num_rd_access << "%, PRE (hit, reqs)=( "
      << num_prefetch_hits << ", " << num_prefetch_requests << " )" << std::endl;
  }
  if (num_wr_access > 0) {
    std::cout << "  -- L1$" << ((type == ct_cachel1d) ? "D[" : "I[")
      << std::setw(3) << num << "] : WR (miss, access)=( "
      << std::setw(8) << num_wr_miss << ", " << std::setw(8) << num_wr_access << ")= "
      << std::setw(6) << std::setiosflags(std::ios::fixed) << std::setprecision(2)
      << 100.00*num_wr_miss/num_wr_access << "%" << std::endl;
  }

  if ((type == ct_cachel1d) && (num_ev_coherency > 0 || num_ev_capacity > 0 || num_coherency_access > 0)) {
    std::cout << "  -- L1$D[" << std::setw(3) << num
      << "] : (ev_coherency, ev_capacity, coherency_access, up_req, bypass, nack)=( "
      << std::setw(8) << num_ev_coherency << ", " << std::setw(8) << num_ev_capacity << ", "
      << std::setw(8) << num_coherency_access << ", " << std::setw(8) << num_upgrade_req << ", "
      << std::setw(8) << num_bypass << ", " << std::setw(8) << num_nack << "), ";

    std::map<uint64_t, uint64_t> dirty_cl_per_offset;
    int32_t  addr_offset_lsb = get_param_uint64("addr_offset_lsb", "", 48);

    for (uint32_t j = 0; j < num_sets; j++) {
      for (uint32_t i = 0; i < num_ways; i++) {
        if (tags[j][i]->second == cs_modified) {
          uint64_t addr   = ((tags[j][i]->first * num_sets) << set_lsb);
          uint64_t offset = addr >> addr_offset_lsb;

          if (dirty_cl_per_offset.find(offset) == dirty_cl_per_offset.end()) {
            dirty_cl_per_offset[offset] = 1;
          } else {
            dirty_cl_per_offset[offset]++;
          }
        }
      }
    }

    std::cout << "#_dirty_lines (pid:#) = ";

    for (auto m :  dirty_cl_per_offset) {
      std::cout << m.first << ": " << m.second << " , ";
    }
    std::cout << std::endl;
  } else if ((type == ct_cachel1i) &&
      (num_ev_coherency > 0 || num_coherency_access > 0)) {
    std::cout << "  -- L1$I[" << std::setw(3) << num << "] : (ev_coherency, coherency_access, bypass)=( "
      << std::setw(10) << num_ev_coherency << ", " << std::setw(10) << num_coherency_access << ", "
      << std::setw(10) << num_bypass << ")" << std::endl;
  }

  for (uint32_t i = 0; i < num_sets; i++) {
    for (uint32_t j = 0; j < num_ways; j++) {
      delete tags[i][j];
    }
    delete[] tags[i];
  }
  delete[] tags;

  for (uint32_t i = 0; i < num_pre_entries; i++) {
    delete pres[i];
  }
  delete[] pres;
}


void CacheL1::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  /* if (rep_q.empty() == false || req_q.empty() == false) {
    event_time += process_interval - event_time%process_interval;
  } */
  geq->add_event(event_time, this);
  req_event.insert(std::pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL1::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  /* if (rep_q.empty() == false || req_q.empty() == false) {
    event_time += process_interval - event_time%process_interval;
  } */
  geq->add_event(event_time, this);
  rep_event.insert(std::pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL1::show_state(uint64_t address) {
  uint32_t set = (address >> set_lsb) % num_sets;
  uint64_t tag = (address >> set_lsb) / num_sets;
  for (uint32_t i = 0; i < num_ways; i++) {
    if (tags[set][i]->second != cs_invalid && tags[set][i]->first == tag) {
      LOG(WARNING) << "  -- L1" << ((type == ct_cachel1d) ? "D[" : "I[") << num
        << "] : " << tags[set][i]->second << std::endl;
      break;
    }
  }
}


uint32_t CacheL1::process_event(uint64_t curr_time) {
  std::multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();
  std::multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();
  l1_tag_pair * set_it = nullptr;

  LocalQueueElement * rep_lqe = nullptr;
  LocalQueueElement * req_lqe = nullptr;
  // event -> queue
  if (rep_q.empty() == false) {
    rep_lqe = rep_q.front();
    rep_q.pop();
  } else if (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time) {
    rep_lqe = rep_event_iter->second;
    rep_event_iter = rep_event.erase(rep_event_iter);
  }

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time) {
    rep_q.push(rep_event_iter->second);
    rep_event_iter = rep_event.erase(rep_event_iter);
  }

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time) {
    // TODO(gajh): do we need some XOR schemes for better distribution across banks?
    uint32_t bank = (req_event_iter->second->address >> set_lsb) % num_banks;
    req_qs[bank].push(req_event_iter->second);
    req_event_iter = req_event.erase(req_event_iter);
  }

  // reply events have higher priority than request events
  if (rep_lqe != nullptr) {
    bool sent_to_l2    = false;
    bool addr_in_cache = false;
    for (int32_t index = 0; index < (1 << (l2_set_lsb - set_lsb)); index++) {
      // L1 -> LSU
      uint64_t address = ((rep_lqe->address >> l2_set_lsb) << l2_set_lsb) +
        (rep_lqe->address + index*(1 << set_lsb))%(1 << l2_set_lsb);
      // uint64_t address = rep_lqe->address;
      uint32_t set = (address >> set_lsb) % num_sets;
      uint64_t tag = (address >> set_lsb) / num_sets;
      auto tags_set    = tags[set];
      event_type etype = rep_lqe->type;

      // display_event(curr_time, rep_lqe, "P");
      uint32_t idx = 0;
      for ( ; idx < num_ways; idx++) {
        if (tags_set[idx]->second != cs_invalid && tags_set[idx]->first == tag) {
          set_it = tags_set[idx];
          break;
        }
      }

      switch (etype) {
        case et_nack:
          num_nack++;
        case et_rd_bypass:
          if (sent_to_l2 == true) break;
          sent_to_l2 = true;
          num_bypass++;
          rep_lqe->from.pop();
          add_event_to_lsu(curr_time, rep_lqe);
          break;

        case et_evict:
          num_coherency_access++;
          if (set_it != nullptr) {
            num_ev_coherency++;

            if (set_it->second == cs_modified) {
              set_it->second = cs_invalid;
              if (sent_to_l2 == false) {
                sent_to_l2 = true;
                rep_lqe->from.pop();
                rep_lqe->from.push(this);
                cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
              }
              break;
            }
            set_it->second = cs_invalid;
          }
          if (index == (1 << (l2_set_lsb - set_lsb)) - 1 && sent_to_l2 == false) delete rep_lqe;
          break;

        case et_m_to_s:
        case et_m_to_m:
          num_coherency_access++;
          if (set_it != nullptr && set_it->second == cs_modified) {
            if (etype == et_m_to_m) {
              num_ev_coherency++;
              set_it->second = cs_invalid;
            } else {
              set_it->second = cs_shared;
            }
          }
          if (sent_to_l2 == true) break;
          sent_to_l2 = true;
          rep_lqe->from.pop();
          rep_lqe->from.push(this);
          cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
          break;

        case et_dir_rd:
          num_coherency_access++;
          if (set_it == nullptr) {
            // oops -- the cache line is already evicted. do nothing.
            // delete rep_lqe;
          } else {
            if (sent_to_l2 == true) break;
            sent_to_l2 = true;
            if (set_it->second != cs_modified) {
              show_state(address);
              LOG(FATAL) << *this << *rep_lqe << *geq;
            }
            num_ev_coherency++;
            set_it->second = cs_exclusive;
            rep_lqe->type    = et_evict;
            rep_lqe->from.push(this);
            cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
          }
          if (index == (1 << (l2_set_lsb - set_lsb)) - 1 && sent_to_l2 == false) delete rep_lqe;
          break;

        case et_read:
        case et_write:
          // TODO(gajh): Why check *index* only for et_read and et_write?
          if (index != 0) break;
          rep_lqe->from.pop();
          if (set_it == nullptr) {
            idx = 0;
            set_it = tags_set[idx];
            if (set_it->second != cs_invalid) {
              // evicted due to lack of $ capacity
              num_ev_capacity++;
              auto lqe = new LocalQueueElement(this,
                  (set_it->second == cs_modified) ? et_evict : et_evict_nd,
                  ((set_it->first*num_sets + set) << set_lsb), rep_lqe->th_id);
              cachel2->add_rep_event(curr_time + l1_to_l2_t, lqe);
            }
          } else {
            addr_in_cache = true;
          }

          set_it->first  = tag;
          set_it->second = (etype == et_read && (addr_in_cache == false || set_it->second != cs_modified)) ? cs_exclusive : cs_modified;
          update_LRU(idx, tags_set, set_it);
          add_event_to_lsu(curr_time, rep_lqe);
          break;

        case et_e_to_s:
        case et_s_to_s:
        default:
          show_state(address);
          LOG(FATAL) << *this << *rep_lqe << *geq;
          break;
      }
    }
  } else {
    for (auto && req_q : req_qs) {
      if (req_q.empty() == true) {
        continue;
      }

      req_lqe = req_q.front();
      req_q.pop();
      // process the first request event
      const uint64_t & address = req_lqe->address;
      uint32_t set     = (address >> set_lsb) % num_sets;
      uint64_t tag     = (address >> set_lsb) / num_sets;
      auto tags_set    = tags[set];
      event_type etype = req_lqe->type;
      bool hit = always_hit;
      bool is_coherence_miss = false;

      // display_event(curr_time, req_lqe, "Q");
      DLOG_IF(FATAL, etype != et_read && etype != et_write) << "req_lqe->type should be either et_read or et_write, not " << etype << ".\n";
      if (etype == et_read) {
        num_rd_access++;
        for (uint32_t idx = 0; idx < num_ways && hit == false; idx++) {
          auto set_it = tags_set[idx];
          if (set_it->second == cs_invalid) {
            continue;
          } else if (set_it->first == tag) {
            if (set_it->second == cs_modified || set_it->second == cs_shared || set_it->second == cs_exclusive) {
              hit = true;
              update_LRU(idx, tags_set, set_it);
            }
            break;
          }
        }
      } else {
        num_wr_access++;
        for (uint32_t idx = 0; idx < num_ways && hit == false; idx++) {
          auto set_it = tags_set[idx];
          if (set_it->second == cs_invalid) {
            continue;
          } else if (set_it->first == tag) {
            if (set_it->second == cs_modified) {
              hit = true;
              update_LRU(idx, tags_set, set_it);
            } else if (set_it->second == cs_shared || set_it->second == cs_exclusive) {
              // on a write miss, invalidate the entry so that the following
              // cache accesses to the address experience misses as well
              // TODO(gajh): does it mean that there is no MSHR in L1$?
              num_upgrade_req++;
              is_coherence_miss = true;
              set_it->second = cs_invalid;
            }
            break;
          }
        }
      }
      if (etype == et_read && use_prefetch == true) {
        // currently prefetch is conducted for read requests only.
        do_prefetch(curr_time, *req_lqe);
      }

      if (hit == false) {
        if (is_coherence_miss == false) {
          (etype == et_write) ? num_wr_miss++ : num_rd_miss++;
        }
        req_lqe->from.push(this);
        cachel2->add_req_event(curr_time + l1_to_l2_t, req_lqe);
      } else {
        add_event_to_lsu(curr_time, req_lqe);
      }
    }
  }

  if (rep_q.empty() == false) {
    geq->add_event(curr_time + process_interval, this);
  } else {
    for (auto && req_q : req_qs) {
      if (req_q.empty() == false) {
        geq->add_event(curr_time + process_interval, this);
        break;
      }
    }
  }

  return 0;
}


void CacheL1::add_event_to_lsu(uint64_t curr_time, LocalQueueElement * lqe) {
  if (type == ct_cachel1d) {
    (lqe->from.top())->add_rep_event(curr_time + l1_to_lsu_t, lqe);
  } else {
    (lqe->from.top())->add_req_event(curr_time + l1_to_lsu_t, lqe);
  }
}


void CacheL1::do_prefetch(uint64_t curr_time, const LocalQueueElement & req_lqe) {
  // update the prefetch entries if
  //  1) current and (previous or next) $ line addresses are in the L1 $
  //  2) do not cause multiple prefetches to the same $ line (check existing entries)
  uint64_t address = ((req_lqe.address >> set_lsb) << set_lsb);
  uint64_t prev_addr = address - (1 << set_lsb);
  uint64_t next_addr = address + (1 << set_lsb);
  bool     next_addr_exist = false;
  bool     prev_addr_exist = false;
  for (uint32_t idx = 0; idx < num_pre_entries; idx++) {
    auto pre = pres[idx];
    if (pre->addr != 0 && pre->addr == next_addr) {
      pre->hit  = true;
      next_addr_exist = true;
      break;
    }
  }
  // check_prev first
  if (next_addr_exist == false) {
    uint32_t set = (prev_addr >> set_lsb) % num_sets;
    uint64_t tag = (prev_addr >> set_lsb) / num_sets;
    for (uint32_t idx = 0; idx < num_ways; idx++) {
      auto set_it = tags[set][idx];
      if (set_it->second == cs_invalid) {
        continue;
      } else if (set_it->first == tag) {
        prev_addr_exist = true;
        break;
      }
    }
    if (prev_addr_exist == true) {
      LocalQueueElement * lqe = new LocalQueueElement(this, et_read, next_addr, req_lqe.th_id);
      cachel2->add_req_event(curr_time + l1_to_l2_t, lqe);
      // update the prefetch entry
      if (pres[oldest_pre_entry_idx]->addr != 0) {
        num_prefetch_requests++;
        num_prefetch_hits += (pres[oldest_pre_entry_idx]->hit == true) ? 1 : 0;
      }
      pres[oldest_pre_entry_idx]->hit  = false;
      pres[oldest_pre_entry_idx]->addr = next_addr;
      oldest_pre_entry_idx = (oldest_pre_entry_idx + 1) % num_pre_entries;
    }
  }
  for (uint32_t idx = 0; idx < num_pre_entries && prev_addr_exist == false; idx++) {
    auto pre = pres[idx];
    if (pre->addr != 0 && pre->addr == prev_addr) {
      pre->hit  = true;
      prev_addr_exist = true;
      break;
    }
  }
  if (prev_addr_exist == false) {
    next_addr_exist = false;
    uint32_t set = (next_addr >> set_lsb) % num_sets;
    uint64_t tag = (next_addr >> set_lsb) / num_sets;
    for (uint32_t idx = 0; idx < num_ways; idx++) {
      auto set_it = tags[set][idx];
      if (set_it->second == cs_invalid) {
        continue;
      } else if (set_it->first == tag) {
        next_addr_exist = true;
        break;
      }
    }
    if (next_addr_exist == true) {
      LocalQueueElement * lqe = new LocalQueueElement(this, et_read, prev_addr, req_lqe.th_id);
      cachel2->add_req_event(curr_time + l1_to_l2_t, lqe);
      // update the prefetch entry
      if (pres[oldest_pre_entry_idx]->addr != 0) {
        num_prefetch_requests++;
        num_prefetch_hits += (pres[oldest_pre_entry_idx]->hit == true) ? 1 : 0;
      }
      pres[oldest_pre_entry_idx]->hit  = false;
      pres[oldest_pre_entry_idx]->addr = prev_addr;
      oldest_pre_entry_idx = (oldest_pre_entry_idx + 1) % num_pre_entries;
    }
  }
}


void CacheL1::update_LRU(uint32_t idx, l1_tag_pair ** tags_set, l1_tag_pair * const set_it) {
  for (uint32_t i = idx; i < num_ways-1; i++) {
    tags_set[i] = tags_set[i+1];
  }
  tags_set[num_ways-1] = set_it;
}


std::ostream & operator<<(std::ostream & out, CacheL2::L2Entry & l2) {
  out << " l2entry: tag= " << std::hex << l2.tag << std::dec;
  out << ", type= " << l2.type << ", type_l1l2= " << l2.type_l1l2;
  out << ", sharedl1=[ ";
  for (auto && it : l2.sharedl1)  out << *it << ", ";
  out << "], first_acc_time= " << l2.first_access_time;
  out << ", last_acc_time= " << l2.last_access_time;
  if (l2.pending != nullptr)  out << ", pending= " << *(l2.pending);
  return out;
}


// in L2, num_sets is the number of sets of all L2 banks.
// set_lsb still sets the size of a cache line.
// bank and set numbers are specified like:
// [ MSB <-----------------> LSB ]
// [ ... SETS  BANKS  CACHE_LINE ]
CacheL2::CacheL2(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
  :Cache(type_, num_, mcsim_),
  l2_to_l1_t(get_param_uint64("to_l1_t", 45)),
  l2_to_dir_t(get_param_uint64("to_dir_t", 90)),
  l2_to_xbar_t(get_param_uint64("to_xbar_t", 90)),
  num_flits_per_packet(get_param_uint64("num_flits_per_packet", 1)) {
    num_sets         = get_param_uint64("num_sets",  512);
    num_ways         = get_param_uint64("num_ways",  8);
    process_interval = get_param_uint64("process_interval", 20);
    num_banks_log2   = log2(num_banks);

    num_sets_per_subarray = get_param_uint64("num_sets_per_subarray", 1);
    always_hit            = get_param_bool("always_hit", false);
    display_life_time     = get_param_bool("display_life_time", false);

    num_destroyed_cache_lines = 0;
    cache_line_life_time      = 0;
    time_between_last_access_and_cache_destroy = 0;
    num_ev_from_l1      = 0;
    num_ev_from_l1_miss = 0;

    // tags   = vector< list< L2Entry > >(num_sets, list< L2Entry >(num_ways, L2Entry()));
    tags = new L2Entry ** [num_sets];
    for (uint32_t i = 0; i < num_sets; i++) {
      tags[i] = new L2Entry * [num_ways];
      // tags[i][0] = LRU, tags[i][num_ways -1] = MRU
      for (uint32_t j = 0; j < num_ways; j++) {
        tags[i][j] = new L2Entry();
      }
    }
  }


CacheL2::~CacheL2() {
  if (num_rd_access > 0) {
    std::cout << "  -- L2$ [" << std::setw(3) << num << "] : RD (miss, acc)=( "
      << std::setw(8) << num_rd_miss << ", " << std::setw(8) << num_rd_access << ")= "
      << std::setw(6) << std::setiosflags(std::ios::fixed) << std::setprecision(2) << 100.00*num_rd_miss/num_rd_access << "%" << std::endl;
  }
  if (num_wr_access > 0) {
    std::cout << "  -- L2$ [" << std::setw(3) << num << "] : WR (miss, acc)=( "
      << std::setw(8) << num_wr_miss << ", " << std::setw(8) << num_wr_access << ")= "
      << std::setw(6) << std::setiosflags(std::ios::fixed) << std::setprecision(2) << 100.00*num_wr_miss/num_wr_access << "%" << std::endl;
  }

  if (num_ev_coherency > 0 || num_ev_capacity > 0 || num_coherency_access > 0 || num_upgrade_req > 0) {
    std::cout << "  -- L2$ [" << std::setw(3) << num << "] : (ev_coherency, ev_capacity, coherency_acc, up_req, bypass, nack)=( "
      << std::setw(8) << num_ev_coherency << ", " << std::setw(8) << num_ev_capacity << ", "
      << std::setw(8) << num_coherency_access << ", " << std::setw(8) << num_upgrade_req << ", "
      << std::setw(8) << num_bypass << ", " << std::setw(8) << num_nack << ")" << std::endl;
  }
  if (num_ev_from_l1 > 0) {
    std::cout << "  -- L2$ [" << std::setw(3) << num << "] : EV_from_L1 (miss, acc)=( "
      << std::setw(8) << num_ev_from_l1_miss << ", " << std::setw(8) << num_ev_from_l1 << ")= "
      << std::setiosflags(std::ios::fixed) << std::setprecision(2) << 100.0*num_ev_from_l1_miss/num_ev_from_l1 << "%, ";
  }
  if (num_rd_access > 0 || num_wr_access > 0) {
    uint32_t num_cache_lines    = 0;
    uint32_t num_i_cache_lines  = 0;
    uint32_t num_e_cache_lines  = 0;
    uint32_t num_s_cache_lines  = 0;
    uint32_t num_m_cache_lines  = 0;
    uint32_t num_tr_cache_lines = 0;
    int32_t  addr_offset_lsb = get_param_uint64("addr_offset_lsb", "", 48);

    std::map<uint64_t, uint64_t> dirty_cl_per_offset;

    for (uint32_t j = 0; j < num_sets; j++) {
      // for (list<CacheL2::L2Entry>::iterator iter = tags[j].begin(); iter != tags[j].end(); ++iter)
      for (uint32_t k = 0; k < num_ways; k++) {
        L2Entry * iter = tags[j][k];
        if (iter->type == cs_modified) {
          uint64_t addr   = ((iter->tag * num_sets) << set_lsb);
          uint64_t offset = addr >> addr_offset_lsb;

          if (dirty_cl_per_offset.find(offset) == dirty_cl_per_offset.end()) {
            dirty_cl_per_offset[offset] = 1;
          } else {
            dirty_cl_per_offset[offset]++;
          }
        }
        switch (iter->type) {
          case cs_invalid:   num_i_cache_lines++;  break;
          case cs_exclusive: num_e_cache_lines++;  break;
          case cs_shared:    num_s_cache_lines++;  break;
          case cs_modified:  num_m_cache_lines++;  break;
          default:           num_tr_cache_lines++; break;
        }
      }
    }
    num_cache_lines = num_i_cache_lines + num_e_cache_lines +
      num_s_cache_lines + num_m_cache_lines + num_tr_cache_lines;

    std::cout << " L2$ (i,e,s,m,tr) ratio=("
      << std::setiosflags(std::ios::fixed) << std::setw(3) << 1000 * num_i_cache_lines  / num_cache_lines << ", "
      << std::setiosflags(std::ios::fixed) << std::setw(3) << 1000 * num_e_cache_lines  / num_cache_lines << ", "
      << std::setiosflags(std::ios::fixed) << std::setw(3) << 1000 * num_s_cache_lines  / num_cache_lines << ", "
      << std::setiosflags(std::ios::fixed) << std::setw(3) << 1000 * num_m_cache_lines  / num_cache_lines << ", "
      << std::setiosflags(std::ios::fixed) << std::setw(3) << 1000 * num_tr_cache_lines / num_cache_lines << "), num_dirty_lines (pid:#) = ";

    for (auto m_iter = dirty_cl_per_offset.begin(); m_iter != dirty_cl_per_offset.end(); m_iter++) {
      std::cout << m_iter->first << " : " << m_iter->second << ", ";
    }
    std::cout << std::endl;
  }
  if (display_life_time == true && num_destroyed_cache_lines > 0) {
    std::cout << "  -- L2$ [" << std::setw(3) << num << "] : (cache_line_life_time, time_between_last_access_and_cache_destroy) = ("
      << std::setiosflags(std::ios::fixed) << std::setprecision(2)
      << 1.0 * cache_line_life_time / (process_interval * num_destroyed_cache_lines) << ", "
      << std::setiosflags(std::ios::fixed) << std::setprecision(2)
      << 1.0 * time_between_last_access_and_cache_destroy / (process_interval * num_destroyed_cache_lines)
      << ") L2$ cycles" << std::endl;
  }

  for (uint32_t j = 0; j < num_sets; j++) {
    for (uint32_t k = 0; k < num_ways; k++) {
      delete tags[j][k];
    }
    delete[] tags[j];
  }
  delete[] tags;
}


void CacheL2::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  event_time = ceil_by_y(event_time, process_interval);
  geq->add_event(event_time, this);
  req_event.insert(std::pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL2::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  event_time = ceil_by_y(event_time, process_interval);
  geq->add_event(event_time, this);
  rep_event.insert(std::pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL2::show_state(uint64_t address) {
  uint32_t set = (address >> set_lsb) % num_sets;
  uint64_t tag = (address >> set_lsb) / num_sets;

  for (uint32_t k = 0; k < num_ways; k++) {
    L2Entry * set_it = tags[set][k];
    if (set_it->type != cs_invalid && set_it->tag == tag) {
      std::stringstream ss;
      ss << "  -- L2 [" << num << "] : " << set_it->type
        << ", " << set_it->type_l1l2;
      for (auto && it : set_it->sharedl1) ss << ", " << *it;
      LOG(WARNING) << ss.str() << std::endl;
      break;
    }
  }
}


uint32_t CacheL2::process_event(uint64_t curr_time) {
  std::multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();
  std::multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();
  L2Entry * set_it = nullptr;

  LocalQueueElement * rep_lqe = nullptr;
  LocalQueueElement * req_lqe = nullptr;
  // event -> queue
  if (rep_q.empty() == false) {
    rep_lqe = rep_q.front();
    rep_q.pop();
  } else if (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time) {
    rep_lqe = rep_event_iter->second;
    rep_event_iter = rep_event.erase(rep_event_iter);
  }

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time) {
    rep_q.push(rep_event_iter->second);
    rep_event_iter = rep_event.erase(rep_event_iter);
  }

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time) {
    uint32_t bank = (req_event_iter->second->address >> set_lsb) % num_banks;
    req_qs[bank].push(req_event_iter->second);
    req_event_iter = req_event.erase(req_event_iter);
  }

  if (rep_lqe != nullptr) {
    // display_event(curr_time, rep_lqe, "P");
    // reply events have a higher priority than request events
    const uint64_t & address = rep_lqe->address;
    const uint32_t set = (address >> set_lsb) % num_sets;
    const uint64_t tag = (address >> set_lsb) / num_sets;
    auto tags_set    = tags[set];
    const event_type etype = rep_lqe->type;

    uint32_t idx = 0;
    // look for an entry which already has tag
    for (idx = 0; idx < num_ways; idx++) {
      set_it = tags_set[idx];
      if (set_it->type == cs_invalid) {
        continue;
      } else if (set_it->tag == tag) {
        break;
      }
    }

    if (etype == et_write_nd) {
      rep_lqe->from.pop();
      if (idx == num_ways || set_it->type != cs_tr_to_m) {
        rep_lqe->type = et_nack;
        (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        auto lqe = new LocalQueueElement(this, et_e_to_i, address, rep_lqe->th_id);
        add_event_to_LL(curr_time, lqe, false);
      } else {
        req_L1_evict(curr_time, set_it, (set_it->tag*num_sets + set) << set_lsb, rep_lqe, false);
        set_it->type      = cs_modified;
        set_it->type_l1l2 = cs_modified;
        set_it->tag       = tag;
        set_it->sharedl1.insert(rep_lqe->from.top());
        set_it->last_access_time = curr_time;
        update_LRU(idx, tags_set, set_it);

        rep_lqe->type = et_write;
        (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);

        auto lqe = new LocalQueueElement(this, et_e_to_m, address, rep_lqe->th_id);
        add_event_to_LL(curr_time, lqe, false);
      }
    } else if (etype == et_e_rd || etype == et_s_rd || etype == et_write) {
      bool bypass = false;
      bool shared = false;
      rep_lqe->from.pop();
      // read miss return traffic
      if (idx == num_ways) {
        set_it = tags_set[0];
        idx    = 0;
        uint64_t set_addr = ((set_it->tag*num_sets + set) << set_lsb);

        if (set_it->type == cs_tr_to_s || set_it->type == cs_tr_to_m ||
            set_it->type == cs_tr_to_e || set_it->type == cs_tr_to_i) {
          bypass = true;
          if (rep_lqe->from.size() > 1) {
            rep_lqe->type = et_nack;
            (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
          }
          auto lqe = new LocalQueueElement(this, et_evict, rep_lqe->address, rep_lqe->th_id);
          add_event_to_LL(curr_time, lqe, false);
          if (rep_lqe->from.size() == 1) {
            delete rep_lqe;
          }
        } else if (set_it->type != cs_invalid) {
          num_ev_capacity++;
          num_destroyed_cache_lines++;
          cache_line_life_time += (curr_time - set_it->first_access_time);
          time_between_last_access_and_cache_destroy += (curr_time - set_it->last_access_time);
          set_it->first_access_time = curr_time;
          set_it->last_access_time  = curr_time;
          // capacity miss
          req_L1_evict(curr_time, set_it, set_addr, rep_lqe, true);
          // then send eviction event to Directory or Crossbar
          if (set_it->type_l1l2 != cs_modified) {
            auto lqe = new LocalQueueElement(this, et_evict, set_addr, rep_lqe->th_id);
            add_event_to_LL(curr_time, lqe, false, set_it->type == cs_modified);
          }
        } else {
          set_it->first_access_time = curr_time;
          set_it->last_access_time  = curr_time;
        }
      } else {
        uint64_t set_addr = ((set_it->tag*num_sets + set) << set_lsb);

        if (etype == et_write) {
          req_L1_evict(curr_time, set_it, set_addr, rep_lqe, false);
        } else if (etype == et_e_rd || etype == et_s_rd) {
          if (set_it->type == cs_modified || set_it->type == cs_tr_to_e) {
            bypass = true;  // this event happened earlier, don't change the state of cache
            if (rep_lqe->from.size() > 1) {
              rep_lqe->type = et_rd_bypass;
              (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            } else {
              delete rep_lqe;
            }
          } else if (etype == et_s_rd) {
            shared = true;
          }
        }
      }

      CHECK_NE(idx, num_ways) << *this << *rep_lqe << *geq;
      if (bypass == false) {
        set_it->type = (etype == et_e_rd) ? cs_exclusive : (etype == et_s_rd) ? cs_shared : cs_modified;
        if (rep_lqe->from.size() > 1) {
          set_it->type_l1l2 = (etype == et_write) ? cs_modified : (shared == true) ? cs_shared : cs_exclusive;
          set_it->tag       = tag;
          set_it->sharedl1.insert(rep_lqe->from.top());
        } else {
          set_it->type_l1l2 = cs_invalid;
          set_it->tag       = tag;
        }
        set_it->last_access_time = curr_time;
        update_LRU(idx, tags_set, set_it);

        rep_lqe->type = (etype == et_write) ? et_write : et_read;
        if (rep_lqe->from.size() > 1) {
          (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        } else {
          delete rep_lqe;
        }
      } else {
        num_bypass++;
      }
    } else if (etype == et_m_to_s || etype == et_m_to_m) {
      DLOG_IF(FATAL, rep_lqe->from.empty()) << "rep_lqe->from must not be empty.\n";
      rep_lqe->from.pop();
      num_coherency_access++;

      if (idx != num_ways && set_it->type == cs_tr_to_i && set_it->pending != nullptr) {
        num_ev_coherency++;
        switch (set_it->type_l1l2) {
          case cs_tr_to_i:
            delete rep_lqe;
            break;
          case cs_tr_to_m:
            rep_lqe->type = et_nack;
            rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            break;
          default:  // set_it->type_l1l2 == cs_tr_to_s
            rep_lqe->type = et_nack;
            rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            set_it->sharedl1.insert(rep_lqe->from.top());
            req_L1_evict(curr_time, set_it, (set_it->tag*num_sets + set) << set_lsb, rep_lqe, true);
            break;
        }

        add_event_to_LL(curr_time, set_it->pending, true, true);
        num_destroyed_cache_lines++;
        cache_line_life_time += (curr_time - set_it->first_access_time);
        time_between_last_access_and_cache_destroy += (curr_time - set_it->last_access_time);
        set_it->pending = nullptr;
        set_it->type_l1l2 = cs_invalid;
        set_it->type      = cs_invalid;
        update_LRU(idx, tags_set, set_it);
      } else if (idx != num_ways && (set_it->type_l1l2 == cs_tr_to_m || set_it->type_l1l2 == cs_tr_to_s)) {
        num_ev_coherency++;
        set_it->last_access_time = curr_time;
        set_it->type_l1l2 = (set_it->type_l1l2 == cs_tr_to_s) ? cs_shared :
          (set_it->pending == nullptr) ? cs_modified : cs_invalid;
        set_it->sharedl1.insert(rep_lqe->from.top());
        rep_lqe->type = (etype == et_m_to_s) ? et_read :
          (set_it->pending == nullptr) ? et_write : et_nack;
        rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        if (set_it->pending != nullptr) {
          add_event_to_LL(curr_time, set_it->pending, true, true);
          set_it->pending = nullptr;
          set_it->type    = cs_shared;
        }
        update_LRU(idx, tags_set, set_it);
      } else {
        show_state(rep_lqe->address);
        rep_lqe->from.top()->show_state(rep_lqe->address);
        LOG(FATAL) << *this << *rep_lqe << *geq;
      }
    } else if (etype == et_evict || etype == et_evict_nd) {
      num_ev_from_l1++;
      if (idx != num_ways) {
        set_it->last_access_time = curr_time;
        if (set_it->type == cs_tr_to_s) {
          num_coherency_access++;
          set_it->type = cs_shared;
          add_event_to_LL(curr_time, set_it->pending, true, true);
          set_it->pending = nullptr;
          set_it->sharedl1.insert(rep_lqe->from.top());
        } else {
          // cache line is evicted from L1
          set_it->sharedl1.erase(rep_lqe->from.top());
          if (set_it->sharedl1.empty() == true && set_it->type_l1l2 != cs_tr_to_s &&
              set_it->type_l1l2 != cs_tr_to_m && set_it->type_l1l2 != cs_tr_to_i) {
            set_it->type_l1l2 = cs_invalid;
          }
          update_LRU(idx, tags_set, set_it);
        }
        delete rep_lqe;
      } else {
        num_ev_from_l1_miss++;
        if (etype == et_evict && always_hit == false) {
          rep_lqe->from.push(this);
          add_event_to_LL(curr_time, rep_lqe, true, true);
        } else {
          delete rep_lqe;
        }
      }
    } else if (etype == et_dir_rd) {
      num_coherency_access++;
      if (idx == num_ways) {
        rep_lqe->type = et_dir_rd_nd;
        // cache line is already evicted -- return now
        add_event_to_LL(curr_time, rep_lqe, false);
      } else {
        if (set_it->type != cs_modified) {
          LOG(ERROR) << *set_it << std::endl;
          show_state(rep_lqe->address);
          rep_lqe->from.top()->show_state(rep_lqe->address);
          LOG(FATAL) << *this << *rep_lqe << *geq;
        } else if (set_it->type_l1l2 == cs_invalid ||
            set_it->type_l1l2 == cs_exclusive ||
            set_it->type_l1l2 == cs_shared) {
          set_it->type      = cs_shared;
          set_it->last_access_time = curr_time;
          add_event_to_LL(curr_time, rep_lqe, true, true);
        } else if (set_it->type_l1l2 == cs_modified) {
          if (set_it->sharedl1.size() != 1) {
            LOG(FATAL) << *set_it << std::endl << *this << *rep_lqe << *geq;
          }
          // special case: data is in L1
          set_it->last_access_time = curr_time;
          set_it->type_l1l2 = cs_exclusive;
          set_it->type      = cs_tr_to_s;
          set_it->pending   = rep_lqe;
          auto lqe = new LocalQueueElement(this, et_dir_rd,
              ((set_it->tag*num_sets + set) << set_lsb), rep_lqe->th_id);
          (*(set_it->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
          set_it->sharedl1.clear();
        } else if (set_it->type_l1l2 == cs_tr_to_m || set_it->type_l1l2 == cs_tr_to_s) {
          if (set_it->pending != nullptr) {
            LOG(FATAL) << *set_it << std::endl << *this << *rep_lqe << *geq;
          }
          set_it->last_access_time = curr_time;
          set_it->pending          = rep_lqe;
        } else {
          // DIR->L2->L1->L2->DIR traffic -- not implemented yet
          LOG(FATAL) << *set_it << std::endl << *this << *rep_lqe << *geq;
        }
      }
    } else if (etype == et_nack) {
      num_nack++;
      num_bypass++;
      rep_lqe->from.pop();
      if (rep_lqe->from.size() > 1) {
        (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
      } else {
        delete rep_lqe;
      }
    } else if (etype == et_e_to_s || etype == et_s_to_s) {
      num_coherency_access++;

      if (idx != num_ways) {
        if (set_it->type != cs_exclusive && set_it->type != cs_shared && set_it->type != cs_tr_to_m) {
          LOG(FATAL) << "[" << curr_time << "] " << *set_it << std::endl << *this << *rep_lqe << *geq;
        }
        set_it->last_access_time = curr_time;
        set_it->type = cs_shared;
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, true, true);
      } else {
        // the cache line is evicted already so that we can't get data from this L2
        rep_lqe->type = (etype == et_e_to_s) ? et_e_to_s_nd : et_s_to_s_nd;
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, false);
      }
    } else if (etype == et_invalidate || etype == et_invalidate_nd) {
      bool enter_intermediate_state = false;

      num_coherency_access++;
      if (idx != num_ways) {
        if (set_it->type == cs_tr_to_s ||  // set_it->type == cs_tr_to_m ||
            set_it->type == cs_tr_to_e || set_it->type == cs_tr_to_i) {
          show_state(rep_lqe->address);
          LOG(FATAL) << *this << *rep_lqe << *geq;
        } else if (set_it->type == cs_modified && set_it->type_l1l2 == cs_modified) {
          enter_intermediate_state = true;
          if (set_it->sharedl1.size() != 1) {
            LOG(FATAL) << *set_it << std:: endl << *this << *rep_lqe << *geq;
          }
          // special case: data is in L1
          set_it->last_access_time = curr_time;
          set_it->type_l1l2 = cs_tr_to_i;
          set_it->type      = cs_tr_to_i;
          set_it->pending   = rep_lqe;
          auto lqe = new LocalQueueElement(this, et_m_to_m,
              ((set_it->tag*num_sets + set) << set_lsb), rep_lqe->th_id);
          (*(set_it->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
          set_it->sharedl1.clear();
        } else if (set_it->type == cs_modified &&
            (set_it->type_l1l2 == cs_tr_to_m || set_it->type_l1l2 == cs_tr_to_s)) {
          enter_intermediate_state = true;
          if (set_it->pending != nullptr) {
            LOG(FATAL) << *set_it << std::endl << *this << *rep_lqe << *geq;
          }
          set_it->last_access_time = curr_time;
          set_it->type      = cs_tr_to_i;
          set_it->pending   = rep_lqe;
        } else {
          // evict the corresponding cache lines in L1 and L2 and return
          req_L1_evict(curr_time, set_it, (set_it->tag*num_sets + set) << set_lsb, rep_lqe, true);

          num_ev_coherency++;
          num_destroyed_cache_lines++;
          cache_line_life_time += (curr_time - set_it->first_access_time);
          time_between_last_access_and_cache_destroy += (curr_time - set_it->last_access_time);
          set_it->type      = cs_invalid;
          set_it->type_l1l2 = cs_invalid;
        }
      } else {
        rep_lqe->type = et_invalidate_nd;
      }

      if (enter_intermediate_state == false) {
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, true, rep_lqe->type == et_invalidate);
      }
    } else if (etype == et_nop) {
      delete rep_lqe;
    } else {
      LOG(FATAL) << *this << *rep_lqe << *geq;
    }
  } else {
    bool any_request = false;

    for (auto && req_q : req_qs) {
      if (req_q.empty() == true) {
        continue;
      }
      any_request = true;

      req_lqe = req_q.front();
      req_q.pop();
      // process the first request event
      const uint64_t & address = req_lqe->address;
      uint32_t set = (address >> set_lsb) % num_sets;
      uint64_t tag = (address >> set_lsb) / num_sets;
      auto tags_set    = tags[set];
      event_type etype = req_lqe->type;
      bool is_coherence_miss = false;

      bool hit = always_hit;
      bool enter_intermediate_state = false;

      DLOG_IF(FATAL, etype != et_read && etype != et_write)
        << "req_lqe->type should be et_read or et_write, not " << etype << ".\n";
      if (etype == et_read) {
        // see if cache hits
        num_rd_access++;

        for (uint32_t idx = 0; idx < num_ways; idx++) {
          auto set_it = tags_set[idx];
          if (set_it->type == cs_invalid || set_it->type == cs_tr_to_e) {
            continue;
          }
          if (set_it->tag == tag && req_lqe->from.size() == 1) {
            hit = true;
          } else if (set_it->tag == tag) {
            if (set_it->type_l1l2 == cs_invalid &&
                (set_it->type == cs_exclusive || set_it->type == cs_shared || set_it->type == cs_modified)) {
              // cache hit, and type_l1l2 will be cs_exclusive
              set_it->type_l1l2 = cs_exclusive;
              set_it->sharedl1.insert(req_lqe->from.top());
            } else if (set_it->type_l1l2 == cs_exclusive &&
                (set_it->type == cs_exclusive || set_it->type == cs_shared || set_it->type == cs_modified)) {
              // cache hit, and type_l1l2 will be cs_exclusive or cs_shared
              set_it->sharedl1.insert(req_lqe->from.top());
              if (set_it->sharedl1.size() > 1) {
                set_it->type_l1l2 = cs_shared;
              }
            } else if (set_it->type_l1l2 == cs_shared &&
                (set_it->type == cs_exclusive || set_it->type == cs_shared || set_it->type == cs_modified)) {
              // cache hit, and type_l1l2 will be cs_shared
              set_it->sharedl1.insert(req_lqe->from.top());
            } else if (set_it->type_l1l2 == cs_modified && set_it->type == cs_modified) {
              // cache hit, and type_l1l2 will be cs_shared, m_to_s event request will be delivered to L1
              if (set_it->sharedl1.size() > 1) {
                LOG(FATAL) << *set_it << std::endl << *this << *req_lqe << *geq;
              }

              if (set_it->sharedl1.size() == 1) {
                if (*(set_it->sharedl1.begin()) != req_lqe->from.top()) {
                  enter_intermediate_state = true;
                  req_lqe->from.push(this);
                  req_lqe->type = et_m_to_s;
                  (*(set_it->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
                  set_it->type_l1l2 = cs_tr_to_s;
                  set_it->sharedl1.clear();
                }
              } else {
                set_it->type_l1l2 = cs_shared;
                set_it->sharedl1.insert(req_lqe->from.top());
              }
            } else if ((set_it->type_l1l2 == cs_exclusive && set_it->type == cs_tr_to_s) ||
                set_it->type_l1l2 == cs_tr_to_s || set_it->type_l1l2 == cs_tr_to_m ||
                set_it->type_l1l2 == cs_tr_to_i || set_it->type == cs_tr_to_m) {
              req_lqe->type = et_nack;
            } else {
              LOG(FATAL) << "[" << curr_time << "] " << *set_it << std::endl << *this << *req_lqe << *geq;
            }

            set_it->last_access_time = curr_time;
            hit = true;
            update_LRU(idx, tags_set, set_it);
            break;
          }
        }
      } else {
        num_wr_access++;

        for (uint32_t idx = 0; idx < num_ways; idx++) {
          set_it = tags_set[idx];
          if (set_it->type == cs_exclusive || set_it->type == cs_shared) {
            if (set_it->tag == tag) {
              if (set_it->type == cs_exclusive && set_it->sharedl1.size() == 1 &&
                  (*(set_it->sharedl1.begin()) == req_lqe->from.top())) {
                set_it->last_access_time = curr_time;
                set_it->type = cs_tr_to_m;
              } else {
                if (set_it->sharedl1.empty() == false) {
                  set_it->last_access_time = curr_time;
                  set_it->type = cs_invalid;
                }
                req_L1_evict(curr_time, set_it, (set_it->tag*num_sets + set) << set_lsb, req_lqe, false);
              }
              num_upgrade_req++;
              is_coherence_miss = true;
              break;
            }
          } else if (set_it->type == cs_invalid || set_it->type == cs_tr_to_e) {
            continue;
          } else if (set_it->tag == tag) {
            if (set_it->type == cs_modified && set_it->type_l1l2 == cs_invalid) {
              // cache hit, and type_l1l2 will be cs_modified
            } else if (set_it->type == cs_modified && set_it->type_l1l2 == cs_modified) {
              // cache hit, and type_l1l2 will be cs_modified
              if (set_it->sharedl1.size() != 1) {
                LOG(FATAL) << "[" << curr_time << "] " << *set_it << std::endl << *this << *req_lqe << *geq;
              }
              if (*(set_it->sharedl1.begin()) != req_lqe->from.top()) {
                enter_intermediate_state = true;
                req_lqe->from.push(this);
                req_lqe->type = et_m_to_m;
                (*(set_it->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
                set_it->type_l1l2 = cs_tr_to_m;
                set_it->sharedl1.clear();
                set_it->last_access_time = curr_time;
                hit = true;
                update_LRU(idx, tags_set, set_it);
                break;
              }
            } else if (set_it->type == cs_modified && set_it->type_l1l2 == cs_exclusive) {
              // cache hit, and type_l1l2 will be cs_modified
              if (set_it->sharedl1.size() > 1) {
                LOG(FATAL) << "[" << curr_time << "] " << *set_it << std::endl << *this << *req_lqe << *geq;
              }

              if (set_it->sharedl1.empty() == false && (*(set_it->sharedl1.begin())) != req_lqe->from.top()) {
                auto lqe = new LocalQueueElement(this, et_evict,
                    ((set_it->tag*num_sets + set) << set_lsb), req_lqe->th_id);
                (*(set_it->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
                set_it->sharedl1.clear();
              }
            } else if (set_it->type == cs_modified && set_it->type_l1l2 == cs_shared) {
              // cache hit, and type_l1l2 will be cs_modified
              req_L1_evict(curr_time, set_it, (set_it->tag*num_sets + set) << set_lsb, req_lqe, false);
            } else if ((set_it->type_l1l2 == cs_exclusive && set_it->type == cs_tr_to_s) ||
                set_it->type_l1l2 == cs_tr_to_s || set_it->type_l1l2 == cs_tr_to_m ||
                set_it->type_l1l2 == cs_tr_to_i || set_it->type == cs_tr_to_m) {
              set_it->last_access_time = curr_time;
              req_lqe->type = et_nack;
              hit = true;
              update_LRU(idx, tags_set, set_it);
              break;
            } else {
              LOG(FATAL) << "[" << curr_time << "] " << *set_it << std::endl << *this << *req_lqe << *geq;
            }

            set_it->last_access_time = curr_time;
            set_it->type_l1l2 = cs_modified;
            set_it->sharedl1.insert(req_lqe->from.top());
            hit = true;
            update_LRU(idx, tags_set, set_it);
            break;
          }
        }
      }

      if (enter_intermediate_state == false) {
        if (hit == false) {
          if (is_coherence_miss == false) {
            (etype == et_write) ? num_wr_miss++ : num_rd_miss++;
          }

          req_lqe->from.push(this);
          if (geq->which_mc(address) == directory->num) {
            directory->add_req_event(curr_time + l2_to_dir_t, req_lqe);
          } else {
            crossbar->add_req_event(curr_time + l2_to_xbar_t, req_lqe, this);
          }
        } else if (req_lqe->from.size() > 1) {
          req_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
        }
      }
    }

    if (any_request == false) {
      LOG(FATAL) << *(req_event_iter->second) << *geq;
    }
  }

  if (rep_q.empty() == false) {
    geq->add_event(curr_time + process_interval, this);
  } else {
    for (auto && req_q : req_qs) {
      if (req_q.empty() == false) {
        geq->add_event(curr_time + process_interval, this);
        break;
      }
    }
  }

  return 0;
}


void CacheL2::add_event_to_LL(
    uint64_t curr_time,
    LocalQueueElement * lqe,
    bool check_top,
    bool is_data) {
  if ((check_top == true  && lqe->from.top() == directory) ||
      (check_top == false && geq->which_mc(lqe->address) == directory->num)) {
    directory->add_rep_event(curr_time + l2_to_dir_t, lqe);
  } else {
    if (is_data) {
      crossbar->add_rep_event(curr_time+l2_to_xbar_t, lqe, num_flits_per_packet, this);
    } else {
      crossbar->add_rep_event(curr_time+l2_to_xbar_t, lqe, this);
    }
  }
}


void CacheL2::test_tags(uint32_t set) {
  std::set<uint64_t> tag_set;
  for (uint32_t k = 0; k < num_ways; k++) {
    L2Entry * iter = tags[set][k];

    if (iter->type != cs_invalid) {
      std::stringstream ss;
      if (tag_set.find(iter->tag) != tag_set.end()) {
        for (uint32_t kk = 0; kk < num_ways; kk++) {
          ss << tags[set][kk]->type << tags[set][kk]->tag << ", ";
        }
        LOG(FATAL) << ss.str() << std::endl;
      }
      tag_set.insert(iter->tag);
    }
  }
}


void CacheL2::update_LRU(uint32_t idx, L2Entry ** tags_set, L2Entry * const set_it) {
  for (uint32_t i = idx; i < num_ways-1; i++) {
    tags_set[i] = tags_set[i+1];
  }
  tags_set[num_ways-1] = set_it;
}


void CacheL2::req_L1_evict(uint64_t curr_time, L2Entry * const set_it, uint64_t addr, LocalQueueElement * lqe, bool always) {
  for (auto && it : set_it->sharedl1) {
    if (always == true || it != lqe->from.top()) {
      auto new_lqe = new LocalQueueElement(this, et_evict, addr, lqe->th_id);
      it->add_rep_event(curr_time + l2_to_l1_t, new_lqe);
    }
  }
  set_it->sharedl1.clear();
}

}  // namespace PinPthread

