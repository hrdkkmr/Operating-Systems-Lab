#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "meminfo.h"

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
  // Lab 8 Q4: lazy allocation.  For n > 0 only the size grows
  // (sbrklazy, no growproc/allocuvm); frames are allocated by the page
  // fault handler when a page is first touched.  For n <= 0 growproc()
  // shrinks the address space and frees only pages really mapped.
  // Both report the old size read under the same lock as the resize, so
  // concurrent sbrk() calls by threads that share the address space each
  // get a distinct region.
  if(n > 0){
    if(sbrklazy(n, &addr) < 0)
      return -1;
  } else if(growproc(n, &addr) < 0)
    return -1;
  return addr;
}

// Lab 8 Q1: int va2pa(int va).  va is taken as a full 32-bit word
// (argint into an int, then used as uint), so addresses >= 0x80000000
// are not mangled.  The result is the physical address including the
// page offset, or -1.  Physical memory ends at PHYSTOP (224 MB), so a
// valid result is always positive.
int
sys_va2pa(void)
{
  int va;

  if(argint(0, &va) < 0)
    return -1;
  return va2pa(myproc()->pgdir, (uint)va);
}

// Lab 8 Q2: int getmeminfo(struct meminfo *mi).
int
sys_getmeminfo(void)
{
  struct meminfo *umi;
  struct meminfo mi;
  struct proc *p = myproc();

  // Validate (and, if lazy/COW, prepare) the destination first, so that
  // any page this allocates is already reflected in the numbers below.
  if(argptrw(0, (char**)&umi, sizeof(mi)) < 0)
    return -1;
  kmemstat(&mi.total_pages, &mi.free_pages);
  mi.proc_pages = p->sz / PGSIZE;
  uvmstat(p->pgdir, &mi.pt_pages, &mi.pt_user_pages,
          &mi.resident_pages, &mi.shared_pages);
  if(copyout(p->pgdir, (uint)umi, &mi, sizeof(mi)) < 0)
    return -1;
  return 0;
}

// Lab 8 Q3: int vmprint(void).
int
sys_vmprint(void)
{
  struct proc *p = myproc();

  vmprint(p->pgdir, p->sz);
  return 0;
}

// Lab 8 Q6 (inferred extension): int mprotect(void *addr, int len) and
// int munprotect(void *addr, int len); len is a number of pages.
int
sys_mprotect(void)
{
  int addr, len;

  if(argint(0, &addr) < 0 || argint(1, &len) < 0)
    return -1;
  return mprotect((uint)addr, len, 1);
}

int
sys_munprotect(void)
{
  int addr, len;

  if(argint(0, &addr) < 0 || argint(1, &len) < 0)
    return -1;
  return mprotect((uint)addr, len, 0);
}

// Lab 8 Q7 (inferred extension): void *shm_open(int key) and
// int shm_close(int key).
int
sys_shm_open(void)
{
  int key;

  if(argint(0, &key) < 0)
    return -1;
  return shm_open(key);
}

int
sys_shm_close(void)
{
  int key;

  if(argint(0, &key) < 0)
    return -1;
  return shm_close(key);
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
