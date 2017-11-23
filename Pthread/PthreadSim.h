/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2009 Intel Corporation. All rights reserved.
 
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
/* PthreadSim:                                                                 */
/* custom pthread library implementation                                       */
/* provides the standard pthread API and context switching mechanism           */
/* --------------------------------------------------------------------------- */

#ifndef PTHREAD_SIM_H
#define PTHREAD_SIM_H

#include "PthreadMalloc.h"
#include "PthreadJoin.h"
#include "PthreadCancel.h"
#include "PthreadCleanup.h"
#include "PthreadCond.h"
#include "PthreadMutex.h"
#include "PthreadKey.h"
#include "PthreadBarrier.h"
#include <string>
#include <vector>

//#ifndef EXTENGINE
//#include "PthreadScheduler.h"
//#else
#include "EEPthreadScheduler.h"
//#endif 

namespace PinPthread 
{

  class PthreadSim 
  {
    public:
      PthreadSim(uint32_t argc, char** argv);
      ~PthreadSim();
    public:
      int   pthread_cancel(pthread_t);
      void  pthread_cleanup_pop_(int, CONTEXT*);
      void  pthread_cleanup_push_(ADDRINT, ADDRINT);
      int   pthread_cond_broadcast(pthread_cond_t*);
      int   pthread_cond_destroy(pthread_cond_t*);
      int   pthread_cond_signal(pthread_cond_t*);
      void  pthread_cond_timedwait(pthread_cond_t*, pthread_mutex_t*, const struct timespec*, CONTEXT*);
      void  pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*, CONTEXT*);
      void  pthread_create(pthread_t*, pthread_attr_t*, CONTEXT*, ADDRINT, ADDRINT);
      int   pthread_detach(pthread_t);
      int   pthread_equal(pthread_t, pthread_t);
      void  pthread_exit(void*, CONTEXT*);
      int   pthread_getattr(pthread_t, pthread_attr_t*);
      void* pthread_getspecific(pthread_key_t);
      void  pthread_join(pthread_t, void**, CONTEXT*);
      int   pthread_key_create(pthread_key_t*, void(*)(void*));
      int   pthread_key_delete(pthread_key_t);
      int   pthread_kill(pthread_t, int);
      int   pthread_mutex_lock(pthread_mutex_t*, CONTEXT*);
      int   pthread_mutex_trylock(pthread_mutex_t*);
      int   pthread_mutex_unlock(pthread_mutex_t*);
      pthread_t pthread_self();
      int   pthread_setcancelstate(int, int*);
      int   pthread_setcanceltype(int, int*);
      int   pthread_setspecific(pthread_key_t, void*);
      void  pthread_testcancel(CONTEXT*);
      int   pthread_barrier_init(pthread_barrier_t *, pthread_barrierattr_t *, unsigned int );
      int   pthread_barrier_destroy(pthread_barrier_t *);
      int   pthread_barrier_wait(pthread_barrier_t *, CONTEXT *);
    public:
      bool  inmtmode();
      void  docontextswitch(const CONTEXT *);
      void  process_ins(
          const CONTEXT * context, ADDRINT ip,
          ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
          ADDRINT waddr, UINT32 wlen,
          bool isbranch, bool isbranchtaken, uint32_t category,
          uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
          uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3);
      void  mcsim_skip_instrs_begin();
      void  mcsim_skip_instrs_end();
      void  mcsim_spinning_begin();
      void  mcsim_spinning_end();
      void  set_stack(CONTEXT * ctxt);
    public: 
      void  threadsafemalloc(bool, bool, const string*);
    private:
      pthread_t new_thread_id;               // the id to assign to the next new thread
      PthreadMalloc* mallocmanager;          // ensure thread-safe memory allocation
    public:
      PthreadScheduler* scheduler;           // schedule the threads
      uint64_t          skip_first;
      uint64_t          first_instrs;
      uint32_t          pid;
      uint32_t          total_num;
      char *            tmp_shared;
      string            trace_name;
      uint64_t          trace_skip_first;
      double            agile_bank_th;
      string            agile_page_list_file_name;
      void  initiate(CONTEXT * ctxt);

    private:
      PthreadJoinManager* joinmanager;       // join and detaches the threads
      PthreadCancelManager* cancelmanager;   // track cancellation state for all threads
      PthreadCleanupManager* cleanupmanager; // responsible for cleaning up the threads
      PthreadCondManager* condmanager;       // blocks and unblocks threads on condition variables
      PthreadMutexManager* mutexmanager;     // block and unblock threads on mutexes
      PthreadTLSManager* tlsmanager;         // manage thread local storage key/data
      PthreadBarrierManager * barriermanager;
  };

}  // namespace PinPthread

#endif  // #ifndef PTHREAD_SIM_H

