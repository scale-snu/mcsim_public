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

typedef uint8_t  UINT8;   // LINUX HOSTS
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef int8_t  INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

#if defined(TARGET_IA32)
typedef UINT32 ADDRINT;
typedef INT32 ADDRDELTA;
#elif defined(TARGET_IPF) || defined(TARGET_IA32E)
typedef UINT64 ADDRINT;
typedef INT64 ADDRDELTA;
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


const uint32_t instr_batch_size = 32;

enum pts_msg_type {
  pts_constructor,
  pts_destructor,
  pts_resume_simulation,
  pts_add_instruction,
  pts_set_stack_n_size,
  pts_set_active,
  pts_get_num_hthreads,
  pts_get_param_uint64,
  pts_get_param_bool,
  pts_get_curr_time,
  pts_invalid,
};


struct PTSInstr {
  uint32_t hthreadid_;
  uint64_t curr_time_;
  uint64_t waddr;
  UINT32   wlen;
  uint64_t raddr;
  uint64_t raddr2;
  UINT32   rlen;
  uint64_t ip;
  uint32_t category;
  bool     isbranch;
  bool     isbranchtaken;
  bool     islock;
  bool     isunlock;
  bool     isbarrier;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};

typedef union {
  PTSInstr   instr[instr_batch_size];
  char       str[1024];
} instr_n_str;

struct PTSMessage {
  pts_msg_type type;
  bool         bool_val;
  bool         killed;
  uint32_t     uint32_t_val;
  uint64_t     uint64_t_val;
  ADDRINT      stack_val;
  ADDRINT      stacksize_val;
  instr_n_str  val;
};


namespace PinPthread  {
class McSim;

class PthreadTimingSimulator {
 public:
  explicit PthreadTimingSimulator(const std::string & mdfile);
  explicit PthreadTimingSimulator(int port_num) { }
  ~PthreadTimingSimulator();

  std::pair<uint32_t, uint64_t> resume_simulation(bool must_switch);
  // return value -- whether we have to resume simulation
  bool add_instruction(
      uint32_t hthreadid_,
      uint64_t curr_time_,
      uint64_t waddr,
      UINT32   wlen,
      uint64_t raddr,
      uint64_t raddr2,
      UINT32   rlen,
      uint64_t ip,
      uint32_t category,
      bool     isbranch,
      bool     isbranchtaken,
      bool     islock,
      bool     isunlock,
      bool     isbarrier,
      uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
      uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3);
  void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
  void set_active(int32_t pth_id, bool is_active);

  uint32_t get_num_hthreads() const;
  uint64_t get_param_uint64(const std::string & idx_, uint64_t def_value) const;
  bool     get_param_bool(const std::string & idx_, bool def_value) const;
  std::string   get_param_str(const std::string & idx_) const;
  uint64_t get_curr_time() const;

 private:
  // std::map<string, string> params;
  std::map<std::string, bool> params_bool;
  std::map<std::string, uint64_t> params_uint64_t;
  std::map<std::string, std::string> params_string;

  void md_table_decoding(const toml::table & table, const std::string & prefix);

 public:
  std::vector<std::string>      trace_files;

  McSim * mcsim;
};
}  // namespace PinPthread

#endif  // PTS_H_
