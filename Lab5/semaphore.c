#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

#define MAX_SEMAPHORES 8

struct semaphore {
  struct spinlock lock;
  int value;
};

static struct semaphore semaphores[MAX_SEMAPHORES];

void
semaphore_init(void)
{
  int i;
  for(i = 0; i < MAX_SEMAPHORES; i++){
    initlock(&semaphores[i].lock, "semaphore");
    semaphores[i].value = 0;
  }
}

int
sem_init(int id, int value)
{
  if(id < 0 || id >= MAX_SEMAPHORES || value < 0)
    return -1;
  acquire(&semaphores[id].lock);
  semaphores[id].value = value;
  release(&semaphores[id].lock);
  return 0;
}

int
sem_wait(int id)
{
  if(id < 0 || id >= MAX_SEMAPHORES)
    return -1;

  acquire(&semaphores[id].lock);
  while(semaphores[id].value == 0){
    // sleep() atomically acquires ptable.lock before releasing this lock,
    // so a signal cannot be lost between the test and sleep.
    sleep(&semaphores[id], &semaphores[id].lock);
  }
  semaphores[id].value--;
  release(&semaphores[id].lock);
  return 0;
}

int
sem_signal(int id)
{
  if(id < 0 || id >= MAX_SEMAPHORES)
    return -1;
  acquire(&semaphores[id].lock);
  semaphores[id].value++;
  wakeup(&semaphores[id]);
  release(&semaphores[id].lock);
  return 0;
}
