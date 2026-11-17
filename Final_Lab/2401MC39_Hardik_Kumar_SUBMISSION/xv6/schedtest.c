#include "types.h"
#include "stat.h"
#include "user.h"

void
child_work(int prio)
{
  volatile int x = 0;
  volatile int i;

  // CPU-bound loop long enough to span multiple timer ticks
  for(i = 0; i < 50000000; i++){
    x += i;
  }

  printf(1, "priority %d finished\n", prio);
  exit();
}

int
main(void)
{
  int pids[3];
  int prios[3] = {5, 10, 15};
  int i;

  printf(1, "schedtest: starting priority scheduler test\n");

  // Parent sets its own priority to 1 (highest)
  set_priority(getpid(), 1);

  // Parent creates three children and sets their priorities
  for(i = 0; i < 3; i++){
    pids[i] = fork();
    if(pids[i] < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pids[i] == 0){
      child_work(prios[i]);
    }
    set_priority(pids[i], prios[i]);
  }

  // Parent waits for all three children
  for(i = 0; i < 3; i++){
    wait();
  }

  printf(1, "Done\n");
  exit();
}
