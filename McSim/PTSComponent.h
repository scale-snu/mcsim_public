// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef MCSIM_PTSCOMPONENT_H_
#define MCSIM_PTSCOMPONENT_H_

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
  UINT64     address;
  UINT32     th_id;
  bool       dummy;
  INT32      rob_entry;

  LocalQueueElement() : from(), th_id(0), dummy(false), rob_entry(-1) { }
  LocalQueueElement(Component * comp, event_type type_, UINT64 address_, UINT32 th_id_ = 0):
      from(), type(type_), address(address_),
      th_id(th_id_), dummy(false), rob_entry(-1) {
    from.push(comp);
  }

  void display();

  friend std::ostream& operator<<(std::ostream &out, LocalQueueElement &l);
};


class Component {  // meta-class
 public:
  Component(component_type type_, UINT32 num_, McSim * mcsim_);
  virtual ~Component();

  UINT32               process_interval;
  component_type       type;
  UINT32               num;
  McSim              * mcsim;
  GlobalEventQueue   * geq;  // global event queue

  virtual void add_req_event(UINT64, LocalQueueElement *, Component * from) { ASSERTX(0); }
  virtual void add_rep_event(UINT64, LocalQueueElement *, Component * from) { ASSERTX(0); }
  virtual void add_req_event(UINT64 a, LocalQueueElement * b) { add_req_event(a, b, NULL); }
  virtual void add_rep_event(UINT64 a, LocalQueueElement * b) { add_rep_event(a, b, NULL); }
  virtual UINT32 process_event(UINT64 curr_time) = 0;
  virtual void show_state(UINT64 address) { }
  virtual void display();
  virtual std::ostream & print(std::ostream & out) const;

  std::multimap<UINT64, LocalQueueElement *> req_event;
  std::multimap<UINT64, LocalQueueElement *> rep_event;
  std::queue<LocalQueueElement *> req_q;
  std::queue<LocalQueueElement *> rep_q;

 protected:
  const std::string prefix_str() const;
  UINT32 get_param_uint64(const std::string & param, UINT32 def = 0) const;
  UINT32 get_param_uint64(
    const std::string & param,
    const std::string & prefix,
    UINT32 def = 0) const;
  std::string get_param_str(const std::string & param) const;
  bool   get_param_bool(const std::string & param, bool def_value) const;
  UINT32 log2(UINT64 num);
  inline UINT64 ceil_by_y(UINT64 x, UINT64 y) { return ((x + y - 1) / y) * y; }

 public:
  friend std::ostream& operator<<(std::ostream &out, const Component &c) { return c.print(out); }
};


using event_queue_t = std::map<UINT64, std::set<Component *> >;

class GlobalEventQueue {
 public:
  // private:
  event_queue_t event_queue;
  UINT64 curr_time;
  McSim * mcsim;

 public:
  explicit GlobalEventQueue(McSim * mcsim_);
  ~GlobalEventQueue();
  void add_event(UINT64 event_time, Component *);
  UINT32 process_event();
  void display();

  UINT32 num_hthreads;
  UINT32 num_mcs;
  UINT32 interleave_base_bit;
  UINT32 interleave_xor_base_bit;
  UINT32 page_sz_base_bit;
  UINT32 which_mc(UINT64);  // which mc does an address belong to?

  friend std::ostream& operator<<(std::ostream &out, GlobalEventQueue &g);
};

}  // namespace PinPthread

#endif  // MCSIM_PTSCOMPONENT_H_
