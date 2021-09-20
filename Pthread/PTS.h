// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef PTS_H_
#define PTS_H_

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <ostream>
#include <fstream>
#include <iomanip>
#include "pin.H"
#include "PthreadUtil.h"
#include "snappy.h"
#include "McSim/PTSInterface.h"

const uint32_t instr_group_size = 100000;

typedef int * INT_PTR;
typedef void * VOID_PTR;
typedef char * CHAR_PTR;

struct PTSInstrTrace {
  uint64_t waddr;
  uint32_t wlen;
  uint64_t raddr;
  uint64_t raddr2;
  uint32_t rlen;
  uint64_t ip;
  uint32_t category;
  bool     isbranch;
  bool     isbranchtaken;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};


namespace PinPthread {

class PthreadTimingSimulator {
 public:
  explicit PthreadTimingSimulator(uint32_t pid, uint32_t total_num, char * tmp_shared);
  ~PthreadTimingSimulator();

  std::pair<uint32_t, uint64_t> resume_simulation(bool must_switch, bool killed = false);
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
      uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3,
      bool     can_be_piled = false);  // whether we have to resume simulation
  void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
  void set_active(int32_t pth_id, bool is_active);

  uint32_t get_num_hthreads();
  uint64_t get_param_uint64(const string & idx_, uint64_t def_value);
  bool     get_param_bool(const string & idx_, bool def_value);
  uint64_t get_curr_time();

  uint32_t           num_piled_instr;      // in this object
  uint32_t           num_hthreads;
  uint32_t         * num_available_slot;   // in the timing simulator

 private:
  // shared memory interface to commnunicate with mcsim
  int     mmapfd;
  char  * maped;
  int     flag;
  int     lock_fd;

  uint32_t        pid;
  uint32_t        total_num;
  char          * tmp_shared;
  PTSMessage    * ptsmessage;
  volatile bool * mmap_flag;

#ifdef LOG_TRACE
  std::ofstream InstTraceFile;
#endif

  void send_instr_batch();
  inline void sync_with_mcsim();
#ifdef LOG_TRACE
  inline void record_inst(ADDRINT ip, ADDRINT addr, string op);
  inline void record_transfer(uint32_t num_inst);
#endif
};
}  // namespace PinPthread

#endif  // PTS_H_
