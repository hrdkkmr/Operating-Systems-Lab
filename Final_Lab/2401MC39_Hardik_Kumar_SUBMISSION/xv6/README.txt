Name: Hardik Kumar
Roll Number: 2401MC39
Set Number: Set 2
Course: MA3105 - Operating System Lab Examination
Date: 17 November 2026

------------------------------------------------------------------------------------------------------------------------------
Question 1: Page Protection with mprotect() and munprotect()


- In vm.c:
  1. Implemented static helper change_protect(void *addr, int len, int writable).
     It validates that addr is page-aligned (addr % PGSIZE == 0), len > 0, and that the
     requested range does not overflow or exceed the process address space (myproc()->sz).
     It walks the page directory (using static walkpgdir) and ensures every page in the
     range is present (PTE_P). It then clears PTE_W (if writable == 0 for mprotect) or
     sets PTE_W (if writable == 1 for munprotect). Finally, it invalidates TLB entries by
     reloading %cr3 using lcr3(V2P(myproc()->pgdir)).
  2. Implemented mprotect(addr, len) and munprotect(addr, len) calling change_protect.

- In defs.h:
  Declared prototypes for mprotect(void*, int) and munprotect(void*, int).

- In syscall.h, syscall.c, sysproc.c, user.h, usys.S:
  Registered system call numbers SYS_mprotect (22) and SYS_munprotect (23),
  added dispatch entries and sys_mprotect() / sys_munprotect() argument handlers,
  and defined user-space stubs and prototypes.

- In Makefile:
  Added _prottest to UPROGS and prottest.c to EXTRA.

- In prottest.c:
  Implemented tests to verify:
  (i) Allocation via sbrk(4096), storing 'A', calling mprotect, and verifying read succeeds.
  (ii) Forking a child that attempts to write to the protected page; the kernel detects
       trap 14 (page fault) and terminates the child, while the parent continues and
       confirms the value remains 'A'.
  (iii) Calling munprotect and verifying writing 'C' succeeds.
  (iv) Validating that mprotect returns -1 on unaligned address, len = 0, len = -1,
       and ranges exceeding process address space.

----------------------------------------------------------------------------------------------------------------------
Question 2: Priority-Based CPU Scheduler


- In proc.h:
  Added 'int priority;' (range 1 highest to 20 lowest) to struct proc.

- In proc.c:
  In allocproc(), initialized p->priority = 10 so every newly allocated process,
  including child processes created by fork(), starts with default priority 10.

- In proc.c & defs.h:
  Implemented int set_priority(int pid, int priority). It validates priority is within
  1 to 20 (returning -1 otherwise), acquires ptable.lock, searches the process table for
  a process with matching pid, saves the old priority, updates p->priority, releases
  ptable.lock, and returns the old priority. Returns -1 if no matching process is found.

- In proc.c (scheduler):
  Replaced the round-robin scan with priority scheduling. In each iteration while
  holding ptable.lock, the scheduler scans ptable.proc to find the RUNNABLE process with
  the smallest priority number. In case of ties, the strict inequality keeps the first
  candidate found in table order. If a runnable candidate is found, context switch is
  performed via swtch().

- In syscall.h, syscall.c, sysproc.c, user.h, usys.S:
  Registered SYS_set_priority (24) and wired up system call dispatcher and user wrappers.

- In Makefile:
  Added _schedtest to UPROGS and schedtest.c to EXTRA.

- In schedtest.c:
  Parent sets its priority to 1 so it cannot be preempted during child creation. It forks
  three children with priorities 5, 10, and 15 using set_priority(). Each child executes
  a CPU-bound arithmetic loop spanning multiple timer ticks and prints its priority and
  completion. Under single CPU (CPUS=1), children execute and finish strictly in priority
  order (5, then 10, then 15). Parent waits for all three children and prints 'Done'.

  ----------------------------------------------------------------------------------------------------------------------------
