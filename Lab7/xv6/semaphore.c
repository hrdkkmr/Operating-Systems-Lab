#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"

#define MAX_SEMAPHORES 10

struct semaphore {
  int value;
  int active;
  struct spinlock lk;
};

struct {
  struct spinlock lock;
  struct semaphore sems[MAX_SEMAPHORES];
} sem_table;

void
seminit(void)
{
  initlock(&sem_table.lock, "sem_table");
  for(int i = 0; i < MAX_SEMAPHORES; i++){
    initlock(&sem_table.sems[i].lk, "semaphore");
    sem_table.sems[i].active = 0;
    sem_table.sems[i].value = 0;
  }
}

int
sem_init(int sem_id, int value)
{
  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return -1;

  acquire(&sem_table.lock);
  acquire(&sem_table.sems[sem_id].lk);
  sem_table.sems[sem_id].value = value;
  sem_table.sems[sem_id].active = 1;
  release(&sem_table.sems[sem_id].lk);
  release(&sem_table.lock);
  return 0;
}

int
sem_wait(int sem_id)
{
  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return -1;

  struct semaphore *s = &sem_table.sems[sem_id];
  acquire(&s->lk);
  s->value--;
  while(s->value < 0){
    sleep(s, &s->lk);
  }
  release(&s->lk);
  return 0;
}

int
sem_signal(int sem_id)
{
  if(sem_id < 0 || sem_id >= MAX_SEMAPHORES)
    return -1;

  struct semaphore *s = &sem_table.sems[sem_id];
  acquire(&s->lk);
  s->value++;
  wakeup(s);
  release(&s->lk);
  return 0;
}