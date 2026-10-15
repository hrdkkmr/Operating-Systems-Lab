// Lab 8 Q3: dump the page table with vmprint().
//
// Calls vmprint() at start-up, after sbrk(8 * 4096) and after the heap
// is shrunk with sbrk(-4096).  Because sbrk() is lazy in this kernel
// (Q4), the 8 new pages have no PTEs until they are touched, so the
// program dumps the table once more between growing and shrinking:
//   dump 1: start-up
//   dump 2: right after sbrk(8*4096)       -> no new PTEs yet (lazy)
//   dump 3: after touching the 8 pages     -> 8 new PTEs
//   dump 4: after sbrk(-4096)              -> the last one is gone
// In every dump the kernel prints the stack guard page (present, PTE_U
// clear) and compares the PTE_U page count with sz/PGSIZE - 1.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "meminfo.h"

#define PGSIZE 4096

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

static void
dump(char *title, struct meminfo *m)
{
  printf(1, "\n---------- %s ----------\n", title);
  if(vmprint() < 0)
    printf(1, "vmprint failed\n");
  if(getmeminfo(m) < 0){
    printf(1, "getmeminfo failed\n");
    exit();
  }
  printf(1, "(getmeminfo: sz/PGSIZE %d, resident user pages %d)\n",
         m->proc_pages, m->resident_pages);
}

int
main(int argc, char *argv[])
{
  struct meminfo m0, m1, m2, m3;
  char *old, *brk;
  int i;

  printf(1, "\n=== vmprinttest: Q3 page-table dump ===\n");
  dump("dump 1: start-up", &m0);
  // At start-up everything below sz was mapped by exec(): text/data,
  // guard page (no PTE_U) and stack.  The resident count includes only
  // PTE_U pages below KERNBASE.
  check(m0.resident_pages == m0.proc_pages - 1,
        "user-accessible pages == sz/PGSIZE - 1 (guard page)");

  old = sbrk(8 * PGSIZE);
  if(old == (char*)-1){
    printf(1, "sbrk(8*4096) failed\n");
    exit();
  }
  printf(1, "\nsbrk(8*4096) returned old break 0x%x\n", (uint)old);
  dump("dump 2: after sbrk(8*4096), pages not touched yet (lazy)", &m1);
  check(m1.proc_pages == m0.proc_pages + 8, "sz grew by 8 pages");
  check(m1.resident_pages == m0.resident_pages,
        "no new PTEs yet: lazy allocation maps pages on first touch");

  for(i = 0; i < 8; i++)
    old[i * PGSIZE] = 1;
  dump("dump 3: after touching the 8 new pages", &m2);
  check(m2.resident_pages == m0.resident_pages + 8,
        "8 new user PTEs appeared");
  check(m2.resident_pages == m2.proc_pages - 1,
        "user-accessible pages == sz/PGSIZE - 1 again");

  if(sbrk(-PGSIZE) == (char*)-1){
    printf(1, "sbrk(-4096) failed\n");
    exit();
  }
  brk = sbrk(0);
  printf(1, "\nsbrk(-4096): new break 0x%x\n", (uint)brk);
  dump("dump 4: after sbrk(-4096)", &m3);
  check(m3.resident_pages == m2.resident_pages - 1,
        "the PTE of the released page disappeared");
  check(m3.resident_pages == m3.proc_pages - 1,
        "user-accessible pages == sz/PGSIZE - 1");

  printf(1, "\nvmprinttest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
