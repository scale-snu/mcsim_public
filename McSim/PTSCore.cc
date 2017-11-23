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
#include "PTSCore.h"
#include "PTSTLB.h"
#include <iomanip>

using namespace PinPthread;

extern ostream & operator << (ostream & output, ins_type it);

Core::Core(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  num_hthreads(get_param_uint64("num_hthreads", "pts.", max_hthreads)),
  hthreads(),
  is_active(),
  lastly_served_thread_num(0),
  last_time_no_mem_served(0),
  last_time_mem_served(0),
  num_bubbled_slots(0)
{
  process_interval = get_param_uint64("process_interval", "pts.lsu.", 10);
}


Core::~Core()
{
  if (num_bubbled_slots > 0)
  {
    cout << "  -- CORE[" << num << "] : num_bubbled_slots = " << num_bubbled_slots << endl;
  }
}


uint32_t Core::process_event(uint64_t curr_time)
{
  bool action = false;

  for (uint32_t i = 0; i < hthreads.size() && action == false; i++)
  {
    uint32_t idx = (i + lastly_served_thread_num + 1) % hthreads.size();
    Hthread * hthread = hthreads[idx];

    if (is_active[idx] == false || 
        (hthread->resume_time > curr_time && 
         (hthread->latest_bmp_time <= curr_time || curr_time % process_interval != 0)))
    {
      continue;
    }

    if (mcsim->skip_all_instrs == true)
    {
      while (hthread->mem_acc.empty() == false)
      {
        hthread->mem_acc.pop();
      }
      hthread->resume_time = curr_time;
    }

    if (curr_time % process_interval == 0)
    {
      if (hthread->latest_bmp_time > curr_time)
      {
        action = true;
        lastly_served_thread_num = hthread->num;
        geq->add_event(curr_time + process_interval, this);
        num_bubbled_slots++;
        break;
      }
      
      if (hthread->mem_acc.empty() == true)
      {
        is_active[idx] = false;
        if (hthread->active == false)
        {
          continue;
        }
        else
        {
          return hthread->num;
        }
      }

      switch (hthread->mem_acc.front().first)
      {
        case mem_rd:
        case mem_wr:
          if (last_time_mem_served >= curr_time && curr_time > 0)
          {
            continue;
          }
          action = true;
          lastly_served_thread_num = hthread->num;
          last_time_mem_served = curr_time;
          hthread->process_event(curr_time);
          break;
        default:
          if (last_time_no_mem_served >= curr_time && curr_time > 0)
          {
            continue;
          }
          action = true;
          lastly_served_thread_num = hthread->num;
          last_time_no_mem_served = curr_time;
          hthread->process_event(curr_time);
          break;
      }
    }
    else
    {
      action = true;
      if (hthread->mem_acc.empty() == true ||
          (hthread->mem_acc.front().first != mem_rd &&
           hthread->mem_acc.front().first != mem_wr &&
           (hthread->tlb_rd == false || hthread->bypass_tlb == true)))
      {
        geq->add_event(curr_time + (process_interval - curr_time%process_interval), this);
      }
      else
      {
        hthread->process_event(curr_time);
      }
    }
  }

  return num_hthreads;
}



Hthread::Hthread(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  active(false), spinning(NULL), stack(0), stacksize(0),
  resume_time(0),
  num_branch(0), num_branch_miss(0), num_nacks(0),
  num_consecutive_nacks(0), 
  num_x87_ops(0), num_call_ops(0), total_mem_wr_time(0), total_mem_rd_time(0),
  lsu_to_l1i_t(get_param_uint64("to_l1i_t", 10)),
  lsu_to_l1d_t(get_param_uint64("to_l1d_t", 10)),
  lsu_to_l1i_t_for_x87_op(get_param_uint64("to_l1i_t_for_x87_op", lsu_to_l1i_t)),
  branch_miss_penalty(get_param_uint64("branch_miss_penalty", 100)),
  spinning_slowdown(get_param_uint64("spinning_slowdown", 10)),
  lock_t(get_param_uint64("lock_t", 100)),
  unlock_t(get_param_uint64("unlock_t", 100)),
  barrier_t(get_param_uint64("barrier_t", 100)),
  consecutive_nack_threshold(get_param_uint64("consecutive_nack_threshold", 1000)),
  was_nack(false),
  latest_ip(0), latest_bmp_time(0)
{
  process_interval = get_param_uint64("process_interval", 100);

  num_hthreads = get_param_uint64("num_hthreads", "pts.", max_hthreads);
  bp = new BranchPredictor(
      get_param_uint64("num_bp_entries", 256),
      get_param_uint64("gp_size", 0));
  ASSERTX(mem_acc.empty());

  bypass_tlb = get_param_str("bypass_tlb") == "true" ? true : false;
  display_barrier = get_param_str("display_barrier") == "true" ? true : false;
}



Hthread::~Hthread()
{
  if (num_branch > 0 || latest_ip != 0)
  {
    cout << "  -- HTH [" << num << "] : branch (miss, access) = ("
      << num_branch_miss << ", " << num_branch << ") = "
      << ((num_branch == 0) ? 0 : 100.00*num_branch_miss/num_branch) << "%, "
      << " #_nacks = " << num_nacks 
      << ", #_x87_ops = " << num_x87_ops
      << ", #_call_ops = " << num_call_ops
      << ", latest_ip = 0x" << hex << latest_ip << dec
      << ", tot_mem_wr_time = " << total_mem_wr_time
      << ", tot_mem_rd_time = " << total_mem_rd_time << endl;
  }

  delete bp;
}



uint32_t Hthread::process_event(uint64_t curr_time)
{
  if (bypass_tlb == true)
  {
    tlb_rd   = true;
    mem_time = curr_time;
  }
  LocalQueueElement * lqe;
  uint32_t to_l1i_t = lsu_to_l1i_t;

  ins_type it   = mem_acc.front().first;
  uint64_t addr = mem_acc.front().second;

  switch (it)
  {
    case ins_lock:
    case ins_unlock:
      mem_acc.pop();
      resume_time = curr_time + (it == ins_lock ? lock_t : unlock_t);
      geq->add_event(resume_time, core);
      return num_hthreads;

    case ins_barrier:
      mem_acc.pop();
      resume_time = curr_time + barrier_t;
      geq->add_event(resume_time, core);

      if (display_barrier == true)
      {
        cout << "  -- [" << setw(12) << curr_time << "] :";
        cout << " thread " << num << " reached a barrier : prev ip = 0x";
        cout << hex << latest_ip << dec << endl;
      }
      return num_hthreads;

    case ins_branch_taken:
    case ins_branch_not_taken:
      num_branch++;
      if (bp->miss(addr, it == ins_branch_taken))
      {
        num_branch_miss++;
        geq->add_event(curr_time + process_interval, core);
        curr_time += branch_miss_penalty;
        latest_bmp_time = curr_time;
      }
    case ins_x87:
      if ((bypass_tlb == true || tlb_rd == false) && it == ins_x87)
      {
        num_x87_ops++;
      }
      to_l1i_t = lsu_to_l1i_t_for_x87_op;
    case no_mem:
      if ((latest_ip >> cachel1i->set_lsb) == (addr >> cachel1i->set_lsb))
      {
        // instruction locates at an instruction buffer
        if (curr_time%process_interval != 0)
        {
          curr_time += process_interval - curr_time%process_interval;
        }
        if (addr != 0)
        {
          latest_ip = addr;
        }
        mem_acc.pop();
        resume_time = curr_time + to_l1i_t;
        geq->add_event(resume_time, core);
        return num_hthreads;
      }
      lqe = new LocalQueueElement();
      lqe->from.push(this);
      lqe->address = addr;
      if (tlb_rd == false)
      {
        if (curr_time%process_interval != 0)
        {
          curr_time += process_interval - curr_time%process_interval;
        }
        lqe->type = et_tlb_rd;
        tlbl1i->add_req_event(curr_time + to_l1i_t, lqe);
        resume_time = (uint64_t)-1;
      }
      else
      {
        if (lqe->address != 0)
        {
          latest_ip = lqe->address;
        }
        lqe->type = et_read;
        cachel1i->add_req_event(curr_time + to_l1i_t, lqe); 
        mcsim->update_os_page_req_dist(addr);
        resume_time = (uint64_t)-1;
      }
      return num_hthreads;
      break;

    default:
      lqe = new LocalQueueElement();
      lqe->th_id = num;
      lqe->from.push(this);
      lqe->address = addr;
      if (tlb_rd == false)
      {
        lqe->type = et_tlb_rd;
        tlbl1d->add_req_event(curr_time + lsu_to_l1d_t, lqe);
        resume_time = (uint64_t)-1;
        mem_time    = curr_time;
      }
      else
      {
        lqe->type = (mem_acc.front().first == mem_rd) ? et_read : et_write;
        if ((*spinning) > 0 || was_nack == true)
        {
          cachel1d->add_req_event(curr_time + lsu_to_l1d_t + process_interval*spinning_slowdown, lqe);
          was_nack = false;
        }
        else
        {
          cachel1d->add_req_event(curr_time + lsu_to_l1d_t, lqe);
        }
        mcsim->update_os_page_req_dist(addr);
        resume_time = (uint64_t)-1;
      }
      return num_hthreads;
      break;
  }  
}



// a hack to differentiate I$ and D$ accesses -- NACK is supported
void Hthread::add_req_event(
    uint64_t event_time, 
    LocalQueueElement * local_event,
    Component * from)
{
  geq->add_event(event_time, core);
  if (local_event->type == et_tlb_rd)
  {
    tlb_rd = true;
    resume_time = event_time;
  }
  else if (local_event->type == et_nack)
  {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;
    was_nack = true;

    if (num_consecutive_nacks > consecutive_nack_threshold)
    {
      display();
      cout << " " << num << ", latest_ip = 0x" << hex << latest_ip << dec << endl;
      local_event->display();
      geq->display();  ASSERTX(0);
    }
  }
  else 
  {
    num_consecutive_nacks = 0;
    if (mem_acc.empty() == true)
    {
      display();  local_event->display();  geq->display();  ASSERTX(0);
    }
    resume_time = event_time;
    mem_acc.pop();
    tlb_rd = false;
  }
  delete local_event;
}



void Hthread::add_rep_event(
    uint64_t event_time, 
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time%process_interval != 0)
  {
    event_time += process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, core);
  if (local_event->type == et_nack)
  {
    num_nacks++;
    num_consecutive_nacks++;
    resume_time = event_time;
    was_nack = true;

    if (num_consecutive_nacks > consecutive_nack_threshold)
    {
      display();
      cout << " # of nacks = " << num_consecutive_nacks << ", latest_ip = 0x" << hex << latest_ip << dec << endl;
      //mcsim->show_state(local_event->address);
      local_event->display();  geq->display();  ASSERTX(0);
    }
  }
  else
  {
    num_consecutive_nacks = 0;
    if (mem_acc.empty() == false &&
        (mem_acc.front().first == mem_rd || mem_acc.front().first == mem_wr))
    {
      if (mem_acc.front().first == mem_rd)
      {
        total_mem_rd_time += (event_time - mem_time) + process_interval - (event_time - mem_time)%process_interval;
      }
      else
      {
        total_mem_wr_time += (event_time - mem_time) + process_interval - (event_time - mem_time)%process_interval;
      }
      mem_acc.pop();
      resume_time = event_time;
      tlb_rd = false;
    }
    else
    {
      display();  local_event->display();

      while (mem_acc.empty() == false)
      {
        cout << "  -- (" << mem_acc.front().first << ", " << mem_acc.front().second << ")" << endl;
        mem_acc.pop();
      }
      geq->display();  ASSERTX(0);
    }
  }
  delete local_event;
}


bool Hthread::is_private(ADDRINT addr)
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


BranchPredictor::BranchPredictor(uint32_t num_entries_, uint32_t gp_size_)
 :num_entries(num_entries_), gp_size(gp_size_), bimodal_entry(num_entries, 1),
  global_history(0)
{
}


bool BranchPredictor::miss(uint64_t addr, bool taken)
{
  bool miss;
  global_history      = (gp_size == 0) ? 0 : ((global_history << 1) + (taken == true ? 1 : 0));
  addr                = addr ^ (global_history << (64 - gp_size));
  uint32_t curr_entry = bimodal_entry[addr%num_entries];

  miss = (curr_entry > 1 && taken == false) || (curr_entry < 2 && taken == true);
  bimodal_entry[addr%num_entries] = (curr_entry == 0 && taken == false) ? 0 :
    (curr_entry == 3 && taken == true)  ? 3 :
    (taken == true) ? (curr_entry + 1) : (curr_entry - 1);
  return miss;
}

