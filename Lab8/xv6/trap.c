#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
      // Lab 7 Q4: one aging step per global tick.  Only CPU 0 counts
      // ticks, so aging runs exactly once per tick however many CPUs
      // there are.  Called after releasing tickslock (takes ptable.lock).
      age_runnable();
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  // Lab 8 Q4/Q5/Q6: page faults.  rcr2() holds the faulting address and
  // tf->err says whether the page was present (FEC_PR) and whether the
  // access was a write (FEC_WR).  pagefault() -> uvmfault() (vm.c):
  //  - absent page in [heap_start, sz): demand allocation of a zeroed
  //    frame (kalloc, memset, mappages with PTE_W|PTE_U);
  //  - write to a present PTE_COW page: copy-on-write;
  //  - anything else (address >= sz, below the heap, guard page, write to
  //    an mprotect'ed page, kernel address, kalloc failure) fails.
  // A failed user-mode fault kills only the faulting process.  A fault
  // in kernel mode on a user address (the kernel copying to/from user
  // memory) is resolved the same way; syscall.c normally resolves such
  // pages before the kernel touches them, so this is only a safety net,
  // and an unresolvable kernel fault is still a kernel bug (panic).
  case T_PGFLT: {
    uint va = rcr2();
    int r = FAULT_RANGE;

    if(myproc() != 0 && va < KERNBASE)
      r = pagefault(va, (tf->err & FEC_WR) != 0);
    if(r == 0)
      break;
    if(myproc() != 0 && (tf->cs&3) == DPL_USER){
      cprintf("pid %d %s: page fault: %s %s va 0x%x eip 0x%x (sz 0x%x): "
              "%s -- kill proc\n",
              myproc()->pid, myproc()->name,
              (tf->err & FEC_WR) ? "write" : "read",
              (tf->err & FEC_PR) ? "protection violation at" : "of absent page",
              va, tf->eip, myproc()->sz,
              va >= KERNBASE ? "kernel address" : faultreason(r));
      myproc()->killed = 1;
      break;
    }
    cprintf("kernel page fault on va 0x%x eip 0x%x: %s\n",
            va, tf->eip, faultreason(r));
    panic("trap: kernel page fault");
  }

  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}


