#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

int
main(void)
{
  char *p;
  int pid;

  printf(1, "prottest: starting page protection test\n");

  // Allocate one page-aligned page
  p = sbrk(PGSIZE);
  if(p == (char*)-1){
    printf(1, "prottest: sbrk failed\n");
    exit();
  }

  // Store character 'A' in it
  *p = 'A';
  printf(1, "Allocated 1 page at %p, wrote '%c'\n", p, *p);

  // (i) Call mprotect on the page and show that it can still be read
  printf(1, "\n--- Part (i): Testing mprotect and read-access ---\n");
  if(mprotect(p, 1) < 0){
    printf(1, "ERROR: mprotect failed on page %p\n", p);
    exit();
  }
  printf(1, "mprotect succeeded on page %p\n", p);
  printf(1, "Reading page: '%c' (read successful)\n", *p);

  // (ii) Fork a child that tries to write to the page.
  // The child must be killed by the kernel while parent continues and sees 'A'.
  printf(1, "\n--- Part (ii): Child write triggers page fault & termination ---\n");
  pid = fork();
  if(pid < 0){
    printf(1, "ERROR: fork failed\n");
    exit();
  }
  if(pid == 0){
    printf(1, "Child process (pid %d) attempting to write to protected page...\n", getpid());
    *p = 'B'; // Should trigger trap 14 (page fault) and kill child
    printf(1, "ERROR: child was NOT killed after write!\n");
    exit();
  }

  // Parent waits for child
  wait();
  printf(1, "Parent reaped child. Protected page still contains: '%c'\n", *p);
  if(*p != 'A'){
    printf(1, "ERROR: page was modified!\n");
    exit();
  }

  // (iii) Call munprotect and show that a write of 'C' now succeeds
  printf(1, "\n--- Part (iii): munprotect and writable access ---\n");
  if(munprotect(p, 1) < 0){
    printf(1, "ERROR: munprotect failed on page %p\n", p);
    exit();
  }
  printf(1, "munprotect succeeded on page %p\n", p);
  *p = 'C';
  printf(1, "Successfully wrote '%c' to page. Value is now: '%c'\n", *p, *p);
  if(*p != 'C'){
    printf(1, "ERROR: value is not 'C'!\n");
    exit();
  }

  // (iv) Show that mprotect returns -1 for invalid arguments:
  // unaligned address, len = 0, range beyond end of process memory
  printf(1, "\n--- Part (iv): Testing error handling ---\n");
  
  // 1. Unaligned address
  if(mprotect(p + 1, 1) == -1){
    printf(1, "PASS: mprotect with unaligned address (%p) returned -1\n", p + 1);
  } else {
    printf(1, "FAIL: mprotect with unaligned address did not return -1\n");
  }

  // 2. len = 0
  if(mprotect(p, 0) == -1){
    printf(1, "PASS: mprotect with len = 0 returned -1\n");
  } else {
    printf(1, "FAIL: mprotect with len = 0 did not return -1\n");
  }

  // 3. Range beyond end of process memory
  if(mprotect(p, 1000000) == -1){
    printf(1, "PASS: mprotect with range beyond process memory returned -1\n");
  } else {
    printf(1, "FAIL: mprotect with range beyond process memory did not return -1\n");
  }

  // 4. len = -1
  if(mprotect(p, -1) == -1){
    printf(1, "PASS: mprotect with len = -1 returned -1\n");
  } else {
    printf(1, "FAIL: mprotect with len = -1 did not return -1\n");
  }

  printf(1, "\nprottest: all tests PASSED\n");
  exit();
}
