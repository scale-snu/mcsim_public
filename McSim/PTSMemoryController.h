// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTSMEMORYCONTROLLER_H_
#define PTSMEMORYCONTROLLER_H_

#include <list>
#include <map>
#include <stack>
#include <queue>
#include <utility>
#include <vector>
#include <set>

#include "McSim.h"

// some of the acronyms used here
// ------------------------------
//  pd  : powerdown
//  vmd : virtual memory device


namespace PinPthread {

enum mc_bank_action {
  mc_bank_activate,
  mc_bank_read,
  mc_bank_write,
  mc_bank_precharge,
  mc_bank_idle,
};

enum mc_scheduling_policy {
  mc_scheduling_open,
  mc_scheduling_closed,
  mc_scheduling_pred,
};


class MemoryController : public Component {
 public:
  explicit MemoryController(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~MemoryController();

  void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  uint32_t process_event(uint64_t curr_time);
  void show_state(uint64_t curr_time);

  // Directory * directory;  // uplink
  Component * directory;  // uplink
  std::vector<LocalQueueElement *> req_l;

  class BankStatus {
   public:
    explicit BankStatus(uint32_t num_entries):
      action_time(0), page_num(0), action_type(mc_bank_idle),
      last_activate_time(0) { }

    uint64_t action_time;
    uint64_t page_num;
    mc_bank_action action_type;
    uint64_t last_activate_time;
  };

  // The unit of each t* value is "process_interval"
  // assume that RL = WL (in terms of DDRx)
  const uint32_t tRCD;
  const uint32_t tRR;
  const uint32_t tRP;
  const uint32_t tCL;           // CAS latency
  const uint32_t tBL;           // burst length
  const uint32_t tBBL;          // bank burst length
  const uint32_t tRAS;          // activate to precharge
  // send multiple column level commands
  const uint32_t tWRBUB;        // WR->RD bubble between any ranks
  const uint32_t tRWBUB;        // RD->WR bubble between any ranks
  const uint32_t tRRBUB;        // RD->RD bubble between two different ranks
  const uint32_t tWTR;          // WR->RD time in the same rank
  const uint32_t req_window_sz;  // up to how many requests can be considered during scheduling
  const uint32_t mc_to_dir_t;

  const uint32_t rank_interleave_base_bit;
  const uint32_t bank_interleave_base_bit;
  const uint64_t page_sz_base_bit;   // byte addressing
  const uint32_t mc_interleave_base_bit;
  const uint32_t interleave_xor_base_bit;
  const uint32_t addr_offset_lsb;

  const uint32_t num_hthreads;
  const uint32_t num_mcs;
  const uint32_t num_ranks_per_mc;
  const uint32_t num_banks_per_rank;
  const uint64_t num_pages_per_bank;  // set to public for MC unit test for a while
  const uint64_t num_cached_pages_per_bank;
  const uint32_t num_pred_entries;

  const bool     par_bs;  // parallelism-aware batch-scheduling

  uint64_t num_reqs;

 protected:
  mc_scheduling_policy policy;

  uint64_t num_read;
  uint64_t num_write;
  uint64_t num_activate;
  uint64_t num_precharge;
  uint64_t num_write_to_read_switch;
  uint64_t num_refresh;  // a refresh command is applied to all VMD/BANK in a rank
  uint64_t num_pred_miss;
  uint64_t num_pred_hit;
  uint64_t num_global_pred_miss;
  uint64_t num_global_pred_hit;
  std::vector<uint64_t> pred_history;              // size : num_hthreads

  uint64_t num_rw_interval;
  uint64_t num_conflict_interval;
  uint64_t num_pre_interval;

  uint64_t curr_refresh_page;
  uint64_t curr_refresh_bank;

  int32_t curr_batch_last;
  std::vector<int32_t> num_req_from_a_th;

  std::vector<std::vector<BankStatus>> bank_status;  // [rank][bank]
  std::vector<uint64_t> last_activate_time;          // [rank]
  std::vector<uint64_t> last_write_time;             // [rank]
  std::pair<uint32_t, uint64_t> last_read_time;      // <rank, tick>
  std::vector<uint64_t> last_read_time_rank;         // [rank]
  std::vector<bool>     is_last_time_write;          // [rank]
  std::map<uint64_t, mc_bank_action> dp_status;      // reuse (RD,WR,IDLE) BankStatus
  std::map<uint64_t, mc_bank_action> rd_dp_status;   // reuse (RD,WR,IDLE) BankStatus
  std::map<uint64_t, mc_bank_action> wr_dp_status;   // reuse (RD,WR,IDLE) BankStatus

  const uint64_t refresh_interval;
  const bool     full_duplex;
  const bool     is_fixed_latency;       // infinite BW
  const bool     is_fixed_bw_n_latency;  // take care of BW as well

  uint64_t last_process_time;
  uint64_t packet_time_in_mc_acc;

  uint32_t base0, base1, base2;
  uint32_t width0, width1, width2;

  void pre_processing(uint64_t curr_time);
  void check_bank_status(LocalQueueElement * local_event);
  inline uint32_t get_rank_num(uint64_t addr) {
    return ((addr >> rank_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % num_ranks_per_mc;
  }
  inline uint32_t get_bank_num(uint64_t addr) {
    return ((addr >> bank_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % num_banks_per_rank;
  }
  uint64_t get_page_num(uint64_t addr);

 public:
  std::map<uint64_t, uint64_t> os_page_acc_dist;       // os page access distribution
  std::map<uint64_t, uint64_t> os_page_acc_dist_curr;  // os page access distribution
  const bool     display_os_page_usage_summary;
  const bool     display_os_page_usage;
  void     update_acc_dist();
};

}  // namespace PinPthread

#endif  // PTSMEMORYCONTROLLER_H_
