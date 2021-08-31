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

#ifndef PTSXBAR_H_
#define PTSXBAR_H_

#include <list>
#include <queue>
#include <stack>
#include <vector>

#include "McSim.h"

namespace PinPthread {

class NoC : public Component {
 public:
  using EventPair = std::pair<LocalQueueElement *, Component *>;
  enum noc_priority { noc_rep, noc_crq, noc_req };

  explicit NoC(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~NoC();

  std::vector<Directory *> directory;
  std::vector<CacheL2   *> cachel2;
  uint64_t num_req, num_rep, num_crq, num_flits;
  uint64_t num_data_transfers;

  virtual void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL) = 0;
  virtual void add_crq_event(uint64_t, LocalQueueElement *, Component * from = NULL) = 0;  // coherence request 
  virtual void add_crq_event(uint64_t, LocalQueueElement *, uint32_t num_flits, Component * from = NULL) = 0;
  virtual void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL) = 0;
  virtual void add_rep_event(uint64_t, LocalQueueElement *, uint32_t num_flits, Component * from = NULL) = 0;
  virtual uint32_t process_event(uint64_t curr_time) = 0;
};


class Crossbar : public NoC {
 public:
  explicit Crossbar(component_type type_, uint32_t num_, McSim * mcsim_, uint32_t num_ports_);
  ~Crossbar();

  // not sure if req_queue and rep_queue are enough to avoid deadlock (due to
  // circular dependency with finite buffer size) or more queues are necessary.
  std::multimap<uint64_t, EventPair> crq_events;  // <event, from>
  std::multimap<uint64_t, EventPair> req_events;
  std::multimap<uint64_t, EventPair> rep_events;
  std::vector< std::multimap<noc_priority, EventPair> > queues;  // <event, to>
  std::vector<bool> already_sent;

  const uint32_t xbar_to_dir_t;
  const uint32_t xbar_to_l2_t;
  const uint32_t num_ports;

  // token
  bool     clockwise;
  uint32_t top_priority;

  void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  void add_crq_event(uint64_t, LocalQueueElement *, Component * from = NULL);  // coherence request 
  void add_crq_event(uint64_t, LocalQueueElement *, uint32_t num_flits, Component * from = NULL);
  void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  void add_rep_event(uint64_t, LocalQueueElement *, uint32_t num_flits, Component * from = NULL);
  uint32_t process_event(uint64_t curr_time);
};

}  // namespace PinPthread

#endif  // PTSXBAR_H_

