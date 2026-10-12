# OS Lab Assignment 7 — Threads, Semaphores, Deadlock and Priority Scheduling in xv6

**Platform:** MIT xv6 for x86 (32-bit, rev-11 layout), extended by the Lab 5
shared page and semaphores. Built with gcc 15.2 and run on QEMU 10.2 (TCG)
with 1, 2 and 4 CPUs.
**Objective:** add kernel-supported threads (Q1) and counting semaphores (Q2);
simulate deadlock detection, the Banker's algorithm and a real circular wait
(Q3); and replace round-robin scheduling with priority scheduling with aging
(Q4). Measurements come from the logs in `output_logs/`, except those marked
*exploratory*, which come from development runs before the final build.

The starting commit did not build (`semaphore_init` undefined; `allocproc`
wrote through an uninitialized pointer). Two environment problems were found
and fixed during testing. First, xv6 had been running on **one** CPU, because
SeaBIOS's MP table lists one CPU per socket; the Makefile now uses
`-smp N,sockets=N`. Second, once SMP worked, the idle CPU's busy scheduler
loop slowed the busy CPU's disk I/O so much that `usertests` seemed hung;
idle CPUs now `hlt`.

## 1. Q1 — Threads

**Creation.** `clone(fcn, arg, stack)` allocates a proc with `allocproc()`,
which gives it its own kernel stack, trap frame and context, starting in
`forkret` → `trapret`. It copies the caller's trap frame, then sets
`eip = fcn` and `esp = stack + 4096 − 20`. A two-word frame is written with
`copyout`: a fake return PC `0xffffffff`, then `arg`. This is exactly how
`fcn(arg)` would see its own call under cdecl, and it keeps the i386 rule
that `esp + 4` is 16-byte aligned on entry. The stack must be a non-null,
page-aligned page inside the address space, and `fcn` must lie below `sz`.
These are checked **under `ptable.lock`**, because another thread may resize
the address space concurrently. On failure the kernel stack is freed and
`clone` returns -1. The thread duplicates the caller's file references and
cwd, which xv6 keeps per proc. It returns the new thread's pid.

**Shared address space and its lifetime.** The thread receives the *same*
`pgdir` and is marked `is_thread = 1`. The design question is who frees the
page table. The rule chosen is **leader ownership**. The group leader (the
unique non-thread proc with that `pgdir`) owns it, and threads never free it.
The leader cannot release it while threads live: `exit()` and the commit
point of `exec()` first call `reap_threads()`. That function marks every
thread of the group killed, wakes sleeping ones, adopts them so their exit
wakes the leader, and sleeps until all have exited and their kernel stacks
are freed. Only then can the leader become a ZOMBIE whose `pgdir` `wait()`
frees.

This gives the same guarantee as a reference count without one: a thread is
never visible after its leader's last scan, because `clone` publishes the
thread (`pgdir`, `is_thread`, `parent`) atomically under `ptable.lock`, and
any thread doing so is itself still alive. `exec()` from a non-leader thread
returns -1, since it would free the page table under its leader. `growproc()`
holds `ptable.lock` while it reads the old size, grows or shrinks, and
publishes the new `sz` to every proc sharing the `pgdir`. Without that, two
threads' `sbrk` calls could return the same region. `fork()` from any thread
still `copyuvm`s an independent copy.

**Termination and joining.** A thread ends with `exit()`. It closes its files
and becomes a ZOMBIE, then `wakeup1(parent)`. `join()` scans for a ZOMBIE
`is_thread` child of the caller, frees only its kernel stack and slot, and
returns its pid. It returns -1 if the caller has no threads or was killed.
`wait()` skips threads, so it never reaps them and a thread child doesn't
count as a child. Threads created by an exiting thread are handed to the
leader.

**Observed behaviour.** `threadtest` starts three threads that each run
`counter++` 100,000 times. That compiles to `mov counter,%eax; add $1,%eax;
mov %eax,counter`. When two CPUs interleave between the load and the store,
one increment overwrites the other. Under TCG, 100,000 iterations take only
a few milliseconds, so the threads spin on a start flag until all three
exist (`-n` disables this).

| CPUs | Runs with lost updates | Example finals |
|---|---|---|
| 1 | 0 / 5 | 300000 |
| 2 (barrier) | 6 / 10 | 213814 – 300000 |
| 2 (no barrier) | 1 / 10 | 266134 |
| 4 | 2 / 5 | 108449, 240953 |

The result is nondeterministic because whether updates are lost depends on
how the CPUs' instruction streams happen to overlap. 300,000 is a legitimate
outcome. On one CPU a loss needs a timer interrupt between the load and the
store. TCG only takes interrupts between translated blocks, and the whole
loop body is one block, so it never happens there; real hardware could
differ.

All 11 kernel checks pass on 1, 2 and 4 CPUs: sharing in both directions;
four invalid-argument cases; `join` with no threads; `wait` ignoring a
finished thread that `join` then collects; fork independence; `exec` from a
thread; and 40 consecutive leader exits with one thread spinning and one
blocked in `sem_wait`, all reaped without exhausting the 64 proc slots.

## 2. Q2 — Semaphores

**Data structure and logic** (`semaphore.c`). There are 10 semaphores, each
`{spinlock, value ≥ 0, waiters, initialized}`.
`sem_wait`: under the lock, `while(value == 0) { if killed return -1;
waiters++; sleep(s, &lock); waiters--; } value--`.
`sem_signal`: under the lock, `value++` and `wakeup(s)` if there are waiters.

`sleep()` takes `ptable.lock` before it releases the semaphore lock, and
`sem_signal` must hold that lock to change the value. So a signal cannot
fall between the test and the sleep, and no wakeup is lost. The lock order
is always semaphore lock → `ptable.lock`, the same order every other xv6
sleeper uses. `ptable.lock` is never held across a sleep on behalf of a
semaphore, and blocked processes sleep instead of spinning.

The non-negative count is equivalent to the textbook signed version, where
`−value` would count waiters; here waiters are counted explicitly. Errors
return -1: an id outside 0–9; a negative initial value; wait or signal on an
uninitialized semaphore; overflow; a killed waiter; and **`sem_init` while
processes are blocked** on that semaphore. Re-initializing an idle semaphore
is allowed so that tests can be re-run.

**Counter.** The same three threads, now running `sem_wait(0); counter++;
sem_signal(0)`, give exactly **300,000**, against 159,990 without the
semaphore in the same boot. The mutex ensures at most one thread is ever
between its load and its store. Correctness costs speed: 17 ticks without
the semaphore versus 5,133 ticks with it (2 CPUs), because each increment
now needs two system calls and contended waits put threads to sleep.

**Producer–consumer.** One producer thread and one consumer thread share a
5-slot circular buffer and 20 items, with `EMPTY = 5`, `FULL = 0` and
`MUTEX = 1`. The producer does `wait(EMPTY), wait(MUTEX)`, puts the item,
then `signal(MUTEX), signal(FULL)`; the consumer is symmetric. The order
"counting semaphore first, then mutex" avoids holding the mutex while
blocked. `in`, `out` and `count` are touched only while holding `MUTEX`.

The consumer is slow for the first 10 items, and the log shows `buffer holds
5/5` while the producer waits. The producer is slow for the last 10, and the
log shows `0/5` while the consumer waits. All 20 items are consumed exactly
once, in FIFO order (sum 210), maximum occupancy is 5, and both threads
terminate.

## 3. Q3 — Deadlock

**Detection** (`detect_deadlock`) uses current Allocation, outstanding
Request and Available. Work starts as Available. Processes holding nothing
start finished. The algorithm repeatedly finishes any process whose Request
is ≤ Work and adds its Allocation back, then reports the unfinished
processes as deadlocked. **Banker's** (`is_safe_state`) instead uses future
claims, Need = Max − Allocation. `request_resources` checks Request ≤ Need
(else error) and Request ≤ Available (else wait), grants tentatively, runs
the safety check, and keeps or rolls back the grant. Inputs are bounds-checked
(n ≤ 8, m ≤ 4, counts in 0–1000, Allocation ≤ Max).

| Scenario | Outcome (computed and printed by the program with all intermediate Work vectors) |
|---|---|
| 1. Detection; Available (0 0 0), P2 requests (0 0 1) | P0 finishes, Work = (0 1 0); **P1–P4 deadlocked** |
| 2. Same allocation, P2 requests nothing | All finish (P0, P2, P3, P4, P1); P1, P3, P4 were *waiting, not deadlocked*. Banker on Available (3 3 2): **safe <P1 P3 P4 P0 P2>**; P1's request (1 0 2) granted |
| 3. The state after that grant (safe) | P0's request (0 2 0) fits Available but leaves Work (2 1 0) below every Need → **unsafe, rolled back**; (3 3 0) must wait; (0 0 3) exceeds Need and is rejected |

**Real circular wait.** Processes A and B share two pipes. Each holds the
only write end of its outgoing pipe and will write only after reading from
the other: A waits for B and B waits for A. Before blocking, each records its
pid, its state and whom it waits for in the Lab 5 shared page (`shm_get`).
The parent acts as watchdog with bounded waits. It confirms that both are
waiting, sleeps 100 ticks, and confirms that neither has progressed. It then
finds the cycle A → B → A in the reported wait-for graph, and recovers by
killing victim A. A's exit closes its write end, so B's `read` returns EOF
and B exits normally. Both are reaped and the shell remains usable.

**Limits.** The dependency edges are *reported by the processes*. The kernel
does not inspect pipe waits, and the matrix detector is a separate
simulation that does not look at these processes. Blocking alone would not
prove deadlock; the proof here is the established cycle plus the observed
lack of progress.

## 4. Q4 — Priority scheduling with aging

**Selection.** `scheduler()` scans the process table under `ptable.lock` and
picks, among RUNNABLE processes, the numerically lowest priority (1 =
highest). **Equal priorities** go to the process with the smallest
`last_sched`: every dispatch takes a ticket `++sched_seq`, so the process
that has waited longest since its last run wins. That is a FIFO queue per
priority level.

My first design, a "next slot" cursor, was measurably unfair: equal children (*exploratory*)
finished at 24, 27 and 47 ticks. The periodically waking priority-1 parent
kept resetting the cursor. Timer interrupts call `yield()` every tick, so
higher-priority work preempts within 10 ms. New processes start at 10; the
range is 1–20. `set_priority`/`get_priority` return -1 for a bad priority or
for a pid that is unused or a ZOMBIE.

**Aging.** On every tick, CPU 0 (the only CPU that counts `ticks`) calls
`age_runnable()` under `ptable.lock`. Every **RUNNABLE** process increments
`wait_ticks`; RUNNING, SLEEPING, ZOMBIE and EMBRYO processes don't. When the
count exceeds 30, priority improves by one level (never past 1) and the
counter restarts. Dispatch resets it. Strict priority alone would starve
priority 20 indefinitely; aging guarantees that a waiting process climbs one
level every 31 ticks until it ties with the running processes and joins
their round robin.

**Measurements** (`priotest`). The children are released together. The
parent runs at priority 1 and samples `get_priority` every 5 ticks. Times
are in ticks from the start signal (first run / finish).

| Priority | 2 CPUs | Final prio | 1 CPU | Final prio |
|---|---|---|---|---|
| 5 | 0 / 262 | 5 | 0 / 603 | 5 |
| 10 | 1 / 336 | 10 | 152 / 1003 | 5 |
| 15 | 155 / 493 | 10 | 308 / 1145 | 5 |
| 20 | **310** / 598 | 10 | **464** / 1206 | 5 |

On 1 CPU, priority 5 runs alone. Each other child gets the CPU exactly when
aging brings it level with priority 5: 5, 10 and 15 levels × ~31 ticks gives
152, 308 and 464 ticks. On 2 CPUs, priorities 5 and 10 run at once, and 15
and 20 start when they age to 10. Priority 20 is never starved, and
completion order follows priority. Three equal-priority children finished
within 6 ticks of each other (2 CPUs) and 37 ticks (1 CPU), out of 423 and
849.

## 5. Testing and conclusion

All four programs end with `ALL CHECKS PASSED` on the final build. The stock
`usertests` pass on 2 and 4 CPUs. A regression session reran every Lab 7
program alongside the Lab 5 programs (`prodcons`, `readwrite`, `dining`,
`peterson`), `zombie`, `forktest` and `ls` without failures or slot
exhaustion (`TESTING.md`).

Known limitations:
* no TLB shootdown when a thread shrinks memory while another runs on a
  different CPU;
* `shm_get` is not available to non-leader threads;
* `set_priority` has no permission check;
* priorities only improve with age and never decay back;
* the unsynchronized race was observed only with ≥ 2 CPUs under TCG.
