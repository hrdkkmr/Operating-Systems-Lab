// Lab 7 Q3: deadlock detection, deadlock avoidance, and a real circular wait.
//
// Part 1 (matrix simulation, pure user-space computation):
//   detect_deadlock()   - deadlock DETECTION.  Uses Allocation, the current
//                         outstanding Request of each process and Available.
//                         Answers "can the processes that are waiting right
//                         now ever be satisfied?"
//   is_safe_state()     - Banker's safety algorithm (deadlock AVOIDANCE).
//                         Uses Allocation, Max (future claims) and Available,
//                         with Need = Max - Allocation.  Answers "is there an
//                         order in which every process can still obtain its
//                         maximum claim and finish?"
//   request_resources() - Banker's resource-request algorithm: validate a
//                         request, grant it tentatively, run the safety check,
//                         keep it if safe, roll it back if not.
//   Three scenarios: (1) a deadlocked state, (2) a state where everybody can
//   finish (both by detection and by Banker), plus a granted request,
//   (3) a safe state that would become unsafe after a request.
//
// Part 2 (real xv6 processes): two processes connected by two pipes form a
//   genuine circular wait; a watchdog parent observes it through a shared
//   page, confirms that neither process makes progress, finds the cycle in
//   the wait-for graph the processes reported, and recovers by killing a
//   victim.  Every wait in the parent is bounded, so the demo cannot hang
//   the shell.  The matrix detector of Part 1 does NOT inspect these
//   processes; the two parts are independent demonstrations.
//
// Usage: deadlock [matrix | procs]      (default: both parts)

#include "types.h"
#include "stat.h"
#include "user.h"

#define MAXP 8              // maximum number of processes in a scenario
#define MAXR 4              // maximum number of resource types
#define MAXUNITS 1000       // sanity bound for any single resource count

// ---------------------------------------------------------------------
// Printing helpers

static void
print_vec(char *name, int m, int *v)
{
  int j;

  printf(1, "%s(", name);
  for(j = 0; j < m; j++)
    printf(1, j ? " %d" : "%d", v[j]);
  printf(1, ")");
}

static void
print_matrix(char *name, int n, int m, int mat[][MAXR])
{
  int i, j;

  printf(1, "  %s:\n", name);
  for(i = 0; i < n; i++){
    printf(1, "    P%d:", i);
    for(j = 0; j < m; j++)
      printf(1, " %d", mat[i][j]);
    printf(1, "\n");
  }
}

// a <= b componentwise
static int
leq(int m, int *a, int *b)
{
  int j;

  for(j = 0; j < m; j++)
    if(a[j] > b[j])
      return 0;
  return 1;
}

static int
is_zero(int m, int *a)
{
  int j;

  for(j = 0; j < m; j++)
    if(a[j] != 0)
      return 0;
  return 1;
}

// Every count must be in [0, MAXUNITS] and the dimensions within bounds,
// so no index goes out of range and no sum can overflow.
static int
valid_dims(int n, int m)
{
  return n >= 1 && n <= MAXP && m >= 1 && m <= MAXR;
}

static int
valid_vec(int m, int *v)
{
  int j;

  for(j = 0; j < m; j++)
    if(v[j] < 0 || v[j] > MAXUNITS)
      return 0;
  return 1;
}

static int
valid_matrix(int n, int m, int mat[][MAXR])
{
  int i;

  for(i = 0; i < n; i++)
    if(!valid_vec(m, mat[i]))
      return 0;
  return 1;
}

// ---------------------------------------------------------------------
// Deadlock detection (several instances per resource type)
//
// Returns the number of deadlocked processes (0 = no deadlock) and marks
// them in deadlocked[], or -1 if the input is invalid.
int
detect_deadlock(int n, int m, int alloc[][MAXR], int request[][MAXR],
                int avail[], int deadlocked[])
{
  int work[MAXR], finish[MAXP];
  int i, j, pass, progress, count;

  if(!valid_dims(n, m) || !valid_matrix(n, m, alloc) ||
     !valid_matrix(n, m, request) || !valid_vec(m, avail))
    return -1;

  // Step 1: Work = Available.
  for(j = 0; j < m; j++)
    work[j] = avail[j];
  // Step 2: a process holding nothing cannot be part of a deadlock
  // (nobody can be waiting for it), so it is considered finished.
  for(i = 0; i < n; i++)
    finish[i] = is_zero(m, alloc[i]);

  printf(1, "  Detection: ");
  print_vec("Work = Available = ", m, work);
  printf(1, "\n");
  for(i = 0; i < n; i++)
    if(finish[i])
      printf(1, "    P%d holds nothing -> Finish = true initially\n", i);

  // Steps 3-5: repeatedly let any process whose outstanding request fits
  // in Work run to completion and return its allocation.
  pass = 0;
  do {
    pass++;
    progress = 0;
    for(i = 0; i < n; i++){
      if(finish[i] || !leq(m, request[i], work))
        continue;
      for(j = 0; j < m; j++)
        work[j] += alloc[i][j];
      finish[i] = 1;
      progress = 1;
      printf(1, "    pass %d: P%d ", pass, i);
      print_vec("Request ", m, request[i]);
      printf(1, " <= Work -> can finish, releases ");
      print_vec("", m, alloc[i]);
      printf(1, ", ");
      print_vec("Work = ", m, work);
      printf(1, "\n");
    }
  } while(progress);
  printf(1, "    pass %d: no further process can finish\n", pass);

  // Step 6: whoever is still unfinished is deadlocked.
  count = 0;
  for(i = 0; i < n; i++){
    deadlocked[i] = !finish[i];
    if(deadlocked[i]){
      count++;
      printf(1, "    P%d: DEADLOCKED - ", i);
      print_vec("Request ", m, request[i]);
      printf(1, " can never be met by ");
      print_vec("Work ", m, work);
      printf(1, "\n");
    } else if(!is_zero(m, request[i]))
      printf(1, "    P%d: waiting for resources, but NOT deadlocked"
             " (its request can be satisfied)\n", i);
    else
      printf(1, "    P%d: not waiting, not deadlocked\n", i);
  }
  return count;
}

// ---------------------------------------------------------------------
// Banker's algorithm

static void
compute_need(int n, int m, int alloc[][MAXR], int max[][MAXR],
             int need[][MAXR])
{
  int i, j;

  for(i = 0; i < n; i++)
    for(j = 0; j < m; j++)
      need[i][j] = max[i][j] - alloc[i][j];
}

// Returns 1 if safe (a safe sequence is stored in seq[0..n-1]),
// 0 if unsafe, -1 if the input is invalid (including Allocation > Max).
int
is_safe_state(int n, int m, int alloc[][MAXR], int max[][MAXR], int avail[],
              int seq[])
{
  int need[MAXP][MAXR], work[MAXR], finish[MAXP];
  int i, j, k, pass, progress;

  if(!valid_dims(n, m) || !valid_matrix(n, m, alloc) ||
     !valid_matrix(n, m, max) || !valid_vec(m, avail))
    return -1;
  for(i = 0; i < n; i++)
    for(j = 0; j < m; j++)
      if(alloc[i][j] > max[i][j])
        return -1;

  compute_need(n, m, alloc, max, need);
  print_matrix("Need = Max - Allocation", n, m, need);

  for(j = 0; j < m; j++)
    work[j] = avail[j];
  for(i = 0; i < n; i++)
    finish[i] = 0;

  printf(1, "  Safety check: ");
  print_vec("Work = Available = ", m, work);
  printf(1, "\n");
  k = 0;
  pass = 0;
  do {
    pass++;
    progress = 0;
    for(i = 0; i < n; i++){
      if(finish[i] || !leq(m, need[i], work))
        continue;
      for(j = 0; j < m; j++)
        work[j] += alloc[i][j];
      finish[i] = 1;
      seq[k++] = i;
      progress = 1;
      printf(1, "    pass %d: P%d ", pass, i);
      print_vec("Need ", m, need[i]);
      printf(1, " <= Work -> runs to completion, ");
      print_vec("Work = ", m, work);
      printf(1, "\n");
    }
  } while(progress);

  if(k == n){
    printf(1, "    safe sequence: <");
    for(i = 0; i < n; i++)
      printf(1, i ? " P%d" : "P%d", seq[i]);
    printf(1, ">\n");
    return 1;
  }
  printf(1, "    no remaining process has Need <= ");
  print_vec("Work ", m, work);
  printf(1, "; cannot finish:");
  for(i = 0; i < n; i++)
    if(!finish[i])
      printf(1, " P%d", i);
  printf(1, "\n");
  return 0;
}

#define REQ_GRANTED    1
#define REQ_MUST_WAIT  0
#define REQ_UNSAFE    -2
#define REQ_INVALID   -1

// Banker's resource-request algorithm for process p.  On REQ_GRANTED the
// state (alloc, avail) is updated; in every other case it is unchanged.
int
request_resources(int n, int m, int alloc[][MAXR], int max[][MAXR],
                  int avail[], int p, int req[])
{
  int seq[MAXP], j, r;

  printf(1, "  P%d requests ", p);
  print_vec("", m, req);
  printf(1, "\n");
  if(!valid_dims(n, m) || p < 0 || p >= n || !valid_vec(m, req))
    return REQ_INVALID;

  // 1. Request <= Need, otherwise the process exceeded its maximum claim.
  for(j = 0; j < m; j++)
    if(req[j] > max[p][j] - alloc[p][j]){
      printf(1, "    exceeds the remaining need of P%d -> error, rejected\n", p);
      return REQ_INVALID;
    }
  // 2. Request <= Available, otherwise the process must wait.
  if(!leq(m, req, avail)){
    printf(1, "    exceeds ");
    print_vec("Available ", m, avail);
    printf(1, " -> P%d must wait (state unchanged)\n", p);
    return REQ_MUST_WAIT;
  }
  // 3. Pretend to grant it.
  for(j = 0; j < m; j++){
    avail[j] -= req[j];
    alloc[p][j] += req[j];
  }
  printf(1, "    tentatively granted: ");
  print_vec("Available = ", m, avail);
  printf(1, ", Allocation[P%d] = ", p);
  print_vec("", m, alloc[p]);
  printf(1, "\n");
  // 4. Keep it only if the resulting state is safe.
  r = is_safe_state(n, m, alloc, max, avail, seq);
  if(r == 1){
    printf(1, "    -> resulting state is SAFE: request GRANTED\n");
    return REQ_GRANTED;
  }
  // 5. Roll back.
  for(j = 0; j < m; j++){
    avail[j] += req[j];
    alloc[p][j] -= req[j];
  }
  printf(1, "    -> resulting state is UNSAFE: request DENIED, rolled back to ");
  print_vec("Available = ", m, avail);
  printf(1, "\n");
  return REQ_UNSAFE;
}

// ---------------------------------------------------------------------
// Scenarios.  5 processes, 3 resource types A B C, total (10 5 7) in the
// Banker scenarios and (7 2 6) in the detection scenarios.

static int failures;

static void
expect(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

static void
copy_matrix(int n, int m, int dst[][MAXR], int src[][MAXR])
{
  int i, j;

  for(i = 0; i < n; i++)
    for(j = 0; j < m; j++)
      dst[i][j] = src[i][j];
}

static int det_alloc[MAXP][MAXR] = {
  {0,1,0}, {2,0,0}, {3,0,3}, {2,1,1}, {0,0,2} };

static void
scenario1(void)
{
  int request[MAXP][MAXR] = {
    {0,0,0}, {2,0,2}, {0,0,1}, {1,0,0}, {0,0,2} };
  int avail[MAXR] = {0,0,0};
  int dl[MAXP], r;

  printf(1, "\n==== Scenario 1: a deadlocked state (detection) ====\n");
  print_matrix("Allocation", 5, 3, det_alloc);
  print_matrix("Request (outstanding)", 5, 3, request);
  printf(1, "  ");
  print_vec("Available = ", 3, avail);
  printf(1, "\n");
  r = detect_deadlock(5, 3, det_alloc, request, avail, dl);
  printf(1, "  Result: %d process(es) deadlocked\n", r);
  expect(r == 4 && !dl[0] && dl[1] && dl[2] && dl[3] && dl[4],
         "P1, P2, P3, P4 deadlocked; P0 can finish");
}

static void
scenario2(void)
{
  int request[MAXP][MAXR] = {
    {0,0,0}, {2,0,2}, {0,0,0}, {1,0,0}, {0,0,2} };
  int davail[MAXR] = {0,0,0};
  int alloc[MAXP][MAXR] = {
    {0,1,0}, {2,0,0}, {3,0,2}, {2,1,1}, {0,0,2} };
  int max[MAXP][MAXR] = {
    {7,5,3}, {3,2,2}, {9,0,2}, {2,2,2}, {4,3,3} };
  int avail[MAXR] = {3,3,2};
  int req[MAXR] = {1,0,2};
  int dl[MAXP], seq[MAXP], r;

  printf(1, "\n==== Scenario 2: every process can finish ====\n");
  printf(1, "-- 2a. Detection: same allocation as scenario 1, but P2 is no"
         " longer waiting --\n");
  print_matrix("Allocation", 5, 3, det_alloc);
  print_matrix("Request (outstanding)", 5, 3, request);
  printf(1, "  ");
  print_vec("Available = ", 3, davail);
  printf(1, "\n");
  r = detect_deadlock(5, 3, det_alloc, request, davail, dl);
  printf(1, "  Result: %d process(es) deadlocked\n", r);
  expect(r == 0, "no deadlock: P1, P3, P4 are only waiting");

  printf(1, "\n-- 2b. Banker's safety check --\n");
  print_matrix("Allocation", 5, 3, alloc);
  print_matrix("Max", 5, 3, max);
  printf(1, "  ");
  print_vec("Available = ", 3, avail);
  printf(1, "\n");
  r = is_safe_state(5, 3, alloc, max, avail, seq);
  printf(1, "  Result: %s\n", r == 1 ? "SAFE" : "UNSAFE");
  expect(r == 1 && seq[0] == 1 && seq[1] == 3 && seq[2] == 4 &&
         seq[3] == 0 && seq[4] == 2, "safe, sequence <P1 P3 P4 P0 P2>");

  printf(1, "\n-- 2c. Hypothetical request that keeps the state safe --\n");
  r = request_resources(5, 3, alloc, max, avail, 1, req);
  expect(r == REQ_GRANTED && avail[0] == 2 && avail[1] == 3 && avail[2] == 0,
         "P1's request (1 0 2) granted, Available becomes (2 3 0)");
}

static void
scenario3(void)
{
  // The state reached at the end of scenario 2 (after P1's grant).
  int alloc[MAXP][MAXR] = {
    {0,1,0}, {3,0,2}, {3,0,2}, {2,1,1}, {0,0,2} };
  int max[MAXP][MAXR] = {
    {7,5,3}, {3,2,2}, {9,0,2}, {2,2,2}, {4,3,3} };
  int avail[MAXR] = {2,3,0};
  int before[MAXP][MAXR], seq[MAXP], r, i, j, same;
  int req_unsafe[MAXR] = {0,2,0};
  int req_toobig[MAXR] = {3,3,0};
  int req_overclaim[MAXR] = {0,0,3};

  printf(1, "\n==== Scenario 3: safe state that a request would make unsafe ====\n");
  print_matrix("Allocation", 5, 3, alloc);
  print_matrix("Max", 5, 3, max);
  printf(1, "  ");
  print_vec("Available = ", 3, avail);
  printf(1, "\n");
  r = is_safe_state(5, 3, alloc, max, avail, seq);
  printf(1, "  Current state: %s\n", r == 1 ? "SAFE" : "UNSAFE");
  expect(r == 1, "current state is safe");

  copy_matrix(5, 3, before, alloc);
  printf(1, "\n");
  r = request_resources(5, 3, alloc, max, avail, 0, req_unsafe);
  same = 1;
  for(i = 0; i < 5; i++)
    for(j = 0; j < 3; j++)
      if(before[i][j] != alloc[i][j])
        same = 0;
  expect(r == REQ_UNSAFE && same && avail[0] == 2 && avail[1] == 3 &&
         avail[2] == 0,
         "P0's request (0 2 0) fits Available but is unsafe: denied, rolled back");

  printf(1, "\n");
  r = request_resources(5, 3, alloc, max, avail, 4, req_toobig);
  expect(r == REQ_MUST_WAIT, "P4's request (3 3 0) > Available: must wait");
  printf(1, "\n");
  r = request_resources(5, 3, alloc, max, avail, 4, req_overclaim);
  expect(r == REQ_INVALID, "P4's request (0 0 3) > its Need (4 3 1): rejected");

  printf(1, "\n-- input validation --\n");
  {
    int dl[MAXP], bad[MAXR] = {-1,0,0};
    expect(detect_deadlock(0, 3, alloc, alloc, avail, dl) == -1 &&
           detect_deadlock(MAXP + 1, 3, alloc, alloc, avail, dl) == -1 &&
           detect_deadlock(5, MAXR + 1, alloc, alloc, avail, dl) == -1,
           "detector rejects out-of-range dimensions");
    expect(detect_deadlock(5, 3, alloc, alloc, bad, dl) == -1,
           "detector rejects negative resource counts");
    expect(is_safe_state(5, 3, max, alloc, avail, seq) == -1,
           "Banker rejects Allocation > Max");
  }
}

// ---------------------------------------------------------------------
// Part 2: circular wait between two real processes.

#define ST_NONE      0
#define ST_WAITING   1      // about to block in read()
#define ST_PROGRESS  2      // read() returned data: no deadlock after all
#define ST_RELEASED  3      // read() returned EOF: peer's resource released

struct board {              // lives in the shm_get() page
  volatile int pid[2];
  volatile int state[2];
  volatile int waits_for[2];  // index of the process each one waits for
};

// Process `me` holds the only write end of its outgoing pipe (resource it
// owns) and will release a message on it only after it has received one
// from the other process on its incoming pipe.
static void
party(struct board *b, int me, int infd, int outfd)
{
  char c;
  int n;

  b->pid[me] = getpid();
  b->waits_for[me] = 1 - me;
  b->state[me] = ST_WAITING;
  n = read(infd, &c, 1);            // blocks: the peer does the same
  if(n == 1){
    b->state[me] = ST_PROGRESS;
    write(outfd, "x", 1);
  } else
    b->state[me] = ST_RELEASED;
  exit();
}

// Follows waits_for edges from `start`; returns 1 if it comes back.
static int
cycle_from(struct board *b, int start)
{
  int cur, steps;

  cur = start;
  for(steps = 0; steps < 2; steps++){
    if(b->state[cur] != ST_WAITING)
      return 0;
    cur = b->waits_for[cur];
    if(cur == start)
      return 1;
  }
  return 0;
}

static void
circular_wait_demo(void)
{
  struct board *b;
  int a2b[2], b2a[2], pa, pb, t, w1, w2;

  printf(1, "\n==== Part 2: circular wait between real processes ====\n");
  b = (struct board*)shm_get();
  if(b == 0){
    printf(1, "deadlock: shm_get failed\n");
    failures++;
    return;
  }
  memset(b, 0, sizeof(*b));
  if(pipe(a2b) < 0 || pipe(b2a) < 0){
    printf(1, "deadlock: pipe failed\n");
    failures++;
    return;
  }

  pa = fork();
  if(pa == 0){                      // A: reads b2a, owns the write end of a2b
    close(a2b[0]);
    close(b2a[1]);
    party(b, 0, b2a[0], a2b[1]);
  }
  pb = fork();
  if(pb == 0){                      // B: reads a2b, owns the write end of b2a
    close(a2b[1]);
    close(b2a[0]);
    party(b, 1, a2b[0], b2a[1]);
  }
  // The parent must not keep any pipe end open, otherwise it would be an
  // extra holder of the resources and EOF could never be delivered.
  close(a2b[0]); close(a2b[1]);
  close(b2a[0]); close(b2a[1]);
  if(pa < 0 || pb < 0){
    printf(1, "deadlock: fork failed\n");
    if(pa > 0) kill(pa);
    if(pb > 0) kill(pb);
    while(wait() > 0)
      ;
    failures++;
    return;
  }

  // Watchdog step 1: wait (at most 300 ticks) for both to report.
  for(t = 0; t < 300 && !(b->state[0] == ST_WAITING &&
                          b->state[1] == ST_WAITING); t += 2)
    sleep(2);
  printf(1, "  process A = pid %d: holds write end of pipe A->B,"
         " waits to read pipe B->A (from B)\n", pa);
  printf(1, "  process B = pid %d: holds write end of pipe B->A,"
         " waits to read pipe A->B (from A)\n", pb);
  printf(1, "  each sends only after it has received: A waits for B,"
         " B waits for A\n");

  // Watchdog step 2: observe for 100 ticks; a real deadlock makes no
  // progress no matter how long we wait.
  if(b->state[0] == ST_WAITING && b->state[1] == ST_WAITING){
    sleep(100);
    printf(1, "  after 100 more ticks: A state=%s, B state=%s\n",
           b->state[0] == ST_WAITING ? "still blocked" : "progressed",
           b->state[1] == ST_WAITING ? "still blocked" : "progressed");
  }
  w1 = cycle_from(b, 0);
  w2 = cycle_from(b, 1);
  if(w1 && w2)
    printf(1, "  wait-for graph (reported by the processes): A -> B -> A"
           "  => CYCLE, the processes are deadlocked\n");
  expect(w1 && w2, "circular wait established and no progress for 100 ticks");

  // Recovery: terminate a victim.  Its exit closes the only write end of
  // pipe A->B, so B's read returns EOF: B's resource wait is released.
  printf(1, "  recovery: killing victim A (pid %d)\n", pa);
  kill(pa);
  for(t = 0; t < 200 && b->state[1] == ST_WAITING; t += 2)
    sleep(2);
  if(b->state[1] == ST_WAITING){
    printf(1, "  watchdog: B still blocked after 200 ticks, killing it\n");
    kill(pb);
  }
  wait();
  wait();
  printf(1, "  B (pid %d) %s; both processes reaped\n", pb,
         b->state[1] == ST_RELEASED ?
         "saw EOF after A died and exited normally" : "did not recover");
  expect(b->state[1] == ST_RELEASED,
         "killing one process broke the cycle; the other finished");
}

int
main(int argc, char *argv[])
{
  int matrix = 1, procs = 1;

  if(argc > 1 && strcmp(argv[1], "matrix") == 0)
    procs = 0;
  if(argc > 1 && strcmp(argv[1], "procs") == 0)
    matrix = 0;

  if(matrix){
    printf(1, "Part 1: resource-allocation simulation (A B C resources)\n");
    scenario1();
    scenario2();
    scenario3();
  }
  if(procs)
    circular_wait_demo();
  printf(1, "\ndeadlock: %s (%d check failures)\n",
         failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", failures);
  exit();
}
