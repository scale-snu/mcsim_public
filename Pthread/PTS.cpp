// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include "PTS.h"

namespace PinPthread {

PthreadTimingSimulator::PthreadTimingSimulator(uint32_t _pid, uint32_t _total_num, char * _tmp_shared):
  num_piled_instr(0), pid(_pid), total_num(_total_num), tmp_shared(_tmp_shared) {

  // Shared memory
  if ((mmapfd = open(tmp_shared, O_RDWR, 0666)) < 0) {
    perror("open");
    exit(1);
  }

  maped = reinterpret_cast<char *>(mmap(0, sizeof(PTSMessage)+2, PROT_WRITE | PROT_READ, MAP_SHARED, mmapfd, 0));
  if (maped == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  ptsmessage = new (maped) PTSMessage;
  mmap_flag = reinterpret_cast<bool *>(maped + sizeof(PTSMessage));

#ifdef LOG_TRACE
  string trace_header = string("#\n"
                               "# Instruction Trace Transferred By McSimA+ Frontend\n"
                               "#\n");
  InstTraceFile.open("/tmp/fe_transfered_inst.out");
  InstTraceFile.write(trace_header.c_str(), trace_header.size());
  InstTraceFile.setf(ios::showbase);
#endif

  num_hthreads = get_num_hthreads();
  num_available_slot = new uint32_t[num_hthreads];
  for (uint32_t i = 0; i < num_hthreads; i++) {
    num_available_slot[i] = 1;
  }
}


PthreadTimingSimulator::~PthreadTimingSimulator() {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type        = pts_destructor;

  // Shared memory
  mmap_flag[0] = false;
  munmap(maped, sizeof(PTSMessage)+2);

  delete[] num_available_slot;

#ifdef LOG_TRACE
  InstTraceFile.close();
#endif
}


std::pair<uint32_t, uint64_t> PthreadTimingSimulator::resume_simulation(bool must_switch, bool killed) {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type     = pts_resume_simulation;
  ptsmessage->bool_val = must_switch;
  ptsmessage->killed   = killed;

  sync_with_mcsim();

  return std::pair<uint32_t, uint64_t>(ptsmessage->uint32_t_val, ptsmessage->uint64_t_val);
}


void PthreadTimingSimulator::sync_with_mcsim() {
  // shared memory
  mmap_flag[0] = false;
  while (mmap_flag[1]) { }
  mmap_flag[1] = true;
}

#ifdef LOG_TRACE
void PthreadTimingSimulator::record_inst (ADDRINT ip, ADDRINT addr, string op) {
  InstTraceFile << hex << ip << ": "
    << setw(2) << op << " "
    << setw(2+2*sizeof(uint64_t)) << hex << addr << dec << endl;
}

void PthreadTimingSimulator::record_transfer (uint32_t num_inst) {
  InstTraceFile << setw(12) << num_piled_instr << "                    transfer complete !!!" << endl;
}
#endif

bool PthreadTimingSimulator::add_instruction(
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
    bool     can_be_piled) {
  // can_be_piled = false;
  assert(num_piled_instr < instr_batch_size);
  ptsmessage->type         = pts_add_instruction;
  PTSInstr   * ptsinstr    = &(ptsmessage->val.instr[num_piled_instr]);
  ptsinstr->hthreadid_ = hthreadid_;
  ptsinstr->curr_time_ = curr_time_;
  ptsinstr->waddr      = waddr;
  ptsinstr->wlen       = wlen;
  ptsinstr->raddr      = raddr;
  ptsinstr->raddr2     = raddr2;
  ptsinstr->rlen       = rlen;
  ptsinstr->ip         = ip;
  ptsinstr->category   = category;
  ptsinstr->isbranch   = isbranch;
  ptsinstr->isbranchtaken = isbranchtaken;
  ptsinstr->islock     = islock;
  ptsinstr->isunlock   = isunlock;
  ptsinstr->isbarrier  = isbarrier;
  ptsinstr->rr0        = rr0;
  ptsinstr->rr1        = rr1;
  ptsinstr->rr2        = rr2;
  ptsinstr->rr3        = rr3;
  ptsinstr->rw0        = rw0;
  ptsinstr->rw1        = rw1;
  ptsinstr->rw2        = rw2;
  ptsinstr->rw3        = rw3;

#ifdef LOG_TRACE
  if (wlen != 0 && rlen != 0) {
    record_inst(ip, waddr, "RW");
  } else if (wlen == 0 && rlen !=0) {
    if (raddr2 != 0)
      record_inst(ip, raddr2, "R2");
    else
      record_inst(ip, raddr, "R1");
  } else if (wlen != 0 && rlen == 0) {
    record_inst(ip, waddr, "W");
  } else if (wlen == 0 && rlen == 0 && isbranch !=0) {
    record_inst(ip, 0, "B");
  } else {
    record_inst(ip, 0, "E");
  }
#endif

  if (num_piled_instr > 0 && (ptsmessage->val.instr[num_piled_instr-1]).hthreadid_ != hthreadid_) {
    cout << "  ++ [" << std::setw(12) << curr_time_ << "]:  " 
      << (ptsmessage->val.instr[num_piled_instr-1]).hthreadid_ << "  " << hthreadid_ << endl;
    exit(1);
  }

  num_piled_instr++;
  ptsmessage->uint32_t_val = num_piled_instr;

  if (can_be_piled == false || num_piled_instr >= instr_batch_size ||
    num_piled_instr >= num_available_slot[hthreadid_]/* || isbarrier == true*/) {

    sync_with_mcsim();
#ifdef LOG_TRACE
    record_transfer(ptsmessage->uint32_t_val);
#endif

    num_piled_instr    = 0;
    // return value -- how many more available slots to put instructions,
    // 0 means that that we have to resume simulation
    num_available_slot[hthreadid_] = ptsmessage->uint32_t_val;

    return (num_available_slot[hthreadid_] <= 1 ? true : false);
  } else {
    return false;
  }
}


void PthreadTimingSimulator::set_stack_n_size(
    int32_t pth_id,
    ADDRINT stack,
    ADDRINT stacksize) {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type          = pts_set_stack_n_size;
  ptsmessage->uint32_t_val  = pth_id;
  ptsmessage->stack_val     = stack;
  ptsmessage->stacksize_val = stacksize;

  sync_with_mcsim();
}


void PthreadTimingSimulator::set_active(int32_t pth_id, bool is_active) {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type         = pts_set_active;
  ptsmessage->uint32_t_val = pth_id;
  ptsmessage->bool_val     = is_active;

  sync_with_mcsim();
}


uint32_t PthreadTimingSimulator::get_num_hthreads() {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type        = pts_get_num_hthreads;

  sync_with_mcsim();

  return ptsmessage->uint32_t_val;
}


uint64_t PthreadTimingSimulator::get_param_uint64(const std::string & str, uint64_t def) {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type         = pts_get_param_uint64;
  ptsmessage->uint64_t_val = def;
  strcpy(ptsmessage->val.str, str.c_str());

  sync_with_mcsim();

  return ptsmessage->uint64_t_val;
}


bool PthreadTimingSimulator::get_param_bool(const std::string & str, bool def_value) {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type     = pts_get_param_bool;
  ptsmessage->bool_val = def_value;
  strcpy(ptsmessage->val.str, str.c_str());

  sync_with_mcsim();

  return ptsmessage->bool_val;
}


uint64_t PthreadTimingSimulator::get_curr_time() {
  if (num_piled_instr) send_instr_batch();
  ptsmessage->type         = pts_get_curr_time;

  sync_with_mcsim();

  return ptsmessage->uint64_t_val;
}


void PthreadTimingSimulator::send_instr_batch() {
  assert(ptsmessage->type == pts_add_instruction);

  sync_with_mcsim();
#ifdef LOG_TRACE
  record_transfer(ptsmessage->uint32_t_val);
#endif

  // return value -- how many more available slots to put instructions,
  // 0 means that that we have to resume simulation
  PTSInstr * ptsinstr = &(ptsmessage->val.instr[num_piled_instr-1]);
  num_available_slot[ptsinstr->hthreadid_] = ptsmessage->uint32_t_val;
  num_piled_instr     = 0;
}

}  // namespace PinPthread
