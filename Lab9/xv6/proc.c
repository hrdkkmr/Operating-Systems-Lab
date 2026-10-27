#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct {
  struct spinlock lock;
  uint pa;
  int refs;
} shmpage;

static struct proc *initproc;

int nextpid = 1;
static uint sched_seq;   // Lab 7 Q4: dispatch counter, see scheduler()
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&shmpage.lock, "shm");
  shmpage.pa = 0;
  shmpage.refs = 0;
  seminit();
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  // Lab 7 fields are reset here, under ptable.lock, so that a slot
  // recycled from an old thread or an aged process starts clean.
  p->is_thread = 0;
  p->pgdir = 0;     // never let a stale pointer match a live address space
  p->killed = 0;
  p->shm_pa = 0;
  p->priority = DEFAULT_PRIORITY;
  p->wait_ticks = 0;
  p->last_sched = sched_seq;   // joins the back of its priority level
  p->pgfaults = 0;             // Lab 9 Q2: each new process/thread counts from 0

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Allocate/map the assignment shared-memory page for this process.
// The same physical page is mapped into all processes created by fork().
void*
shm_get(void)
{
  struct proc *p = myproc();
  char *mem;

  // A thread shares its leader's pgdir; mapping SHMVA a second time into
  // the same page table would remap the PTE and break the refcount.
  // Threads already see the leader's mapping (call shm_get before clone).
  if(p->is_thread)
    return 0;

  acquire(&shmpage.lock);
  if(p->shm_pa != 0){
    release(&shmpage.lock);
    return (void*)SHMVA;
  }

  if(shmpage.pa == 0){
    mem = kalloc();
    if(mem == 0){
      release(&shmpage.lock);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    shmpage.pa = V2P(mem);
  }

  if(shm_map(p->pgdir, shmpage.pa) < 0){
    if(shmpage.refs == 0){
      kfree(P2V(shmpage.pa));
      shmpage.pa = 0;
    }
    release(&shmpage.lock);
    return 0;
  }

  p->shm_pa = shmpage.pa;
  shmpage.refs++;
  release(&shmpage.lock);
  return (void*)SHMVA;
}

void
shm_release(struct proc *p)
{
  acquire(&shmpage.lock);
  if(p->shm_pa != 0){
    shm_unmap(p->pgdir);
    p->shm_pa = 0;
    if(shmpage.refs > 0)
      shmpage.refs--;
    if(shmpage.refs == 0 && shmpage.pa != 0){
      kfree(P2V(shmpage.pa));
      shmpage.pa = 0;
    }
  }
  release(&shmpage.lock);
}

// Grow current process's memory by n bytes.
// If oldsz is non-zero, the size before growing is stored there.
// Return 0 on success, -1 on failure.
//
// Threads share one pgdir and therefore one size.  The resize and the
// read of the old size are done under ptable.lock so that two threads
// calling sbrk() concurrently cannot both extend from the same old size,
// and the new size is published to every proc sharing the pgdir so that
// argument checks (fetchint/argptr) and fork's copyuvm see the real size.
// Limitation: xv6 has no TLB shootdown, so shrinking memory while another
// thread runs on a different CPU may leave a stale TLB entry there.
int
growproc(int n, uint *oldsz)
{
  uint sz;
  struct proc *curproc = myproc();
  struct proc *p;

  acquire(&ptable.lock);
  sz = curproc->sz;
  if(oldsz)
    *oldsz = sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
      release(&ptable.lock);
      return -1;
    }
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0){
      release(&ptable.lock);
      return -1;
    }
  }
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state != UNUSED && p->pgdir == curproc->pgdir)
      p->sz = sz;
  release(&ptable.lock);
  switchuvm(curproc);
  return 0;
}

// Lab 9 Q2: lazy growth for sbrk(n), n > 0.  Only the size grows; no
// memory is allocated.  The pages are allocated one at a time by the
// page-fault handler (trap.c) when they are first touched.  As in
// growproc(), every thread sharing the address space gets the new size.
// The heap may not reach SHMVA, the fixed shared page just below KERNBASE.
int
growproc_lazy(int n, uint *oldsz)
{
  uint sz;
  struct proc *curproc = myproc();
  struct proc *p;

  acquire(&ptable.lock);
  sz = curproc->sz;
  *oldsz = sz;
  if(sz + n < sz || sz + n > SHMVA){
    release(&ptable.lock);
    return -1;
  }
  sz += n;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state != UNUSED && p->pgdir == curproc->pgdir)
      p->sz = sz;
  release(&ptable.lock);
  return 0;
}

// Release the per-thread resources of a ZOMBIE thread.  The shared pgdir
// is NOT freed: it belongs to the group leader.  Caller holds ptable.lock.
static void
freethread(struct proc *p)
{
  kfree(p->kstack);
  p->kstack = 0;
  p->pgdir = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->is_thread = 0;
  p->state = UNUSED;
}

// Called by a group leader (a non-thread proc) before its address space
// is destroyed, i.e. from exit() and from exec() just before it commits
// to the new image.  Kills every other thread sharing the leader's pgdir,
// adopts them so that their exit() wakes the leader, and sleeps until all
// of them have exited and been reaped here.  Killed threads leave the
// kernel at the next return to user mode or out of their interruptible
// sleep (pipe, sleep, wait/join, sem_wait all check p->killed).
void
reap_threads(struct proc *leader)
{
  struct proc *p;
  int live;

  if(leader->is_thread)
    panic("reap_threads");

  acquire(&ptable.lock);
  for(;;){
    live = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p == leader || !p->is_thread || p->state == UNUSED ||
         p->pgdir != leader->pgdir)
        continue;
      if(p->state == ZOMBIE){
        freethread(p);
        continue;
      }
      live++;
      p->parent = leader;
      p->killed = 1;
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
    }
    if(live == 0)
      break;
    sleep(leader, &ptable.lock);
  }
  release(&ptable.lock);
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  if(curproc->shm_pa != 0){
    acquire(&shmpage.lock);
    if(shm_map(np->pgdir, curproc->shm_pa) < 0){
      release(&shmpage.lock);
      freevm(np->pgdir);
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    np->shm_pa = curproc->shm_pa;
    shmpage.refs++;
    release(&shmpage.lock);
  }

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p, *leader;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // A group leader must not become a ZOMBIE (whose pgdir wait() frees)
  // while threads still execute in that address space.
  if(!curproc->is_thread)
    reap_threads(curproc);

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait() (or join() if we are a thread).
  wakeup1(curproc->parent);

  // An exiting thread hands the threads it created to the group leader,
  // which is guaranteed to be alive (see reap_threads); they can still be
  // collected with join() by the leader.  Ordinary children go to init.
  leader = 0;
  if(curproc->is_thread){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      if(p->state != UNUSED && !p->is_thread && p->pgdir == curproc->pgdir){
        leader = p;
        break;
      }
  }

  // Pass abandoned children to init (or to the leader, see above).
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      if(p->is_thread && leader){
        p->parent = leader;
        if(p->state == ZOMBIE)
          wakeup1(leader);
      } else {
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
// Threads are deliberately skipped: they are collected only by join(),
// and a thread child does not count as a child for the -1 check.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc || p->is_thread)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.  It is a group leader whose exit() already reaped
        // all of its threads, so nothing else uses p->pgdir any more.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        shm_release(p);
        freevm(p->pgdir);
        p->pgdir = 0;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
//
// Lab 7 Q4: priority scheduling with round-robin tie-breaking.
//  - Strict priority: only RUNNABLE processes with the numerically lowest
//    priority value present are eligible.
//  - Round robin among equals: every dispatch takes a ticket from the
//    global counter sched_seq and stores it in p->last_sched.  Among the
//    eligible processes the one with the smallest last_sched, i.e. the one
//    that has gone longest without the CPU, is chosen.  This is exactly a
//    FIFO queue per priority level: a process that has just run goes to
//    the back.  Unlike a "next slot after the last pick" cursor, it is not
//    disturbed when processes of other priorities are dispatched in
//    between (e.g. a high-priority process that wakes up periodically).
//    A process that slept for a long time keeps its old ticket and so is
//    served first among its equals when it wakes.  New processes start
//    with the current counter value (allocproc), i.e. at the back.
//  - Preemption: trap() calls yield() on every timer tick, so a running
//    process is re-evaluated against all RUNNABLE ones at least every tick.
//  - Aging (see age_runnable): prevents starvation of low priorities.
// sched_seq and last_sched are protected by ptable.lock.  The comparison
// uses signed differences so that wrap-around of the 32-bit counter (after
// 2^32 dispatches) does not break the order.
void
scheduler(void)
{
  struct proc *p, *best;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);
    best = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(best == 0 || p->priority < best->priority ||
         (p->priority == best->priority &&
          (int)(p->last_sched - best->last_sched) < 0))
        best = p;
    }

    if(best != 0){
      p = best;
      p->last_sched = ++sched_seq;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->wait_ticks = 0;     // it got the CPU: its waiting period ends

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

    // Nothing was runnable: sleep until the next interrupt (at the latest
    // the next timer tick, i.e. 10 ms) instead of re-scanning at once.
    // The original busy loop re-acquired ptable.lock continuously; every
    // acquire() reads the LAPIC ID through mycpu(), and under QEMU these
    // emulated MMIO accesses from an idle CPU slowed the busy CPU's disk
    // I/O so much that usertests appeared hung with CPUS >= 2.
    if(best == 0)
      sti_hlt();
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s prio=%d%s", p->pid, state, p->name, p->priority,
            p->is_thread ? " thread" : "");
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//PAGEBREAK!
// Lab 7 Q1: create a thread that shares the caller's address space.
//
// stack must be the page-aligned base of one page (PGSIZE bytes) of the
// caller's memory; it stays owned by the caller and must remain valid
// until the thread has been joined.  The new thread starts in user mode
// at fcn(arg) on that stack.  fcn must finish by calling exit(): its fake
// return address 0xffffffff faults if fcn returns, which kills just that
// thread.  Returns the new thread's pid, or -1 on error.
int
clone(void (*fcn)(void*), void *arg, void *stack)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();
  uint top, sp, ustack[2];

  if((np = allocproc()) == 0)
    return -1;

  // Registers of the caller, with the user stack and entry point replaced.
  // The i386 SysV ABI expects (esp + 4) to be 16-byte aligned at function
  // entry, so the frame is placed at top - 20: [fake return pc][arg][pad].
  *np->tf = *curproc->tf;
  top = (uint)stack + PGSIZE;
  sp = top - 20;
  ustack[0] = 0xffffffff;      // fake return PC
  ustack[1] = (uint)arg;
  np->tf->eip = (uint)fcn;
  np->tf->esp = sp;
  np->tf->eax = 0;

  acquire(&ptable.lock);
  // Validate against the current shared size while holding ptable.lock,
  // which growproc() holds when another thread resizes the address space.
  if(curproc->killed || stack == 0 || (uint)stack % PGSIZE != 0 ||
     top < (uint)stack || top > curproc->sz || (uint)fcn >= curproc->sz ||
     copyout(curproc->pgdir, sp, ustack, sizeof(ustack)) < 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }
  // Publish the thread atomically with respect to reap_threads(): from
  // here on it is a member of the leader's group.  It stays EMBRYO (not
  // schedulable) until its file table and cwd are set up below.
  np->pgdir = curproc->pgdir;
  np->sz = curproc->sz;
  np->is_thread = 1;
  np->parent = curproc;
  release(&ptable.lock);

  // xv6 has per-proc file tables, so the thread gets duplicated
  // references to the caller's open files and cwd (same open-file
  // objects and offsets, released when the thread exits).
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// Lab 7 Q1: wait for a thread created by this proc (or handed to it, see
// exit()) to exit, free its kernel stack and proc slot, and return its
// pid.  The shared address space and the user stack are left alone.
// Returns -1 if the caller has no thread children or has been killed.
int
join(void)
{
  struct proc *p;
  int havethreads, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    havethreads = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc || !p->is_thread)
        continue;
      havethreads = 1;
      if(p->state == ZOMBIE){
        pid = p->pid;
        freethread(p);
        release(&ptable.lock);
        return pid;
      }
    }
    if(!havethreads || curproc->killed){
      release(&ptable.lock);
      return -1;
    }
    // A thread's exit() calls wakeup1(parent).
    sleep(curproc, &ptable.lock);  //DOC: join-sleep
  }
}

// Lab 7 Q4: aging.  Called once per timer tick (by CPU 0, which owns the
// global tick counter; see trap.c).  Every process that is RUNNABLE but
// not running accumulates one tick of waiting; RUNNING, SLEEPING, ZOMBIE,
// EMBRYO and UNUSED procs do not.  When a process has waited more than
// AGING_THRESHOLD ticks since it last got a CPU, its priority improves by
// one level (never past MIN_PRIORITY) and its counter restarts.  The
// scheduler resets wait_ticks whenever it dispatches the process.
void
age_runnable(void)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE)
      continue;
    if(++p->wait_ticks > AGING_THRESHOLD){
      if(p->priority > MIN_PRIORITY)
        p->priority--;
      p->wait_ticks = 0;
    }
  }
  release(&ptable.lock);
}

// Lab 7 Q4: set the priority of a live (not UNUSED/ZOMBIE) process.
// Returns 0, or -1 if priority is out of [1,20] or no live
// process has that pid.  The aging counter restarts.
int
set_priority(int pid, int priority)
{
  struct proc *p;

  if(priority < MIN_PRIORITY || priority > MAX_PRIORITY || pid <= 0)
    return -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != UNUSED && p->state != ZOMBIE){
      p->priority = priority;
      p->wait_ticks = 0;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Lab 7 Q4: priority of a live process, or -1 if no live process has pid.
int
get_priority(int pid)
{
  struct proc *p;
  int pr;

  if(pid <= 0)
    return -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != UNUSED && p->state != ZOMBIE){
      pr = p->priority;
      release(&ptable.lock);
      return pr;
    }
  }
  release(&ptable.lock);
  return -1;
}
