#ifndef __PTS_H__
#define __PTS_H__

#include "pin.H"
#include <list>
#include <map>
#include <queue>
#include <stack>
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "PthreadUtil.h"
#include "mcsim_snappy.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

const uint32_t instr_batch_size = 32;

typedef int * INT_PTR;
typedef void * VOID_PTR;
typedef char * CHAR_PTR;

enum pts_msg_type
{
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


struct PTSInstr
{
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


typedef union
{
  PTSInstr   instr[instr_batch_size];
  char       str[2048];
} instr_n_str;


struct PTSMessage
{
  pts_msg_type type;
  bool         bool_val;
  bool         killed;
  uint32_t     uint32_t_val;
  uint64_t     uint64_t_val;
  ADDRINT      stack_val;
  ADDRINT      stacksize_val;
  instr_n_str  val;
};


const uint32_t instr_group_size = 100000;

struct PTSInstrTrace
{
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


using namespace std;

namespace PinPthread 
{
  class McSim;

  class PthreadTimingSimulator
  {
    public:
      PthreadTimingSimulator(const string & mdfile);
      PthreadTimingSimulator(uint32_t pid, uint32_t total_num, char * tmp_shared);
      ~PthreadTimingSimulator();

      pair<uint32_t, uint64_t> resume_simulation(bool must_switch, bool killed = false);
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
          bool     can_be_piled = false
          );  // whether we have to resume simulation
      void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
      void set_active(int32_t pth_id, bool is_active);

      uint32_t get_num_hthreads();
      uint64_t get_param_uint64(const string & idx_, uint64_t def_value);
      bool     get_param_bool(const string & idx_, bool def_value);
      string   get_param_str(const string & idx_);
      uint64_t get_curr_time();

      uint32_t           num_piled_instr;      // in this object
      uint32_t           num_hthreads;
      uint32_t         * num_available_slot;   // in the timing simulator

      // Shared memory
      int     mmapfd;
      char  * maped;
      int     flag;
      int     lock_fd;

      uint32_t        pid;
      uint32_t        total_num;
      char          * tmp_shared;
      PTSMessage    * ptsmessage;
      volatile bool * mmap_flag;

    private:
      void send_instr_batch();
  };
}

#endif  //__PTS_H__
