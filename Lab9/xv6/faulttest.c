// Lab 9 Q2: lazy allocation test.
//
// [1] sbrk(32 * 4096) allocates nothing; writing to exactly 5 different
//     pages causes exactly 5 page faults (read back with getfaults()).
//     Writing again to pages that are already mapped causes no fault.
// [2] fork() still works when most of the heap was never allocated
//     (copyuvm skips those pages); the child gets zero-filled pages.
// [3] Invalid accesses (just above sz, far above sz, the stack guard
//     page) kill only the child that makes them; the parent keeps going.
#include "types.h"
#include "user.h"

#define PGSIZE 4096
#define NPAGES 32

int ok = 1;

void
check(int cond, char *msg)
{
  printf(1, "faulttest: [%s] %s\n", cond ? "PASS" : "FAIL", msg);
  if(!cond)
    ok = 0;
}

// Run an access to addr in a child.  The child reports through a pipe
// if it survives the access; EOF on the pipe means it was killed.
// Returns 1 if the child was killed.
int
badaccess(char *addr, char *what)
{
  int fds[2], pid;
  char c;

  printf(1, "faulttest: child writes to 0x%x (%s)\n", (uint)addr, what);
  pipe(fds);
  pid = fork();
  if(pid < 0){
    printf(1, "faulttest: fork failed\n");
    exit();
  }
  if(pid == 0){
    close(fds[0]);
    *addr = 1;                   // should be killed here
    write(fds[1], "x", 1);       // only reached if the access was allowed
    exit();
  }
  close(fds[1]);
  wait();
  if(read(fds[0], &c, 1) == 1){
    close(fds[0]);
    return 0;
  }
  close(fds[0]);
  return 1;
}

int
main(int argc, char *argv[])
{
  // 5 distinct pages out of the 32; pages 0 and 13 are written twice.
  static int touch[] = { 0, 7, 13, 21, 31, 0, 13 };
  int f0, f1, i, pid, fds[2], cf;
  char *p, *base, *guard;

  base = sbrk(0);
  f0 = getfaults();
  printf(1, "faulttest: [1] page faults handled at start: %d\n", f0);
  check(f0 == 0, "no page faults yet (exec maps the whole program image)");

  p = sbrk(NPAGES * PGSIZE);
  if(p == (char*)-1){
    printf(1, "faulttest: sbrk failed\n");
    exit();
  }
  printf(1, "faulttest: sbrk(%d * 4096) -> heap 0x%x .. 0x%x\n",
         NPAGES, (uint)p, (uint)(p + NPAGES * PGSIZE));
  check(getfaults() == f0, "sbrk itself caused no page fault (nothing allocated)");

  for(i = 0; i < sizeof(touch)/sizeof(touch[0]); i++){
    p[touch[i] * PGSIZE + 100] = 'a' + touch[i];
    printf(1, "faulttest: wrote page %d (va 0x%x), faults so far %d\n",
           touch[i], (uint)(p + touch[i] * PGSIZE + 100), getfaults() - f0);
  }
  f1 = getfaults();
  printf(1, "faulttest: page faults handled for 5 distinct pages: %d\n", f1 - f0);
  check(f1 - f0 == 5, "exactly 5 page faults (re-writing a mapped page is free)");
  for(i = 0; i < 5; i++)
    if(p[touch[i] * PGSIZE + 100] != (char)('a' + touch[i]))
      break;
  check(i == 5, "values written to the 5 pages read back correctly");

  printf(1, "faulttest: [2] fork with %d of %d heap pages never allocated\n",
         NPAGES - 5, NPAGES);
  pipe(fds);
  pid = fork();
  if(pid == 0){
    // Page 3 was never touched by the parent: the child faults it in.
    char v = p[3 * PGSIZE];
    char w = p[7 * PGSIZE + 100];  // copied from the parent, no fault
    char r[3];
    r[0] = (v == 0);
    r[1] = (w == (char)('a' + 7));
    r[2] = getfaults();
    write(fds[1], r, 3);
    exit();
  }
  {
    char r[3];
    close(fds[1]);
    if(read(fds[0], r, 3) != 3)
      r[0] = r[1] = r[2] = 0;
    close(fds[0]);
    wait();
    check(r[0], "child reads 0 from a page the parent never touched");
    check(r[1], "child sees the parent's data in a copied page");
    cf = r[2];
    printf(1, "faulttest: child handled %d page fault(s)\n", cf);
    check(cf == 1, "child's fault counter started at 0 and counted 1 fault");
  }
  check(getfaults() == f1, "parent's fault count unchanged by the child");

  printf(1, "faulttest: [3] invalid accesses kill only the child\n");
  check(badaccess(p + NPAGES * PGSIZE, "first byte above sz"),
        "access just above sz killed the child");
  check(badaccess((char*)0x7ff00000, "far above sz, below KERNBASE"),
        "access far above sz killed the child");
  guard = base - 2 * PGSIZE;
  check(badaccess(guard, "stack guard page, below sz but no PTE_U"),
        "write to the stack guard page killed the child");
  printf(1, "faulttest: parent (pid %d) still running after the children died\n",
         getpid());

  printf(1, ok ? "faulttest: ALL CHECKS PASSED\n" : "faulttest: SOME CHECKS FAILED\n");
  exit();
}
