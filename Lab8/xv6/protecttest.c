// Lab 8 Q6 (inferred extension, see submission/FINAL_STATUS.md):
// memory protection.
//
// New system calls (len is a number of pages, addr must be page aligned
// and the range must lie inside the process size):
//   int mprotect(void *addr, int len)    make the pages read-only
//   int munprotect(void *addr, int len)  make them writable again
// Every faulting access is done in a child process; the child reports
// "survived" through a pipe only if the access did not kill it, so the
// test keeps running after each fault.

#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

static int failures;
static char *page;            // heap page used by most tests

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

// 1 if fn() killed the child, 0 if the child survived it.
static int
killed_in_child(void (*fn)(void))
{
  int fds[2], pid;
  char c;

  if(pipe(fds) < 0 || (pid = fork()) < 0){
    printf(1, "pipe/fork failed\n");
    exit();
  }
  if(pid == 0){
    close(fds[0]);
    fn();
    write(fds[1], "s", 1);
    exit();
  }
  close(fds[1]);
  wait();
  c = 0;
  read(fds[0], &c, 1);
  close(fds[0]);
  return c != 's';
}

static void
write_page(void)
{
  printf(1, "  child: writing protected page 0x%x...\n", (uint)page);
  page[0] = 'X';
}

static void
read_page(void)
{
  if(page[0] != 'A')
    exit();
}

static void
munprotect_then_write(void)
{
  if(munprotect(page, 1) < 0)
    exit();
  page[0] = 'C';              // copy-on-write copy, now writable
  if(page[0] != 'C')
    exit();
}

static void
write_guard(void)
{
  int local;
  char *guard = (char*)(((uint)&local & ~(PGSIZE-1)) - PGSIZE);

  printf(1, "  child: writing stack guard page 0x%x...\n", (uint)guard);
  *guard = 1;
}

static void
read_guard(void)
{
  int local;
  char *guard = (char*)(((uint)&local & ~(PGSIZE-1)) - PGSIZE);

  printf(1, "  child: reading stack guard page 0x%x...\n", (uint)guard);
  if(*(volatile char*)guard == 0x55)
    printf(1, "?");
}

static void
write_kernel(void)
{
  printf(1, "  child: writing kernel memory 0x80100000...\n");
  *(volatile char*)0x80100000 = 0;
}

static void
write_text(void)
{
  printf(1, "  child: writing its own code at 0x%x...\n", (uint)write_text);
  *(volatile char*)write_text = 0xC3;
}

int
main(int argc, char *argv[])
{
  int fds[2], n;
  char *stack_page, *text;
  int local;

  printf(1, "\n=== protecttest: Q6 memory protection ===\n");
  page = sbrk(2 * PGSIZE);
  if(page == (char*)-1){
    printf(1, "sbrk failed\n");
    exit();
  }
  page = (char*)(((uint)page + PGSIZE - 1) & ~(PGSIZE - 1));
  page[0] = 'A';

  printf(1, "\n[1] argument checking\n");
  check(mprotect(page + 1, 1) == -1, "unaligned address rejected");
  check(mprotect(page, 0) == -1, "len 0 rejected");
  check(mprotect(page, -1) == -1, "negative len rejected");
  check(mprotect(sbrk(0), 1) == -1, "range beyond sz rejected");
  check(mprotect((void*)0x80000000, 1) == -1, "kernel address rejected");
  stack_page = (char*)((uint)&local & ~(PGSIZE-1));
  check(mprotect(stack_page - PGSIZE, 1) == -1,
        "guard page (no PTE_U) rejected");

  printf(1, "\n[2] read-only page\n");
  check(mprotect(page, 1) == 0, "mprotect(page, 1) == 0");
  check(page[0] == 'A', "protected page is still readable");
  check(killed_in_child(read_page) == 0, "a child can read it too");
  check(killed_in_child(write_page) == 1, "writing it kills the process");
  check(page[0] == 'A', "contents unchanged");

  printf(1, "\n[3] kernel access to the protected page\n");
  if(pipe(fds) < 0){
    printf(1, "pipe failed\n");
    exit();
  }
  write(fds[1], "zz", 2);
  n = read(fds[0], page, 2);
  check(n == -1, "read() into the protected page fails with -1");
  check(page[0] == 'A', "...and does not modify it");
  check(write(fds[1], page, 1) == 1, "write() from the protected page works");
  close(fds[0]);
  close(fds[1]);

  printf(1, "\n[4] protection is inherited by fork() and survives COW\n");
  check(killed_in_child(write_page) == 1,
        "child (shares the page copy-on-write) is killed on write");
  check(killed_in_child(munprotect_then_write) == 0,
        "child can munprotect its copy and then write it");
  check(page[0] == 'A', "parent's page unchanged by the child");
  check(killed_in_child(write_page) == 1, "parent's page is still protected");

  printf(1, "\n[5] munprotect\n");
  check(munprotect(page, 1) == 0, "munprotect(page, 1) == 0");
  page[0] = 'B';
  check(page[0] == 'B', "page is writable again");
  check(killed_in_child(write_page) == 0, "child can write it too");

  printf(1, "\n[6] protecting program text\n");
  // xv6 maps program text writable; mprotect can fix that.
  text = (char*)((uint)write_text & ~(PGSIZE-1));
  check(mprotect(text, 1) == 0, "mprotect(text page of write_text) == 0");
  check(killed_in_child(write_text) == 1,
        "writing code in a protected text page kills the process");
  check(munprotect(text, 1) == 0, "munprotect(text page) == 0");

  printf(1, "\n[7] pages user code may never touch\n");
  check(killed_in_child(write_guard) == 1, "write to the guard page kills");
  check(killed_in_child(read_guard) == 1, "read of the guard page kills");
  check(killed_in_child(write_kernel) == 1, "write to kernel memory kills");

  printf(1, "\nprotecttest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
