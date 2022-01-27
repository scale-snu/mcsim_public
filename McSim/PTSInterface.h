// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef MCSIM_PTSINTERFACE_H_
#define MCSIM_PTSINTERFACE_H_

#include <stdint.h>

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

union instr_n_str {
  PTSInstr   instr[instr_batch_size];
  char       str[1024];
};

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

#endif  // MCSIM_PTSINTERFACE_H_
