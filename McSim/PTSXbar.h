// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTSXBAR_H_
#define PTSXBAR_H_

#include <list>
#include <map>
#include <queue>
#include <stack>
#include <utility>
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

