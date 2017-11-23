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
/* PthreadUtil:                                                                */
/* helper functions and header files needed by the rest of the pthread library */
/* --------------------------------------------------------------------------- */

#ifndef PTHREAD_UTIL_H
#define PTHREAD_UTIL_H
#include "pin.H"
#include <set>
#include <map>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <bits/local_lim.h>
#include <errno.h>

#include <limits.h>
#include <x86_64-linux-gnu/sys/cdefs.h>
#include "mcsim_pthread.h"

#include "PthreadAttr.h"

#define __SIZEOF_PTHREAD_ATTR_T 56
#define __SIZEOF_PTHREAD_MUTEX_T 40
#define __SIZEOF_PTHREAD_MUTEXATTR_T 4
#define __SIZEOF_PTHREAD_COND_T 48
#define __SIZEOF_PTHREAD_CONDATTR_T 4
#define __SIZEOF_PTHREAD_RWLOCK_T 56
#define __SIZEOF_PTHREAD_RWLOCKATTR_T 8
#define __SIZEOF_PTHREAD_BARRIER_T 32
#define __SIZEOF_PTHREAD_BARRIERATTR_T 4


typedef union {
  char __size[__SIZEOF_PTHREAD_BARRIER_T];
  long int __align;
} pthread_barrier_t;

typedef union {
  char __size[__SIZEOF_PTHREAD_BARRIERATTR_T];
  int __align;
} pthread_barrierattr_t;

/* Flags in mutex attr.  */
#define PTHREAD_MUTEXATTR_PROTOCOL_SHIFT  28
#define PTHREAD_MUTEXATTR_PROTOCOL_MASK   0x30000000
#define PTHREAD_MUTEXATTR_PRIO_CEILING_SHIFT  12
#define PTHREAD_MUTEXATTR_PRIO_CEILING_MASK 0x00fff000
#define PTHREAD_MUTEXATTR_FLAG_ROBUST   0x40000000
#define PTHREAD_MUTEXATTR_FLAG_PSHARED    0x80000000
#define PTHREAD_MUTEXATTR_FLAG_BITS \
  (PTHREAD_MUTEXATTR_FLAG_ROBUST | PTHREAD_MUTEXATTR_FLAG_PSHARED \
   | PTHREAD_MUTEXATTR_PROTOCOL_MASK | PTHREAD_MUTEXATTR_PRIO_CEILING_MASK)

/* Mutex types.  */
enum
{
  PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
};

extern int pthread_mutexattr_gettype (const pthread_mutexattr_t *__restrict __attr, int *__restrict __kind) __THROW __nonnull ((1, 2));
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) __THROW __nonnull ((1));

extern int pthread_attr_setdetachstate (pthread_attr_t *__attr, int __detachstate) __THROW __nonnull ((1));
extern int pthread_attr_getstack (const pthread_attr_t *__restrict __attr, void **__restrict __stackaddr, size_t *__restrict __stacksize) __THROW __nonnull ((1, 2, 3));
extern int pthread_attr_setstack (pthread_attr_t *__attr, void *__stackaddr, size_t __stacksize) __THROW __nonnull ((1));

extern int mock_pthread_exit();




namespace PinPthread 
{
#define VERBOSE 0            // basic pthread calls
#define VERYVERBOSE 0        // routine-level info 
#define VERYVERYVERBOSE 0    // context-switching-level info

  void StartThreadFunc(void*(*)(void*), void*);
}

#endif  // #ifndef PTHREAD_UTIL_H
