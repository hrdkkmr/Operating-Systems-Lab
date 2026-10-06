#include "types.h"
#include "stat.h"
#include "user.h"

volatile int counter = 0;

void worker(void *arg) {
  for(int i = 0; i < 100000; i++){
    counter++;
  }
  exit();
}

int main(void) {
  for(int i = 0; i < 3; i++){
    void *stack = sbrk(4096);
    clone(worker, 0, stack);
  }
  for(int i = 0; i < 3; i++){
    join();
  }
  printf(1, "Final counter value (unsynced): %d (Expected: ~<300000)\n", counter);
  exit();
}