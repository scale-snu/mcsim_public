// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef MCSIM_PTSTLB_H_
#define MCSIM_PTSTLB_H_

#include "McSim.h"

#include <map>
#include <queue>
#include <stack>
#include <vector>

namespace PinPthread {

class TLBL1 : public Component {
 public:
  explicit TLBL1(component_type type_, uint32_t num_, McSim * mcsim_);
  virtual ~TLBL1();
  void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);
  uint32_t process_event(uint64_t curr_time);

  const uint32_t num_entries;
  const uint32_t l1_to_lsu_t;
  const uint32_t page_sz_log2;
  const uint32_t miss_penalty;
  const uint32_t speedup;

 protected:
  uint64_t num_access;
  uint64_t num_miss;

  // currently it is assumed that L1 TLBs are fully-associative
  std::map<uint64_t, uint64_t> entries;  // <page_num, time>
  std::map<uint64_t, std::map<uint64_t, uint64_t>::iterator> LRU;
};

}  // namespace PinPthread

#endif  // MCSIM_PTSTLB_H_
