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
/* PthreadMutexAttr:                                                           */
/* manipulates pthread mutex attribute objects                                 */
/* --------------------------------------------------------------------------- */

#ifndef PTHREAD_MUTEXATTR_H
#define PTHREAD_MUTEXATTR_H

#include "PthreadUtil.h"

/* Mutex attribute data structure.  */
struct pthread_mutexattr
{
  int mutexkind;
};

namespace PinPthread
{

  class PthreadMutexAttr
  {
    public:
      static pthread_mutexattr_t PTHREAD_MUTEXATTR_DEFAULT();
    public:
      static int _pthread_mutexattr_destroy(pthread_mutexattr_t*);
      static int _pthread_mutexattr_getkind(const pthread_mutexattr_t*, int*);
      static int _pthread_mutexattr_init(pthread_mutexattr_t*);
      static int _pthread_mutexattr_setkind(pthread_mutexattr_t*, int);
  };

} // namespace PinPthread

#endif  // #ifndef PTHREAD_MUTEXATTR_H
