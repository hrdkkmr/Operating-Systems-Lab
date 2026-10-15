// Lab 8 Q5 (inferred extension, see submission/FINAL_STATUS.md):
// copy-on-write fork.
//
//  1. the parent fills NPAGES heap pages; fork() shares them: same
//     physical frames in parent and child, read-only with PTE_COW, and the
//     fork costs only page tables + kernel stack, not NPAGES copies;
//  2. the child's first write to a page copies just that page;
//  3. both processes then see independent values;
//  4. when one sharer exits, the other keeps the frame and its next
//     write reuses it without copying (reference count 1);
//  5. the kernel writing into a shared page (read() into a COW buffer)
//     also copies it first;
//  6. exiting and reaping returns every page; 30 fork/write/exit cycles
//     leak nothing.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "meminfo.h"

#define PGSIZE  4096
#define NPAGES  32
#define CYCLES  30

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
  printf(1, "%s free_pages %d  resident %d  shared(refcount>1) %d\n",
         label, m->free_pages, m->resident_pages, m->shared_pages);
}

int
main(int argc, char *argv[])
{
  struct meminfo m0, m1, c0, c1, c2, m2, m3, m4;
  char *heap;
  int i, pid, pa_parent, pa_child0, ok, fds[2], go[2], cyc;
  int rep[8];

  printf(1, "\n=== cowtest: Q5 copy-on-write fork ===\n");
  if(pipe(fds) < 0 || pipe(go) < 0){
    printf(1, "pipe failed\n");
    exit();
  }
  heap = sbrk(NPAGES * PGSIZE);
  if(heap == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  for(i = 0; i < NPAGES; i++)
    heap[i * PGSIZE] = 'p';
  pa_parent = va2pa((int)heap);

  printf(1, "\n[1] fork shares the pages\n");
  stats("  parent before fork:      ", &m0);
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    getmeminfo(&c0);
    pa_child0 = va2pa((int)heap);
    heap[0] = 'c';                       // [2] first write: one copy
    getmeminfo(&c1);
    rep[0] = pa_child0;
    rep[1] = va2pa((int)heap);
    rep[2] = c0.free_pages - c1.free_pages;
    rep[3] = c0.shared_pages;
    for(i = 1; i < NPAGES; i++)
      heap[i * PGSIZE] = 'c';
    getmeminfo(&c2);
    rep[4] = c1.free_pages - c2.free_pages;
    rep[5] = c0.free_pages;
    rep[6] = c0.pt_pages;
    write(fds[1], rep, sizeof(rep));
    read(go[0], &i, 1);                  // let the parent look first
    for(i = 0; i < NPAGES; i++)
      if(heap[i * PGSIZE] != 'c')
        break;
    if(i != NPAGES)
      printf(1, "  child: own data lost!\n");
    exit();
  }
  if(read(fds[0], rep, sizeof(rep)) != sizeof(rep)){
    printf(1, "pipe read failed\n");
    exit();
  }
  printf(1, "  child right after fork:   free_pages %d  shared %d\n",
         rep[5], rep[3]);
  // Every xv6 process has its own page directory and its own copy of
  // the kernel page tables (setupkvm), plus a kernel stack.  Anything
  // beyond that is a user page copied by a write since the fork (both
  // processes write their stacks before the child measures).
  printf(1, "  fork() used %d pages = %d page-directory/page-table pages + "
         "1 kernel stack + %d copied page(s); an eager fork would also "
         "copy all %d resident user pages\n", m0.free_pages - rep[5],
         rep[6], m0.free_pages - rep[5] - rep[6] - 1, m0.resident_pages);
  check(rep[0] == pa_parent, "child maps the same frame as the parent");
  check(rep[3] >= NPAGES, "all heap pages are shared after fork");
  check(m0.free_pages - rep[5] - rep[6] - 1 <= 2,
        "fork copied no heap page (at most the 2 stack pages written since)");

  printf(1, "\n[2] child writes one page\n");
  printf(1, "  child: pa before write 0x%x, after write 0x%x, pages used %d\n",
         (uint)rep[0], (uint)rep[1], rep[2]);
  check(rep[1] != rep[0], "write gave the child a private frame");
  check(rep[2] == 1, "exactly one page was copied");
  printf(1, "  child wrote the other %d pages: %d more copies\n",
         NPAGES - 1, rep[4]);
  check(rep[4] == NPAGES - 1, "one copy per written page");

  printf(1, "\n[3] independent values\n");
  for(ok = 1, i = 0; i < NPAGES; i++)
    if(heap[i * PGSIZE] != 'p')
      ok = 0;
  check(ok, "parent still sees its own data ('p') in every page");
  check(va2pa((int)heap) == pa_parent, "parent kept the original frame");
  stats("  parent, child alive:     ", &m1);
  write(go[1], "g", 1);
  wait();
  stats("  parent, child reaped:    ", &m2);
  check(m2.free_pages == m0.free_pages,
        "free_pages back to the pre-fork value");

  printf(1, "\n[4] last sharer reuses the frame without copying\n");
  heap[0] = 'P';
  getmeminfo(&m3);
  check(m3.free_pages == m2.free_pages && va2pa((int)heap) == pa_parent,
        "write after the child exited: no copy, same frame");

  printf(1, "\n[5] kernel write into a shared page (read() into COW buffer)\n");
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    // heap page 1 is shared with the parent; the kernel copies it before
    // read() stores into it.
    write(fds[1], "KERNEL", 6);
    if(read(fds[0], heap + PGSIZE, 6) != 6 || heap[PGSIZE] != 'K')
      printf(1, "  child: read into COW page failed\n");
    rep[0] = va2pa((int)(heap + PGSIZE));
    write(go[1], rep, 4);
    exit();
  }
  read(go[0], rep, 4);
  wait();
  check(heap[PGSIZE] == 'p', "parent's page untouched by the child's read()");
  check(rep[0] != va2pa((int)(heap + PGSIZE)),
        "child's read() buffer got a private frame");

  printf(1, "\n[6] leak test: %d x (fork, child writes all pages, exit, wait)\n",
         CYCLES);
  getmeminfo(&m3);
  for(cyc = 0; cyc < CYCLES; cyc++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      for(i = 0; i < NPAGES; i++)
        heap[i * PGSIZE] = 'x';
      exit();
    }
    // parent writes half of the pages while the child may still run
    for(i = 0; i < NPAGES; i += 2)
      heap[i * PGSIZE] = 'p';
    wait();
  }
  stats("  after the cycles:        ", &m4);
  check(m4.free_pages == m3.free_pages, "no pages leaked");
  check(m4.shared_pages == 0, "no page is still marked shared");

  printf(1, "\ncowtest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
