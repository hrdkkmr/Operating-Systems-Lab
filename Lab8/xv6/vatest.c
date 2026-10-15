// Lab 8 Q1: virtual to physical address translation with va2pa().
//
// Prints the VA -> PA mapping of a global, a local (stack) and a malloc()
// block, checks that unmapped / kernel-only addresses give -1, and uses
// fork() to compare the parent's and child's physical address for the
// same global variable.
//
// Note on fork: this kernel implements copy-on-write fork (Lab 8 Q5), so
// directly after fork() parent and child map the SAME frame (read-only).
// The first write gives the writer a private copy; from then on the two
// processes use different physical addresses for the same virtual
// address, which is what original xv6's copying fork() showed at once.
// The program prints both stages.

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

int global_var = 42;          // in the data segment

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

// Translate and print.  Pointers are passed as int but printed as uint,
// so no address is sign-mangled; va2pa returns the full physical address
// (frame | offset) or -1.
static int
show(char *name, void *p)
{
  int pa = va2pa((int)p);

  if(pa == -1)
    printf(1, "%s: va 0x%x -> pa -1 (not mapped / not user)\n", name, (uint)p);
  else
    printf(1, "%s: va 0x%x -> pa 0x%x\n", name, (uint)p, (uint)pa);
  return pa;
}

int
main(int argc, char *argv[])
{
  int local_var = 7;          // on the user stack
  char *heap;
  char *lazy;
  int pa, pa_parent, pa_shared, fds[2], pid;
  int report[3];

  printf(1, "\n=== vatest: Q1 virtual -> physical translation ===\n");

  printf(1, "\n-- global, local and heap variables --\n");
  pa = show("global_var", &global_var);
  check(pa != -1 && (pa & (PGSIZE-1)) == ((uint)&global_var & (PGSIZE-1)),
        "global translated, page offset preserved");
  pa = show("local_var ", &local_var);
  check(pa != -1 && (pa & (PGSIZE-1)) == ((uint)&local_var & (PGSIZE-1)),
        "local translated, page offset preserved");
  heap = malloc(100);
  if(heap == 0){
    printf(1, "malloc failed\n");
    exit();
  }
  heap[0] = 'x';              // make sure the (lazy) heap page is backed
  pa = show("malloc blk", heap);
  check(pa != -1 && (pa & (PGSIZE-1)) == ((uint)heap & (PGSIZE-1)),
        "malloc block translated, page offset preserved");

  printf(1, "\n-- unmapped and kernel-only addresses --\n");
  pa = show("0x7FFFFFFF", (void*)0x7FFFFFFF);
  check(pa == -1, "va2pa(0x7FFFFFFF) == -1 (unmapped)");
  pa = show("0x80100000", (void*)0x80100000);
  check(pa == -1, "va2pa(kernel text 0x80100000) == -1 (PTE_U clear)");

  printf(1, "\n-- lazily allocated heap page (Q4) --\n");
  lazy = sbrk(PGSIZE);
  if(lazy == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  pa = show("new sbrk page, untouched", lazy);
  check(pa == -1, "untouched sbrk() page has no frame yet");
  lazy[10] = 1;
  pa = show("same page after a write", lazy + 10);
  check(pa != -1, "page backed after first touch");

  printf(1, "\n-- fork: same VA, physical address in parent and child --\n");
  if(pipe(fds) < 0){
    printf(1, "pipe failed\n");
    exit();
  }
  pa_parent = va2pa((int)&global_var);
  printf(1, "parent pid %d before fork: &global_var 0x%x -> pa 0x%x\n",
         getpid(), (uint)&global_var, (uint)pa_parent);
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    // child
    report[0] = (int)&global_var;
    report[1] = va2pa((int)&global_var);      // still shared (COW)
    global_var = 100;                         // write -> private copy
    report[2] = va2pa((int)&global_var);
    printf(1, "child  pid %d: &global_var 0x%x, pa before write 0x%x, "
           "after write 0x%x, value %d\n", getpid(), (uint)report[0],
           (uint)report[1], (uint)report[2], global_var);
    write(fds[1], report, sizeof(report));
    exit();
  }
  close(fds[1]);
  if(read(fds[0], report, sizeof(report)) != sizeof(report)){
    printf(1, "pipe read failed\n");
    exit();
  }
  wait();
  pa_shared = report[1];
  global_var = 200;                           // parent writes its copy
  pa = va2pa((int)&global_var);
  printf(1, "parent pid %d: &global_var 0x%x -> pa 0x%x, value %d\n",
         getpid(), (uint)&global_var, (uint)pa, global_var);
  check(report[0] == (int)&global_var,
        "parent and child use the same virtual address");
  check(pa_shared == pa_parent,
        "right after fork the child shares the parent's frame (copy-on-write)");
  check(report[2] != pa,
        "after writing, parent and child have different physical addresses");
  check(global_var == 200, "parent's value unaffected by the child's write");

  printf(1, "\nvatest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
