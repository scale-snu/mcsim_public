#include "PthreadMutexAttr.h"

using namespace PinPthread;

/* --------------------------------------------------------------------------- */
/* PTHREAD_MUTEXATTR_DEFAULT:                                                  */
/* return a default pthread mutex attribute object                             */
/* --------------------------------------------------------------------------- */

pthread_mutexattr_t PthreadMutexAttr::PTHREAD_MUTEXATTR_DEFAULT() 
{
  pthread_mutexattr_t attr;
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_FAST_NP);
  return attr;
}

/* --------------------------------------------------------------------------- */
/* pthread_mutexattr_destroy:                                                  */
/* destroy a mutex attribute object - cannot be used until it is reinitialized */
/* --------------------------------------------------------------------------- */

int PthreadMutexAttr::_pthread_mutexattr_destroy(pthread_mutexattr_t* attr) 
{
  /* do nothing, as in the LinuxThreads implementation */
  return 0;
}

/* --------------------------------------------------------------------------- */
/* pthread_mutexattr_getkind:                                                  */
/* return the type of the given mutex                                          */
/* --------------------------------------------------------------------------- */

int PthreadMutexAttr::_pthread_mutexattr_getkind(const pthread_mutexattr_t* attr, int* kind) 
{
  ASSERTX(attr != NULL);
  ASSERTX(kind != NULL);
  if (*kind == PTHREAD_MUTEX_DEFAULT) 
  {
    *kind = PTHREAD_MUTEX_FAST_NP;
  }
  ASSERTX((*kind == PTHREAD_MUTEX_FAST_NP) ||
      (*kind == PTHREAD_MUTEX_RECURSIVE_NP) ||
      (*kind == PTHREAD_MUTEX_ERRORCHECK_NP) ||
      (*kind == PTHREAD_MUTEX_DEFAULT));
  pthread_mutexattr_gettype(attr, kind);
  return 0;
}

/* --------------------------------------------------------------------------- */
/* pthread_mutexattr_init:                                                     */
/* initialize the mutex attribute object with the default value                */
/* --------------------------------------------------------------------------- */

int PthreadMutexAttr::_pthread_mutexattr_init(pthread_mutexattr_t* attr) 
{
  ASSERTX(attr != NULL);
  *attr = PTHREAD_MUTEXATTR_DEFAULT();
  return 0;
}

/* --------------------------------------------------------------------------- */
/* pthread_mutexattr_setkind:                                                  */
/* change the type of the given mutex                                          */
/* --------------------------------------------------------------------------- */

int PthreadMutexAttr::_pthread_mutexattr_setkind(pthread_mutexattr_t* attr, int kind) 
{
  ASSERTX(attr != NULL);
  if (kind == PTHREAD_MUTEX_DEFAULT) 
  {
    pthread_mutexattr_settype(attr, PTHREAD_MUTEX_FAST_NP);
  }
  else 
  {
    pthread_mutexattr_settype(attr, kind);
  }
  if ((kind == PTHREAD_MUTEX_FAST_NP) ||
      (kind == PTHREAD_MUTEX_RECURSIVE_NP) ||
      (kind == PTHREAD_MUTEX_ERRORCHECK_NP) ||
      (kind == PTHREAD_MUTEX_DEFAULT))
  {
    return 0;
  }
  else 
  {
    return EINVAL;
  }
}
