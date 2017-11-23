#include "PthreadBarrier.h"

using namespace PinPthread;

unsigned int PthreadBarrierManager::NumParticipants(pthread_barrier_t* barrier)
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  ASSERTX(barrierptr != barriers.end());
  return barrierptr->second->num_participants;
}

/* --------------------------------------------------------------------------- */
/* HasMoreWaiters:                                                             */
/* determine whether there are threads waiting on the given barrier variable   */
/* --------------------------------------------------------------------------- */

bool PthreadBarrierManager::HasMoreWaiters(pthread_barrier_t* barrier) 
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  ASSERTX(barrierptr != barriers.end());
  return (!barrierptr->second->waiters.empty());
}

/* --------------------------------------------------------------------------- */
/* AddWaiter:                                                                  */
/* force the given thread to wait on the given barrier variable                */
/* --------------------------------------------------------------------------- */

bool PthreadBarrierManager::AddWaiter(pthread_barrier_t* barrier, pthread_t thread)
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  ASSERTX(barrierptr != barriers.end());
  barrierptr->second->PushWaiter(thread);
  return (barrierptr->second->waiters.size() == barrierptr->second->num_participants);
}

/* --------------------------------------------------------------------------- */
/* RemoveWaiter:                                                               */
/* release the next thread waiting on the given barrier variable               */
/* --------------------------------------------------------------------------- */

void PthreadBarrierManager::RemoveWaiter(pthread_barrier_t* barrier, pthread_t* thread)
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  ASSERTX(barrierptr != barriers.end());
  barrierptr->second->PopWaiter(thread);
}

void PthreadBarrierManager::InitWaiters(pthread_barrier_t* barrier, unsigned int num)
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  if (barrierptr == barriers.end())
  {
    barriers[barrier] = new PthreadBarrierWaiters(num);
  }
  else
  {
    barrierptr->second->num_participants = num;
    barrierptr->second->waiters.clear();
  }
}

void PthreadBarrierManager::DestroyWaiters(pthread_barrier_t * barrier)
{
  pthreadbarrier_queue_t::iterator barrierptr = barriers.find(barrier);
  ASSERTX(barrierptr != barriers.end());
  barriers.erase(barrierptr);
}


/* --------------------------------------------------------------------------- */
/* PthreadWaiters Functions:                                                   */
/* --------------------------------------------------------------------------- */

PthreadBarrierWaiters::PthreadBarrierWaiters(unsigned int num) 
  :num_participants(num), waiters()
{
}

PthreadBarrierWaiters::~PthreadBarrierWaiters() {}

void PthreadBarrierWaiters::PushWaiter(pthread_t thread) 
{
  waiters.push_back(thread);
}

void PthreadBarrierWaiters::PopWaiter(pthread_t* thread) 
{
  ASSERTX(!waiters.empty());
  *thread = waiters.front();
  waiters.erase(waiters.begin());
}

bool PthreadBarrierWaiters::IsEmpty() 
{
  return (waiters.empty());
}

