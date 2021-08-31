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
#include <sstream>

#include "PTSDirectory.h"
#include "PTSMemoryController.h"
#include "PTSXbar.h"


namespace PinPthread {

std::ostream & operator << (std::ostream & output, mc_bank_action action) {
  switch (action) {
    case mc_bank_activate:  output << "ACT"; break;
    case mc_bank_read:      output << "RD"; break;
    case mc_bank_write:     output << "WR"; break;
    case mc_bank_precharge: output << "PRE"; break;
    case mc_bank_idle:      output << "IDL"; break;
    default: break;
  }
  return output;
}

std::ostream & operator << (std::ostream & output, mc_scheduling_policy policy) {
  switch (policy) {
    case mc_scheduling_open:   output << "open"; break;
    case mc_scheduling_closed: output << "closed"; break;
    case mc_scheduling_pred:   output << "pred"; break;
    default: break;
  }
  return output;
}

MemoryController::MemoryController(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Component(type_, num_, mcsim_),
  req_l(),
  tRCD(get_param_uint64("tRCD", 10)),
  tRR(get_param_uint64("tRR",  5)),
  tRP(get_param_uint64("tRP",  tRP)),
  tCL(get_param_uint64("tCL",  10)),
  tBL(get_param_uint64("tBL",  10)),
  tBBL(get_param_uint64("tBBL", tBL)),
  tRAS(get_param_uint64("tRAS", 15)),
  tWRBUB(get_param_uint64("tWRBUB", 2)),
  tRWBUB(get_param_uint64("tRWBUB", 2)),
  tRRBUB(get_param_uint64("tRRBUB", 2)),
  tWTR(get_param_uint64("tWTR", 8)),
  req_window_sz(get_param_uint64("req_window_sz", 16)),
  mc_to_dir_t(get_param_uint64("to_dir_t", 1000)),
  rank_interleave_base_bit(get_param_uint64("rank_interleave_base_bit", 14)),
  bank_interleave_base_bit(get_param_uint64("bank_interleave_base_bit", 14)),
  page_sz_base_bit(get_param_uint64("page_sz_base_bit", 12)),
  mc_interleave_base_bit(get_param_uint64("interleave_base_bit", 12)),
  interleave_xor_base_bit(get_param_uint64("interleave_xor_base_bit", 20)),
  addr_offset_lsb(get_param_uint64("addr_offset_lsb", "", 48)),
  num_hthreads(mcsim_->get_num_hthreads()),
  num_mcs(get_param_uint64("num_mcs", "pts.", 2)),
  num_ranks_per_mc(get_param_uint64("num_ranks_per_mc", 1)),
  num_banks_per_rank(get_param_uint64("num_banks_per_rank", 8)),
  num_pages_per_bank(get_param_uint64("num_pages_per_bank", 8192)),
  num_cached_pages_per_bank(get_param_uint64("num_cached_pages_per_bank", 4)),
  num_pred_entries(get_param_uint64("num_pred_entries", 1)),
  par_bs(get_param_bool("par_bs", false)),
  num_reqs(0), num_read(0), num_write(0), num_activate(0),
  num_precharge(0), num_write_to_read_switch(0), num_refresh(0),
  num_pred_miss(0), num_pred_hit(0), num_global_pred_miss(0),
  num_global_pred_hit(0), pred_history(num_hthreads, 0),
  num_rw_interval(0), num_conflict_interval(0), num_pre_interval(0),
  curr_refresh_page(0), curr_refresh_bank(0), curr_batch_last(-1),
  num_req_from_a_th(num_hthreads, 0),
  bank_status(num_ranks_per_mc, std::vector<BankStatus>(num_banks_per_rank, BankStatus(num_pred_entries))),
  last_activate_time(num_ranks_per_mc, 0),
  last_write_time(num_ranks_per_mc, 0),
  last_read_time(std::pair<uint32_t, uint64_t>(0, 0)),
  last_read_time_rank(num_ranks_per_mc, 0),
  is_last_time_write(num_ranks_per_mc, false),
  dp_status(), rd_dp_status(), wr_dp_status(),
  refresh_interval(get_param_uint64("refresh_interval",  0)),
  full_duplex(get_param_bool("full_duplex", false)),
  is_fixed_latency(get_param_bool("is_fixed_latency", false)),
  is_fixed_bw_n_latency(get_param_bool("is_fixed_bw_n_latency", false)),
  last_process_time(0), packet_time_in_mc_acc(0),
  display_os_page_usage_summary(get_param_bool("display_os_page_usage_summary", false)),
  display_os_page_usage(get_param_bool("display_os_page_usage", false)) {
  process_interval = get_param_uint64("process_interval", 10);
  // TODO(gajh): refresh implementation should be restored soon.
  if (refresh_interval != 0) geq->add_event(refresh_interval, this);

  if (get_param_str("scheduling_policy") == "open") {
    policy = mc_scheduling_open;
  } else if (get_param_str("scheduling_policy") == "pred") {
    policy = mc_scheduling_pred;
  } else {
    policy = mc_scheduling_closed;
  }
  // variables below are used to find page_num quickly
  std::multimap<uint32_t, uint32_t> interleavers;
  interleavers.insert(std::pair<uint32_t, uint32_t>(rank_interleave_base_bit, num_ranks_per_mc));
  interleavers.insert(std::pair<uint32_t, uint32_t>(bank_interleave_base_bit, num_banks_per_rank));
  interleavers.insert(std::pair<uint32_t, uint32_t>(mc_interleave_base_bit,   num_mcs));

  std::multimap<uint32_t, uint32_t>::iterator iter = interleavers.begin();
  base2 = iter->first; width2 = iter->second; ++iter;
  base1 = iter->first; width1 = iter->second; ++iter;
  base0 = iter->first; width0 = iter->second; ++iter;
}


MemoryController::~MemoryController() {
  if (num_read > 0) {
    update_acc_dist();
    std::cout << "  -- MC  [" << std::setw(3) << num << "] : (rd, wr, act, pre) = ("
         << std::setw(9) << num_read << ", " << std::setw(9) << num_write << ", "
         << std::setw(9) << num_activate << ", " << std::setw(9) << num_precharge
         << "), # of WR->RD switch = " << num_write_to_read_switch
         << ", #_refresh = " << num_refresh << ", "
         << os_page_acc_dist.size() << " pages acc, "
         << "avg_tick_in_mc= " << packet_time_in_mc_acc / (num_read + num_write) << std::endl;
    std::cout << "  -- MC  [" << std::setw(3) << num << "] : "
         << "local pred (miss,hit)=( " << num_pred_miss << ", " << num_pred_hit << "), "
         << "global pred (miss,hit)=( " << num_global_pred_miss << ", " << num_global_pred_hit << ")" << std::endl;
  }
  if (display_os_page_usage == true) {
    for (auto && iter : os_page_acc_dist) {
      std::cout << "  -- page 0x" << std::setfill('0') << std::setw(8) << std::hex << iter.first * (1 << page_sz_base_bit)
           << std::setfill(' ') << std::dec << " is accessed ("
           << std::setw(7) << mcsim->os_page_req_dist[iter.first]
           << ", " << std::setw(7) << iter.second << ") times at (Core, MC)." <<  std::endl;
    }
  }
}


void MemoryController::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  event_time = ceil_by_y(event_time, process_interval);

  if (is_fixed_latency == true) {
    if (local_event->type == et_evict || local_event->type == et_dir_evict) {
      delete local_event;
    } else {
      directory->add_rep_event(event_time + mc_to_dir_t, local_event);
    }
  } else if (is_fixed_bw_n_latency == true) {
    last_process_time = (event_time == 0 || event_time > last_process_time) ? event_time : (last_process_time + process_interval);

    if (local_event->type == et_evict || local_event->type == et_dir_evict) {
      num_write++;
      delete local_event;
    } else {
      num_read++;
      directory->add_rep_event(last_process_time + mc_to_dir_t, local_event);
    }
  } else {
    geq->add_event(event_time, this);
    req_event.insert(std::pair<uint64_t, LocalQueueElement *>(event_time, local_event));
    // check_bank_status(local_event);
  }

  // update access distribution
  num_reqs++;
  uint64_t page_num = get_page_num(local_event->address);
  auto p_iter = os_page_acc_dist_curr.find(page_num);

  if (p_iter != os_page_acc_dist_curr.end()) {
    (p_iter->second)++;
  } else {
    os_page_acc_dist_curr.insert(std::pair<uint64_t, uint64_t>(page_num, 1));
  }
}


void MemoryController::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  add_req_event(event_time, local_event, from);
}


uint64_t MemoryController::get_page_num(uint64_t addr) {
  uint64_t page_num = addr;

  page_num = (((page_num >> base0) / width0) << base0) + (page_num % (1 << base0));
  page_num = (((page_num >> base1) / width1) << base1) + (page_num % (1 << base1));
  page_num = (((page_num >> base2) / width2) << base2) + (page_num % (1 << base2));

  return (page_num >> page_sz_base_bit);
}


void MemoryController::show_state(uint64_t curr_time) {
  LOG(INFO) << "  -- MC  [" << num << "] : curr_time = " << curr_time << std::endl;

  uint32_t i = 0;
  for (auto && it : req_l) {
    uint32_t rank_num = get_rank_num(it->address);
    uint32_t bank_num = get_bank_num(it->address);
    std::cout << "  -- req_l[" << std::setw(2) <<  i << "] = (" << rank_num << ", " << bank_num
      << ", 0x" << std::hex << get_page_num(it->address) << std::dec << ") : " << std::hex
      << reinterpret_cast<uint64_t *>(it) << std::dec << " : " << *it;
    i++;
  }
  if (curr_batch_last >= 0) {
    std::cout << "  -- current batch ends at req_l[" << std::setw(2) << curr_batch_last << "]\n";
  }

  for (uint32_t i = 0; i < num_ranks_per_mc; i++) {
    for (uint32_t k = 0; k < num_banks_per_rank; k++) {
      std::cout << "  -- bank_status[" << std::setw(2) <<  i << "][" << std::setw(2) << k << "] = ("
        << std::setw(10) << bank_status[i][k].action_time << ", "
        << std::hex << "0x" << bank_status[i][k].page_num << std::dec << ", "
        << bank_status[i][k].action_type << ")" << std::endl;
    }
  }

  for (uint32_t i = 0; i < num_ranks_per_mc; i++) {
    std::cout << "  -- last_act_time[" << i << "] = " << last_activate_time[i] << std::endl;
  }

  auto miter = dp_status.begin();
  while (miter != dp_status.end()) {
    std::cout << "  -- data_path_status = ("
      << miter->first << ", "
      << miter->second << ")" << std::endl;
    ++miter;
  }
  miter = rd_dp_status.begin();
  while (miter != rd_dp_status.end()) {
    std::cout << "  -- rd_dp_status = ("
      << miter->first << ", "
      << miter->second << ")" << std::endl;
    ++miter;
  }
  miter = wr_dp_status.begin();
  while (miter != wr_dp_status.end()) {
    std::cout << "  -- wr_dp_status = ("
      << miter->first << ", "
      << miter->second << ")" << std::endl;
    ++miter;
  }
}


uint32_t MemoryController::process_event(uint64_t curr_time) {
  if (last_process_time > 0) {
    packet_time_in_mc_acc += (curr_time - last_process_time) * req_l.size();
  }
  last_process_time = curr_time;

  pre_processing(curr_time);
  uint32_t i;
  std::vector<LocalQueueElement *>::iterator iter, iter2;

  int32_t c_idx    = -1;  // candidate index
  auto c_iter      = req_l.end();
  bool    page_hit = false;
  int32_t num_req_from_the_same_thread = req_l.size() + 1;

  // first, find a request that can be serviced at this cycle
  for (iter = req_l.begin(), i = 0; iter != req_l.end(); ++iter, ++i) {
    if (c_idx >= 0 && iter != req_l.begin() && (int32_t)i > curr_batch_last) {
      // we found a candidate from the ready batch already
      break;
    }

    if (i >= req_window_sz)
      break;

    // check constraints
    const uint64_t & address  = (*iter)->address;
    const event_type type   = (*iter)->type;
    const uint32_t th_id   = (*iter)->th_id;
    const uint32_t rank_num = get_rank_num(address);
    const uint32_t bank_num = get_bank_num(address);
    const uint64_t page_num = get_page_num(address);

    BankStatus & curr_bank = bank_status[rank_num][bank_num];
    std::map<uint64_t, mc_bank_action>::iterator miter, rd_miter, wr_miter;

    if (curr_bank.action_time > curr_time) {
      continue;
    }
    switch (curr_bank.action_type) {
      case mc_bank_precharge:
        if (curr_bank.action_time + tRP*process_interval > curr_time) {
          break;
        }
      case mc_bank_idle:
        if (page_hit == false &&  // page_hit has priority 2
            last_activate_time[rank_num] + tRR*process_interval <= curr_time) {
          int32_t n_req_from_curr_th = num_req_from_a_th[th_id];
          if (n_req_from_curr_th < num_req_from_the_same_thread ||
              (n_req_from_curr_th == num_req_from_the_same_thread && c_idx == -1)) {
            c_idx  = i;
            c_iter = iter;
            num_req_from_the_same_thread = n_req_from_curr_th;
          }
        }
        break;
      case mc_bank_activate:
        if (curr_bank.action_time + tRCD*process_interval > curr_time) {
          break;
        }
      case mc_bank_read:
      case mc_bank_write:
        if (curr_bank.action_time + tBBL*process_interval > curr_time) {
          break;
        }
        if (curr_bank.page_num != page_num) {  // row miss
          if (policy == mc_scheduling_open) {
            uint32_t k = 0;
            bool need_precharge = true;
            iter2 = req_l.begin();

            while (iter2 != req_l.end() && k++ < req_window_sz) {
              if ((int32_t)i <= curr_batch_last && (int32_t)k > curr_batch_last + 1) {
                break;
              }
              if (iter == iter2) {
                iter2++;
                continue;
              }
              if (rank_num == get_rank_num((*iter2)->address) &&
                  bank_num == get_bank_num((*iter2)->address) &&
                  curr_bank.page_num == get_page_num((*iter2)->address)) {
                need_precharge = false;
                break;
              }
              iter2++;
            }

            if (need_precharge == true &&
                page_hit == false &&  // page_hit has priority 2
                last_activate_time[rank_num] + tRR*process_interval <= curr_time) {
              int32_t n_req_from_curr_th = num_req_from_a_th[th_id];
              if (n_req_from_curr_th < num_req_from_the_same_thread ||
                  (n_req_from_curr_th == num_req_from_the_same_thread && c_idx == -1)) {
                c_idx  = i;
                c_iter = iter;
                num_req_from_the_same_thread = n_req_from_curr_th;
              }
            }
          }
        } else {  // row hit
          dp_status.erase(dp_status.begin(), dp_status.lower_bound(curr_time));
          auto miter = dp_status.lower_bound(curr_time + tCL*process_interval);
          bool met_constraints = false;
          switch (type) {
            case et_rd_dir_info_req:
            case et_rd_dir_info_rep:
            case et_read:
            case et_e_rd:
            case et_s_rd:
              // read
              rd_dp_status.erase(rd_dp_status.begin(), rd_dp_status.lower_bound(curr_time));
              rd_miter = rd_dp_status.lower_bound(curr_time + tCL*process_interval);
              if ((full_duplex == false &&
                    (miter == dp_status.end() || miter->first >= curr_time + (tCL+tBL)*process_interval)) ||
                  (full_duplex == true &&
                   (rd_miter == rd_dp_status.end() || rd_miter->first >= curr_time + (tCL+tBL)*process_interval))) {
                bool wrbub = false;
                // WRBUB
                miter = dp_status.lower_bound(curr_time + tCL*process_interval - tWRBUB*process_interval);
                while (full_duplex == false &&
                    miter != dp_status.end() &&
                    miter->first < curr_time + tCL*process_interval) {
                  if (miter->second == mc_bank_write) {
                    wrbub = true;
                    break;
                  }
                  ++miter;
                }

                if (wrbub == false && tWTR > 0 && last_write_time[rank_num] + tWTR*process_interval > curr_time) {
                  wrbub = true;  // tWTR constraint
                }

                if (wrbub == false && last_read_time.first != rank_num &&
                    curr_time < tRRBUB*process_interval + last_read_time.second) {
                  wrbub = true;  // tRRBUB
                }

                if (wrbub == false) {  // service the read request
                  met_constraints = true;
                }
              }
              break;
            case et_evict:
            case et_dir_evict:
            case et_s_rd_wr:
              // write
              wr_dp_status.erase(wr_dp_status.begin(), wr_dp_status.lower_bound(curr_time));
              wr_miter = wr_dp_status.lower_bound(curr_time + tCL*process_interval);
              if ((full_duplex == false &&
                    (miter == dp_status.end() || miter->first >= curr_time + (tCL+tBL)*process_interval)) ||
                  (full_duplex == true  &&
                   (wr_miter == wr_dp_status.end() || wr_miter->first >= curr_time + (tCL+tBL)*process_interval))) {
                bool rwbub = false;
                // RWBUB
                miter = dp_status.lower_bound(curr_time + tCL*process_interval - tRWBUB*process_interval);
                while (full_duplex == false &&
                    miter != dp_status.end() &&
                    miter->first < curr_time + tCL*process_interval) {
                  if (miter->second == mc_bank_read) {
                    rwbub = true;
                    break;
                  }
                  ++miter;
                }

                if (rwbub == false && last_read_time_rank[rank_num] + tRWBUB*process_interval > curr_time) {
                  rwbub = true;  // tRTW constraint
                }

                if (rwbub == false) {
                  met_constraints = true;  // service the write request
                }
              }
              break;
            default:
              show_state(curr_time);
              LOG(FATAL) << "currenly at req_l[" << i << "]\n";
              break;
          }
          if (met_constraints == true &&
              (page_hit == false ||  // this request is page_hit
               num_req_from_a_th[th_id] < num_req_from_the_same_thread)) {
            c_idx  = i;
            c_iter = iter;
            page_hit = true;
            num_req_from_the_same_thread = num_req_from_a_th[th_id];
          }
        }
        break;
      default:
        show_state(curr_time);
        LOG(FATAL) << "currenly at req_l[" << i << "]\n";
        break;
    }
  }

  if (c_idx >= 0) {
    i    = c_idx;
    iter = c_iter;
    // check bank_status
    const uint64_t address  = (*iter)->address;
    const event_type type   = (*iter)->type;
    const uint32_t th_id = (*iter)->th_id;
    const uint32_t rank_num = get_rank_num(address);
    const uint32_t bank_num = get_bank_num(address);
    const uint64_t page_num = get_page_num(address);
    BankStatus & curr_bank = bank_status[rank_num][bank_num];

    std::map<uint64_t, mc_bank_action>::iterator miter, rd_miter, wr_miter;

    switch (curr_bank.action_type) {
      case mc_bank_precharge:
      case mc_bank_idle:
        curr_bank.action_time  = curr_time;
        curr_bank.page_num     = page_num;
        curr_bank.action_type  = mc_bank_activate;
        last_activate_time[rank_num] = curr_time;
        curr_bank.last_activate_time = curr_time;
        num_activate++;
        break;
      case mc_bank_activate:
      case mc_bank_read:
      case mc_bank_write:
        if (curr_bank.page_num != page_num) {  // row miss
          if (policy == mc_scheduling_open) {
            num_precharge++;
            curr_bank.action_time = tRP*process_interval +
                std::max(curr_time, tRAS*process_interval + curr_bank.last_activate_time);
            curr_bank.action_type = mc_bank_precharge;
          }
          break;
        } else {  // row hit
          mc_bank_action curr_action_type;
          switch (type) {
            case et_rd_dir_info_req:
            case et_rd_dir_info_rep:
            case et_read:
            case et_e_rd:
            case et_s_rd:
              // read
              if (is_last_time_write[rank_num] == true) {
                is_last_time_write[rank_num] = false;
                num_write_to_read_switch++;
              }
              num_read++;
              curr_bank.action_time = curr_time;

              for (uint32_t j = 0; j < tBL; j++) {
                uint64_t next_time = curr_time + (tCL+j)*process_interval;
                dp_status.insert(std::pair{next_time, mc_bank_read});
                rd_dp_status.insert(std::pair{next_time, mc_bank_read});
              }
              last_read_time.first  = rank_num;
              last_read_time.second = curr_time;  // + (tCL + tBL)*process_interval;
              last_read_time_rank[rank_num] = curr_time + (tCL + tBL)*process_interval;
              directory->add_rep_event(curr_time + mc_to_dir_t, *iter);

              curr_action_type = mc_bank_read;
              break;
            case et_evict:
            case et_dir_evict:
            case et_s_rd_wr:
              // write
              is_last_time_write[rank_num] = true;
              num_write++;
              curr_bank.action_time = curr_time;

              for (uint32_t j = 0; j < tBL; j++) {
                uint64_t next_time = curr_time + (tCL+j)*process_interval;
                dp_status.insert(std::pair{next_time, mc_bank_write});
                wr_dp_status.insert(std::pair{next_time, mc_bank_write});
              }
              last_write_time[rank_num] = curr_time + (tCL+tBL)*process_interval;

              if (type == et_s_rd_wr) {
                (*iter)->type = et_s_rd;
                directory->add_rep_event(curr_time + mc_to_dir_t, *iter);
              } else {
                delete *iter;
              }

              curr_action_type = mc_bank_write;
              break;
            default:
              show_state(curr_time);
              LOG(FATAL) << "currenly at req_l[" << i << "]\n";
              break;
          }
          if (par_bs == true) num_req_from_a_th[th_id]--;
          if (policy == mc_scheduling_open) {
            curr_bank.action_type = curr_action_type;
          } else {
            num_precharge++;
            curr_bank.action_time = tRP*process_interval +
              std::max(curr_time, tRAS*process_interval + curr_bank.last_activate_time);
            curr_bank.action_type = mc_bank_precharge;
            iter2 = iter;
            uint32_t i2 = i;
            ++iter2;
            ++i2;
            for ( ; iter2 != req_l.end() && i2 < req_window_sz; ++iter2, ++i2) {
              if (rank_num == get_rank_num((*iter2)->address) &&
                  bank_num == get_bank_num((*iter2)->address) &&
                  page_num == get_page_num((*iter2)->address)) {
                curr_bank.action_type = curr_action_type;
                num_precharge--;
                curr_bank.action_time -= tRP*process_interval;
                break;
              }
            }
          }

          if (par_bs == true && curr_batch_last == (int32_t)i) {
            if (i == 0) {
              curr_batch_last = std::min((int32_t)req_window_sz-1, (int32_t)req_l.size()-2);
            } else {
              curr_batch_last--;
            }
          } else if (par_bs == true && curr_batch_last > (int32_t)i) {
            curr_batch_last--;
          }
          req_l.erase(iter);
        }
        break;
      default:
        show_state(curr_time);
        LOG(FATAL) << "currenly at req_l[" << i << "]\n";
        break;
    }
  }

  if (req_l.empty() == false) {
    geq->add_event(curr_time + process_interval, this);
  }
  return 0;
}


void MemoryController::pre_processing(uint64_t curr_time) {
  auto req_event_iter = req_event.begin();

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time) {
    if (par_bs == true) {
      num_req_from_a_th[req_event_iter->second->th_id]++;
    }
    req_l.push_back(req_event_iter->second);
    req_event_iter = req_event.erase(req_event_iter);
  }
  if (par_bs == true && curr_batch_last == -1 && req_l.size() > 0) {
    curr_batch_last = std::min(static_cast<uint32_t>(req_l.size()), req_window_sz) - 1;
  }
}


void MemoryController::update_acc_dist() {
  for (auto && it : os_page_acc_dist_curr) {
    auto p_iter = os_page_acc_dist.find(it.first);

    if (p_iter == os_page_acc_dist.end()) {
      os_page_acc_dist.insert(std::pair<uint64_t, uint64_t>(it.first, 1));
    } else {
      p_iter->second += it.second;
    }
  }

  os_page_acc_dist_curr.clear();
}


void MemoryController::check_bank_status(LocalQueueElement * local_event) {
  uint64_t address  = (local_event)->address;
  uint32_t rank_num = get_rank_num(address);
  uint32_t bank_num = get_bank_num(address);
  uint64_t page_num = get_page_num(address);

  BankStatus & curr_bank = bank_status[rank_num][bank_num];

  if (page_num != curr_bank.page_num) {
    num_conflict_interval++;
  }
  num_rw_interval++;
}

}  // namespace PinPthread
