#include "types.h"
#include "stat.h"
#include "user.h"

#define ITERATIONS 10

struct shared_data {
  volatile int flag[2];
  volatile int turn;
  volatile int shared_counter;
};

static void
delay(void)
{
  volatile int i;
  for(i = 0; i < 100000; i++)
    ;
}

int
main(void)
{
  struct shared_data *s;
  int pid, id, other, i;

  s = (struct shared_data*)shm_get();
  if(s == 0){
    printf(1, "shm_get failed\n");
    exit();
  }

  // Initialize BEFORE fork, so both processes start with valid state.
  s->flag[0] = 0;
  s->flag[1] = 0;
  s->turn = 0;
  s->shared_counter = 0;

  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }

  id = (pid == 0) ? 1 : 0;
  other = 1 - id;

  for(i = 0; i < ITERATIONS; i++){
    s->flag[id] = 1;
    s->turn = other;

    while(s->flag[other] && s->turn == other)
      ;

    s->shared_counter++;
    printf(1, "Process %d in CS, counter = %d\n",
           id, s->shared_counter);

    s->flag[id] = 0;
    delay();
  }

  if(id == 0){
    wait();
    printf(1, "Final counter = %d (expected %d)\n",
           s->shared_counter, 2 * ITERATIONS);
  }

  exit();
}
