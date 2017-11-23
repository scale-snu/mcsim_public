/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/* --------------------------------------------------------------------------- */
/* PthreadScheduler:                                                           */
/* schedules the next thread to run                                            */
/* provides the context switching mechanism                                    */
/* --------------------------------------------------------------------------- */

#ifndef EE_PTHREAD_SCHEDULER_H
#define EE_PTHREAD_SCHEDULER_H

#include "PthreadUtil.h"
#include "PTS.h"
#include <list>
#include <stack>
#include <queue>
#include <vector>
#include <map>
#include <iomanip>

const static uint32_t def_stack_sz = 0x800000;


namespace PinPthread 
{
  class Pthread
  {
    public:
      Pthread(pthread_attr_t*, CONTEXT*, ADDRINT, ADDRINT, uint64_t curr_time_, PthreadTimingSimulator * const pts);
      ~Pthread();
    public:
      bool active;           // whether the thread can be scheduled to run
      bool started;          // whether the thread has begun execution
      bool executed;
      ADDRINT* stack;        // thread-specific stack space
      ADDRINT stacksize;     // size of the stack
      CONTEXT startctxt;     // context to start this thread
      ADDRINT * registers;
      FPSTATE * fpstate;

      uint64_t curr_time;
      uint64_t num_ins;
      uint64_t num_ins_for_spinning;
      uint64_t num_ins_mem_rd;
      uint64_t num_ins_mem_wr;
      uint64_t num_ins_2nd_mem_rd;
      int32_t  skip_instrs;
      int32_t  spinning;
      uint64_t num_mutex_lock;
      uint64_t num_mutex_trylock;
  };

  typedef std::map<pthread_t, Pthread*> pthread_queue_t;


  class PthreadScheduler
  {
    public:
      PthreadScheduler(uint32_t pid, uint32_t total_num, char * tmp_shared);
      ~PthreadScheduler();

      void AddThread(pthread_t, pthread_attr_t*, CONTEXT*, ADDRINT, ADDRINT);
      void KillThread(pthread_t);
      void BlockThread(pthread_t, const CONTEXT *, uint64_t barrier = 0, uint32_t n_part = 0);
      void UnblockThread(pthread_t, bool isbarrier = false);
      pthread_t GetCurrentThread();
      bool IsThreadValid(pthread_t);
      void GetAttr(pthread_t, pthread_attr_t*);
      UINT32 GetNumActiveThreads();

      void PlayTraces(const string & trace_name, uint64_t trace_skip_first);
    private:
      uint64_t GetPhysicalAddr(uint64_t vaddr);
      inline pthread_queue_t::iterator GetThreadPtr(pthread_t);
      inline Pthread* GetThread(pthread_queue_t::iterator);
      inline bool IsActive(pthread_t);
      inline bool IsActive(pthread_queue_t::iterator);
      inline void SetActiveState(pthread_t, bool);
      inline bool HasStarted(pthread_queue_t::iterator);
      inline void StartThread(pthread_queue_t::iterator);
    public:
      inline CONTEXT* GetCurrentContext();
      inline CONTEXT* GetCurrentStartCtxt();
    public:
      pthread_queue_t pthreads;          // list of thread state
      pthread_queue_t pthreads_dummy;    // list of thread state
      pthread_queue_t::iterator current; // pointer to the current thread running
      UINT32 nactive;                    // number of active threads

    public:
      uint64_t curr_time;  // current time of the scheduler
      void process_ins(
          const CONTEXT * context, ADDRINT ip,
          ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
          ADDRINT waddr, UINT32 wlen,
          bool isbranch, bool isbranchtaken, uint32_t category,
          uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
          uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3);
      void resume_simulation(bool must_switch = false, bool killed = false);
      void add_synch_instruction(pthread_t thread, bool islock, bool isunlock, uint64_t isbarrier = 0, uint32_t n_part = 0);
      PthreadTimingSimulator * pts;

      std::map<Pthread *, uint32_t> pth_to_hth;
      std::vector<pthread_queue_t::iterator> hth_to_pth;
      std::map<uint64_t, uint64_t> v_to_p;
      uint32_t page_sz_log2;
      uint64_t num_page_allocated;

      uint64_t total_instrs;
      uint64_t total_discarded_instrs;
      uint64_t total_discarded_mem_rd;
      uint64_t total_discarded_mem_wr;
      uint64_t total_discarded_2nd_mem_rd;
      void mcsim_skip_instrs_begin() { current->second->skip_instrs++; }
      void mcsim_skip_instrs_end()   { current->second->skip_instrs--; }
      void mcsim_spinning_begin()    { current->second->spinning++; }
      void mcsim_spinning_end()      { current->second->spinning--; }
      void set_stack(CONTEXT * ctxt);

      uint64_t num_cond_broadcast;
      uint64_t num_cond_signal;
      uint64_t num_cond_wait;
      uint64_t num_barrier_wait;

      bool      ignore_skip_instrs;
      int	      pid,total_num;
      char *    tmp_shared;
      uint64_t  skip_first;
      uint64_t  first_instrs;
      double    agile_bank_th;
      bool      repeat_playing;
      map<uint64_t, double> addr_perc;
  };

} // namespace PinPthread

#endif  // #ifndef EE_PTHREAD_SCHEDULER_H

