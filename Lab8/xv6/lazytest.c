// Lab 8 Q4: lazy allocation (demand paging for the heap).
//
//   lazytest        run all the tests below
//   lazytest oom    additionally run the out-of-memory test (a child
//                   touches more memory than the machine has and must be
//                   killed by the page-fault handler, not crash xv6)
//
// Size choice: the kernel manages about 56,000 4 KB pages (PHYSTOP =
// 224 MB) and user memory may grow up to USERTOP (~2 GB), so the 32 MB
// region of the assignment fits easily.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "meminfo.h"

#define PGSIZE  4096
#define MB      (1024 * 1024)
#define BIG     (32 * MB)

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

static void
stats(char *label, struct meminfo *m)
{
  if(getmeminfo(m) < 0){
    printf(1, "getmeminfo failed\n");
    exit();
  }
  printf(1, "%s\n  free_pages %d  proc_pages(sz/PGSIZE) %d  resident %d"
         "  pt_pages %d (user %d)\n", label, m->free_pages, m->proc_pages,
         m->resident_pages, m->pt_pages, m->pt_user_pages);
}

// Run fn in a child and report whether the child was killed before it
// could write "ok" to the pipe.  The parent continues either way.
static int
killed_in_child(void (*fn)(void))
{
  int fds[2], pid;
  char c;

  if(pipe(fds) < 0)
    return -1;
  pid = fork();
  if(pid < 0)
    return -1;
  if(pid == 0){
    close(fds[0]);
    fn();
    write(fds[1], "k", 1);     // only reached if fn() did not fault
    exit();
  }
  close(fds[1]);
  wait();
  c = 0;
  read(fds[0], &c, 1);
  close(fds[0]);
  return c != 'k';
}

static volatile int sink;

static void
read_beyond_sz(void)
{
  char *p = sbrk(0) + PGSIZE;
  printf(1, "  child: reading 0x%x (>= sz)...\n", (uint)p);
  sink = *(volatile char*)p;
}

static void
write_below_usertop(void)
{
  char *p = (char*)0x70000000;
  printf(1, "  child: writing 0x%x (unmapped, far above sz)...\n", (uint)p);
  *p = 1;
}

static void
read_kernel(void)
{
  char *p = (char*)0x80000000;
  printf(1, "  child: reading kernel address 0x%x...\n", (uint)p);
  sink = *(volatile char*)p;
}

static void
write_guard(void)
{
  int local;
  char *guard = (char*)(((uint)&local & ~(PGSIZE-1)) - PGSIZE);
  printf(1, "  child: writing the stack guard page 0x%x...\n", (uint)guard);
  *guard = 1;
}

static void
oom_child(void)
{
  char *p;
  uint n, i;

  // More than all of physical memory: sbrk() itself succeeds (lazy).
  n = 240 * MB;
  p = sbrk(n);
  if(p == (char*)-1){
    printf(1, "  child: sbrk(240 MB) failed\n");
    exit();
  }
  printf(1, "  child: sbrk(240 MB) ok, touching every page...\n");
  for(i = 0; i < n; i += PGSIZE)
    p[i] = 1;
}

int
main(int argc, char *argv[])
{
  struct meminfo m0, m1, m2, m3, m4, m5, m6;
  char *a, *buf;
  int i, fds[2], pid, n, offs[4], nt;
  char msg[16];

  printf(1, "\n=== lazytest: Q4 lazy allocation ===\n");
  stats("\n[1] initial:", &m0);

  a = sbrk(BIG);
  if(a == (char*)-1){
    printf(1, "sbrk(32 MB) failed\n");
    exit();
  }
  stats("\n[2] after sbrk(32 MB), nothing touched:", &m1);
  printf(1, "  region 0x%x - 0x%x\n", (uint)a, (uint)(a + BIG));
  check(m1.proc_pages == m0.proc_pages + BIG / PGSIZE,
        "sz grew by 8192 pages");
  check(m1.free_pages == m0.free_pages,
        "sbrk(32 MB) consumed no physical pages");

  printf(1, "\n[3] touch 4 widely spaced pages\n");
  offs[0] = 0;
  offs[1] = 8 * MB;
  offs[2] = 16 * MB + 5 * PGSIZE;
  offs[3] = BIG - PGSIZE;
  nt = 4;
  for(i = 0; i < nt; i++){
    a[offs[i]] = 'A' + i;
    printf(1, "  touched 0x%x\n", (uint)(a + offs[i]));
  }
  stats("  after touching:", &m2);
  printf(1, "  free_pages dropped by %d = %d data pages + %d page-table "
         "pages\n", m1.free_pages - m2.free_pages, nt,
         m2.pt_pages - m1.pt_pages);
  check(m2.resident_pages == m1.resident_pages + nt,
        "exactly the 4 touched pages became resident");
  check(m1.free_pages - m2.free_pages == nt + (m2.pt_pages - m1.pt_pages),
        "only touched pages (+ their page tables) consumed frames");

  printf(1, "\n[4] repeated access to the same pages\n");
  for(i = 0; i < nt; i++)
    a[offs[i]]++;
  getmeminfo(&m3);
  for(i = 0; i < nt; i++)
    if(a[offs[i]] != 'A' + i + 1)
      break;
  check(i == nt, "values survive repeated access");
  check(m3.free_pages == m2.free_pages, "no new frames on repeated access");
  check(a[12345 * 2] == 0, "an untouched page reads as zero");

  printf(1, "\n[5] system calls with untouched lazy buffers\n");
  if(pipe(fds) < 0){
    printf(1, "pipe failed\n");
    exit();
  }
  buf = a + 20 * MB;                    // untouched page
  write(fds[1], "hello lazy", 10);
  n = read(fds[0], buf, 10);
  check(n == 10 && buf[0] == 'h' && buf[9] == 'y',
        "read() into an untouched page works");
  n = write(fds[1], a + 24 * MB, 16);   // untouched -> zeros are written
  check(n == 16, "write() from an untouched page works");
  n = read(fds[0], msg, 16);
  check(n == 16 && msg[0] == 0 && msg[15] == 0,
        "...and the bytes written from it are zeros (fresh page)");
  close(fds[0]);
  close(fds[1]);

  printf(1, "\n[6] fork with lazy (absent) pages\n");
  getmeminfo(&m4);
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    // copyuvm skipped the absent pages; touched ones are shared (COW).
    if(a[offs[1]] != 'B' + 1 || a[28 * MB] != 0){
      printf(1, "  child: wrong data\n");
      exit();
    }
    a[offs[1]] = 'z';                   // copy-on-write copy
    a[28 * MB] = 'n';                   // new lazy page in the child only
    printf(1, "  child: read touched + untouched pages, wrote both, ok\n");
    exit();
  }
  wait();
  getmeminfo(&m5);
  check(m5.free_pages == m4.free_pages,
        "free_pages back to the pre-fork value after wait()");
  check(a[offs[1]] == 'B' + 1, "parent's page unchanged by the child");
  check(va2pa((int)(a + 28 * MB)) == -1,
        "page touched only by the child is still absent in the parent");

  printf(1, "\n[7] invalid accesses in child processes\n");
  check(killed_in_child(read_beyond_sz) == 1, "read at sz + 4096 killed the child");
  check(killed_in_child(write_below_usertop) == 1,
        "write far above sz killed the child");
  check(killed_in_child(read_kernel) == 1, "read of a kernel address killed the child");
  check(killed_in_child(write_guard) == 1, "write to the stack guard page killed the child");
  check(sbrk(0x7FFFFFFF) == (char*)-1, "sbrk beyond USERTOP fails");
  check(sbrk(-0x7FFFFFFF) == (char*)-1, "sbrk below address 0 fails");

  if(argc > 1 && strcmp(argv[1], "oom") == 0){
    printf(1, "\n[7b] out of memory in a page fault\n");
    getmeminfo(&m4);
    check(killed_in_child(oom_child) == 1,
          "child touching 240 MB was killed (kalloc failure)");
    getmeminfo(&m5);
    check(m5.free_pages == m4.free_pages, "all its frames were reclaimed");
  }

  printf(1, "\n[8] shrink the 32 MB region\n");
  sbrk(-BIG);
  stats("  after sbrk(-32 MB):", &m6);
  printf(1, "  (xv6 keeps page-table pages until exit: user pt pages %d)\n",
         m6.pt_user_pages);
  check(m6.proc_pages == m0.proc_pages, "sz back to the initial value");
  check(m6.resident_pages == m0.resident_pages,
        "every page of the region was unmapped");
  check(m6.free_pages == m0.free_pages - (m6.pt_pages - m0.pt_pages),
        "all data frames freed (only retained page tables differ)");

  printf(1, "\nlazytest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
