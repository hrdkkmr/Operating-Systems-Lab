// memtest.c — Lab 10 Q1: Physical Memory Allocation Tracking
//
// Demonstrates the memstats() system call.  The program:
//   1. Displays memory statistics before allocating anything.
//   2. Allocates 16 pages (16 * 4096 = 65536 bytes) with sbrk().
//   3. Touches every page so the lazy allocator actually maps them.
//   4. Displays updated statistics showing the rise in allocated pages.
//   5. Releases the pages with sbrk(-16*4096).
//   6. Displays final statistics showing the freed pages.

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE  4096
#define NPAGES  16

int
main(void)
{
  char *p;
  int  i, before, after, final_val;

  printf(1, "=== memtest: Physical Memory Allocation Tracking ===\n\n");

  // --- Snapshot 1: before allocation ---
  printf(1, "[1] Statistics BEFORE allocation:\n");
  before = memstats();
  printf(1, "    (memstats returned current pages in use = %d)\n\n", before);

  // --- Allocate NPAGES * PGSIZE bytes ---
  p = sbrk(NPAGES * PGSIZE);
  if(p == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  printf(1, "[2] Allocated %d pages (%d bytes) via sbrk()\n",
         NPAGES, NPAGES * PGSIZE);

  // Touch every page to force the lazy allocator to map each one.
  for(i = 0; i < NPAGES; i++)
    p[i * PGSIZE] = (char)i;

  printf(1, "    Touched all %d pages to force physical allocation.\n\n", NPAGES);

  // --- Snapshot 2: after allocation ---
  printf(1, "[3] Statistics AFTER allocation:\n");
  after = memstats();
  printf(1, "    (memstats returned current pages in use = %d)\n\n", after);

  if(after > before)
    printf(1, "    => Page count increased by %d  [PASS]\n\n", after - before);
  else
    printf(1, "    => Page count did NOT increase as expected  [FAIL]\n\n");

  // --- Release the pages ---
  sbrk(-(NPAGES * PGSIZE));
  printf(1, "[4] Released %d pages via sbrk(-%d)\n\n",
         NPAGES, NPAGES * PGSIZE);

  // --- Snapshot 3: after release ---
  printf(1, "[5] Statistics AFTER release:\n");
  final_val = memstats();
  printf(1, "    (memstats returned current pages in use = %d)\n\n", final_val);

  printf(1, "=== memtest complete ===\n");
  exit();
}
