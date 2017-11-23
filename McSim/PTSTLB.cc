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

#include "PTSTLB.h"
#include "PTSCache.h"
#include <iomanip>

using namespace PinPthread;

extern ostream& operator<<(ostream & output, component_type ct);


TLBL1::TLBL1(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_),
  num_entries  (get_param_uint64("num_entries", 64)),
  l1_to_lsu_t  (get_param_uint64("to_lsu_t", 0)),
  page_sz_log2 (get_param_uint64("page_sz_log2", 13)),  // 8192 bytes
  miss_penalty (get_param_uint64("miss_penalty", 100)),
  num_access(0), num_miss(0)
{
  process_interval = get_param_uint64("process_interval", 10);
  speedup          = get_param_uint64("speedup", 1);
}



TLBL1::~TLBL1()
{
  if (num_access > 0)
  {
    if (type == ct_tlbl1d)
    {
      cout << "  -- TLBD[";
    }
    else
    {
      cout << "  -- TLBI[";
    }
    cout << num << "] : (miss, access) = (" << num_miss << ", "
         << num_access << ") = " << 100.00*num_miss/num_access << "%" << endl;
  }
}



void TLBL1::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  geq->add_event(event_time, this);
  req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}



uint32_t TLBL1::process_event(uint64_t curr_time)
{
  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();

  LocalQueueElement * req_lqe = NULL;
  // event -> queue
  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    req_q.push(req_event_iter->second);
    ++req_event_iter;
  }
  req_event.erase(curr_time);


  for (uint32_t i = 0; i < speedup; i++)
  {
    if (req_q.empty() == true) break;

    req_lqe = req_q.front();
    req_q.pop();

    // process the first request event
    uint64_t address  = req_lqe->address;
    uint64_t page_num = (address >> page_sz_log2);

    num_access++;
    if (entries.find(page_num) == entries.end())
    {
      num_miss++;
      if (type == ct_tlbl1i)
      {
        (req_lqe->from.top())->add_req_event(curr_time + l1_to_lsu_t + miss_penalty, req_lqe);
      }
      else
      {
        (req_lqe->from.top())->add_rep_event(curr_time + l1_to_lsu_t + miss_penalty, req_lqe);
      }
      if (LRU.size() >= num_entries)
      {
        entries.erase(LRU.begin()->second);
        LRU.erase(LRU.begin());
      }
      LRU.insert(pair<uint64_t, map<uint64_t, uint64_t>::iterator>(curr_time, entries.insert(pair<uint64_t, uint64_t>(page_num, curr_time)).first));
    }
    else
    {
      if (type == ct_tlbl1i)
      {
        (req_lqe->from.top())->add_req_event(curr_time + l1_to_lsu_t, req_lqe);
      }
      else
      {
        (req_lqe->from.top())->add_rep_event(curr_time + l1_to_lsu_t, req_lqe);
      }
      LRU.erase(entries[page_num]);
      entries[page_num] = curr_time;
      LRU.insert(pair<uint64_t, map<uint64_t, uint64_t>::iterator>(curr_time, entries.find(page_num)));
    }
  }

  if (req_q.empty() == false)
  {
    uint64_t event_time = curr_time + process_interval;
    geq->add_event(event_time, this);
  }

  return 0;
}


