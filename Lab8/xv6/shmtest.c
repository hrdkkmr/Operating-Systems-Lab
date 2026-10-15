// Lab 8 Q7 (inferred extension, see submission/FINAL_STATUS.md):
// shared memory between processes.
//
// New system calls:
//   void *shm_open(int key)  attach region key (0..7), creating one zeroed
//                            page on first use; returns its fixed address
//                            (0x7FFF7000 + key*4096) or -1
//   int shm_close(int key)   detach it; the frame is freed when the last
//                            process detaches (exit and exec detach too)
// fork() children inherit attached regions (same frame, not copy-on-
// write).  "shmtest peer" is run with exec() by the test to show that a
// completely new program image finds the region again by its key.

#include "types.h"
#include "stat.h"
#include "user.h"
#include "meminfo.h"

#define KEY 3

static int failures;

static void
check(int ok, char *what)
{
  printf(1, "  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok)
    failures++;
}

static int
freepages(void)
{
  struct meminfo m;

  if(getmeminfo(&m) < 0){
    printf(1, "getmeminfo failed\n");
    exit();
  }
  return m.free_pages;
}

// Run by exec("shmtest", "peer"): open KEY again, check the parent's
// data, add a line and exit (exit detaches the region).
static void
peer(void)
{
  char *p = shm_open(KEY);

  if(p == (char*)-1){
    printf(1, "  peer: shm_open failed\n");
    exit();
  }
  printf(1, "  peer (pid %d, new image after exec): region at 0x%x, pa 0x%x, "
         "reads \"%s\"\n", getpid(), (uint)p, (uint)va2pa((int)p), p + 512);
  strcpy(p + 1024, "written by the exec'ed peer");
  exit();
}

int
main(int argc, char *argv[])
{
  char *p, *q;
  int f0, f1, f2, pid, pa, fds[2];
  int *counter;
  char c;
  char *args[] = { "shmtest", "peer", 0 };

  if(argc > 1 && strcmp(argv[1], "peer") == 0)
    peer();

  printf(1, "\n=== shmtest: Q7 shared memory ===\n");
  if(pipe(fds) < 0){
    printf(1, "pipe failed\n");
    exit();
  }
  f0 = freepages();

  printf(1, "\n[1] create / attach\n");
  p = shm_open(KEY);
  check(p != (char*)-1, "shm_open(3) succeeded");
  if(p == (char*)-1)
    exit();
  pa = va2pa((int)p);
  printf(1, "  region 3 at va 0x%x -> pa 0x%x\n", (uint)p, (uint)pa);
  check(shm_open(KEY) == p, "a second shm_open returns the same address");
  check(shm_open(-1) == (char*)-1 && shm_open(8) == (char*)-1,
        "invalid keys rejected");
  check(p[0] == 0 && p[4095] == 0, "new region is zero-filled");

  printf(1, "\n[2] parent writes, child reads and updates\n");
  strcpy(p, "hello from the parent");
  counter = (int*)(p + 256);
  *counter = 1;
  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }
  if(pid == 0){
    printf(1, "  child: va 0x%x -> pa 0x%x, reads \"%s\", counter %d\n",
           (uint)p, (uint)va2pa((int)p), p, *counter);
    if(strcmp(p, "hello from the parent") != 0)
      exit();
    strcpy(p, "reply from the child");
    *counter += 41;
    write(fds[1], "d", 1);
    exit();
  }
  read(fds[0], &c, 1);
  wait();
  printf(1, "  parent reads \"%s\", counter %d\n", p, *counter);
  check(strcmp(p, "reply from the child") == 0,
        "parent sees the child's update (same physical page)");
  check(*counter == 42, "shared counter updated by the child");

  printf(1, "\n[3] unrelated program image finds the region by key\n");
  strcpy(p + 512, "message for the peer");
  pid = fork();
  if(pid == 0){
    exec("shmtest", args);
    printf(1, "exec failed\n");
    exit();
  }
  wait();
  printf(1, "  parent reads \"%s\"\n", p + 1024);
  check(strcmp(p + 1024, "written by the exec'ed peer") == 0,
        "exec'ed peer read and wrote the same region");

  printf(1, "\n[4] lifetime: frame stays alive while anyone maps it\n");
  q = shm_open(5);
  strcpy(q, "region 5 data");
  pid = fork();
  if(pid == 0){
    read(fds[0], &c, 1);                // wait until the parent detached
    printf(1, "  child: parent detached, region 5 still reads \"%s\"\n", q);
    if(strcmp(q, "region 5 data") != 0)
      exit();
    write(fds[1], "k", 1);
    exit();
  }
  check(shm_close(5) == 0, "parent shm_close(5)");
  write(fds[1], "g", 1);
  wait();
  c = 0;
  read(fds[0], &c, 1);
  check(c == 'k', "child still saw valid data after the parent detached");

  printf(1, "\n[5] detach and cleanup\n");
  f1 = freepages();
  check(shm_close(KEY) == 0, "shm_close(3) == 0");
  check(shm_close(KEY) == -1, "second shm_close(3) == -1");
  f2 = freepages();
  printf(1, "  free pages: start %d, before close %d, after close %d\n",
         f0, f1, f2);
  check(f2 == f1 + 1, "last detach freed the region's frame");
  check(f2 == f0 - 1, "back to the start value (minus 1 retained "
        "page-table page for the region's 4 MB slot)");
  check(va2pa((int)p) == -1, "region is unmapped after shm_close");

  printf(1, "\nshmtest: %s (%d failure(s))\n",
         failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", failures);
  exit();
}
