// Lab 7 Q2: synchronization with kernel counting semaphores.
//
// Part A - shared counter.  Three threads (clone/join from Q1) each add
//   100000 to a shared counter.  It is run twice in a row:
//     1. unsynchronized, exactly as in threadtest (Q1), for comparison;
//     2. with every increment surrounded by sem_wait(MUTEX)/sem_signal(MUTEX),
//        where MUTEX was initialized to 1.
//   With the mutex at most one thread is ever between its load and its
//   store of the counter, so no update can be lost and the result must be
//   exactly 300000.  Without it, increments overlap and get lost.
//
// Part B - producer/consumer on a bounded buffer of 5 slots, one producer
//   thread and one consumer thread, NITEMS = 20 items (values 1..20).
//     EMPTY = 5  free slots      producer: wait(EMPTY) wait(MUTEX) put
//     FULL  = 0  filled slots              signal(MUTEX) signal(FULL)
//     MUTEX = 1  protects buf/in/out/count (consumer is symmetric)
//   The consumer is slow during the first half (the buffer fills to 5 and
//   the producer must block on EMPTY) and the producer is slow during the
//   second half (the buffer drains to 0 and the consumer must block on
//   FULL).  Every put/get is logged with its slot and the occupancy, and
//   the program checks occupancy stays in [0,5], items arrive in FIFO
//   order, and all 20 items are produced and consumed exactly once.
//   Both threads run a fixed number of iterations, so both terminate.
//
// Semaphore ids are global kernel objects (0..9); this program uses 0-4.

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE    4096
#define NTHREADS  3
#define NITER     100000

#define SEM_COUNTER 0       // Part A mutex
#define SEM_EMPTY   1       // Part B
#define SEM_FULL    2
#define SEM_MUTEX   3
#define SEM_TEST    4       // API checks

#define BUFSZ     5
#define NITEMS    20
#define SLOW      30        // ticks; much longer than printing one log line

struct tstack {
  void *raw;
  void *base;
};

static int
spawn(void (*fn)(void*), void *arg, struct tstack *s)
{
  s->raw = malloc(2 * PGSIZE);
  if(s->raw == 0)
    return -1;
  s->base = (void*)(((uint)s->raw + PGSIZE - 1) & ~(PGSIZE - 1));
  return clone(fn, arg, s->base);
}

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

// ---------------------------------------------------------------------
// Part A

volatile int counter;
volatile int go;
volatile int sem_errors;

void
plain_worker(void *arg)
{
  int i;

  (void)arg;
  while(!go)
    ;
  for(i = 0; i < NITER; i++)
    counter++;
  exit();
}

void
locked_worker(void *arg)
{
  int i;

  (void)arg;
  while(!go)
    ;
  for(i = 0; i < NITER; i++){
    if(sem_wait(SEM_COUNTER) < 0)
      sem_errors++;
    counter++;                      // critical section
    if(sem_signal(SEM_COUNTER) < 0)
      sem_errors++;
  }
  exit();
}

static int
run_counter(void (*fn)(void*))
{
  struct tstack st[NTHREADS];
  int i, t0, t1;

  counter = 0;
  go = 0;
  for(i = 0; i < NTHREADS; i++)
    if(spawn(fn, 0, &st[i]) < 0){
      printf(1, "synctest: clone failed\n");
      exit();
    }
  t0 = uptime();
  go = 1;                           // start all threads together
  for(i = 0; i < NTHREADS; i++)
    if(join() < 0){
      printf(1, "synctest: join failed\n");
      exit();
    }
  t1 = uptime();
  for(i = 0; i < NTHREADS; i++)
    free(st[i].raw);
  printf(1, "    (%d ticks)\n", t1 - t0);
  return counter;
}

static void
part_a(void)
{
  int plain, locked;

  printf(1, "\n=== Part A: shared counter, %d threads x %d increments ===\n",
         NTHREADS, NITER);

  printf(1, "  1. without synchronization (as in Q1 threadtest):\n");
  plain = run_counter(plain_worker);
  printf(1, "     final counter = %d of %d -> %s\n", plain, NTHREADS * NITER,
         plain == NTHREADS * NITER ? "no update lost this time" :
                                     "updates were lost (race)");

  printf(1, "  2. with sem_wait/sem_signal on a mutex semaphore (value 1):\n");
  if(sem_init(SEM_COUNTER, 1) < 0){
    printf(1, "synctest: sem_init failed\n");
    exit();
  }
  sem_errors = 0;
  locked = run_counter(locked_worker);
  printf(1, "     final counter = %d of %d\n", locked, NTHREADS * NITER);
  check(locked == NTHREADS * NITER && sem_errors == 0,
        "synchronized counter is exactly 300000");
}

// ---------------------------------------------------------------------
// Semaphore API checks

volatile int blocked_ret;

void
blocker(void *arg)
{
  (void)arg;
  blocked_ret = sem_wait(SEM_TEST);   // blocks: value is 0
  exit();
}

static void
api_checks(void)
{
  struct tstack st;
  int tid;

  printf(1, "\n=== Semaphore API checks ===\n");
  check(sem_init(-1, 1) == -1 && sem_init(10, 1) == -1,
        "sem_init rejects ids outside 0..9");
  check(sem_init(SEM_TEST, -1) == -1, "sem_init rejects a negative value");
  check(sem_wait(10) == -1 && sem_signal(-3) == -1,
        "sem_wait/sem_signal reject invalid ids");

  check(sem_init(SEM_TEST, 0) == 0, "sem_init(4, 0) succeeds");
  blocked_ret = 99;
  tid = spawn(blocker, 0, &st);
  sleep(10);                          // let the thread block in sem_wait
  check(tid > 0 && blocked_ret == 99, "sem_wait on value 0 blocks");
  check(sem_init(SEM_TEST, 1) == -1,
        "sem_init refuses to reset a semaphore that has a waiter");
  check(sem_signal(SEM_TEST) == 0, "sem_signal succeeds");
  check(join() == tid && blocked_ret == 0,
        "the signal wakes the waiter, its sem_wait returns 0");
  check(sem_init(SEM_TEST, 1) == 0,
        "sem_init on the now idle semaphore succeeds");
  free(st.raw);
}

// ---------------------------------------------------------------------
// Part B

int buf[BUFSZ];
volatile int in, out, count;        // protected by SEM_MUTEX
volatile int maxcount;
volatile int violations;
volatile int produced, consumed, consumed_sum, order_errors;

void
producer(void *arg)
{
  int item, slot;

  (void)arg;
  for(item = 1; item <= NITEMS; item++){
    if(item > NITEMS / 2)
      sleep(SLOW);                  // second half: slow producer
    sem_wait(SEM_EMPTY);            // wait for a free slot
    sem_wait(SEM_MUTEX);
    slot = in;
    buf[slot] = item;
    in = (in + 1) % BUFSZ;
    count++;
    produced++;
    if(count > BUFSZ || count < 0)
      violations++;
    if(count > maxcount)
      maxcount = count;
    printf(1, "  producer: put item %d into slot %d   (buffer holds %d/%d)\n",
           item, slot, count, BUFSZ);
    sem_signal(SEM_MUTEX);
    sem_signal(SEM_FULL);           // one more filled slot
  }
  exit();
}

void
consumer(void *arg)
{
  int i, item, slot;

  (void)arg;
  for(i = 1; i <= NITEMS; i++){
    if(i <= NITEMS / 2)
      sleep(SLOW);                  // first half: slow consumer
    sem_wait(SEM_FULL);             // wait for a filled slot
    sem_wait(SEM_MUTEX);
    slot = out;
    item = buf[slot];
    out = (out + 1) % BUFSZ;
    count--;
    consumed++;
    consumed_sum += item;
    if(count < 0 || count > BUFSZ)
      violations++;
    if(item != i)
      order_errors++;
    printf(1, "  consumer: got item %d from slot %d   (buffer holds %d/%d)\n",
           item, slot, count, BUFSZ);
    sem_signal(SEM_MUTEX);
    sem_signal(SEM_EMPTY);          // one more free slot
  }
  exit();
}

static void
part_b(void)
{
  struct tstack ps, cs;

  printf(1, "\n=== Part B: producer/consumer, buffer of %d, %d items ===\n",
         BUFSZ, NITEMS);
  in = out = count = 0;
  maxcount = 0;
  violations = produced = consumed = consumed_sum = order_errors = 0;
  if(sem_init(SEM_EMPTY, BUFSZ) < 0 || sem_init(SEM_FULL, 0) < 0 ||
     sem_init(SEM_MUTEX, 1) < 0){
    printf(1, "synctest: sem_init failed\n");
    exit();
  }
  if(spawn(producer, 0, &ps) < 0 || spawn(consumer, 0, &cs) < 0){
    printf(1, "synctest: clone failed\n");
    exit();
  }
  if(join() < 0 || join() < 0){
    printf(1, "synctest: join failed\n");
    exit();
  }
  free(ps.raw);
  free(cs.raw);

  printf(1, "\n  produced %d, consumed %d, sum of consumed items %d (expect %d)\n",
         produced, consumed, consumed_sum, NITEMS * (NITEMS + 1) / 2);
  printf(1, "  maximum occupancy observed %d, final occupancy %d\n",
         maxcount, count);
  check(produced == NITEMS && consumed == NITEMS &&
        consumed_sum == NITEMS * (NITEMS + 1) / 2,
        "every item produced and consumed exactly once");
  check(order_errors == 0, "items consumed in FIFO order 1..20");
  check(violations == 0 && maxcount <= BUFSZ && count == 0,
        "occupancy always within 0..5 (no overflow/underflow), ends at 0");
  check(maxcount == BUFSZ, "buffer reached capacity 5 (producer had to wait)");
}

int
main(void)
{
  part_a();
  api_checks();
  part_b();
  printf(1, "\nsynctest: %s (%d check failures)\n",
         failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", failures);
  exit();
}
