// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  uint shm_pa;                  // Physical address of shared page, if mapped
  // Lab 7 Q1: a thread (is_thread == 1) shares pgdir with its group
  // leader, the unique non-thread proc using the same pgdir.  The leader
  // owns the address space: it is freed only when the leader is reaped by
  // wait(), and the leader kills and reaps every thread of its group in
  // exit()/exec() before the pgdir can go away.  Threads never free it.
  int is_thread;               // 1 if created by clone(), 0 if normal proc
  // Lab 7 Q4: scheduling priority and aging state (see scheduler()).
  int priority;                // 1 (highest) to 20 (lowest), default 10
  int wait_ticks;              // Consecutive ticks RUNNABLE but not scheduled
  uint last_sched;             // dispatch sequence number of its last run
  int pgfaults;                // Lab 9 Q2: lazy page faults handled for this proc
};

#define MIN_PRIORITY      1    // numerically smallest = most important
#define MAX_PRIORITY     20
#define DEFAULT_PRIORITY 10
#define AGING_THRESHOLD  30    // age after waiting more than this many ticks

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
