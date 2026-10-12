#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  uint addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  // growproc reports the old size it extended from, read under the same
  // lock as the resize, so concurrent sbrk() calls by threads that share
  // the address space each get a distinct region.
  if(growproc(n, &addr) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Lab 5: map the shared synchronization page (also used by Lab 7 Q3).
int
sys_shm_get(void)
{
  return (int)shm_get();
}

// Lab 7 Q1.  The three arguments are fetched as raw words: arg is opaque
// to the kernel (it need not be a valid pointer), and clone() itself
// validates fcn and stack against the shared address space.
int
sys_clone(void)
{
  int fcn, arg, stack;

  if(argint(0, &fcn) < 0 || argint(1, &arg) < 0 || argint(2, &stack) < 0)
    return -1;
  return clone((void(*)(void*))fcn, (void*)arg, (void*)stack);
}

int
sys_join(void)
{
  return join();
}

// Lab 7 Q2 (semaphore.c).
int
sys_sem_init(void)
{
  int id, value;

  if(argint(0, &id) < 0 || argint(1, &value) < 0)
    return -1;
  return sem_init(id, value);
}

int
sys_sem_wait(void)
{
  int id;

  if(argint(0, &id) < 0)
    return -1;
  return sem_wait(id);
}

int
sys_sem_signal(void)
{
  int id;

  if(argint(0, &id) < 0)
    return -1;
  return sem_signal(id);
}

// Lab 7 Q4.
int
sys_set_priority(void)
{
  int pid, priority;

  if(argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;
  return set_priority(pid, priority);
}

int
sys_get_priority(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return get_priority(pid);
}
