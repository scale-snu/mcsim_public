/* --------------------------------------------------------------------------- */
/* PthreadBarrier:                                                             */
/* manipulates pthread barrier objects                                         */
/* manages the synchronization of threads using barriers                       */
/* --------------------------------------------------------------------------- */

#ifndef PTHREAD_BARRIER_H
#define PTHREAD_BARRIER_H

#include "PthreadAttr.h"

namespace PinPthread 
{

  class PthreadBarrier
  {
    public:
      static int pthread_barrier_init(pthread_barrier_t*, const pthread_barrierattr_t *, unsigned int num);
      static int pthread_barrier_destroy(pthread_barrier_t*);
  };

  typedef std::vector<pthread_t> pthread_barrierwaiterfifo_t;

  class PthreadBarrierWaiters
  {
    public:
      PthreadBarrierWaiters(unsigned int num);
      ~PthreadBarrierWaiters();
      void PushWaiter(pthread_t);
      void PopWaiter(pthread_t*);
      bool IsEmpty();
    public:
      unsigned int num_participants;
      pthread_barrierwaiterfifo_t waiters; // fifo of waiting threads for one barrier
  };

  typedef std::map<pthread_barrier_t*, PthreadBarrierWaiters *> pthreadbarrier_queue_t;

  class PthreadBarrierManager 
  {
    public:
      unsigned int NumParticipants(pthread_barrier_t *);
      bool HasMoreWaiters(pthread_barrier_t *);
      bool AddWaiter(pthread_barrier_t *, pthread_t);  // true if ready to remove
      void RemoveWaiter(pthread_barrier_t *, pthread_t *);
      void InitWaiters(pthread_barrier_t *, unsigned int num);
      void DestroyWaiters(pthread_barrier_t *);
    private:
      pthreadbarrier_queue_t barriers; // list of waiting threads indexed by barrier
  };

} // namespace PinPthread

#endif  // #ifndef PTHREAD_BARRIER_H
