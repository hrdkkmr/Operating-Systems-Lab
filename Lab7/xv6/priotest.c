#include "types.h"
#include "stat.h"
#include "user.h"

int main(void) {
  int priorities[] = {5, 10, 15, 20};
  for(int i = 0; i < 4; i++){
    int pid = fork();
    if(pid == 0){
      set_priority(getpid(), priorities[i]);
      for(volatile double j = 0; j < 10000000; j++);
      printf(1, "Process with initial priority %d finished.\n", priorities[i]);
      exit();
    }
  }
  for(int i = 0; i < 4; i++) wait();
  exit();
}