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
#include <iomanip>
#include <sstream>

#include "PTSXbar.h"
#include "PTSCache.h"
#include "PTSDirectory.h"


namespace PinPthread {

NoC::NoC(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Component(type_, num_, mcsim_), directory(), cachel2(),
  num_req(0), num_rep(0), num_crq(0), num_flits(0), num_data_transfers(0) {
}


NoC::~NoC() {
  if (num_req > 0) {
    std::cout << "  -- NoC [" << std::setw(3) << num << "] : (req, crq, rep) = ("
              << num_req << ", " << num_crq << ", " << num_rep
              << "), num_data_transfers = " << num_data_transfers << std::endl;
  }
}


Crossbar::Crossbar(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_,
    uint32_t num_ports_):
  NoC(type_, num_, mcsim_),
  queues(num_ports_), already_sent(num_ports_),
  xbar_to_dir_t(get_param_uint64("to_dir_t", 90)),
  xbar_to_l2_t(get_param_uint64("to_l2_t", 90)),
  num_ports(num_ports_),
  clockwise(true), top_priority(0) {
  process_interval = get_param_uint64("process_interval", 10);
}


Crossbar::~Crossbar() {
}


void Crossbar::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  ASSERTX(from);
  if (event_time % process_interval != 0) {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_req++;
  num_flits++;
  geq->add_event(event_time, this);
  req_events.insert(std::pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}


void Crossbar::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  ASSERTX(from);
  if (event_time % process_interval != 0) {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_crq++;
  num_flits++;
  geq->add_event(event_time, this);
  crq_events.insert(std::pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}


void Crossbar::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from) {
  for (uint32_t i = 0; i < num_flits - 1; i++) {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_crq_event(event_time, dummy_lqe, from);
    ++num_crq;
  }

  add_crq_event(event_time, local_event, from);
  num_data_transfers++;
}


void Crossbar::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from) {
  ASSERTX(from);
  if (event_time % process_interval != 0) {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_rep++;
  num_flits++;
  geq->add_event(event_time, this);
  rep_events.insert(std::pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}


void Crossbar::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from) {
  for (uint32_t i = 0; i < num_flits - 1; i++) {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_rep_event(event_time, dummy_lqe, from);
    ++num_rep;
  }

  add_rep_event(event_time, local_event, from);
  num_data_transfers++;
}


uint32_t Crossbar::process_event(uint64_t curr_time) {
  auto req_event_iter = req_events.begin();
  auto rep_event_iter = rep_events.begin();
  auto crq_event_iter = crq_events.begin();

  while (rep_event_iter != rep_events.end() && rep_event_iter->first == curr_time) {
    if (rep_event_iter->second.first->type == et_evict ||
        rep_event_iter->second.first->type == et_invalidate ||
        rep_event_iter->second.first->type == et_invalidate_nd ||
        rep_event_iter->second.first->type == et_nop ||
        rep_event_iter->second.first->type == et_e_to_i ||
        rep_event_iter->second.first->type == et_e_to_m) {
      uint32_t which_mc = geq->which_mc(rep_event_iter->second.first->address);
      queues[rep_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_rep, EventPair(rep_event_iter->second.first, directory[which_mc])));
    } else {
      queues[rep_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_rep, EventPair(rep_event_iter->second.first, rep_event_iter->second.first->from.top())));
    }
    ++rep_event_iter;
  }

  while (crq_event_iter != crq_events.end() && crq_event_iter->first == curr_time) {
    // special case --  from.top() is the target L2
    queues[crq_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_crq, EventPair(crq_event_iter->second.first, crq_event_iter->second.first->from.top())));
    crq_event_iter->second.first->from.pop();
    ++crq_event_iter;
  }

  while (req_event_iter != req_events.end() && req_event_iter->first == curr_time) {
    // process the first request event
    uint32_t which_mc  = geq->which_mc(req_event_iter->second.first->address);  // TODO(gajh) : it is assumed that (directory[i]->num == i)
    queues[req_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_req, EventPair(req_event_iter->second.first, directory[which_mc])));
    ++req_event_iter;
  }

  req_events.erase(curr_time);  // add_req_event
  crq_events.erase(curr_time);  // add_rep_event
  rep_events.erase(curr_time);  // add_rep_event


  // send events to destinations
  for (uint32_t i = 0; i < num_ports; i++) {
    already_sent[i] = false;
  }

  for (uint32_t i = 0; i < num_ports; i++) {
    uint32_t idx = top_priority + num_ports;
    idx += ((clockwise == true) ? i : (0-i));
    idx %= num_ports;

    auto iter = queues[idx].begin();
    if (iter != queues[idx].end()) {
      if (already_sent[iter->second.second->num] == false) {
        already_sent[iter->second.second->num] = true;
        if (iter->second.first->dummy == true) {
          delete iter->second.first;
        } else if (iter->first == noc_req) {
          iter->second.second->add_req_event(curr_time + xbar_to_dir_t, iter->second.first);
        } else {
          iter->second.second->add_rep_event(curr_time + xbar_to_dir_t, iter->second.first);
        }
        queues[idx].erase(iter);
        iter = queues[idx].begin();
      } else {
        ++iter;
      }

      if (iter != queues[idx].end()) {
        if (already_sent[iter->second.second->num] == false) {
          already_sent[iter->second.second->num] = true;
          if (iter->second.first->dummy == true) {
            delete iter->second.first;
          } else if (iter->first == noc_req) {
            iter->second.second->add_req_event(curr_time + xbar_to_dir_t, iter->second.first);
          } else {
            iter->second.second->add_rep_event(curr_time + xbar_to_dir_t, iter->second.first);
          }
          queues[idx].erase(iter);
        }
      }
    }
  }
  if (clockwise == true) {
    clockwise = false;
  } else {
    clockwise = true;
    top_priority = (top_priority + 1)%num_ports;
  }

  for (uint32_t i = 0; i < num_ports; i++) {
    if (queues[i].empty() == false) {
      geq->add_event(curr_time + process_interval, this);
      break;
    }
  }

  return 0;
}

}  // namespace PinPthread
