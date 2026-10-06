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
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
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

int sys_clone(void) {
  void *fcn, *arg, *stack;
  if(argptr(0, (void*)&fcn, sizeof(void*)) < 0 ||
     argptr(1, (void*)&arg, sizeof(void*)) < 0 ||
     argptr(2, (void*)&stack, sizeof(void*)) < 0)
    return -1;
  return clone((void(*)(void*))fcn, arg, stack);
}

int sys_join(void) {
  return join();
}

int sys_sem_init(void) {
  int id, val;
  if(argint(0, &id) < 0 || argint(1, &val) < 0) return -1;
  return sem_init(id, val);
}

int sys_sem_wait(void) {
  int id;
  if(argint(0, &id) < 0) return -1;
  return sem_wait(id);
}

int sys_sem_signal(void) {
  int id;
  if(argint(0, &id) < 0) return -1;
  return sem_signal(id);
}

int sys_set_priority(void) {
  int pid, pr;
  if(argint(0, &pid) < 0 || argint(1, &pr) < 0) return -1;
  return set_priority(pid, pr);
}

int sys_get_priority(void) {
  int pid;
  if(argint(0, &pid) < 0) return -1;
  return get_priority(pid);
}
