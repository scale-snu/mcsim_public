/* Copyright (C) 2002-2016 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "PthreadUtil.h"


int pthread_attr_setdetachstate (pthread_attr_t *attr,
    int detachstate)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid values.  */
  if (detachstate != PTHREAD_CREATE_DETACHED
      && __builtin_expect (detachstate != PTHREAD_CREATE_JOINABLE, 0))
    return EINVAL;

  /* Set the flag.  It is nonzero if threads are created detached.  */
  if (detachstate == PTHREAD_CREATE_DETACHED)
    iattr->flags |= ATTR_FLAG_DETACHSTATE;
  else
    iattr->flags &= ~ATTR_FLAG_DETACHSTATE;

  return 0;
}

int pthread_attr_setschedpolicy (pthread_attr_t *attr,
    int policy)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid values.  */
  if (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR)
    return EINVAL;

  /* Store the new values.  */
  iattr->schedpolicy = policy;

  /* Remember we set the value.  */
  iattr->flags |= ATTR_FLAG_POLICY_SET;

  return 0;
}

int pthread_attr_setinheritsched (pthread_attr_t *attr,
    int inherit)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid values.  */
  if (inherit != PTHREAD_INHERIT_SCHED && inherit != PTHREAD_EXPLICIT_SCHED)
    return EINVAL;

  /* Store the new values.  */
  if (inherit != PTHREAD_INHERIT_SCHED)
    iattr->flags |= ATTR_FLAG_NOTINHERITSCHED;
  else
    iattr->flags &= ~ATTR_FLAG_NOTINHERITSCHED;

  return 0;
}

int pthread_attr_setscope (pthread_attr_t *attr, int scope)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid values.  */
  switch (scope)
  {
    case PTHREAD_SCOPE_SYSTEM:
      iattr->flags &= ~ATTR_FLAG_SCOPEPROCESS;
      break;

    case PTHREAD_SCOPE_PROCESS:
      return ENOTSUP;

    default:
      return EINVAL;
  }

  return 0;
}

int pthread_attr_setstack (pthread_attr_t *attr,
    void *stackaddr, size_t stacksize)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid sizes.  */
  if (stacksize < PTHREAD_STACK_MIN)
    return EINVAL;

  iattr->stacksize = stacksize;
  iattr->stackaddr = (char *) stackaddr + stacksize;
  iattr->flags |= ATTR_FLAG_STACKADDR;

  return 0;
}

int pthread_attr_getdetachstate (const pthread_attr_t *attr,
    int *detachstate)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  *detachstate = (iattr->flags & ATTR_FLAG_DETACHSTATE
      ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);

  return 0;
}

int pthread_attr_getstack (const pthread_attr_t *attr,
    void **stackaddr, size_t *stacksize)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Store the result.  */
  *stackaddr = (char *) iattr->stackaddr - iattr->stacksize;
  *stacksize = iattr->stacksize;

  return 0;
}

#define ARCH_STACK_DEFAULT_SIZE (2 * 1024 * 1024)  
int pthread_attr_getstacksize (const pthread_attr_t *attr,
    size_t *stacksize)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* If the user has not set a stack size we return what the system
   *      will use as the default.  */
  *stacksize = iattr->stacksize ?: ARCH_STACK_DEFAULT_SIZE;

  return 0;
}

int pthread_attr_setstacksize (pthread_attr_t *attr,
    size_t stacksize)
{
  struct pthread_attr *iattr;
  iattr = (struct pthread_attr *) attr;

  /* Catch invalid sizes.  */
  if (stacksize < PTHREAD_STACK_MIN)
    return EINVAL;

  iattr->stacksize = stacksize;

  return 0;
}

int pthread_mutexattr_gettype (const pthread_mutexattr_t *attr,
    int *kind)
{
  //  const struct pthread_mutexattr *iattr;
  //  iattr = (const struct pthread_mutexattr *) attr;
  //
  //  *kind = iattr->mutexkind & ~PTHREAD_MUTEXATTR_FLAG_BITS;
  //
  return 0;
}

int pthread_mutexattr_settype (pthread_mutexattr_t *attr,
    int kind)
{
  //  struct pthread_mutexattr *iattr;
  //  if (kind < PTHREAD_MUTEX_NORMAL || kind > PTHREAD_MUTEX_ADAPTIVE_NP)
  //    return EINVAL;
  //  iattr = (struct pthread_mutexattr *) attr;
  //
  //  iattr->mutexkind = (iattr->mutexkind & PTHREAD_MUTEXATTR_FLAG_BITS) | kind;

  return 0;
}

int mock_pthread_exit(){
  //cerr << "This is mock_pthread_exit()" << endl;
  return 0;
}



namespace PinPthread
{
  /* --------------------------------------------------------------------------- */
  /* StartThreadFunc:                                                            */
  /* wrapper function for the thread func to catch the end of the thread         */
  /* --------------------------------------------------------------------------- */

  void StartThreadFunc(void*(*func)(void*), void* arg)
  {
    /* original version code */
    /*#ifdef TARGET_IA32E
        void* retval = NULL;
      #else
        void* retval = func(arg);
      #endif
        pthread_exit(retval);
    */

    mock_pthread_exit();
  }
} // namespace PinPthread
