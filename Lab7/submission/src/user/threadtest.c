// Lab 7 Q1: user-level threads created with clone() and collected with join().
//
// Experiment: three threads share one global counter and each increments
// it 100000 times WITHOUT synchronization.  counter++ compiles to a load,
// an add and a store; if two threads interleave between the load and the
// store (on two CPUs, or when a timer interrupt preempts a thread mid-way),
// one increment overwrites the other and is lost.  Whether and how often
// that happens depends on timing, so the final value is nondeterministic:
// it is usually below 300000 with CPUS >= 2, but 300000 is also a valid
// outcome.  On one CPU the race needs a timer interrupt to land exactly
// inside the three-instruction window; under QEMU's TCG emulator that
// never happens (interrupts are taken between translated blocks, and the
// whole loop body is one block), so CPUS=1 runs print 300000.
// The program reports what it actually observed.
//
// After the experiment, a set of checks exercises the kernel rules for
// threads: shared memory, argument validation, wait() vs join(),
// fork() independence, exec() from a thread, and teardown of a group whose
// leader exits while threads are still running.
//
// Start barrier: 100000 iterations take only a few milliseconds in QEMU,
// so if each thread started incrementing as soon as it was created, an
// early thread could finish before main had created the later ones and
// the threads would overlap only partly (or not at all).  By default the
// threads therefore spin on `go` until main has created all three, so the
// increments really run concurrently.  The barrier only changes when the
// threads start; the increments themselves remain unsynchronized.  Pass
// -n to disable it and compare.
//
// Usage: threadtest [runs] [-n]   (repeat the experiment `runs` times;
//                                  -n = no start barrier)

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE    4096
#define NTHREADS  3
#define NITER     100000

// Each thread gets one page-aligned page of stack carved out of a malloc'd
// block.  The block stays allocated until the thread has been joined.
struct tstack {
  void *raw;      // pointer returned by malloc, passed to free
  void *base;     // page-aligned stack base handed to clone
};

static int
stack_alloc(struct tstack *s)
{
  s->raw = malloc(2 * PGSIZE);
  if(s->raw == 0)
    return -1;
  s->base = (void*)(((uint)s->raw + PGSIZE - 1) & ~(PGSIZE - 1));
  return 0;
}

static void
stack_free(struct tstack *s)
{
  free(s->raw);
  s->raw = s->base = 0;
}

volatile int counter = 0;
volatile int go = 0;        // start barrier, set by main

void
worker(void *arg)
{
  int i;

  (void)arg;
  while(!go)
    ;
  for(i = 0; i < NITER; i++)
    counter++;          // unsynchronized read-modify-write: may race
  exit();
}

// Returns the final counter, or -1 if a thread could not be created/joined.
static int
counter_experiment(int verbose, int barrier)
{
  struct tstack st[NTHREADS];
  int tids[NTHREADS];
  int i, pid, joined;

  counter = 0;
  go = !barrier;
  for(i = 0; i < NTHREADS; i++){
    if(stack_alloc(&st[i]) < 0){
      printf(1, "threadtest: malloc failed\n");
      return -1;
    }
    tids[i] = clone(worker, (void*)i, st[i].base);
    if(tids[i] < 0){
      printf(1, "threadtest: clone failed\n");
      return -1;
    }
    if(verbose)
      printf(1, "  created thread %d (tid %d), stack 0x%x\n",
             i, tids[i], (uint)st[i].base);
  }
  go = 1;
  joined = 0;
  for(i = 0; i < NTHREADS; i++){
    pid = join();
    if(pid < 0){
      printf(1, "threadtest: join failed\n");
      return -1;
    }
    joined++;
    if(verbose)
      printf(1, "  joined tid %d\n", pid);
  }
  if(join() != -1){
    printf(1, "threadtest: extra join succeeded\n");
    return -1;
  }
  for(i = 0; i < NTHREADS; i++)
    stack_free(&st[i]);
  if(joined != NTHREADS)
    return -1;
  return counter;
}

// ---------------------------------------------------------------------
// Checks

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

volatile int shared_in, shared_out;

void
echo_worker(void *arg)
{
  // Reads a value the main thread stored before clone(), writes a result
  // the main thread reads after join(): both directions of sharing.
  shared_out = shared_in + (int)arg;
  exit();
}

void
quick_worker(void *arg)
{
  (void)arg;
  exit();
}

void
spin_worker(void *arg)
{
  (void)arg;
  for(;;)
    shared_out++;
}

void
semblock_worker(void *arg)
{
  (void)arg;
  sem_wait(9);          // never signalled: blocks until killed
  exit();
}

volatile int exec_result;

void
exec_worker(void *arg)
{
  char *argv[] = { "echo", "exec-from-thread-should-not-run", 0 };

  (void)arg;
  exec_result = exec("echo", argv);
  exit();
}

static void
run_checks(void)
{
  struct tstack st, st2;
  int tid, pid, r, i, ok;

  printf(1, "\nChecks:\n");
  if(stack_alloc(&st) < 0 || stack_alloc(&st2) < 0){
    printf(1, "threadtest: malloc failed\n");
    failures++;
    return;
  }

  // 1. Shared address space in both directions.
  shared_in = 41;
  shared_out = 0;
  tid = clone(echo_worker, (void*)1, st.base);
  r = join();
  check(tid > 0 && r == tid && shared_out == 42,
        "thread reads and writes the creator's globals (41 + 1 == 42)");

  // 2. Argument validation.
  check(clone(quick_worker, 0, (char*)st.base + 4) == -1,
        "clone rejects a stack that is not page aligned");
  check(clone(quick_worker, 0, (void*)0x70000000) == -1,
        "clone rejects a stack outside the address space");
  check(clone(quick_worker, 0, 0) == -1,
        "clone rejects a null stack");
  check(clone((void(*)(void*))0x70000000, 0, st.base) == -1,
        "clone rejects an entry point outside the address space");
  check(join() == -1, "join with no threads returns -1");

  // 3. wait() must not reap a thread; join() must.
  tid = clone(quick_worker, 0, st.base);
  sleep(5);                               // let it become a ZOMBIE
  r = wait();
  check(tid > 0 && r == -1, "wait() ignores a finished thread (returns -1)");
  r = join();
  check(r == tid, "join() then collects that thread");

  // 4. fork() gives the child an independent copy of the address space.
  counter = 777;
  pid = fork();
  if(pid == 0){
    counter = 12345;
    exit();
  }
  wait();
  check(pid > 0 && counter == 777,
        "fork child's write to the counter is invisible to the parent");

  // 5. exec() from a non-leader thread is refused.
  exec_result = 0;
  tid = clone(exec_worker, 0, st.base);
  r = join();
  check(tid > 0 && r == tid && exec_result == -1,
        "exec() called by a thread returns -1");

  // 6. A leader that exits with live threads (one spinning in user mode,
  //    one blocked in the kernel in sem_wait) must kill and reap them.
  //    Repeated 40 times: each round uses 3 proc slots, so a leak would
  //    exhaust the 64-slot table and make fork/clone fail.
  sem_init(9, 0);
  ok = 1;
  for(i = 0; i < 40 && ok; i++){
    pid = fork();
    if(pid < 0){
      ok = 0;
      break;
    }
    if(pid == 0){
      if(clone(spin_worker, 0, st.base) < 0 ||
         clone(semblock_worker, 0, st2.base) < 0)
        printf(1, "  (clone failed in child)\n");
      sleep(1);
      exit();                             // no join: kernel must clean up
    }
    if(wait() != pid)
      ok = 0;
  }
  check(ok, "40 x leader exits with a spinning and a blocked thread: reaped");

  stack_free(&st);
  stack_free(&st2);
}

int
main(int argc, char *argv[])
{
  int runs, r, i, below, total, barrier;

  runs = 1;
  barrier = 1;
  for(i = 1; i < argc; i++){
    if(strcmp(argv[i], "-n") == 0)
      barrier = 0;
    else
      runs = atoi(argv[i]);
  }
  if(runs < 1)
    runs = 1;

  printf(1, "threadtest: %d threads x %d unsynchronized increments, %s\n",
         NTHREADS, NITER,
         barrier ? "threads start together (barrier)" :
                   "threads start as created (no barrier)");
  below = 0;
  for(i = 0; i < runs; i++){
    r = counter_experiment(i == 0, barrier);
    if(r < 0){
      printf(1, "threadtest: run %d FAILED\n", i + 1);
      exit();
    }
    total = NTHREADS * NITER;
    printf(1, "run %d: final counter = %d (expected %d if no update is lost)"
           " -> %s\n", i + 1, r, total,
           r == total ? "no lost updates this run" : "lost updates (race)");
    if(r < total)
      below++;
  }
  printf(1, "%d of %d runs lost updates\n", below, runs);

  run_checks();
  printf(1, "\nthreadtest: %s (%d check failures)\n",
         failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", failures);
  exit();
}
