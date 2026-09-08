#include "types.h"
#include "stat.h"
#include "user.h"

#define NPHIL 5
#define CYCLES 5

struct table {
  volatile int completed[NPHIL];
};

static void
delay(void)
{
  volatile int i;
  for(i = 0; i < 100000; i++)
    ;
}

static void
philosopher(struct table *t, int id)
{
  int left, right, first, second, cycle;

  left = id;
  right = (id + 1) % NPHIL;

  // Resource ordering: acquire the lower-numbered fork first.
  if(left < right){
    first = left;
    second = right;
  } else {
    first = right;
    second = left;
  }

  for(cycle = 0; cycle < CYCLES; cycle++){
    printf(1, "Philosopher %d: THINKING\n", id);
    delay();

    printf(1, "Philosopher %d: HUNGRY\n", id);

    sem_wait(first);
    printf(1, "Philosopher %d: picked fork %d\n", id, first);

    sem_wait(second);
    printf(1, "Philosopher %d: picked fork %d\n", id, second);

    printf(1, "Philosopher %d: EATING\n", id);
    delay();

    printf(1, "Philosopher %d: finished eating\n", id);

    sem_signal(second);
    sem_signal(first);

    t->completed[id]++;
  }

  printf(1, "Philosopher %d: completed all %d cycles\n", id, CYCLES);
  exit();
}

int
main(void)
{
  struct table *t;
  int i, pid, children;

  t = (struct table*)shm_get();
  if(t == 0){
    printf(1, "shm_get failed\n");
    exit();
  }

  for(i = 0; i < NPHIL; i++)
    t->completed[i] = 0;

  for(i = 0; i < NPHIL; i++)
    sem_init(i, 1);

  children = 0;

  for(i = 0; i < NPHIL; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0)
      philosopher(t, i);
    children++;
  }

  for(i = 0; i < children; i++)
    wait();

  printf(1, "Dining Philosophers completed successfully\n");
  for(i = 0; i < NPHIL; i++)
    printf(1, "Philosopher %d completed %d cycles\n", i, t->completed[i]);

  exit();
}
