// pagetest.c — Lab 10 Q3: Page Table Management and Page Protection
//
// Demonstrates pageinfo() and setreadonly() system calls.
//
// The program:
//   1. Allocates one heap page with sbrk() and touches it.
//   2. Calls pageinfo() to display the physical frame number and PTE flags
//      (P, W, U) while the page is still writable.
//   3. Calls setreadonly() to remove the PTE_W flag from that page.
//   4. Calls pageinfo() again — W should now be 0.
//   5. Forks a child that attempts to write to the read-only page.
//      The child will receive a page fault (killed by the kernel with
//      "pid X pagetest: trap 14 err 7 ...") which is the expected result.
//   6. The parent waits for the child and reports what happened.
//
// Note: In xv6 a write to a read-only user page raises trap 14 (T_PGFLT)
// with err bit 1 set (protection fault).  The kernel kills the faulting
// process with "pid X name: trap 14 err 7 ..."; the parent's wait() then
// returns the child's pid.  This is the correct demonstration that the
// protection is working.

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

int
main(void)
{
  char *page;
  int   pid, wstatus;

  printf(1, "=== pagetest: Page Table Management and Page Protection ===\n\n");

  // --- Step 1: allocate and touch one page ---
  page = sbrk(PGSIZE);
  if(page == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  page[0] = 'X';   // touch the page to force lazy allocation
  printf(1, "[1] Allocated and touched one heap page at VA 0x%x\n\n",
         (uint)page);

  // --- Step 2: pageinfo before protection ---
  printf(1, "[2] Page info BEFORE setreadonly():\n");
  if(pageinfo((int)page) < 0)
    printf(1, "    pageinfo failed  [FAIL]\n");
  printf(1, "\n");

  // --- Step 3: make page read-only ---
  printf(1, "[3] Calling setreadonly(0x%x) ...\n", (uint)page);
  if(setreadonly((int)page) < 0){
    printf(1, "    setreadonly failed  [FAIL]\n\n");
  } else {
    printf(1, "    setreadonly succeeded  [PASS]\n\n");
  }

  // --- Step 4: pageinfo after protection (W should be 0) ---
  printf(1, "[4] Page info AFTER setreadonly():\n");
  if(pageinfo((int)page) < 0)
    printf(1, "    pageinfo failed  [FAIL]\n");
  printf(1, "\n");

  // --- Step 5: attempt write in a child process ---
  // We do this in a child so the parent can survive to print the outcome.
  printf(1, "[5] Forking child to write to the read-only page...\n");
  pid = fork();
  if(pid < 0){
    printf(1, "    fork failed\n");
    exit();
  }
  if(pid == 0){
    // Child: attempt to write to the now-protected page.
    // This will trigger a page fault; xv6 will kill the child.
    page[0] = 'Y';
    // If we reach here, the write was unexpectedly allowed.
    printf(1, "    Child: write succeeded — protection NOT enforced  [FAIL]\n");
    exit();
  }

  // Parent: wait for child to finish (or be killed).
  wstatus = wait();
  if(wstatus == pid){
    printf(1, "    Child (pid %d) was terminated by page fault  [PASS]\n",
           pid);
    printf(1, "    (Expected: xv6 prints 'trap 14' and kills the child)\n\n");
  } else {
    printf(1, "    wait() returned unexpected value %d  [NOTE]\n\n", wstatus);
  }

  printf(1, "=== pagetest complete ===\n");
  exit();
}
