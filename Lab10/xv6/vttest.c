// vttest.c — Lab 10 Q2: Virtual-to-Physical Address Translation
//
// Demonstrates the vtop() system call which uses walkpgdir() in vm.c to
// translate user virtual addresses into physical addresses.
//
// The program:
//   1. Tests vtop() on the address of a stack variable.
//   2. Allocates a heap page with sbrk(), touches it, then calls vtop().
//   3. Shows that multiple consecutive virtual addresses map to consecutive
//      physical addresses within the same page (same frame, different offset).
//   4. Demonstrates that a virtual address beyond the process's address space
//      returns -1 (invalid / unmapped).

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

int
main(void)
{
  char *heap;
  int   pa, pa2;
  int   stack_var = 42;

  printf(1, "=== vttest: Virtual-to-Physical Address Translation ===\n\n");

  // --- Test 1: stack variable ---
  printf(1, "[1] Stack variable address:\n");
  printf(1, "    VA = 0x%x\n", (uint)&stack_var);
  pa = vtop((int)&stack_var);
  if(pa == -1)
    printf(1, "    PA = unmapped / not present  [FAIL]\n\n");
  else
    printf(1, "    PA = 0x%x  [PASS]\n\n", (uint)pa);

  // --- Test 2: heap allocation ---
  heap = sbrk(PGSIZE);
  if(heap == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  // Touch the page to force the lazy allocator to map it.
  heap[0] = 'A';
  heap[PGSIZE / 2] = 'B';

  printf(1, "[2] Heap page (base of page, offset 0):\n");
  printf(1, "    VA = 0x%x\n", (uint)heap);
  pa = vtop((int)heap);
  if(pa == -1)
    printf(1, "    PA = unmapped / not present  [FAIL]\n\n");
  else
    printf(1, "    PA = 0x%x  [PASS]\n\n", (uint)pa);

  // --- Test 3: mid-page offset — same frame, different offset ---
  printf(1, "[3] Same heap page at offset +%d:\n", PGSIZE / 2);
  printf(1, "    VA = 0x%x\n", (uint)(heap + PGSIZE / 2));
  pa2 = vtop((int)(heap + PGSIZE / 2));
  if(pa2 == -1){
    printf(1, "    PA = unmapped  [FAIL]\n\n");
  } else {
    printf(1, "    PA = 0x%x\n", (uint)pa2);
    // The physical frame must be the same; only the offset differs.
    if((pa & ~0xFFF) == (pa2 & ~0xFFF))
      printf(1, "    Same physical frame as test 2  [PASS]\n\n");
    else
      printf(1, "    Different frame — unexpected  [FAIL]\n\n");
  }

  // --- Test 4: unmapped / out-of-bounds virtual address ---
  // Use an address well above the current process size.
  printf(1, "[4] Unmapped virtual address (0x7fff0000):\n");
  pa = vtop(0x7fff0000);
  if(pa == -1)
    printf(1, "    vtop returned -1 (not mapped)  [PASS]\n\n");
  else
    printf(1, "    vtop returned 0x%x — should have been -1  [FAIL]\n\n",
           (uint)pa);

  printf(1, "=== vttest complete ===\n");
  exit();
}
