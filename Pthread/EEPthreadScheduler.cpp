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
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "EEPthreadScheduler.h"


namespace PinPthread {

/* --------------------------------------------------------------------------- */
/* PthreadScheduler Constructor and Destructor                                 */
/* --------------------------------------------------------------------------- */
PthreadScheduler::PthreadScheduler(uint32_t _pid, uint32_t _total_num, char * _tmp_shared):
  nactive(0), curr_time(0), pth_to_hth(), total_instrs(0),
  total_discarded_instrs(0), total_discarded_mem_rd(0),
  total_discarded_mem_wr(0), total_discarded_2nd_mem_rd(0),
  num_cond_broadcast(0), num_cond_signal(0), num_cond_wait(0),
  num_barrier_wait(0),
  pid(_pid), total_num(_total_num), tmp_shared(_tmp_shared), skip_first(0), first_instrs(0) {
  pts          = new PthreadTimingSimulator(pid, total_num, tmp_shared);
  hth_to_pth   = std::vector<pthread_queue_t::iterator>(pts->num_hthreads);
  pthreads_dummy[0] = new Pthread(nullptr, nullptr, 0, 0, 0, pts);
  pthreads_dummy[0]->active = false;

  ignore_skip_instrs = pts->get_param_bool("pts.ignore_skip_instrs", false);
  repeat_playing     = pts->get_param_bool("pts.repeat_playing", false);
  num_page_allocated = 0;
}


PthreadScheduler::~PthreadScheduler() {
  resume_simulation(true, true);  // send kill signal to backend
  KillThread(GetCurrentThread());

  cout << "  ++ {" << setw(2) << pid << "} total number of unsimulated (ins, rd, wr, rd_2nd): ("
    << total_discarded_instrs << ", " << total_discarded_mem_rd << ", "
    << total_discarded_mem_wr << ", " << total_discarded_2nd_mem_rd << ")" << endl;
  cout << "  ++ {" << setw(2) << pid << "} (cond_broadcast, cond_signal, cond_wait, barrier) = ("
    << num_cond_broadcast << ", "
    << num_cond_signal << ", "
    << num_cond_wait << ", "
    << num_barrier_wait << ")" << endl;

  delete pts;
}


void PthreadScheduler::PlayTraces(const string & trace_name, uint64_t trace_skip_first) {
  uint64_t num_sent_instrs = 0;
  do {
    ifstream trace_file(trace_name.c_str(), ios::binary);
    if (trace_file.fail()) {
      cout << "failed to open " << trace_name << endl;
      return;
    }

    Footer* footer = new Footer;
    PTSInstrTrace * instrs = new PTSInstrTrace[instr_group_size];
    const size_t maxCompressedLength = snappy::MaxCompressedLength(sizeof(PTSInstrTrace)*instr_group_size);
    size_t * compressed_length = new size_t;
    (*compressed_length) = 0;
    char * compressed = new char[maxCompressedLength];
    size_t *slice_count = new size_t;

    // read footer in trace file
    trace_file.seekg(0, ios::end);
    size_t total_size = trace_file.tellg();
    size_t offset = total_size - sizeof(struct Footer);

    trace_file.seekg(offset, ios::beg);
    trace_file.read(reinterpret_cast<char *>(footer), sizeof(struct Footer));

    ASSERT(footer->magic_number_ == kTraceMagicNumber, "[ERROR] magic number!\n");
    ASSERT(footer->offset_ == offset, "[ERROR] offset!\n");
    *slice_count = footer->total_slice_;

    trace_file.seekg(0, ios::beg);

    do {
      trace_file.read(reinterpret_cast<char *>(slice_count), sizeof(size_t));
      trace_file.read(reinterpret_cast<char *>(compressed_length), sizeof(size_t));
      trace_file.read(compressed, *compressed_length);
      if (snappy::RawUncompress(compressed, *compressed_length, reinterpret_cast<char *>(instrs)) == false) {
        cout << "file " << trace_name << " is corrupted" << endl;
        trace_file.close();
        return;
      }

      for (uint32_t i = 0; i < instr_group_size; i++) {
        PTSInstrTrace & curr_instr = instrs[i];

        if (num_sent_instrs++ >= trace_skip_first) {
          process_ins(
              nullptr,
              curr_instr.ip,
              curr_instr.raddr,
              curr_instr.raddr2,
              curr_instr.rlen,
              curr_instr.waddr,
              curr_instr.wlen,
              curr_instr.isbranch,
              curr_instr.isbranchtaken,
              curr_instr.category,
              curr_instr.rr0,
              curr_instr.rr1,
              curr_instr.rr2,
              curr_instr.rr3,
              curr_instr.rw0,
              curr_instr.rw1,
              curr_instr.rw2,
              curr_instr.rw3);
        }
      }
    } while (*slice_count < footer->total_slice_);


    delete[] instrs;
    delete[] compressed;
    delete[] slice_count;
  } while (repeat_playing == true);
}


/* --------------------------------------------------------------------------- */
/* AddThread:                                                                  */
/* add an active thread to the queue                                           */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::AddThread(
    pthread_t thread, pthread_attr_t* attr,
    CONTEXT* startctxt,
    ADDRINT func, ADDRINT arg) {
  ASSERTX(pthreads.find(thread) == pthreads.end());
  pthreads[thread] = new Pthread(attr, startctxt, func, arg, curr_time, pts);
  if (pthreads.size() == 1) {
    current = pthreads.begin();
  } else if (pthreads.size() > pts->num_hthreads) {
    std::cout << "  ++ (# of pthreads) > (# of hthreads) is not supported yet" << std::endl;
    exit(1);
  }
  nactive++;

  // create a mapping between pthread and hthread
  // currently, it is assumed that #(pthread) == #(hthread)
  auto pthread_num = pthreads.size()-1;
  hth_to_pth.insert(hth_to_pth.begin()+pthread_num, pthreads.find(thread));
  pth_to_hth[pthreads[thread]] = pthread_num;

  pts->set_stack_n_size(pthread_num,
      (ADDRINT)pthreads.find(thread)->second->stack,
      (ADDRINT)pthreads.find(thread)->second->stacksize);
  pts->set_active(pthread_num, pthreads.find(thread)->second->active);

  cout << "  ++ [" << std::setw(12) << pts->get_curr_time() << "]: {"
    << setw(2) << pid << "} thread " << pth_to_hth[pthreads[thread]] << " is created" << std::endl;

  if (pthread_num) {
    // instead of switching context, let the method resume_simulation()
    // find the newly added thread at the next time the method is called.
    pts->add_instruction(pthreads.size()-1, curr_time, 0, 0, 0, 0, 0, 0, 0,
        false, false, false, false, false,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
}


/* --------------------------------------------------------------------------- */
/* KillThread:                                                                 */
/* destroy the given thread                                                    */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::KillThread(pthread_t thread) {
  if (thread == GetCurrentThread()) {
    ASSERTX(IsActive(thread));
  }

  pts->set_stack_n_size(pth_to_hth[pthreads[thread]], 0, 0);
  pts->set_active(pth_to_hth[pthreads[thread]], false);
  cout << "  ++ [" << std::setw(12) << pts->get_curr_time() << "]: {"
    << setw(2) << pid << "} thread " << pth_to_hth[pthreads[thread]] << " is killed : ";
  delete pthreads[thread];
  pthreads.erase(thread);
  nactive--;
}


/* --------------------------------------------------------------------------- */
/* BlockThread:                                                                */
/* deschedule the given thread                                                 */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::BlockThread(pthread_t thread, const CONTEXT * ctxt, uint64_t barrier, uint32_t n_part) {
  if (barrier != 0) {
    // gajh: send barrier address to the timing simulator through waddr
    // gajh: send # barrier participants through wlen
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, barrier, n_part, 0, 0, 0, 0, 0,
        false, false, false, false, true,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
  ASSERTX(IsActive(thread));
  SetActiveState(thread, false);
  nactive--;

  ASSERT(nactive > 0, "[ERROR] Deadlocked!\n");

  PIN_SaveContext(ctxt, GetCurrentContext());
  PIN_GetContextFPState(ctxt, current->second->fpstate);
  current->second->executed = false;

  resume_simulation(true);
}


/* --------------------------------------------------------------------------- */
/* UnblockThread:                                                              */
/* enable the given thread to be scheduled again                               */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::UnblockThread(pthread_t thread, bool isbarrier) {
  ASSERTX(!IsActive(thread));
  SetActiveState(thread, true);
  // if (isbarrier == false)
  {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, 0, 0, 0, 0, 0, 0, 0,
        // false, false, false, false, isbarrier,
        false, false, false, false, false,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
  nactive++;
}


void PthreadScheduler::add_synch_instruction(pthread_t thread, bool islock, bool isunlock, uint64_t barrier, uint32_t n_part) {
  Pthread *pthread = current->second;
  if ((ignore_skip_instrs == false && pthread->skip_instrs > 0 && pthread->spinning <= 0)
      /* || (skip_first > first_instrs + total_discarded_instrs) */) {
    return;
  }
  if (barrier != 0) {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, barrier, n_part, 0, 0, 0, 0, 0,
        false, false, islock, isunlock, true,
        0, 0, 0, 0, 0, 0, 0, 0);
  } else {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, 0, 0, 0, 0, 0, 0, 0,
        false, false, islock, isunlock, false,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
}


/* --------------------------------------------------------------------------- */
/* GetCurrentThread:                                                           */
/* return the id of the current thread running                                 */
/* --------------------------------------------------------------------------- */
pthread_t PthreadScheduler::GetCurrentThread() {
  return current->first;
}


/* --------------------------------------------------------------------------- */
/* IsThreadValid:                                                              */
/* determine whether the given thread is valid (active or inactive)            */
/* --------------------------------------------------------------------------- */
bool PthreadScheduler::IsThreadValid(pthread_t thread) {
  return (pthreads.find(thread) != pthreads.end());
}


/* --------------------------------------------------------------------------- */
/* GetAttr:                                                                    */
/* return the given thread's attribute fields relevant to the scheduler        */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::GetAttr(pthread_t thread, pthread_attr_t* attr) {
  pthread_queue_t::iterator threadptr = pthreads.find(thread);
  ADDRINT stacksize = (threadptr->second)->stacksize;
  ADDRINT* stack = (threadptr->second)->stack;
  if (stack == nullptr) {
    PthreadAttr::_pthread_attr_setstack(attr, reinterpret_cast<void*>(0xbfff0000), 0x10000);
  } else {
    PthreadAttr::_pthread_attr_setstack(attr, reinterpret_cast<void*>(stack), stacksize);
  }
}


/* --------------------------------------------------------------------------- */
/* GetNumActiveThreads:                                                        */
/* return the number of currently active threads                               */
/* --------------------------------------------------------------------------- */
UINT32 PthreadScheduler::GetNumActiveThreads() {
  return nactive;
}


/* --------------------------------------------------------------------------- */
/* Scheduling Functions:                                                       */
/* --------------------------------------------------------------------------- */
void PthreadScheduler::process_ins(
    const CONTEXT * context, ADDRINT ip,
    ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
    ADDRINT waddr, UINT32 wlen,
    bool isbranch, bool isbranchtaken, uint32_t category,
    uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
    uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3) {
  Pthread * pthread = current->second;

  if (pthread->executed == false && context != nullptr) {
    pthread->executed = true;
    return;
  }

  // first_instrs++;
  if ((ignore_skip_instrs == false && pthread->skip_instrs > 0 && pthread->spinning <= 0)
      /* ||(skip_first > first_instrs + total_discarded_instrs) */) {
    return;
  }

  pthread->num_ins_mem_rd     += (raddr)  ? 1 : 0;
  pthread->num_ins_mem_wr     += (waddr)  ? 1 : 0;
  pthread->num_ins_2nd_mem_rd += (raddr2) ? 1 : 0;
  pthread->num_ins++;
  pthread->num_ins_for_spinning += (pthread->spinning > 0) ? 1 : 0;
  total_instrs++;

  bool must_resume = pts->add_instruction(pth_to_hth[current->second], curr_time,
      waddr, wlen, raddr, raddr2, rlen, ip, category,
      isbranch, isbranchtaken, false, false, false,
      rr0, rr1, rr2, rr3, rw0, rw1, rw2, rw3, true);

  if (must_resume) {
    if (nactive > 1) {
      /* for (int i = REG_GR_BASE; i <= REG_LAST; i++) {
        if (i >= REG_XMM_BASE && i <= REG_YMM_LAST) continue;
        current->second->registers[i] = PIN_GetContextReg(context, (REG)i);
      } */
      PIN_SaveContext(context, GetCurrentContext());
      PIN_GetContextFPState(context, current->second->fpstate);
      pthread->executed = false;
    }
    resume_simulation();
  }
}


void PthreadScheduler::resume_simulation(bool must_switch, bool killed) {
  std::pair<uint32_t, uint64_t> ret_val;
  ret_val   = pts->resume_simulation(must_switch, killed);
  curr_time = ret_val.second;

  if (hth_to_pth.at(ret_val.first) == current && HasStarted(current)) {
    current->second->executed = true;
    return;
  }
  current   = hth_to_pth.at(ret_val.first);

  if (nactive > 1 || must_switch) {
    if (!HasStarted(current)) {
      StartThread(current);
    }
    PIN_SetContextFPState(GetCurrentStartCtxt(), current->second->fpstate);
    PIN_ExecuteAt(GetCurrentStartCtxt());
  }
}


void PthreadScheduler::set_stack(CONTEXT * ctxt) {
  if (current->second->stack == nullptr) {
    current->second->stacksize = pts->get_param_uint64("stack_sz", def_stack_sz);
    current->second->stack     = reinterpret_cast<ADDRINT *>(PIN_GetContextReg(ctxt, REG_STACK_PTR) -
        current->second->stacksize + sizeof(ADDRINT));
    pts->set_stack_n_size(current->first,
        (ADDRINT) current->second->stack,
        (ADDRINT) current->second->stacksize);
  }
}


/* --------------------------------------------------------------------------- */
/* Pthread Constructor and Destructor:                                         */
/* --------------------------------------------------------------------------- */
Pthread::Pthread(
    pthread_attr_t* attr, CONTEXT* _startctxt, ADDRINT func,
    ADDRINT arg, uint64_t curr_time_, PthreadTimingSimulator * const pts_):
  active(true), executed(true), curr_time(curr_time_), num_ins(0), num_ins_for_spinning(0),
  num_ins_mem_rd(0), num_ins_mem_wr(0), num_ins_2nd_mem_rd(0),
  skip_instrs(0), spinning(0), num_mutex_lock(0), num_mutex_trylock(0) {
  if (_startctxt != nullptr) {  // new threads
    started = false;
    stacksize = pts_->get_param_uint64("stack_sz", def_stack_sz);
    if (((stacksize / sizeof(ADDRINT)) % 2) == 0) {  // align stack
      stacksize += sizeof(ADDRINT);
    }
    stack = reinterpret_cast<ADDRINT *>(mmap(
        0, stacksize,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANON,
        -1, 0));
    ASSERTX(stack != MAP_FAILED);
    mprotect(stack, sizeof(ADDRINT), PROT_NONE);        // delineate top of stack
    ADDRINT* sp = &(stack[stacksize/sizeof(ADDRINT) - 1]);
    ASSERTX(((ADDRINT)sp & 0x7) == 0);

#ifdef TARGET_IA32E
    ASSERTX(((ADDRINT)sp & 0x8) == 0);
    *(--sp) = (ADDRINT)StartThreadFunc;
    PIN_SaveContext(_startctxt, &startctxt);
    PIN_SetContextReg(&startctxt, REG_STACK_PTR, (ADDRINT)sp);
    PIN_SetContextReg(&startctxt, REG_GDI, (ADDRINT)arg);
    PIN_SetContextReg(&startctxt, REG_INST_PTR, (ADDRINT)func);
#else
    *(sp--) = arg;
    *(sp--) = func;
    PIN_SaveContext(_startctxt, &startctxt);
    PIN_SetContextReg(&startctxt, REG_STACK_PTR, (ADDRINT)sp);
    PIN_SetContextReg(&startctxt, REG_INST_PTR, (ADDRINT)StartThreadFunc);
#endif
    registers = new ADDRINT[REG_LAST + 1];
    fpstate   = new FPSTATE;
    PIN_GetContextFPState(_startctxt, fpstate);
  } else {  // initial thread
    stack     = nullptr;
    stacksize = 0;
    started   = true;

    registers = new ADDRINT[REG_LAST + 1];
    fpstate   = new FPSTATE;
  }
}


Pthread::~Pthread() {
  delete [] registers;
  delete fpstate;
  // CHAR * fpstate_char = reinterpret_cast<CHAR *>(fpstate);
  munmap(stack, stacksize);

  std::cout << "  ++ num_ins : (mem_rd, mem_wr, 2nd_mem_rd, spin, lock, trylock, all)=";
  std::cout << "( " << dec << setw(10) << num_ins_mem_rd << ", "
    << setw(10) << num_ins_mem_wr << ", "
    << setw(8) << num_ins_2nd_mem_rd << ", "
    << setw(8) << num_ins_for_spinning << ", "
    << setw(8) << num_mutex_lock << ", "
    << setw(8) << num_mutex_trylock << ", "
    << setw(10) << num_ins << ")" << std::endl;
}


/* --------------------------------------------------------------------------- */
/* Functions for Manipulating STL Structure(s):                                */
/* --------------------------------------------------------------------------- */
pthread_queue_t::iterator PthreadScheduler::GetThreadPtr(pthread_t thread) {
  auto threadptr = pthreads.find(thread);
  ASSERTX(threadptr != pthreads.end());
  return threadptr;
}


bool PthreadScheduler::IsActive(pthread_t thread) {
  return IsActive(GetThreadPtr(thread));
}


bool PthreadScheduler::IsActive(pthread_queue_t::iterator threadptr) {
  return ((threadptr->second)->active);
}


void PthreadScheduler::SetActiveState(pthread_t thread, bool active) {
  auto threadptr = GetThreadPtr(thread);
  (threadptr->second)->active = active;
  pts->set_active(threadptr->first, (threadptr->second)->active);
}


bool PthreadScheduler::HasStarted(pthread_queue_t::iterator threadptr) {
  return ((threadptr->second)->started);
}


void PthreadScheduler::StartThread(pthread_queue_t::iterator threadptr) {
  (threadptr->second)->started = true;
}


CONTEXT* PthreadScheduler::GetCurrentContext() {
  return (&((current->second)->startctxt));
}


CONTEXT* PthreadScheduler::GetCurrentStartCtxt() {
  return (&((current->second)->startctxt));
}

}  // namespace PinPthread
