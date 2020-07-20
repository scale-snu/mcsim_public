#include "pin.H"
#include "mypthreadtool.h"
#include <iostream>
#include <ostream>

using namespace PinPthread;
using namespace std;


int main(int argc, char** argv) 
{
  Init(argc, argv);
  PIN_InitSymbols();
  PIN_Init(argc, argv);
  IMG_AddInstrumentFunction(FlagImg, 0);
  RTN_AddInstrumentFunction(FlagRtn, 0);
  TRACE_AddInstrumentFunction(FlagTrace, 0);
  PIN_AddFiniFunction(Fini, 0);
  PIN_StartProgram();
  return 0;
}


namespace PinPthread
{
  VOID Init(int32_t argc, char** argv) 
  {
    pthreadsim = new PthreadSim(argc, argv);
  }

  VOID Fini(INT32 code, VOID* v) 
  {
    delete pthreadsim;
  }

  VOID ProcessMemIns(
      CONTEXT * context,
      ADDRINT ip,
      ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
      ADDRINT waddr, UINT32  wlen,
      BOOL    isbranch,
      BOOL    isbranchtaken,
      UINT32  category,
      UINT32  rr0,
      UINT32  rr1,
      UINT32  rr2,
      UINT32  rr3,
      UINT32  rw0,
      UINT32  rw1,
      UINT32  rw2,
      UINT32  rw3)
  { // for memory address and register index, '0' means invalid
    if (pthreadsim->first_instrs < pthreadsim->skip_first)
    {
      pthreadsim->first_instrs++;
      return;
    }
    else if (pthreadsim->first_instrs == pthreadsim->skip_first)
    {
      pthreadsim->first_instrs++;
      pthreadsim->initiate(context);
    }
    pthreadsim->process_ins(
        context,
        ip,
        raddr, raddr2, rlen,
        waddr,         wlen,
        isbranch,
        isbranchtaken,
        category,
        rr0, rr1, rr2, rr3,
        rw0, rw1, rw2, rw3);
  }

  VOID NewPthreadSim(CONTEXT* ctxt)
  {
    pthreadsim->set_stack(ctxt);
  }



  /* ------------------------------------------------------------------ */
  /* Instrumentation Routines                                           */
  /* ------------------------------------------------------------------ */

  VOID FlagImg(IMG img, VOID* v) 
  {
    RTN rtn;
    rtn = RTN_FindByName(img, "__kmp_get_global_thread_id");
    if (rtn != RTN_Invalid()) 
    {
      RTN_Replace(rtn, (AFUNPTR)CallPthreadSelf);
    }
    rtn = RTN_FindByName(img, "__kmp_check_stack_overlap");
    if (rtn != RTN_Invalid()) 
    {
      RTN_Replace(rtn, (AFUNPTR)DummyFunc);
    }
    rtn = RTN_FindByName(img, "mcsim_skip_instrs_begin");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
    }
    rtn = RTN_FindByName(img, "mcsim_skip_instrs_end");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
    }
    rtn = RTN_FindByName(img, "mcsim_spinning_begin");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningBegin);
    }
    rtn = RTN_FindByName(img, "mcsim_spinning_end");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningEnd);
    }
    rtn = RTN_FindByName(img, "__parsec_bench_begin");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
    }
    rtn = RTN_FindByName(img, "__parsec_roi_begin");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
    }
    rtn = RTN_FindByName(img, "__parsec_roi_end");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
    }
    rtn = RTN_FindByName(img, "__parsec_bench_end");
    if (rtn != RTN_Invalid())
    {
      RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
    }
    rtn = RTN_FindByName(img, "pthread_attr_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_getdetachstate");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetdetachstate,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_getstackaddr");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstackaddr,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_getstacksize");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstacksize,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_getstack");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstack,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_setdetachstate");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetdetachstate,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_setstackaddr");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstackaddr,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_setstacksize");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstacksize,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_attr_setstack");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstack,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cancel");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCancel,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cleanup_pop");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPop,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cleanup_push");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPush,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_condattr_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_condattr_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_broadcast");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondBroadcast,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_signal");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondSignal,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_timedwait");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondTimedwait,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_cond_wait");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondWait,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_create");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCreate,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_detach");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadDetach,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_equal");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadEqual,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "mock_pthread_exit");
    if (rtn != RTN_Invalid())
    {
      cerr << "mock_pthread_exit" << endl;
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadExit,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_END);
    }
    /*rtn = RTN_FindByName(img, "pthread_exit");
      if (rtn != RTN_Invalid())
      {
      cerr << "mypthreadtool.cpp: pthread_exit" << endl;
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadExit,
      IARG_CONTEXT,
      IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
      IARG_END);
      }*/
    rtn = RTN_FindByName(img, "pthread_getattr");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetattr,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_getspecific");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetspecific,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_join");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadJoin,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_key_create");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyCreate,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_key_delete");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyDelete,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_kill");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKill,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_gettype");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_getkind");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_settype");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutexattr_setkind");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutex_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutex_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexLock,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexTrylock,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexUnlock,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_once");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadOnce,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_self");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSelf,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_setcancelstate");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcancelstate,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_setcanceltype");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcanceltype,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_setspecific");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "libc_tsd_set");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_testcancel");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadTestcancel,
          IARG_CONTEXT,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrier_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrier_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierDestroy,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrier_wait");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierWait,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrierattr_init");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrInit,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrierattr_destroy");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrDestory,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrierattr_getpshared");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrGetpshared,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
    rtn = RTN_FindByName(img, "pthread_barrierattr_setpshared");
    if (rtn != RTN_Invalid())
    {
      RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrSetpshared,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
          IARG_RETURN_REGS, REG_GAX,
          IARG_END);
    }
  }


  VOID FlagRtn(RTN rtn, VOID* v) 
  {
    RTN_Open(rtn);
    string * rtn_name = new string(RTN_Name(rtn));
#if VERYVERBOSE
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)PrintRtnName,
        IARG_PTR, rtn_name,
        IARG_END);
#endif
    if (rtn_name->find("main") != string::npos)
    {
      RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)NewPthreadSim,
          IARG_CONTEXT,
          IARG_END);
    }
    else if (((rtn_name->find("__kmp") != string::npos) &&
          (rtn_name->find("yield") != string::npos)) ||
        (rtn_name->find("__sleep") != string::npos) ||
        (rtn_name->find("__kmp_wait_sleep") != string::npos))
    {
      RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)DoContextSwitch,
          IARG_CONTEXT,
          IARG_END);
    }
    /*else if ((rtn_name->find("__pthread_return_void") != string::npos) ||
      (rtn_name->find("pthread_mutex_t") != string::npos) ||
      (rtn_name->find("pthread_atfork") != string::npos))
      {
      }*/
    else if ((rtn_name->find("pthread") != string::npos) ||
        (rtn_name->find("sigwait") != string::npos) ||
        (rtn_name->find("tsd") != string::npos) ||
        ((rtn_name->find("fork") != string::npos) &&
         (rtn_name->find("__kmp") == string::npos)))
    {
      RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)WarnNYI,
          IARG_PTR, rtn_name,
          IARG_INST_PTR,
          IARG_END);
    }
    RTN_Close(rtn);
  }


  VOID FlagTrace(TRACE trace, VOID* v) 
  {
    if (TRACE_Address(trace) == (ADDRINT)mock_pthread_exit) 
    {
      TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)CallPthreadExit,
          IARG_CONTEXT,
          IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
          IARG_END);
    }
    else if (!INS_IsAddedForFunctionReplacement(BBL_InsHead(TRACE_BblHead(trace)))) 
    {
      for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
      {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) 
        {
          if (INS_IsCall(ins) && !INS_IsDirectControlFlow(ins))    // indirect call
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
                IARG_BRANCH_TARGET_ADDR,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_BOOL, false,
                IARG_END);
          }
          else if (INS_IsDirectControlFlow(ins))    // tail call or conventional call
          {
            ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
            RTN src_rtn = INS_Rtn(ins);
            RTN dest_rtn = RTN_FindByAddress(target);
            if (INS_IsCall(ins) || (src_rtn != dest_rtn)) 
            {
              BOOL tailcall = !INS_IsCall(ins);
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
                  IARG_ADDRINT, target,
                  IARG_FUNCARG_CALLSITE_VALUE, 0,
                  IARG_FUNCARG_CALLSITE_VALUE, 1,
                  IARG_BOOL, tailcall,
                  IARG_END);
            }
          }
          else if (INS_IsRet(ins))                                          // return
          {
            RTN rtn = INS_Rtn(ins);
            if (RTN_Valid(rtn) && (RTN_Name(rtn) != "_dl_runtime_resolve")) 
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessReturn,
                  IARG_PTR, new string(RTN_Name(rtn)),
                  IARG_END);
            }
          }

          if (((INS_Address(ins) - (ADDRINT)StartThreadFunc) < 0) ||
              ((INS_Address(ins) - (ADDRINT)StartThreadFunc) > 8 * sizeof(ADDRINT)))
          {
            bool is_mem_wr   = INS_IsMemoryWrite(ins);
            bool is_mem_rd   = INS_IsMemoryRead(ins);
            bool has_mem_rd2 = INS_HasMemoryRead2(ins);

            if (is_mem_wr && is_mem_rd && has_mem_rd2) 
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_MEMORYREAD_EA,
                  IARG_MEMORYREAD2_EA,
                  IARG_MEMORYREAD_SIZE,
                  IARG_MEMORYWRITE_EA,
                  IARG_MEMORYWRITE_SIZE,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32,  INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
            else if (is_mem_wr && is_mem_rd && !has_mem_rd2) 
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_MEMORYREAD_EA,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_MEMORYREAD_SIZE,
                  IARG_MEMORYWRITE_EA,
                  IARG_MEMORYWRITE_SIZE,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32,  INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
            else if (is_mem_wr && !is_mem_rd) 
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_UINT32, 0,
                  IARG_MEMORYWRITE_EA,
                  IARG_MEMORYWRITE_SIZE,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32, INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
            else if (!is_mem_wr && is_mem_rd && has_mem_rd2)
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_MEMORYREAD_EA,
                  IARG_MEMORYREAD2_EA,
                  IARG_MEMORYREAD_SIZE,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_UINT32, 0,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32, INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
            else if (!is_mem_wr && is_mem_rd && !has_mem_rd2) 
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_MEMORYREAD_EA,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_MEMORYREAD_SIZE,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_UINT32, 0,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32, INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
            else
            {
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                  IARG_CONTEXT,
                  IARG_INST_PTR,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_UINT32,  0,
                  IARG_ADDRINT, (ADDRINT)0,
                  IARG_UINT32,  0,
                  IARG_BOOL, INS_IsControlFlow(ins),
                  IARG_BRANCH_TAKEN,
                  IARG_UINT32, INS_Category(ins),
                  IARG_UINT32, INS_RegR(ins, 0),
                  IARG_UINT32, INS_RegR(ins, 1),
                  IARG_UINT32, INS_RegR(ins, 2),
                  IARG_UINT32, INS_RegR(ins, 3),
                  IARG_UINT32, INS_RegW(ins, 0),
                  IARG_UINT32, INS_RegW(ins, 1),
                  IARG_UINT32, INS_RegW(ins, 2),
                  IARG_UINT32, INS_RegW(ins, 3),
                  IARG_END);
            }
          }
        }
      }
    }
  }


  /* ------------------------------------------------------------------ */
  /* Pthread Hooks                                                      */
  /* ------------------------------------------------------------------ */

  int CallPthreadAttrDestroy(ADDRINT _attr) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_destroy((pthread_attr_t*)_attr);" << endl;
    return PthreadAttr::_pthread_attr_destroy((pthread_attr_t*)_attr);
  }

  int CallPthreadAttrGetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_getdetachstate((pthread_attr_t*)_attr, (int*)_detachstate);" << endl;
    return PthreadAttr::_pthread_attr_getdetachstate((pthread_attr_t*)_attr, (int*)_detachstate);
  }

  int CallPthreadAttrGetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_getstack((pthread_attr_t*)_attr, (void**)_stackaddr, (size_t*)_stacksize);" << endl;
    return PthreadAttr::_pthread_attr_getstack((pthread_attr_t*)_attr, (void**)_stackaddr, (size_t*)_stacksize);
  }

  int CallPthreadAttrGetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_getstackaddr((pthread_attr_t*)_attr, (void**)_stackaddr);" << endl;
    return PthreadAttr::_pthread_attr_getstackaddr((pthread_attr_t*)_attr, (void**)_stackaddr);
  }

  int CallPthreadAttrGetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_getstacksize((pthread_attr_t*)_attr, (size_t*)_stacksize);" << endl;
    return PthreadAttr::_pthread_attr_getstacksize((pthread_attr_t*)_attr, (size_t*)_stacksize);
  }

  int CallPthreadAttrInit(ADDRINT _attr) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_init((pthread_attr_t*)_attr);" << endl;
    return PthreadAttr::_pthread_attr_init((pthread_attr_t*)_attr);
  }

  int CallPthreadAttrSetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_setdetachstate((pthread_attr_t*)_attr, (int)_detachstate);" << endl;
    return PthreadAttr::_pthread_attr_setdetachstate((pthread_attr_t*)_attr, (int)_detachstate);
  }

  int CallPthreadAttrSetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_setstack((pthread_attr_t*)_attr, (void*)_stackaddr, (size_t)_stacksize);" << endl;
    return PthreadAttr::_pthread_attr_setstack((pthread_attr_t*)_attr, (void*)_stackaddr, (size_t)_stacksize);
  }

  int CallPthreadAttrSetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_setstackaddr((pthread_attr_t*)_attr, (void*)_stackaddr);" << endl;
    return PthreadAttr::_pthread_attr_setstackaddr((pthread_attr_t*)_attr, (void*)_stackaddr);
  }

  int CallPthreadAttrSetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
  {
    //cerr << "return PthreadAttr::_pthread_attr_setstacksize((pthread_attr_t*)_attr, (size_t)_stacksize);" << endl;
    return PthreadAttr::_pthread_attr_setstacksize((pthread_attr_t*)_attr, (size_t)_stacksize);
  }

  int CallPthreadCancel(ADDRINT _thread) 
  {
    return pthreadsim->pthread_cancel((pthread_t)_thread);
  }

  VOID CallPthreadCleanupPop(CONTEXT* ctxt, ADDRINT _execute) 
  {
    pthreadsim->pthread_cleanup_pop_((int)_execute, ctxt);
  }

  VOID CallPthreadCleanupPush(ADDRINT _routine, ADDRINT _arg) 
  {
    pthreadsim->pthread_cleanup_push_(_routine, _arg);
  }

  int CallPthreadCondattrDestroy(ADDRINT _attr) 
  {
    return 0;
  }

  int CallPthreadCondattrInit(ADDRINT _attr) 
  {
    return 0;
  }

  int CallPthreadCondBroadcast(ADDRINT _cond) 
  {
    //cerr << "return pthreadsim->pthread_cond_broadcast((pthread_cond_t*)_cond);" << endl;
    return pthreadsim->pthread_cond_broadcast((pthread_cond_t*)_cond);

  }

  int CallPthreadCondDestroy(ADDRINT _cond) 
  {
    //cerr << "return pthreadsim->pthread_cond_destroy((pthread_cond_t*)_cond);" << endl;
    return pthreadsim->pthread_cond_destroy((pthread_cond_t*)_cond);
  }

  int CallPthreadCondInit(ADDRINT _cond, ADDRINT _condattr) 
  {
    //cerr << "return PthreadCond::pthread_cond_init((pthread_cond_t*)_cond, (pthread_condattr_t*)_condattr);" << endl;
    return PthreadCond::pthread_cond_init((pthread_cond_t*)_cond, (pthread_condattr_t*)_condattr);
  }

  int CallPthreadCondSignal(ADDRINT _cond) 
  {
    //cerr << "return pthreadsim->pthread_cond_signal((pthread_cond_t*)_cond);" << endl;
    return pthreadsim->pthread_cond_signal((pthread_cond_t*)_cond);
  }

  VOID CallPthreadCondTimedwait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex, ADDRINT _abstime) 
  {
    //cerr << "pthreadsim->pthread_cond_timedwait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, (const struct timespec*)_abstime, context);" << endl;
    pthreadsim->pthread_cond_timedwait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, (const struct timespec*)_abstime, context);
  }

  VOID CallPthreadCondWait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex) 
  {
    //cerr << "pthreadsim->pthread_cond_wait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, context);" << endl;
    pthreadsim->pthread_cond_wait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, context);
  }

  VOID CallPthreadCreate(CONTEXT* ctxt,
      ADDRINT _thread, ADDRINT _attr, ADDRINT _func, ADDRINT _arg) 
  {
    //cerr << "pthreadsim->pthread_create((pthread_t*)_thread, (pthread_attr_t*)_attr, ctxt, _func, _arg);" << endl;
    pthreadsim->pthread_create((pthread_t*)_thread, (pthread_attr_t*)_attr, ctxt, _func, _arg);
  }

  int CallPthreadDetach(ADDRINT _th) 
  {
    //cerr << "return pthreadsim->pthread_detach((pthread_t)_th);" << endl;
    return pthreadsim->pthread_detach((pthread_t)_th);
  }

  int CallPthreadEqual(ADDRINT _thread1, ADDRINT _thread2) 
  {
    //cerr << "return pthreadsim->pthread_equal((pthread_t)_thread1, (pthread_t)_thread2);" << endl;
    return pthreadsim->pthread_equal((pthread_t)_thread1, (pthread_t)_thread2);
  }

  VOID CallPthreadExit(CONTEXT* ctxt, ADDRINT _retval)  
  {
    //cerr << "pthreadsim->pthread_exit((void*)_retval, ctxt);" << endl;
    pthreadsim->pthread_exit((void*)_retval, ctxt);
  }

  int CallPthreadGetattr(ADDRINT _th, ADDRINT _attr) 
  {
    //cerr << "return pthreadsim->pthread_getattr((pthread_t)_th, (pthread_attr_t*)_attr);" << endl;
    return pthreadsim->pthread_getattr((pthread_t)_th, (pthread_attr_t*)_attr);
  }

  VOID* CallPthreadGetspecific(ADDRINT _key) 
  {
    //cerr << "return pthreadsim->pthread_getspecific((pthread_key_t)_key);" << endl;
    return pthreadsim->pthread_getspecific((pthread_key_t)_key);
  }

  VOID CallPthreadJoin(CONTEXT* ctxt,
      ADDRINT _th, ADDRINT _thread_return)
  {
    pthreadsim->pthread_join((pthread_t)_th, (void**)_thread_return, ctxt);
  }

  int CallPthreadKeyCreate(ADDRINT _key, ADDRINT _func) 
  {
    return pthreadsim->pthread_key_create((pthread_key_t*)_key, (void(*)(void*))_func);
  }

  int CallPthreadKeyDelete(ADDRINT _key) 
  {
    //cerr << "return pthreadsim->pthread_key_delete((pthread_key_t)_key);" << endl;
    return pthreadsim->pthread_key_delete((pthread_key_t)_key);
  }

  int CallPthreadKill(ADDRINT _thread, ADDRINT _signo) 
  {
    //cerr << "return pthreadsim->pthread_kill((pthread_t)_thread, (int)_signo);" << endl;
    return pthreadsim->pthread_kill((pthread_t)_thread, (int)_signo);
  }

  int CallPthreadMutexattrDestroy(ADDRINT _attr) 
  {
    //cerr << "return PthreadMutexAttr::_pthread_mutexattr_destroy((pthread_mutexattr_t*) _attr);" << endl;
    return PthreadMutexAttr::_pthread_mutexattr_destroy((pthread_mutexattr_t*) _attr);
  }

  int CallPthreadMutexattrGetkind(ADDRINT _attr, ADDRINT _kind) 
  {
    //cerr << "return PthreadMutexAttr::_pthread_mutexattr_getkind((pthread_mutexattr_t*)_attr, (int*)_kind);" << endl;
    return PthreadMutexAttr::_pthread_mutexattr_getkind((pthread_mutexattr_t*)_attr, (int*)_kind);
  }

  int CallPthreadMutexattrInit(ADDRINT _attr) 
  {
    //cerr << "return PthreadMutexAttr::_pthread_mutexattr_init((pthread_mutexattr_t*)_attr);" << endl;
    return PthreadMutexAttr::_pthread_mutexattr_init((pthread_mutexattr_t*)_attr);
  }

  int CallPthreadMutexattrSetkind(ADDRINT _attr, ADDRINT _kind) 
  {
    //cerr << "return PthreadMutexAttr::_pthread_mutexattr_setkind((pthread_mutexattr_t*)_attr, (int)_kind);" << endl;
    return PthreadMutexAttr::_pthread_mutexattr_setkind((pthread_mutexattr_t*)_attr, (int)_kind);
  }

  int CallPthreadMutexDestroy(ADDRINT _mutex) 
  {
    //cerr << "return PthreadMutex::_pthread_mutex_destroy((pthread_mutex_t*)_mutex);" << endl;
    return PthreadMutex::_pthread_mutex_destroy((pthread_mutex_t*)_mutex);
  }

  int CallPthreadMutexInit(ADDRINT _mutex, ADDRINT _mutexattr) 
  {
    //cerr << "return PthreadMutex::_pthread_mutex_init((pthread_mutex_t*)_mutex, (pthread_mutexattr_t*)_mutexattr);" << endl;
    return PthreadMutex::_pthread_mutex_init((pthread_mutex_t*)_mutex, (pthread_mutexattr_t*)_mutexattr);
  }

  VOID CallPthreadMutexLock(CONTEXT* context, ADDRINT _mutex) 
  {
    //cerr << "pthreadsim->pthread_mutex_lock((pthread_mutex_t*)_mutex, context);" << endl;
    pthreadsim->pthread_mutex_lock((pthread_mutex_t*)_mutex, context);
  }

  int CallPthreadMutexTrylock(ADDRINT _mutex) 
  {
    //cerr << "return pthreadsim->pthread_mutex_trylock((pthread_mutex_t*)_mutex);" << endl;
    return pthreadsim->pthread_mutex_trylock((pthread_mutex_t*)_mutex);
  }

  int CallPthreadMutexUnlock(ADDRINT _mutex) 
  {
    //cerr << "return pthreadsim->pthread_mutex_unlock((pthread_mutex_t*)_mutex);" << endl;
    return pthreadsim->pthread_mutex_unlock((pthread_mutex_t*)_mutex);
  }

  VOID CallPthreadOnce(CONTEXT* ctxt, ADDRINT _oncecontrol, ADDRINT _initroutine) 
  {
    //cerr << "PthreadOnce::pthread_once((pthread_once_t*)_oncecontrol, _initroutine, ctxt);" << endl;
    PthreadOnce::pthread_once((pthread_once_t*)_oncecontrol, _initroutine, ctxt);
  }

  pthread_t CallPthreadSelf() 
  {
    //cerr << "return pthreadsim->pthread_self();" << endl;
    return pthreadsim->pthread_self();
  }

  int CallPthreadSetcancelstate(ADDRINT _state, ADDRINT _oldstate) 
  {
    //cerr << "return pthreadsim->pthread_setcancelstate((int)_state, (int*)_oldstate);" << endl;
    return pthreadsim->pthread_setcancelstate((int)_state, (int*)_oldstate);
  }

  int CallPthreadSetcanceltype(ADDRINT _type, ADDRINT _oldtype) 
  {
    //cerr << "return pthreadsim->pthread_setcanceltype((int)_type, (int*)_oldtype);" << endl;
    return pthreadsim->pthread_setcanceltype((int)_type, (int*)_oldtype);
  }

  int CallPthreadSetspecific(ADDRINT _key, ADDRINT _pointer) 
  {
    //cerr << "return pthreadsim->pthread_setspecific((pthread_key_t)_key, (VOID*)_pointer);" << endl;
    return pthreadsim->pthread_setspecific((pthread_key_t)_key, (VOID*)_pointer);
  }

  int CallPthreadBarrierInit(ADDRINT _barrier, ADDRINT _barrierattr, ADDRINT num) 
  {
    //cerr << "return pthreadsim->pthread_barrier_init((pthread_barrier_t *)_barrier, (pthread_barrierattr_t *)_barrierattr, (unsigned int) num);" << endl;
    return pthreadsim->pthread_barrier_init((pthread_barrier_t *)_barrier, (pthread_barrierattr_t *)_barrierattr, (unsigned int) num);
  }

  int CallPthreadBarrierDestroy(ADDRINT _barrier) 
  {
    //cerr << "return pthreadsim->pthread_barrier_destroy((pthread_barrier_t *)_barrier);" << endl;
    return pthreadsim->pthread_barrier_destroy((pthread_barrier_t *)_barrier);
  }

  int CallPthreadBarrierWait(CONTEXT* context, ADDRINT _barrier) 
  {
    //cerr << "return pthreadsim->pthread_barrier_wait((pthread_barrier_t *)_barrier, context);" << endl;
    return pthreadsim->pthread_barrier_wait((pthread_barrier_t *)_barrier, context);
  }

  int CallPthreadBarrierattrInit(ADDRINT _barrierattr)
  {
    return 0;  // not implemented yet
  }

  int CallPthreadBarrierattrDestory(ADDRINT _barrierattr)
  {
    return 0;  // not implemented yet
  }

  int CallPthreadBarrierattrGetpshared(ADDRINT _barrierattr, ADDRINT value)
  {
    return 0;  // not implemented yet
  }

  int CallPthreadBarrierattrSetpshared(ADDRINT _barrierattr, ADDRINT value)
  {
    return 0;  // not implemented yet
  }

  VOID CallPthreadTestcancel(CONTEXT* ctxt) 
  {
    pthreadsim->pthread_testcancel(ctxt);
  }

  VOID CallMcSimSkipInstrsBegin()
  {
    pthreadsim->mcsim_skip_instrs_begin();
  }

  VOID CallMcSimSkipInstrsEnd()
  {
    pthreadsim->mcsim_skip_instrs_end();
  }

  VOID CallMcSimSpinningBegin()
  {
    pthreadsim->mcsim_spinning_begin();
  }

  VOID CallMcSimSpinningEnd()
  {
    pthreadsim->mcsim_spinning_end();
  }


  /* ------------------------------------------------------------------ */
  /* Thread-Safe Memory Allocation Support                              */
  /* ------------------------------------------------------------------ */

  VOID ProcessCall(ADDRINT target, ADDRINT arg0, ADDRINT arg1, BOOL tailcall) 
  {
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(target);
    PIN_UnlockClient();
    if (RTN_Valid(rtn)) 
    {
      string temp_string(RTN_Name(rtn));
      pthreadsim->threadsafemalloc(true, tailcall, &temp_string);
    }
  }

  VOID ProcessReturn(const string* rtn_name) 
  {
    ASSERTX(rtn_name != NULL);
    pthreadsim->threadsafemalloc(false, false, rtn_name);
  }

  /* ------------------------------------------------------------------ */
  /* Thread Scheduler                                                   */
  /* ------------------------------------------------------------------ */

  VOID DoContextSwitch(CONTEXT* context) 
  {
    pthreadsim->docontextswitch(context);
  }

  /* ------------------------------------------------------------------ */
  /* Debugging Support                                                  */
  /* ------------------------------------------------------------------ */

  VOID WarnNYI(const string* rtn_name,
      ADDRINT ip) 
  {
    std::cout << "NYI: " << *rtn_name << " at: 0x" << hex << ip << dec <<  "\n" << flush;
    //ASSERTX(0);
  }

  VOID PrintRtnName(const string* rtn_name) 
  {
    std::cout << "RTN " << *rtn_name << "\n" << flush;
  }

} // namespace PinPthread
