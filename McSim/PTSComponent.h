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

#ifndef PTSCOMPONENT_H_
#define PTSCOMPONENT_H_

#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>

#include "PTS.h"

namespace PinPthread {

class Component;
class GlobalEventQueue;

enum component_type {
  ct_core,
  ct_o3core,
  ct_cachel1d,
  ct_cachel1i,
  ct_cachel2,
  ct_directory,
  ct_crossbar,
  ct_memory_controller,
  ct_tlbl1d,
  ct_tlbl1i,
  ct_tlbl2,
};

enum event_type {
  et_read,
  et_write,
  et_write_nd,
  et_evict,
  et_evict_nd,
  et_dir_evict,
  et_e_to_m,
  et_e_to_i,
  et_e_to_s,
  et_e_to_s_nd,
  et_m_to_s,
  et_m_to_m,
  et_m_to_i,
  et_s_to_s,
  et_s_to_s_nd,
  et_m_to_e,
  et_dir_rd,      // read request originated from a directory
  et_dir_rd_nd,
  et_e_rd,
  et_s_rd,
  et_nack,
  et_invalidate,  // originated from a directory
  et_invalidate_nd,
  et_s_rd_wr,     // when a directory state changes from modified to shared, data must be written
  et_rd_bypass,   // this read is older than what is in the cache
  et_tlb_rd,
  et_i_to_e,
  et_rd_dir_info_req,
  et_rd_dir_info_rep,
  et_nop,
};

struct LocalQueueElement {
  std::stack<Component *> from;  // where it is from
  event_type type;
  uint64_t   address;
  uint32_t   th_id;
  bool       dummy;
  int32_t    rob_entry;

  LocalQueueElement() : from(), th_id(0), dummy(false), rob_entry(-1) { }
  LocalQueueElement(Component * comp, event_type type_, uint64_t address_, uint32_t th_id_ = 0):
    from(), type(type_), address(address_), th_id(th_id_), dummy(false), rob_entry(-1) { from.push(comp); }

  void display();

  friend std::ostream& operator<<(std::ostream &out, LocalQueueElement &l);
};


class Component {  // meta-class
 public:
  Component(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~Component();

  uint32_t                 process_interval;
  component_type           type;
  uint32_t                 num;
  McSim                  * mcsim;
  GlobalEventQueue       * geq;  // global event queue

  virtual void add_req_event(uint64_t, LocalQueueElement *, Component * from) { ASSERTX(0); }
  virtual void add_rep_event(uint64_t, LocalQueueElement *, Component * from) { ASSERTX(0); }
  virtual void add_req_event(uint64_t a, LocalQueueElement * b) { add_req_event(a, b, NULL); }
  virtual void add_rep_event(uint64_t a, LocalQueueElement * b) { add_rep_event(a, b, NULL); }
  virtual uint32_t process_event(uint64_t curr_time) = 0;
  virtual void show_state(uint64_t address) { }
  virtual void display();
  virtual std::ostream & print(std::ostream & out) const;

  std::multimap<uint64_t, LocalQueueElement *> req_event;
  std::multimap<uint64_t, LocalQueueElement *> rep_event;
  std::queue<LocalQueueElement *> req_q;
  std::queue<LocalQueueElement *> rep_q;

 protected:
  const char * prefix_str() const;
  uint32_t get_param_uint64(const std::string & param, uint32_t def = 0) const;
  uint32_t get_param_uint64(const std::string & param, const std::string & prefix, uint32_t def = 0) const;
  std::string   get_param_str(const std::string & param) const;
  bool     get_param_bool(const std::string & param, bool def_value) const;
  uint32_t log2(uint64_t num);
  inline uint64_t ceil_by_y(uint64_t x, uint64_t y) { return ((x + y - 1) / y) * y; }

 public:
  friend std::ostream& operator<<(std::ostream &out, const Component &c) { return c.print(out); }
};


typedef std::map<uint64_t, std::set<Component *> > event_queue_t;

class GlobalEventQueue {
 public:
  // private:
  event_queue_t event_queue;
  uint64_t curr_time;
  McSim * mcsim;

 public:
  explicit GlobalEventQueue(McSim * mcsim_);
  ~GlobalEventQueue();
  void add_event(uint64_t event_time, Component *);
  uint32_t process_event();
  uint32_t process_event_isolateTEST(component_type target);
  void display();

  uint32_t num_hthreads;
  uint32_t num_mcs;
  uint32_t interleave_base_bit;
  uint32_t interleave_xor_base_bit;
  uint32_t page_sz_base_bit;
  uint32_t which_mc(uint64_t);  // which mc does an address belong to?

  friend std::ostream& operator<<(std::ostream &out, GlobalEventQueue &g);
};

}  // namespace PinPthread

#endif  // PTSCOMPONENT_H_
