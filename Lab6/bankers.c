#include "types.h"
#include "stat.h"
#include "user.h"

#define N 5
#define M 3

/*
 * Allocation[i][j] = resources currently allocated
 * to process i.
 */
int Allocation[N][M] = {
  {0, 1, 0},
  {2, 0, 0},
  {3, 0, 2},
  {2, 1, 1},
  {0, 0, 2}
};

/*
 * Max[i][j] = maximum resources that process i
 * may request.
 */
int Max[N][M] = {
  {7, 5, 3},
  {3, 2, 2},
  {9, 0, 2},
  {2, 2, 2},
  {4, 3, 3}
};

/*
 * Available[j] = currently available instances
 * of resource type j.
 */
int Available[M] = {3, 3, 2};

/*
 * Need[i][j] = Max[i][j] - Allocation[i][j]
 */
int Need[N][M];


/*
 * Calculate the Need matrix.
 */
void
calculateNeed(void)
{
  int i, j;

  for(i = 0; i < N; i++){
    for(j = 0; j < M; j++){
      Need[i][j] = Max[i][j] - Allocation[i][j];
    }
  }
}


/*
 * Print a matrix.
 */
void
printMatrix(char *name, int matrix[N][M])
{
  int i, j;

  printf(1, "\n%s:\n", name);

  printf(1, "       A  B  C\n");

  for(i = 0; i < N; i++){
    printf(1, "P%d     ", i);

    for(j = 0; j < M; j++){
      printf(1, "%d  ", matrix[i][j]);
    }

    printf(1, "\n");
  }
}


/*
 * Print Available resources.
 */
void
printAvailable(void)
{
  printf(1, "\nAvailable: [%d %d %d]\n",
         Available[0],
         Available[1],
         Available[2]);
}


/*
 * Safety Algorithm
 *
 * Returns:
 *   1 -> system is safe
 *   0 -> system is unsafe
 *
 * safeSequence[] contains a valid sequence if safe.
 */
int
safetyAlgorithm(int safeSequence[N])
{
  int Work[M];
  int Finish[N];
  int count;
  int i, j;
  int found;
  int canFinish;

  /*
   * Work = Available
   */
  for(j = 0; j < M; j++)
    Work[j] = Available[j];

  /*
   * Initially no process has finished.
   */
  for(i = 0; i < N; i++)
    Finish[i] = 0;

  count = 0;

  while(count < N){

    found = 0;

    for(i = 0; i < N; i++){

      if(Finish[i])
        continue;

      /*
       * Check whether:
       *
       * Need[i] <= Work
       */
      canFinish = 1;

      for(j = 0; j < M; j++){
        if(Need[i][j] > Work[j]){
          canFinish = 0;
          break;
        }
      }

      if(canFinish){

        /*
         * Process can finish.
         *
         * When it finishes, it releases
         * its allocated resources.
         */
        for(j = 0; j < M; j++)
          Work[j] += Allocation[i][j];

        safeSequence[count] = i;
        count++;

        Finish[i] = 1;
        found = 1;
      }
    }

    /*
     * No unfinished process could execute.
     * Therefore the state is unsafe.
     */
    if(!found)
      return 0;
  }

  return 1;
}


/*
 * Print a safe sequence.
 */
void
printSafeSequence(int safeSequence[N])
{
  int i;

  for(i = 0; i < N; i++){
    printf(1, "P%d", safeSequence[i]);

    if(i != N - 1)
      printf(1, " -> ");
  }

  printf(1, "\n");
}


/*
 * Resource Request Algorithm
 *
 * process  = process making the request
 * Request[] = requested resources
 *
 * Returns:
 *   1 -> request granted
 *   0 -> request denied
 */
int
requestResources(int process, int Request[M])
{
  int j;
  int safeSequence[N];

  printf(1, "\n----------------------------------------\n");
  printf(1, "Resource Request Scenario\n");
  printf(1, "----------------------------------------\n");

  printf(1, "Process P%d requests: [%d %d %d]\n",
         process,
         Request[0],
         Request[1],
         Request[2]);

  /*
   * Step 1:
   *
   * Check:
   * Request <= Need
   */
  for(j = 0; j < M; j++){
    if(Request[j] > Need[process][j]){

      printf(1,
             "Request denied: request exceeds P%d's remaining Need.\n",
             process);

      return 0;
    }
  }

  /*
   * Step 2:
   *
   * Check:
   * Request <= Available
   */
  for(j = 0; j < M; j++){
    if(Request[j] > Available[j]){

      printf(1,
             "Request denied: requested resources are not Available.\n");

      return 0;
    }
  }

  /*
   * Step 3:
   *
   * Pretend to allocate the requested resources.
   */
  for(j = 0; j < M; j++){
    Available[j] -= Request[j];
    Allocation[process][j] += Request[j];
    Need[process][j] -= Request[j];
  }

  /*
   * Step 4:
   *
   * Check whether the resulting state is safe.
   */
  if(safetyAlgorithm(safeSequence)){

    printf(1, "Request GRANTED.\n");
    printf(1, "System remains in a SAFE state.\n");

    printf(1, "New safe sequence: ");
    printSafeSequence(safeSequence);

    return 1;
  }

  /*
   * Step 5:
   *
   * Safety check failed.
   *
   * Roll back the temporary allocation.
   */
  for(j = 0; j < M; j++){
    Available[j] += Request[j];
    Allocation[process][j] -= Request[j];
    Need[process][j] += Request[j];
  }

  printf(1,
         "Request denied — would lead to unsafe state.\n");

  return 0;
}


/*
 * Main test program.
 */
int
main(void)
{
  int safeSequence[N];

  /*
   * Scenario 1:
   *
   * P1 requests [1, 0, 2].
   *
   * This request is safe and will be granted.
   */
  int safeRequest[M] = {1, 0, 2};

  /*
   * Scenario 2:
   *
   * After Scenario 1, P0 requests [0, 2, 0].
   *
   * The request is <= Need and <= Available,
   * but granting it makes the system unsafe.
   *
   * Therefore it must be rolled back.
   */
  int unsafeRequest[M] = {0, 2, 0};

  printf(1, "\n========================================\n");
  printf(1, "       BANKER'S ALGORITHM - XV6\n");
  printf(1, "========================================\n");

  /*
   * Calculate Need = Max - Allocation.
   */
  calculateNeed();

  /*
   * Display initial matrices.
   */
  printMatrix("Allocation Matrix", Allocation);
  printMatrix("Max Matrix", Max);
  printMatrix("Need Matrix", Need);
  printAvailable();

  /*
   * Initial Safety Check.
   */
  printf(1, "\n========================================\n");
  printf(1, "           INITIAL SAFETY CHECK\n");
  printf(1, "========================================\n");

  if(safetyAlgorithm(safeSequence)){

    printf(1, "System is in a SAFE state.\n");

    printf(1, "Safe sequence: ");
    printSafeSequence(safeSequence);

  } else {

    printf(1, "System is in an UNSAFE state.\n");
  }

  /*
   * Scenario 1:
   * Safe request.
   */
  printf(1, "\n========================================\n");
  printf(1, "           SCENARIO 1\n");
  printf(1, "========================================\n");

  requestResources(1, safeRequest);

  /*
   * Scenario 2:
   * Request causes unsafe state.
   */
  printf(1, "\n========================================\n");
  printf(1, "           SCENARIO 2\n");
  printf(1, "========================================\n");

  requestResources(0, unsafeRequest);

  /*
   * Final state.
   *
   * Because Scenario 2 was rolled back,
   * the matrices should represent the state
   * after the successful Scenario 1 request.
   */
  printf(1, "\n========================================\n");
  printf(1, "             FINAL STATE\n");
  printf(1, "========================================\n");

  printMatrix("Final Allocation Matrix", Allocation);
  printMatrix("Final Need Matrix", Need);
  printAvailable();

  printf(1, "\nBanker's Algorithm test completed.\n");

  exit();
}