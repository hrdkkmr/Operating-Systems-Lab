// Lab 9 Q1: print the page table of this process with vmprint(),
// grow the heap by four pages with sbrk(4 * 4096) and print it again,
// and check that exactly four new valid user pages appear.
//
// Because Q2 makes sbrk() lazy, the four pages only receive physical
// frames when they are first touched.  The test therefore prints three
// dumps: at start-up, right after sbrk() (no new pages yet, only sz
// grows) and after writing one byte to each of the four pages (exactly
// four new pages).
#include "types.h"
#include "user.h"

#define PGSIZE 4096
#define NPAGES 4

int
main(int argc, char *argv[])
{
  int before, aftersbrk, aftertouch, i, ok = 1;
  char *p;

  printf(1, "pttest: [1] page table at start-up\n");
  before = vmprint();

  p = sbrk(NPAGES * PGSIZE);
  if(p == (char*)-1){
    printf(1, "pttest: sbrk failed\n");
    exit();
  }
  printf(1, "\npttest: [2] after sbrk(%d * 4096) = 0x%x (lazy: no frames yet)\n",
         NPAGES, (uint)p);
  aftersbrk = vmprint();

  for(i = 0; i < NPAGES; i++)
    p[i * PGSIZE] = 'A' + i;     // first touch -> one page fault per page
  printf(1, "\npttest: [3] after touching the %d new pages\n", NPAGES);
  aftertouch = vmprint();

  printf(1, "\npttest: valid user pages: start %d, after sbrk %d, after touch %d\n",
         before, aftersbrk, aftertouch);
  if(aftersbrk != before){
    printf(1, "pttest: [FAIL] sbrk mapped pages eagerly\n");
    ok = 0;
  }
  if(aftertouch - before != NPAGES){
    printf(1, "pttest: [FAIL] %d new pages, expected %d\n",
           aftertouch - before, NPAGES);
    ok = 0;
  } else
    printf(1, "pttest: [PASS] exactly %d new pages appeared\n", NPAGES);
  printf(1, ok ? "pttest: ALL CHECKS PASSED\n" : "pttest: SOME CHECKS FAILED\n");
  exit();
}
