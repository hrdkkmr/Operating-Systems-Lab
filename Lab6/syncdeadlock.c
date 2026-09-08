#include "types.h"
#include "stat.h"
#include "user.h"

#define PRINTER 0
#define SCANNER 1
#define DISK    2

#define NUM_PROCESSES 5
#define NUM_CYCLES 3

/*
 * Resource ordering:
 *
 * Printer < Scanner < Disk
 *
 * Every process acquires resources only in this order.
 *
 * P0: Printer -> Scanner
 * P1: Printer -> Disk
 * P2: Scanner -> Disk
 * P3: Printer -> Disk
 * P4: Printer -> Scanner
 */

void
busy_delay(void)
{
  int i;

  for(i = 0; i < 10000000; i++)
    ;
}

void
do_work(int pid, int cycle)
{
  printf(1, "P%d Cycle %d: started work\n", pid, cycle);

  busy_delay();

  printf(1, "P%d Cycle %d: completed work\n", pid, cycle);
}

void
process_task(int pid, int first, int second)
{
  int cycle;

  for(cycle = 1; cycle <= NUM_CYCLES; cycle++){

    printf(1, "P%d Cycle %d: requesting resource %d\n",
           pid, cycle, first);

    sem_wait(first);

    printf(1, "P%d Cycle %d: granted resource %d\n",
           pid, cycle, first);

    printf(1, "P%d Cycle %d: requesting resource %d\n",
           pid, cycle, second);

    sem_wait(second);

    printf(1, "P%d Cycle %d: granted resource %d\n",
           pid, cycle, second);

    do_work(pid, cycle);

    /*
     * Release in reverse order.
     */
    sem_signal(second);

    printf(1, "P%d Cycle %d: released resource %d\n",
           pid, cycle, second);

    sem_signal(first);

    printf(1, "P%d Cycle %d: released resource %d\n",
           pid, cycle, first);
  }

  printf(1, "P%d: ALL CYCLES COMPLETED\n", pid);
}

int
main(void)
{
  int i;
  int pid;

  /*
   * Initialize resource pools.
   *
   * Printer = 2 instances
   * Scanner = 1 instance
   * Disk    = 2 instances
   */
  sem_init(PRINTER, 2);
  sem_init(SCANNER, 1);
  sem_init(DISK, 2);

  printf(1, "\n");
  printf(1, "========================================\n");
  printf(1, " Combined Synchronization Test\n");
  printf(1, "========================================\n");
  printf(1, "Resources:\n");
  printf(1, "  Printer = 2\n");
  printf(1, "  Scanner = 1\n");
  printf(1, "  Disk    = 2\n");
  printf(1, "\n");
  printf(1, "Resource ordering:\n");
  printf(1, "  Printer < Scanner < Disk\n");
  printf(1, "\n");

  for(i = 0; i < NUM_PROCESSES; i++){

    pid = fork();

    if(pid < 0){
      printf(1, "Fork failed for P%d\n", i);
      exit();
    }

    if(pid == 0){

      if(i == 0){
        process_task(0, PRINTER, SCANNER);
      }
      else if(i == 1){
        process_task(1, PRINTER, DISK);
      }
      else if(i == 2){
        process_task(2, SCANNER, DISK);
      }
      else if(i == 3){
        process_task(3, PRINTER, DISK);
      }
      else if(i == 4){
        process_task(4, PRINTER, SCANNER);
      }

      exit();
    }
  }

  /*
   * Parent waits for all five processes.
   */
  for(i = 0; i < NUM_PROCESSES; i++)
    wait();

  printf(1, "\n");
  printf(1, "========================================\n");
  printf(1, " ALL 5 PROCESSES COMPLETED SUCCESSFULLY\n");
  printf(1, " No deadlock occurred.\n");
  printf(1, "========================================\n");

  exit();
}