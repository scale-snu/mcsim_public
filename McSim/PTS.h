// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTS_H_
#define PTS_H_


#include <stdint.h>
#include <stdlib.h>
#include <list>
#include <map>
#include <queue>
#include <stack>
#include <utility>
#include <vector>
#include <string>

#include "toml.hpp"

using UINT8  = uint8_t;   // LINUX HOSTS
using UINT16 = uint16_t;
using UINT32 = uint32_t;
using UINT64 = uint64_t;

using INT8   = int8_t;
using INT16  = int16_t;
using INT32  = int32_t;
using INT64  = int64_t;

#if defined(TARGET_IA32)  // Intel(R) 32 architectures
  using ADDRINT = UINT32;
  using ADDRDELTA = INT32;
#elif defined(TARGET_IA32E)  // Intel(R) 64 architectures
  using ADDRINT = UINT64;
  using ADDRDELTA = INT64;
#elif defined(TARGET_IPF)
  using ADDRINT = UINT64;
  using ADDRDELTA = INT64;
#else
  #error "xxx"
#endif

#include <assert.h>
#if defined(NDEBUG)
#define ASSERTX(a)
#else
#define ASSERTX(a) if (!a) { std::cout << __FILE__ << ", line: " << __LINE__ << std::endl; assert(a); }
#endif

// please refer to 'xed-category-enu.h'.
// const uint32_t XED_CATEGORY_X87_ALU = 36;
// const uint32_t XED_CATEGORY_CALL    = 5;

#include "PTSInterface.h"

namespace PinPthread  {
class McSim;

class PthreadTimingSimulator {
 public:
  explicit PthreadTimingSimulator(const std::string & mdfile);
  explicit PthreadTimingSimulator(int port_num) { }
  ~PthreadTimingSimulator();

  std::vector<std::string> trace_files;
  McSim * mcsim;

  std::pair<UINT32, UINT64> resume_simulation(bool must_switch);
  // return value -- whether we have to resume simulation
  bool add_instruction(
      UINT32 hthreadid_,
      UINT64 curr_time_,
      UINT64 waddr,
      UINT32 wlen,
      UINT64 raddr,
      UINT64 raddr2,
      UINT32 rlen,
      UINT64 ip,
      UINT32 category,
      bool   isbranch,
      bool   isbranchtaken,
      bool   islock,
      bool   isunlock,
      bool   isbarrier,
      UINT32 rr0, UINT32 rr1, UINT32 rr2, UINT32 rr3,
      UINT32 rw0, UINT32 rw1, UINT32 rw2, UINT32 rw3);

  UINT64 get_param_uint64(const std::string & idx_, UINT64 def_value) const;
  bool   get_param_bool(const std::string & idx_, bool def_value) const;
  std::string get_param_str(const std::string & idx_) const;

  void set_stack_n_size(INT32 pth_id, ADDRINT stack, ADDRINT stacksize);
  void set_active(INT32 pth_id, bool is_active);

  UINT32 get_num_hthreads() const;
  UINT64 get_curr_time() const;

 private:
  std::map<std::string, bool> params_bool;
  std::map<std::string, UINT64> params_uint64_t;
  std::map<std::string, std::string> params_string;

  void md_table_decoding(const toml::table & table, const std::string & prefix);
};
}  // namespace PinPthread

#endif  // PTS_H_
