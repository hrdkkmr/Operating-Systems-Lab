// Lab 7 Q2: kernel counting semaphores.
//
// A fixed table of NSEM semaphores, named by id 0..NSEM-1 and shared by
// every process and thread in the system.
//
// Invariant: value >= 0 is the number of units available.
//   sem_wait:   while value == 0, sleep; then value--.
//   sem_signal: value++, then wake the sleepers so one can take the unit.
// This is equivalent to the textbook signed-count formulation (decrement
// first and block if the result is negative, where -value is the number
// of waiters): here the number of blocked processes is tracked explicitly
// in `waiters` instead of being encoded in a negative value, which keeps
// the "units available" count meaningful and makes re-checking after a
// wakeup straightforward.
//
// Locking: each semaphore has its own spinlock protecting value, waiters
// and initialized.  sem_wait() tests the value and calls sleep() while
// holding that lock; sleep() acquires ptable.lock before releasing it, and
// sem_signal() must hold the same lock to change value before wakeup()
// takes ptable.lock.  So a signal cannot slip in between the test and the
// sleep, and no wakeup is lost.  Lock order is always sem lock -> ptable
// lock, the same order sleep()/wakeup() use for every other xv6 lock, so
// no deadlock is introduced, and nothing ever sleeps holding ptable.lock
// on behalf of a semaphore.  Blocked processes sleep (no busy waiting).
//
// wakeup() wakes every sleeper on the channel (xv6 has no wake-one).
// Each re-checks value under the lock, and those that find it 0 sleep
// again, so correctness does not depend on how many are woken.
//
// Return values (all calls): 0 on success, -1 on error:
//   sem_init   -1 if sem_id is out of range, value < 0, or processes are
//              currently blocked on the semaphore (re-initializing it then
//              would strand or wrongly release them).  Re-initializing an
//              idle semaphore is allowed, so test programs can be re-run.
//   sem_wait   -1 if sem_id is out of range or never initialized, or if
//              the caller was killed while blocked (so that kill() and
//              thread-group teardown can always interrupt a waiter).
//   sem_signal -1 if sem_id is out of range or never initialized, or if
//              the count would overflow.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

#define NSEM 10
#define SEM_VALUE_MAX 0x7fffffff

struct semaphore {
  struct spinlock lock;
  int value;          // units available, always >= 0
  int waiters;        // processes currently blocked in sem_wait
  int initialized;    // set by the first successful sem_init
};

static struct semaphore sems[NSEM];

void
seminit(void)
{
  int i;

  for(i = 0; i < NSEM; i++){
    initlock(&sems[i].lock, "semaphore");
    sems[i].value = 0;
    sems[i].waiters = 0;
    sems[i].initialized = 0;
  }
}

int
sem_init(int sem_id, int value)
{
  struct semaphore *s;

  if(sem_id < 0 || sem_id >= NSEM || value < 0)
    return -1;
  s = &sems[sem_id];

  acquire(&s->lock);
  if(s->waiters > 0){
    release(&s->lock);
    return -1;
  }
  s->value = value;
  s->initialized = 1;
  release(&s->lock);
  return 0;
}

int
sem_wait(int sem_id)
{
  struct semaphore *s;

  if(sem_id < 0 || sem_id >= NSEM)
    return -1;
  s = &sems[sem_id];

  acquire(&s->lock);
  if(!s->initialized){
    release(&s->lock);
    return -1;
  }
  while(s->value == 0){
    if(myproc()->killed){
      release(&s->lock);
      return -1;
    }
    s->waiters++;
    sleep(s, &s->lock);
    s->waiters--;
  }
  s->value--;
  release(&s->lock);
  return 0;
}

int
sem_signal(int sem_id)
{
  struct semaphore *s;

  if(sem_id < 0 || sem_id >= NSEM)
    return -1;
  s = &sems[sem_id];

  acquire(&s->lock);
  if(!s->initialized || s->value == SEM_VALUE_MAX){
    release(&s->lock);
    return -1;
  }
  s->value++;
  if(s->waiters > 0)
    wakeup(s);
  release(&s->lock);
  return 0;
}
