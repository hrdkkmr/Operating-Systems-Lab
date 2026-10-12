// Lab 7 Q4: priority scheduling with aging.
//
// Experiment 1 (strict priority + aging): four CPU-bound children with
//   priorities 5, 10, 15 and 20 do the same fixed amount of work.
// Experiment 2 (round robin among equals): three CPU-bound children, all at
//   priority 15, do the same work; equal priority should give them similar
//   start and finish times.
//
// How the measurements are taken:
//  - The parent runs at priority 1 so that it is always scheduled when it
//    wakes up.  It forks the children, sets each child's priority with
//    set_priority() (and checks it with get_priority()), then releases all
//    of them at once by writing to a "go" pipe they are blocked on.
//  - While the children run, the parent wakes every POLL ticks and reads
//    every child's priority with get_priority(); each change is printed as
//    an aging event with its time.  Aging only happens to RUNNABLE
//    processes that have waited > 30 ticks, so events are >= 31 ticks apart
//    and every step is observed.  The parent stops polling when
//    get_priority() returns -1 for all children (they have exited).
//  - Each child records (with uptime()) when it first got the CPU after the
//    start signal and when it finished, and sends these through a result
//    pipe.  Children print nothing, so their output cannot interleave.
//  - Times are in ticks (10 ms each) relative to the start signal.
//
// What to expect, and what not to assume: with N CPUs the N best-priority
// RUNNABLE processes run at once; the others wait and age one level per 31
// ticks until they tie with the running ones, after which equal priorities
// share the CPUs round-robin.  So the priority-20 child must eventually get
// CPU time before the others finish, but exact order and times depend on the
// number of CPUs, emulator speed, timer granularity and the parent's
// polling, and are reported as measured rather than asserted.
//
// Usage: priotest [units]      (work per child, default 4000 units; one unit
//                              took about 0.2 ticks of CPU under QEMU TCG)

#include "types.h"
#include "stat.h"
#include "user.h"

#define MAXCH   4
#define POLL    5                // parent sampling period, ticks
#define UNIT    250000           // loop iterations per work unit

struct result {
  int idx;
  int pid;
  int start_prio;
  int first;                     // tick when it first ran after "go"
  int done;                      // tick when it finished its work
  int end_prio;                  // priority when it finished
};

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

static void
burn(int units)
{
  volatile int x = 0;
  int i, j;

  for(i = 0; i < units; i++)
    for(j = 0; j < UNIT; j++)
      x++;
}

static void
child(int idx, int units, int gofd, int resfd)
{
  struct result r;
  char c;

  read(gofd, &c, 1);             // blocked here until the parent says go
  r.first = uptime();            // first instruction after being scheduled
  r.idx = idx;
  r.pid = getpid();
  r.start_prio = get_priority(r.pid);
  burn(units);
  r.done = uptime();
  r.end_prio = get_priority(r.pid);
  write(resfd, &r, sizeof(r));
  exit();
}

// Runs one experiment; fills res[] (indexed by child number).
static void
experiment(int n, int *prio, int units, struct result *res)
{
  int gop[2], resp[2], pid[MAXCH], cur[MAXCH], alive, i, t0, now, pr;
  int events, order[MAXCH], k;
  struct result r;

  if(pipe(gop) < 0 || pipe(resp) < 0){
    printf(1, "priotest: pipe failed\n");
    exit();
  }
  for(i = 0; i < n; i++){
    pid[i] = fork();
    if(pid[i] < 0){
      printf(1, "priotest: fork failed\n");
      exit();
    }
    if(pid[i] == 0){
      close(gop[1]);
      close(resp[0]);
      child(i, units, gop[0], resp[1]);
    }
    if(set_priority(pid[i], prio[i]) < 0)
      printf(1, "priotest: set_priority(%d, %d) failed\n", pid[i], prio[i]);
    cur[i] = get_priority(pid[i]);
    printf(1, "  child %d: pid %d, priority set to %d (get_priority -> %d)\n",
           i, pid[i], prio[i], cur[i]);
  }
  close(gop[0]);
  close(resp[1]);

  t0 = uptime();
  write(gop[1], "gggg", n);      // release all children at once
  close(gop[1]);

  events = 0;
  alive = n;
  while(alive > 0 && uptime() - t0 < 20000){
    sleep(POLL);
    now = uptime() - t0;
    alive = 0;
    for(i = 0; i < n; i++){
      if(cur[i] < 0)
        continue;
      pr = get_priority(pid[i]);
      if(pr < 0){
        cur[i] = -1;               // exited
        continue;
      }
      alive++;
      if(pr != cur[i]){
        printf(1, "  t=%d: aging: child %d (pid %d) priority %d -> %d\n",
               now, i, pid[i], cur[i], pr);
        events++;
        cur[i] = pr;
      }
    }
  }
  if(alive > 0){
    printf(1, "priotest: children still running after 20000 ticks, killing\n");
    for(i = 0; i < n; i++)
      kill(pid[i]);
  }

  for(i = 0; i < n; i++)
    res[i].pid = -1;
  for(k = 0; k < n; k++){
    if(read(resp[0], &r, sizeof(r)) != sizeof(r))
      break;
    r.first -= t0;
    r.done -= t0;
    res[r.idx] = r;
  }
  close(resp[0]);
  for(i = 0; i < n; i++)
    wait();
  printf(1, "  %d aging event(s) observed\n", events);

  // Completion order by finish time (stable for ties).
  for(i = 0; i < n; i++)
    order[i] = i;
  for(i = 1; i < n; i++)
    for(k = i; k > 0 && res[order[k]].done < res[order[k-1]].done; k--){
      int tmp = order[k];
      order[k] = order[k-1];
      order[k-1] = tmp;
    }

  printf(1, "\n  rank  child  pid  prio(start->end)  first-run  finished\n");
  for(k = 0; k < n; k++){
    i = order[k];
    printf(1, "  %d     %d      %d    %d -> %d           t=%d        t=%d\n",
           k + 1, i, res[i].pid, prio[i], res[i].end_prio,
           res[i].first, res[i].done);
  }
}

static void
api_checks(void)
{
  int pid, me;

  printf(1, "\n=== set_priority / get_priority checks ===\n");
  me = getpid();
  check(set_priority(me, 0) == -1 && set_priority(me, 21) == -1,
        "priorities outside 1..20 are rejected");
  check(set_priority(-4, 5) == -1 && set_priority(99999, 5) == -1,
        "set_priority on a nonexistent pid returns -1");
  check(get_priority(0) == -1 && get_priority(99999) == -1,
        "get_priority on a nonexistent pid returns -1");
  check(set_priority(me, 7) == 0 && get_priority(me) == 7,
        "set_priority(self, 7) then get_priority -> 7");
  pid = fork();
  if(pid == 0){
    sleep(20);
    exit();
  }
  check(pid > 0 && get_priority(pid) == 10, "a new process starts at priority 10");
  wait();
  check(get_priority(pid) == -1, "get_priority of an exited process returns -1");
}

int
main(int argc, char *argv[])
{
  int units = 4000;
  int prio1[MAXCH] = { 5, 10, 15, 20 };
  int prio2[MAXCH] = { 15, 15, 15 };
  struct result res[MAXCH];
  int i, minf, maxf, maxfirst, r20;

  if(argc > 1)
    units = atoi(argv[1]);
  if(units < 1)
    units = 1;

  api_checks();
  if(set_priority(getpid(), 1) < 0){
    printf(1, "priotest: cannot set own priority\n");
    exit();
  }

  printf(1, "\n=== Experiment 1: priorities 5, 10, 15, 20; %d work units each"
         " ===\n", units);
  experiment(4, prio1, units, res);
  r20 = 3;
  check(res[r20].pid > 0 && res[r20].first >= 0,
        "the priority-20 child got the CPU and completed (no starvation)");
  // Aging is only required if the child waited more than 30 ticks for its
  // first turn (with a small `units` it may never wait that long).
  if(res[r20].first > 30)
    check(res[r20].end_prio < 20, "the priority-20 child, which waited more"
          " than 30 ticks, was aged to a better priority");
  else
    printf(1, "  (priority-20 child ran within 30 ticks; aging not required)\n");
  maxfirst = 0;
  for(i = 0; i < 3; i++)
    if(res[i].done > maxfirst)
      maxfirst = res[i].done;
  printf(1, "  priority-20 child first ran at t=%d; last of the other children"
         " finished at t=%d\n", res[r20].first, maxfirst);
  check(res[0].first <= res[r20].first,
        "priority 5 started no later than priority 20");

  printf(1, "\n=== Experiment 2: three children, all priority 15; %d work units"
         " each ===\n", units);
  experiment(3, prio2, units, res);
  minf = maxf = res[0].done;
  for(i = 1; i < 3; i++){
    if(res[i].done < minf) minf = res[i].done;
    if(res[i].done > maxf) maxf = res[i].done;
  }
  printf(1, "  finish times spread over %d ticks (first %d, last %d)\n",
         maxf - minf, minf, maxf);
  check(res[0].pid > 0 && res[1].pid > 0 && res[2].pid > 0,
        "all equal-priority children completed");
  // Equal work at equal priority: with round robin they finish close
  // together.  An unfair choice (e.g. always the lowest slot) would let
  // two finish around half-way and the third much later.
  check(maxf > 0 && (maxf - minf) * 4 <= maxf,
        "finish times within 25% of each other (round robin is fair)");

  printf(1, "\npriotest: %s (%d check failures)\n",
         failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", failures);
  exit();
}
