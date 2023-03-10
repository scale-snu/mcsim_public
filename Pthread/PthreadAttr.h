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
/* PthreadAttr:                                                                */
/* manipulates pthread attribute objects                                       */
/* --------------------------------------------------------------------------- */

#ifndef PTHREAD_ATTR_H
#define PTHREAD_ATTR_H

#include "PthreadUtil.h"

struct pthread_attr
{
  /* Scheduler parameters and priority.  */
  struct sched_param schedparam;
  int schedpolicy;
  /* Various flags like detachstate, scope, etc.  */
  int flags;
  /* Size of guard area.  */
  size_t guardsize;
  /* Stack handling.  */
  void *stackaddr;
  size_t stacksize;
  /* Affinity map.  */
  cpu_set_t *cpuset;
  size_t cpusetsize;
};

#define ATTR_FLAG_DETACHSTATE   0x0001
#define ATTR_FLAG_NOTINHERITSCHED 0x0002
#define ATTR_FLAG_SCOPEPROCESS    0x0004
#define ATTR_FLAG_STACKADDR   0x0008
#define ATTR_FLAG_OLDATTR   0x0010
#define ATTR_FLAG_SCHED_SET   0x0020
#define ATTR_FLAG_POLICY_SET    0x0040

namespace PinPthread 
{

  class PthreadAttr 
  {
    public:
      static pthread_attr_t PTHREAD_ATTR_DEFAULT();
    public:
      static int _pthread_attr_destroy(pthread_attr_t*);
      static int _pthread_attr_getdetachstate(pthread_attr_t*, int*);
      static int _pthread_attr_getstack(pthread_attr_t*, void**, size_t*);
      static int _pthread_attr_getstackaddr(pthread_attr_t*, void**);
      static int _pthread_attr_getstacksize(pthread_attr_t*, size_t*);
      static int _pthread_attr_init(pthread_attr_t*);
      static int _pthread_attr_setdetachstate(pthread_attr_t*, int);
      static int _pthread_attr_setstack(pthread_attr_t*, void*, size_t);
      static int _pthread_attr_setstackaddr(pthread_attr_t*, void*);
      static int _pthread_attr_setstacksize(pthread_attr_t*, size_t);
  };

} // namespace PinPthread

#endif  // #ifndef PTHREAD_ATTR_H
