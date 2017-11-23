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
#include "PTSO3Core.h"
#include "PTSTLB.h"
#include <iomanip>
#include <sstream>

using namespace PinPthread;

static const int32_t word_log = 3;


ostream& operator<<(ostream & output, o3_instr_queue_state iqs)
{
  switch (iqs)
  {
    case o3iqs_not_in_queue:  output << "iqs_niq"; break;
    case o3iqs_being_loaded:  output << "iqs_bld"; break;
    case o3iqs_ready:         output << "iqs_rdy"; break;
    default:                  output << "iqs_inv"; break;
  }
  return output;
}


ostream & operator<<(ostream & output, o3_instr_rob_state irs)
{
  switch (irs)
  {
    case o3irs_issued:     output << "irs_iss"; break;
    case o3irs_executing:  output << "irs_exe"; break;
    case o3irs_completed:  output << "irs_cmp"; break;
    default:               output << "irs_inv"; break;
  }
  return output;
}


extern ostream & operator << (ostream & output, ins_type it);

O3Core::O3Core(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  num_hthreads(get_param_uint64("num_hthreads", "pts.", max_hthreads)),
  is_active(),
  last_time_no_mem_served(0),
  last_time_mem_served(0),
  num_bubbled_slots(0),
  active(false), stack(0), stacksize(0),
  resume_time(0),
  num_instrs(0), num_branch(0), num_branch_miss(0), num_nacks(0),
  num_consecutive_nacks(0), 
  num_x87_ops(0), num_call_ops(0), total_mem_wr_time(0),
  total_mem_rd_time(0), total_dependency_distance(0),
  lsu_to_l1i_t(get_param_uint64("to_l1i_t", 10)),
  lsu_to_l1d_t(get_param_uint64("to_l1d_t", 10)),
  lsu_to_l1i_t_for_x87_op(get_param_uint64("to_l1i_t_for_x87_op", lsu_to_l1i_t)),
  branch_miss_penalty(get_param_uint64("branch_miss_penalty", 100)),
  spinning_slowdown(get_param_uint64("spinning_slowdown", 10)),
  lock_t(get_param_uint64("lock_t", 100)),
  unlock_t(get_param_uint64("unlock_t", 100)),
  barrier_t(get_param_uint64("barrier_t", 100)),
  sse_t(get_param_uint64("sse_t", 40)),
  consecutive_nack_threshold(get_param_uint64("consecutive_nack_threshold", 1000)),
  was_nack(false),
  latest_ip(0), latest_bmp_time(0)
{
  process_interval    = get_param_uint64("process_interval", 80);
  branch_miss_penalty = ((branch_miss_penalty + process_interval - 1)/process_interval)*process_interval;
  lock_t              = ((lock_t              + process_interval - 1)/process_interval)*process_interval;
  unlock_t            = ((unlock_t            + process_interval - 1)/process_interval)*process_interval;
  barrier_t           = ((barrier_t           + process_interval - 1)/process_interval)*process_interval;           

  bp = new BranchPredictor(
      get_param_uint64("num_bp_entries", 256),
      get_param_uint64("gp_size", 0));
  ASSERTX(mem_acc.empty());

  bypass_tlb      = get_param_str("bypass_tlb")      == "true" ? true : false;
  display_barrier = get_param_str("display_barrier") == "true" ? true : false;
  mimick_inorder  = get_param_str("mimick_inorder")  == "true" ? true : false;

  o3queue_max_size = get_param_uint64("o3queue_max_size", 64) + 4;
  o3rob_max_size   = get_param_uint64("o3rob_max_size",   16);
  max_issue_width  = get_param_uint64("max_issue_width",   4);
  max_commit_width = get_param_uint64("max_commit_width",  4);
  max_alu          = get_param_uint64("max_alu", max_commit_width);
  max_ldst         = get_param_uint64("max_ldst", max_commit_width);
  max_ld           = get_param_uint64("max_ld", max_commit_width);
  max_st           = get_param_uint64("max_st", max_commit_width);
  max_sse          = get_param_uint64("max_sse", max_commit_width);
  o3queue = new O3Queue[o3queue_max_size];
  o3rob   = new O3ROB[o3rob_max_size];
  o3queue_head = 0;
  o3queue_size = 0;
  o3rob_head   = 0;
  o3rob_size   = 0;

  for (unsigned int i = 0; i < o3queue_max_size; i++)
  {
    o3queue[i].state = o3iqs_invalid;
  }

  for (unsigned int i = 0; i < o3rob_max_size; i++)
  {
    o3rob[i].state = o3irs_invalid;
  }

  if (o3rob_max_size <= 4)
  {
    cout << "as of now, it is assumed that o3rob_max_size >= 4" << endl;
    exit(1);
  }
}


O3Core::~O3Core()
{
  if (num_instrs > 0)
  {
    cout << "  -- OOO [" << setw(3) << num << "] : fetched " << setw(10) << num_instrs
         << " instrs, branch (miss, access)=( "
         << setw(8) << num_branch_miss << ", " << setw(10) << num_branch << ")= ";
    if (num_branch > 0)
    {
      cout << setw(6) << setiosflags(ios::fixed) << setprecision(2) << 100.00*num_branch_miss/num_branch << "%,";
    }
    else
    {
      cout << setw(6) << 0 << "%,";
    }
    cout << " nacks= " << num_nacks 
         << ", x87_ops= " << num_x87_ops
         << ", call_ops= " << num_call_ops
         << ", latest_ip= 0x" << hex << latest_ip << dec
         << ", tot_mem_wr_time= " << total_mem_wr_time
         << ", tot_mem_rd_time= " << total_mem_rd_time
         << ", tot_dep_dist= " << total_dependency_distance<< endl;
  }

  delete bp;
}


uint32_t O3Core::process_event(uint64_t curr_time)
{
  if (mcsim->skip_all_instrs == true)
  {
    o3queue_size = 0;
    resume_time  = curr_time;
  }

  if (o3queue_size <= (o3queue_max_size >> 1))
  {
    if (active == false)
    {
      //return num_hthreads;
    }
    else
    {
      geq->add_event(curr_time, this);
      return num;
    }
    //geq->add_event(curr_time, this);
    //return (active == false ? num_hthreads : num);
  }

  LocalQueueElement * lqe;

  // process o3queue

  //  check queue and send a request to iTLB -- TODO: can we send multiple requests to an iTLB simultaneously?
  uint64_t addr_to_read = 0;
  for (unsigned int i = 0; i < o3queue_size; i++)
  {
    unsigned int idx = (i + o3queue_head) % o3queue_max_size;
    if (o3queue[idx].state == o3iqs_not_in_queue)
    {
      if (addr_to_read == 0)
      {
        addr_to_read = (o3queue[idx].ip >> cachel1i->set_lsb) << cachel1i->set_lsb;
        o3queue[idx].state = o3iqs_being_loaded;
        lqe = new LocalQueueElement();
        lqe->th_id = num;
        lqe->from.push(this);
        lqe->address = addr_to_read;
        if (bypass_tlb == true)
        {
          lqe->type = et_read;
          cachel1i->add_req_event(curr_time + lsu_to_l1i_t, lqe);
        }
        else
        {
          lqe->type    = et_tlb_rd;
          tlbl1i->add_req_event(curr_time + lsu_to_l1i_t, lqe);
        }
      }
      else if (addr_to_read == (o3queue[idx].ip >> cachel1i->set_lsb) << cachel1i->set_lsb)
      {
        o3queue[idx].state = o3iqs_being_loaded;
      }
    }
  }

  //  check queue and send the instructions to the reorder buffer
  for (unsigned int i = 0; i < max_issue_width && o3queue_size > 0 && o3rob_size < o3rob_max_size - 3; i++)
  {
    uint64_t dependency_distance = o3rob_size;
    O3Queue & o3queue_entry      = o3queue[o3queue_head];

    if (o3queue_entry.state == o3iqs_ready && o3queue_entry.ready_time <= curr_time)
    {
      unsigned int rob_idx = (o3rob_head + o3rob_size) % o3rob_max_size;
      bool branch_miss     = false;

      if (o3queue_entry.type == ins_branch_taken || o3queue_entry.type == ins_branch_not_taken)
      {
        num_branch++;
        if (bp->miss(o3queue_entry.ip, o3queue_entry.type == ins_branch_taken))
        {
          num_branch_miss++;
          branch_miss = true;
        }
      }

      int32_t instr_dep  = -1;
      int32_t branch_dep = -1;
      for (unsigned int j = 0; j < o3rob_size; j++)
      {
        int rob_idx = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
        if (o3rob[rob_idx].state == o3irs_completed && o3rob[rob_idx].ready_time <= curr_time) continue;
        if (o3rob[rob_idx].branch_miss == true)
        {
          branch_dep = rob_idx;
          j          = o3rob_size;
          dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
          break;
        }
      }
      // register dependency resolution:
      // assume that false dependencies are resolved by register renaming
      int32_t rr0 = -1;
      int32_t rr1 = -1;
      int32_t rr2 = -1;
      int32_t rr3 = -1;
      for (unsigned int j = 0; j < o3rob_size; j++)
      {
        int rob_idx = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
        if (o3rob[rob_idx].state == o3irs_completed && o3rob[rob_idx].ready_time <= curr_time) continue;
        if (o3queue_entry.rr0 != 0 && rr0 == -1 &&
            (o3queue_entry.rr0 == o3rob[rob_idx].rw0 ||
             o3queue_entry.rr0 == o3rob[rob_idx].rw1 ||
             o3queue_entry.rr0 == o3rob[rob_idx].rw2 ||
             o3queue_entry.rr0 == o3rob[rob_idx].rw3))
        {
          rr0 = rob_idx;
          dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
        }
        if (o3queue_entry.rr1 != 0 && rr1 == -1 &&
            (o3queue_entry.rr1 == o3rob[rob_idx].rw0 ||
             o3queue_entry.rr1 == o3rob[rob_idx].rw1 ||
             o3queue_entry.rr1 == o3rob[rob_idx].rw2 ||
             o3queue_entry.rr1 == o3rob[rob_idx].rw3))
        {
          rr1 = rob_idx;
          dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
        }
        if (o3queue_entry.rr2 != 0 && rr2 == -1 &&
            (o3queue_entry.rr2 == o3rob[rob_idx].rw0 ||
             o3queue_entry.rr2 == o3rob[rob_idx].rw1 ||
             o3queue_entry.rr2 == o3rob[rob_idx].rw2 ||
             o3queue_entry.rr2 == o3rob[rob_idx].rw3))
        {
          rr2 = rob_idx;
          dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
        }
        if (o3queue_entry.rr3 != 0 && rr3 == -1 &&
            (o3queue_entry.rr3 == o3rob[rob_idx].rw0 ||
             o3queue_entry.rr3 == o3rob[rob_idx].rw1 ||
             o3queue_entry.rr3 == o3rob[rob_idx].rw2 ||
             o3queue_entry.rr3 == o3rob[rob_idx].rw3))
        {
          rr3 = rob_idx;
          dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
        }
      }
 
      // fill ROB
      geq->add_event(curr_time + process_interval, this);
      if (o3queue_entry.raddr != 0)
      {
        O3ROB & o3rob_entry    = o3rob[rob_idx];
        o3rob_entry.state      = o3irs_issued;
        o3rob_entry.ready_time = curr_time + process_interval;
        o3rob_entry.ip         = o3queue_entry.ip;
        int32_t mem_dep        = -1;
        for (unsigned int j = 0; j < o3rob_size; j++)
        {
          int rob_idx = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
          if ((o3rob[rob_idx].state != o3irs_completed ||
               o3rob[rob_idx].ready_time > curr_time) &&
              (o3rob[rob_idx].memaddr >> word_log) ==
              (o3queue_entry.raddr >> word_log))
          {
            mem_dep = rob_idx;
            dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
            break;
          }
        }
        o3rob_entry.memaddr    = o3queue_entry.raddr;
        o3rob_entry.branch_miss = branch_miss;
        o3rob_entry.isread     = true;
        o3rob_entry.mem_dep    = mem_dep;
        o3rob_entry.instr_dep  = instr_dep;
        o3rob_entry.branch_dep = branch_dep;
        o3rob_entry.type       = o3queue_entry.type;
        o3rob_entry.rr0        = (int32_t)rr0;
        o3rob_entry.rr1        = (int32_t)rr1;
        o3rob_entry.rr2        = (int32_t)rr2;
        o3rob_entry.rr3        = (int32_t)rr3;
        o3rob_entry.rw0        = o3queue_entry.rw0;
        o3rob_entry.rw1        = o3queue_entry.rw1;
        o3rob_entry.rw2        = o3queue_entry.rw2;
        o3rob_entry.rw3        = o3queue_entry.rw3;
        instr_dep              = rob_idx;
        rob_idx = (rob_idx + 1) % o3rob_max_size;
        o3rob_size++;
      }
      if (o3queue_entry.raddr2 != 0)
      {
        O3ROB & o3rob_entry    = o3rob[rob_idx];
        o3rob_entry.state      = o3irs_issued;
        o3rob_entry.ready_time = curr_time + process_interval;
        o3rob_entry.ip         = o3queue_entry.ip;
        int32_t mem_dep        = -1;
        for (unsigned int j = 0; j < o3rob_size; j++)
        {
          int rob_idx = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
          if ((o3rob[rob_idx].state != o3irs_completed ||
               o3rob[rob_idx].ready_time > curr_time) &&
              (o3rob[rob_idx].memaddr >> word_log) ==
              (o3queue_entry.raddr2 >> word_log))
          {
            mem_dep = rob_idx;
            dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
            break;
          }
        }
        o3rob_entry.memaddr    = o3queue_entry.raddr2;
        o3rob_entry.branch_miss = branch_miss;
        o3rob_entry.isread     = true;
        o3rob_entry.mem_dep    = mem_dep;
        o3rob_entry.instr_dep  = instr_dep;
        o3rob_entry.branch_dep = branch_dep;
        o3rob_entry.type       = o3queue_entry.type;
        o3rob_entry.rr0        = (int32_t)rr0;
        o3rob_entry.rr1        = (int32_t)rr1;
        o3rob_entry.rr2        = (int32_t)rr2;
        o3rob_entry.rr3        = (int32_t)rr3;
        o3rob_entry.rw0        = o3queue_entry.rw0;
        o3rob_entry.rw1        = o3queue_entry.rw1;
        o3rob_entry.rw2        = o3queue_entry.rw2;
        o3rob_entry.rw3        = o3queue_entry.rw3;
        instr_dep              = rob_idx;
        rob_idx = (rob_idx + 1) % o3rob_max_size;
        o3rob_size++;
      }
      if (o3queue_entry.waddr != 0)
      {
        O3ROB & o3rob_entry    = o3rob[rob_idx];
        o3rob_entry.state      = o3irs_issued;
        o3rob_entry.ready_time = curr_time + process_interval;
        o3rob_entry.ip         = o3queue_entry.ip;
        int32_t mem_dep        = -1;
        for (unsigned int j = 0; j < o3rob_size; j++)
        {
          int rob_idx = (o3rob_head + o3rob_size - 1 - j) % o3rob_max_size;
          if ((o3rob[rob_idx].state != o3irs_completed ||
               o3rob[rob_idx].ready_time > curr_time) &&
              (o3rob[rob_idx].memaddr >> word_log) ==
              (o3queue_entry.waddr >> word_log))
          {
            mem_dep = rob_idx;
            dependency_distance = (j+1 < dependency_distance) ? (j+1) : dependency_distance;
            break;
          }
        }
        o3rob_entry.memaddr    = o3queue_entry.waddr;
        o3rob_entry.branch_miss = branch_miss;
        o3rob_entry.isread     = false;
        o3rob_entry.mem_dep    = mem_dep;
        o3rob_entry.instr_dep  = instr_dep;
        o3rob_entry.branch_dep = branch_dep;
        o3rob_entry.type       = o3queue_entry.type;
        o3rob_entry.rr0        = (int32_t)rr0;
        o3rob_entry.rr1        = (int32_t)rr1;
        o3rob_entry.rr2        = (int32_t)rr2;
        o3rob_entry.rr3        = (int32_t)rr3;
        o3rob_entry.rw0        = o3queue_entry.rw0;
        o3rob_entry.rw1        = o3queue_entry.rw1;
        o3rob_entry.rw2        = o3queue_entry.rw2;
        o3rob_entry.rw3        = o3queue_entry.rw3;
        rob_idx = (rob_idx + 1) % o3rob_max_size;
        o3rob_size++;
      }
      if (o3queue_entry.raddr == 0 && o3queue_entry.raddr2 == 0 && o3queue_entry.waddr == 0)
      {
        O3ROB & o3rob_entry    = o3rob[rob_idx];
        o3rob_entry.state      = o3irs_issued;
        o3rob_entry.ready_time = curr_time + process_interval;
        o3rob_entry.ip         = o3queue_entry.ip;
        o3rob_entry.memaddr    = o3queue_entry.waddr;
        o3rob_entry.branch_miss = branch_miss;
        o3rob_entry.isread     = false;
        o3rob_entry.mem_dep    = -1;
        o3rob_entry.instr_dep  = instr_dep;
        o3rob_entry.branch_dep = branch_dep;
        o3rob_entry.type       = o3queue_entry.type;
        o3rob_entry.rr0        = (int32_t)rr0;
        o3rob_entry.rr1        = (int32_t)rr1;
        o3rob_entry.rr2        = (int32_t)rr2;
        o3rob_entry.rr3        = (int32_t)rr3;
        o3rob_entry.rw0        = o3queue_entry.rw0;
        o3rob_entry.rw1        = o3queue_entry.rw1;
        o3rob_entry.rw2        = o3queue_entry.rw2;
        o3rob_entry.rw3        = o3queue_entry.rw3;
        rob_idx = (rob_idx + 1) % o3rob_max_size;
        o3rob_size++;
      }
      o3queue_size--;
      o3queue_entry.state = o3iqs_invalid;
      o3queue_head = (o3queue_head + 1) % o3queue_max_size;
      total_dependency_distance += dependency_distance;
    }
  }

  // process o3rob
  
  // check if ready to execute
  int32_t num_alu  = 0;
  int32_t num_ldst = 0;
  int32_t num_ld   = 0;
  int32_t num_st   = 0;
  int32_t num_sse  = 0;
  for (unsigned int i = 0; i < o3rob_size; i++)
  {
    int rob_idx = (o3rob_head + i) % o3rob_max_size;
    O3ROB & o3rob_entry = o3rob[rob_idx];

    if (o3rob_entry.state == o3irs_issued && o3rob_entry.ready_time <= curr_time)
    {
      if (o3rob_entry.type == ins_notify)
      {
        if (i == 0)
        {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time + barrier_t;
          geq->add_event(o3rob_entry.ready_time, this);
          mcsim->is_migrate_ready[o3rob_entry.rw0] = true;
        }
        else
        {
          geq->add_event(curr_time + process_interval, this);
          i = o3rob_size;
        }
      }
      else if (o3rob_entry.type == ins_waitfor)
      {
        if (i == 0 && mcsim->is_migrate_ready[o3rob_entry.rw0] == true)
        {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time + process_interval;
          geq->add_event(o3rob_entry.ready_time, this);
          mcsim->is_migrate_ready[o3rob_entry.rw0] = false;
        }
        else
        {
          geq->add_event(curr_time + process_interval, this);
          i = o3rob_size;
        }
      }
      else if (o3rob_entry.mem_dep == -1 && o3rob_entry.instr_dep == -1 && o3rob_entry.branch_dep == -1 &&
               o3rob_entry.rr0 == -1     && o3rob_entry.rr1 == -1 &&
               o3rob_entry.rr2 == -1     && o3rob_entry.rr3 == -1)
      {
        if (o3rob_entry.memaddr == (uint64_t)0 && num_alu < max_alu && 
            (o3rob_entry.type != ins_x87 || num_sse < max_sse))
        {
          o3rob_entry.state = o3irs_completed;
          o3rob_entry.ready_time = curr_time + 
              ((o3rob_entry.type == ins_lock) ? lock_t :
               (o3rob_entry.type == ins_unlock) ? unlock_t :
               (o3rob_entry.type == ins_barrier) ? barrier_t : 
               (o3rob_entry.type == ins_x87) ? sse_t :
               (o3rob_entry.branch_miss == true) ? (branch_miss_penalty + process_interval) : process_interval);
          geq->add_event(o3rob_entry.ready_time, this);
          num_alu++;
          num_sse += ((o3rob_entry.type == ins_x87) ? 1 : 0);
        }
        else if (o3rob_entry.memaddr != (uint64_t)0 && num_ldst < max_ldst &&
                 ((o3rob_entry.isread == true && num_ld < max_ld) ||
                  (o3rob_entry.isread == false && num_st < max_st)))
        {
          o3rob_entry.state = o3irs_executing;
          lqe = new LocalQueueElement();
          lqe->th_id = num;
          lqe->from.push(this);
          lqe->address = o3rob_entry.memaddr;
          lqe->rob_entry = rob_idx;
          if (bypass_tlb == true)
          {
            lqe->type = (o3rob_entry.isread == true) ? et_read : et_write;
            cachel1d->add_req_event(curr_time + lsu_to_l1d_t, lqe); 
          }
          else
          {
            lqe->type = et_tlb_rd;
            tlbl1d->add_req_event(curr_time + lsu_to_l1d_t, lqe);
          }
          num_ldst++;
          if (o3rob_entry.isread == true)
          {
            num_ld++;
          }
          else
          {
            num_st++;
          }
        }
      }
      else
      {
        if (mimick_inorder == true)
        {
          geq->add_event(curr_time + process_interval, this);
          i = o3rob_size;
        }
      }
    }
    else
    {
      if (mimick_inorder == true)
      {
        geq->add_event(curr_time + process_interval, this);
        i = o3rob_size;
      }
    }
  }

  for (unsigned int i = 0; i < o3rob_size; i++)
  {
    int rob_idx = (o3rob_head + i) % o3rob_max_size;
    O3ROB & o3rob_entry = o3rob[rob_idx];
    bool already_add_event = false;

    if (o3rob_entry.state == o3irs_completed && o3rob_entry.ready_time == curr_time)
    {
      if (already_add_event == false)
      {
        already_add_event = true;
        geq->add_event(curr_time + process_interval, this);
      }
      for (unsigned int j = i+1; j < o3rob_size; j++)
      {
        int next_idx = (o3rob_head + j) % o3rob_max_size;
        O3ROB & o3rob_next = o3rob[next_idx];
        if (o3rob_next.state != o3irs_issued) continue;
        if (o3rob_next.mem_dep    == rob_idx) o3rob_next.mem_dep    = -1;
        if (o3rob_next.instr_dep  == rob_idx) o3rob_next.instr_dep  = -1;
        if (o3rob_next.branch_dep == rob_idx) o3rob_next.branch_dep = -1;
        if (o3rob_next.rr0        == rob_idx) o3rob_next.rr0        = -1;
        if (o3rob_next.rr1        == rob_idx) o3rob_next.rr1        = -1;
        if (o3rob_next.rr2        == rob_idx) o3rob_next.rr2        = -1;
        if (o3rob_next.rr3        == rob_idx) o3rob_next.rr3        = -1;
      }
    }
  }

  // check if ready to commit
  for (unsigned int i = 0; i < max_commit_width && o3rob_size > 0; i++)
  {
    O3ROB & o3rob_entry = o3rob[o3rob_head];

    if (o3rob_entry.state == o3irs_completed && o3rob_entry.ready_time <= curr_time)
    {
      o3rob_entry.state = o3irs_invalid;
      o3rob_size--;
      o3rob_head = (o3rob_head + 1) % o3rob_max_size;
    }
  }
  if (o3rob_size > 0 && o3rob[o3rob_head].state == o3irs_completed && o3rob[o3rob_head].ready_time <= curr_time)
  {
    geq->add_event(curr_time + process_interval, this);
  }

  /*if (o3rob_size != 0)
  {
    geq->add_event(curr_time + process_interval, this);
  }*/

  
  /*if (curr_time > 100000000 && curr_time < 100050000)
  {
    cout << "  * " << curr_time << endl;
    displayO3Queue();
    displayO3ROB();
  }*/
  return num_hthreads;
}


// a hack to differentiate I$ and D$ accesses -- NACK is supported
void O3Core::add_req_event(
    uint64_t event_time, 
    LocalQueueElement * local_event,
    Component * from)
{
  if (local_event->type == et_tlb_rd)
  {
    resume_time = event_time;
    
    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    latest_ip         = local_event->address;
    local_event->type = et_read;
    cachel1i->add_req_event(event_time + lsu_to_l1i_t, local_event); 
  }
  else if (local_event->type == et_nack)
  {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type    = et_read;
    cachel1i->add_req_event(event_time + lsu_to_l1i_t + spinning_slowdown*process_interval, local_event); 

    if (num_consecutive_nacks > consecutive_nack_threshold)
    {
      displayO3Queue();
      displayO3ROB();
      cout << " " << num << ", latest_ip = 0x" << hex << latest_ip << dec << endl;
      local_event->display();
      geq->display();  ASSERTX(0);
    }
  }
  else 
  {
    uint64_t aligned_event_time = event_time;
    if (aligned_event_time%process_interval != 0)
    {
      aligned_event_time += process_interval - aligned_event_time%process_interval;
    }
    geq->add_event(aligned_event_time, this);
    num_consecutive_nacks = 0;

    // move the state of O3Queue entries from being_loaded to ready
    for (unsigned int i = 0; i < o3queue_size; i++)
    {
      unsigned int idx = (i + o3queue_head) % o3queue_max_size;
      if (o3queue[idx].state == o3iqs_being_loaded &&
          local_event->address == (o3queue[idx].ip >> cachel1i->set_lsb) << cachel1i->set_lsb)
      {
        o3queue[idx].state = o3iqs_ready;
        o3queue[idx].ready_time = aligned_event_time;
      }
    }
    mcsim->update_os_page_req_dist(local_event->address);
    delete local_event;
  }
}


void O3Core::add_rep_event(
    uint64_t event_time, 
    LocalQueueElement * local_event,
    Component * from)
{
  if (local_event->type == et_tlb_rd)
  {
    resume_time = event_time;
    
    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type = (o3rob[local_event->rob_entry].isread == true) ? et_read : et_write;
    cachel1d->add_req_event(event_time + lsu_to_l1d_t, local_event); 
  }
  else if (local_event->type == et_nack)
  {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;

    while (local_event->from.empty() == false) local_event->from.pop();
    local_event->from.push(this);
    local_event->type = (o3rob[local_event->rob_entry].isread == true) ? et_read : et_write;
    cachel1d->add_req_event(event_time + lsu_to_l1d_t + spinning_slowdown*process_interval, local_event); 

    if (num_consecutive_nacks > consecutive_nack_threshold)
    {
      displayO3Queue();
      displayO3ROB();
      cout << " " << num << ", latest_ip = 0x" << hex << latest_ip << dec << endl;
      local_event->display();
      geq->display();  ASSERTX(0);
    }
  }
  else
  {
    uint64_t aligned_event_time = event_time;
    if (aligned_event_time%process_interval != 0)
    {
      aligned_event_time += process_interval - aligned_event_time%process_interval;
    }
    geq->add_event(aligned_event_time, this);
    num_consecutive_nacks = 0;

    // move the state of O3ROB entries from execute to complete
    assert(local_event->rob_entry >= 0 && local_event->rob_entry < (int32_t)o3rob_max_size);
    O3ROB & o3rob_entry    = o3rob[local_event->rob_entry];
    if (o3rob_entry.isread)
    {
      total_mem_rd_time += (aligned_event_time - o3rob_entry.ready_time);
    }
    else
    {
      total_mem_wr_time += (aligned_event_time - o3rob_entry.ready_time);
    }
    o3rob_entry.state      = o3irs_completed;
    o3rob_entry.ready_time = aligned_event_time + ((o3rob_entry.branch_miss == true) ? branch_miss_penalty : 0);
    geq->add_event(o3rob_entry.ready_time, this);
    /*cout << event_time << endl;
    displayO3Queue();
    displayO3ROB();*/
    mcsim->update_os_page_req_dist(local_event->address);
    delete local_event;
  }
}


bool O3Core::is_private(ADDRINT addr)
{
  // currently only memory accesses in a stack are treated as a private access
  if (addr < stack || addr >= stack + stacksize)
  {
    return false;
  }
  else
  {
    return true;
  }
}


void O3Core::displayO3Queue()
{
  cout << "  OOO[" << setw(3) << num << "]Q: head=" << o3queue_head << ", size=" << o3queue_size << endl;
  for (unsigned int i = 0; i < o3queue_max_size; i++)
  {
    O3Queue & o3queue_entry = o3queue[i];
    cout << "  - " << setw(3) << i << ": " << o3queue_entry.state << ", " 
         << dec << o3queue_entry.ready_time << ", "
         << hex << o3queue_entry.waddr << ", " << o3queue_entry.wlen << ", " << o3queue_entry.raddr << ", " << o3queue_entry.raddr2 << ", " << o3queue_entry.rlen << ", "
         << o3queue_entry.ip << ", " << o3queue_entry.type << ": " << dec
         << o3queue_entry.rr0 << ", " << o3queue_entry.rr1 << ", " << o3queue_entry.rr2 << ", " << o3queue_entry.rr3 << ", "
         << o3queue_entry.rw0 << ", " << o3queue_entry.rw1 << ", " << o3queue_entry.rw2 << ", " << o3queue_entry.rw3 << dec << endl;
  }
}


void O3Core::displayO3ROB()
{
  cout << "  OOO[" << setw(3) << num << "]R: head=" << o3rob_head << ", size=" << o3rob_size << endl;
  for (unsigned int i = 0; i < o3rob_max_size; i++)
  {
    O3ROB & o3rob_entry = o3rob[i];
    cout << "  - " << setw(3) << i << ": " << o3rob_entry.state << ", "
         << dec << o3rob_entry.ready_time << ", "
         << hex << o3rob_entry.ip << ", " << o3rob_entry.memaddr << ", " << o3rob_entry.isread << ", "
         << o3rob_entry.branch_miss << ": " << dec << o3rob_entry.mem_dep << ", "
         << o3rob_entry.instr_dep << ", " << o3rob_entry.branch_dep << ", " << o3rob_entry.type << ": "
         << o3rob_entry.rr0 << ", " << o3rob_entry.rr1 << ", " << o3rob_entry.rr2 << ", " << o3rob_entry.rr3 << ", "
         << o3rob_entry.rw0 << ", " << o3rob_entry.rw1 << ", " << o3rob_entry.rw2 << ", " << o3rob_entry.rw3 << dec << endl;
  }
}

