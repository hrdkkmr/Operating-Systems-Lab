#include "types.h"
#include "stat.h"
#include "user.h"

#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL  2

volatile int counter = 0;

void sync_worker(void *arg) {
  for(int i = 0; i < 100000; i++){
    sem_wait(SEM_MUTEX);
    counter++;
    sem_signal(SEM_MUTEX);
  }
  exit();
}

int main(void) {
  sem_init(SEM_MUTEX, 1);
  for(int i = 0; i < 3; i++){
    void *stack = sbrk(4096);
    clone(sync_worker, 0, stack);
  }
  for(int i = 0; i < 3; i++){
    join();
  }
  printf(1, "Final counter value (synced): %d (Expected: 300000)\n", counter);
  exit();
}