// Lab 8 Q2: physical memory statistics with getmeminfo().
//
//  1. print the statistics at start-up (baseline);
//  2. grow the heap with sbrk() and touch every new page (with lazy
//     allocation, Q4, only touched pages consume frames), print again;
//  3. fork a child, print the parent's and the child's view while the
//     child is alive, let the child write to the heap (copy-on-write
//     copies, Q5), make it exit, reap it with wait(), print again and
//     check that free_pages is back at its pre-fork value;
//  4. give the heap back and check free_pages returns to the baseline;
//  5. repeat allocate/fork/exit/wait 20 times and check for leaks.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "meminfo.h"

#define PGSIZE  4096
#define NPAGES  64           // heap pages allocated by the test
#define CYCLES  20           // fork/exit cycles in the leak test

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
  printf(1, "%s\n  total_pages %d  free_pages %d  proc_pages(sz/PGSIZE) %d"
         "  pt_pages %d (user %d)  resident %d  shared %d\n", label,
         m->total_pages, m->free_pages, m->proc_pages, m->pt_pages,
         m->pt_user_pages, m->resident_pages, m->shared_pages);
}

int
main(int argc, char *argv[])
{
  struct meminfo base, grown, child, alive, reaped, after, m;
  char *heap;
  int i, pid, go[2], rep[2], cyc;
  int before_cycles;

  printf(1, "\n=== memtest: Q2 physical memory statistics ===\n");
  // Pipes are created before the baseline so that their buffers are
  // already part of it.
  if(pipe(go) < 0 || pipe(rep) < 0){
    printf(1, "pipe failed\n");
    exit();
  }

  stats("\n[1] baseline:", &base);
  check(base.free_pages > 0 && base.free_pages < base.total_pages,
        "0 < free_pages < total_pages");
  check(base.pt_pages >= 2, "page directory + at least one page table");

  heap = sbrk(NPAGES * PGSIZE);
  if(heap == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  for(i = 0; i < NPAGES; i++)
    heap[i * PGSIZE] = (char)i;     // touch: frames are allocated now
  stats("\n[2] after sbrk(64 pages) and touching every page:", &grown);
  printf(1, "  free_pages dropped by %d (64 data pages + %d new page-table "
         "pages)\n", base.free_pages - grown.free_pages,
         grown.pt_pages - base.pt_pages);
  check(grown.proc_pages == base.proc_pages + NPAGES,
        "proc_pages grew by 64");
  check(base.free_pages - grown.free_pages ==
        NPAGES + (grown.pt_pages - base.pt_pages),
        "free_pages dropped by exactly the touched pages + page tables");

  printf(1, "\n[3] fork a child\n");
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    stats("  child, right after fork:", &child);
    for(i = 0; i < NPAGES; i++)
      heap[i * PGSIZE] = (char)(i + 1);   // copy-on-write copies
    stats("  child, after writing its 64 heap pages:", &m);
    printf(1, "  child: its writes consumed %d more pages (copies)\n",
           child.free_pages - m.free_pages);
    write(rep[1], "r", 1);
    read(go[0], &i, 1);                   // stay alive until told to exit
    exit();
  }
  read(rep[0], &i, 1);                    // child has done its writes
  stats("  parent, child alive:", &alive);
  printf(1, "  fork + child's writes use %d pages in total while the child "
         "is alive\n", grown.free_pages - alive.free_pages);
  check(alive.free_pages < grown.free_pages,
        "free_pages decreases while the forked child exists");
  write(go[1], "g", 1);
  wait();
  stats("  parent, after child exit + wait():", &reaped);
  check(reaped.free_pages == grown.free_pages,
        "free_pages back to the pre-fork value after the child is reaped");
  for(i = 0; i < NPAGES; i++)
    if(heap[i * PGSIZE] != (char)i)
      break;
  check(i == NPAGES, "parent's heap data unchanged by the child's writes");

  printf(1, "\n[4] release the heap with sbrk(-64 pages)\n");
  sbrk(-NPAGES * PGSIZE);
  stats("  after shrinking:", &after);
  printf(1, "  (page-table pages are kept by xv6 until exit: pt_pages %d -> "
         "%d)\n", base.pt_pages, after.pt_pages);
  check(after.free_pages == base.free_pages - (after.pt_pages - base.pt_pages),
        "free_pages back to the baseline (apart from retained page tables)");
  check(after.proc_pages == base.proc_pages, "proc_pages back to baseline");

  printf(1, "\n[5] leak test: %d x (sbrk + touch, fork, child writes, exit, "
         "wait, shrink)\n", CYCLES);
  getmeminfo(&m);
  before_cycles = m.free_pages;
  for(cyc = 0; cyc < CYCLES; cyc++){
    heap = sbrk(16 * PGSIZE);
    if(heap == (char*)-1){
      printf(1, "sbrk failed\n");
      exit();
    }
    for(i = 0; i < 16; i++)
      heap[i * PGSIZE] = 1;
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      for(i = 0; i < 16; i++)
        heap[i * PGSIZE] = 2;
      exit();
    }
    wait();
    sbrk(-16 * PGSIZE);
  }
  stats("  after the cycles:", &m);
  printf(1, "  free_pages before %d, after %d\n", before_cycles, m.free_pages);
  check(m.free_pages == before_cycles, "no pages leaked over the cycles");

  printf(1, "\nmemtest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
