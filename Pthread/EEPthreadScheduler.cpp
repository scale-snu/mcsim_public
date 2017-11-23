#include "EEPthreadScheduler.h"
#include <assert.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>

using namespace PinPthread;
using namespace std;


/* --------------------------------------------------------------------------- */
/* PthreadScheduler Constructor and Destructor                                 */
/* --------------------------------------------------------------------------- */
PthreadScheduler::PthreadScheduler(uint32_t _pid, uint32_t _total_num, char * _tmp_shared):
  nactive(0), curr_time(0), pth_to_hth(), total_instrs(0),
  total_discarded_instrs(0), total_discarded_mem_rd(0),
  total_discarded_mem_wr(0), total_discarded_2nd_mem_rd(0),
  num_cond_broadcast(0), num_cond_signal(0), num_cond_wait(0),
  num_barrier_wait(0),
  pid(_pid), total_num(_total_num), tmp_shared(_tmp_shared), skip_first(0), first_instrs(0), agile_bank_th(0)
{
  pts          = new PthreadTimingSimulator(pid, total_num, tmp_shared); 
  hth_to_pth   = vector<pthread_queue_t::iterator>(pts->num_hthreads);
  pthreads_dummy[0] = new Pthread(NULL, NULL, 0, 0, 0, pts);
  pthreads_dummy[0]->active = false;

  page_sz_log2       = pts->get_param_uint64("pts.mc.page_sz_base_bit", 12);
  ignore_skip_instrs = pts->get_param_bool("pts.ignore_skip_instrs", false);
  repeat_playing     = pts->get_param_bool("pts.repeat_playing", false);
  num_page_allocated = 0;
}


PthreadScheduler::~PthreadScheduler()
{
  resume_simulation(true, true);  // send kill signal to backend
  KillThread(GetCurrentThread());

  cout << "  -- total number of unsimulated (ins, rd, wr, rd_2nd): (" 
    << total_discarded_instrs << ", " << total_discarded_mem_rd << ", "
    << total_discarded_mem_wr << ", " << total_discarded_2nd_mem_rd << ")" << endl;
  cout << "  -- (cond_broadcast, cond_signal, cond_wait, barrier) = ("
    << num_cond_broadcast << ", "
    << num_cond_signal << ", "
    << num_cond_wait << ", "
    << num_barrier_wait << ")" << endl;

  delete pts;
}


void PthreadScheduler::PlayTraces(const string & trace_name, uint64_t trace_skip_first)
{
}


uint64_t PthreadScheduler::GetPhysicalAddr(uint64_t vaddr)
{
  uint64_t paddr;

  if (v_to_p.find(vaddr >> page_sz_log2) == v_to_p.end())
  {
    v_to_p.insert(std::pair<uint64_t, uint64_t>(vaddr >> page_sz_log2, num_page_allocated++));
  }
  paddr = v_to_p[vaddr >> page_sz_log2];
  paddr <<= page_sz_log2;
  paddr += (vaddr % (1 << page_sz_log2));

  return paddr;
}



/* --------------------------------------------------------------------------- */
/* AddThread:                                                                  */
/* add an active thread to the queue                                           */
/* --------------------------------------------------------------------------- */

void PthreadScheduler::AddThread(
    pthread_t thread, pthread_attr_t* attr,
    CONTEXT* startctxt,
    ADDRINT func, ADDRINT arg)
{
  ASSERTX(pthreads.find(thread) == pthreads.end());
  pthreads[thread] = new Pthread(attr, startctxt, func, arg, curr_time, pts);
  if (pthreads.size() == 1) 
  {
    current = pthreads.begin();
  }
  else if (pthreads.size() > pts->num_hthreads)
  {
    std::cout << "  -- (# of pthreads) > (# of hthreads) is not supported yet" << std::endl;
    exit(1);
  }
  nactive++;

  // create a mapping between pthread and hthread
  // currently, it is assumed that #(pthread) == #(hthread)
  hth_to_pth[pthreads.size()-1] = pthreads.find(thread);
  pth_to_hth[pthreads[thread]]  = pthreads.size()-1;
  pts->set_stack_n_size(pthreads.size()-1, 
      (ADDRINT)pthreads.find(thread)->second->stack,
      (ADDRINT)pthreads.find(thread)->second->stacksize);
  pts->set_active(pthreads.size()-1, pthreads.find(thread)->second->active);

  cout << "  -- [" << std::setw(12) << pts->get_curr_time() << "]: {"
    << setw(2) << pid << "} thread " << pth_to_hth[pthreads[thread]] << " is created" << std::endl;

  if (pthreads.size() > 1)
  {
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

void PthreadScheduler::KillThread(pthread_t thread) 
{
  if (thread == GetCurrentThread()) 
  {
    ASSERTX(IsActive(thread));
  }

  pts->set_stack_n_size(pth_to_hth[pthreads[thread]], 0, 0);
  pts->set_active(pth_to_hth[pthreads[thread]], false);
  cout << "  -- [" << std::setw(12) << pts->get_curr_time() << "]: {"
    << setw(2) << pid << "} thread " << pth_to_hth[pthreads[thread]] << " is killed : ";
  delete pthreads[thread];
  pthreads.erase(thread);
  nactive--;
}



/* --------------------------------------------------------------------------- */
/* BlockThread:                                                                */
/* deschedule the given thread                                                 */
/* --------------------------------------------------------------------------- */

void PthreadScheduler::BlockThread(pthread_t thread, const CONTEXT * ctxt, uint64_t barrier, uint32_t n_part)
{
  if (barrier != 0)
  { // gajh: send barrier address to timing simulator through waddr
    // gajh: send # barrier participants through wlen
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, barrier, n_part, 0, 0, 0, 0, 0,
        false, false, false, false, true,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
  ASSERTX(IsActive(thread));
  SetActiveState(thread, false);
  nactive--;

  ASSERT(nactive > 0, "[ERROR] Deadlocked!\n");

  /*for (int i = REG_GR_BASE; i <= REG_LAST; i++)
    {
    if (i >= REG_XMM_BASE && i <= REG_YMM_LAST) continue;
    current->second->registers[i] = PIN_GetContextReg(ctxt, (REG)i);
    }*/
  PIN_SaveContext(ctxt, GetCurrentContext());
  PIN_GetContextFPState(ctxt, current->second->fpstate);
  current->second->executed = false;

  resume_simulation(true);
}

/* --------------------------------------------------------------------------- */
/* UnblockThread:                                                              */
/* enable the given thread to be scheduled again                               */
/* --------------------------------------------------------------------------- */

void PthreadScheduler::UnblockThread(pthread_t thread, bool isbarrier) 
{
  ASSERTX(!IsActive(thread));
  SetActiveState(thread, true);
  //if (curr_time >= 8266900) cout << "AA" << endl;
  //if (isbarrier == false)
  {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, 0, 0, 0, 0, 0, 0, 0,
        //false, false, false, false, isbarrier,
        false, false, false, false, false,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
  nactive++;
}



void PthreadScheduler::add_synch_instruction(pthread_t thread, bool islock, bool isunlock, uint64_t barrier, uint32_t n_part)
{
  Pthread *pthread = current->second;
  if ((ignore_skip_instrs == false && pthread->skip_instrs > 0 && pthread->spinning <= 0)/* ||
                                                                                            (skip_first > first_instrs + total_discarded_instrs)*/)
  {
    return;
  }
  if (barrier != 0)
  {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, barrier, n_part, 0, 0, 0, 0, 0,
        false, false, islock, isunlock, true,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
  else
  {
    pts->add_instruction(pth_to_hth[GetThreadPtr(thread)->second], curr_time, 0, 0, 0, 0, 0, 0, 0,
        false, false, islock, isunlock, false,
        0, 0, 0, 0, 0, 0, 0, 0);
  }
}



/* --------------------------------------------------------------------------- */
/* GetCurrentThread:                                                           */
/* return the id of the current thread running                                 */
/* --------------------------------------------------------------------------- */

pthread_t PthreadScheduler::GetCurrentThread() 
{
  return current->first;
}

/* --------------------------------------------------------------------------- */
/* IsThreadValid:                                                              */
/* determine whether the given thread is valid (active or inactive)            */
/* --------------------------------------------------------------------------- */

bool PthreadScheduler::IsThreadValid(pthread_t thread) 
{
  return (pthreads.find(thread) != pthreads.end());
}

/* --------------------------------------------------------------------------- */
/* GetAttr:                                                                    */
/* return the given thread's attribute fields relevant to the scheduler        */
/* --------------------------------------------------------------------------- */

void PthreadScheduler::GetAttr(pthread_t thread, pthread_attr_t* attr) 
{
  pthread_queue_t::iterator threadptr = pthreads.find(thread);
  ADDRINT stacksize = (threadptr->second)->stacksize;
  ADDRINT* stack = (threadptr->second)->stack;
  if (stack == NULL) 
  {
    PthreadAttr::_pthread_attr_setstack(attr, (void*)0xbfff0000, 0x10000);
  }
  else 
  {
    PthreadAttr::_pthread_attr_setstack(attr, (void*)stack, stacksize);
  }
}

/* --------------------------------------------------------------------------- */
/* GetNumActiveThreads:                                                        */
/* return the number of currently active threads                               */
/* --------------------------------------------------------------------------- */

UINT32 PthreadScheduler::GetNumActiveThreads() 
{
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
    uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3)
{
  Pthread * pthread = current->second;

  if (pthread->executed == false && context != NULL)
  {
    pthread->executed = true;
    return;
  }

  //first_instrs++;
  if ((ignore_skip_instrs == false && pthread->skip_instrs > 0 && pthread->spinning <= 0)/* ||
                                                                                            (skip_first > first_instrs + total_discarded_instrs)*/)
  {
    return;
  }

  pthread->num_ins_mem_rd     += (raddr)  ? 1 : 0;
  pthread->num_ins_mem_wr     += (waddr)  ? 1 : 0;
  pthread->num_ins_2nd_mem_rd += (raddr2) ? 1 : 0;
  pthread->num_ins++;
  pthread->num_ins_for_spinning += (pthread->spinning > 0) ? 1 : 0;
  total_instrs++;

  /*if (agile_bank_th >= 1.0)
    { // temporary removed. this code this code sent all memory requests to agile rows 
  // when I allocate every address space (100% of mica bucket sorted file) of mica's bucket
  if (raddr  != 0) raddr  |= ((uint64_t)1 << 63);
  if (raddr2 != 0) raddr2 |= ((uint64_t)1 << 63);
  if (waddr  != 0) waddr  |= ((uint64_t)1 << 63);
  if (ip     != 0) ip     |= ((uint64_t)1 << 63);
  }*/
  //else if (agile_bank_th > 0)
  if (agile_bank_th > 0)
  {
    if (raddr != 0 &&
        addr_perc.find(raddr >> page_sz_log2) != addr_perc.end() &&
        addr_perc[raddr >> page_sz_log2] < agile_bank_th)
    {
      raddr |= ((uint64_t)1 << 63);
    }
    if (raddr2 != 0 &&
        addr_perc.find(raddr2 >> page_sz_log2) != addr_perc.end() &&
        addr_perc[raddr2 >> page_sz_log2] < agile_bank_th)
    {
      raddr2 |= ((uint64_t)1 << 63);
    }
    if (waddr != 0 &&
        addr_perc.find(waddr >> page_sz_log2) != addr_perc.end() &&
        addr_perc[waddr >> page_sz_log2] < agile_bank_th)
    {
      waddr |= ((uint64_t)1 << 63);
    }
    if (ip != 0 &&
        addr_perc.find(ip >> page_sz_log2) != addr_perc.end() &&
        addr_perc[ip >> page_sz_log2] < agile_bank_th)
    {
      ip |= ((uint64_t)1 << 63);
    }
  }
  bool must_resume = pts->add_instruction(pth_to_hth[current->second], curr_time,
      waddr, wlen, raddr, raddr2, rlen, ip, category,
      isbranch, isbranchtaken, false, false, false,
      rr0, rr1, rr2, rr3, rw0, rw1, rw2, rw3, true);

  if (must_resume)
  {
    if (nactive > 1)
    {
      /*for (int i = REG_GR_BASE; i <= REG_LAST; i++)
        {
        if (i >= REG_XMM_BASE && i <= REG_YMM_LAST) continue;
        current->second->registers[i] = PIN_GetContextReg(context, (REG)i);
        }*/
      PIN_SaveContext(context, GetCurrentContext());
      PIN_GetContextFPState(context, current->second->fpstate);
      pthread->executed = false;
    }
    resume_simulation();
  }
}



void PthreadScheduler::resume_simulation(bool must_switch, bool killed)
{
  pair<uint32_t, uint64_t> ret_val;
  ret_val   = pts->resume_simulation(must_switch, killed);
  curr_time = ret_val.second;
  /*if (curr_time >= 8266000)
    {
    cout << ret_val.first << " --- " << ret_val.second << " --- "
    << (hth_to_pth[ret_val.first] == current) << " --- "
    << HasStarted(current) << endl;
    }*/

  if (hth_to_pth[ret_val.first] == current && HasStarted(current)) 
  {
    current->second->executed = true;
    return;
  }
  current   = hth_to_pth[ret_val.first];


  if (nactive > 1 || must_switch)
  {
    if (!HasStarted(current))
    {
      StartThread(current);
    }
    PIN_SetContextFPState(GetCurrentStartCtxt(), current->second->fpstate);
    PIN_ExecuteAt(GetCurrentStartCtxt());
  }
}



void PthreadScheduler::set_stack(CONTEXT * ctxt)
{
  if (current->second->stack == NULL)
  {
    current->second->stacksize = pts->get_param_uint64("stack_sz", def_stack_sz);
    current->second->stack     = (ADDRINT *)(PIN_GetContextReg(ctxt, REG_STACK_PTR) -
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
    ADDRINT arg, uint64_t curr_time_, PthreadTimingSimulator * const pts_) :
  active(true), executed(true), curr_time(curr_time_), num_ins(0), num_ins_for_spinning(0),
  num_ins_mem_rd(0), num_ins_mem_wr(0), num_ins_2nd_mem_rd(0), 
  skip_instrs(0), spinning(0), num_mutex_lock(0), num_mutex_trylock(0)
{
  if (_startctxt != NULL)   // new threads
  {
    started = false;
    stacksize = pts_->get_param_uint64("stack_sz", def_stack_sz);
    if (((stacksize / sizeof(ADDRINT)) % 2) == 0)       // align stack
    {
      stacksize += sizeof(ADDRINT);
    }
    stack = (ADDRINT*)mmap(
        0, stacksize,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANON,
        -1, 0);
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
  }
  else                      // initial thread
  {
    stack     = NULL;
    stacksize = 0;
    started   = true;

    registers = new ADDRINT[REG_LAST + 1];
    fpstate   = new FPSTATE;
  }
}



Pthread::~Pthread() 
{
  delete [] registers;
  //CHAR * fpstate_char = reinterpret_cast<CHAR *>(fpstate);
  munmap(stack, stacksize);

  std::cout << "  -- num_ins : (mem_rd, mem_wr, 2nd_mem_rd, spin, lock, trylock, all)=";
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

pthread_queue_t::iterator PthreadScheduler::GetThreadPtr(pthread_t thread) 
{
  pthread_queue_t::iterator threadptr = pthreads.find(thread);
  ASSERTX(threadptr != pthreads.end());
  return threadptr;
}



bool PthreadScheduler::IsActive(pthread_t thread) 
{
  return IsActive(GetThreadPtr(thread));
}



bool PthreadScheduler::IsActive(pthread_queue_t::iterator threadptr) 
{
  return ((threadptr->second)->active);
}



void PthreadScheduler::SetActiveState(pthread_t thread, bool active) 
{
  pthread_queue_t::iterator threadptr = GetThreadPtr(thread);
  (threadptr->second)->active = active;
  pts->set_active(threadptr->first, (threadptr->second)->active);
}



bool PthreadScheduler::HasStarted(pthread_queue_t::iterator threadptr) 
{
  return ((threadptr->second)->started);
}



void PthreadScheduler::StartThread(pthread_queue_t::iterator threadptr) 
{
  (threadptr->second)->started = true;
}



CONTEXT* PthreadScheduler::GetCurrentContext() 
{
  return (&((current->second)->startctxt));
}



CONTEXT* PthreadScheduler::GetCurrentStartCtxt() 
{
  return (&((current->second)->startctxt));
}
