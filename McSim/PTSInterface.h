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

#ifndef PTSINTERFACE_H_
#define PTSINTERFACE_H_

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

#endif  // PTSINTERFACE_H_
